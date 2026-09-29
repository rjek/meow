#include <stdlib.h>
#include <string.h>

#include "mas.h"

enum expr_kind { E_CONST, E_SYMBOL, E_HERE, E_UNARY, E_BINARY };

struct expr {
	enum expr_kind kind;
	int op;
	int64_t value;
	struct symbol *sym;
	struct expr *l;
	struct expr *r;
	struct loc loc;
};

static struct expr *expr_new(enum expr_kind kind, const struct loc *loc)
{
	struct expr *e = xcalloc(1, sizeof *e);

	e->kind = kind;
	e->loc = *loc;
	return e;
}

struct expr *expr_const(int64_t v)
{
	struct loc none = { NULL, 0, 0 };
	struct expr *e = expr_new(E_CONST, &none);

	e->value = v;
	return e;
}

struct expr *expr_symbol(const char *name, const struct loc *loc)
{
	struct expr *e = expr_new(E_SYMBOL, loc);

	e->sym = sym_lookup(name);
	e->sym->refs++;
	return e;
}

const struct loc *expr_loc(const struct expr *e)
{
	return &e->loc;
}

bool expr_lazy_set;

static struct expr *parse_binary(struct lexer *lx, int level);

static struct expr *parse_primary(struct lexer *lx)
{
	struct token *t = lex_peek(lx);
	struct loc loc = lex_loc(lx);
	struct expr *e;

	switch (t->kind) {
	case TOK_NUMBER:
		e = expr_new(E_CONST, &loc);
		e->value = t->number;
		lex_next(lx);
		return e;
	case TOK_IDENT:
		if (strcmp(t->text, ".") == 0) {
			e = expr_new(E_HERE, &loc);
		} else {
			struct symbol *sym;

			if (t->text[0] == '.') {
				sym = sym_lookup(local_label_name(t->text));
			} else {
				sym = sym_lookup(t->text);
			}
			if (sym->settable == true && expr_lazy_set == false) {
				/* SET variables take their value now */
				struct value v;

				e = expr_new(E_CONST, &loc);
				if (expr_eval(sym->expr, NULL, true, &v) == true) {
					e->value = v.v;
				}
			} else {
				e = expr_new(E_SYMBOL, &loc);
				e->sym = sym;
				sym->refs++;
			}
		}
		lex_next(lx);
		return e;
	case TOK_STRING:
		if (t->len == 1) {
			e = expr_new(E_CONST, &loc);
			e->value = (unsigned char)t->text[0];
			lex_next(lx);
			return e;
		}
		error_at(&loc, "string in expression");
		lex_next(lx);
		return NULL;
	case TOK_PUNCT:
		if (t->punct == '(') {
			lex_next(lx);
			e = parse_binary(lx, 0);
			if (lex_expect_punct(lx, ')') == false) {
				return NULL;
			}
			return e;
		}
		if (t->punct == '-' || t->punct == '~' || t->punct == '!' ||
		    t->punct == '+') {
			int op = t->punct;
			struct expr *operand;

			lex_next(lx);
			operand = parse_primary(lx);
			if (operand == NULL) {
				return NULL;
			}
			if (op == '+') {
				return operand;
			}
			e = expr_new(E_UNARY, &loc);
			e->op = op;
			e->l = operand;
			return e;
		}
		break;
	default:
		break;
	}
	error_at(&loc, "expected expression");
	return NULL;
}

/* Binary operators by precedence, lowest first. */
static const int levels[][5] = {
	{ P_LOR, 0 },
	{ P_LAND, 0 },
	{ '|', 0 },
	{ '^', 0 },
	{ '&', 0 },
	{ P_EQ, P_NE, 0 },
	{ '<', '>', P_LE, P_GE, 0 },
	{ P_SHL, P_SHR, 0 },
	{ '+', '-', 0 },
	{ '*', '/', '%', 0 },
};

#define NLEVELS ((int)(sizeof levels / sizeof levels[0]))

static struct expr *parse_binary(struct lexer *lx, int level)
{
	struct expr *l;

	if (level == NLEVELS) {
		return parse_primary(lx);
	}
	l = parse_binary(lx, level + 1);
	while (l != NULL) {
		struct token *t = lex_peek(lx);
		int i;
		int op = 0;

		if (t->kind != TOK_PUNCT) {
			break;
		}
		for (i = 0; levels[level][i] != 0; i++) {
			if (levels[level][i] == t->punct) {
				op = t->punct;
			}
		}
		if (op == 0) {
			break;
		}
		{
			struct loc loc = lex_loc(lx);
			struct expr *r;
			struct expr *e;

			lex_next(lx);
			r = parse_binary(lx, level + 1);
			if (r == NULL) {
				return NULL;
			}
			e = expr_new(E_BINARY, &loc);
			e->op = op;
			e->l = l;
			e->r = r;
			l = e;
		}
	}
	return l;
}

struct expr *expr_parse(struct lexer *lx)
{
	return parse_binary(lx, 0);
}

static bool eval_symbol(struct expr *e, const struct item *here, bool report,
			struct value *out)
{
	struct symbol *s = e->sym;

	switch (s->kind) {
	case SYM_LABEL:
		/* forward references use the previous pass's address */
		if (s->anchor == NULL ||
		    (layout_done == false && s->anchor->pass + 1 < layout_pass) ||
		    s->anchor->pass == 0) {
			if (report == true) {
				error_at(&e->loc, "'%s' is not yet defined here",
					 s->name);
			}
			return false;
		}
		out->v = s->anchor->addr;
		out->sec = s->sec;
		out->ext = NULL;
		return true;
	case SYM_EQU:
		if (s->evaluating == true) {
			if (report == true) {
				error_at(&e->loc, "'%s' is defined in terms of itself",
					 s->name);
			}
			return false;
		}
		s->evaluating = true;
		{
			bool ok = expr_eval(s->expr, here, report, out);

			s->evaluating = false;
			return ok;
		}
	case SYM_SECTION:
		out->v = 0;
		out->sec = s->sec;
		out->ext = NULL;
		return true;
	case SYM_REGISTER:
		if (report == true) {
			error_at(&e->loc, "'%s' is a register name", s->name);
		}
		return false;
	case SYM_UNDEFINED:
		if (s->imported == true) {
			out->v = 0;
			out->sec = NULL;
			out->ext = s;
			return true;
		}
		if (report == true) {
			error_at(&e->loc, "undefined symbol '%s'", s->name);
		}
		return false;
	}
	return false;
}

static bool relocatable_error(struct expr *e, bool report, const char *what)
{
	if (report == true) {
		error_at(&e->loc, "%s of a non-absolute value", what);
	}
	return false;
}

bool expr_eval(struct expr *e, const struct item *here, bool report,
	       struct value *out)
{
	struct value a;
	struct value b;

	if (e == NULL) {
		return false;
	}
	switch (e->kind) {
	case E_CONST:
		out->v = e->value;
		out->sec = NULL;
		out->ext = NULL;
		return true;
	case E_HERE:
		if (here == NULL) {
			if (report == true) {
				error_at(&e->loc, "'.' has no value here");
			}
			return false;
		}
		out->v = here->addr;
		out->sec = here->sec;
		out->ext = NULL;
		return true;
	case E_SYMBOL:
		return eval_symbol(e, here, report, out);
	case E_UNARY:
		if (expr_eval(e->l, here, report, &a) == false) {
			return false;
		}
		if (value_is_abs(&a) == false) {
			return relocatable_error(e, report, "unary operation");
		}
		out->sec = NULL;
		out->ext = NULL;
		switch (e->op) {
		case '-': out->v = -a.v; break;
		case '~': out->v = ~a.v; break;
		case '!': out->v = a.v == 0 ? 1 : 0; break;
		}
		return true;
	case E_BINARY:
		if (expr_eval(e->l, here, report, &a) == false ||
		    expr_eval(e->r, here, report, &b) == false) {
			return false;
		}
		out->sec = NULL;
		out->ext = NULL;
		if (e->op == '+') {
			if (value_is_abs(&b) == true) {
				out->sec = a.sec;
				out->ext = a.ext;
			} else if (value_is_abs(&a) == true) {
				out->sec = b.sec;
				out->ext = b.ext;
			} else {
				return relocatable_error(e, report, "addition");
			}
			out->v = a.v + b.v;
			return true;
		}
		if (e->op == '-') {
			if (value_is_abs(&b) == true) {
				out->sec = a.sec;
				out->ext = a.ext;
			} else if (a.sec != NULL && a.sec == b.sec) {
				/* difference of two labels in one section */
			} else if (a.ext != NULL && a.ext == b.ext) {
			} else {
				return relocatable_error(e, report,
							 "subtraction");
			}
			out->v = a.v - b.v;
			return true;
		}
		if (value_is_abs(&a) == false || value_is_abs(&b) == false) {
			return relocatable_error(e, report, "operation");
		}
		switch (e->op) {
		case '*': out->v = a.v * b.v; break;
		case '/':
		case '%':
			if (b.v == 0) {
				if (report == true) {
					error_at(&e->loc, "division by zero");
				}
				return false;
			}
			out->v = e->op == '/' ? a.v / b.v : a.v % b.v;
			break;
		case P_SHL: out->v = (int64_t)((uint64_t)a.v << (b.v & 63)); break;
		case P_SHR: out->v = (int64_t)((uint64_t)a.v >> (b.v & 63)); break;
		case '<': out->v = a.v < b.v; break;
		case '>': out->v = a.v > b.v; break;
		case P_LE: out->v = a.v <= b.v; break;
		case P_GE: out->v = a.v >= b.v; break;
		case P_EQ: out->v = a.v == b.v; break;
		case P_NE: out->v = a.v != b.v; break;
		case '&': out->v = a.v & b.v; break;
		case '^': out->v = a.v ^ b.v; break;
		case '|': out->v = a.v | b.v; break;
		case P_LAND: out->v = a.v != 0 && b.v != 0; break;
		case P_LOR: out->v = a.v != 0 || b.v != 0; break;
		}
		return true;
	}
	return false;
}

bool expr_eval_abs(struct expr *e, const struct item *here, bool report,
		   int64_t *out)
{
	struct value v;

	if (expr_eval(e, here, report, &v) == false) {
		return false;
	}
	if (value_is_abs(&v) == false) {
		if (report == true) {
			error_at(&e->loc, "expression must be absolute");
		}
		return false;
	}
	*out = v.v;
	return true;
}
