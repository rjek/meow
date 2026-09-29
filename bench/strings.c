#include "bench.h"

/* byte loops as the C library would write them */
static int len(const char *s) { const char *p = s; while (*p) p++; return p - s; }
static char *copy(char *d, const char *s) { char *r = d; while ((*d++ = *s++) != 0) ; return r; }
static int cmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a - *b; }
static char *find(const char *s, int c) { for (; *s; s++) if (*s == c) return (char *)s; return 0; }
static void *mcpy(void *d, const void *s, int n) { char *dp = d; const char *sp = s; while (n-- > 0) *dp++ = *sp++; return d; }
static void *mset(void *d, int c, int n) { char *dp = d; while (n-- > 0) *dp++ = (char)c; return d; }

static char text[600], work[600];
static const char *words[] = { "meow", "microcontroller", "assembler", "linker", "simulator", "compiler", "z" };

int main(void)
{
    unsigned sum = 0;
    int i, iter;

    text[0] = 0;
    for (i = 0; len(text) < 500; i++) { copy(text + len(text), words[i % 7]); copy(text + len(text), " "); }
    for (iter = 0; iter < 60; iter++) {
        copy(work, text);
        sum += len(work);
        sum += cmp(work, text) == 0;
        sum += find(work, 'z') - work;
        mcpy(work + 100, text, 300);
        mset(work + 50, 'a' + (iter & 7), 40);
        for (i = 0; i < 7; i++) sum = sum * 3 + cmp(work + i * 60, words[i]);
    }
    report("strings", sum);
    return 0;
}
