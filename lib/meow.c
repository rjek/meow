#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "meow.h"

static const char *const reg_names[16] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10",
	"sp", "lr", "ir", "sr", "pc"
};

static const char *const cond_names[16] = {
	"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
	"hi", "ls", "ge", "lt", "gt", "le", "", "nv"
};

const char *meow_reg_name(unsigned reg)
{
	return reg_names[reg & 15];
}

const char *meow_cond_name(unsigned cond)
{
	return cond_names[cond & 15];
}

static bool ieq(const char *a, const char *b)
{
	while (*a != '\0' && *b != '\0') {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
			return false;
		}
		a++;
		b++;
	}
	return *a == *b;
}

int meow_cond_parse(const char *s)
{
	unsigned i;

	if (ieq(s, "hs") == true) {
		return MEOW_COND_CS;
	}
	if (ieq(s, "lo") == true) {
		return MEOW_COND_CC;
	}
	if (ieq(s, "al") == true) {
		return MEOW_COND_AL;
	}
	for (i = 0; i < 16; i++) {
		if (cond_names[i][0] != '\0' && ieq(s, cond_names[i]) == true) {
			return (int)i;
		}
	}
	return -1;
}

int meow_reg_parse(const char *s, bool *alt)
{
	static const struct { const char *name; int reg; } aliases[] = {
		{ "sp", MEOW_SP }, { "lr", MEOW_LR }, { "ir", MEOW_IR },
		{ "sr", MEOW_SR }, { "pc", MEOW_PC }, { "at", MEOW_R10 },
		{ "a1", 0 }, { "a2", 1 }, { "a3", 2 }, { "a4", 3 },
		{ "v1", 4 }, { "v2", 5 }, { "v3", 6 }, { "v4", 7 },
		{ "v5", 8 }, { "v6", 9 },
	};
	size_t i;

	*alt = false;
	if (tolower((unsigned char)s[0]) == 'a' &&
	    tolower((unsigned char)s[1]) != '\0') {
		bool dummy;
		int r;

		/* a1-a4 are argument registers, everything else with a
		 * leading a is the alternative bank. */
		if (s[1] < '1' || s[1] > '4' || s[2] != '\0') {
			r = meow_reg_parse(s + 1, &dummy);
			if (r >= 0 && dummy == false) {
				*alt = true;
				return r;
			}
		}
	}
	if (tolower((unsigned char)s[0]) == 'r' && isdigit((unsigned char)s[1])) {
		char *end;
		long n = strtol(s + 1, &end, 10);

		if (*end == '\0' && n >= 0 && n <= 15) {
			return (int)n;
		}
		return -1;
	}
	for (i = 0; i < sizeof aliases / sizeof aliases[0]; i++) {
		if (ieq(s, aliases[i].name) == true) {
			return aliases[i].reg;
		}
	}
	return -1;
}

bool meow_cond_true(uint32_t sr, unsigned cond)
{
	bool n = (sr & MEOW_SR_N) != 0;
	bool z = (sr & MEOW_SR_Z) != 0;
	bool c = (sr & MEOW_SR_C) != 0;
	bool v = (sr & MEOW_SR_V) != 0;

	switch (cond & 15) {
	case MEOW_COND_EQ: return z == true;
	case MEOW_COND_NE: return z == false;
	case MEOW_COND_CS: return c == true;
	case MEOW_COND_CC: return c == false;
	case MEOW_COND_MI: return n == true;
	case MEOW_COND_PL: return n == false;
	case MEOW_COND_VS: return v == true;
	case MEOW_COND_VC: return v == false;
	case MEOW_COND_HI: return c == true && z == false;
	case MEOW_COND_LS: return c == false || z == true;
	case MEOW_COND_GE: return n == v;
	case MEOW_COND_LT: return n != v;
	case MEOW_COND_GT: return n == v && z == false;
	case MEOW_COND_LE: return n != v || z == true;
	case MEOW_COND_AL: return true;
	default: return false;
	}
}
