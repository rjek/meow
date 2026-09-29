/* Instruction parsing, relaxation and encoding. */
#include <stdlib.h>
#include <string.h>

#include "mas.h"

/* Encoded words for one instruction item. */
struct words {
	uint16_t w[16];
	unsigned n;
};

static void put(struct words *ws, uint16_t w)
{
	if (ws->n < sizeof ws->w / sizeof ws->w[0]) {
		ws->w[ws->n] = w;
	}
	ws->n++;
}

/* ---- operand parsing -------------------------------------------------- */

static bool parse_register(struct lexer *lx, struct operand *op, bool need)
{
	struct token *t = lex_peek(lx);
	int r;

	if (t->kind == TOK_IDENT) {
		struct symbol *s = sym_find(t->text);

		if (s != NULL && s->kind == SYM_REGISTER) {
			op->kind = OP_REG;
			op->reg = s->reg;
			op->alt = s->alt;
			lex_next(lx);
			return true;
		}
		r = meow_reg_parse(t->text, &op->alt);
		if (r >= 0) {
			op->kind = OP_REG;
			op->reg = (unsigned)r;
			lex_next(lx);
			return true;
		}
	}
	if (need == true) {
		struct loc l = lex_loc(lx);

		error_at(&l, "expected a register");
	}
	return false;
}

static bool parse_immediate(struct lexer *lx, struct operand *op)
{
	if (lex_expect_punct(lx, '#') == false) {
		return false;
	}
	op->kind = OP_IMM;
	op->expr = expr_parse(lx);
	return op->expr != NULL;
}

static bool parse_reg_or_imm(struct lexer *lx, struct operand *op)
{
	if (lex_is_punct(lx, '#') == true) {
		return parse_immediate(lx, op);
	}
	return parse_register(lx, op, true);
}

static bool parse_target(struct lexer *lx, struct operand *op)
{
	op->kind = OP_EXPR;
	op->expr = expr_parse(lx);
	return op->expr != NULL;
}

static bool parse_mem(struct lexer *lx, struct operand *op)
{
	struct loc l = lex_loc(lx);

	if (lex_expect_punct(lx, '[') == false) {
		return false;
	}
	if (parse_register(lx, op, true) == false) {
		return false;
	}
	op->kind = OP_MEM;
	op->wb = 0;
	if (lex_accept_punct(lx, ',') == true) {
		/* [ra, #-n]! */
		if (lex_expect_punct(lx, '#') == false ||
		    expr_eval_abs(expr_parse(lx), NULL, true, &op->disp) == false ||
		    lex_expect_punct(lx, ']') == false ||
		    lex_expect_punct(lx, '!') == false) {
			return false;
		}
		if (op->disp >= 0) {
			error_at(&l, "pre-indexed form must decrement");
			return false;
		}
		op->wb = 1;
		return true;
	}
	if (lex_expect_punct(lx, ']') == false) {
		return false;
	}
	if (lex_accept_punct(lx, ',') == true) {
		/* [ra], #n */
		if (lex_expect_punct(lx, '#') == false ||
		    expr_eval_abs(expr_parse(lx), NULL, true, &op->disp) == false) {
			return false;
		}
		if (op->disp == 0) {
			error_at(&l, "post-index displacement cannot be zero");
			return false;
		}
		op->wb = op->disp < 0 ? 2 : 3;
	}
	if (op->alt == true) {
		error_at(&l, "address register cannot be in the alternative bank");
		return false;
	}
	return true;
}

static bool parse_reglist(struct lexer *lx, struct operand *op)
{
	struct loc l = lex_loc(lx);

	if (lex_expect_punct(lx, '{') == false) {
		return false;
	}
	op->kind = OP_REGLIST;
	op->reglist = 0;
	do {
		struct operand a;
		struct operand b;

		if (parse_register(lx, &a, true) == false) {
			return false;
		}
		b = a;
		if (lex_accept_punct(lx, '-') == true) {
			if (parse_register(lx, &b, true) == false) {
				return false;
			}
			if (b.reg < a.reg) {
				error_at(&l, "register range is backwards");
				return false;
			}
		}
		if (a.alt == true || b.alt == true) {
			error_at(&l, "alternative bank in register list");
			return false;
		}
		while (a.reg <= b.reg) {
			op->reglist |= (uint16_t)(1u << a.reg);
			a.reg++;
		}
	} while (lex_accept_punct(lx, ',') == true);
	if (lex_expect_punct(lx, '}') == false) {
		return false;
	}
	if (op->reglist == 0) {
		error_at(&l, "empty register list");
		return false;
	}
	return true;
}

/* ---- mnemonic parsers ------------------------------------------------- */

enum {
	F_ADD = 0, F_SUB = 1,			/* arithmetic */
	F_LOAD = 0, F_STORE = 1,		/* memory */
	F_SIZE_W = 0, F_SIZE_B = 2, F_SIZE_H = 4, F_SIZE_HH = 8,
	F_SHIFT_LSL = 0, F_SHIFT_LSR, F_SHIFT_ASR, F_SHIFT_ROL, F_SHIFT_ROR,
	F_BIT_INV = 4,				/* or'd with MEOW_BITOP_* */
	F_MOV_B = 1, F_MOV_W = 2
};

static bool no_alt(const struct operand *op, const struct loc *l,
		   const char *what)
{
	if (op->kind == OP_REG && op->alt == true) {
		error_at(l, "%s cannot use the alternative bank", what);
		return false;
	}
	return true;
}

static int p_branch(const struct mnemonic *mn, struct lexer *lx,
		    struct instr *in)
{
	struct loc l = lex_loc(lx);

	if (in->cond == MEOW_COND_NV) {
		if (mn->arg != 0) {
			error_at(&l, "BL cannot use the NV condition");
			return 0;
		}
		if (parse_immediate(lx, &in->ops[0]) == false) {
			return 0;
		}
	} else if (parse_target(lx, &in->ops[0]) == false) {
		return 0;
	}
	in->nops = 1;
	return 1;
}

static int p_bnv(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	(void)mn;
	in->cond = MEOW_COND_NV;
	if (parse_immediate(lx, &in->ops[0]) == false) {
		return 0;
	}
	in->nops = 1;
	return 1;
}

static int p_none(const struct mnemonic *mn, struct lexer *lx,
		  struct instr *in)
{
	(void)mn;
	(void)lx;
	in->nops = 0;
	return 1;
}

/* ADD/SUB rd, rs | rd, rs, #imm | rd, #imm */
static int p_arith(const struct mnemonic *mn, struct lexer *lx,
		   struct instr *in)
{
	struct loc l = lex_loc(lx);

	(void)mn;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_reg_or_imm(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	if (in->ops[1].kind == OP_REG && lex_accept_punct(lx, ',') == true) {
		if (parse_immediate(lx, &in->ops[2]) == false) {
			return 0;
		}
		in->nops = 3;
	}
	return no_alt(&in->ops[0], &l, "ADD/SUB") &&
	       no_alt(&in->ops[1], &l, "ADD/SUB");
}

/* CMP rn, #imm | {a}rn, {a}rm */
static int p_cmp(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	(void)mn;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_reg_or_imm(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	if (in->ops[1].kind == OP_IMM && in->ops[0].alt == true) {
		error_at(&l, "CMP with an immediate cannot use the alternative bank");
		return 0;
	}
	return 1;
}

/* TST {a}rn, #value */
static int p_tst(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	(void)mn;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_immediate(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	return 1;
}

/* MOV{B}{W} {a}rd, {a}rs | MOV rd, #imm */
static int p_mov(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	in->flags = mn->arg;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_reg_or_imm(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	if (in->ops[1].kind == OP_IMM) {
		if (in->flags != 0) {
			error_at(&l, "swapping MOV needs a register operand");
			return 0;
		}
		return no_alt(&in->ops[0], &l, "MOV with an immediate");
	}
	return 1;
}

static int p_ldi(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	(void)mn;
	if (parse_immediate(lx, &in->ops[0]) == false) {
		return 0;
	}
	in->nops = 1;
	return 1;
}

/* LSL/LSR/ASR/ROL/ROR rd, #imm | rd, rs */
static int p_shift(const struct mnemonic *mn, struct lexer *lx,
		   struct instr *in)
{
	struct loc l = lex_loc(lx);

	in->flags = mn->arg;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_reg_or_imm(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	return no_alt(&in->ops[0], &l, "shifts") &&
	       no_alt(&in->ops[1], &l, "shifts");
}

/* AND/ORR/EOR/BIC/ORN/EON/MVN rd, rs | rd, #value */
static int p_bit(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	in->flags = mn->arg;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_reg_or_imm(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	return no_alt(&in->ops[0], &l, "bit operations") &&
	       no_alt(&in->ops[1], &l, "bit operations");
}

/* NOT rd */
static int p_not(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	in->flags = mn->arg;
	if (parse_register(lx, &in->ops[0], true) == false) {
		return 0;
	}
	in->ops[1] = in->ops[0];
	in->nops = 2;
	return no_alt(&in->ops[0], &l, "NOT");
}

/* LDR/STR rv, [ra]... | LDR rd, =expr */
static int p_mem(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	in->flags = mn->arg;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false) {
		return 0;
	}
	if (no_alt(&in->ops[0], &l, "memory access") == false) {
		return 0;
	}
	if (lex_accept_punct(lx, '=') == true) {
		struct loc el = lex_loc(lx);

		if ((in->flags & (F_STORE | F_SIZE_B | F_SIZE_H | F_SIZE_HH)) != 0) {
			error_at(&el, "only LDR can load a literal");
			return 0;
		}
		in->ops[1].kind = OP_EXPR;
		in->ops[1].expr = expr_parse(lx);
		if (in->ops[1].expr == NULL) {
			return 0;
		}
		in->nops = 2;
		return 1;
	}
	if (parse_mem(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	{
		unsigned size = (in->flags & F_SIZE_B) != 0 ? 1 :
				(in->flags & (F_SIZE_H | F_SIZE_HH)) != 0 ? 2 : 4;
		int64_t d = in->ops[1].disp < 0 ? -in->ops[1].disp
						: in->ops[1].disp;

		if (in->ops[1].wb != 0 && d != size) {
			error_at(&l, "writeback must be by the access size (%u)",
				 size);
			return 0;
		}
		if (in->ops[1].wb != 0 && in->ops[0].reg == in->ops[1].reg) {
			error_at(&l, "writeback onto the value register is unpredictable");
			return 0;
		}
	}
	return 1;
}

/* ADR rd, label */
static int p_adr(const struct mnemonic *mn, struct lexer *lx,
		 struct instr *in)
{
	struct loc l = lex_loc(lx);

	(void)mn;
	if (parse_register(lx, &in->ops[0], true) == false ||
	    lex_expect_punct(lx, ',') == false ||
	    parse_target(lx, &in->ops[1]) == false) {
		return 0;
	}
	in->nops = 2;
	return no_alt(&in->ops[0], &l, "ADR");
}

static int p_reglist(const struct mnemonic *mn, struct lexer *lx,
		     struct instr *in)
{
	(void)mn;
	if (parse_reglist(lx, &in->ops[0]) == false) {
		return 0;
	}
	in->nops = 1;
	return 1;
}

/* ---- mnemonic table --------------------------------------------------- */

enum {
	M_B, M_BL, M_BNV, M_ADD, M_SUB, M_ADDS, M_CMP, M_TST, M_MOV, M_LDI, M_SHIFT,
	M_BIT, M_NOT, M_MEM, M_ADR, M_PUSH, M_POP, M_NOP, M_RET, M_IRQRTN
};

struct mnemonic_def {
	const char *name;
	int (*parse)(const struct mnemonic *, struct lexer *, struct instr *);
	unsigned arg;
	int which;
};

static const struct mnemonic_def defs[] = {
	{ "b",      p_branch, 0, M_B },
	{ "bl",     p_branch, 1, M_BL },
	{ "bnv",    p_bnv,    0, M_BNV },
	{ "add",    p_arith,  F_ADD, M_ADD },
	{ "sub",    p_arith,  F_SUB, M_SUB },
	{ "adds",   p_arith,  F_ADD, M_ADDS },
	{ "subs",   p_arith,  F_SUB, M_ADDS },
	{ "cmp",    p_cmp,    0, M_CMP },
	{ "tst",    p_tst,    0, M_TST },
	{ "mov",    p_mov,    0, M_MOV },
	{ "movb",   p_mov,    F_MOV_B, M_MOV },
	{ "movw",   p_mov,    F_MOV_W, M_MOV },
	{ "movbw",  p_mov,    F_MOV_B | F_MOV_W, M_MOV },
	{ "movwb",  p_mov,    F_MOV_B | F_MOV_W, M_MOV },
	{ "ldi",    p_ldi,    0, M_LDI },
	{ "lsl",    p_shift,  F_SHIFT_LSL, M_SHIFT },
	{ "asl",    p_shift,  F_SHIFT_LSL, M_SHIFT },
	{ "lsr",    p_shift,  F_SHIFT_LSR, M_SHIFT },
	{ "asr",    p_shift,  F_SHIFT_ASR, M_SHIFT },
	{ "rol",    p_shift,  F_SHIFT_ROL, M_SHIFT },
	{ "ror",    p_shift,  F_SHIFT_ROR, M_SHIFT },
	{ "mvn",    p_bit,    MEOW_BITOP_NOT, M_BIT },
	{ "and",    p_bit,    MEOW_BITOP_AND, M_BIT },
	{ "orr",    p_bit,    MEOW_BITOP_ORR, M_BIT },
	{ "eor",    p_bit,    MEOW_BITOP_EOR, M_BIT },
	{ "bic",    p_bit,    MEOW_BITOP_AND | F_BIT_INV, M_BIT },
	{ "orn",    p_bit,    MEOW_BITOP_ORR | F_BIT_INV, M_BIT },
	{ "eon",    p_bit,    MEOW_BITOP_EOR | F_BIT_INV, M_BIT },
	{ "not",    p_not,    MEOW_BITOP_NOT, M_BIT },
	{ "ldr",    p_mem,    F_LOAD | F_SIZE_W, M_MEM },
	{ "ldrb",   p_mem,    F_LOAD | F_SIZE_B, M_MEM },
	{ "ldrh",   p_mem,    F_LOAD | F_SIZE_H, M_MEM },
	{ "ldrhh",  p_mem,    F_LOAD | F_SIZE_HH, M_MEM },
	{ "str",    p_mem,    F_STORE | F_SIZE_W, M_MEM },
	{ "strb",   p_mem,    F_STORE | F_SIZE_B, M_MEM },
	{ "strh",   p_mem,    F_STORE | F_SIZE_H, M_MEM },
	{ "strhh",  p_mem,    F_STORE | F_SIZE_HH, M_MEM },
	{ "adr",    p_adr,    0, M_ADR },
	{ "push",   p_reglist, 0, M_PUSH },
	{ "pop",    p_reglist, 0, M_POP },
	{ "nop",    p_none,   0, M_NOP },
	{ "ret",    p_none,   0, M_RET },
	{ "irqrtn", p_none,   0, M_IRQRTN },
};

#define NDEFS (sizeof defs / sizeof defs[0])

static struct mnemonic mnemonics[NDEFS];

static const struct mnemonic_def *def_of(const struct mnemonic *mn)
{
	return &defs[mn - mnemonics];
}

static const struct mnemonic *find_exact(const char *word)
{
	size_t i;

	for (i = 0; i < NDEFS; i++) {
		if (ieq(word, defs[i].name) == true) {
			if (mnemonics[i].name == NULL) {
				mnemonics[i].name = defs[i].name;
				mnemonics[i].parse = defs[i].parse;
				mnemonics[i].arg = defs[i].arg;
			}
			return &mnemonics[i];
		}
	}
	return NULL;
}

/* Only B and BL take a condition suffix: BLT is B LT, BLLT is BL LT. */
const struct mnemonic *mnemonic_lookup(const char *word, int *cond,
				       unsigned *suffix)
{
	const struct mnemonic *mn = find_exact(word);
	int c;

	*cond = MEOW_COND_AL;
	*suffix = 0;
	if (mn != NULL) {
		return mn;
	}
	if ((word[0] == 'b' || word[0] == 'B') && strlen(word) == 4 &&
	    (word[1] == 'l' || word[1] == 'L')) {
		c = meow_cond_parse(word + 2);
		if (c >= 0) {
			*cond = c;
			return find_exact("bl");
		}
	}
	if ((word[0] == 'b' || word[0] == 'B') && strlen(word) == 3) {
		c = meow_cond_parse(word + 1);
		if (c >= 0) {
			*cond = c;
			return find_exact("b");
		}
	}
	return NULL;
}

bool parse_instruction(struct lexer *lx, const struct loc *loc)
{
	struct token *t = lex_peek(lx);
	const struct mnemonic *mn;
	int cond;
	unsigned suffix;
	struct item *it;
	struct instr *in;

	mn = mnemonic_lookup(t->text, &cond, &suffix);
	if (mn == NULL) {
		return false;
	}
	lex_next(lx);
	it = item_new(ITEM_INSTR, loc);
	in = &it->u.instr;
	in->mn = mn;
	in->cond = cond;
	if (mn->parse(mn, lx, in) == 0) {
		return true;	/* error already reported */
	}
	lex_expect_eol(lx);
	if (def_of(mn)->which == M_MEM && in->ops[1].kind == OP_EXPR) {
		in->lit = literal_add(in->ops[1].expr, loc);
	}
	item_append(it);
	return true;
}

/* ---- encoding --------------------------------------------------------- */

static void enc_add(struct words *ws, unsigned sub, unsigned rd,
		    unsigned rs, unsigned imm)
{
	put(ws, sub != 0 ? MEOW_ENCODE_SUB3(rd, imm, rs)
			 : MEOW_ENCODE_ADD3(rd, imm, rs));
}

static void enc_add8(struct words *ws, unsigned sub, unsigned rd, unsigned imm)
{
	put(ws, sub != 0 ? MEOW_ENCODE_SUB8(rd, imm) : MEOW_ENCODE_ADD8(rd, imm));
}

static void enc_mov(struct words *ws, unsigned rd, bool rda, unsigned rs,
		    bool rsa, unsigned flags)
{
	put(ws, MEOW_ENCODE_MOV(rd, (flags & F_MOV_B) != 0,
				(flags & F_MOV_W) != 0, rda, rsa, rs));
}

static void enc_ldi(struct words *ws, int64_t v)
{
	put(ws, MEOW_ENCODE_LDI((uint32_t)v));
}

static void enc_shift_imm(struct words *ws, unsigned rd, unsigned kind,
			  unsigned amount)
{
	unsigned arith = kind == F_SHIFT_ASR;
	unsigned left = kind == F_SHIFT_LSL || kind == F_SHIFT_ROL;
	unsigned rot = kind == F_SHIFT_ROL || kind == F_SHIFT_ROR;

	put(ws, arith != 0 ? MEOW_ENCODE_ASRI(rd, amount)
			   : MEOW_ENCODE_SHI(rd, left, rot, amount));
}

static void enc_adds(struct words *ws, unsigned sub, unsigned rd,
		     unsigned rs)
{
	put(ws, MEOW_ENCODE_ADDSR(rd, sub, rs));
}

static void enc_adds_imm(struct words *ws, unsigned sub, unsigned rd,
			 unsigned imm)
{
	put(ws, MEOW_ENCODE_ADDSI(rd, sub, imm));
}

static void enc_bit_reg(struct words *ws, unsigned rd, unsigned op,
			unsigned inv, unsigned rs)
{
	put(ws, MEOW_ENCODE_BITR(inv, rd, op, rs));
}

static void enc_bit_imm(struct words *ws, unsigned rd, unsigned op,
			unsigned inv, unsigned bit)
{
	put(ws, MEOW_ENCODE_BITI(inv, rd, op, bit));
}

static void enc_mem(struct words *ws, unsigned flags, unsigned rv,
		    unsigned ra, int wb)
{
	unsigned half = (flags & (F_SIZE_H | F_SIZE_HH)) != 0;
	unsigned hilo = (flags & F_SIZE_B) != 0 ? 0 :
			(flags & F_SIZE_HH) != 0 ? 0 : 1;
	unsigned w = wb == 2 || wb == 3;
	unsigned d = wb == 1 || wb == 3;

	put(ws, MEOW_ENCODE_MEM((flags & F_STORE) != 0, rv, half, hilo, w, d, ra));
}

static int bit_index(uint32_t v)
{
	int i;

	if (v == 0 || (v & (v - 1)) != 0) {
		return -1;
	}
	for (i = 0; (v & 1) == 0; i++) {
		v >>= 1;
	}
	return i;
}

static bool fits_simm(int64_t v, unsigned bits)
{
	int64_t lim = (int64_t)1 << (bits - 1);

	return v >= -lim && v < lim;
}

/* Materialise a 32-bit constant in rd, using ir as scratch unless rd is ir.
 * Never uses more than eight words.  Returns the number of words used. */
static void mov_imm(struct words *ws, unsigned rd, uint32_t v)
{
	int32_t sv = (int32_t)v;
	int bit = bit_index(v);
	unsigned chunks;
	unsigned i;

	if (v == 0) {
		enc_bit_reg(ws, rd, MEOW_BITOP_EOR, 0, rd);
		return;
	}
	if (fits_simm(sv, 12) == true) {
		enc_ldi(ws, sv);
		if (rd != MEOW_IR) {
			enc_mov(ws, rd, false, MEOW_IR, false, 0);
		}
		return;
	}
	if (fits_simm(~sv, 12) == true) {
		enc_ldi(ws, ~sv);
		enc_bit_reg(ws, rd, MEOW_BITOP_NOT, 0, MEOW_IR);
		return;
	}
	if (bit >= 0) {
		enc_bit_reg(ws, rd, MEOW_BITOP_EOR, 0, rd);
		enc_bit_imm(ws, rd, MEOW_BITOP_ORR, 0, (unsigned)bit);
		return;
	}
	/* LDI the top, then shift in a byte at a time. */
	chunks = fits_simm(sv >> 8, 12) == true ? 1 :
		 fits_simm(sv >> 16, 12) == true ? 2 : 3;
	enc_ldi(ws, sv >> (8 * chunks));
	if (rd != MEOW_IR) {
		enc_mov(ws, rd, false, MEOW_IR, false, 0);
	}
	for (i = chunks; i > 0; i--) {
		unsigned byte = (v >> (8 * (i - 1))) & 0xff;

		enc_shift_imm(ws, rd, F_SHIFT_LSL, 8);
		if (byte != 0) {
			enc_add8(ws, 0, rd, byte);
		}
	}
}

/* Materialise into ir for use as a second operand. */
static bool operand_to_ir(struct words *ws, const struct loc *l, unsigned rd,
			  uint32_t v, bool report)
{
	if (rd == MEOW_IR) {
		if (report == true) {
			error_at(l, "immediate does not fit and ir is the destination");
		}
		return false;
	}
	mov_imm(ws, MEOW_IR, v);
	return true;
}

/* Value of an immediate operand; false if not (yet) known. */
static bool imm_value(struct operand *op, struct item *it, bool report,
		      int64_t *v)
{
	struct value val;

	if (expr_eval(op->expr, it, report, &val) == false) {
		return false;
	}
	if (value_is_abs(&val) == false) {
		if (report == true) {
			error_at(expr_loc(op->expr),
				 "immediate must be absolute");
		}
		return false;
	}
	*v = val.v;
	return true;
}

static bool check_range(const struct loc *l, int64_t v, int64_t lo, int64_t hi,
			bool report, const char *what)
{
	if (v < lo || v > hi) {
		if (report == true) {
			error_at(l, "%s %lld out of range %lld to %lld", what,
				 (long long)v, (long long)lo, (long long)hi);
		}
		return false;
	}
	return true;
}

/* Offset from this item to a target in the same section. */
static bool pc_delta(struct item *it, struct expr *e, bool report,
		     int64_t *delta)
{
	struct value v;

	if (expr_eval(e, it, report, &v) == false) {
		return false;
	}
	if (v.sec != it->sec || v.ext != NULL) {
		if (report == true) {
			error_at(expr_loc(e), "target is not in this section");
		}
		return false;
	}
	*delta = v.v - (int64_t)it->addr;
	return true;
}

/* Long branch: LDI #d; ADD pc, ir, where d is relative to the ADD. */
static bool enc_long_branch(struct words *ws, const struct loc *l,
			    int64_t delta, unsigned words_before, bool report)
{
	int64_t d = delta - (int64_t)(words_before + 1) * 2;

	if (fits_simm(d, 12) == false) {
		if (report == true) {
			error_at(l, "branch target too far; use LDR pc, =label");
		}
		return false;
	}
	enc_ldi(ws, d);
	enc_add(ws, 0, MEOW_PC, MEOW_IR, 0);
	return true;
}

static bool enc_branch(struct words *ws, struct item *it, struct instr *in,
		       bool report, unsigned link)
{
	int64_t delta;
	bool known;
	unsigned words = 0;
	bool need_long;

	known = pc_delta(it, in->ops[0].expr, report, &delta);
	if (known == false) {
		delta = 0;
	}
	if (link != 0) {
		words = 1;	/* ADD lr, pc, #n, patched below */
		put(ws, 0);
	}
	delta -= (int64_t)words * 2;
	need_long = known == true && (delta < -512 || delta > 510 ||
				      (delta & 1) != 0);
	if (in->min_words > words + 1) {
		need_long = true;
	}
	if (need_long == false) {
		put(ws, MEOW_ENCODE_B(in->cond, (uint32_t)(delta / 2)));
	} else {
		if ((delta & 1) != 0 && report == true) {
			error_at(&it->loc, "branch target is not halfword aligned");
			return false;
		}
		if (in->cond != MEOW_COND_AL) {
			/* skip the two-word long branch */
			put(ws, MEOW_ENCODE_B(in->cond ^ 1, 3));
			if (enc_long_branch(ws, &it->loc, delta - 2, 0,
					    report) == false) {
				return false;
			}
		} else if (enc_long_branch(ws, &it->loc, delta, 0,
					   report) == false) {
			return false;
		}
	}
	if (link != 0) {
		ws->w[0] = MEOW_ENCODE_ADD3(MEOW_LR, ws->n * 2, MEOW_PC);
	}
	return true;
}

static bool enc_arith(struct words *ws, struct item *it, struct instr *in,
		      bool report)
{
	unsigned sub = in->mn->arg;
	unsigned rd = in->ops[0].reg;
	int64_t v;

	if (in->nops == 3) {
		if (imm_value(&in->ops[2], it, report, &v) == false) {
			v = 1;
		}
		if (v == 0) {
			if (report == true) {
				error_at(&it->loc, "use MOV to copy a register");
			}
			return false;
		}
		if (check_range(&it->loc, v, 1, 15, report, "immediate") == false) {
			return false;
		}
		enc_add(ws, sub, rd, in->ops[1].reg, (unsigned)v);
		return true;
	}
	if (in->ops[1].kind == OP_REG) {
		enc_add(ws, sub, rd, in->ops[1].reg, 0);
		return true;
	}
	if (imm_value(&in->ops[1], it, report, &v) == false) {
		v = 0;
	}
	if (v < 0 && v >= -255) {
		sub ^= 1;
		v = -v;
	}
	if (v >= 0 && v <= 255) {
		enc_add8(ws, sub, rd, (unsigned)v);
		return true;
	}
	if (operand_to_ir(ws, &it->loc, rd, (uint32_t)v, report) == false) {
		return false;
	}
	enc_add(ws, sub, rd, MEOW_IR, 0);
	return true;
}

/* ADDS/SUBS rd, rs | rd, #0..31 */
static bool enc_arith_s(struct words *ws, struct item *it, struct instr *in,
			bool report)
{
	unsigned sub = in->mn->arg;
	unsigned rd = in->ops[0].reg;
	int64_t v;

	if (in->nops == 3) {
		if (report == true) {
			error_at(&it->loc, "ADDS and SUBS have no three-operand form");
		}
		return false;
	}
	if (in->ops[1].kind == OP_REG) {
		enc_adds(ws, sub, rd, in->ops[1].reg);
		return true;
	}
	if (imm_value(&in->ops[1], it, report, &v) == false) {
		v = 0;
	}
	if (check_range(&it->loc, v, 0, 31, report, "immediate") == false) {
		return false;
	}
	enc_adds_imm(ws, sub, rd, (unsigned)v);
	return true;
}

static bool enc_cmp(struct words *ws, struct item *it, struct instr *in,
		    bool report)
{
	struct operand *a = &in->ops[0];
	struct operand *b = &in->ops[1];
	int64_t v;

	if (b->kind == OP_REG) {
		put(ws, MEOW_ENCODE_CMPR(a->reg, a->alt, b->alt, b->reg));
		return true;
	}
	if (imm_value(b, it, report, &v) == false) {
		v = 0;
	}
	if (fits_simm(v, 8) == true) {
		put(ws, MEOW_ENCODE_CMPI(a->reg, (uint32_t)v));
		return true;
	}
	if (operand_to_ir(ws, &it->loc, a->reg, (uint32_t)v, report) == false) {
		return false;
	}
	put(ws, MEOW_ENCODE_CMPR(a->reg, 0, 0, MEOW_IR));
	return true;
}

static bool enc_tst(struct words *ws, struct item *it, struct instr *in,
		    bool report)
{
	int64_t v;
	int bit;

	if (imm_value(&in->ops[1], it, report, &v) == false) {
		v = 1;
	}
	bit = bit_index((uint32_t)v);
	if (bit < 0 || (v >> 32) != 0) {
		if (report == true) {
			error_at(&it->loc, "TST needs a value with one bit set");
		}
		return false;
	}
	put(ws, MEOW_ENCODE_TST(in->ops[0].reg, in->ops[0].alt, (unsigned)bit));
	return true;
}

static bool enc_movi(struct words *ws, struct item *it, struct instr *in,
		     bool report)
{
	struct operand *a = &in->ops[0];
	struct operand *b = &in->ops[1];
	int64_t v;

	if (b->kind == OP_REG) {
		enc_mov(ws, a->reg, a->alt, b->reg, b->alt, in->flags);
		return true;
	}
	if (imm_value(b, it, report, &v) == false) {
		v = 0;
	}
	if (check_range(&it->loc, v, INT32_MIN, UINT32_MAX, report,
			"immediate") == false) {
		return false;
	}
	mov_imm(ws, a->reg, (uint32_t)v);
	return true;
}

static bool enc_ldi_op(struct words *ws, struct item *it, struct instr *in,
		       bool report)
{
	int64_t v;

	if (imm_value(&in->ops[0], it, report, &v) == false) {
		v = 0;
	}
	if (check_range(&it->loc, v, -2048, 2047, report, "immediate") == false) {
		return false;
	}
	enc_ldi(ws, v);
	return true;
}

static bool enc_shift(struct words *ws, struct item *it, struct instr *in,
		      bool report)
{
	unsigned kind = in->flags;
	unsigned rd = in->ops[0].reg;
	int64_t v;

	if (in->ops[1].kind == OP_REG) {
		unsigned arith = kind == F_SHIFT_ASR;
		unsigned left = kind == F_SHIFT_LSL || kind == F_SHIFT_ROL;
		unsigned rot = kind == F_SHIFT_ROL || kind == F_SHIFT_ROR;

		put(ws, arith != 0 ? MEOW_ENCODE_ASRR(rd, in->ops[1].reg)
				   : MEOW_ENCODE_SHR(rd, left, rot, in->ops[1].reg));
		return true;
	}
	if (imm_value(&in->ops[1], it, report, &v) == false) {
		v = 0;
	}
	if (check_range(&it->loc, v, 0, 31, report, "shift amount") == false) {
		return false;
	}
	enc_shift_imm(ws, rd, kind, (unsigned)v);
	return true;
}

static bool enc_bit(struct words *ws, struct item *it, struct instr *in,
		    bool report)
{
	unsigned op = in->flags & 3;
	unsigned inv = (in->flags & F_BIT_INV) != 0;
	unsigned rd = in->ops[0].reg;
	int64_t v;
	int bit;

	if (in->ops[1].kind == OP_REG) {
		enc_bit_reg(ws, rd, op, inv, in->ops[1].reg);
		return true;
	}
	if (imm_value(&in->ops[1], it, report, &v) == false) {
		v = 1;
	}
	bit = bit_index((uint32_t)v);
	if (bit >= 0 && (v >> 32) == 0) {
		enc_bit_imm(ws, rd, op, inv, (unsigned)bit);
		return true;
	}
	if (operand_to_ir(ws, &it->loc, rd, (uint32_t)v, report) == false) {
		return false;
	}
	enc_bit_reg(ws, rd, op, inv, MEOW_IR);
	return true;
}

static bool enc_memop(struct words *ws, struct item *it, struct instr *in,
		      bool report)
{
	(void)it;
	(void)report;
	enc_mem(ws, in->flags, in->ops[0].reg, in->ops[1].reg, in->ops[1].wb);
	return true;
}

/* ADR rd, target: shortest pc-relative sequence for a distance d. */
static bool enc_adr_delta(struct words *ws, struct item *it, unsigned rd,
			  int64_t d, bool report, unsigned min_words)
{
	if (min_words <= 1) {
		if (d == 0) {
			enc_mov(ws, rd, false, MEOW_PC, false, 0);
			return true;
		}
		if (d >= 1 && d <= 15) {
			enc_add(ws, 0, rd, MEOW_PC, (unsigned)d);
			return true;
		}
		if (d <= -1 && d >= -15) {
			enc_add(ws, 1, rd, MEOW_PC, (unsigned)-d);
			return true;
		}
	}
	if (min_words <= 2) {
		if (d >= -255 && d <= 255) {
			enc_mov(ws, rd, false, MEOW_PC, false, 0);
			enc_add8(ws, d < 0, rd, (unsigned)(d < 0 ? -d : d));
			return true;
		}
	}
	if (rd == MEOW_IR && fits_simm(d - 2, 12) == true) {
		enc_ldi(ws, d - 2);
		enc_add(ws, 0, MEOW_IR, MEOW_PC, 0);
		return true;
	}
	if (rd != MEOW_IR && fits_simm(d - 2, 12) == true) {
		enc_ldi(ws, d - 2);
		enc_mov(ws, rd, false, MEOW_PC, false, 0);
		enc_add(ws, 0, rd, MEOW_IR, 0);
		return true;
	}
	if (report == true) {
		error_at(&it->loc, "ADR target too far (%lld bytes); use LDR rd, =label",
			 (long long)d);
	}
	return false;
}

static bool enc_adr(struct words *ws, struct item *it, struct instr *in,
		    bool report)
{
	int64_t d;

	if (pc_delta(it, in->ops[1].expr, report, &d) == false) {
		d = 0;
	}
	return enc_adr_delta(ws, it, in->ops[0].reg, d, report, in->min_words);
}

/* LDR rd, =expr: MOV if the constant is cheap, else a literal pool load. */
static bool enc_ldr_lit(struct words *ws, struct item *it, struct instr *in,
			bool report)
{
	unsigned rd = in->ops[0].reg;
	struct value v;
	struct item *pool = in->lit->pool;
	int64_t d = 0;

	if (in->min_words <= 2 &&
	    expr_eval(in->ops[1].expr, it, false, &v) == true &&
	    value_is_abs(&v) == true) {
		struct words tmp = { { 0 }, 0 };

		mov_imm(&tmp, rd, (uint32_t)v.v);
		if (tmp.n <= 2) {
			*ws = tmp;
			return true;
		}
	}
	if (pool == NULL) {
		if (report == true) {
			error_at(&it->loc, "no literal pool follows this LDR; add LTORG");
		}
		return false;
	}
	if (layout_done == true || pool->pass + 1 >= layout_pass) {
		d = (int64_t)pool->addr + in->lit->offset - (int64_t)it->addr;
	}
	if (enc_adr_delta(ws, it, MEOW_IR, d, report,
			  in->min_words > 1 ? in->min_words - 1 : 1) == false) {
		return false;
	}
	enc_mem(ws, F_LOAD | F_SIZE_W, rd, MEOW_IR, 0);
	return true;
}

static bool enc_pushpop(struct words *ws, struct item *it, struct instr *in,
			bool report, unsigned pop)
{
	uint16_t list = in->ops[0].reglist;
	int r;

	(void)it;
	(void)report;
	if (pop != 0) {
		for (r = 0; r < 16; r++) {
			if ((list & (1u << r)) != 0) {
				enc_mem(ws, F_LOAD | F_SIZE_W, (unsigned)r,
					MEOW_SP, 3);
			}
		}
	} else {
		for (r = 15; r >= 0; r--) {
			if ((list & (1u << r)) != 0) {
				enc_mem(ws, F_STORE | F_SIZE_W, (unsigned)r,
					MEOW_SP, 1);
			}
		}
	}
	return true;
}

static bool encode(struct words *ws, struct item *it, bool report)
{
	struct instr *in = &it->u.instr;
	int64_t v;

	ws->n = 0;
	switch (def_of(in->mn)->which) {
	case M_B:
		if (in->cond != MEOW_COND_NV) {
			return enc_branch(ws, it, in, report, 0);
		}
		/* fall through */
	case M_BL:
		return enc_branch(ws, it, in, report, 1);
	case M_BNV:
		if (imm_value(&in->ops[0], it, report, &v) == false) {
			v = 0;
		}
		if (check_range(&it->loc, v, -512, 510, report,
				"BNV operand") == false) {
			return false;
		}
		if ((v & 1) != 0) {
			if (report == true) {
				error_at(&it->loc, "BNV operand must be even");
			}
			return false;
		}
		put(ws, MEOW_ENCODE_B(MEOW_COND_NV, (uint32_t)(v / 2)));
		return true;
	case M_ADD:
	case M_SUB:
		return enc_arith(ws, it, in, report);
	case M_ADDS:
		return enc_arith_s(ws, it, in, report);
	case M_CMP:
		return enc_cmp(ws, it, in, report);
	case M_TST:
		return enc_tst(ws, it, in, report);
	case M_MOV:
		return enc_movi(ws, it, in, report);
	case M_LDI:
		return enc_ldi_op(ws, it, in, report);
	case M_SHIFT:
		return enc_shift(ws, it, in, report);
	case M_BIT:
		return enc_bit(ws, it, in, report);
	case M_MEM:
		if (in->ops[1].kind == OP_EXPR) {
			return enc_ldr_lit(ws, it, in, report);
		}
		return enc_memop(ws, it, in, report);
	case M_ADR:
		return enc_adr(ws, it, in, report);
	case M_PUSH:
		return enc_pushpop(ws, it, in, report, 0);
	case M_POP:
		return enc_pushpop(ws, it, in, report, 1);
	case M_NOP:
		enc_mov(ws, 0, false, 0, false, 0);
		return true;
	case M_RET:
		enc_mov(ws, MEOW_PC, false, MEOW_LR, false, 0);
		return true;
	case M_IRQRTN:
		put(ws, MEOW_ENCODE_B(MEOW_COND_NV, MEOW_BNV_IRQRTN / 2));
		return true;
	}
	return false;
}

uint32_t instr_size(struct item *it)
{
	struct words ws;
	struct instr *in = &it->u.instr;

	encode(&ws, it, false);
	if (ws.n > in->min_words) {
		in->min_words = ws.n;
	}
	return in->min_words * 2;
}

void instr_emit(struct item *it, uint8_t *out)
{
	struct words ws;
	struct instr *in = &it->u.instr;
	unsigned i;

	if (encode(&ws, it, true) == false) {
		ws.n = 0;
	}
	if (ws.n > in->min_words) {
		error_at(&it->loc, "internal: instruction grew at emit time");
		ws.n = in->min_words;
	}
	while (ws.n < in->min_words) {
		put(&ws, MEOW_ENCODE_MOV(0, 0, 0, 0, 0, 0));
	}
	for (i = 0; i < ws.n; i++) {
		out[i * 2] = (uint8_t)(ws.w[i] & 0xff);
		out[i * 2 + 1] = (uint8_t)(ws.w[i] >> 8);
	}
}
