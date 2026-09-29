/*
 * isagen: turns meow.isa into C tables and documentation fragments.
 *
 *   isagen -h out.h  meow.isa      C header
 *   isagen -c out.c  meow.isa      C tables
 *   isagen -d doc.md meow.isa      rewrite <!-- isa:NAME --> regions in place
 *   isagen -m        meow.isa      print all documentation fragments
 *   isagen -v        meow.isa      check and report reserved encoding space
 */

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ENCS 32
#define MAX_FIELDS 8
#define MAX_SYNTAX 6
#define MAX_LINE 256

struct field {
	char letter;
	char name[16];
	char kind[8];
	char desc[MAX_LINE];
	unsigned shift;
	unsigned width;
};

struct enc {
	char name[16];
	char bits[17];
	uint16_t mask;
	uint16_t match;
	unsigned nfields;
	struct field fields[MAX_FIELDS];
	unsigned nsyntax;
	char syntax[MAX_SYNTAX][MAX_LINE];
};

static struct enc encs[MAX_ENCS];
static unsigned nencs;
static const char *isa_path;

static void die(int line, const char *msg, const char *arg)
{
	if (line > 0) {
		fprintf(stderr, "%s:%d: %s%s\n", isa_path, line, msg, arg);
	} else {
		fprintf(stderr, "isagen: %s%s\n", msg, arg);
	}
	exit(1);
}

static void upper(char *dst, const char *src, size_t n)
{
	size_t i;

	for (i = 0; i + 1 < n && src[i] != '\0'; i++) {
		dst[i] = (char)toupper((unsigned char)src[i]);
	}
	dst[i] = '\0';
}

static struct field *find_field(struct enc *e, char letter)
{
	unsigned i;

	for (i = 0; i < e->nfields; i++) {
		if (e->fields[i].letter == letter) {
			return &e->fields[i];
		}
	}
	return NULL;
}

/* Derive mask, match and field positions from the bits string. */
static void resolve(struct enc *e, int line)
{
	unsigned i;
	unsigned f;

	for (i = 0; i < 16; i++) {
		unsigned bit = 15 - i;
		char c = e->bits[i];

		if (c == '0' || c == '1') {
			e->mask |= (uint16_t)(1u << bit);
			if (c == '1') {
				e->match |= (uint16_t)(1u << bit);
			}
		} else if (find_field(e, c) == NULL) {
			char s[2] = { c, '\0' };
			die(line, "bit letter with no field: ", s);
		}
	}
	for (f = 0; f < e->nfields; f++) {
		struct field *fl = &e->fields[f];
		int lo = -1;
		int hi = -1;

		for (i = 0; i < 16; i++) {
			if (e->bits[i] == fl->letter) {
				unsigned bit = 15 - i;
				if (hi < 0) {
					hi = (int)bit;
				}
				if (lo >= 0 && (unsigned)lo != bit + 1) {
					die(line, "field is not contiguous: ",
					    fl->name);
				}
				lo = (int)bit;
			}
		}
		if (lo < 0) {
			die(line, "field not used in bits: ", fl->name);
		}
		fl->shift = (unsigned)lo;
		fl->width = (unsigned)(hi - lo + 1);
	}
}

static void parse(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[MAX_LINE];
	int lineno = 0;
	struct enc *cur = NULL;

	if (f == NULL) {
		die(0, "cannot open ", path);
	}
	while (fgets(line, sizeof line, f) != NULL) {
		char *p = line;
		char *word;

		lineno++;
		line[strcspn(line, "\r\n")] = '\0';
		while (isspace((unsigned char)*p)) {
			p++;
		}
		if (*p == '\0' || *p == ';') {
			continue;
		}
		word = strtok(p, " \t");
		if (strcmp(word, "enc") == 0) {
			char *name = strtok(NULL, " \t");

			if (cur != NULL) {
				die(lineno, "enc inside enc", "");
			}
			if (name == NULL || nencs == MAX_ENCS) {
				die(lineno, "bad enc line", "");
			}
			cur = &encs[nencs++];
			memset(cur, 0, sizeof *cur);
			snprintf(cur->name, sizeof cur->name, "%s", name);
		} else if (cur == NULL) {
			die(lineno, "directive outside enc: ", word);
		} else if (strcmp(word, "bits") == 0) {
			char *tok;
			size_t n = 0;

			while ((tok = strtok(NULL, " \t")) != NULL) {
				size_t l = strlen(tok);
				if (n + l > 16) {
					die(lineno, "more than 16 bits", "");
				}
				memcpy(cur->bits + n, tok, l);
				n += l;
			}
			if (n != 16) {
				die(lineno, "fewer than 16 bits", "");
			}
			cur->bits[16] = '\0';
		} else if (strcmp(word, "field") == 0) {
			char *letter = strtok(NULL, " \t");
			char *name = strtok(NULL, " \t");
			char *kind = strtok(NULL, " \t");
			char *desc = strtok(NULL, "");
			struct field *fl;

			if (letter == NULL || name == NULL || kind == NULL ||
			    strlen(letter) != 1 ||
			    cur->nfields == MAX_FIELDS) {
				die(lineno, "bad field line", "");
			}
			fl = &cur->fields[cur->nfields++];
			fl->letter = letter[0];
			snprintf(fl->name, sizeof fl->name, "%s", name);
			snprintf(fl->kind, sizeof fl->kind, "%s", kind);
			if (desc != NULL) {
				while (isspace((unsigned char)*desc)) {
					desc++;
				}
				snprintf(fl->desc, sizeof fl->desc, "%s", desc);
			}
		} else if (strcmp(word, "syntax") == 0) {
			char *rest = strtok(NULL, "");

			if (rest == NULL || cur->nsyntax == MAX_SYNTAX) {
				die(lineno, "bad syntax line", "");
			}
			while (isspace((unsigned char)*rest)) {
				rest++;
			}
			snprintf(cur->syntax[cur->nsyntax++], MAX_LINE, "%s",
				 rest);
		} else if (strcmp(word, "end") == 0) {
			if (cur->bits[0] == '\0') {
				die(lineno, "enc has no bits: ", cur->name);
			}
			resolve(cur, lineno);
			cur = NULL;
		} else {
			die(lineno, "unknown directive: ", word);
		}
	}
	if (cur != NULL) {
		die(lineno, "missing end", "");
	}
	fclose(f);
}

static void check_overlap(void)
{
	unsigned i;
	unsigned j;

	for (i = 0; i < nencs; i++) {
		for (j = i + 1; j < nencs; j++) {
			uint16_t common = encs[i].mask & encs[j].mask;

			if ((encs[i].match & common) ==
			    (encs[j].match & common)) {
				fprintf(stderr,
					"isagen: %s and %s overlap\n",
					encs[i].name, encs[j].name);
				exit(1);
			}
		}
	}
}

static const struct enc *decode(uint16_t w)
{
	unsigned i;

	for (i = 0; i < nencs; i++) {
		if ((w & encs[i].mask) == encs[i].match) {
			return &encs[i];
		}
	}
	return NULL;
}

static void report(void)
{
	unsigned w;
	unsigned reserved = 0;
	unsigned counts[MAX_ENCS] = { 0 };
	unsigned i;

	for (w = 0; w < 65536; w++) {
		const struct enc *e = decode((uint16_t)w);

		if (e == NULL) {
			reserved++;
		} else {
			counts[e - encs]++;
		}
	}
	for (i = 0; i < nencs; i++) {
		printf("%-6s %s  mask %04x match %04x  %5u words\n",
		       encs[i].name, encs[i].bits, encs[i].mask,
		       encs[i].match, counts[i]);
	}
	printf("reserved: %u words\n", reserved);
}

static void gen_header(FILE *o)
{
	unsigned i;
	unsigned f;

	fputs("/* Generated by isagen from meow.isa.  Do not edit. */\n"
	      "#ifndef MEOW_ISA_H\n#define MEOW_ISA_H\n\n"
	      "#include <stdint.h>\n\n", o);
	fputs("enum meow_enc {\n", o);
	for (i = 0; i < nencs; i++) {
		fprintf(o, "\tMEOW_ENC_%s,\n", encs[i].name);
	}
	fputs("\tMEOW_ENC_COUNT,\n\tMEOW_ENC_NONE = MEOW_ENC_COUNT\n};\n\n", o);
	fputs("enum meow_field_kind {\n"
	      "\tMEOW_FK_REG,\n\tMEOW_FK_BANK,\n\tMEOW_FK_UIMM,\n"
	      "\tMEOW_FK_SIMM,\n\tMEOW_FK_COND,\n\tMEOW_FK_FLAG,\n"
	      "\tMEOW_FK_OP\n};\n\n"
	      "struct meow_field {\n"
	      "\tconst char *name;\n\tunsigned shift;\n\tunsigned width;\n"
	      "\tenum meow_field_kind kind;\n};\n\n"
	      "struct meow_enc_desc {\n"
	      "\tconst char *name;\n\tconst char *bits;\n"
	      "\tuint16_t mask;\n\tuint16_t match;\n"
	      "\tunsigned nfields;\n\tconst struct meow_field *fields;\n"
	      "};\n\n"
	      "extern const struct meow_enc_desc meow_encs[MEOW_ENC_COUNT];\n\n"
	      "enum meow_enc meow_enc_of(uint16_t word);\n\n"
	      "/* Sign-extend the low w bits of v. */\n"
	      "static inline int32_t meow_sext(uint32_t v, unsigned w)\n"
	      "{\n\tuint32_t m = 1u << (w - 1);\n\n"
	      "\treturn (int32_t)((v & (m + m - 1)) ^ m) - (int32_t)m;\n}\n\n",
	      o);
	for (i = 0; i < nencs; i++) {
		const struct enc *e = &encs[i];
		char en[16];

		upper(en, e->name, sizeof en);
		fprintf(o, "/* %s: %s */\n", e->name, e->bits);
		fprintf(o, "#define MEOW_%s_MASK 0x%04x\n", en, e->mask);
		fprintf(o, "#define MEOW_%s_MATCH 0x%04x\n", en, e->match);
		for (f = 0; f < e->nfields; f++) {
			const struct field *fl = &e->fields[f];
			char fn[16];
			unsigned fm = (1u << fl->width) - 1;

			upper(fn, fl->name, sizeof fn);
			fprintf(o, "#define MEOW_%s_%s_SHIFT %u\n", en, fn,
				fl->shift);
			fprintf(o, "#define MEOW_%s_%s_WIDTH %u\n", en, fn,
				fl->width);
			fprintf(o, "#define MEOW_%s_%s(w) (((w) >> %u) & 0x%x)\n",
				en, fn, fl->shift, fm);
			if (strcmp(fl->kind, "simm") == 0) {
				fprintf(o,
					"#define MEOW_%s_%s_S(w) "
					"meow_sext(MEOW_%s_%s(w), %u)\n",
					en, fn, en, fn, fl->width);
			}
		}
		fprintf(o, "#define MEOW_ENCODE_%s(", en);
		for (f = 0; f < e->nfields; f++) {
			fprintf(o, "%s%s", f > 0 ? ", " : "",
				e->fields[f].name);
		}
		fprintf(o, ") \\\n\t((uint16_t)(0x%04xu", e->match);
		for (f = 0; f < e->nfields; f++) {
			const struct field *fl = &e->fields[f];
			unsigned fm = (1u << fl->width) - 1;

			fprintf(o, " \\\n\t| (((uint32_t)(%s) & 0x%xu) << %u)",
				fl->name, fm, fl->shift);
		}
		fputs("))\n\n", o);
	}
	fputs("#endif /* MEOW_ISA_H */\n", o);
}

static const char *kind_enum(const char *kind)
{
	if (strcmp(kind, "reg") == 0) {
		return "MEOW_FK_REG";
	}
	if (strcmp(kind, "bank") == 0) {
		return "MEOW_FK_BANK";
	}
	if (strcmp(kind, "uimm") == 0) {
		return "MEOW_FK_UIMM";
	}
	if (strcmp(kind, "simm") == 0) {
		return "MEOW_FK_SIMM";
	}
	if (strcmp(kind, "cond") == 0) {
		return "MEOW_FK_COND";
	}
	if (strcmp(kind, "flag") == 0) {
		return "MEOW_FK_FLAG";
	}
	if (strcmp(kind, "op") == 0) {
		return "MEOW_FK_OP";
	}
	die(0, "unknown field kind: ", kind);
	return NULL;
}

static void gen_source(FILE *o)
{
	unsigned i;
	unsigned f;

	fputs("/* Generated by isagen from meow.isa.  Do not edit. */\n"
	      "#include \"meow_isa.h\"\n\n", o);
	for (i = 0; i < nencs; i++) {
		const struct enc *e = &encs[i];

		fprintf(o, "static const struct meow_field fields_%s[] = {\n",
			e->name);
		for (f = 0; f < e->nfields; f++) {
			const struct field *fl = &e->fields[f];

			fprintf(o, "\t{ \"%s\", %u, %u, %s },\n", fl->name,
				fl->shift, fl->width, kind_enum(fl->kind));
		}
		fputs("};\n\n", o);
	}
	fputs("const struct meow_enc_desc meow_encs[MEOW_ENC_COUNT] = {\n", o);
	for (i = 0; i < nencs; i++) {
		const struct enc *e = &encs[i];

		fprintf(o, "\t{ \"%s\", \"%s\", 0x%04x, 0x%04x, %u, fields_%s },\n",
			e->name, e->bits, e->mask, e->match, e->nfields,
			e->name);
	}
	fputs("};\n\n"
	      "enum meow_enc meow_enc_of(uint16_t word)\n{\n"
	      "\tunsigned i;\n\n"
	      "\tfor (i = 0; i < MEOW_ENC_COUNT; i++) {\n"
	      "\t\tif ((word & meow_encs[i].mask) == meow_encs[i].match) {\n"
	      "\t\t\treturn (enum meow_enc)i;\n"
	      "\t\t}\n\t}\n"
	      "\treturn MEOW_ENC_NONE;\n}\n", o);
}

/* Field label inside the box: the letter for one-bit fields, else the
 * name centred in the field's cells. */
static void box_label(char *out, size_t n, const struct field *fl)
{
	size_t cells = fl->width * 3 - 1;
	size_t len = strlen(fl->name);
	size_t pad;
	size_t i;

	if (cells + 1 > n) {
		cells = n - 1;
	}
	if (fl->width == 1) {
		out[0] = ' ';
		out[1] = fl->letter;
		out[2] = '\0';
		return;
	}
	if (len > cells) {
		len = cells;
	}
	pad = (cells - len) / 2;
	for (i = 0; i < cells; i++) {
		out[i] = (i >= pad && i < pad + len) ? fl->name[i - pad] : ' ';
	}
	out[cells] = '\0';
}

static void gen_doc_one(FILE *o, const struct enc *e)
{
	unsigned i;
	unsigned f;
	char label[64];

	fprintf(o, "```\n 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0\n");
	fputs("+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+\n|", o);
	for (i = 0; i < 16;) {
		char c = e->bits[i];

		if (c == '0' || c == '1') {
			fprintf(o, " %c|", c);
			i++;
		} else {
			const struct field *fl = find_field((struct enc *)e, c);

			box_label(label, sizeof label, fl);
			fprintf(o, "%s|", label);
			i += fl->width;
		}
	}
	fputs("\n+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+--+\n```\n\n",
	      o);
	fputs("| Bits | Field | Meaning |\n|---|---|---|\n", o);
	for (f = 0; f < e->nfields; f++) {
		const struct field *fl = &e->fields[f];

		if (fl->width == 1) {
			fprintf(o, "| %u (%c) | %s | %s |\n", fl->shift,
				fl->letter, fl->name, fl->desc);
		} else {
			fprintf(o, "| %u:%u | %s | %s |\n",
				fl->shift + fl->width - 1, fl->shift,
				fl->name, fl->desc);
		}
	}
	fputs("\n", o);
	if (e->nsyntax > 0) {
		fputs("Syntax:\n\n```\n", o);
		for (i = 0; i < e->nsyntax; i++) {
			fprintf(o, "%s\n", e->syntax[i]);
		}
		fputs("```\n", o);
	}
}

static void gen_doc_all(FILE *o)
{
	unsigned i;

	for (i = 0; i < nencs; i++) {
		fprintf(o, "## %s\n\n", encs[i].name);
		gen_doc_one(o, &encs[i]);
		fputs("\n", o);
	}
}

/* Rewrite <!-- isa:NAME --> ... <!-- /isa --> regions in a Markdown file. */
static void update_doc(const char *path)
{
	FILE *in = fopen(path, "r");
	char tmp[1024];
	FILE *out;
	char line[MAX_LINE];
	bool skipping = false;
	int lineno = 0;

	if (in == NULL) {
		die(0, "cannot open ", path);
	}
	snprintf(tmp, sizeof tmp, "%s.tmp", path);
	out = fopen(tmp, "w");
	if (out == NULL) {
		die(0, "cannot write ", tmp);
	}
	while (fgets(line, sizeof line, in) != NULL) {
		lineno++;
		if (strncmp(line, "<!-- isa:", 9) == 0) {
			char name[16];
			unsigned i;
			const struct enc *e = NULL;

			if (sscanf(line + 9, "%15s", name) != 1) {
				die(0, "bad isa marker in ", path);
			}
			name[strcspn(name, " -")] = '\0';
			for (i = 0; i < nencs; i++) {
				if (strcmp(encs[i].name, name) == 0) {
					e = &encs[i];
				}
			}
			if (e == NULL) {
				fprintf(stderr, "%s:%d: unknown encoding %s\n",
					path, lineno, name);
				exit(1);
			}
			fputs(line, out);
			gen_doc_one(out, e);
			skipping = true;
		} else if (strncmp(line, "<!-- /isa -->", 13) == 0) {
			skipping = false;
			fputs(line, out);
		} else if (skipping == false) {
			fputs(line, out);
		}
	}
	if (skipping == true) {
		die(0, "unterminated isa region in ", path);
	}
	fclose(in);
	fclose(out);
	if (rename(tmp, path) != 0) {
		die(0, "cannot replace ", path);
	}
}

int main(int argc, char *argv[])
{
	const char *mode;
	const char *out = NULL;
	FILE *o;

	if (argc < 3) {
		fputs("usage: isagen -h|-c|-d FILE meow.isa | isagen -m|-v meow.isa\n",
		      stderr);
		return 1;
	}
	mode = argv[1];
	if (strcmp(mode, "-m") == 0 || strcmp(mode, "-v") == 0) {
		isa_path = argv[2];
	} else {
		if (argc < 4) {
			die(0, "missing output file", "");
		}
		out = argv[2];
		isa_path = argv[3];
	}
	parse(isa_path);
	check_overlap();
	if (strcmp(mode, "-v") == 0) {
		report();
		return 0;
	}
	if (strcmp(mode, "-m") == 0) {
		gen_doc_all(stdout);
		return 0;
	}
	if (strcmp(mode, "-d") == 0) {
		update_doc(out);
		return 0;
	}
	o = fopen(out, "w");
	if (o == NULL) {
		die(0, "cannot write ", out);
	}
	if (strcmp(mode, "-h") == 0) {
		gen_header(o);
	} else if (strcmp(mode, "-c") == 0) {
		gen_source(o);
	} else {
		die(0, "unknown mode ", mode);
	}
	fclose(o);
	return 0;
}
