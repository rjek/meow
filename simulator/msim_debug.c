/*
 * msim_debug.c
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

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <histedit.h>

#include "msim_core.h"

static inline void msim_print_next_instruction(struct msim_ctx *ctx)
{
	u_int32_t instr = msim_memget(ctx, ctx->r[MSIM_PC],
					MSIM_ACCESS_HALFWORD);
	printf("0x%08x\t%s\n", ctx->r[MSIM_PC], msim_disassemble(instr));
}

static void msim_debug_step(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	int cycles = 1;
	
	if (argc > 1) {
		cycles = strtol(argv[1], NULL, 0);
	}

	for (; cycles > 0; cycles--) {
		msim_run(ctx, 1, false);
		if (msim_breakpoint(ctx, ctx->r[MSIM_PC]) == true) {
			printf("msim: hit breakpoint at 0x%08x\n",
				ctx->r[MSIM_PC]);
			break;
		}
	}
	
	msim_print_next_instruction(ctx);
}

static void msim_debug_run(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	while (true) {
		msim_run(ctx, 1, false);
		if (msim_breakpoint(ctx, ctx->r[MSIM_PC]) == true) {
			printf("msim: hit breakpoint at 0x%08x\n",
				ctx->r[MSIM_PC]);
			break;
		}
	}
	
	msim_print_next_instruction(ctx);
}

static void msim_debug_peek(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	msim_mem_access_type t = MSIM_ACCESS_WORD;
	bool instr = false, str = false;
	u_int32_t a, d;
	
	
	if (argc < 2) {
		printf("usage: peek <address> [type]\n");
		return;
	}
	
	if (argc > 2) {
		switch (argv[2][0]) {
		case 'w':
			t = MSIM_ACCESS_WORD;
			break;
		case 'h':
			t = MSIM_ACCESS_HALFWORD;
			break;
		case 'i':
			t = MSIM_ACCESS_HALFWORD;
			instr = true;
			break;
		case 'b':
			t = MSIM_ACCESS_BYTE;
			break;
		case 's':
			t = MSIM_ACCESS_BYTE;
			str = true;
			break;
		default:
			printf("unknown type '%s'\n", argv[3]);
			return;
		}
	}
	
	a = strtoul(argv[1], NULL, 0);
	
	if (str == true) {
		int len = 0;
		
		printf("0x%08x: '", a);
		d = msim_memget(ctx, a, t);
		while (d != 0) {
			if (d < 32 || d >= 127) {
				printf("(%02x)", d);
			} else {
				printf("%c", d);
			}
			a++; len++;
			d = msim_memget(ctx, a, t);
		}
		printf("' (%d characters)\n", len);
		return;
	} else if (instr == true) {
		d = msim_memget(ctx, a, t);
		printf("0x%08x: %s\n", a, msim_disassemble(d));
		return;
	}

	printf("0x%08x: ", a);

	d = msim_memget(ctx, a, t);
	
	switch (t) {
	case MSIM_ACCESS_BYTE:
		if (d >= 32 && d < 127)
			printf("'%c' ", d);
		printf("0x%02x %d\n", d, d);
		break;
	case MSIM_ACCESS_HALFWORD:
		printf("0x%04x %d\n", d, d);
		break;
	case MSIM_ACCESS_WORD:
		printf("0x%08x %d\n", d, d);
		break;
	}
	
	
}

static void msim_debug_poke(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	msim_mem_access_type t = MSIM_ACCESS_WORD;
	u_int32_t a, d;
	
	if (argc < 3) {
		printf("usage: poke <address> <value> [type]\n");
		return;
	}
	
	if (argc > 3) {
		switch (argv[3][0]) {
		case 'w':
			t = MSIM_ACCESS_WORD;
			break;
		case 'h':
			t = MSIM_ACCESS_HALFWORD;
			break;
		case 'b':
			t = MSIM_ACCESS_BYTE;
			break;
		default:
			printf("unknown type '%s'\n", argv[3]);
			return;
		}
	}
	
	a = strtoul(argv[1], NULL, 0);
	d = strtoul(argv[2], NULL, 0);
	
	msim_memset(ctx, a, t, d);
}

static void msim_debug_show(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	msim_mem_access_type t = MSIM_ACCESS_WORD;
	msim_bank_type b = MSIM_THIS_BANK;
	msim_register reg;
	char buf[5];
	char *r = buf;
	u_int32_t d;
	
	if (argc < 2) {
		printf("usage: show [a][r]<0 .. 15> [type]\n");
		return;
	}
	
	strncpy(buf, argv[1], 4);
	
	if (r[0] == 'a' || r[0] == 'A') {
		b = MSIM_OTHER_BANK;
		r++;
	}
	
	if (r[0] == 'r' || r[0] == 'R') {
		r++;
	}
	
	if (!strcmp(r, "sp") || !strcmp(r, "SP"))
		reg = MSIM_SP;
	else if (!strcmp(r, "lr") || !strcmp(r, "LR"))
		reg = MSIM_LR;
	else if (!strcmp(r, "ir") || !strcmp(r, "IR"))
		reg = MSIM_IR;
	else if (!strcmp(r, "sr") || !strcmp(r, "SR"))
		reg = MSIM_SR;
	else if (!strcmp(r, "pc") || !strcmp(r, "PC"))
		reg = MSIM_PC;
	else
		reg = strtoul(r, NULL, 0);
	
	if (argc > 2) {
		switch (argv[2][0]) {
		case 'w':
			t = MSIM_ACCESS_WORD;
			break;
		case 'h':
			t = MSIM_ACCESS_HALFWORD;
			break;
		case 'b':
			t = MSIM_ACCESS_BYTE;
			break;
		default:
			printf("unknown type '%s'\n", argv[3]);
			return;
		}	
	}

	d = (b == MSIM_THIS_BANK) ? ctx->r[reg] : ctx->ar[reg];

	printf("%sR%d: ", (b == MSIM_THIS_BANK) ? "" : "A", reg);

	switch (t) {
	case MSIM_ACCESS_BYTE:
		if (d >= 32 && d < 127)
			printf("'%c' ", d);
		printf("0x%02x %d\n", d, d);
		break;
	case MSIM_ACCESS_HALFWORD:
		printf("0x%04x %d\n", d, d);
		break;
	case MSIM_ACCESS_WORD:
		printf("0x%08x %d\n", d, d);
		break;
	}

}

static void msim_debug_set(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	msim_bank_type b = MSIM_THIS_BANK;
	msim_register reg;
	char buf[5];
	char *r = buf;
	u_int32_t d;
	
	if (argc < 3) {
		printf("usage: set [a][r]<0 .. 15> <value>\n");
		return;
	}
	
	strncpy(buf, argv[1], 4);
	
	if (r[0] == 'a' || r[0] == 'A') {
		b = MSIM_OTHER_BANK;
		r++;
	}
	
	if (r[0] == 'r' || r[0] == 'R') {
		r++;
	}
	
	if (!strcmp(r, "sp") || !strcmp(r, "SP"))
		reg = MSIM_SP;
	else if (!strcmp(r, "lr") || !strcmp(r, "LR"))
		reg = MSIM_LR;
	else if (!strcmp(r, "ir") || !strcmp(r, "IR"))
		reg = MSIM_IR;
	else if (!strcmp(r, "sr") || !strcmp(r, "SR"))
		reg = MSIM_SR;
	else if (!strcmp(r, "pc") || !strcmp(r, "PC"))
		reg = MSIM_PC;
	else
		reg = strtoul(r, NULL, 0);
	
	d = strtoul(argv[2], NULL, 0);
	
	if (b == MSIM_THIS_BANK)
		ctx->r[reg] = d;
	else
		ctx->ar[reg] = d;

}

static void msim_debug_breakpoint(struct msim_ctx *ctx, const int argc,
							const char *argv[])
{
	if (argc < 2) {
		/* just list the breakpoints */
		int i, t = 0;
		for (i = 0; i < MSIM_DEBUG_BREAKPOINTS; i++) {
			if (ctx->breakpoints[i] != -1) {
				u_int16_t instrword = 
					msim_memget(ctx, ctx->breakpoints[i],
							MSIM_ACCESS_WORD);
				
				t++;
				printf("%2d: 0x%08x %s\n", t,
					ctx->breakpoints[i],
					msim_disassemble(instrword));
			}
		}
		
		if (t == 0)
			printf("no breakpoints set.\n");
	
	} else {
		u_int32_t a = strtoul(argv[1], NULL, 0);
		
		if (a % 2 != 0) {
			printf("breakpoints must be at multiples of 2.\n");
			return;
		}
		
		if (msim_breakpoint(ctx, a) == true) {
			msim_breakpoint_del(ctx, a);
			printf("breakpoint deleted\n");
			return;
		}
		
		if (msim_breakpoint_add(ctx, a) == true) {
			printf("breakpoint added\n");
		} else {
			printf("maximum of %d breakpoints reached\n",
				MSIM_DEBUG_BREAKPOINTS);
		}			
	}
}

static void msim_debug_help(void)
{
	puts("Commands:");
	puts("step [n]         Perform n cycles, or 1 cycle if omitted");
	puts("run              Run until break point or HALT");
	puts("peek <a> [t]     Display word at a, or optionally other type");
	puts("poke <a> <v> [t] Set word at a, or optionally other type");
	puts("show <r> [t]     Shows the word in register R, or other type");
	puts("set <r> <v>      Sets the register R to value");
	puts("breakpoint [a]   Toggles a breakpoint at a, or lists "
					"current breakpoints");
	puts("help             Shows this help text");
	puts("quit             Quits the simulator");
	puts("\nTypes:");
	puts("w                Word (default)");
	puts("b                Byte");
	puts("h                Half-word");
	puts("s                C string (only for reading)");
	puts("i                Instruction (only for reading)");
}

static bool msim_debug_main(struct msim_ctx *ctx, const int argc,
						const char *argv[])
{
	/* TODO: make this table-driven? */
	
	if (argc == 0 || !strcmp(argv[0], "step")) {
		/* step - default if no command */
		msim_debug_step(ctx, argc, argv);
	} else if (!strcmp(argv[0], "run")) {
		/* run until end or breakpoint */
		msim_debug_run(ctx, argc, argv);
	} else if (!strcmp(argv[0], "peek")) {
		/* display memory location contents */
		msim_debug_peek(ctx, argc, argv);
	} else if (!strcmp(argv[0], "poke")) {
		/* write memory location contents */
		msim_debug_poke(ctx, argc, argv);
	} else if (!strcmp(argv[0], "show")) {
		/* show contents of registers */
		msim_debug_show(ctx, argc, argv);
	} else if (!strcmp(argv[0], "set")) {
		/* set contents of registers */
		msim_debug_set(ctx, argc, argv);
	} else if (!strcmp(argv[0], "dump") || !strcmp(argv[0], "d")) {
		/* dump state */
		msim_print_state(ctx);
	} else if (!strcmp(argv[0], "breakpoint") || !strcmp(argv[0], "b")) {
		/* toggle breakpoint at memory location */
		msim_debug_breakpoint(ctx, argc, argv);
	} else if (!strcmp(argv[0], "help")) {
		/* print out some useful usage information */
		msim_debug_help();
	} else if (!strcmp(argv[0], "quit") || !strcmp(argv[0], "q")) {
		/* quit the debugger */
		
		return true;
	} else {
		printf("msim: unknown command '%s'\n", argv[0]);
	}
	
	return false;
}

static char *el_prompt(EditLine *e)
{
	return "(msim) ";
}

static char *completions[] = {
	"step ", "run", "peek ", "poke ", "breakpoint ",
	"help", "quit", "show ", "set ", "dump",
	
	NULL
};

static unsigned char msim_complete(EditLine *e, int ch)
{
        const char *ptr, *answer = NULL;
        const LineInfo *lf = el_line(e);
        int len, i = 0;
        
        /*
         * Find the last word and its length
         */
        for (ptr = lf->cursor - 1; !isspace(*ptr) && ptr >= lf->buffer; ptr--)
                continue;
        len = lf->cursor - ++ptr;
        
	if (len == 0) {
		/* it's not worth bothering */
		return CC_ERROR;
	}
	
	while(completions[i] != NULL) {
		if (strncmp(completions[i], ptr, len) == 0) {
			if (answer != NULL) {
				/* more than one match - return error */
				return CC_ERROR;
			} else {
				answer = completions[i] + len;
			}
		}
		i++;
	}
	
	if (answer == NULL)
		return CC_ERROR;
	
	if (el_insertstr(e, answer) == -1)
		return CC_ERROR;
	else
		return CC_REFRESH;

        return (CC_ERROR);
}

void msim_debugger(struct msim_ctx *ctx)
{
	EditLine *e = el_init("msim", stdin, stdout, stderr);
	History *h = history_init();
	Tokenizer *t = tok_init(NULL);
	HistEvent he;
	u_int32_t instr;
	
	history(h, &he, H_SETSIZE, 1024);
	
	el_set(e, EL_PROMPT, el_prompt);
	el_set(e, EL_EDITOR, "emacs");
	el_set(e, EL_SIGNAL, 1);
	el_set(e, EL_HIST, history, h);
	el_set(e, EL_ADDFN, "ed-complete", "Complete argument", msim_complete);
	el_set(e, EL_BIND, "^I", "ed-complete", NULL);

	msim_debug_init(ctx);
	
	instr = msim_memget(ctx, ctx->r[MSIM_PC],
				MSIM_ACCESS_HALFWORD);
	printf("0x%08x\t%s\n", ctx->r[MSIM_PC], msim_disassemble(instr));
	
	while (true) {
		int argc, ln;
		const char **argv, *l = el_gets(e, &ln);
		
		if (ln == 0) break;	/* Ctrl-D, or error, etc */
		history(h, &he, H_ENTER, l);
		tok_str(t, l, &argc, &argv);
		if (msim_debug_main(ctx, argc, argv))
			break;
		tok_reset(t);
	}
	
	el_end(e);
	history_end(h);
	tok_end(t);
}

void msim_debug_init(struct msim_ctx *ctx)
{
	int i;
	
	for (i = 0; i < MSIM_DEBUG_BREAKPOINTS; i++)
		ctx->breakpoints[i] = -1;
}

bool msim_breakpoint_add(struct msim_ctx *ctx, u_int32_t address)
{
	int i;
	
	for (i = 0; i < MSIM_DEBUG_BREAKPOINTS; i++) {
		if (ctx->breakpoints[i] == -1)
			break;
	}
	
	if (i >= MSIM_DEBUG_BREAKPOINTS) return false;
	
	ctx->breakpoints[i] = address;
	
	return true;
}

bool msim_breakpoint(struct msim_ctx *ctx, u_int32_t address)
{
	int i;
	
	for (i = 0; i < MSIM_DEBUG_BREAKPOINTS; i++) {
		if (ctx->breakpoints[i] == address)
			return true;
	}
	
	return false;
}

void msim_breakpoint_del(struct msim_ctx *ctx, u_int32_t address)
{
	int i;
	
	for (i = 0; i < MSIM_DEBUG_BREAKPOINTS; i++) {
		if (ctx->breakpoints[i] == address) {
			ctx->breakpoints[i] = -1;
			break;
		}
	}
}

const char *msim_disassemble(u_int16_t instrword)
{
	static char mnem[256];
	struct msim_instr decoded;
	
	/* we have internal knowledge that neither of these functions require
	 * an msim context to operate.  Naughty us.
	 */
	msim_decode(NULL, instrword, &decoded);
	msim_mnemonic(NULL, mnem, 256, &decoded);
	
	return mnem;
}

char *msim_mnemonic(struct msim_ctx *ctx, char *buf, unsigned int bufl, 
			struct msim_instr *instr)
{
	char tmp[256];
	static char *r[16] = {
		"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9",
		"r10", "sp", "lr", "ir", "sr", "pc" };
	
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
		snprintf(tmp, 256, "\t%s, ", r[instr->destination]);
		APPEND(tmp);
		if (instr->subop == true) {
			snprintf(tmp, 256, "#%d  ; 0x%08x", instr->immediate,
							instr->immediate);
			APPEND(tmp);
		} else {
			snprintf(tmp, 256, "%s, #%d  ; 0x%08x",
					r[instr->source],
					instr->immediate, instr->immediate);
			APPEND(tmp);
		}
		break;
	
	case MSIM_OPCODE_CMP:
		if (instr->subop == false) {
			APPEND("CMP");
			snprintf(tmp, 256, "\t%s, #%d  ; 0x%08d",
				r[instr->destination], instr->immediate,
				instr->immediate);
			APPEND(tmp);
		} else if (instr->istst == true) {
			APPEND("TST");
			snprintf(tmp, 256, "\t%s%s, #%d",
				instr->destinationbank == MSIM_THIS_BANK ?
				"" : "a", r[instr->destination],
				ffs(instr->immediate));
			APPEND(tmp);
		} else {
			APPEND("CMP");
			snprintf(tmp, 256, "\t%s%s, %s%s",
				instr->destinationbank == MSIM_THIS_BANK ?
				"" : "a", r[instr->destination],
				instr->sourcebank == MSIM_THIS_BANK ?
				"" : "a", r[instr->source]);
			APPEND(tmp);
		}
		break;
		
	case MSIM_OPCODE_MOV:
		if (instr->subop == true) {
			APPEND("LDI");
			snprintf(tmp, 256, "\t#%d  ; 0x%08x",
				instr->immediate,
				instr->immediate);
			APPEND(tmp);	
		} else {
			APPEND("MOV");
			if (instr->halfwordswap == true) APPEND("W");
			if (instr->byteswap == true) APPEND("B");
			snprintf(tmp, 256, "\t%s%s, %s%s",
				instr->destinationbank == MSIM_THIS_BANK
				? "" : "a", r[instr->destination],
				instr->sourcebank == MSIM_THIS_BANK
				? "" : "a", r[instr->source]);
			APPEND(tmp);
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
		
		snprintf(tmp, 256, "\t%s, ", r[instr->destination]);
		APPEND(tmp);
		
		if (instr->immver)
			snprintf(tmp, 256, "#%d", instr->immediate);
		else
			snprintf(tmp, 256, "%s", r[instr->source]);
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
		snprintf(tmp, 256, "\t%s, ", r[instr->destination]);
		APPEND(tmp);
		if (instr->immver == true)
			snprintf(tmp, 256, "#%d", ffs(instr->immediate));
		else
			snprintf(tmp, 256, "%s", r[instr->source]);
		APPEND(tmp);
		break;
			
	case MSIM_OPCODE_MEM:
		if (instr->memop == MSIM_MEM_LOAD)
			APPEND("LDR");
		else
			APPEND("STR");
		
		if (instr->memsize != MSIM_ACCESS_WORD) {
			if (instr->memsize == MSIM_ACCESS_BYTE)
				APPEND("B");
			else {
				if (instr->memhilo == MSIM_MEM_HI)
					APPEND("H");
				else
					APPEND("L");
			}
		}
			
		if (instr->writeback == true) {
			if (instr->memdirection == MSIM_MEM_DECREASE)
				APPEND("D");
			else
				APPEND("I");
		}
		
		snprintf(tmp, 256, "\t%s, %s", r[instr->destination],
						r[instr->source]);
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
	printf("     R8       R9       R10      R11/SP   R12/LR   R13/IR   R14/SR   R15/PC\n");
	printf("0    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->r[8], ctx->r[9], ctx->r[10], ctx->r[11], ctx->r[12], ctx->r[13], ctx->r[14], ctx->r[15]);
		
	printf("     N Z C V a a a a a a a a a a a a a a a a a a a a a a a a a a a I\n0 SR ");
	for (i = 31; i >= 0; i--)
		printf("%s ", (ctx->r[MSIM_SR] & (1<<i)) ? "1" : "0");
	printf("\n");
	
	printf("     R0       R1       R2       R3       R4       R5       R6       R7\n");
	printf("1    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->ar[0], ctx->ar[1], ctx->ar[2], ctx->ar[3], ctx->ar[4], ctx->ar[5], ctx->ar[6], ctx->ar[7]);
	printf("     R8       R9       R10      R11/SP   R12/LR   R13/IR   R14/SR   R15/PC\n");
	printf("1    %08x %08x %08x %08x %08x %08x %08x %08x\n",
		ctx->ar[8], ctx->ar[9], ctx->ar[10], ctx->ar[11], ctx->ar[12], ctx->ar[13], ctx->ar[14], ctx->ar[15]);

	printf("     N Z C V a a a a a a a a a a a a a a a a a a a a a a a a a a a I\n1 SR ");
	for (i = 31; i >= 0; i--)
		printf("%s ", (ctx->ar[MSIM_SR] & (1<<i)) ? "1" : "0");
	printf("\n\n");
}
