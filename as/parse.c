/* Line-level assembly: sources, labels, directives, macros, conditionals,
 * literal pools, layout and emission. */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "mas.h"

struct symbol *entry_symbol;
unsigned layout_pass;
bool layout_done;

struct source_line *listing_lines;
int listing_count;
static int listing_cap;

/* ---- include path ----------------------------------------------------- */

static const char *include_dirs[32];
static unsigned n_include_dirs;

void add_include_dir(const char *dir)
{
	if (n_include_dirs < sizeof include_dirs / sizeof include_dirs[0]) {
		include_dirs[n_include_dirs++] = dir;
	}
}

static FILE *open_relative(const char *from, const char *name, char **path)
{
	char buf[1024];
	FILE *f;
	unsigned i;
	const char *slash;

	if (name[0] == '/') {
		f = fopen(name, "rb");
		if (f != NULL) {
			*path = xstrdup(name);
		}
		return f;
	}
	slash = from != NULL ? strrchr(from, '/') : NULL;
	if (slash != NULL) {
		snprintf(buf, sizeof buf, "%.*s/%s", (int)(slash - from), from,
			 name);
	} else {
		snprintf(buf, sizeof buf, "%s", name);
	}
	f = fopen(buf, "rb");
	for (i = 0; f == NULL && i < n_include_dirs; i++) {
		snprintf(buf, sizeof buf, "%s/%s", include_dirs[i], name);
		f = fopen(buf, "rb");
	}
	if (f != NULL) {
		*path = xstrdup(buf);
	}
	return f;
}

/* ---- sources ---------------------------------------------------------- */

struct macro {
	char *name;
	char **params;
	char **defaults;
	unsigned nparams;
	char **lines;
	unsigned nlines;
	struct loc loc;
	struct macro *next;
};

static struct macro *macros;

enum source_kind { SRC_FILE, SRC_MACRO, SRC_WHILE };

struct source {
	enum source_kind kind;
	FILE *f;
	char *path;
	int line;
	char **lines;		/* macro or while body */
	unsigned nlines;
	unsigned next;
	struct loc loc;		/* of the body's first line */
	struct macro *macro;
	const char *saved_expansion_name;
	struct loc saved_expansion_loc;
	struct expr *cond;	/* while */
	struct loc invoke_loc;
	unsigned iterations;
	unsigned if_depth;	/* conditional stack depth on entry */
	struct source *up;
};

static struct source *src;
static unsigned source_depth;

static void push_source(struct source *s)
{
	s->up = src;
	src = s;
	source_depth++;
	if (source_depth > 200) {
		fatal("include or macro nesting too deep");
	}
}

/* ---- conditionals ----------------------------------------------------- */

struct cond_state {
	bool active;		/* lines are assembled */
	bool parent_active;
	bool taken;		/* some branch of this IF was taken */
	bool seen_else;
	struct loc loc;
};

static struct cond_state cond_stack[64];
static unsigned cond_depth;

static bool assembling(void)
{
	return cond_depth == 0 || cond_stack[cond_depth - 1].active == true;
}

/* ---- literal pools ---------------------------------------------------- */

struct literal *literal_add(struct expr *e, const struct loc *loc)
{
	struct literal *l = xcalloc(1, sizeof *l);

	if (cur_sec == NULL) {
		sec_select(sec_lookup(".text", true));
	}
	l->expr = e;
	l->loc = *loc;
	*cur_sec->pending_tail = l;
	cur_sec->pending_tail = &l->next;
	return l;
}

bool literal_pool_flush(const struct loc *loc, bool at_end)
{
	struct item *al;
	struct item *it;
	struct literal *l;
	unsigned n = 0;

	(void)at_end;
	if (cur_sec == NULL || cur_sec->pending == NULL) {
		return false;
	}
	al = item_new(ITEM_ALIGN, loc);
	al->u.align.align = 4;
	item_append(al);
	it = item_new(ITEM_POOL, loc);
	it->u.pool.lits = cur_sec->pending;
	for (l = cur_sec->pending; l != NULL; l = l->next) {
		l->pool = it;
		l->offset = n * 4;
		n++;
	}
	it->u.pool.n = n;
	item_append(it);
	cur_sec->pending = NULL;
	cur_sec->pending_tail = &cur_sec->pending;
	return true;
}

/* ---- labels ----------------------------------------------------------- */

static struct item *anchor_here(const struct loc *loc)
{
	struct item *it = item_new(ITEM_ANCHOR, loc);

	item_append(it);
	return it;
}

static bool define_symbol(struct symbol *s, const struct loc *loc)
{
	if (s->kind != SYM_UNDEFINED) {
		error_at(loc, "'%s' already defined at %s:%d", s->name,
			 s->def.file, s->def.line);
		return false;
	}
	if (s->imported == true) {
		error_at(loc, "'%s' is imported and cannot be defined", s->name);
		return false;
	}
	s->def = *loc;
	return true;
}

static void define_label(const char *name, const struct loc *loc)
{
	struct symbol *s;

	if (name[0] == '.') {
		s = sym_lookup(local_label_name(name));
	} else {
		s = sym_lookup(name);
		set_label_scope(name);
	}
	if (define_symbol(s, loc) == false) {
		return;
	}
	s->kind = SYM_LABEL;
	s->anchor = anchor_here(loc);
	s->sec = cur_sec;
}

/* ---- data directives -------------------------------------------------- */

static void add_listing_line(const char *text, const struct loc *loc)
{
	if (listing_count == listing_cap) {
		listing_cap = listing_cap == 0 ? 256 : listing_cap * 2;
		listing_lines = xrealloc(listing_lines,
					 (size_t)listing_cap * sizeof *listing_lines);
	}
	listing_lines[listing_count].text = xstrdup(text);
	listing_lines[listing_count].loc = *loc;
	listing_count++;
}

static void d_data(struct lexer *lx, const struct loc *loc, unsigned width)
{
	struct item *it = item_new(ITEM_DATA, loc);
	unsigned cap = 8;

	it->u.data.width = width;
	it->u.data.exprs = xmalloc(cap * sizeof *it->u.data.exprs);
	do {
		struct token *t = lex_peek(lx);

		if (t->kind == TOK_STRING && width == 1) {
			/* strings become constant expressions per byte */
			{
				size_t i;

				for (i = 0; i < t->len; i++) {
					if (it->u.data.n == cap) {
						cap *= 2;
						it->u.data.exprs = xrealloc(
							it->u.data.exprs,
							cap * sizeof *it->u.data.exprs);
					}
					it->u.data.exprs[it->u.data.n++] =
						expr_const((unsigned char)t->text[i]);
				}
			}
			lex_next(lx);
			continue;
		}
		{
			struct expr *e = expr_parse(lx);

			if (e == NULL) {
				return;
			}
			if (it->u.data.n == cap) {
				cap *= 2;
				it->u.data.exprs = xrealloc(it->u.data.exprs,
							    cap * sizeof *it->u.data.exprs);
			}
			it->u.data.exprs[it->u.data.n++] = e;
		}
	} while (lex_accept_punct(lx, ',') == true);
	if (lex_expect_eol(lx) == true) {
		item_append(it);
	}
}

static void d_space(struct lexer *lx, const struct loc *loc)
{
	struct item *it = item_new(ITEM_SPACE, loc);

	it->u.space.count = expr_parse(lx);
	if (it->u.space.count == NULL) {
		return;
	}
	if (lex_accept_punct(lx, ',') == true) {
		int64_t fill;

		if (expr_eval_abs(expr_parse(lx), NULL, true, &fill) == false) {
			return;
		}
		it->u.space.fill = (unsigned)fill & 0xff;
	}
	if (lex_expect_eol(lx) == true) {
		item_append(it);
	}
}

static void d_align(struct lexer *lx, const struct loc *loc)
{
	struct item *it = item_new(ITEM_ALIGN, loc);
	int64_t n = 4;

	if (lex_at_eol(lx) == false) {
		if (expr_eval_abs(expr_parse(lx), NULL, true, &n) == false) {
			return;
		}
		if (lex_accept_punct(lx, ',') == true) {
			int64_t fill;

			if (expr_eval_abs(expr_parse(lx), NULL, true, &fill) == false) {
				return;
			}
			it->u.align.fill = (unsigned)fill & 0xff;
		}
	}
	if (n <= 0 || (n & (n - 1)) != 0 || n > 65536) {
		error_at(loc, "alignment must be a power of two");
		return;
	}
	it->u.align.align = (unsigned)n;
	if (lex_expect_eol(lx) == true) {
		item_append(it);
	}
}

static char *take_filename(struct lexer *lx)
{
	struct token *t = lex_peek(lx);
	char *name;

	if (t->kind == TOK_STRING) {
		name = xstrdup(t->text);
		lex_next(lx);
		return name;
	}
	/* bare name: the rest of the line up to a comment */
	{
		const char *p = lx->line + t->col - 1;
		const char *end;

		if (t->kind == TOK_EOL) {
			struct loc l = lex_loc(lx);

			error_at(&l, "expected a file name");
			return NULL;
		}
		end = p;
		while (*end != '\0' && *end != ';' && *end != '\n' &&
		       *end != '\r') {
			end++;
		}
		while (end > p && (end[-1] == ' ' || end[-1] == '\t')) {
			end--;
		}
		name = xstrndup(p, (size_t)(end - p));
		lx->pos = (size_t)(end - lx->line);
		lx->have_tok = false;
		return name;
	}
}

static void d_incbin(struct lexer *lx, const struct loc *loc)
{
	char *name = take_filename(lx);
	char *path;
	FILE *f;
	struct item *it;
	long size;

	if (name == NULL) {
		return;
	}
	f = open_relative(src->path, name, &path);
	if (f == NULL) {
		error_at(loc, "cannot open '%s'", name);
		free(name);
		return;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	it = item_new(ITEM_BYTES, loc);
	it->u.bytes.n = (size_t)size;
	it->u.bytes.data = xmalloc((size_t)size);
	if (fread(it->u.bytes.data, 1, (size_t)size, f) != (size_t)size) {
		error_at(loc, "short read on '%s'", path);
	}
	fclose(f);
	free(name);
	item_append(it);
}

static bool push_file(const char *name, const struct loc *loc)
{
	struct source *s = xcalloc(1, sizeof *s);
	char *path = NULL;

	s->kind = SRC_FILE;
	s->f = open_relative(src != NULL ? src->path : NULL, name, &path);
	if (s->f == NULL) {
		if (loc != NULL) {
			error_at(loc, "cannot open '%s'", name);
		} else {
			fprintf(stderr, "mas: cannot open '%s'\n", name);
			error_count++;
		}
		free(s);
		return false;
	}
	s->path = path;
	s->if_depth = cond_depth;
	push_source(s);
	return true;
}

static void d_symbols(struct lexer *lx, const struct loc *loc, bool export)
{
	do {
		struct token *t = lex_peek(lx);
		struct symbol *s;

		if (t->kind != TOK_IDENT) {
			struct loc l = lex_loc(lx);

			error_at(&l, "expected a symbol name");
			return;
		}
		s = sym_lookup(t->text);
		if (export == true) {
			s->exported = true;
		} else {
			if (s->kind != SYM_UNDEFINED) {
				error_at(loc, "'%s' is defined and cannot be imported",
					 s->name);
			}
			s->imported = true;
		}
		lex_next(lx);
	} while (lex_accept_punct(lx, ',') == true);
	lex_expect_eol(lx);
}

static void d_area(struct lexer *lx, const struct loc *loc)
{
	struct token *t = lex_peek(lx);
	struct section *s;
	char *name;

	if (t->kind == TOK_PUNCT && t->punct == '|') {
		const char *p = lx->line + lx->pos;
		const char *end = strchr(p, '|');

		if (end == NULL) {
			error_at(loc, "unterminated |name|");
			return;
		}
		name = xstrndup(p, (size_t)(end - p));
		lx->pos = (size_t)(end + 1 - lx->line);
		lx->have_tok = false;
	} else if (t->kind == TOK_IDENT) {
		name = xstrdup(t->text);
		lex_next(lx);
	} else {
		error_at(loc, "AREA needs a name");
		return;
	}
	s = sec_lookup(name, true);
	free(name);
	while (lex_accept_punct(lx, ',') == true) {
		t = lex_peek(lx);
		if (t->kind != TOK_IDENT) {
			struct loc l = lex_loc(lx);

			error_at(&l, "expected an AREA attribute");
			return;
		}
		if (ieq(t->text, "CODE") == true) {
			s->kind = SEC_CODE;
		} else if (ieq(t->text, "DATA") == true) {
			s->kind = SEC_DATA;
			s->readonly = false;
		} else if (ieq(t->text, "BSS") == true ||
			   ieq(t->text, "NOINIT") == true) {
			s->kind = SEC_BSS;
			s->readonly = false;
		} else if (ieq(t->text, "READONLY") == true) {
			s->readonly = true;
		} else if (ieq(t->text, "READWRITE") == true) {
			s->readonly = false;
		} else if (ieq(t->text, "ALIGN") == true) {
			int64_t n;

			lex_next(lx);
			if (lex_expect_punct(lx, '=') == false ||
			    expr_eval_abs(expr_parse(lx), NULL, true, &n) == false) {
				return;
			}
			if (n < 2 || (n & (n - 1)) != 0) {
				error_at(loc, "AREA alignment must be a power of two");
				return;
			}
			s->align = (unsigned)n;
			continue;
		} else {
			struct loc l = lex_loc(lx);

			error_at(&l, "unknown AREA attribute '%s'", t->text);
			return;
		}
		lex_next(lx);
	}
	if (lex_expect_eol(lx) == true) {
		sec_select(s);
	}
}

/* ---- macros ----------------------------------------------------------- */

static struct macro *macro_find(const char *name)
{
	struct macro *m;

	for (m = macros; m != NULL; m = m->next) {
		if (ieq(m->name, name) == true) {
			return m;
		}
	}
	return NULL;
}

static struct macro *defining;
static char **capture;
static unsigned capture_n;
static unsigned capture_cap;
static unsigned capture_nesting;	/* nested WHILE depth while capturing */
static enum { CAP_NONE, CAP_MACRO, CAP_WHILE } capturing;
static struct expr *while_cond;
static struct loc while_loc;
static struct loc capture_loc;

static void capture_line(const char *text)
{
	if (capture_n == capture_cap) {
		capture_cap = capture_cap == 0 ? 32 : capture_cap * 2;
		capture = xrealloc(capture, capture_cap * sizeof *capture);
	}
	capture[capture_n++] = xstrdup(text);
}

static void d_macro(struct lexer *lx, const struct loc *loc)
{
	struct token *t = lex_peek(lx);
	struct macro *m;

	if (t->kind != TOK_IDENT) {
		error_at(loc, "MACRO needs a name");
		return;
	}
	if (macro_find(t->text) != NULL) {
		error_at(loc, "macro '%s' already defined", t->text);
	}
	m = xcalloc(1, sizeof *m);
	m->name = xstrdup(t->text);
	m->loc = *loc;
	lex_next(lx);
	m->params = xmalloc(32 * sizeof *m->params);
	m->defaults = xcalloc(32, sizeof *m->defaults);
	while (lex_at_eol(lx) == false) {
		if (m->nparams > 0 && lex_expect_punct(lx, ',') == false) {
			return;
		}
		t = lex_peek(lx);
		if (t->kind != TOK_IDENT || t->text[0] != '$' || m->nparams == 32) {
			struct loc l = lex_loc(lx);

			error_at(&l, "macro parameters are $names");
			return;
		}
		m->params[m->nparams] = xstrdup(t->text);
		lex_next(lx);
		if (lex_accept_punct(lx, '=') == true) {
			/* default: raw text up to the next comma or comment */
			const char *p = lx->line + lx->pos;
			const char *end = p;

			while (*p == ' ' || *p == '\t') {
				p++;
			}
			end = p;
			while (*end != '\0' && *end != ',' && *end != ';') {
				end++;
			}
			m->defaults[m->nparams] = xstrndup(p, (size_t)(end - p));
			lx->pos = (size_t)(end - lx->line);
			lx->have_tok = false;
		}
		m->nparams++;
	}
	defining = m;
	capturing = CAP_MACRO;
	capture_n = 0;
	capture_loc = *loc;
}

static void finish_macro(void)
{
	struct macro *m = defining;

	m->lines = xmalloc(capture_n * sizeof *m->lines);
	memcpy(m->lines, capture, capture_n * sizeof *m->lines);
	m->nlines = capture_n;
	m->next = macros;
	macros = m;
	defining = NULL;
	capturing = CAP_NONE;
}

/* Substitute $param (optionally terminated by a dot) and \@ in a line. */
static char *substitute(const char *line, struct macro *m, char **args,
			unsigned id)
{
	size_t cap = strlen(line) * 2 + 64;
	char *out = xmalloc(cap);
	size_t n = 0;
	const char *p = line;

	while (*p != '\0') {
		const char *rep = NULL;
		size_t skip = 0;
		size_t rl;

		if (*p == '$') {
			unsigned i;
			size_t best = 0;

			for (i = 0; i < m->nparams; i++) {
				size_t pl = strlen(m->params[i]);

				if (strncmp(p, m->params[i], pl) == 0 && pl > best) {
					best = pl;
					rep = args[i];
				}
			}
			if (rep != NULL) {
				skip = best;
				if (p[skip] == '.') {
					skip++;
				}
			}
		} else if (p[0] == '\\' && p[1] == '@') {
			static char idbuf[16];

			snprintf(idbuf, sizeof idbuf, "%u", id);
			rep = idbuf;
			skip = 2;
		}
		if (rep == NULL) {
			if (n + 2 > cap) {
				cap *= 2;
				out = xrealloc(out, cap);
			}
			out[n++] = *p++;
			continue;
		}
		rl = strlen(rep);
		if (n + rl + 1 > cap) {
			cap = (n + rl + 1) * 2;
			out = xrealloc(out, cap);
		}
		memcpy(out + n, rep, rl);
		n += rl;
		p += skip;
	}
	out[n] = '\0';
	return out;
}

static unsigned macro_id;

static void invoke_macro(struct macro *m, struct lexer *lx,
			 const struct loc *loc)
{
	char *args[32];
	unsigned n = 0;
	unsigned i;
	struct source *s;

	/* arguments are raw text separated by commas */
	while (lex_at_eol(lx) == false) {
		const char *p = lx->line + lex_peek(lx)->col - 1;
		const char *end = p;
		int depth = 0;

		while (*end != '\0' && *end != ';' &&
		       (*end != ',' || depth > 0)) {
			if (*end == '(' || *end == '[' || *end == '{') {
				depth++;
			} else if (*end == ')' || *end == ']' || *end == '}') {
				depth--;
			} else if (*end == '"') {
				end++;
				while (*end != '\0' && *end != '"') {
					end++;
				}
				if (*end == '\0') {
					break;
				}
			}
			end++;
		}
		{
			const char *e = end;

			while (e > p && (e[-1] == ' ' || e[-1] == '\t')) {
				e--;
			}
			if (n < 32) {
				args[n] = xstrndup(p, (size_t)(e - p));
			}
			n++;
		}
		lx->pos = (size_t)(end - lx->line);
		lx->have_tok = false;
		if (lex_accept_punct(lx, ',') == false) {
			break;
		}
	}
	if (n > m->nparams) {
		error_at(loc, "macro '%s' takes %u argument%s, %u given",
			 m->name, m->nparams, m->nparams == 1 ? "" : "s", n);
		return;
	}
	for (i = n; i < m->nparams; i++) {
		if (m->defaults[i] == NULL) {
			error_at(loc, "macro '%s' needs argument %s", m->name,
				 m->params[i]);
			return;
		}
		args[i] = xstrdup(m->defaults[i]);
	}
	s = xcalloc(1, sizeof *s);
	s->kind = SRC_MACRO;
	s->macro = m;
	s->lines = xmalloc((m->nlines + 1) * sizeof *s->lines);
	macro_id++;
	for (i = 0; i < m->nlines; i++) {
		s->lines[i] = substitute(m->lines[i], m, args, macro_id);
	}
	for (i = 0; i < m->nparams; i++) {
		free(args[i]);
	}
	s->nlines = m->nlines;
	s->loc = m->loc;
	s->path = m->loc.file != NULL ? xstrdup(m->loc.file) : NULL;
	s->invoke_loc = *loc;
	s->if_depth = cond_depth;
	s->saved_expansion_name = expansion_name;
	s->saved_expansion_loc = expansion_loc;
	expansion_name = m->name;
	expansion_loc = *loc;
	push_macro_scope();
	push_source(s);
}

static void start_while(struct source *up)
{
	struct source *s;
	unsigned i;
	int64_t v = 0;

	(void)up;
	capturing = CAP_NONE;
	if (expr_eval_abs(while_cond, NULL, true, &v) == false || v == 0) {
		capture_n = 0;
		return;
	}
	s = xcalloc(1, sizeof *s);
	s->kind = SRC_WHILE;
	s->lines = xmalloc((capture_n + 1) * sizeof *s->lines);
	for (i = 0; i < capture_n; i++) {
		s->lines[i] = capture[i];
	}
	s->nlines = capture_n;
	s->cond = while_cond;
	s->loc = while_loc;
	s->path = while_loc.file != NULL ? xstrdup(while_loc.file) : NULL;
	s->invoke_loc = while_loc;
	s->if_depth = cond_depth;
	capture_n = 0;
	push_source(s);
}

/* ---- conditionals ----------------------------------------------------- */

static void d_if(struct lexer *lx, const struct loc *loc)
{
	struct cond_state *c;
	int64_t v = 0;
	bool parent = assembling();

	if (cond_depth == sizeof cond_stack / sizeof cond_stack[0]) {
		error_at(loc, "IF nested too deeply");
		return;
	}
	c = &cond_stack[cond_depth++];
	c->parent_active = parent;
	c->seen_else = false;
	c->loc = *loc;
	if (parent == true) {
		if (expr_eval_abs(expr_parse(lx), NULL, true, &v) == false) {
			v = 0;
		}
		lex_expect_eol(lx);
	}
	c->active = parent == true && v != 0;
	c->taken = c->active;
}

static void d_else(struct lexer *lx, const struct loc *loc)
{
	struct cond_state *c;

	if (cond_depth == 0 || cond_depth == src->if_depth) {
		error_at(loc, "ELSE without IF");
		return;
	}
	c = &cond_stack[cond_depth - 1];
	if (c->seen_else == true) {
		error_at(loc, "second ELSE for IF at line %d", c->loc.line);
	}
	c->seen_else = true;
	if (lex_is_ident(lx, "IF") == true) {
		int64_t v = 0;

		lex_next(lx);
		if (c->parent_active == true && c->taken == false) {
			if (expr_eval_abs(expr_parse(lx), NULL, true, &v) == false) {
				v = 0;
			}
			c->active = v != 0;
			c->taken = c->active;
		} else {
			c->active = false;
		}
		c->seen_else = false;
		return;
	}
	c->active = c->parent_active == true && c->taken == false;
	c->taken = true;
	lex_expect_eol(lx);
}

static void d_endif(struct lexer *lx, const struct loc *loc)
{
	if (cond_depth == 0 || cond_depth == src->if_depth) {
		error_at(loc, "ENDIF without IF");
		return;
	}
	cond_depth--;
	lex_expect_eol(lx);
}

/* ---- directive dispatch ----------------------------------------------- */

static void d_equ(struct lexer *lx, const struct loc *loc, const char *label)
{
	struct symbol *s;

	if (label == NULL) {
		error_at(loc, "EQU needs a label");
		return;
	}
	s = sym_lookup(label);
	if (define_symbol(s, loc) == false) {
		return;
	}
	s->kind = SYM_EQU;
	s->expr = expr_parse(lx);
	if (s->expr == NULL) {
		s->kind = SYM_UNDEFINED;
		return;
	}
	lex_expect_eol(lx);
}

/* name SET expr: an absolute variable, evaluated now, redefinable. */
static void d_set(struct lexer *lx, const struct loc *loc, const char *label)
{
	struct symbol *s;
	int64_t v;

	if (label == NULL) {
		error_at(loc, "SET needs a label");
		return;
	}
	s = sym_lookup(label);
	if (s->kind != SYM_UNDEFINED && s->settable == false) {
		error_at(loc, "'%s' is not a SET variable", s->name);
		return;
	}
	if (expr_eval_abs(expr_parse(lx), NULL, true, &v) == false) {
		return;
	}
	s->kind = SYM_EQU;
	s->settable = true;
	s->def = *loc;
	s->expr = expr_const(v);
	lex_expect_eol(lx);
}

static void d_rn(struct lexer *lx, const struct loc *loc, const char *label)
{
	struct symbol *s;
	struct token *t = lex_peek(lx);
	int r;
	bool alt;

	if (label == NULL) {
		error_at(loc, "RN needs a label");
		return;
	}
	if (t->kind != TOK_IDENT || (r = meow_reg_parse(t->text, &alt)) < 0) {
		struct symbol *alias = t->kind == TOK_IDENT ? sym_find(t->text) : NULL;

		if (alias == NULL || alias->kind != SYM_REGISTER) {
			struct loc l = lex_loc(lx);

			error_at(&l, "RN needs a register");
			return;
		}
		r = (int)alias->reg;
		alt = alias->alt;
	}
	lex_next(lx);
	s = sym_lookup(label);
	if (define_symbol(s, loc) == false) {
		return;
	}
	s->kind = SYM_REGISTER;
	s->reg = (unsigned)r;
	s->alt = alt;
	lex_expect_eol(lx);
}

static void d_entry(struct lexer *lx, const struct loc *loc)
{
	struct token *t = lex_peek(lx);

	if (entry_symbol != NULL) {
		error_at(loc, "ENTRY already given");
	}
	if (t->kind == TOK_IDENT) {
		entry_symbol = sym_lookup(t->text);
		lex_next(lx);
	} else {
		entry_symbol = sym_lookup("__entry");
		if (define_symbol(entry_symbol, loc) == true) {
			entry_symbol->kind = SYM_LABEL;
			entry_symbol->anchor = anchor_here(loc);
			entry_symbol->sec = cur_sec;
		}
	}
	lex_expect_eol(lx);
}

static void d_message(struct lexer *lx, const struct loc *loc, bool is_error)
{
	struct token *t = lex_peek(lx);
	const char *msg = t->kind == TOK_STRING ? t->text : "";

	if (is_error == true) {
		error_at(loc, "%s", msg);
	} else {
		fprintf(stderr, "%s:%d: %s\n", loc->file, loc->line, msg);
	}
	lex_next(lx);
}

static void d_assert(struct lexer *lx, const struct loc *loc)
{
	int64_t v;

	if (expr_eval_abs(expr_parse(lx), NULL, true, &v) == true && v == 0) {
		error_at(loc, "assertion failed");
	}
	lex_expect_eol(lx);
}

static void end_source(void);

/* Returns true if the word was a directive. */
static bool directive(const char *word, struct lexer *lx,
		      const struct loc *loc, const char *label)
{
	static const struct {
		const char *name;
		int id;
	} table[] = {
		{ "AREA", 1 }, { "EQU", 2 }, { "*", 2 }, { "RN", 3 },
		{ "DCB", 4 }, { "DCW", 5 }, { "DCD", 6 }, { "SPACE", 7 },
		{ "%", 7 }, { "ALIGN", 8 }, { "INCBIN", 9 }, { "GET", 10 },
		{ "INCLUDE", 10 }, { "EXPORT", 11 }, { "GLOBAL", 11 },
		{ "IMPORT", 12 }, { "EXTERN", 12 }, { "END", 13 },
		{ "ENTRY", 14 }, { "LTORG", 15 }, { "MACRO", 16 },
		{ "MEND", 17 }, { "ENDMACRO", 17 }, { "IF", 18 }, { "[", 18 },
		{ "ELSE", 19 }, { "|", 19 }, { "ENDIF", 20 }, { "]", 20 },
		{ "WHILE", 21 }, { "WEND", 22 }, { "ASSERT", 23 },
		{ "INFO", 24 }, { "ERROR", 25 }, { "SET", 26 },
	};
	size_t i;
	int id = 0;

	for (i = 0; i < sizeof table / sizeof table[0]; i++) {
		if (ieq(word, table[i].name) == true) {
			id = table[i].id;
		}
	}
	if (id == 0) {
		return false;
	}
	lex_next(lx);
	switch (id) {
	case 1: d_area(lx, loc); break;
	case 2: d_equ(lx, loc, label); break;
	case 3: d_rn(lx, loc, label); break;
	case 4: d_data(lx, loc, 1); break;
	case 5: d_data(lx, loc, 2); break;
	case 6: d_data(lx, loc, 4); break;
	case 7: d_space(lx, loc); break;
	case 8: d_align(lx, loc); break;
	case 9: d_incbin(lx, loc); break;
	case 10: {
		char *name = take_filename(lx);

		if (name != NULL) {
			push_file(name, loc);
			free(name);
		}
		break;
	}
	case 11: d_symbols(lx, loc, true); break;
	case 12: d_symbols(lx, loc, false); break;
	case 13:
		while (src != NULL && src->kind != SRC_FILE) {
			end_source();
		}
		if (src != NULL) {
			end_source();
		}
		break;
	case 14: d_entry(lx, loc); break;
	case 15: literal_pool_flush(loc, false); break;
	case 16: d_macro(lx, loc); break;
	case 17: error_at(loc, "MEND without MACRO"); break;
	case 18: d_if(lx, loc); break;
	case 19: d_else(lx, loc); break;
	case 20: d_endif(lx, loc); break;
	case 21:
		expr_lazy_set = true;
		while_cond = expr_parse(lx);
		expr_lazy_set = false;
		if (while_cond == NULL) {
			break;
		}
		while_loc = *loc;
		capturing = CAP_WHILE;
		capture_n = 0;
		capture_nesting = 0;
		capture_loc = *loc;
		break;
	case 22: error_at(loc, "WEND without WHILE"); break;
	case 23: d_assert(lx, loc); break;
	case 24: d_message(lx, loc, false); break;
	case 25: d_message(lx, loc, true); break;
	case 26: d_set(lx, loc, label); break;
	}
	return true;
}

/* ---- lines ------------------------------------------------------------ */

static bool first_word(const char *line, char *buf, size_t n)
{
	const char *p = line;
	size_t i = 0;

	while (*p == ' ' || *p == '\t') {
		p++;
	}
	while (*p != '\0' && !isspace((unsigned char)*p) && i + 1 < n) {
		buf[i++] = *p++;
	}
	buf[i] = '\0';
	return i > 0;
}

/* Capture mode: collecting a MACRO or WHILE body. */
static void capture_mode(const char *line, const struct loc *loc)
{
	char word[64];
	char rest[64];
	const char *p;

	first_word(line, word, sizeof word);
	/* a label in column 1 followed by the terminator also counts */
	p = line;
	while (*p != '\0' && !isspace((unsigned char)*p)) {
		p++;
	}
	first_word(p, rest, sizeof rest);
	if (capturing == CAP_MACRO) {
		if (ieq(word, "MEND") == true || ieq(word, "ENDMACRO") == true ||
		    ieq(rest, "MEND") == true || ieq(rest, "ENDMACRO") == true) {
			finish_macro();
			return;
		}
		if (ieq(word, "MACRO") == true) {
			error_at(loc, "MACRO inside MACRO");
		}
	} else {
		if (ieq(word, "WHILE") == true) {
			capture_nesting++;
		} else if (ieq(word, "WEND") == true) {
			if (capture_nesting == 0) {
				start_while(src);
				return;
			}
			capture_nesting--;
		}
	}
	capture_line(line);
}

static void process_line(const char *text, const struct loc *loc)
{
	struct lexer lx;
	char *label = NULL;
	struct token *t;
	struct loc lloc = *loc;

	if (capturing != CAP_NONE) {
		capture_mode(text, loc);
		return;
	}
	add_listing_line(text, loc);
	lex_init(&lx, text, loc);
	t = lex_peek(&lx);
	if (t->kind == TOK_EOL) {
		return;
	}
	/* label: column 1, or any identifier followed by a colon */
	if (t->kind == TOK_IDENT && t->col == 1) {
		label = xstrdup(t->text);
		lex_next(&lx);
		lex_accept_punct(&lx, ':');
	} else if (t->kind == TOK_IDENT) {
		const char *p = text + lx.pos;

		while (*p == ' ' || *p == '\t') {
			p++;
		}
		if (*p == ':') {
			label = xstrdup(t->text);
			lex_next(&lx);
			lex_accept_punct(&lx, ':');
		}
	}
	t = lex_peek(&lx);
	if (t->kind == TOK_IDENT || (t->kind == TOK_PUNCT &&
	    (t->punct == '[' || t->punct == '|' || t->punct == ']' ||
	     t->punct == '*' || t->punct == '%'))) {
		char word[64];
		bool is_cond;

		if (t->kind == TOK_IDENT) {
			snprintf(word, sizeof word, "%s", t->text);
		} else {
			word[0] = (char)t->punct;
			word[1] = '\0';
		}
		is_cond = ieq(word, "IF") == true || ieq(word, "[") == true ||
			  ieq(word, "ELSE") == true || ieq(word, "|") == true ||
			  ieq(word, "ENDIF") == true || ieq(word, "]") == true;
		if (assembling() == false) {
			if (is_cond == true) {
				directive(word, &lx, loc, label);
			}
			free(label);
			return;
		}
		if (ieq(word, "EQU") == true || ieq(word, "*") == true ||
		    ieq(word, "RN") == true || ieq(word, "SET") == true) {
			directive(word, &lx, loc, label);
			free(label);
			return;
		}
		if (label != NULL) {
			define_label(label, &lloc);
		}
		if (directive(word, &lx, loc, label) == true) {
			free(label);
			return;
		}
		if (macro_find(word) != NULL) {
			lex_next(&lx);
			invoke_macro(macro_find(word), &lx, loc);
			free(label);
			return;
		}
		if (parse_instruction(&lx, loc) == true) {
			free(label);
			return;
		}
		{
			struct loc l = lex_loc(&lx);

			error_at(&l, "unknown mnemonic or directive '%s'", word);
		}
		free(label);
		return;
	}
	if (assembling() == false) {
		free(label);
		return;
	}
	if (label != NULL) {
		define_label(label, &lloc);
		free(label);
	}
	lex_expect_eol(&lx);
}

static void end_source(void)
{
	struct source *s = src;

	if (cond_depth != s->if_depth) {
		error_at(&cond_stack[cond_depth - 1].loc, "IF without ENDIF");
		cond_depth = s->if_depth;
	}
	if (capturing == CAP_MACRO && s->kind == SRC_FILE) {
		error_at(&capture_loc, "MACRO without MEND");
		capturing = CAP_NONE;
	}
	if (capturing == CAP_WHILE && s->kind == SRC_FILE) {
		error_at(&capture_loc, "WHILE without WEND");
		capturing = CAP_NONE;
	}
	src = s->up;
	source_depth--;
	if (s->kind == SRC_FILE) {
		fclose(s->f);
	} else if (s->kind == SRC_MACRO) {
		expansion_name = s->saved_expansion_name;
		expansion_loc = s->saved_expansion_loc;
		pop_macro_scope();
	}
	free(s);
}

/* Fetch the next line from the source stack; false at the end. */
static bool next_line(char **text, struct loc *loc)
{
	static char buf[4096];

	while (src != NULL) {
		struct source *s = src;

		if (s->kind == SRC_FILE) {
			if (fgets(buf, sizeof buf, s->f) == NULL) {
				end_source();
				continue;
			}
			s->line++;
			buf[strcspn(buf, "\r\n")] = '\0';
			*text = buf;
			loc->file = s->path;
			loc->line = s->line;
			loc->col = 0;
			return true;
		}
		if (s->next == s->nlines) {
			if (s->kind == SRC_WHILE) {
				int64_t v = 0;

				s->iterations++;
				if (s->iterations > 100000) {
					error_at(&s->invoke_loc, "WHILE ran 100000 times");
				} else if (expr_eval_abs(s->cond, NULL, true, &v) == true &&
					   v != 0) {
					s->next = 0;
					continue;
				}
			}
			end_source();
			continue;
		}
		*text = s->lines[s->next];
		loc->file = s->path;
		loc->line = s->loc.line + (int)s->next + 1;
		loc->col = 0;
		s->next++;
		return true;
	}
	return false;
}

bool assemble_file(const char *path)
{
	char *text;
	struct loc loc;

	if (push_file(path, NULL) == false) {
		return false;
	}
	while (next_line(&text, &loc) == true) {
		process_line(text, &loc);
	}
	return true;
}

void assemble_define(const char *assignment)
{
	const char *eq = strchr(assignment, '=');
	char *name;
	struct lexer lx;
	struct loc loc = { "<command line>", 0, 0 };
	struct symbol *s;

	name = eq != NULL ? xstrndup(assignment, (size_t)(eq - assignment))
			  : xstrdup(assignment);
	s = sym_lookup(name);
	if (define_symbol(s, &loc) == false) {
		return;
	}
	s->kind = SYM_EQU;
	lex_init(&lx, eq != NULL ? eq + 1 : "1", &loc);
	s->expr = expr_parse(&lx);
	if (s->expr == NULL) {
		s->kind = SYM_UNDEFINED;
	}
	free(name);
}

/* ---- layout and emission ---------------------------------------------- */

static void flush_all_pools(void)
{
	struct section *s;
	struct loc loc = { "<end of input>", 0, 0 };

	for (s = sec_first(); s != NULL; s = s->next) {
		sec_select(s);
		literal_pool_flush(&loc, true);
	}
}

static uint32_t item_size(struct item *it)
{
	switch (it->kind) {
	case ITEM_BYTES:
		return (uint32_t)it->u.bytes.n;
	case ITEM_DATA:
		return it->u.data.width * it->u.data.n;
	case ITEM_ALIGN:
		return (0u - it->addr) & (it->u.align.align - 1);
	case ITEM_SPACE: {
		int64_t n;

		if (expr_eval_abs(it->u.space.count, it, layout_pass == 2, &n) == false) {
			n = 0;
		}
		if (n < 0) {
			error_at(&it->loc, "negative SPACE");
			n = 0;
		}
		return (uint32_t)n;
	}
	case ITEM_INSTR:
		return instr_size(it);
	case ITEM_POOL:
		return it->u.pool.n * 4;
	case ITEM_ANCHOR:
		return 0;
	}
	return 0;
}

void layout(void)
{
	bool changed = true;

	flush_all_pools();
	while (changed == true) {
		struct section *s;

		layout_pass++;
		if (layout_pass > 100) {
			fatal("layout did not converge");
		}
		changed = false;
		for (s = sec_first(); s != NULL; s = s->next) {
			uint32_t addr = 0;
			struct item *it;

			for (it = s->items; it != NULL; it = it->next) {
				uint32_t size;

				it->addr = addr;
				it->pass = layout_pass;
				size = item_size(it);
				if (size != it->size) {
					changed = true;
					it->size = size;
				}
				addr += size;
			}
			s->size = addr;
		}
	}
	layout_done = true;
}

static void add_reloc(struct section *s, uint32_t offset, unsigned type,
		      const struct value *v, const struct loc *loc)
{
	struct reloc *r = xcalloc(1, sizeof *r);

	r->offset = offset;
	r->type = type;
	r->sym = v->ext;
	r->sec = v->sec;
	r->addend = v->v;
	r->loc = *loc;
	*s->reloc_tail = r;
	s->reloc_tail = &r->next;
}

static void store_le(uint8_t *p, uint64_t v, unsigned width)
{
	unsigned i;

	for (i = 0; i < width; i++) {
		p[i] = (uint8_t)(v >> (8 * i));
	}
}

static void emit_value(struct section *s, uint32_t offset, unsigned width,
		       struct expr *e, struct item *it)
{
	struct value v;
	static const unsigned types[5] = { 0, R_MEOW_ABS8, R_MEOW_ABS16, 0,
					   R_MEOW_ABS32 };

	if (expr_eval(e, it, true, &v) == false) {
		return;
	}
	if (value_is_abs(&v) == true) {
		int64_t lim = (int64_t)1 << (width * 8);

		if (v.v < -lim / 2 || v.v >= lim) {
			error_at(expr_loc(e), "value %lld does not fit in %u byte%s",
				 (long long)v.v, width, width == 1 ? "" : "s");
		}
		store_le(s->data + offset, (uint64_t)v.v, width);
		return;
	}
	add_reloc(s, offset, types[width], &v, expr_loc(e));
}

void emit(void)
{
	struct section *s;

	for (s = sec_first(); s != NULL; s = s->next) {
		struct item *it;

		s->data = xcalloc(s->size, 1);
		for (it = s->items; it != NULL; it = it->next) {
			uint8_t *p = s->data + it->addr;

			if (s->kind == SEC_BSS && it->kind != ITEM_SPACE &&
			    it->kind != ITEM_ALIGN && it->kind != ITEM_ANCHOR) {
				error_at(&it->loc, "data in a BSS area");
				continue;
			}
			switch (it->kind) {
			case ITEM_BYTES:
				memcpy(p, it->u.bytes.data, it->u.bytes.n);
				break;
			case ITEM_DATA: {
				unsigned i;

				for (i = 0; i < it->u.data.n; i++) {
					emit_value(s, it->addr + i * it->u.data.width,
						   it->u.data.width,
						   it->u.data.exprs[i], it);
				}
				break;
			}
			case ITEM_ALIGN:
				memset(p, (int)it->u.align.fill, it->size);
				break;
			case ITEM_SPACE:
				memset(p, (int)it->u.space.fill, it->size);
				break;
			case ITEM_INSTR:
				instr_emit(it, p);
				break;
			case ITEM_POOL: {
				struct literal *l;

				for (l = it->u.pool.lits; l != NULL; l = l->next) {
					emit_value(s, it->addr + l->offset, 4,
						   l->expr, it);
				}
				break;
			}
			case ITEM_ANCHOR:
				break;
			}
		}
	}
}
