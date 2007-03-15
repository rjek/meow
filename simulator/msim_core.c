/*
 * msim_core.c
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

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/types.h>

#include "msim_core.h"

struct msim_ctx *msim_init(void)
{
	struct msim_ctx *ctx = calloc(1, sizeof(struct msim_ctx));
	int i;
	
	assert(ctx != NULL);
	
	ctx->r = ctx->realr;
	ctx->ar = ctx->realar;
	
	ctx->ar[MSIM_SR] = 1;	/* set bit indicating interrupt mode */
	
	/* set all device IDs to 0xffffffff, meaning "none installed" */
	for (i = 0; i < 32; i++)
		ctx->areas[i].deviceid = 0xffffffff;
	
	msim_add_builtin_bnvs(ctx);
	
	return ctx;
}

void msim_destroy(struct msim_ctx *ctx)
{
	msim_del_builtin_bnvs(ctx);
	free(ctx);
}

/* -- BNV handling -------------------------------------------------------- */

void msim_add_bnv(struct msim_ctx *ctx, signed int op, msim_bnvop func,
			void *fctx)
{
	ctx->bnvops[op + 256] = func;
	ctx->bnvopsctx[op + 256] = fctx;
}

void msim_del_bnv(struct msim_ctx *ctx, signed int op)
{
	msim_add_bnv(ctx, op, NULL, NULL);
}

static void msim_builtin_get_model(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	ctx->r[MSIM_IR] = 0x00000100;
}

static void msim_builtin_get_bus_id(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	ctx->r[MSIM_IR] = 0;
}

static void msim_builtin_irqrtn(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	if (MSIM_SR_IRQ(ctx->r[MSIM_SR])) {
		/* only do the swap if we're in interrupt mode */
		msim_swap_banks(ctx);
	}
}

static void msim_builtin_exit(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	fprintf(stdout, "msim: simulated system called HALT\n");
	exit(ctx->r[MSIM_IR]);
}

static void msim_builtin_dump_state(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	msim_print_state(ctx);
}

static void msim_builtin_print(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	intptr_t type = (intptr_t)bnvctx;
	
	switch (type) {
	case 0: printf("%c", ctx->r[MSIM_IR]); break;
	case 1: printf("%d", ctx->r[MSIM_IR]); break;
	case 2: printf("%x", ctx->r[MSIM_IR]); break;
	}
}

void msim_add_builtin_bnvs(struct msim_ctx *ctx)
{
	/* positive values are architecture-defined */
	msim_add_bnv(ctx, 0, msim_builtin_get_model, NULL);
	msim_add_bnv(ctx, 2, msim_builtin_get_bus_id, NULL);
	msim_add_bnv(ctx, 4, msim_builtin_irqrtn, NULL);
	
	/* negative values are implementation-defined */
	msim_add_bnv(ctx, -2, msim_builtin_exit, NULL);
	msim_add_bnv(ctx, -4, msim_builtin_dump_state, NULL);
	msim_add_bnv(ctx, -6, msim_builtin_print, (void *) 0);
	msim_add_bnv(ctx, -8, msim_builtin_print, (void *) 1);
	msim_add_bnv(ctx, -10, msim_builtin_print, (void *) 2);
}

void msim_del_builtin_bnvs(struct msim_ctx *ctx)
{
	unsigned int i;
	
	for (i = -10; i < 5; i += 2)
		msim_del_bnv(ctx, i);
}

/* -- Device/Chipselect handling ------------------------------------------ */

void msim_device_add(struct msim_ctx *ctx, const int area, const u_int32_t id,
			msim_read_mem read, msim_write_mem write, 
			msim_reset_mem reset, void *fctx)
{
	ctx->areas[area].read = read;
	ctx->areas[area].write = write;
	ctx->areas[area].reset = reset;
	ctx->areas[area].ctx = fctx;
	ctx->areas[area].deviceid = id;
}

void msim_device_remove(struct msim_ctx *ctx, const int area)
{
	ctx->areas[area].read = NULL;
	ctx->areas[area].write = NULL;
	ctx->areas[area].reset = NULL;
	ctx->areas[area].ctx = NULL;
	ctx->areas[area].deviceid = 0xffffffff;
}

void msim_memset(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t,	u_int32_t d)
{
	int area = ptr >> 27;
	
	if (ctx->areas[area].write == NULL) {
		fprintf(stdout,
	"warning: attempt to write to %x, but no device is attached there.\n",
			ptr);
		return;
	}

	ctx->areas[area].write(ptr & ~(31<<27), t, d, ctx->areas[area].ctx);
}

u_int32_t msim_memget(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t)
{
	int area = ptr >> 27;
	
	if (ctx->areas[area].read == NULL) {
		fprintf(stdout,
	"warning: attempt to read from %x, but no device is attached there.\n",
			ptr);
		return 0;
	}
	
	return ctx->areas[area].read(ptr & ~(31<<27), t, ctx->areas[area].ctx);
}

/* -- Simulator code------------------------------------------------------- */

inline void msim_swap_banks(struct msim_ctx *ctx)
{	
	u_int32_t *t;
	
	MSIM_LOG("msim: swapping register banks\n");
	
	t = ctx->r;
	ctx->r = ctx->ar;
	ctx->ar = t;
}

void msim_irq(struct msim_ctx *ctx)
{
	msim_swap_banks(ctx);
	ctx->r[MSIM_PC] = 32;
	ctx->nopcincrement = true;
}

inline u_int16_t msim_fetch(struct msim_ctx *ctx)
{
	return msim_memget(ctx, ctx->r[MSIM_PC], MSIM_ACCESS_HALFWORD);
}

void msim_decode(struct msim_ctx *ctx, u_int16_t instrword,
			struct msim_instr *instr)
{
	memset(instr, 0, sizeof(struct msim_instr));

	switch (instrword >> 13) {
	case 0:
		instr->opcode = MSIM_OPCODE_B;
		instr->condition = (instrword >> 9) & 15;
		instr->immediate =
			MSIM_SIGN_EXTEND(
				(instrword & (~(127<<9))), 16, 9) * 2;
		
		break;
		
	case 1:
		instr->opcode = MSIM_OPCODE_ADD;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->destination = MSIM_GET_DREG(instrword);
		if (instr->subop == false) {
			instr->source = MSIM_GET_SREG(instrword);
			instr->immediate = (instrword >> 4) & 15;
		} else {
			instr->immediate = (instrword & 0xff);
		}		
		break;
			
	case 2:
		instr->opcode = MSIM_OPCODE_SUB;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->destination = MSIM_GET_DREG(instrword);
		if (instr->subop == false) {
			instr->source = MSIM_GET_SREG(instrword);
			instr->immediate = (instrword >> 4) & 15;
		} else {
			instr->immediate = (instrword & 0xff);
		}
		break;
		
	case 3:
		instr->opcode = MSIM_OPCODE_CMP;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->destination = MSIM_GET_DREG(instrword);
		instr->source = MSIM_GET_SREG(instrword);
		if (instr->subop == false)
			/* compare dreg to 8 bit immediate */
			instr->immediate = MSIM_SIGN_EXTEND(
				instrword & 0xff, 32, 8);
		else {
			if (instrword & (1<<7)) {
				/* tst */
				instr->destinationbank =
					(instrword & (1<<5)) != 0;
				instr->immediate = 1<<(instrword & 31);
				instr->istst = true;
			} else {
				/* cmp two registers */
				instr->source = MSIM_GET_SREG(instrword);
				instr->destinationbank =
					(instrword & (1<<5)) != 0;
				instr->sourcebank =
					(instrword & (1<<4)) != 0;
				instr->cmp2reg = true;
			}
		}
		break;
			
	case 4:
		instr->opcode = MSIM_OPCODE_MOV;
		instr->subop = MSIM_GET_SUBOP(instrword);
		if (instr->subop == false) {
			instr->destination = MSIM_GET_DREG(instrword);
			instr->source = MSIM_GET_SREG(instrword);
			instr->destinationbank =
					(instrword & (1<<5)) != 0;
			instr->sourcebank = (instrword & (1<<4)) != 0;
			instr->byteswap = (instrword & (1<<6)) != 0;
			instr->halfwordswap = (instrword & (1<<7)) != 0;
		} else {
			instr->destination = MSIM_IR;
			instr->immediate = MSIM_SIGN_EXTEND(
				instrword & (~(15<<12)), 16, 12);
		}
		break;
			
	case 5:
		instr->opcode = MSIM_OPCODE_LSH;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->arithmetic = instr->subop;
		instr->roll = (instrword & (1<<6)) != 0;
		instr->destination = MSIM_GET_DREG(instrword);
		instr->shiftdirection = (instrword & (1<<7))
						? MSIM_SHIFT_LEFT
						: MSIM_SHIFT_RIGHT;
		if ((instrword & (1<<5)) != 0)
			instr->source = MSIM_GET_SREG(instrword);
		else {
			instr->immediate = instrword & 31;
			instr->immver = true;
		}
		
		break;
	
	case 6:
		instr->opcode = MSIM_OPCODE_BIT;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->inverted = instr->subop;
		instr->destination = MSIM_GET_DREG(instrword);
		if (instrword & (1<<5)) {
			instr->immver = true;
			instr->immediate = 1 << (instrword & 31);
		} else {
			instr->immver = false;
			instr->source = MSIM_GET_SREG(instrword);
		}
		instr->bitop = (instrword >> 6) & 3;
		break;
			
	case 7:
		instr->opcode = MSIM_OPCODE_MEM;
		instr->subop = MSIM_GET_SUBOP(instrword);
		instr->memop = instr->subop ? MSIM_MEM_STORE :
						MSIM_MEM_LOAD;
		instr->destination = MSIM_GET_DREG(instrword);
		instr->source = MSIM_GET_SREG(instrword);
		
		instr->memsize = (instrword & 1<<7) ?
					MSIM_ACCESS_HALFWORD :
					MSIM_ACCESS_BYTE;
		
		instr->memhilo = (instrword & 1<<6) ?
					MSIM_MEM_LO : MSIM_MEM_HI;
		
		instr->writeback = ((instrword & 1<<5) != 0);
		instr->memdirection = (instrword & 1<<4) ?
					MSIM_MEM_INCREASE :
					MSIM_MEM_DECREASE;
					
		/* make use of a useless encoding for doing 32 bit accesses */
		if (instr->memsize == MSIM_ACCESS_BYTE &&
			instr->memhilo == MSIM_MEM_LO)
			instr->memsize = MSIM_ACCESS_WORD;
		break;
	}
						
}

void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr)
{
	u_int32_t tmp = 0, alu;
	switch (instr->opcode) {
	case MSIM_OPCODE_B:
		if (msim_cond_match(ctx->r[MSIM_SR], instr->condition)) {
			ctx->r[MSIM_PC] += instr->immediate;
			ctx->nopcincrement = true;
		} else if (instr->condition == MSIM_COND_NV) {
			signed int operation = (instr->immediate) + 256;
			if (ctx->bnvops[operation] == NULL) {
				fprintf(stderr,
				"msim: unhandled BNV %d at %08x\n",
				instr->immediate,
				ctx->r[MSIM_PC]);	
			} else {
				ctx->bnvops[operation](ctx, instr->immediate,
					ctx->bnvopsctx[operation]);
			}
		}
		break;
		
	case MSIM_OPCODE_ADD:
		if (instr->subop == false)
		  	if (instr->immediate == 0)
				ctx->r[instr->destination] +=
					ctx->r[instr->source];
			else
				ctx->r[instr->destination] = 
					ctx->r[instr->source] +
					instr->immediate;
		else
			ctx->r[instr->destination] = 
				ctx->r[instr->destination] +
				instr->immediate;
		
		if (instr->destination == MSIM_PC) ctx->nopcincrement = true;
		break;
		
	case MSIM_OPCODE_SUB:
		if (instr->subop == false)
			ctx->r[instr->destination] = 
				ctx->r[instr->destination] -
				ctx->r[instr->source] -
				instr->immediate;
		else
			ctx->r[instr->destination] = 
				ctx->r[instr->destination] -
				instr->immediate;
		
		if (instr->destination == MSIM_PC) ctx->nopcincrement = true;
		break;
			
	case MSIM_OPCODE_CMP:
		if (instr->subop == true && instr->istst == true) {
			tmp = (instr->destinationbank == MSIM_THIS_BANK)
				? ctx->r[instr->destination]
				: ctx->ar[instr->destination];
			tmp &= (instr->immediate);
			
			if (tmp & (1<<31))
				MSIM_SET_NFLAG(ctx->r[MSIM_SR]);
			else
				MSIM_CLEAR_NFLAG(ctx->r[MSIM_SR]);
					
			if (tmp == 0)
				MSIM_SET_ZFLAG(ctx->r[MSIM_SR]);
			else
				MSIM_CLEAR_ZFLAG(ctx->r[MSIM_SR]);
			
			break;
		} else if (instr->subop == true) {
			tmp = (instr->sourcebank == MSIM_THIS_BANK)
				? ctx->r[instr->source]
				: ctx->ar[instr->source];
			
		} else {
			tmp = instr->immediate;
		}
		
		alu = (instr->destinationbank == MSIM_THIS_BANK)
			? ctx->r[instr->destination]
			: ctx->ar[instr->destination];
		
		if (alu < tmp)
			MSIM_SET_CFLAG(ctx->r[MSIM_SR]);
		else
			MSIM_CLEAR_CFLAG(ctx->r[MSIM_SR]);		
	
		alu -= tmp;
		
		if (alu & (1<<31))
			MSIM_SET_NFLAG(ctx->r[MSIM_SR]);
		else
			MSIM_CLEAR_NFLAG(ctx->r[MSIM_SR]);
			
		if (alu == 0)
			MSIM_SET_ZFLAG(ctx->r[MSIM_SR]);
		else
			MSIM_CLEAR_ZFLAG(ctx->r[MSIM_SR]);
		
		MSIM_CLEAR_VFLAG(ctx->r[MSIM_SR]);
			
		break;
		
	case MSIM_OPCODE_MOV:
		if (instr->subop == false) {
			/* mov rather than ldi */
			u_int32_t s;
			s = (instr->sourcebank == MSIM_THIS_BANK) ?
				ctx->r[instr->source] :
				ctx->ar[instr->source];
			
			if (instr->byteswap)
				s = (s & 0xff00ff00) >> 8 |
					(s & 0x00ff00ff) << 8;
			
			if (instr->halfwordswap)
				s = (s << 16) |	(s >> 16);
			
			if (instr->destinationbank == MSIM_THIS_BANK) {
				ctx->r[instr->destination] = s;
				if (instr->destination == MSIM_PC)
					ctx->nopcincrement = true;
			} else {
				ctx->ar[instr->destination] = s;
			}
		} else {
			/* ldi rather than mov */
			ctx->r[MSIM_IR] = instr->immediate;
		}
		break;
		
	case MSIM_OPCODE_LSH:
		tmp = (instr->immver == true) ? instr->immediate :
			ctx->r[instr->source];

		if (instr->arithmetic == false &&
		    instr->roll == false) {
			if (instr->shiftdirection == MSIM_SHIFT_RIGHT)
				ctx->r[instr->destination] >>= tmp;
			else
				ctx->r[instr->destination] <<= tmp;
		}
			
		if (instr->arithmetic == true &&
			instr->roll == false) {
			if (instr->shiftdirection == MSIM_SHIFT_RIGHT)
				ctx->r[instr->destination] =
				 MSIM_SIGN_EXTEND(
				  ctx->r[instr->destination] >> tmp,
				   32, 32 - tmp);
			else
				ctx->r[instr->destination] = 
				(ctx->r[instr->destination] & 1<<31) |
				 ((ctx->r[instr->destination] << tmp) &
				  (~(1<<31)));
		}
				
		if (instr->roll == true) {
			if (instr->shiftdirection == MSIM_SHIFT_RIGHT)
				ctx->r[instr->destination] =
				 ((ctx->r[instr->destination]) >> tmp) | 
				  ((ctx->r[instr->destination]) << 
				   (32 - tmp));
			else
				ctx->r[instr->destination] =
				((ctx->r[instr->destination]) << tmp) | 
				 ((ctx->r[instr->destination]) >> 
				  (32 - tmp));
		}
		
		if (instr->destination == MSIM_PC) ctx->nopcincrement = true;
		break;
			
	case MSIM_OPCODE_BIT:
		tmp = (instr->immver == true) ? instr->immediate :
			ctx->r[instr->source];
		if (instr->inverted)
			tmp = ~tmp;
			
		switch (instr->bitop) {
		case MSIM_BITOP_NOT:
			ctx->r[instr->destination] = ~tmp;
			break;
		case MSIM_BITOP_AND:
			ctx->r[instr->destination] &= tmp;
			break;
		case MSIM_BITOP_ORR:
			ctx->r[instr->destination] |= tmp;
			break;
		case MSIM_BITOP_EOR:
			ctx->r[instr->destination] ^= tmp;
			break;
		}
		if (instr->destination == MSIM_PC) ctx->nopcincrement = true;
		break;
			
	case MSIM_OPCODE_MEM:	
		if (instr->memop == MSIM_MEM_LOAD) {
			tmp = msim_memget(ctx, ctx->r[instr->source],
						instr->memsize);
		
			if (instr->memsize == MSIM_ACCESS_BYTE) {
				ctx->r[instr->destination] &= (~0xff);
				ctx->r[instr->destination] |= tmp & 0xff;
			}
			
			if (instr->memsize == MSIM_ACCESS_HALFWORD &&
				instr->memhilo == MSIM_MEM_LO) {
				ctx->r[instr->destination] &= (~0xffff);
				ctx->r[instr->destination] |= tmp & 0xffff;
			}
			
			if (instr->memsize == MSIM_ACCESS_HALFWORD &&
				instr->memhilo == MSIM_MEM_HI) {
				ctx->r[instr->destination] &= (~0xffff0000);
				ctx->r[instr->destination] |=
					((tmp & 0xffff) << 16);	
			}
			
			if (instr->memsize == MSIM_ACCESS_WORD) {
				ctx->r[instr->destination] = tmp;
			}
		} else {
			if (instr->memsize == MSIM_ACCESS_BYTE) {
				tmp = ctx->r[instr->destination] & 0xff;
			}
			
			if (instr->memsize == MSIM_ACCESS_HALFWORD &&
				instr->memhilo == MSIM_MEM_LO) {
				tmp = ctx->r[instr->destination] & 0xffff;
			}
			
			if (instr->memsize == MSIM_ACCESS_HALFWORD &&
				instr->memhilo == MSIM_MEM_HI) {
				tmp = (ctx->r[instr->destination] >> 16) & 
					0xffff;
			}
			
			if (instr->memsize == MSIM_ACCESS_WORD) {
				tmp = ctx->r[instr->destination];
			}
			
			msim_memset(ctx, ctx->r[instr->source],
					instr->memsize, tmp);
		}
			
		if (instr->writeback == true) {
			int delta = 0;
			
			switch (instr->memsize) {
			case MSIM_ACCESS_BYTE:
				delta = 1;
				break;
			case MSIM_ACCESS_HALFWORD:
				delta = 2;
				break;
			case MSIM_ACCESS_WORD:
				delta = 4;
				break;
			}
			if (instr->memdirection == MSIM_MEM_DECREASE)
				delta = -delta;
			ctx->r[instr->source] += delta;
		}
		
		if (instr->destination == MSIM_PC) ctx->nopcincrement = true;
		break;
	}
	
	if (ctx->nopcincrement == true)
		ctx->nopcincrement = false;
	else
		ctx->r[MSIM_PC] += 2;
}

inline bool msim_cond_match(u_int32_t sr, msim_condition_type condition)
{
	bool nflag = MSIM_SR_NFLAG(sr);
	bool zflag = MSIM_SR_ZFLAG(sr);
	bool cflag = MSIM_SR_CFLAG(sr);
	bool vflag = MSIM_SR_VFLAG(sr);
	
	switch (condition) {
	case MSIM_COND_EQ: return zflag == true;
	case MSIM_COND_NE: return zflag == false;
	case MSIM_COND_CS: return cflag == true;
	case MSIM_COND_CC: return cflag == false;
	case MSIM_COND_MI: return nflag == true;
	case MSIM_COND_PL: return nflag == false;
	case MSIM_COND_VS: return vflag == true;
	case MSIM_COND_VC: return vflag == false;
	case MSIM_COND_HI: return (cflag == true) && (zflag == false);
	case MSIM_COND_LS: return (cflag == false) && (zflag == true);
	case MSIM_COND_GE: return nflag == vflag;
	case MSIM_COND_LT: return nflag != vflag;
	case MSIM_COND_GT: return (nflag == vflag) && (zflag == false);
	case MSIM_COND_LE: return (nflag != vflag) || (zflag == true);
	case MSIM_COND_AL: return true;
	case MSIM_COND_NV: return false;
	}
	
	return false;
}

void msim_run(struct msim_ctx *ctx, unsigned int instructions, bool trace)
{
	for (; instructions > 0; instructions--) {
		u_int16_t i = msim_fetch(ctx);
		char dis[256];
		msim_decode(ctx, i, &(ctx->instr));
		if (trace == true) {
			int b;
			
			printf("0x%08x : ", ctx->r[MSIM_PC]);
			
			for (b = 15; b >= 0; b--) {
				printf("%s", (i) & (1<<b) ? "1":"0");
				if (b % 4 == 0) printf(" ");
			}
			
			printf(": %-30s cycle %d\n",
				msim_mnemonic(ctx, dis, 256, &(ctx->instr)),
				ctx->cyclecount);
		}
				
		msim_execute(ctx, &(ctx->instr));
		ctx->cyclecount++;
	}
}

/* -- Simple built-in devices---------------------------------------------- */

struct msim_rom_ctx {
	unsigned char 	*rom;
	size_t		size;
};

static u_int32_t msim_rom_read(const u_int32_t ptr, msim_mem_access_type access,
				void *ctx)
{
	struct msim_rom_ctx *mctx = (struct msim_rom_ctx *)ctx;
	unsigned char *rom = mctx->rom;
	u_int32_t r = 0;
	
	
	if (ptr > mctx->size)
		return 0;
	
	switch (access) {
	case MSIM_ACCESS_BYTE:
		r = rom[ptr];
		break;
	case MSIM_ACCESS_HALFWORD:
		r = rom[ptr] | (rom[ptr + 1] << 8);
		break;
	case MSIM_ACCESS_WORD:
		r = rom[ptr] | (rom[ptr + 1] << 8) | (rom[ptr + 2] << 16) |
			(rom[ptr + 3] << 24);
		break;
	}
	
	return r;	
}

static void msim_rom_write(const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *ctx)
{
	fprintf(stderr,
		"warning: attempt to write value %x into ROM at %x\n", d, ptr);
}

void msim_add_rom_from_file(struct msim_ctx *ctx, int area, char *filename)
{
	FILE *fh = fopen(filename, "r");
	struct msim_rom_ctx *mctx;
	int fs;

	if (fh == NULL) {
		fprintf(stderr,
			"warning: unable to open ROM file %s\n", filename);
		return;
	}
	
	mctx = malloc(sizeof(struct msim_rom_ctx));
	
	fseek(fh, 0, SEEK_END);
	fs = ftell(fh);
	fseek(fh, 0, SEEK_SET);
	
	mctx->rom = malloc(fs);
	mctx->size = fs;
	fread(mctx->rom, fs, 1, fh);
	fclose(fh);
	
	msim_device_add(ctx, area, 0x00000000, msim_rom_read, msim_rom_write,
			NULL, mctx);
}

void msim_del_rom(struct msim_ctx *ctx, int area)
{
	struct msim_rom_ctx *mctx =
		(struct msim_rom_ctx *)ctx->areas[area].ctx;
	free(mctx->rom);
	free(ctx->areas[area].ctx);
	msim_device_remove(ctx, area);
}

struct msim_ram_ctx {
	unsigned char 	*ram;
	size_t		size;
};

static u_int32_t msim_ram_read(const u_int32_t ptr, msim_mem_access_type access,
				void *ctx)
{
	struct msim_ram_ctx *mctx = (struct msim_ram_ctx *)ctx;
	unsigned char *ram = mctx->ram;
	u_int32_t r = 0;
	
	if (ptr > mctx->size)
		return 0;
	
	switch (access) {
	case MSIM_ACCESS_BYTE:
		r = ram[ptr];
		break;
	case MSIM_ACCESS_HALFWORD:
		r = ram[ptr] | (ram[ptr + 1] << 8);
		break;
	case MSIM_ACCESS_WORD:
		r = ram[ptr] | (ram[ptr + 1] << 8) | (ram[ptr + 2] << 16) |
			(ram[ptr + 3] << 24);
		break;
	}
	
	return r;
}

static void msim_ram_write(const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *ctx)
{
	struct msim_ram_ctx *mctx = (struct msim_ram_ctx *)ctx;
	unsigned char *ram = mctx->ram;
	
	if (ptr > mctx->size)
		return;
	
	switch (access) {
	case MSIM_ACCESS_BYTE:
		ram[ptr] = d & 0xff;
		break;
	case MSIM_ACCESS_HALFWORD:
		ram[ptr] = (d & 0xff);
		ram[ptr + 1] = (d >> 8) & 0xff;
		break;
	case MSIM_ACCESS_WORD:
		ram[ptr] = (d & 0xff);
		ram[ptr + 1] = (d >> 8) & 0xff;
		ram[ptr + 2] = (d >> 16) & 0xff;
		ram[ptr + 3] = (d >> 24) & 0xff;			
		break;
	}
}

void msim_add_ram(struct msim_ctx *ctx, int area, size_t size)
{
	struct msim_ram_ctx *mctx = malloc(sizeof(struct msim_ram_ctx));
	unsigned char *ram = calloc(size, 1);
	
	mctx->ram = ram;
	mctx->size = size;
	
	msim_device_add(ctx, area, 0x00000001, msim_ram_read, msim_ram_write, 
			NULL, mctx);
}

void msim_del_ram(struct msim_ctx *ctx, int area)
{
	struct msim_ram_ctx *mctx =
		(struct msim_ram_ctx *)ctx->areas[area].ctx;
	free(mctx->ram);
	free(ctx->areas[area].ctx);
	msim_device_remove(ctx, area);
}

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	struct msim_ctx *ctx = msim_init();
	int i;
	
	msim_add_rom_from_file(ctx, 0, "masm.out");
	msim_add_ram(ctx, 1, 4096);
	
	for (i = (argc < 2) ? 10 : atoi(argv[1]); i > 0; i--) {
		msim_run(ctx, 1);
		msim_print_state(ctx);
	}
	
	msim_del_rom(ctx, 0);
	msim_del_ram(ctx, 1);
	msim_destroy(ctx);
	
	return 0;
}

#endif
