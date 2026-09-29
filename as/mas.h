/* mas: the MEOW assembler.  Internal interfaces. */
#ifndef MAS_H
#define MAS_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "meow.h"

/* ---- diagnostics ------------------------------------------------------ */

struct loc {
	const char *file;
	int line;
	int col;
};

extern int error_count;
extern int warning_count;
extern bool warnings_are_errors;

void error_at(const struct loc *loc, const char *fmt, ...);
extern const char *expansion_name;	/* macro being expanded, for notes */
extern struct loc expansion_loc;
void warn_at(const struct loc *loc, const char *fmt, ...);
void fatal(const char *fmt, ...);
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t size);
void *xrealloc(void *p, size_t n);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

/* ---- lexer ------------------------------------------------------------ */

enum tok_kind {
	TOK_EOL,
	TOK_IDENT,
	TOK_NUMBER,
	TOK_STRING,
	TOK_PUNCT
};

/* Multi-character punctuation gets its own code; single characters use
 * their own value. */
enum punct {
	P_SHL = 256, P_SHR, P_LE, P_GE, P_EQ, P_NE, P_LAND, P_LOR
};

struct token {
	enum tok_kind kind;
	int punct;
	uint32_t number;
	char *text;		/* identifier or decoded string, malloc'd */
	size_t len;		/* string length (may contain NUL) */
	int col;
};

struct lexer {
	const char *line;
	size_t pos;
	struct loc loc;
	struct token tok;
	bool have_tok;
};

void lex_init(struct lexer *lx, const char *line, const struct loc *loc);
struct token *lex_peek(struct lexer *lx);
void lex_next(struct lexer *lx);	/* consume the peeked token */
bool lex_is_punct(struct lexer *lx, int punct);
bool lex_accept_punct(struct lexer *lx, int punct);
bool lex_expect_punct(struct lexer *lx, int punct);
bool lex_is_ident(struct lexer *lx, const char *word);	/* case-insensitive */
bool lex_at_eol(struct lexer *lx);
bool lex_expect_eol(struct lexer *lx);
struct loc lex_loc(struct lexer *lx);
void lex_free_token(struct token *t);

bool ieq(const char *a, const char *b);

/* ---- symbols and sections --------------------------------------------- */

struct section;
struct item;
struct expr;

enum sym_kind {
	SYM_UNDEFINED,		/* referenced, never defined */
	SYM_LABEL,		/* address in a section */
	SYM_EQU,		/* expression */
	SYM_REGISTER,		/* RN */
	SYM_SECTION
};

struct symbol {
	char *name;
	enum sym_kind kind;
	struct section *sec;	/* label or section symbol */
	struct item *anchor;	/* label: the item it precedes */
	struct expr *expr;	/* EQU */
	bool evaluating;	/* EQU recursion guard */
	bool settable;		/* defined by SET, may be redefined */
	unsigned reg;
	bool alt;
	bool exported;
	bool imported;
	struct loc def;
	unsigned refs;
	int index;		/* ELF symbol index */
	struct symbol *next;	/* hash chain */
	struct symbol *list;	/* definition order */
};

struct symbol *sym_lookup(const char *name);	/* creates undefined */
struct symbol *sym_find(const char *name);
struct symbol *sym_first(void);			/* definition order */
const char *local_label_name(const char *name);	/* scope-qualified copy */
void set_label_scope(const char *name);
void push_macro_scope(void);
void pop_macro_scope(void);

/* A value: absolute, section-relative, or external symbol plus addend. */
struct value {
	int64_t v;
	struct section *sec;
	struct symbol *ext;
};

static inline bool value_is_abs(const struct value *v)
{
	return v->sec == NULL && v->ext == NULL;
}

enum sec_kind { SEC_CODE, SEC_DATA, SEC_BSS };

struct reloc {
	uint32_t offset;
	unsigned type;		/* R_MEOW_* */
	struct symbol *sym;	/* external, or NULL for sec */
	struct section *sec;
	int64_t addend;
	struct loc loc;
	struct reloc *next;
};

struct section {
	char *name;
	enum sec_kind kind;
	bool readonly;
	unsigned align;
	struct item *items;
	struct item **tail;
	struct literal *pending;	/* literals awaiting a pool */
	struct literal **pending_tail;
	uint32_t size;		/* after layout */
	uint32_t base;		/* flat output address */
	uint8_t *data;		/* after emit */
	struct reloc *relocs;
	struct reloc **reloc_tail;
	struct symbol *sym;
	int index;		/* ELF section index */
	struct section *next;
};

struct section *sec_lookup(const char *name, bool create);
struct section *sec_first(void);
extern struct section *cur_sec;
void sec_select(struct section *s);

/* ---- items ------------------------------------------------------------ */

enum item_kind {
	ITEM_BYTES,		/* literal data */
	ITEM_DATA,		/* DCB/DCW/DCD expressions */
	ITEM_ALIGN,
	ITEM_SPACE,
	ITEM_INSTR,
	ITEM_POOL,
	ITEM_ANCHOR		/* zero-size label anchor */
};

struct literal {
	struct expr *expr;
	struct item *pool;	/* NULL until placed */
	uint32_t offset;	/* within the pool */
	struct loc loc;
	struct literal *next;
};

/* Operand of an instruction, as parsed. */
struct operand {
	enum { OP_NONE, OP_REG, OP_IMM, OP_EXPR, OP_MEM, OP_REGLIST } kind;
	unsigned reg;
	bool alt;
	struct expr *expr;	/* immediate or target */
	int wb;			/* MEM: 0 none, 1 dec before, 2 dec after, 3 inc after */
	int64_t disp;		/* MEM: displacement as written */
	uint16_t reglist;
};

struct instr {
	const struct mnemonic *mn;
	int cond;		/* B, BL */
	unsigned flags;		/* mnemonic-specific: size, swap bits, ... */
	unsigned nops;
	struct operand ops[3];
	unsigned min_words;	/* relaxation state: never shrinks */
	struct literal *lit;	/* LDR =expr */
};

struct item {
	enum item_kind kind;
	struct loc loc;
	uint32_t addr;		/* section offset after layout */
	uint32_t size;
	unsigned pass;		/* layout pass that assigned addr */
	struct section *sec;
	int list_line;		/* listing: source line index, -1 if none */
	struct item *list_next;	/* next item from the same source line */
	union {
		struct {
			uint8_t *data;
			size_t n;
		} bytes;
		struct {
			unsigned width;
			struct expr **exprs;
			unsigned n;
		} data;
		struct {
			unsigned align;
			unsigned fill;
		} align;
		struct {
			struct expr *count;
			unsigned fill;
		} space;
		struct instr instr;
		struct {
			struct literal *lits;
			unsigned n;
		} pool;
	} u;
	struct item *next;
};

struct item *item_new(enum item_kind kind, const struct loc *loc);
extern unsigned layout_pass;		/* 0 before layout starts */
extern bool layout_done;
void item_append(struct item *it);	/* to cur_sec */
extern struct item *last_item;		/* for label anchoring */

/* ---- expressions ------------------------------------------------------ */

struct expr *expr_parse(struct lexer *lx);
extern bool expr_lazy_set;	/* do not snapshot SET variables */
/* Evaluate; here is the address the expression is evaluated at (for "."
 * and for relaxation), or NULL.  Reports errors when report is true.
 * Returns false if not evaluable. */
bool expr_eval(struct expr *e, const struct item *here, bool report,
	       struct value *out);
bool expr_eval_abs(struct expr *e, const struct item *here, bool report,
		   int64_t *out);
struct expr *expr_const(int64_t v);
struct expr *expr_symbol(const char *name, const struct loc *loc);
const struct loc *expr_loc(const struct expr *e);

/* ---- instructions ----------------------------------------------------- */

struct mnemonic {
	const char *name;
	int (*parse)(const struct mnemonic *mn, struct lexer *lx,
		     struct instr *in);
	unsigned arg;
};

const struct mnemonic *mnemonic_lookup(const char *word, int *cond,
				       unsigned *suffix);
bool parse_instruction(struct lexer *lx, const struct loc *loc);
uint32_t instr_size(struct item *it);		/* relaxation */
void instr_emit(struct item *it, uint8_t *out);	/* final encoding */
void instr_check_final(struct item *it);

/* ---- assembly driver -------------------------------------------------- */

struct source_line {
	char *text;
	struct loc loc;
};

extern struct source_line *listing_lines;
extern int listing_count;

bool assemble_file(const char *path);
void assemble_define(const char *assignment);	/* -D name=value */
void add_include_dir(const char *dir);
void layout(void);
void emit(void);
bool literal_pool_flush(const struct loc *loc, bool at_end);
struct literal *literal_add(struct expr *e, const struct loc *loc);

extern struct symbol *entry_symbol;

/* ---- output ----------------------------------------------------------- */

void write_flat(const char *path, uint32_t base);
void write_elf(const char *path);
void write_listing(FILE *f);
void write_map(FILE *f);

/* Relocation types. */
#define R_MEOW_NONE  0
#define R_MEOW_ABS32 1
#define R_MEOW_ABS16 2
#define R_MEOW_ABS8  3
#define R_MEOW_REL32 4

#endif /* MAS_H */
