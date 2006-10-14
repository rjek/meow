/*
 * msim.h
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
 
#ifndef __MSIM_H__
#define __MSIM_H__
 
#include <sys/types.h>
#include <stdbool.h>
 
enum {
	MSIM_ACCESS_BYTE,
 	MSIM_ACCESS_HALFWORD
} msim_mem_access_type;

typedef u_int16_t (*msim_read_mem)(const u_int32_t /* ptr */, 
					const msim_mem_access_type,
					void *ctx);
typedef void (*msim_write_mem)(const u_int32_t /* ptr */,
				const msim_mem_access_type,
				const u_int16_t d,
				void *ctx);

typedef void (*msim_reset_mem)(void *ctx);

struct msim_ctx {
	bool		irqmode;
	u_int32_t	r[16];
	u_int32_t	ar[16];
	struct {
		 msim_read_mem	*read;
		 msim_write_mem	*write;
		 msim_reset_mem *reset;
	}		areas[16];
};

struct msim_ctx *msim_init(void);
void msim_destroy(struct msim_ctx *ctx);
void msim_device_add(struct msim_ctx *ctx, const int area, msim_read_mem *read,
 			msim_write_mem *write, msim_reset_mem *reset, 
 			void *fctx);
void msim_device_remove(struct msim_ctx *ctx, const int area);
void msim_run(struct msim_ctx *ctx, unsigned int instructions);

#endif /* __MSIM_H__ */
