#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* standard input through the library: characters, lines and scanf */
int main(void)
{
    char line[64];
    int a, b, c, n, total = 0;
    double d;

    c = getchar();
    printf("first %c\n", c);
    if (fgets(line, sizeof line, stdin) == NULL) return 1;
    printf("rest [%s] %d\n", line, (int)strlen(line));
    n = scanf("%d %d", &a, &b);
    printf("scanf %d: %d %d\n", n, a, b);
    if (fgets(line, sizeof line, stdin) == NULL) return 1;   /* the rest of the scanf line */
    if (fgets(line, sizeof line, stdin) == NULL) return 1;
    d = strtod(line, NULL);                 /* PDCLib's scanf has no %f */
    printf("strtod %.3f\n", d);
    while (fgets(line, sizeof line, stdin) != NULL) {
        total += (int)strlen(line);
        printf("line %s", line);
    }
    printf("%d %d %d\n", total, feof(stdin) != 0, ferror(stdin) != 0);
    printf("%d\n", getchar());
    return 0;
}
