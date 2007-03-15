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

#include "msim_core.h"
#include "msim_chairman.h"

static u_int32_t msim_sys_read_chip_selects(struct msim_ctx *ctx, u_int32_t p)
{
	return 0;
}

static void msim_sys_write_chip_selects(struct msim_ctx *ctx, u_int32_t p,
					u_int32_t d)
{

}

static u_int32_t msim_sys_read_irq_masks(struct msim_ctx *ctx, u_int32_t p)
{
	return 0;
}

static void msim_sys_write_irq_masks(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d)
{

}

static u_int32_t msim_sys_read_irqs(struct msim_ctx *ctx, u_int32_t p)
{
	return 0;
}

static void msim_sys_write_irqs(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d)
{

}

static u_int32_t msim_sys_read_timer(struct msim_ctx *ctx, u_int32_t p)
{
	return 0;
}

static void msim_sys_write_timer(struct msim_ctx *ctx, u_int32_t p,
						u_int32_t d)
{

}

static u_int32_t msim_sys_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	if (access != MSIM_ACCESS_WORD) {
		fprintf(stderr,
		"msim: attempt to read non-word from system controller\n");
	}
	
	/* TODO: When we add more functions to this, we should make this
	 * function table driven rather than an if/else ladder.  The same
	 * is true of the _write() function below it.
	 */
	
	if (ptr >= 0 && ptr < 0x2000)
		return msim_sys_read_chip_selects(ctx, ptr);
	else if (ptr >= 0x2000 && ptr < 0x2400)
		return msim_sys_read_irq_masks(ctx, ptr);
	else if (ptr == 0x2400)
		return msim_sys_read_irqs(ctx, ptr);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		return msim_sys_read_timer(ctx, ptr);
	else
		fprintf(stderr, "msim: attempt to read from undefined area in"
			" system controller\n");

	return 0;
}

static void msim_sys_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	if (access != MSIM_ACCESS_WORD) {
		fprintf(stderr,
		"msim: attempt to write non-word from system controller\n");
	}
	
	if (ptr >= 0 && ptr < 0x2000)
		msim_sys_write_chip_selects(ctx, ptr, d);
	else if (ptr >= 0x2000 && ptr < 0x2400)
		msim_sys_write_irq_masks(ctx, ptr, d);
	else if (ptr == 0x2400)
		msim_sys_write_irqs(ctx, ptr, d);
	else if (ptr >= 0x2404 && ptr < 0x2410)
		msim_sys_write_timer(ctx, ptr, d);
	else
		fprintf(stderr, "msim: attempt to write from undefined area in"
			" system controller\n");
}

static void msim_sys_tick(struct msim_ctx *ctx, void *fctx)
{

}

void msim_add_sys(struct msim_ctx *ctx, int area)
{
	msim_device_add(ctx, area, 0x00000002, msim_sys_read, msim_sys_write,
			NULL, msim_sys_tick, NULL);
}

void msim_del_sys(struct msim_ctx *ctx, int area)
{
	msim_device_remove(ctx, area);
}
