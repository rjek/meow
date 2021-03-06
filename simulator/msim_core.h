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
struct msim_instr;

#include "config.h"
#include "msim_debug.h"

#define MSIM_SR_NFLAG(i) (((i) & 1<<31) != 0)
#define MSIM_SR_ZFLAG(i) (((i) & 1<<30) != 0)
#define MSIM_SR_CFLAG(i) (((i) & 1<<29) != 0)
#define MSIM_SR_VFLAG(i) (((i) & 1<<28) != 0)
#define MSIM_SR_IRQ(i)   (((i) & 1) != 0)

#define MSIM_SET_NFLAG(x) ((x) |= (1<<31))
#define MSIM_SET_ZFLAG(x) ((x) |= (1<<30))
#define MSIM_SET_CFLAG(x) ((x) |= (1<<29))
#define MSIM_SET_VFLAG(x) ((x) |= (1<<28))

#define MSIM_CLEAR_NFLAG(x) ((x) &= ~(1<<31))
#define MSIM_CLEAR_ZFLAG(x) ((x) &= ~(1<<30))
#define MSIM_CLEAR_CFLAG(x) ((x) &= ~(1<<29))
#define MSIM_CLEAR_VFLAG(x) ((x) &= ~(1<<28))

#define MSIM_SIGN_EXTEND(v, t, s) ((v)|(((v)&(1<<((s)-1)))?(((1<<((t)-(s)))-1)<<(s)):(0)))

#define MSIM_GET_SUBOP(i) (((i) >> 12) & 1)
#define MSIM_GET_DREG(i) (((i) >> 8) & 15)
#define MSIM_GET_SREG(i) ((i) & 15)

#define MSIM_LOG printf

typedef enum {
	MSIM_ACCESS_BYTE = 0,
 	MSIM_ACCESS_HALFWORD = 1,
 	MSIM_ACCESS_WORD = 2,
} msim_mem_access_type;

typedef enum {
	MSIM_OPCODE_B	= 0,
	MSIM_OPCODE_ADD	= 1,
	MSIM_OPCODE_SUB	= 2,
	MSIM_OPCODE_CMP = 3,
	MSIM_OPCODE_MOV = 4,
	MSIM_OPCODE_LSH = 5,
	MSIM_OPCODE_BIT = 6,
	MSIM_OPCODE_MEM = 7
} msim_opcode_type;

typedef enum {
	MSIM_THIS_BANK 	= 0,
	MSIM_OTHER_BANK = 1
} msim_bank_type;

typedef enum {
	MSIM_SHIFT_LEFT = 0,
	MSIM_SHIFT_RIGHT = 1
} msim_shift_direction;

typedef enum {
	MSIM_BITOP_NOT	= 0,
	MSIM_BITOP_AND	= 1,
	MSIM_BITOP_ORR	= 2,
	MSIM_BITOP_EOR	= 3
} msim_bit_op_type;

typedef enum {
	MSIM_MEM_LOAD = 0,
	MSIM_MEM_STORE = 1
} msim_mem_op_type;

typedef enum {
	MSIM_MEM_HI = 0,
	MSIM_MEM_LO = 1
} msim_mem_hilo_type;

typedef enum {
	MSIM_MEM_DECREASE = 0,
	MSIM_MEM_INCREASE = 1
} msim_mem_direction;

typedef enum {
	MSIM_COND_EQ	= 0,
	MSIM_COND_NE	= 1,
	MSIM_COND_CS	= 2,
	MSIM_COND_HS	= 2,
	MSIM_COND_CC	= 3,
	MSIM_COND_LO	= 3,
	MSIM_COND_MI	= 4,
	MSIM_COND_PL	= 5,
	MSIM_COND_VS	= 6,
	MSIM_COND_VC	= 7,
	MSIM_COND_HI	= 8,
	MSIM_COND_LS	= 9,
	MSIM_COND_GE	= 10,
	MSIM_COND_LT	= 11,
	MSIM_COND_GT	= 12,
	MSIM_COND_LE	= 13,
	MSIM_COND_AL	= 14,
	MSIM_COND_NV	= 15
} msim_condition_type;

typedef enum {
	MSIM_R0		= 0,
	MSIM_R1		= 1,
	MSIM_R2		= 2,
	MSIM_R3		= 3,
	MSIM_R4		= 4,
	MSIM_R5		= 5,
	MSIM_R6		= 6,
	MSIM_R7		= 7,
	MSIM_R8		= 8,
	MSIM_R9		= 9,
	MSIM_R10	= 10,
	MSIM_R11	= 11,
	MSIM_R12	= 12,
	MSIM_R13	= 13,
	MSIM_R14	= 14,
	MSIM_R15	= 15,
	
	/* some useful aliases */
	MSIM_SP		= 11,		/* immediate register */
	MSIM_LR		= 12,		/* stack pointer */
	MSIM_IR		= 13,		/* link register */
	MSIM_SR		= 14,		/* status register */
	MSIM_PC		= 15		/* program counter */
} msim_register;

struct msim_instr {
	/* general to most instructions */
	msim_opcode_type	opcode;
	msim_register		destination;
	msim_register		source;
	msim_bank_type		destinationbank;
	msim_bank_type		sourcebank;
	int32_t			immediate;
	bool			immver;		/* gerenic flag to say if this
						is the immediate version of
						some instructions
						*/
	bool			subop;		/* fourth bit.  used to
						distinguish between the
						different versions of ADD, etc
						*/	
	/* specifics for branch */
	msim_condition_type	condition;
	
	/* specifics for move */
	bool			byteswap, halfwordswap;
	
	/* specifics for cmp */
	bool			cmp2reg, istst;
	
	/* specifics for logical shift */
	msim_shift_direction	shiftdirection;
	bool			roll, arithmetic;
	
	/* specifics for bit */
	msim_bit_op_type	bitop;
	bool			inverted;
	
	/* specifics for mem */
	msim_mem_op_type	memop;
	msim_mem_access_type	memsize;
	msim_mem_hilo_type	memhilo;
	msim_mem_direction	memdirection;
	bool			writeback;

};

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

struct msim_ctx {
	bool		init;
	bool		irqmode;
	bool		nopcincrement;
	u_int32_t	*r;
	u_int32_t	*ar;
	u_int32_t	realr[16];
	u_int32_t	realar[16];
	unsigned int	cyclecount;
	
	struct {
		 msim_read_mem	read;
		 msim_write_mem	write;
		 msim_reset_mem reset;
		 msim_device_tick tick;
		 void		*ctx;
		 u_int32_t	deviceid;
	}		areas[32];
	
	/* we also keep a pre-compiled non-sparse list of the tick functions
	 * as iterating through the above array for every cycle is expensive.
	 */
	msim_device_tick stick[32]; 	/* actual short list */
	unsigned int sticka[31]; 	/* shortlist entry's area numbers */
	unsigned int sticks; 		/* size of shortlist */
	
	msim_bnvop	bnvops[512];
	void		*bnvopsctx[512];
	
	struct msim_instr instr;
	
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
void msim_decode(struct msim_ctx *ctx, u_int16_t instrword, 
			struct msim_instr *instr);
void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr);
bool msim_cond_match(u_int32_t pc, msim_condition_type condition);

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
