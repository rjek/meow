/* wc: lines, words and bytes of standard input */
#include <stdio.h>
#include <ctype.h>

int main(int argc, char **argv)
{
    int c, lines = 0, words = 0, bytes = 0, inword = 0;

    (void)argc;
    (void)argv;
    while ((c = getchar()) != EOF) {
        bytes++;
        if (c == '\n') {
            lines++;
        }
        if (isspace(c)) {
            inword = 0;
        } else if (inword == 0) {
            inword = 1;
            words++;
        }
    }
    printf("%d %d %d\n", lines, words, bytes);
    return 0;
}
