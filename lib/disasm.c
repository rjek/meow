#include <stdio.h>

#include "meow.h"

static const char *bank_reg(unsigned reg, unsigned alt, char *tmp)
{
	snprintf(tmp, 8, "%s%s", alt != 0 ? "a" : "", meow_reg_name(reg));
	return tmp;
}

static const char *shift_mnemonic(unsigned left, unsigned rot)
{
	if (rot != 0) {
		return left != 0 ? "ROL" : "ROR";
	}
	return left != 0 ? "LSL" : "LSR";
}

static const char *bit_mnemonic(unsigned op, unsigned inv)
{
	static const char *const names[2][4] = {
		{ "MVN", "AND", "ORR", "EOR" },
		{ NULL, "BIC", "ORN", "EON" }
	};

	return names[inv & 1][op & 3];
}

size_t meow_disasm(uint16_t w, uint32_t pc, char *buf, size_t len)
{
	char a[8];
	char b[8];
	const char *m;
	int n;

	switch (meow_enc_of(w)) {
	case MEOW_ENC_B: {
		unsigned cond = MEOW_B_COND(w);
		int32_t off = MEOW_B_OFF_S(w) * 2;

		if (cond == MEOW_COND_NV) {
			n = snprintf(buf, len, "BNV #%d", (int)off);
		} else {
			n = snprintf(buf, len, "B%s 0x%08x",
				     meow_cond_name(cond),
				     (unsigned)(pc + (uint32_t)off));
		}
		break;
	}
	case MEOW_ENC_ADD3:
	case MEOW_ENC_SUB3:
		m = (w & 0x4000) != 0 ? "SUB" : "ADD";
		if (MEOW_ADD3_IMM(w) == 0) {
			n = snprintf(buf, len, "%s %s, %s", m,
				     meow_reg_name(MEOW_ADD3_RD(w)),
				     meow_reg_name(MEOW_ADD3_RS(w)));
		} else {
			n = snprintf(buf, len, "%s %s, %s, #%u", m,
				     meow_reg_name(MEOW_ADD3_RD(w)),
				     meow_reg_name(MEOW_ADD3_RS(w)),
				     MEOW_ADD3_IMM(w));
		}
		break;
	case MEOW_ENC_ADD8:
	case MEOW_ENC_SUB8:
		m = (w & 0x4000) != 0 ? "SUB" : "ADD";
		n = snprintf(buf, len, "%s %s, #%u", m,
			     meow_reg_name(MEOW_ADD8_RD(w)), MEOW_ADD8_IMM(w));
		break;
	case MEOW_ENC_CMPI:
		n = snprintf(buf, len, "CMP %s, #%d",
			     meow_reg_name(MEOW_CMPI_RN(w)),
			     (int)MEOW_CMPI_IMM_S(w));
		break;
	case MEOW_ENC_CMPR:
		n = snprintf(buf, len, "CMP %s, %s",
			     bank_reg(MEOW_CMPR_RN(w), MEOW_CMPR_BN(w), a),
			     bank_reg(MEOW_CMPR_RM(w), MEOW_CMPR_BM(w), b));
		break;
	case MEOW_ENC_TST:
		n = snprintf(buf, len, "TST %s, #0x%x",
			     bank_reg(MEOW_TST_RN(w), MEOW_TST_BN(w), a),
			     1u << MEOW_TST_BIT(w));
		break;
	case MEOW_ENC_MOV:
		n = snprintf(buf, len, "MOV%s%s %s, %s",
			     MEOW_MOV_BSW(w) != 0 ? "B" : "",
			     MEOW_MOV_HSW(w) != 0 ? "W" : "",
			     bank_reg(MEOW_MOV_RD(w), MEOW_MOV_BD(w), a),
			     bank_reg(MEOW_MOV_RS(w), MEOW_MOV_BS(w), b));
		break;
	case MEOW_ENC_LDI:
		n = snprintf(buf, len, "LDI #%d", (int)MEOW_LDI_IMM_S(w));
		break;
	case MEOW_ENC_SHI:
		n = snprintf(buf, len, "%s %s, #%u",
			     shift_mnemonic(MEOW_SHI_LEFT(w), MEOW_SHI_ROT(w)),
			     meow_reg_name(MEOW_SHI_RD(w)), MEOW_SHI_IMM(w));
		break;
	case MEOW_ENC_SHR:
		n = snprintf(buf, len, "%s %s, %s",
			     shift_mnemonic(MEOW_SHR_LEFT(w), MEOW_SHR_ROT(w)),
			     meow_reg_name(MEOW_SHR_RD(w)),
			     meow_reg_name(MEOW_SHR_RS(w)));
		break;
	case MEOW_ENC_ASRI:
		n = snprintf(buf, len, "ASR %s, #%u",
			     meow_reg_name(MEOW_ASRI_RD(w)), MEOW_ASRI_IMM(w));
		break;
	case MEOW_ENC_ASRR:
		n = snprintf(buf, len, "ASR %s, %s",
			     meow_reg_name(MEOW_ASRR_RD(w)),
			     meow_reg_name(MEOW_ASRR_RS(w)));
		break;
	case MEOW_ENC_SPMEM:
		n = snprintf(buf, len, "%s %s, [sp, #%u]",
			     MEOW_SPMEM_STORE(w) != 0 ? "STR" : "LDR",
			     meow_reg_name(MEOW_SPMEM_RV(w)), 4 * MEOW_SPMEM_IMM(w));
		break;
	case MEOW_ENC_BITR:
		m = bit_mnemonic(MEOW_BITR_OP(w), MEOW_BITR_INV(w));
		if (m == NULL) {
			goto reserved;
		}
		n = snprintf(buf, len, "%s %s, %s", m,
			     meow_reg_name(MEOW_BITR_RD(w)),
			     meow_reg_name(MEOW_BITR_RS(w)));
		break;
	case MEOW_ENC_BITI:
		m = bit_mnemonic(MEOW_BITI_OP(w), MEOW_BITI_INV(w));
		if (m == NULL) {
			goto reserved;
		}
		n = snprintf(buf, len, "%s %s, #0x%x", m,
			     meow_reg_name(MEOW_BITI_RD(w)),
			     1u << MEOW_BITI_BIT(w));
		break;
	case MEOW_ENC_MEM: {
		static const char *const size_suffix[2][2] = {
			{ "B", "" }, { "HH", "H" }
		};
		static const unsigned size_bytes[2][2] = {
			{ 1, 4 }, { 2, 2 }
		};
		unsigned half = MEOW_MEM_HALF(w);
		unsigned hilo = MEOW_MEM_HILO(w);
		unsigned size = size_bytes[half][hilo];
		const char *rv = meow_reg_name(MEOW_MEM_RV(w));
		const char *ra = meow_reg_name(MEOW_MEM_RA(w));

		m = MEOW_MEM_STORE(w) != 0 ? "STR" : "LDR";
		if (MEOW_MEM_WB(w) == 0 && MEOW_MEM_DIR(w) == 0) {
			n = snprintf(buf, len, "%s%s %s, [%s]", m,
				     size_suffix[half][hilo], rv, ra);
		} else if (MEOW_MEM_WB(w) == 0) {
			n = snprintf(buf, len, "%s%s %s, [%s, #-%u]!", m,
				     size_suffix[half][hilo], rv, ra, size);
		} else {
			n = snprintf(buf, len, "%s%s %s, [%s], #%s%u", m,
				     size_suffix[half][hilo], rv, ra,
				     MEOW_MEM_DIR(w) != 0 ? "" : "-", size);
		}
		break;
	}
	default:
	reserved:
		n = snprintf(buf, len, "DCW 0x%04x ; reserved", w);
		break;
	}
	if (n < 0) {
		return 0;
	}
	return (size_t)n < len ? (size_t)n : len - 1;
}
