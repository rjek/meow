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


static u_int32_t msim_sys_read(const u_int32_t ptr, msim_mem_access_type access,
				void *ctx)
{
	return 0;
}

static void msim_sys_write(const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *ctx)
{

}

void msim_add_sys(struct msim_ctx *ctx, int area)
{
	msim_device_add(ctx, area, 0x00000002, msim_sys_read, msim_sys_write,
			NULL, NULL);
}

void msim_del_sys(struct msim_ctx *ctx, int area)
{
	msim_device_remove(ctx, area);
}
