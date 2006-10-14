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
		msim_fetch_decode(ctx, &instr);
		msim_execute(ctx, &instr);
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
			instr->nflag = (instrword & 1<<13) != 0;
			instr->zflag = (instrword & 1<<14) != 0;
			instr->cflag = (instrword & 1<<15) != 0;
			instr->vflag = (instrword & 1<<16) != 0;
			instr->immediate = (instrword & (~(127<<10))) << 1;
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
			MSIM_SET_PC(ctx->r[15], instr->immediate);
			ctx->nopcincrement = true;
			break;
	}
	
	if (ctx->nopcincrement == true)
		ctx->nopcincrement == false;
	else
		MSIM_SET_PC(ctx->r[15], (ctx->r[15] & MSIM_PC_ADDR_MASK) + 2);
}

#ifdef TEST_RIG

int main(int argc, char *argv[])
{
	struct msim_ctx *ctx = msim_init();
	
	msim_run(ctx, 5);
}

#endif
