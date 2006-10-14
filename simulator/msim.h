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

#define MSIM_PC_ADDR_MASK (~(1 | (15<<28)))
#define MSIM_SET_PC(x, y) ((x) = ((x) & ~MSIM_PC_ADDR_MASK) | (y) & MSIM_PC_ADDR_MASK)
#define MSIM_PC_NFLAG(i) (((i) & 1<<31) != 0)
#define MSIM_PC_ZFLAG(i) (((i) & 1<<30) != 0)
#define MSIM_PC_CFLAG(i) (((i) & 1<<29) != 0)
#define MSIM_PC_VFLAG(i) (((i) & 1<<28) != 0)

#define MSIM_SIGN_EXTEND(v, t, s) ((v) | (((v) & (1 << ((s)-1)))?(((1<<((t)-(s)))-1)<<(s)):(0)))

#define MSIM_GET_SUBOP(i) (((i) >> 12) & 1)
#define MSIM_GET_DREG(i) (((i) >> 8) & 15)
#define MSIM_GET_SREG(i) ((i) & 15)

#define MSIM_REG_PC	15
#define MSIM_REG_LR	14
#define MSIM_REG_SP	13
#define MSIM_REG_IR	12
#define MSIM_REG_AT	11

typedef enum {
	MSIM_ACCESS_BYTE = 0,
 	MSIM_ACCESS_HALFWORD = 1
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

struct msim_instr {
	/* general to most instructions */
	msim_opcode_type	opcode;
	int			destination;
	int			source;
	msim_bank_type		destinationbank;
	msim_bank_type		sourcebank;
	int16_t			immediate;
	bool			subop;		/* fourth bit.  used to
						distinguish between the
						different versions of ADD, etc
						*/	
	/* specifics for branch */
	msim_condition_type	condition;
	
	/* specifics for move */
	bool			byteswap, halfwordswap;
	
	/* specifics for logical shift */
	msim_shift_direction	shiftdirection;
	bool			roll, arithmetic;
	
	/* specifics for bit */
	msim_bit_op_type	bitop;
	bool			inverted;
	
	/* specifics for mem */
	msim_mem_op_type	memop;
	msim_mem_access_type	memsize;
	msim_mem_direction	memdirection;
	bool			writeback;

};

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
	bool		nopcincrement;
	u_int32_t	r[16];
	u_int32_t	ar[16];
	struct {
		 msim_read_mem	read;
		 msim_write_mem	write;
		 msim_reset_mem reset;
		 void		*ctx;
	}		areas[16];
	struct msim_instr instr;
};

struct msim_ctx *msim_init(void);
void msim_destroy(struct msim_ctx *ctx);
void msim_device_add(struct msim_ctx *ctx, const int area, msim_read_mem read,
 			msim_write_mem write, msim_reset_mem reset, 
 			void *fctx);
void msim_device_remove(struct msim_ctx *ctx, const int area);
void msim_run(struct msim_ctx *ctx, unsigned int instructions);

void msim_memset(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t,	u_int16_t d);
u_int16_t msim_memget(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t);

void msim_fetch_decode(struct msim_ctx *ctx, struct msim_instr *instr);
void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr);
bool msim_cond_match(u_int32_t pc, msim_condition_type condition);

#endif /* __MSIM_H__ */