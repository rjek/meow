/* scanf called again after a line: %s must skip the newline left behind */
#include <stdio.h>
int main(void)
{
    char a[64], b[16];
    int n = 0, r;

    while ((r = scanf("%63s %15s", a, b)) == 2) {
        printf("[%s|%s]\n", a, b);
        n++;
    }
    printf("r=%d n=%d\n", r, n);
    r = scanf("%15s", a);
    printf("single r=%d\n", r);
    return 0;
}
