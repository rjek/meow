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
#include <time.h>
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
	ctx->ncpus = 1;
	ctx->running = true;
	
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
	ctx->r[MSIM_IR] = 0x00000000;		/* the MEOW project's msim, revision 0 */
}

static void msim_builtin_get_bus_id(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	ctx->r[MSIM_IR] = ctx->cpu;
}

static void msim_builtin_wfi(struct msim_ctx *ctx, signed int op,
				void *bnvctx)
{
	ctx->wfi = true;		/* the Chairman's tick wakes it */
}

static void msim_builtin_irqrtn(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	if (MSIM_SR_IRQ(ctx->r[MSIM_SR])) {
		/* only do the swap if we're in interrupt mode */
		msim_swap_banks(ctx);
		ctx->nopcincrement = true;
	}
}

static void msim_builtin_exit(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	fflush(stdout);
	if (ctx->trace != NULL) {
		fclose(ctx->trace);
	}
	exit(ctx->r[MSIM_IR]);
}

static void msim_builtin_dump_state(struct msim_ctx *ctx, signed int op,
					void *bnvctx)
{
	msim_print_state(ctx);
}

static void msim_builtin_time(struct msim_ctx *ctx, signed int op,
				void *bnvctx)
{
	ctx->r[MSIM_IR] = (u_int32_t)time(NULL);
}

static void msim_builtin_cycles(struct msim_ctx *ctx, signed int op,
				void *bnvctx)
{
	ctx->r[MSIM_IR] = ctx->cyclecount;
}

/* One character from standard input into ir, or -1 at its end.  Output
 * is flushed first so a prompt shows before the wait. */
static void msim_builtin_getc(struct msim_ctx *ctx, signed int op,
				void *bnvctx)
{
	int c;

	fflush(stdout);
	c = getchar();
	ctx->r[MSIM_IR] = c == EOF ? 0xffffffffu : (u_int32_t)c;
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
	
	fflush(stdout);
}

void msim_add_builtin_bnvs(struct msim_ctx *ctx)
{
	/* positive values are architecture-defined */
	msim_add_bnv(ctx, 0, msim_builtin_get_model, NULL);
	msim_add_bnv(ctx, 2, msim_builtin_get_bus_id, NULL);
	msim_add_bnv(ctx, 4, msim_builtin_irqrtn, NULL);
	msim_add_bnv(ctx, 6, msim_builtin_wfi, NULL);
	
	/* negative values are implementation-defined */
	msim_add_bnv(ctx, -2, msim_builtin_exit, NULL);
	msim_add_bnv(ctx, -4, msim_builtin_dump_state, NULL);
	msim_add_bnv(ctx, -6, msim_builtin_print, (void *) 0);
	msim_add_bnv(ctx, -8, msim_builtin_print, (void *) 1);
	msim_add_bnv(ctx, -10, msim_builtin_print, (void *) 2);
	msim_add_bnv(ctx, -12, msim_builtin_getc, NULL);
	msim_add_bnv(ctx, -14, msim_builtin_time, NULL);
	msim_add_bnv(ctx, -16, msim_builtin_cycles, NULL);
}

void msim_del_builtin_bnvs(struct msim_ctx *ctx)
{
	unsigned int i;
	
	for (i = -10; i < 5; i += 2)
		msim_del_bnv(ctx, i);
}

/* -- Device/Chipselect handling ------------------------------------------ */

void msim_device_add(struct msim_ctx *ctx, const unsigned int area, 
			const u_int32_t id, msim_read_mem read,
			msim_write_mem write, msim_reset_mem reset, 
			msim_device_tick tick, void *fctx)
{
	ctx->areas[area].read = read;
	ctx->areas[area].write = write;
	ctx->areas[area].reset = reset;
	ctx->areas[area].tick = tick;
	ctx->areas[area].ctx = fctx;
	ctx->areas[area].deviceid = id;
	ctx->areas[area].size = 0;
	
	if (tick != NULL) {
		/* ticked in chip-select order, so that a device's interrupt,
		 * raised in its tick, is seen by the Chairman's in the same
		 * cycle, as the hardware sees it before the next instruction */
		unsigned int i, j;

		assert(ctx->sticks < 31);
		for (i = 0; i < ctx->sticks && ctx->sticka[i] < area; i++) {
		}
		for (j = ctx->sticks; j > i; j--) {
			ctx->stick[j] = ctx->stick[j - 1];
			ctx->sticka[j] = ctx->sticka[j - 1];
		}
		ctx->stick[i] = tick;
		ctx->sticka[i] = area;
		ctx->sticks++;
	}
}

void msim_device_remove(struct msim_ctx *ctx, const unsigned int area)
{
	if (ctx->areas[area].tick != NULL) {
		/* we need to remove it from the ticker shortlist.
		 * first of all, find out which one in the short list it is
		 */
		 int a, i;
		 
		 for (a = 0; a < ctx->sticks; a++) {
		 	if (ctx->sticka[a] == area) break;
		 }
		 
		 /* shuffle all the others down */
		 for (i = a; i < ctx->sticks; i++) {
		 	ctx->stick[i] = ctx->stick[i + 1];
		 	ctx->sticka[i] = ctx->sticka[i + 1];
		 }
		 
		 ctx->sticks--;
	}

	ctx->areas[area].read = NULL;
	ctx->areas[area].write = NULL;
	ctx->areas[area].reset = NULL;
	ctx->areas[area].tick = NULL;
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

	ctx->areas[area].write(ctx, 
				ptr & ~(31<<27), t, d, ctx->areas[area].ctx);
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
	
	return ctx->areas[area].read(ctx, 
				ptr & ~(31<<27), t, ctx->areas[area].ctx);
}

/* -- Simulator code------------------------------------------------------- */

inline void msim_swap_banks(struct msim_ctx *ctx)
{	
	u_int32_t *t;
	
	t = ctx->r;
	ctx->r = ctx->ar;
	ctx->ar = t;
}

void msim_irq(struct msim_ctx *ctx)
{
	ctx->wfi = false;
	if (MSIM_SR_IRQ(ctx->r[MSIM_SR]) == 0) {
		/* only enter interrupt mode if we're not already there */
		msim_swap_banks(ctx);
		ctx->r[MSIM_PC] = 32;
	}
}

inline u_int16_t msim_fetch(struct msim_ctx *ctx)
{
	return msim_memget(ctx, ctx->r[MSIM_PC], MSIM_ACCESS_HALFWORD);
}

static void set_nz(struct msim_ctx *ctx, u_int32_t v)
{
	u_int32_t sr = ctx->r[MSIM_SR] & ~(MEOW_SR_N | MEOW_SR_Z);

	if ((v & 0x80000000u) != 0) {
		sr |= MEOW_SR_N;
	}
	if (v == 0) {
		sr |= MEOW_SR_Z;
	}
	ctx->r[MSIM_SR] = sr;
}

/* CMP is SUBS with the result discarded: ARM flag semantics. */
static void compare(struct msim_ctx *ctx, u_int32_t a, u_int32_t b)
{
	u_int32_t d = a - b;
	u_int32_t sr;

	set_nz(ctx, d);
	sr = ctx->r[MSIM_SR] & ~(MEOW_SR_C | MEOW_SR_V);
	if (a >= b) {
		sr |= MEOW_SR_C;
	}
	if (((a ^ b) & (a ^ d) & 0x80000000u) != 0) {
		sr |= MEOW_SR_V;
	}
	ctx->r[MSIM_SR] = sr;
}

static u_int32_t *bank_reg(struct msim_ctx *ctx, unsigned reg, unsigned alt)
{
	return alt != 0 ? &ctx->ar[reg] : &ctx->r[reg];
}

static void reserved(struct msim_ctx *ctx, u_int16_t w)
{
	fprintf(stderr, "msim: reserved instruction %04x at %08x\n", w,
		ctx->r[MSIM_PC]);
}

static void shift(struct msim_ctx *ctx, unsigned reg, unsigned arith,
		  unsigned left, unsigned rot, unsigned amount)
{
	u_int32_t v = ctx->r[reg];

	amount &= 31;
	if (rot != 0) {
		if (amount != 0) {
			v = left != 0 ? (v << amount) | (v >> (32 - amount))
				      : (v >> amount) | (v << (32 - amount));
		}
	} else if (left != 0) {
		v <<= amount;
	} else if (arith != 0) {
		v = (u_int32_t)((int32_t)v >> amount);
	} else {
		v >>= amount;
	}
	ctx->r[reg] = v;
	if (reg == MSIM_PC) {
		ctx->nopcincrement = true;
	}
}

static void bitop(struct msim_ctx *ctx, u_int16_t w, unsigned reg,
		  unsigned op, unsigned inv, u_int32_t operand)
{
	if (inv != 0) {
		if (op == MEOW_BITOP_NOT) {
			reserved(ctx, w);
			return;
		}
		operand = ~operand;
	}
	switch (op) {
	case MEOW_BITOP_NOT: ctx->r[reg] = ~operand; break;
	case MEOW_BITOP_AND: ctx->r[reg] &= operand; break;
	case MEOW_BITOP_ORR: ctx->r[reg] |= operand; break;
	case MEOW_BITOP_EOR: ctx->r[reg] ^= operand; break;
	}
	if (reg == MSIM_PC) {
		ctx->nopcincrement = true;
	}
}

static void mem(struct msim_ctx *ctx, u_int16_t w)
{
	unsigned rv = MEOW_MEM_RV(w);
	unsigned ra = MEOW_MEM_RA(w);
	unsigned half = MEOW_MEM_HALF(w);
	unsigned hilo = MEOW_MEM_HILO(w);
	unsigned wb = MEOW_MEM_WB(w);
	unsigned dir = MEOW_MEM_DIR(w);
	msim_mem_access_type type;
	u_int32_t size;
	u_int32_t addr;

	if (half != 0) {
		type = MSIM_ACCESS_HALFWORD;
		size = 2;
	} else if (hilo != 0) {
		type = MSIM_ACCESS_WORD;
		size = 4;
	} else {
		type = MSIM_ACCESS_BYTE;
		size = 1;
	}
	if (wb == 0 && dir != 0) {
		ctx->r[ra] -= size;	/* decrease before */
	}
	addr = ctx->r[ra];
	if (MEOW_MEM_STORE(w) != 0) {
		u_int32_t v = ctx->r[rv];

		if (half != 0 && hilo == 0) {
			v >>= 16;
		}
		msim_memset(ctx, addr, type, v);
	} else {
		u_int32_t v = msim_memget(ctx, addr, type);

		if (half != 0 && hilo == 0) {
			ctx->r[rv] = (ctx->r[rv] & 0xffffu) | (v << 16);
		} else {
			ctx->r[rv] = v;
		}
	}
	if (wb != 0) {
		ctx->r[ra] += dir != 0 ? size : (u_int32_t)-(int32_t)size;
	}
	if (rv == MSIM_PC || (wb != 0 && ra == MSIM_PC)) {
		ctx->nopcincrement = true;
	}
}

void msim_execute(struct msim_ctx *ctx, u_int16_t w)
{
	switch (meow_enc_of(w)) {
	case MEOW_ENC_B: {
		unsigned cond = MEOW_B_COND(w);
		int32_t off = MEOW_B_OFF_S(w) * 2;

		if (cond == MEOW_COND_NV) {
			int op = off + 256;

			if (ctx->bnvops[op] == NULL) {
				fprintf(stderr,
					"msim: unhandled BNV %d at %08x\n",
					(int)off, ctx->r[MSIM_PC]);
			} else {
				ctx->bnvops[op](ctx, off, ctx->bnvopsctx[op]);
			}
		} else if (meow_cond_true(ctx->r[MSIM_SR], cond) == true) {
			ctx->r[MSIM_PC] += (u_int32_t)off;
			ctx->nopcincrement = true;
		}
		break;
	}
	case MEOW_ENC_ADD3:
	case MEOW_ENC_SUB3: {
		unsigned rd = MEOW_ADD3_RD(w);
		u_int32_t src = ctx->r[MEOW_ADD3_RS(w)];
		u_int32_t imm = MEOW_ADD3_IMM(w);
		u_int32_t v = imm == 0 ? ctx->r[rd] : src;
		u_int32_t operand = imm == 0 ? src : imm;

		ctx->r[rd] = meow_enc_of(w) == MEOW_ENC_ADD3 ? v + operand
							     : v - operand;
		if (rd == MSIM_PC) {
			ctx->nopcincrement = true;
		}
		break;
	}
	case MEOW_ENC_ADD8:
		ctx->r[MEOW_ADD8_RD(w)] += MEOW_ADD8_IMM(w);
		if (MEOW_ADD8_RD(w) == MSIM_PC) {
			ctx->nopcincrement = true;
		}
		break;
	case MEOW_ENC_SUB8:
		ctx->r[MEOW_SUB8_RD(w)] -= MEOW_SUB8_IMM(w);
		if (MEOW_SUB8_RD(w) == MSIM_PC) {
			ctx->nopcincrement = true;
		}
		break;
	case MEOW_ENC_CMPI:
		compare(ctx, ctx->r[MEOW_CMPI_RN(w)],
			(u_int32_t)MEOW_CMPI_IMM_S(w));
		break;
	case MEOW_ENC_CMPR:
		compare(ctx, *bank_reg(ctx, MEOW_CMPR_RN(w), MEOW_CMPR_BN(w)),
			*bank_reg(ctx, MEOW_CMPR_RM(w), MEOW_CMPR_BM(w)));
		break;
	case MEOW_ENC_TST:
		set_nz(ctx, *bank_reg(ctx, MEOW_TST_RN(w), MEOW_TST_BN(w)) &
			    (1u << MEOW_TST_BIT(w)));
		break;
	case MEOW_ENC_MOV: {
		u_int32_t v = *bank_reg(ctx, MEOW_MOV_RS(w), MEOW_MOV_BS(w));

		if (MEOW_MOV_BSW(w) != 0) {
			v = ((v & 0xff00ff00u) >> 8) | ((v & 0x00ff00ffu) << 8);
		}
		if (MEOW_MOV_HSW(w) != 0) {
			v = (v << 16) | (v >> 16);
		}
		*bank_reg(ctx, MEOW_MOV_RD(w), MEOW_MOV_BD(w)) = v;
		if (MEOW_MOV_RD(w) == MSIM_PC && MEOW_MOV_BD(w) == 0) {
			ctx->nopcincrement = true;
		}
		break;
	}
	case MEOW_ENC_LDI:
		ctx->r[MSIM_IR] = (u_int32_t)MEOW_LDI_IMM_S(w);
		break;
	case MEOW_ENC_SHI:
		shift(ctx, MEOW_SHI_RD(w), 0, MEOW_SHI_LEFT(w), MEOW_SHI_ROT(w),
		      MEOW_SHI_IMM(w));
		break;
	case MEOW_ENC_SHR:
		shift(ctx, MEOW_SHR_RD(w), 0, MEOW_SHR_LEFT(w), MEOW_SHR_ROT(w),
		      ctx->r[MEOW_SHR_RS(w)]);
		break;
	case MEOW_ENC_ASRI:
		shift(ctx, MEOW_ASRI_RD(w), 1, 0, 0, MEOW_ASRI_IMM(w));
		break;
	case MEOW_ENC_ASRR:
		shift(ctx, MEOW_ASRR_RD(w), 1, 0, 0, ctx->r[MEOW_ASRR_RS(w)]);
		break;
	case MEOW_ENC_SPMEM: {
		u_int32_t addr = ctx->r[MSIM_SP] + 4 * MEOW_SPMEM_IMM(w);
		unsigned rv = MEOW_SPMEM_RV(w);

		if (MEOW_SPMEM_STORE(w) != 0) {
			msim_memset(ctx, addr, MSIM_ACCESS_WORD, ctx->r[rv]);
		} else {
			ctx->r[rv] = msim_memget(ctx, addr, MSIM_ACCESS_WORD);
			if (rv == MSIM_PC) {
				ctx->nopcincrement = true;
			}
		}
		break;
	}
	case MEOW_ENC_BITR:
		bitop(ctx, w, MEOW_BITR_RD(w), MEOW_BITR_OP(w),
		      MEOW_BITR_INV(w), ctx->r[MEOW_BITR_RS(w)]);
		break;
	case MEOW_ENC_BITI:
		bitop(ctx, w, MEOW_BITI_RD(w), MEOW_BITI_OP(w),
		      MEOW_BITI_INV(w), 1u << MEOW_BITI_BIT(w));
		break;
	case MEOW_ENC_MEM:
		mem(ctx, w);
		break;
	default:
		reserved(ctx, w);
		break;
	}
	if (ctx->nopcincrement == true) {
		ctx->nopcincrement = false;
	} else {
		ctx->r[MSIM_PC] += 2;
	}
}

struct msim_ctx *msim_cpu(struct msim_ctx *ctx, unsigned int n)
{
	if (ctx->cpus == NULL) {
		return n == 0 ? ctx : NULL;
	}
	return n < ctx->ncpus ? ctx->cpus[n] : NULL;
}

/* Registers as reset leaves them, and pc as the Chairman says */
void msim_reset_cpu(struct msim_ctx *ctx, u_int32_t pc)
{
	memset(ctx->realr, 0, sizeof ctx->realr);
	memset(ctx->realar, 0, sizeof ctx->realar);
	ctx->r = ctx->realr;
	ctx->ar = ctx->realar;
	ctx->ar[MSIM_SR] = 1;
	ctx->r[MSIM_PC] = pc;
	ctx->irqmode = false;
	ctx->nopcincrement = false;
	ctx->wfi = false;
}

/* Make CPUs 1 to n - 1 as copies of CPU 0, once every device is added so
 * that they share them all; they are held in reset until started. */
void msim_add_cpus(struct msim_ctx *ctx, unsigned int n)
{
	unsigned int i;

	assert(ctx->cpu == 0 && ctx->cpus == NULL && n >= 1 && n <= 32);
	ctx->cpus = calloc(n, sizeof *ctx->cpus);
	assert(ctx->cpus != NULL);
	ctx->cpus[0] = ctx;
	ctx->ncpus = n;
	for (i = 1; i < n; i++) {
		struct msim_ctx *c = malloc(sizeof *c);

		assert(c != NULL);
		memcpy(c, ctx, sizeof *c);
		c->cpu = i;
		c->cyclecount = 0;
		c->running = false;
		c->jitter = 0;
		msim_reset_cpu(c, 0);
		ctx->cpus[i] = c;
	}
}

static bool msim_stalled(struct msim_ctx *m)
{
	/* a stall one cycle in four, from a generator of our own so that a
	 * run repeats for a given seed */
	m->jitter = m->jitter * 1103515245u + 12345u;
	return ((m->jitter >> 16) & 3) == 0;
}

/* The trace the hardware's testbench reads: the instruction's address
 * and word, then every register of both banks after it, the active
 * bank first. */
static void msim_trace_line(struct msim_ctx *ctx, u_int32_t pc, u_int16_t w)
{
	int r;

	fprintf(ctx->trace, "%08x %04x", pc, w);
	for (r = 0; r < 16; r++) {
		fprintf(ctx->trace, " %08x", ctx->r[r]);
	}
	for (r = 0; r < 16; r++) {
		fprintf(ctx->trace, " %08x", ctx->ar[r]);
	}
	fprintf(ctx->trace, "\n");
}

static void msim_step(struct msim_ctx *ctx, bool trace)
{
	u_int16_t i = msim_fetch(ctx);
	u_int32_t pc = ctx->r[MSIM_PC];
	char dis[256];

	if (trace == true) {
		int b;

		if (ctx->ncpus > 1) {
			printf("cpu%u ", ctx->cpu);
		}
		printf("0x%08x : ", ctx->r[MSIM_PC]);

		for (b = 15; b >= 0; b--) {
			printf("%s", (i) & (1<<b) ? "1":"0");
			if (b % 4 == 0) printf(" ");
		}

		meow_disasm(i, ctx->r[MSIM_PC], dis, sizeof dis);
		printf(": %-30s cycle %d\n", dis, ctx->cyclecount);
	}

	if (ctx->profile != NULL && ctx->r[MSIM_PC] < MSIM_PROFILE_BYTES) {
		ctx->profile[ctx->r[MSIM_PC] >> 1]++;
	}
	msim_execute(ctx, i);
	ctx->cyclecount++;
	if (ctx->trace != NULL && ctx->cpu == 0) {
		msim_trace_line(ctx, pc, i);
	}
}

/* Run the machine for a number of cycles: every running CPU that is not
 * waiting executes one instruction a cycle, in bus-ID order, and then the
 * devices tick once.  ctx is CPU 0, which stands for the machine. */
void msim_run(struct msim_ctx *ctx, unsigned int instructions, bool trace)
{
	if (ctx->init == false) {
		/* the cpu hasn't been reset yet - call device resets */
		int i;
		
		for (i = 0; i < 32; i++) {
			if (ctx->areas[i].reset != NULL)
				ctx->areas[i].reset(ctx, ctx->areas[i].ctx);
		}
		
		ctx->init = true;
	}
	
	for (; instructions > 0; instructions--) {
		unsigned int n;
		int ticks;

		for (n = 0; n < ctx->ncpus; n++) {
			struct msim_ctx *c = msim_cpu(ctx, n);

			if (c->running == false || c->wfi == true) {
				continue;
			}
			if (ctx->jitter != 0 && msim_stalled(ctx) == true) {
				continue;
			}
			msim_step(c, trace);
		}

		/* run the tickers in our ticker shortlist */
		if (ctx->sticks != 0) {
			for (ticks = 0; ticks < ctx->sticks; ticks++) {
				unsigned int a = ctx->sticka[ticks];
				ctx->stick[ticks](ctx, ctx->areas[a].ctx);
			}
		}
	}
}

/* -- Simple built-in devices---------------------------------------------- */

struct msim_rom_ctx {
	unsigned char 	*rom;
	size_t		size;
};

static u_int32_t msim_rom_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct msim_rom_ctx *mctx = (struct msim_rom_ctx *)fctx;
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

static void msim_rom_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
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
			NULL, NULL, mctx);
	ctx->areas[area].size = fs;
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

static u_int32_t msim_ram_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct msim_ram_ctx *mctx = (struct msim_ram_ctx *)fctx;
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

static void msim_ram_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	struct msim_ram_ctx *mctx = (struct msim_ram_ctx *)fctx;
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
			NULL, NULL, mctx);
	ctx->areas[area].size = size;
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

/* Local memory: a RAM per CPU, reached at chip select 30 as the accessing
 * CPU's own and at chip select 29 as all of them, 4 MB apart.  One store
 * of RAMs, each made when its CPU first touches it, behind a window for
 * each of the two chip selects. */
struct msim_local_ctx {
	size_t		size;
	unsigned char	*ram[32];
};

struct msim_local_window {
	struct msim_local_ctx	*l;
	bool			all;	/* chip select 29: every CPU's in a row */
};

static unsigned char *msim_local_ram(struct msim_ctx *ctx, u_int32_t ptr,
					struct msim_local_window *w,
					u_int32_t *off)
{
	struct msim_local_ctx *l = w->l;
	unsigned int cpu = w->all == true ? (ptr >> 22) & 31 : ctx->cpu;

	*off = w->all == true ? ptr & 0x3fffffu : ptr;
	if (*off >= l->size || cpu >= ctx->ncpus) {
		fprintf(stderr, "msim: access outside local memory, cpu %u"
			" offset 0x%x\n", cpu, *off);
		return NULL;
	}
	if (l->ram[cpu] == NULL) {
		l->ram[cpu] = calloc(l->size, 1);
		assert(l->ram[cpu] != NULL);
	}
	return l->ram[cpu];
}

static u_int32_t msim_local_read(struct msim_ctx *ctx, const u_int32_t ptr,
				msim_mem_access_type access, void *fctx)
{
	struct msim_ram_ctx view;
	u_int32_t off;

	view.ram = msim_local_ram(ctx, ptr, fctx, &off);
	if (view.ram == NULL) {
		return 0;
	}
	view.size = ((struct msim_local_window *)fctx)->l->size;
	return msim_ram_read(ctx, off, access, &view);
}

static void msim_local_write(struct msim_ctx *ctx, const u_int32_t ptr,
				const msim_mem_access_type access,
				const u_int32_t d, void *fctx)
{
	struct msim_ram_ctx view;
	u_int32_t off;

	view.ram = msim_local_ram(ctx, ptr, fctx, &off);
	if (view.ram == NULL) {
		return;
	}
	view.size = ((struct msim_local_window *)fctx)->l->size;
	msim_ram_write(ctx, off, access, d, &view);
}

void msim_add_local(struct msim_ctx *ctx, size_t size)
{
	struct msim_local_ctx *l = calloc(1, sizeof *l);
	struct msim_local_window *own = calloc(1, sizeof *own);
	struct msim_local_window *all = calloc(1, sizeof *all);

	assert(l != NULL && own != NULL && all != NULL);
	l->size = size;
	own->l = l;
	all->l = l;
	all->all = true;
	msim_device_add(ctx, 30, 0x00000004, msim_local_read, msim_local_write,
			NULL, NULL, own);
	ctx->areas[30].size = size;
	msim_device_add(ctx, 29, 0x00000004, msim_local_read, msim_local_write,
			NULL, NULL, all);
	ctx->areas[29].size = size;
}
