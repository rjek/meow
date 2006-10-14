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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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


void msim_device_add(struct msim_ctx *ctx, const int area, msim_read_mem read,
 			msim_write_mem write, msim_reset_mem reset, 
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
	for (; instructions > 0; instructions--) {
		struct msim_instr instr;
		msim_fetch_decode(ctx, &(ctx->instr));
		msim_execute(ctx, &(ctx->instr));
	}
}

void msim_memset(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t,	u_int16_t d)
{
	int area = ptr >> 28;
	
	if (ctx->areas[area].write == NULL) {
		fprintf(stderr, "warning: attempt to write to %z, but no device is attached there.\n", ptr);
		return;
	}

	ctx->areas[area].write(ptr, t, d, ctx->areas[area].ctx);
}

u_int16_t msim_memget(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t)
{
	int area = ptr >> 28;
	
	if (ctx->areas[area].read == NULL) {
		fprintf(stderr, "warning: attempt to read from %x, but no device is attached there.\n", ptr);
		return 0;
	}
	
	return ctx->areas[area].read(ptr, t, ctx->areas[area].ctx);
}

void msim_fetch_decode(struct msim_ctx *ctx, struct msim_instr *instr)
{
	u_int16_t instrword = msim_memget(ctx, ctx->r[15] & MSIM_PC_ADDR_MASK,
						MSIM_ACCESS_HALFWORD);

	memset(instr, 0, sizeof(struct msim_instr));

	switch (instrword >> 29) {
		case 0:
			instr->opcode = MSIM_OPCODE_B;
			instr->condition = (instrword >> 9) & 15;
			instr->immediate =
				MSIM_SIGN_EXTEND(
					(instrword & (~(127<<10))) << 1, 16, 9);
			
			break;
		
		case 1:
			instr->opcode = MSIM_OPCODE_ADD;
			break;
			
		case 2:
			instr->opcode = MSIM_OPCODE_SUB;
			break;
			
		case 3:
			instr->opcode = MSIM_OPCODE_CMP;
			break;
			
		case 4:
			instr->opcode = MSIM_OPCODE_MOV;
			instr->subop = MSIM_GET_SUBOP(instrword);
			if (instr->subop == 0) {
				instr->destination = MSIM_GET_DREG(instrword);
				instr->source = MSIM_GET_SREG(instrword);
				instr->destinationbank =
						(instrword & (1<<5)) != 0;
				instr->sourcebank = (instrword & (1<<4)) != 0;
				instr->byteswap = (instrword & (1<<6)) != 0;
				instr->halfwordswap = (instrword & (1<<7)) != 0;
			} else {
				instr->destination = MSIM_REG_IR;
				instr->immediate = MSIM_SIGN_EXTEND(
					instrword & (~(15<<12)), 16, 12);
			}
			break;
			
		case 5:
			instr->opcode = MSIM_OPCODE_LSH;
			break;
		
		case 6:
			instr->opcode = MSIM_OPCODE_BIT;
			break;
			
		case 7:
			instr->opcode = MSIM_OPCODE_MEM;
			break;
	}
						
}

void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr)
{
	switch (instr->opcode) {
		case MSIM_OPCODE_B:
			if (msim_cond_match(ctx->r[15], instr->condition)) {
				MSIM_SET_PC(ctx->r[15], instr->immediate);
				ctx->nopcincrement = true;
			}
			break;
		
		case MSIM_OPCODE_ADD:
			break;
			
		case MSIM_OPCODE_SUB:
			break;
			
		case MSIM_OPCODE_CMP:
			break;
			
		case MSIM_OPCODE_MOV:
			if (instr->subop == 0) {
				/* mov rather than ldi */
				if (instr->destination == 15 &&
				    instr->source == 15 &&
				    instr->destinationbank == MSIM_OTHER_BANK &&
				    instr->sourcebank == MSIM_OTHER_BANK &&
				    instr->byteswap &&
				    instr->halfwordswap) {
					/* TODO: OMG IT BURNS MY EYES.
					 * This is the magical instruction for
					 * returning from the interrupt handler.
					 */
					fprintf(stderr,
						"warning: intrtn (at %x) is unimplemented.\n",
						ctx->r[15] & MSIM_PC_ADDR_MASK);
				} else {
					u_int32_t s;
					s = (instr->sourcebank == MSIM_THIS_BANK) ?
						ctx->r[instr->source] :
						ctx->ar[instr->source];
					
					if (instr->byteswap)
						s = (s & 0xff00ff00) >> 8 |
							(s & 0x00ff00ff) << 8;
					
					if (instr->halfwordswap)
						s = (s << 16) |	(s >> 16);
					
					if (instr->destinationbank == MSIM_THIS_BANK)
						if (instr->destination == 15) {
							MSIM_SET_PC(ctx->r[15], s & MSIM_PC_ADDR_MASK);
							ctx->nopcincrement = true;
						} else
							ctx->r[instr->destination] = s;
					else
						if (instr->destination == 15) {
							MSIM_SET_PC(ctx->ar[15], s & MSIM_PC_ADDR_MASK);
							ctx->nopcincrement = true;
						} else
							ctx->ar[instr->destination] = s;
				}
			} else {
				/* ldi rather than mov */
				ctx->r[MSIM_REG_IR] = instr->immediate;
			}
			break;
			
		case MSIM_OPCODE_LSH:
			break;
			
		case MSIM_OPCODE_BIT:
			break;
			
		case MSIM_OPCODE_MEM:
			break;
	}
	
	if (ctx->nopcincrement == true)
		ctx->nopcincrement == false;
	else
		MSIM_SET_PC(ctx->r[15], (ctx->r[15] & MSIM_PC_ADDR_MASK) + 2);
}

bool msim_cond_match(u_int32_t pc, msim_condition_type condition)
{
	bool nflag = MSIM_PC_NFLAG(pc);
	bool zflag = MSIM_PC_ZFLAG(pc);
	bool cflag = MSIM_PC_CFLAG(pc);
	bool vflag = MSIM_PC_VFLAG(pc);
	
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

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	struct msim_ctx *ctx = msim_init();
	
	msim_run(ctx, 5);
}

#endif