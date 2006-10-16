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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <sys/types.h>

#include "msim.h"

struct msim_ctx *msim_init(void)
{
	struct msim_ctx *ctx = calloc(1, sizeof(struct msim_ctx));
	
	assert(ctx != NULL);
	
	ctx->r = ctx->realr;
	ctx->ar = ctx->realar;
	
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

void msim_memset(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t,	u_int16_t d)
{
	int area = ptr >> 28;
	
	if (ctx->areas[area].write == NULL) {
		fprintf(stderr, "warning: attempt to write to %x, but no device is attached there.\n", ptr);
		return;
	}

	ctx->areas[area].write(ptr & ~(15<<28), t, d, ctx->areas[area].ctx);
}

u_int16_t msim_memget(struct msim_ctx *ctx, u_int32_t ptr,
			msim_mem_access_type t)
{
	int area = ptr >> 28;
	
	if (ctx->areas[area].read == NULL) {
		fprintf(stderr, "warning: attempt to read from %x, but no device is attached there.\n", ptr);
		return 0;
	}
	
	return ctx->areas[area].read(ptr & ~(15<<28), t, ctx->areas[area].ctx);
}

inline void msim_swap_banks(struct msim_ctx *ctx)
{	
	u_int32_t *t;
	
	MSIM_LOG("msim: swapping register banks\n");
	
	t = ctx->r;
	ctx->r = ctx->ar;
	ctx->ar = t;
}

void msim_irq(struct msim_ctx *ctx, int irq)
{
	/* TODO: design system/interrupt controller, and set the right irq bit
	 * there.
	 */
	 
	msim_swap_banks(ctx);
	ctx->r[15] = 33;		/* bit 0 is set for irq mode */
	ctx->nopcincrement = true;
}

inline u_int16_t msim_fetch(struct msim_ctx *ctx)
{
	return msim_memget(ctx, ctx->r[15] & MSIM_PC_ADDR_MASK,
						MSIM_ACCESS_HALFWORD);
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
					instr->immediate = instrword & 31;
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
				instr->destination = MSIM_REG_IR;
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
			if (instrword & 1<<6) {
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
			
			instr->writeback = ((instrword & 1<<4) != 0);
			instr->memdirection = (instrword & 1<<3) ?
						MSIM_MEM_INCREASE :
						MSIM_MEM_DECREASE;
			break;
	}
						
}

void msim_execute(struct msim_ctx *ctx, struct msim_instr *instr)
{
	u_int32_t tmp;
	switch (instr->opcode) {
		case MSIM_OPCODE_B:
			if (msim_cond_match(ctx->r[15], instr->condition)) {
				MSIM_SET_PC(ctx->r[15], ctx->r[15] + instr->immediate);
				ctx->nopcincrement = true;
			} else if (instr->condition == MSIM_COND_NV) {
				/* special msim meta-op - only one for now */
				switch (instr->immediate) {
					case 0:
						/* trigger an interrupt */
						msim_irq(ctx, 0);
						break;
					case 2:
						/* cause msim to exit(0); */
						exit(0);
						break;
					default:
						fprintf(stderr, "warning: unknown msim bnv instruction %x at %x\n",
							instr->immediate,
							ctx->r[15] & (~1));
						break;
				}
			}
			break;
		
		case MSIM_OPCODE_ADD:
			if (instr->subop == false)
				ctx->r[instr->destination] = 
					ctx->r[instr->destination] +
					ctx->r[instr->source] +
					instr->immediate;
			else
				ctx->r[instr->destination] = 
					ctx->r[instr->destination] +
					instr->immediate;
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
			break;
			
		case MSIM_OPCODE_CMP:
			break;
			
		case MSIM_OPCODE_MOV:
			if (instr->subop == false) {
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
					msim_swap_banks(ctx);
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
							MSIM_SET_PC(ctx->r[15],
							s & MSIM_PC_ADDR_MASK);
							ctx->nopcincrement = true;
						} else
							ctx->r[instr->destination] = s;
					else
						if (instr->destination == 15) {
							MSIM_SET_PC(ctx->ar[15],
							s & MSIM_PC_ADDR_MASK);
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
			break;
			
		case MSIM_OPCODE_MEM:
			
			if (instr->memop == MSIM_MEM_LOAD) {
				tmp = msim_memget(ctx, ctx->r[instr->source],
							instr->memsize);
				
				if (instr->memsize == MSIM_ACCESS_BYTE &&
					instr->memhilo == MSIM_MEM_LO) {
					ctx->r[instr->destination] &= (~0xff);
					ctx->r[instr->destination] |= tmp & 0xff;
				}
				
				if (instr->memsize == MSIM_ACCESS_BYTE &&
					instr->memhilo == MSIM_MEM_HI) {
					ctx->r[instr->destination] &= (~0xff0000);
					ctx->r[instr->destination] |=
						((tmp & 0xff) << 16);
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
			} else {
				/* store */
			}
			
			if (instr->writeback == true) {
				int delta = (instr->memsize == MSIM_ACCESS_BYTE)
						? 1 : 2;
				if (instr->memdirection == MSIM_MEM_DECREASE)
					delta = -delta;
				ctx->r[instr->source] += delta;
			}
			break;
	}
	
	if (ctx->nopcincrement == true)
		ctx->nopcincrement = false;
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

void msim_run(struct msim_ctx *ctx, unsigned int instructions)
{
	for (; instructions > 0; instructions--) {
		u_int16_t i = msim_fetch(ctx);
		char dis[256];
		msim_decode(ctx, i, &(ctx->instr));
		printf("msim: 0x%08x : %s\t\t\tcycle %d\n",
			ctx->r[15] & MSIM_PC_ADDR_MASK,
			msim_mnemonic(ctx, dis, 256, &(ctx->instr)),
			ctx->cyclecount);
		msim_execute(ctx, &(ctx->instr));
		ctx->cyclecount++;
	}
}

char *msim_mnemonic(struct msim_ctx *ctx, char *buf, unsigned int bufl, 
			struct msim_instr *instr)
{
	char tmp[256];
	
	if (bufl == 0)
		return buf;

	buf[0] = '\0';
	bufl--;

#define APPEND(x) do { strncat(buf, x, bufl); bufl -= strlen(x); } while(false)

	switch (instr->opcode) {
	case MSIM_OPCODE_B:
		APPEND("B");
		switch (instr->condition) {
		case MSIM_COND_EQ: APPEND("EQ"); break;
		case MSIM_COND_NE: APPEND("NE"); break;
		case MSIM_COND_CS: APPEND("CS"); break;
		case MSIM_COND_CC: APPEND("CC"); break;
		case MSIM_COND_MI: APPEND("MI"); break;
		case MSIM_COND_PL: APPEND("PL"); break;
		case MSIM_COND_VS: APPEND("VS"); break;
		case MSIM_COND_VC: APPEND("VC"); break;
		case MSIM_COND_HI: APPEND("HI"); break;
		case MSIM_COND_LS: APPEND("LS"); break;
		case MSIM_COND_GE: APPEND("GE"); break;
		case MSIM_COND_LT: APPEND("LT"); break;
		case MSIM_COND_GT: APPEND("GT"); break;
		case MSIM_COND_LE: APPEND("LE"); break;
		case MSIM_COND_AL: APPEND("  "); break;
		case MSIM_COND_NV: APPEND("NV"); break;
		}
		snprintf(tmp, 256, "\t#%d\t; 0x%08x", instr->immediate,
							instr->immediate);
		APPEND(tmp);
		break;
	
	case MSIM_OPCODE_ADD:
	case MSIM_OPCODE_SUB:
		APPEND(instr->opcode == MSIM_OPCODE_ADD ? "ADD" : "SUB");
		snprintf(tmp, 256, "\tR%d, ", instr->destination);
		APPEND(tmp);
		if (instr->subop == true) {
			snprintf(tmp, 256, "#%d\t; 0x%08x", instr->immediate,
							instr->immediate);
			APPEND(tmp);
		} else {
			snprintf(tmp, 256, "R%d, #%d\t; 0x%08x", instr->source,
					instr->immediate, instr->immediate);
			APPEND(tmp);
		}
		break;
	
	case MSIM_OPCODE_CMP:
		if (instr->subop == false) {
			APPEND("CMP");
			snprintf(tmp, 256, "\tR%d, #%d\t; 0x%08d",
				instr->destination, instr->immediate,
				instr->immediate);
			APPEND(tmp);
		} else if (instr->istst == true) {
			APPEND("TST");
			snprintf(tmp, 256, "\t%s%d, #%d",
				instr->destinationbank == MSIM_THIS_BANK ?
				"R" : "AR", instr->destination,
				instr->immediate);
			APPEND(tmp);
		} else {
			APPEND("CMP");
			snprintf(tmp, 256, "\t%s%d, %s%d",
				instr->destinationbank == MSIM_THIS_BANK ?
				"R" : "AR", instr->destination,
				instr->sourcebank == MSIM_THIS_BANK ?
				"R" : "AR", instr->source);
			APPEND(tmp);
		}
		break;
		
	case MSIM_OPCODE_MOV:
		if (instr->destination == 15 &&
			instr->source == 15 &&
			instr->destinationbank == MSIM_OTHER_BANK &&
			instr->sourcebank == MSIM_OTHER_BANK &&
			instr->halfwordswap == true &&
			instr->byteswap == true)
			APPEND("INTRTN");
		else {
			if (instr->subop == true) {
				APPEND("LDI");
				snprintf(tmp, 256, "\t#%d\t; 0x%08x",
					instr->immediate,
					instr->immediate);
				APPEND(tmp);	
			} else {
				APPEND("MOV");
				if (instr->halfwordswap == true) APPEND("W");
				if (instr->byteswap == true) APPEND("B");
				snprintf(tmp, 256, "\t%s%d, %s%d",
					instr->destinationbank == MSIM_THIS_BANK ?
					"R" : "AR", instr->destination,
					instr->sourcebank == MSIM_THIS_BANK ?
					"R" : "AR", instr->source);
				APPEND(tmp);
			}
		}
	
		break;
		
	case MSIM_OPCODE_LSH:
		if (instr->shiftdirection == MSIM_SHIFT_LEFT &&
			instr->roll == false &&
			instr->arithmetic == false) APPEND("LSL");
		if (instr->shiftdirection == MSIM_SHIFT_RIGHT &&
			instr->roll == false &&
			instr->arithmetic == false) APPEND("LSR");
		if (instr->shiftdirection == MSIM_SHIFT_LEFT &&
			instr->roll == false &&
			instr->arithmetic == true) APPEND("ASL");
		if (instr->shiftdirection == MSIM_SHIFT_RIGHT &&
			instr->roll == false &&
			instr->arithmetic == true) APPEND("ASR");
		if (instr->shiftdirection == MSIM_SHIFT_LEFT &&
			instr->roll == true) APPEND("ROL");
		if (instr->shiftdirection == MSIM_SHIFT_RIGHT &&
			instr->roll == true) APPEND("ROR");
		
		snprintf(tmp, 256, "\tR%d, ", instr->destination);
		APPEND(tmp);
		
		if (instr->immver)
			snprintf(tmp, 256, "#%d", instr->immediate);
		else
			snprintf(tmp, 256, "R%d", instr->source);
		APPEND(tmp);
	
		break;
		
	case MSIM_OPCODE_BIT:
		switch (instr->bitop) {
		case MSIM_BITOP_NOT: APPEND("NOT"); break;
		case MSIM_BITOP_AND: APPEND("AND"); break;
		case MSIM_BITOP_ORR: APPEND("ORR"); break;
		case MSIM_BITOP_EOR: APPEND("EOR"); break;
		}
		if (instr->inverted == true) APPEND(" INVERTED");
		snprintf(tmp, 256, "\tR%d, ", instr->destination);
		APPEND(tmp);
		if (instr->immver == true)
			snprintf(tmp, 256, "#%d", ffs(instr->immediate));
		else
			snprintf(tmp, 256, "R%d", instr->source);

		break;
			
	case MSIM_OPCODE_MEM:
		if (instr->memop == MSIM_MEM_LOAD)
			APPEND("LDR");
		else
			APPEND("STR");
		
		if (instr->memhilo == MSIM_MEM_HI)
			APPEND("HI");
		else
			APPEND("LO");
		
		if (instr->memsize == MSIM_ACCESS_BYTE)
			APPEND("B");
			
		if (instr->writeback == true) {
			if (instr->memdirection == MSIM_MEM_DECREASE)
				APPEND("D");
			else
				APPEND("I");
		}
		
		snprintf(tmp, 256, "\tR%d, R%d", instr->destination,
							instr->source);
		APPEND(tmp);
		
		break;
	}
	
	return buf;
#undef APPEND
}

void msim_print_state(struct msim_ctx *ctx)
{
	int i;
	
	printf("Bank R0       R1       R2       R3       R4       R5       R6       R7\n");
	printf("0    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->r[0], ctx->r[1], ctx->r[2], ctx->r[3], ctx->r[4], ctx->r[5], ctx->r[6], ctx->r[7]);
	printf("     R8       R9       R10      R11/AT   R12/IR   R13/SP   R14/LR   R15/PC\n");
	printf("0    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->r[8], ctx->r[9], ctx->r[10], ctx->r[11], ctx->r[12], ctx->r[13], ctx->r[14], ctx->r[15]);
		
	printf("     N Z C V a a a a a a a a a a a a a a a a a a a a a a a a a a a I\n0 PC ");
	for (i = 31; i >= 0; i--)
		printf("%s ", (ctx->r[15] & (1<<i)) ? "1" : "0");
	printf("\n");
	
	printf("     R0       R1       R2       R3       R4       R5       R6       R7\n");
	printf("1    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->ar[0], ctx->ar[1], ctx->ar[2], ctx->ar[3], ctx->ar[4], ctx->ar[5], ctx->ar[6], ctx->ar[7]);
	printf("     R8       R9       R10      R11/AT   R12/IR   R13/SP   R14/LR   R15/PC\n");
	printf("1    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->ar[8], ctx->ar[9], ctx->ar[10], ctx->ar[11], ctx->ar[12], ctx->ar[13], ctx->ar[14], ctx->ar[15]);

	printf("     N Z C V a a a a a a a a a a a a a a a a a a a a a a a a a a a I\n1 PC ");
	for (i = 31; i >= 0; i--)
		printf("%s ", (ctx->ar[15] & (1<<i)) ? "1" : "0");
	printf("\n\n");
}

static u_int16_t msim_rom_read(const u_int32_t ptr, msim_mem_access_type access, void *ctx)
{
	unsigned char *rom = (unsigned char *)ctx;
	
	if (access == MSIM_ACCESS_BYTE) {
		return rom[ptr];
	} else {
		return rom[ptr] | (rom[ptr + 1] << 8);
	}
	
}

static void msim_rom_write(const u_int32_t ptr, const msim_mem_access_type access, const u_int16_t d, void *ctx)
{
	fprintf(stderr, "warning: attempt to write value %x into ROM at %x\n", d, ptr);
}

void msim_add_rom_from_file(struct msim_ctx *ctx, int area, char *filename)
{
	FILE *fh = fopen(filename, "r");
	unsigned char *rom;
	int fs;

	if (fh == NULL) {
		fprintf(stderr, "warning: unable to open ROM file %s\n", filename);
		return;
	}
	
	fseek(fh, 0, SEEK_END);
	fs = ftell(fh);
	fseek(fh, 0, SEEK_SET);
	
	rom = malloc(fs);
	fread(rom, fs, 1, fh);
	fclose(fh);
	
	msim_device_add(ctx, area, msim_rom_read, msim_rom_write, NULL, rom);
}

void msim_del_rom(struct msim_ctx *ctx, int area)
{
	free(ctx->areas[area].ctx);
	msim_device_remove(ctx, area);
}

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	struct msim_ctx *ctx = msim_init();
	int i;
	
	msim_add_rom_from_file(ctx, 0, "masm.out");
	
	for (i = 50; i > 0; i--) {
		msim_run(ctx, 1);
		msim_print_state(ctx);
	}
	
	return 0;
}

#endif
