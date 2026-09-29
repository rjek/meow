#include <stdlib.h>
#include <string.h>

#include "mas.h"

#define HASH_SIZE 1024

static struct symbol *buckets[HASH_SIZE];
static struct symbol *sym_list;
static struct symbol **sym_list_tail = &sym_list;

static unsigned hash(const char *s)
{
	unsigned h = 5381;

	while (*s != '\0') {
		h = h * 33 + (unsigned char)*s++;
	}
	return h % HASH_SIZE;
}

struct symbol *sym_find(const char *name)
{
	struct symbol *s;

	for (s = buckets[hash(name)]; s != NULL; s = s->next) {
		if (strcmp(s->name, name) == 0) {
			return s;
		}
	}
	return NULL;
}

struct symbol *sym_lookup(const char *name)
{
	struct symbol *s = sym_find(name);
	unsigned h;

	if (s != NULL) {
		return s;
	}
	h = hash(name);
	s = xcalloc(1, sizeof *s);
	s->name = xstrdup(name);
	s->kind = SYM_UNDEFINED;
	s->next = buckets[h];
	buckets[h] = s;
	*sym_list_tail = s;
	sym_list_tail = &s->list;
	return s;
}

struct symbol *sym_first(void)
{
	return sym_list;
}

/* Local labels (.name) are qualified by the enclosing global label, and
 * additionally by a macro invocation counter when defined inside one. */
static char scope_name[256] = "";
static unsigned macro_scope[64];
static unsigned macro_depth;
static unsigned macro_counter;

void set_label_scope(const char *name)
{
	snprintf(scope_name, sizeof scope_name, "%s", name);
}

void push_macro_scope(void)
{
	if (macro_depth < sizeof macro_scope / sizeof macro_scope[0]) {
		macro_scope[macro_depth] = ++macro_counter;
	}
	macro_depth++;
}

void pop_macro_scope(void)
{
	macro_depth--;
}

const char *local_label_name(const char *name)
{
	static char buf[512];

	if (macro_depth > 0) {
		snprintf(buf, sizeof buf, "%s%s\001%u", scope_name, name,
			 macro_scope[macro_depth - 1]);
	} else {
		snprintf(buf, sizeof buf, "%s%s", scope_name, name);
	}
	return buf;
}

/* ---- sections --------------------------------------------------------- */

static struct section *sections;
static struct section **sections_tail = &sections;
struct section *cur_sec;
struct item *last_item;

struct section *sec_lookup(const char *name, bool create)
{
	struct section *s;

	for (s = sections; s != NULL; s = s->next) {
		if (strcmp(s->name, name) == 0) {
			return s;
		}
	}
	if (create == false) {
		return NULL;
	}
	s = xcalloc(1, sizeof *s);
	s->name = xstrdup(name);
	s->kind = SEC_CODE;
	s->readonly = true;
	s->align = 4;
	s->tail = &s->items;
	s->pending_tail = &s->pending;
	s->reloc_tail = &s->relocs;
	s->sym = sym_lookup(name);
	s->sym->kind = SYM_SECTION;
	s->sym->sec = s;
	*sections_tail = s;
	sections_tail = &s->next;
	return s;
}

struct section *sec_first(void)
{
	return sections;
}

void sec_select(struct section *s)
{
	cur_sec = s;
	last_item = NULL;
}

struct item *item_new(enum item_kind kind, const struct loc *loc)
{
	struct item *it = xcalloc(1, sizeof *it);

	it->kind = kind;
	it->loc = *loc;
	it->list_line = -1;
	return it;
}

void item_append(struct item *it)
{
	if (cur_sec == NULL) {
		sec_select(sec_lookup(".text", true));
	}
	it->sec = cur_sec;
	it->list_line = listing_count - 1;
	*cur_sec->tail = it;
	cur_sec->tail = &it->next;
	last_item = it;
}
