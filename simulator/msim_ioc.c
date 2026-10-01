/* msim_ioc.c: the IOC input/output controller of the reference's
 * section 6: identification and clock, two UARTs, the SPI master, the
 * GPIO, the real-time clock and counter, and system control.  UART 0 is
 * the console, standard input and output, so the Chairman's serial
 * registers are absent in a machine with an IOC; UART 1 is a loopback,
 * receiving what it sends.  The SPI master has an SD card on it when
 * msim is given an image (msim_sd.c).  The counter is the cycle count,
 * at the Chairman timer's frequency.
 *
 * Besides, a test hook: msim can watch one GPIO line as the output of a
 * software UART, 8N1 at a given baud rate, and print what it decodes,
 * which is how a bit-banging CPU is tested. */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>

#include "msim_core.h"
#include "msim_chairman.h"
#include "msim_ioc.h"
#include "msim_sd.h"

#define IOC_REVISION	0
#define IOC_UARTS	2
#define IOC_GPIO_LINES	32
#define FIFO		16
#define IRQ_UART0	0
#define IRQ_SPI		2
#define IRQ_GPIO	3
#define IRQ_RTC		4
#define STDIN_POLL	4096		/* cycles between looks at standard input */

#define ST_RX		0x01
#define ST_ROOM		0x02
#define ST_IDLE		0x04
#define ST_OVERRUN	0x08
#define ST_FRAMING	0x10
#define ST_BREAK	0x20

struct uart {
	uint8_t		rx[FIFO];
	unsigned	head, count;
	u_int32_t	divisor, ien;
	u_int32_t	errors;		/* the sticky status bits */
	bool		console;	/* the host's standard streams */
	bool		used;		/* a register of it has been touched */
	bool		eof;		/* standard input has ended: a break, once */
};

struct ioc {
	int		area;
	unsigned	freq;
	struct uart	uart[IOC_UARTS];
	struct {
		u_int32_t control;
		u_int32_t in;		/* the byte last received */
		u_int32_t busy_until;	/* cycle the transfer completes */
		bool	busy, complete;
		struct msim_sd *sd;
	} spi;
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

/* ---- UARTs ---- */

static void uart_receive(struct uart *u, uint8_t c)
{
	if (u->count == FIFO) {
		u->errors |= ST_OVERRUN;
		return;
	}
	u->rx[(u->head + u->count) % FIFO] = c;
	u->count++;
}

/* The console's input is the host's standard input, looked at when the
 * program asks and, once it has used the UART at all, now and then
 * besides, so that an interrupt-driven reader sees it too; a program
 * that reads its input through msim's BNVs instead is left alone.  Its
 * end is a break. */
static void uart_poll_stdin(struct uart *u)
{
	struct timeval tv;
	fd_set rfds;

	while (u->eof == false && u->count < FIFO) {
		int c;

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		if (select(1, &rfds, NULL, NULL, &tv) <= 0) {
			break;
		}
		c = getc(stdin);
		if (c == EOF) {
			u->eof = true;
			u->errors |= ST_BREAK;
			break;
		}
		uart_receive(u, (uint8_t)c);
	}
}

static u_int32_t uart_read(struct uart *u, u_int32_t off)
{
	u_int32_t c;

	switch (off) {
	case 0x00:
		if (u->console == true) {
			uart_poll_stdin(u);
		}
		return (u->count != 0 ? ST_RX : 0) | ST_ROOM | ST_IDLE | u->errors |
		       (u->count << 8) | (FIFO << 16);
	case 0x04:
		if (u->count == 0) {
			return 0;
		}
		c = u->rx[u->head];
		u->head = (u->head + 1) % FIFO;
		u->count--;
		return c;
	case 0x08: return u->divisor;
	case 0x0c: return u->ien;
	}
	return 0;
}

static void uart_write(struct uart *u, u_int32_t off, u_int32_t d)
{
	switch (off) {
	case 0x04:
		if (u->console == true) {
			putc((int)(d & 0xff), stdout);
			fflush(stdout);
		} else {
			uart_receive(u, (uint8_t)d);	/* loopback */
		}
		break;
	case 0x08: u->divisor = d; break;
	case 0x0c: u->ien = d; break;
	case 0x10: u->errors &= ~(d & (ST_OVERRUN | ST_FRAMING | ST_BREAK)); break;
	}
}

static bool uart_interrupting(struct uart *u)
{
	return ((u->ien & 1) != 0 && u->count != 0) || (u->ien & 2) != 0;
}

/* ---- SPI ---- */

static void spi_write(struct ioc *i, u_int32_t off, u_int32_t d)
{
	switch (off) {
	case 0x00:
		if ((i->spi.control & 8) != 0 && (d & 8) == 0) {
			msim_sd_deselect(i->spi.sd);
		}
		i->spi.control = d;
		break;
	case 0x04:
		if (i->spi.busy == true || (i->spi.control & 1) == 0) {
			break;			/* ignored, as the reference says */
		}
		if ((i->spi.control & 8) != 0 && i->spi.sd != NULL) {
			i->spi.in = msim_sd_transfer(i->spi.sd, (uint8_t)d);
		} else {
			i->spi.in = 0xff;	/* nothing selected drives the line high */
		}
		i->spi.busy = true;
		i->spi.busy_until = i->cycles + 16 * (((i->spi.control >> 8) & 0xff) + 1);
		break;
	}
}

static u_int32_t spi_read(struct ioc *i, u_int32_t off)
{
	u_int32_t st;

	switch (off) {
	case 0x00: return i->spi.control;
	case 0x04: return i->spi.in;
	case 0x08:
		st = (i->spi.busy == true ? 1u : 0u) | (i->spi.complete == true ? 2u : 0u);
		i->spi.complete = false;
		return st;
	}
	return 0;
}

/* ---- GPIO ---- */

static u_int32_t gpio_input(struct ioc *i)
{
	/* nothing drives the inputs: they read 0, and outputs read back */
	return i->gpio.out & i->gpio.dir;
}

static void gpio_set(struct ioc *i, u_int32_t out)
{
	u_int32_t was = gpio_input(i), now;

	i->gpio.out = out;
	now = gpio_input(i);
	i->gpio.pending |= (now & ~was & i->gpio.rise) | (was & ~now & i->gpio.fall);
}

/* ---- the registers ---- */

static u_int32_t msim_ioc_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct ioc *i = fctx;

	if (access != MSIM_ACCESS_WORD || ptr % 4 != 0) {
		fprintf(stderr, "msim: non-word read from the IOC\n");
		return 0;
	}
	if (ptr >= 0x0100 && ptr < 0x0300) {
		i->uart[(ptr >> 8) - 1].used = true;
		return uart_read(&i->uart[(ptr >> 8) - 1], ptr & 0xff);
	}
	if (ptr >= 0x0300 && ptr < 0x0380) {
		return spi_read(i, ptr & 0x7f);
	}
	switch (ptr) {
	case 0x0000:
		return IOC_REVISION | (IOC_UARTS << 8) | (IOC_GPIO_LINES << 16) |
		       (1u << 24) | (1u << 25);
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

static void msim_ioc_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	struct ioc *i = fctx;

	if (access != MSIM_ACCESS_WORD || ptr % 4 != 0) {
		fprintf(stderr, "msim: non-word write to the IOC\n");
		return;
	}
	if (ptr >= 0x0100 && ptr < 0x0300) {
		i->uart[(ptr >> 8) - 1].used = true;
		uart_write(&i->uart[(ptr >> 8) - 1], ptr & 0xff, d);
		return;
	}
	if (ptr >= 0x0300 && ptr < 0x0380) {
		spi_write(i, ptr & 0x7f, d);
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

/* One cycle: the counter, the alarm, the SPI transfer in progress, the
 * interrupts, and the decoder, which samples the line in the middle of
 * each bit. */
static void msim_ioc_tick(struct msim_ctx *ctx, void *fctx)
{
	struct ioc *i = fctx;
	int n;

	i->cycles++;
	if (i->rtc.alarm != 0 && i->cycles % i->freq == 0 &&
	    i->rtc.seconds_at_zero + i->cycles / i->freq == i->rtc.alarm) {
		i->rtc.status |= 1u;
		msim_sys_raise_irq(ctx, IRQ_RTC);
	}
	if (i->cycles % STDIN_POLL == 0 && i->uart[0].used == true) {
		uart_poll_stdin(&i->uart[0]);
	}
	for (n = 0; n < IOC_UARTS; n++) {
		if (uart_interrupting(&i->uart[n]) == true) {
			msim_sys_raise_irq(ctx, IRQ_UART0 + n);
		}
	}
	if (i->spi.busy == true && i->cycles == i->spi.busy_until) {
		i->spi.busy = false;
		i->spi.complete = true;
		if ((i->spi.control & (1u << 16)) != 0) {
			msim_sys_raise_irq(ctx, IRQ_SPI);
		}
	}
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
	i->uart[0].console = true;
	i->rtc.seconds_at_zero = (u_int32_t)time(NULL);
	i->rtc.status = 2u;			/* not kept while off */
	i->decode.bit = -1;
	i->decode.last = 1;
	setvbuf(stdin, NULL, _IONBF, 0);
	msim_device_add(ctx, area, 0x00000003, msim_ioc_read, msim_ioc_write,
			NULL, msim_ioc_tick, i);
	ctx->areas[area].size = 0x1000;
	msim_sys_console_moved(ctx);
}

void msim_del_ioc(struct msim_ctx *ctx, int area)
{
	struct ioc *i = ctx->areas[area].ctx;

	msim_sd_close(i->spi.sd);
	free(i);
	msim_device_remove(ctx, area);
}

void msim_ioc_decode(struct msim_ctx *ctx, int area, int line, unsigned baud)
{
	struct ioc *i = ctx->areas[area].ctx;

	i->decode.on = true;
	i->decode.line = line;
	i->decode.baud = baud;
}

int msim_ioc_sd(struct msim_ctx *ctx, int area, const char *image)
{
	struct ioc *i = ctx->areas[area].ctx;

	i->spi.sd = msim_sd_open(image);
	return i->spi.sd != NULL ? 0 : -1;
}
