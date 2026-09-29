/* Shared MEOW definitions used by the assembler, simulator and linker. */
#ifndef MEOW_H
#define MEOW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "meow_isa.h"

enum meow_reg {
	MEOW_R0, MEOW_R1, MEOW_R2, MEOW_R3, MEOW_R4, MEOW_R5, MEOW_R6,
	MEOW_R7, MEOW_R8, MEOW_R9, MEOW_R10, MEOW_SP, MEOW_LR, MEOW_IR,
	MEOW_SR, MEOW_PC
};

enum meow_cond {
	MEOW_COND_EQ, MEOW_COND_NE, MEOW_COND_CS, MEOW_COND_CC,
	MEOW_COND_MI, MEOW_COND_PL, MEOW_COND_VS, MEOW_COND_VC,
	MEOW_COND_HI, MEOW_COND_LS, MEOW_COND_GE, MEOW_COND_LT,
	MEOW_COND_GT, MEOW_COND_LE, MEOW_COND_AL, MEOW_COND_NV
};

enum meow_bitop {
	MEOW_BITOP_NOT, MEOW_BITOP_AND, MEOW_BITOP_ORR, MEOW_BITOP_EOR
};

/* Status register bits. */
#define MEOW_SR_N (1u << 31)
#define MEOW_SR_Z (1u << 30)
#define MEOW_SR_C (1u << 29)
#define MEOW_SR_V (1u << 28)
#define MEOW_SR_I (1u << 0)

/* Architecture-defined BNV operations. */
#define MEOW_BNV_MODEL   0
#define MEOW_BNV_BUSID   2
#define MEOW_BNV_IRQRTN  4

#define MEOW_IRQ_VECTOR  32
#define MEOW_CHIPSELECT_SHIFT 27

/* Canonical register names: r0-r10, sp, lr, ir, sr, pc. */
const char *meow_reg_name(unsigned reg);

/* Two-letter condition suffix; "" for AL. */
const char *meow_cond_name(unsigned cond);

/* Parse a condition suffix; returns -1 if not one. */
int meow_cond_parse(const char *s);

/* Parse a register name in either bank; sets *alt for the alternative bank.
 * Accepts r0-r15, sp, lr, ir, sr, pc, a1-a4, v1-v6, at, and the same with
 * an a prefix.  Returns -1 if not a register. */
int meow_reg_parse(const char *s, bool *alt);

bool meow_cond_true(uint32_t sr, unsigned cond);

/* Disassemble one instruction at pc.  Returns the length written. */
size_t meow_disasm(uint16_t word, uint32_t pc, char *buf, size_t len);

#endif /* MEOW_H */
