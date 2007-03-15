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

#include "msim_core.h"
#include "msim_chairman.h"

struct sys {
	struct {
		u_int32_t pending;
		u_int32_t mask[32];
	} irq;
	
	struct {
		u_int32_t frequency;
		u_int32_t reload;
		u_int32_t state;
	} timer;
};

static u_int32_t msim_sys_read_chip_selects(struct msim_ctx *ctx, u_int32_t p,
						struct sys *s)
{
	if (p % 256 == 0) {
		/* chip select device ID (first word of each 256 byte entry) */
		return ctx->areas[(p >> 8) & 31].deviceid;
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

static u_int32_t msim_sys_read_irqs(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	return s->irq.pending;
}

static void msim_sys_write_irqs(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	s->irq.pending = d;
}

static u_int32_t msim_sys_read_timer(struct msim_ctx *ctx, u_int32_t p,
					struct sys *s)
{
	switch (p) {
	case 0: return s->timer.frequency;	break;
	case 4: return s->timer.reload;		break;
	case 8: return s->timer.state;		break;
	}
	return 0;
}

static void msim_sys_write_timer(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d, struct sys *s)
{
	switch (p) {
	case 0:
		fprintf(stderr,
			"msim: attempt to write to frequency register.\n");
		
		break;
	case 4:
		s->timer.reload = s->timer.state = d;
		break;
	case 8:
		s->timer.state = d;
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
		return;
	}
	
	if (p % 4 != 0) {
		fprintf(stderr,
		"msim: attempt to write non-word-aligned data to system controller\n");
		return;
	}
	
	/* TODO: When we add more functions to this, we should make this
	 * function table driven rather than an if/else ladder.  The same
	 * is true of the _write() function below it.
	 */
	
	if (ptr >= 0 && ptr < 0x2000)
		return msim_sys_read_chip_selects(ctx, ptr, s);
	else if (ptr >= 0x2000 && ptr < 0x2400)
		return msim_sys_read_irq_masks(ctx, ptr, s);
	else if (ptr == 0x2400)
		return msim_sys_read_irqs(ctx, ptr, s);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		return msim_sys_read_timer(ctx, ptr, s);
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
	
	if (p % 4 != 0) {
		fprintf(stderr,
		"msim: attempt to write non-word-aligned data to system controller\n");
		return;
	}
	
	if (ptr >= 0 && ptr < 0x2000)
		msim_sys_write_chip_selects(ctx, ptr, d, s);
	else if (ptr >= 0x2000 && ptr < 0x2400)
		msim_sys_write_irq_masks(ctx, ptr, d, s);
	else if (ptr == 0x2400)
		msim_sys_write_irqs(ctx, ptr, d, s);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		msim_sys_write_timer(ctx, ptr, d, s);
	else
		fprintf(stderr, "msim: attempt to write from undefined area in"
			" system controller\n");
}

static void msim_sys_tick(struct msim_ctx *ctx, void *fctx)
{
	struct sys *s = (struct sys *)fctx;
	
	if (s->timer.reload != 0) {
		s->timer.state--;
		if (s->timer.state == 0) {
			s->timer.state = s->timer.reload;
			msim_sys_raise_irq(ctx, 31);
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

void msim_sys_raise_irq(struct msim_ctx *ctx, int irq)
{
	struct sys *s = (struct sys *)ctx->areas[31].ctx;
	
	s->irq.pending |= (1 << irq);
	
	msim_irq(ctx);
}
