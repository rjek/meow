/* msim_ioc.c: the IOC input/output controller of the reference's
 * section 6, as far as msim models it: identification, the GPIO, the
 * real-time clock and counter, and system control.  There are no UARTs
 * yet, since UART 0 would take over the Chairman's console, and no SPI;
 * the identification register says so.  The counter is the cycle
 * count, at the Chairman timer's frequency.
 *
 * Besides, a test hook: msim can watch one GPIO line as the output of a
 * software UART, 8N1 at a given baud rate, and print what it decodes,
 * which is how a bit-banging CPU is tested. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>

#include "msim_core.h"
#include "msim_chairman.h"
#include "msim_ioc.h"

#define IOC_REVISION	0
#define IOC_GPIO_LINES	32
#define IRQ_GPIO	3
#define IRQ_RTC		4

struct ioc {
	int		area;
	unsigned	freq;
	struct {
		u_int32_t dir, out, rise, fall, pending;
	} gpio;
	struct {
		u_int32_t seconds_at_zero;	/* the seconds register less cycles / freq */
		u_int32_t alarm, status;
	} rtc;
	u_int32_t	cycles;
	u_int32_t	leds;
	struct {
		bool	on;
		int	line;
		unsigned baud;
		int	bit;		/* -1 idle; 0 to 7 data; 8 stop */
		u_int32_t start;	/* cycle of the start bit's edge */
		unsigned value;
		int	last;		/* the line's level last cycle */
	} decode;
};

static u_int32_t gpio_input(struct ioc *i)
{
	/* nothing drives the inputs: they read 0, and outputs read back */
	return i->gpio.out & i->gpio.dir;
}

static u_int32_t msim_ioc_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct ioc *i = fctx;

	if (access != MSIM_ACCESS_WORD || ptr % 4 != 0) {
		fprintf(stderr, "msim: non-word read from the IOC\n");
		return 0;
	}
	switch (ptr) {
	case 0x0000:
		return IOC_REVISION | (0u << 8) | (IOC_GPIO_LINES << 16) | (1u << 25);
	case 0x0004: return i->freq;
	case 0x0400: return i->gpio.dir;
	case 0x0404: return i->gpio.out;
	case 0x0408: return gpio_input(i);
	case 0x0414: return i->gpio.rise;
	case 0x0418: return i->gpio.fall;
	case 0x041c: return i->gpio.pending;
	case 0x0500: return i->rtc.seconds_at_zero + i->cycles / i->freq;
	case 0x0504: return i->rtc.alarm;
	case 0x0508: return i->cycles;
	case 0x050c: return i->rtc.status;
	case 0x0f08: return i->leds;
	}
	return 0;				/* reserved, and the write-only ones */
}

static void gpio_set(struct ioc *i, u_int32_t out)
{
	u_int32_t was = gpio_input(i), now;

	i->gpio.out = out;
	now = gpio_input(i);
	i->gpio.pending |= (now & ~was & i->gpio.rise) | (was & ~now & i->gpio.fall);
}

static void msim_ioc_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	struct ioc *i = fctx;

	if (access != MSIM_ACCESS_WORD || ptr % 4 != 0) {
		fprintf(stderr, "msim: non-word write to the IOC\n");
		return;
	}
	switch (ptr) {
	case 0x0400: i->gpio.dir = d; break;
	case 0x0404: gpio_set(i, d); break;
	case 0x040c: gpio_set(i, i->gpio.out | d); break;
	case 0x0410: gpio_set(i, i->gpio.out & ~d); break;
	case 0x0414: i->gpio.rise = d; break;
	case 0x0418: i->gpio.fall = d; break;
	case 0x041c: i->gpio.pending &= ~d; break;
	case 0x0500: i->rtc.seconds_at_zero = d - i->cycles / i->freq; break;
	case 0x0504: i->rtc.alarm = d; break;
	case 0x050c: i->rtc.status &= ~(d & 1u); break;
	case 0x0f00:
		if (d == 1) {
			ctx->wfi = true;
		} else if (d == 2) {
			fflush(stdout);
			exit(0);
		}
		break;
	case 0x0f04:
		fprintf(stderr, "msim: IOC reset requested, not modelled\n");
		break;
	case 0x0f08: i->leds = d; break;
	default:
		if (ptr < 0x1000) {
			fprintf(stderr, "msim: write to a reserved IOC register"
				" 0x%x\n", ptr);
		}
		break;
	}
}

/* One cycle: the counter, the alarm, the interrupts, and the decoder,
 * which samples the line in the middle of each bit. */
static void msim_ioc_tick(struct msim_ctx *ctx, void *fctx)
{
	struct ioc *i = fctx;
	u_int32_t pending_was = i->gpio.pending;

	i->cycles++;
	if (i->rtc.alarm != 0 &&
	    i->rtc.seconds_at_zero + i->cycles / i->freq == i->rtc.alarm &&
	    i->cycles % i->freq == 0) {
		i->rtc.status |= 1u;
		msim_sys_raise_irq(ctx, IRQ_RTC);
	}
	(void)pending_was;
	if (i->gpio.pending != 0) {
		msim_sys_raise_irq(ctx, IRQ_GPIO);
	}
	if (i->decode.on == true) {
		int level = (gpio_input(i) >> i->decode.line) & 1;

		if (i->decode.bit < 0) {
			if (i->decode.last == 1 && level == 0) {
				i->decode.bit = 0;
				i->decode.start = i->cycles;
				i->decode.value = 0;
			}
		} else {
			/* data bit n is sampled (2n + 3) / 2 bit times after
			 * the start edge, the stop bit at (2 * 8 + 3) / 2 */
			u_int32_t at = i->decode.start +
				(u_int32_t)(((2u * i->decode.bit + 3u) * (unsigned long long)i->freq) /
					    (2u * i->decode.baud));

			if (i->cycles >= at) {
				if (i->decode.bit < 8) {
					i->decode.value |= (unsigned)level << i->decode.bit;
					i->decode.bit++;
				} else {
					if (level == 1) {
						putc((int)i->decode.value, stdout);
						fflush(stdout);
					} else {
						fprintf(stderr, "msim: framing error on"
							" GPIO line %d\n", i->decode.line);
					}
					i->decode.bit = -1;
				}
			}
		}
		i->decode.last = level;
	}
}

void msim_add_ioc(struct msim_ctx *ctx, int area)
{
	struct ioc *i = calloc(1, sizeof *i);

	i->area = area;
	i->freq = 1000000;			/* as the Chairman's timer */
	i->rtc.seconds_at_zero = (u_int32_t)time(NULL);
	i->rtc.status = 2u;			/* not kept while off */
	i->decode.bit = -1;
	i->decode.last = 1;
	msim_device_add(ctx, area, 0x00000003, msim_ioc_read, msim_ioc_write,
			NULL, msim_ioc_tick, i);
	ctx->areas[area].size = 0x1000;
}

void msim_del_ioc(struct msim_ctx *ctx, int area)
{
	free(ctx->areas[area].ctx);
	msim_device_remove(ctx, area);
}

void msim_ioc_decode(struct msim_ctx *ctx, int area, int line, unsigned baud)
{
	struct ioc *i = ctx->areas[area].ctx;

	i->decode.on = true;
	i->decode.line = line;
	i->decode.baud = baud;
}
