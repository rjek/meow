/* Golden disassembly vectors, derived by hand from the reference manual. */
#include <stdio.h>
#include <string.h>

#include "meow.h"

static const struct {
	uint16_t word;
	uint32_t pc;
	const char *text;
} vectors[] = {
	{ 0x0000, 0x100, "Beq 0x00000100" },
	{ 0x1c00, 0x100, "B 0x00000100" },
	{ 0x1dff, 0x100, "B 0x000000fe" },
	{ 0x1cff, 0x100, "B 0x000002fe" },
	{ 0x1ffd, 0x000, "BNV #-6" },
	{ 0x1e02, 0x000, "BNV #4" },
	{ 0x2410, 0x000, "ADD r4, r0, #1" },
	{ 0x2400, 0x000, "ADD r4, r0" },
	{ 0x3105, 0x000, "ADD r1, #5" },
	{ 0x44f2, 0x000, "SUB r4, r2, #15" },
	{ 0x5bff, 0x000, "SUB sp, #255" },
	{ 0x6bff, 0x000, "CMP sp, #-1" },
	{ 0x607f, 0x000, "CMP r0, #127" },
	{ 0x7113, 0x000, "CMP r1, ar3" },
	{ 0x7133, 0x000, "CMP ar1, ar3" },
	{ 0x70a5, 0x000, "TST ar0, #0x20" },
	{ 0x709f, 0x000, "TST r0, #0x80000000" },
	{ 0x8081, 0x000, "MOVB r0, r1" },
	{ 0x8243, 0x000, "MOVW r2, r3" },
	{ 0x80c1, 0x000, "MOVBW r0, r1" },
	{ 0x8031, 0x000, "MOV ar0, ar1" },
	{ 0x8f2c, 0x000, "MOV apc, lr" },
	{ 0x9fff, 0x000, "LDI #-1" },
	{ 0x97ff, 0x000, "LDI #2047" },
	{ 0xa483, 0x000, "LSL r4, #3" },
	{ 0xa503, 0x000, "LSR r5, #3" },
	{ 0xb503, 0x000, "ASR r5, #3" },
	{ 0xa4c3, 0x000, "ROL r4, #3" },
	{ 0xa443, 0x000, "ROR r4, #3" },
	{ 0xb443, 0x000, "LDR r4, [sp, #12]" },
	{ 0xb57f, 0x000, "STR r5, [sp, #124]" },
	{ 0xb185, 0x000, "ADDS r1, #5" },
	{ 0xb2df, 0x000, "SUBS r2, #31" },
	{ 0xb3a4, 0x000, "ADDS r3, r4" },
	{ 0xb5e6, 0x000, "SUBS r5, r6" },
	{ 0xa4a2, 0x000, "LSL r4, r2" },
	{ 0xb422, 0x000, "ASR r4, r2" },
	{ 0xc001, 0x000, "MVN r0, r1" },
	{ 0xc041, 0x000, "AND r0, r1" },
	{ 0xd041, 0x000, "BIC r0, r1" },
	{ 0xc081, 0x000, "ORR r0, r1" },
	{ 0xd081, 0x000, "ORN r0, r1" },
	{ 0xc0c1, 0x000, "EOR r0, r1" },
	{ 0xd0c1, 0x000, "EON r0, r1" },
	{ 0xd001, 0x000, "DCW 0xd001 ; reserved" },
	{ 0xc0a5, 0x000, "ORR r0, #0x20" },
	{ 0xd065, 0x000, "BIC r0, #0x20" },
	{ 0xc020, 0x000, "MVN r0, #0x1" },
	{ 0xe607, 0x000, "LDRB r6, [r7]" },
	{ 0xe849, 0x000, "LDR r8, [r9]" },
	{ 0xe17b, 0x000, "LDR r1, [sp], #4" },
	{ 0xf05b, 0x000, "STR r0, [sp, #-4]!" },
	{ 0xf06b, 0x000, "STR r0, [sp], #-4" },
	{ 0xe0c1, 0x000, "LDRH r0, [r1]" },
	{ 0xe081, 0x000, "LDRHH r0, [r1]" },
	{ 0xe0a1, 0x000, "LDRHH r0, [r1], #-2" },
	{ 0xf031, 0x000, "STRB r0, [r1], #1" },
	{ 0x7040, 0x000, "DCW 0x7040 ; reserved" },
};

int main(void)
{
	size_t i;
	int failed = 0;
	bool alt;

	for (i = 0; i < sizeof vectors / sizeof vectors[0]; i++) {
		char buf[64];

		meow_disasm(vectors[i].word, vectors[i].pc, buf, sizeof buf);
		if (strcmp(buf, vectors[i].text) != 0) {
			printf("FAIL 0x%04x: got \"%s\", want \"%s\"\n",
			       vectors[i].word, buf, vectors[i].text);
			failed++;
		}
	}
	if (meow_reg_parse("apc", &alt) != MEOW_PC || alt == false ||
	    meow_reg_parse("a1", &alt) != 0 || alt == true ||
	    meow_reg_parse("aa1", &alt) != 0 || alt == false ||
	    meow_reg_parse("at", &alt) != MEOW_R10 ||
	    meow_reg_parse("r16", &alt) != -1 ||
	    meow_reg_parse("R7", &alt) != 7 ||
	    meow_reg_parse("asr", &alt) != MEOW_SR || alt == false ||
	    meow_cond_parse("HS") != MEOW_COND_CS ||
	    meow_cond_parse("xx") != -1) {
		puts("FAIL register or condition parsing");
		failed++;
	}
	if (meow_cond_true(MEOW_SR_Z, MEOW_COND_LS) == false ||
	    meow_cond_true(MEOW_SR_C, MEOW_COND_LS) == true ||
	    meow_cond_true(MEOW_SR_N, MEOW_COND_LT) == false ||
	    meow_cond_true(MEOW_SR_N | MEOW_SR_V, MEOW_COND_GE) == false) {
		puts("FAIL condition evaluation");
		failed++;
	}
	printf("%s: %zu vectors, %d failures\n", failed == 0 ? "ok" : "FAILED",
	       i, failed);
	return failed != 0;
}
