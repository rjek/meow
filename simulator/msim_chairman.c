/*
 * msim_chairman.h
 * This file is part of MSIM, a MEOW Simulator
 *
 * Copyright (C) 2007 - Rob Kendrick <rjek@rjek.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/select.h>		/* needed for serial port emulation */

#include "msim_core.h"
#include "msim_chairman.h"

struct sys {
	struct {
		u_int32_t pending[32];		/* one word per CPU */
		u_int32_t mask[32];
	} irq;
	
	struct {
		u_int32_t frequency;
		u_int32_t reload[32];		/* one timer per CPU */
		u_int32_t state[32];
	} timer;

	u_int32_t start[32];			/* each CPU's start address */
	u_int32_t lock[32];
	
	struct {
		u_int32_t flags;
		u_int32_t input;
		int absent;		/* an IOC's UART 0 is the console instead */
	} serial;
};

#define IRQ_DOORBELL	30
#define IRQ_TIMER	31

static u_int32_t msim_sys_read_chip_selects(struct msim_ctx *ctx, u_int32_t p,
						struct sys *s)
{
	if (p % 256 == 0) {
		/* chip select device ID (first word of each 256 byte entry) */
		return ctx->areas[(p >> 8) & 31].deviceid;
	}
	if (p % 256 == 4) {
		/* size in bytes, where the device has one */
		return ctx->areas[(p >> 8) & 31].size;
	}
	
	return 0;
}

static void msim_sys_write_chip_selects(struct msim_ctx *ctx, u_int32_t p,
					u_int32_t d, struct sys *s)
{
	fprintf(stderr, 
		"msim: attempt to write to chip select table ignored.\n");
}

static u_int32_t msim_sys_read_irq_masks(struct msim_ctx *ctx, u_int32_t p,
						struct sys *s)
{
	return s->irq.mask[(p >> 2) & 31];
}

static void msim_sys_write_irq_masks(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	s->irq.mask[(p >> 2) & 31] = d;
}

/* The pending words at 0x2200; 0x2400 is the accessing CPU's own */
static u_int32_t msim_sys_read_irqs(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	unsigned int cpu = p == 0x2400 ? ctx->cpu : (p >> 2) & 31;

	return s->irq.pending[cpu];
}

static void msim_sys_write_irqs(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	unsigned int cpu = p == 0x2400 ? ctx->cpu : (p >> 2) & 31;

	s->irq.pending[cpu] &= ~(d);
}

static u_int32_t msim_sys_read_timer(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	switch (p) {
	case 0x2404: return s->timer.frequency;		break;
	case 0x2408: return s->timer.reload[ctx->cpu];	break;
	case 0x240C: return s->timer.state[ctx->cpu];	break;
	}
	return 0;
}

static void msim_sys_set_timer(struct sys *s, unsigned int cpu, int which,
				u_int32_t d)
{
	if (which == 0) {
		s->timer.reload[cpu] = s->timer.state[cpu] = d;
	} else {
		s->timer.state[cpu] = d;
	}
}

static void msim_sys_write_timer(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	switch (p) {
	case 0x2404:
		fprintf(stderr,
			"msim: attempt to write to frequency register.\n");
		
		break;
	case 0x2408:
		msim_sys_set_timer(s, ctx->cpu, 0, d);
		break;
	case 0x240C:
		msim_sys_set_timer(s, ctx->cpu, 1, d);
		break;
	}
}

/* The control block of CPU n at 0x2800 + 0x20 * n, section 5.5 */
static u_int32_t msim_sys_read_cpu(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	unsigned int cpu = (p >> 5) & 31;
	struct msim_ctx *c = msim_cpu(ctx, cpu);

	switch (p & 0x1c) {
	case 0x00:
		if (c == NULL) {
			return 0;
		}
		return 1u | (c->running == true ? 2u : 0u);
	case 0x04: return s->start[cpu];
	case 0x10: return s->timer.reload[cpu];
	case 0x14: return s->timer.state[cpu];
	}
	return 0;
}

static void msim_sys_write_cpu(struct msim_ctx *ctx, u_int32_t p,
				u_int32_t d, struct sys *s)
{
	unsigned int cpu = (p >> 5) & 31;
	struct msim_ctx *c = msim_cpu(ctx, cpu);

	if (c == NULL) {
		fprintf(stderr, "msim: no CPU %u to control\n", cpu);
		return;
	}
	switch (p & 0x1c) {
	case 0x04:
		s->start[cpu] = d;
		break;
	case 0x08:
		if (cpu == 0) {
			fprintf(stderr, "msim: CPU 0 cannot be stopped or"
				" started\n");
		} else if (d == 1) {
			msim_reset_cpu(c, s->start[cpu]);
			c->running = true;
		} else if (d == 0) {
			c->running = false;
			s->irq.mask[cpu] = 0;
			s->irq.pending[cpu] = 0;
			s->timer.reload[cpu] = s->timer.state[cpu] = 0;
		}
		break;
	case 0x0c:
		s->irq.pending[cpu] |= 1u << IRQ_DOORBELL;
		break;
	case 0x10:
		msim_sys_set_timer(s, cpu, 0, d);
		break;
	case 0x14:
		msim_sys_set_timer(s, cpu, 1, d);
		break;
	default:
		fprintf(stderr, "msim: attempt to write a read-only CPU"
			" control register\n");
		break;
	}
}

static u_int32_t msim_sys_read_present(struct msim_ctx *ctx, struct sys *s)
{
	return ctx->ncpus >= 32 ? 0xffffffffu : (1u << ctx->ncpus) - 1;
}

/* Test and set: the read is the taking, section 5.6 */
static u_int32_t msim_sys_read_lock(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	u_int32_t was = s->lock[(p >> 2) & 31];

	s->lock[(p >> 2) & 31] = 1;
	return was;
}

static void msim_sys_write_lock(struct msim_ctx *ctx, u_int32_t p,
				u_int32_t d, struct sys *s)
{
	s->lock[(p >> 2) & 31] = d;
}

static u_int32_t msim_sys_read_serial(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	struct timeval tv;
	fd_set rfds;

	if (s->serial.absent != 0) {
		return 0;		/* not present: reads as nothing */
	}
	/* Look for a byte on the host's standard input, unless one is
	 * already waiting, which the program has yet to take.  Bit 0 of the
	 * flags is fresh; bit 1, msim's own, says the input has ended. */
	if ((s->serial.flags & 3) == 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 50;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);
		setvbuf(stdin, NULL, _IONBF, 0);
		if (select(1, &rfds, NULL, NULL, &tv) > 0) {
			int c = getc(stdin);

			if (c == EOF) {
				s->serial.flags |= 2;
			} else {
				s->serial.flags |= 1;
				s->serial.input = (u_int32_t)c;
			}
		}
	}
	
	switch (p) {
	case 0x2410: return s->serial.flags;
	case 0x2414: s->serial.flags &= ~1u; return s->serial.input;
	case 0x2418: 
		fprintf(stderr,
			"msim: attempt to read from serial output register.\n");
		break;
	}
	
	return 0;
}

static void msim_sys_write_serial(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	if (s->serial.absent != 0) {
		fprintf(stderr, "msim: the console is the IOC's UART 0, not"
			" the Chairman's\n");
		return;
	}
	switch (p) {
	case 0x2410:
	case 0x2414:
		fprintf(stderr,
				"msim: attempt to write to serial flag/in.\n");
		break;
	case 0x2418:
		putc(d, stdout);
		fflush(stdout);
		break;
	}
}

static u_int32_t msim_sys_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct sys *s = (struct sys *)fctx;
	
	if (access != MSIM_ACCESS_WORD) {
		fprintf(stderr,
		"msim: attempt to read non-word from system controller\n");
		return 0;
	}
	
	if (ptr % 4 != 0) {
		fprintf(stderr,
		"msim: attempt to write non-word-aligned data to system controller\n");
		return 0;
	}
	
	/* TODO: When we add more functions to this, we should make this
	 * function table driven rather than an if/else ladder.  The same
	 * is true of the _write() function below it.
	 */
	
	if (ptr >= 0 && ptr < 0x2000)
		return msim_sys_read_chip_selects(ctx, ptr, s);
	else if (ptr >= 0x2000 && ptr < 0x2080)
		return msim_sys_read_irq_masks(ctx, ptr, s);
	else if ((ptr >= 0x2200 && ptr < 0x2280) || ptr == 0x2400)
		return msim_sys_read_irqs(ctx, ptr, s);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		return msim_sys_read_timer(ctx, ptr, s);
	else if (ptr >= 0x2410 && ptr < 0x241c)
		return msim_sys_read_serial(ctx, ptr, s);
	else if (ptr >= 0x2800 && ptr < 0x2c00)
		return msim_sys_read_cpu(ctx, ptr, s);
	else if (ptr == 0x2c00)
		return msim_sys_read_present(ctx, s);
	else if (ptr >= 0x2e00 && ptr < 0x2e80)
		return msim_sys_read_lock(ctx, ptr, s);
	else
		fprintf(stderr, "msim: attempt to read from undefined area in"
			" system controller\n");

	return 0;
}

static void msim_sys_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	struct sys *s = (struct sys *)fctx;

	if (access != MSIM_ACCESS_WORD) {
		fprintf(stderr,
		"msim: attempt to write non-word to system controller\n");
		return;
	}
	
	if (ptr % 4 != 0) {
		fprintf(stderr,
		"msim: attempt to write non-word-aligned data to system controller\n");
		return;
	}
	
	if (ptr >= 0 && ptr < 0x2000)
		msim_sys_write_chip_selects(ctx, ptr, d, s);
	else if (ptr >= 0x2000 && ptr < 0x2080)
		msim_sys_write_irq_masks(ctx, ptr, d, s);
	else if ((ptr >= 0x2200 && ptr < 0x2280) || ptr == 0x2400)
		msim_sys_write_irqs(ctx, ptr, d, s);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		msim_sys_write_timer(ctx, ptr, d, s);
	else if (ptr >= 0x2410 && ptr < 0x241c)
		msim_sys_write_serial(ctx, ptr, d, s);
	else if (ptr >= 0x2800 && ptr < 0x2c00)
		msim_sys_write_cpu(ctx, ptr, d, s);
	else if (ptr >= 0x2e00 && ptr < 0x2e80)
		msim_sys_write_lock(ctx, ptr, d, s);
	else
		fprintf(stderr, "msim: attempt to write from undefined area in"
			" system controller\n");
}

static void msim_sys_tick(struct msim_ctx *ctx, void *fctx)
{
	struct sys *s = (struct sys *)fctx;
	unsigned int n;

	for (n = 0; n < ctx->ncpus; n++) {
		struct msim_ctx *c = msim_cpu(ctx, n);

		if (c->running == false) {
			continue;
		}
		if (s->timer.reload[n] != 0) {
			s->timer.state[n]--;
			if (s->timer.state[n] == 0) {
				s->timer.state[n] = s->timer.reload[n];
				s->irq.pending[n] |= 1u << IRQ_TIMER;
			}
		}
		/* a CPU is interrupted while anything it wants is pending;
		 * one already in interrupt mode is left alone, and one
		 * waiting is woken */
		if ((s->irq.pending[n] & s->irq.mask[n]) != 0) {
			msim_irq(c);
		}
	}
}

void msim_add_sys(struct msim_ctx *ctx, int area)
{
	struct sys *s = calloc(1, sizeof(struct sys));
	
	s->timer.frequency = 1000000; /* fake 1MHz */
	
	msim_device_add(ctx, area, 0x00000002, msim_sys_read, msim_sys_write,
			NULL, msim_sys_tick, s);
}

void msim_del_sys(struct msim_ctx *ctx, int area)
{
	free(ctx->areas[area].ctx);
	msim_device_remove(ctx, area);
}

void msim_sys_console_moved(struct msim_ctx *ctx)
{
	struct sys *s = (struct sys *)ctx->areas[31].ctx;

	if (s != NULL) {
		s->serial.absent = 1;
	}
}

/* A shared source: pending for every CPU, each of which clears its own */
void msim_sys_raise_irq(struct msim_ctx *ctx, int irq)
{
	struct sys *s = (struct sys *)ctx->areas[31].ctx;
	unsigned int n;

	for (n = 0; n < ctx->ncpus; n++) {
		s->irq.pending[n] |= (1u << irq);
	}
	
	/* our tick function will check if a CPU wants to hear about this
	 * interrupt, and will call msim_irq() if it does.
	 */
}
