#include <stdlib.h>
#include <string.h>

#include "mas.h"

int error_count;
const char *expansion_name;
struct loc expansion_loc;
int warning_count;
bool warnings_are_errors;

static void report(const struct loc *loc, const char *kind, const char *fmt,
		   va_list ap)
{
	if (loc != NULL && loc->file != NULL) {
		fprintf(stderr, "%s:%d:", loc->file, loc->line);
		if (loc->col > 0) {
			fprintf(stderr, "%d:", loc->col);
		}
		fputc(' ', stderr);
	} else {
		fputs("mas: ", stderr);
	}
	fprintf(stderr, "%s: ", kind);
	vfprintf(stderr, fmt, ap);
	fputc('\n', stderr);
	if (expansion_name != NULL) {
		fprintf(stderr, "%s:%d: note: in expansion of macro %s\n",
			expansion_loc.file, expansion_loc.line, expansion_name);
	}
}

void error_at(const struct loc *loc, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	report(loc, "error", fmt, ap);
	va_end(ap);
	error_count++;
	if (error_count >= 100) {
		fatal("too many errors");
	}
}

void warn_at(const struct loc *loc, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	report(loc, warnings_are_errors == true ? "error" : "warning", fmt, ap);
	va_end(ap);
	if (warnings_are_errors == true) {
		error_count++;
	} else {
		warning_count++;
	}
}

void fatal(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	report(NULL, "fatal", fmt, ap);
	va_end(ap);
	exit(2);
}

void *xmalloc(size_t n)
{
	void *p = malloc(n == 0 ? 1 : n);

	if (p == NULL) {
		fatal("out of memory");
	}
	return p;
}

void *xcalloc(size_t n, size_t size)
{
	void *p = calloc(n == 0 ? 1 : n, size == 0 ? 1 : size);

	if (p == NULL) {
		fatal("out of memory");
	}
	return p;
}

void *xrealloc(void *p, size_t n)
{
	p = realloc(p, n == 0 ? 1 : n);
	if (p == NULL) {
		fatal("out of memory");
	}
	return p;
}

char *xstrdup(const char *s)
{
	return xstrndup(s, strlen(s));
}

char *xstrndup(const char *s, size_t n)
{
	char *p = xmalloc(n + 1);

	memcpy(p, s, n);
	p[n] = '\0';
	return p;
}
