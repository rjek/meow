#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mas.h"

bool ieq(const char *a, const char *b)
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

void lex_init(struct lexer *lx, const char *line, const struct loc *loc)
{
	lx->line = line;
	lx->pos = 0;
	lx->loc = *loc;
	lx->have_tok = false;
	memset(&lx->tok, 0, sizeof lx->tok);
}

void lex_free_token(struct token *t)
{
	free(t->text);
	t->text = NULL;
}

static bool is_ident_start(int c)
{
	return isalpha(c) || c == '_' || c == '.' || c == '$';
}

static bool is_ident_char(int c)
{
	return isalnum(c) || c == '_' || c == '.' || c == '$';
}

/* Decode one escape after a backslash; p points after the backslash. */
static int unescape(const char **p)
{
	int c = (unsigned char)**p;

	(*p)++;
	switch (c) {
	case 'n': return '\n';
	case 't': return '\t';
	case 'r': return '\r';
	case '0': return '\0';
	case 'a': return '\a';
	case 'b': return '\b';
	case 'e': return 27;
	case 'x': {
		int v = 0;
		int n = 0;

		while (n < 2 && isxdigit((unsigned char)**p)) {
			int d = (unsigned char)**p;
			v = v * 16 + (isdigit(d) ? d - '0' : tolower(d) - 'a' + 10);
			(*p)++;
			n++;
		}
		return v;
	}
	default: return c;
	}
}

static void lex_number(struct lexer *lx, struct token *t)
{
	const char *p = lx->line + lx->pos;
	const char *start = p;
	uint64_t v = 0;
	int base = 10;

	if (*p == '&') {
		base = 16;
		p++;
	} else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
		base = 16;
		p += 2;
	} else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B')) {
		base = 2;
		p += 2;
	}
	while (isalnum((unsigned char)*p) || *p == '_') {
		int c = (unsigned char)*p;
		int d;

		if (c == '_') {
			p++;
			continue;
		}
		d = isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
		if (d >= base) {
			struct loc l = lx->loc;

			l.col = (int)(p - lx->line) + 1;
			error_at(&l, "bad digit '%c' in number", c);
			while (isalnum((unsigned char)*p)) {
				p++;
			}
			break;
		}
		v = v * (uint64_t)base + (uint64_t)d;
		p++;
	}
	if (p == start + (base == 10 ? 0 : (*start == '&' ? 1 : 2))) {
		struct loc l = lx->loc;

		l.col = (int)(start - lx->line) + 1;
		error_at(&l, "number has no digits");
	}
	t->kind = TOK_NUMBER;
	t->number = (uint32_t)v;
	lx->pos = (size_t)(p - lx->line);
}

static void lex_string(struct lexer *lx, struct token *t, int quote)
{
	const char *p = lx->line + lx->pos + 1;
	char *buf = xmalloc(strlen(p) + 1);
	size_t n = 0;

	while (*p != '\0' && *p != quote) {
		if (*p == '\\' && p[1] != '\0') {
			p++;
			buf[n++] = (char)unescape(&p);
		} else {
			buf[n++] = *p++;
		}
	}
	if (*p != quote) {
		error_at(&lx->loc, "unterminated string");
	} else {
		p++;
	}
	buf[n] = '\0';
	if (quote == '\'') {
		/* character constant */
		if (n != 1) {
			struct loc l = lx->loc;

			l.col = t->col;
			error_at(&l, "character constant must be one character");
		}
		t->kind = TOK_NUMBER;
		t->number = n > 0 ? (unsigned char)buf[0] : 0;
		free(buf);
	} else {
		t->kind = TOK_STRING;
		t->text = buf;
		t->len = n;
	}
	lx->pos = (size_t)(p - lx->line);
}

static void lex_scan(struct lexer *lx)
{
	struct token *t = &lx->tok;
	const char *p;
	int c;

	lex_free_token(t);
	memset(t, 0, sizeof *t);
	while (lx->line[lx->pos] == ' ' || lx->line[lx->pos] == '\t') {
		lx->pos++;
	}
	p = lx->line + lx->pos;
	t->col = (int)lx->pos + 1;
	c = (unsigned char)*p;
	if (c == '\0' || c == ';' || c == '\n' || c == '\r') {
		t->kind = TOK_EOL;
		return;
	}
	if (isdigit(c) || (c == '&' && isxdigit((unsigned char)p[1]))) {
		lex_number(lx, t);
		return;
	}
	if (c == '"' || c == '\'') {
		lex_string(lx, t, c);
		return;
	}
	if (is_ident_start(c)) {
		size_t n = 1;

		while (is_ident_char((unsigned char)p[n])) {
			n++;
		}
		t->kind = TOK_IDENT;
		t->text = xstrndup(p, n);
		t->len = n;
		lx->pos += n;
		return;
	}
	t->kind = TOK_PUNCT;
	t->punct = c;
	lx->pos++;
	switch (c) {
	case '<':
		if (p[1] == '<') { t->punct = P_SHL; lx->pos++; }
		else if (p[1] == '=') { t->punct = P_LE; lx->pos++; }
		break;
	case '>':
		if (p[1] == '>') { t->punct = P_SHR; lx->pos++; }
		else if (p[1] == '=') { t->punct = P_GE; lx->pos++; }
		break;
	case '=':
		if (p[1] == '=') { t->punct = P_EQ; lx->pos++; }
		break;
	case '!':
		if (p[1] == '=') { t->punct = P_NE; lx->pos++; }
		break;
	case '&':
		if (p[1] == '&') { t->punct = P_LAND; lx->pos++; }
		break;
	case '|':
		if (p[1] == '|') { t->punct = P_LOR; lx->pos++; }
		break;
	default:
		break;
	}
}

struct token *lex_peek(struct lexer *lx)
{
	if (lx->have_tok == false) {
		lex_scan(lx);
		lx->have_tok = true;
	}
	return &lx->tok;
}

void lex_next(struct lexer *lx)
{
	lex_peek(lx);
	lx->have_tok = false;
}

bool lex_is_punct(struct lexer *lx, int punct)
{
	struct token *t = lex_peek(lx);

	return t->kind == TOK_PUNCT && t->punct == punct;
}

bool lex_accept_punct(struct lexer *lx, int punct)
{
	if (lex_is_punct(lx, punct) == true) {
		lex_next(lx);
		return true;
	}
	return false;
}

static const char *punct_name(int punct, char *buf)
{
	switch (punct) {
	case P_SHL: return "<<";
	case P_SHR: return ">>";
	case P_LE: return "<=";
	case P_GE: return ">=";
	case P_EQ: return "==";
	case P_NE: return "!=";
	case P_LAND: return "&&";
	case P_LOR: return "||";
	default:
		buf[0] = (char)punct;
		buf[1] = '\0';
		return buf;
	}
}

bool lex_expect_punct(struct lexer *lx, int punct)
{
	char b[2];

	if (lex_accept_punct(lx, punct) == true) {
		return true;
	}
	{
		struct loc l = lex_loc(lx);

		error_at(&l, "expected '%s'", punct_name(punct, b));
	}
	return false;
}

bool lex_is_ident(struct lexer *lx, const char *word)
{
	struct token *t = lex_peek(lx);

	return t->kind == TOK_IDENT && ieq(t->text, word) == true;
}

bool lex_at_eol(struct lexer *lx)
{
	return lex_peek(lx)->kind == TOK_EOL;
}

bool lex_expect_eol(struct lexer *lx)
{
	if (lex_at_eol(lx) == true) {
		return true;
	}
	{
		struct loc l = lex_loc(lx);

		error_at(&l, "unexpected text at end of line");
	}
	return false;
}

struct loc lex_loc(struct lexer *lx)
{
	struct loc l = lx->loc;

	l.col = lex_peek(lx)->col;
	return l;
}
