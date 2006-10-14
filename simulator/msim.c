/*
 * msim.c
 * This file is part of MSIM, a MEOW Simulator
 *
 * Copyright (C) 2006 - Rob Kendrick <rjek@rjek.com>
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

#include <stdlib.h>
#include <assert.h>

#include "msim.h"

struct msim_ctx *msim_init(void)
{
	struct msim_ctx *ctx = calloc(1, sizeof(struct msim_ctx));
	
	assert(ctx != NULL);
	
	return ctx;
}

void msim_destroy(struct msim_ctx *ctx)
{
	free(ctx);
}


void msim_device_add(struct msim_ctx *ctx, const int area, msim_read_mem *read,
 			msim_write_mem *write, msim_reset_mem *reset, 
 			void *fctx)
{
	ctx->areas[area].read = read;
	ctx->areas[area].write = write;
	ctx->areas[area].reset = reset;
	ctx->areas[area].ctx = fctx;
}

void msim_device_remove(struct msim_ctx *ctx, const int area)
{
	ctx->areas[area].read = NULL;
	ctx->areas[area].write = NULL;
	ctx->areas[area].reset = NULL;
	ctx->areas[area].ctx = NULL;
}

void msim_run(struct msim_ctx *ctx, unsigned int instructions)
{
	for (; instructions >= 0; instructions--) {
		struct msim_instr instr;
		msim_fetch_decode(ctx, &instr);
		msim_execute(ctx, &instr);
	}
}

void msim_fetch_decode(struct msim_ctx *ctx, struct msim_instr *instr)
{

}

void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr)
{

}
