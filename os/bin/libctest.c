/* The C library from inside a process: stdio, the heap, atexit, strings,
   maths, and its data being this process's own. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <setjmp.h>
#include "catflap.h"

static jmp_buf jb;
static int counter = 5;

static void bye(void)
{
    printf("atexit: counter %d, errno %d\n", counter, errno);
}

static void jump(int n)
{
    if (n > 0) {
        longjmp(jb, n);
    }
}

int main(int argc, char **argv)
{
    char line[80], *p;
    int n = 0, i;
    double total = 0;
    FILE *f;

    printf("libctest pid %d, %d args\n", process_pid(), argc);
    atexit(bye);
    while (fgets(line, sizeof line, stdin) != NULL) {
        total += strtod(line, &p);
        n++;
        printf("line %d: %g (%s)\n", n, strtod(line, NULL), p[0] == '\n' ? "clean" : "junk");
    }
    printf("%d lines, total %.3f, sqrt %.4f, feof %d\n", n, total, sqrt(total), feof(stdin) != 0);
    p = malloc(1000);
    for (i = 0; i < 26; i++) {
        p[i] = (char)('a' + i);
    }
    p[26] = '\0';
    printf("malloc %s", p);
    p = realloc(p, 20000);
    printf(" %s\n", p != NULL ? "grew" : "failed");
    free(p);
    f = fopen("/etc/motd", "r");
    if (f == NULL) {
        printf("motd: cannot open, errno %d\n", errno);
    } else if (fgets(line, sizeof line, f) != NULL) {
        printf("motd: %s", line);
        fclose(f);
    } else {
        printf("motd: cannot read, errno %d\n", errno);
    }
    f = fopen("/no/such", "r");
    printf("bad open: %s, errno %d\n", f == NULL ? "NULL" : "?", errno);
    if ((i = setjmp(jb)) == 0) {
        jump(7);
        printf("not reached\n");
    } else {
        printf("longjmp %d\n", i);
    }
    counter += argc;
    sprintf(line, "%5.1f|%-4d|%x|%s", 3.14159, 42, 255, argv[argc - 1]);
    puts(line);
    exit(3);
}
