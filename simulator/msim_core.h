/*
 * msim_core.h
 * This file is part of MSIM, a MEOW Simulator
 *
 * Copyright (C) 2006-2007 - Rob Kendrick <rjek@rjek.com>
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
 
#ifndef __MSIM_CORE_H__
#define __MSIM_CORE_H__

#include <stdbool.h> 
#include <sys/types.h>

struct msim_ctx;

#include "config.h"
#include "meow.h"
#include "msim_debug.h"

#define MSIM_SR_IRQ(i)   (((i) & MEOW_SR_I) != 0)

#define MSIM_LOG printf

typedef enum {
	MSIM_ACCESS_BYTE = 0,
 	MSIM_ACCESS_HALFWORD = 1,
 	MSIM_ACCESS_WORD = 2,
} msim_mem_access_type;

typedef enum {
	MSIM_THIS_BANK 	= 0,
	MSIM_OTHER_BANK = 1
} msim_bank_type;

typedef enum {
	MSIM_R0 = 0, MSIM_R1, MSIM_R2, MSIM_R3, MSIM_R4, MSIM_R5, MSIM_R6,
	MSIM_R7, MSIM_R8, MSIM_R9, MSIM_R10, MSIM_R11, MSIM_R12, MSIM_R13,
	MSIM_R14, MSIM_R15,
	MSIM_SP = 11,
	MSIM_LR = 12,
	MSIM_IR = 13,
	MSIM_SR = 14,
	MSIM_PC = 15
} msim_register;

struct msim_ctx;

typedef u_int32_t (*msim_read_mem)(struct msim_ctx *ctx, const u_int32_t p, 
					const msim_mem_access_type,
					void *fctx);
typedef void (*msim_write_mem)(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type,
				const u_int32_t d,
				void *fctx);

typedef void (*msim_reset_mem)(struct msim_ctx *ctx, void *fctx);

typedef void (*msim_device_tick)(struct msim_ctx *ctx, void *fctx);

struct msim_ctx;

typedef void (*msim_bnvop)(struct msim_ctx *ctx, signed int op, void *bnvctx);

#define MSIM_PROFILE_BYTES (1u << 20)

struct msim_ctx {
	bool		init;
	bool		irqmode;
	bool		nopcincrement;
	u_int32_t	*r;
	u_int32_t	*ar;
	u_int32_t	realr[16];
	u_int32_t	realar[16];
	unsigned int	cyclecount;
	unsigned int	*profile;	/* executions per halfword of the low 1 MB, or NULL */
	
	struct {
		 msim_read_mem	read;
		 msim_write_mem	write;
		 msim_reset_mem reset;
		 msim_device_tick tick;
		 void		*ctx;
		 u_int32_t	deviceid;
		 u_int32_t	size;		/* bytes, for the chip select table */
	}		areas[32];
	
	/* we also keep a pre-compiled non-sparse list of the tick functions
	 * as iterating through the above array for every cycle is expensive.
	 */
	msim_device_tick stick[32]; 	/* actual short list */
	unsigned int sticka[31]; 	/* shortlist entry's area numbers */
	unsigned int sticks; 		/* size of shortlist */
	
	msim_bnvop	bnvops[512];
	void		*bnvopsctx[512];
	
	/* debugger state */
	u_int32_t breakpoints[MSIM_DEBUG_BREAKPOINTS];
#ifdef MSIM_WITH_LUA
	char *watchpoints[MSIM_DEBUG_WATCHPOINTS];
	bool watching;
	lua_State *l;
#endif
};

struct msim_ctx *msim_init(void);
void msim_destroy(struct msim_ctx *ctx);

void msim_device_add(struct msim_ctx *ctx, const unsigned int area, 
			const u_int32_t id, msim_read_mem read,
			msim_write_mem write, msim_reset_mem reset, 
			msim_device_tick tick, void *fctx);
			
void msim_device_remove(struct msim_ctx *ctx, const unsigned int area);

void msim_run(struct msim_ctx *ctx, unsigned int instructions, bool trace);

void msim_memset(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t,	u_int32_t d);
u_int32_t msim_memget(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t);

void msim_irq(struct msim_ctx *ctx);
void msim_swap_banks(struct msim_ctx *ctx);

u_int16_t msim_fetch(struct msim_ctx *ctx);
void msim_execute(struct msim_ctx *ctx, u_int16_t word);

void msim_add_bnv(struct msim_ctx *ctx, signed int op, msim_bnvop func,
			void *fctx);
void msim_del_bnv(struct msim_ctx *ctx, signed int op);

void msim_add_rom_from_file(struct msim_ctx *ctx, int area, char *filename);
void msim_del_rom(struct msim_ctx *ctx, int area);

void msim_add_ram(struct msim_ctx *ctx, int area, size_t size);
void msim_del_ram(struct msim_ctx *ctx, int area);

void msim_add_builtin_bnvs(struct msim_ctx *ctx);
void msim_del_builtin_bnvs(struct msim_ctx *ctx);

#endif /* __MSIM_H__ */
