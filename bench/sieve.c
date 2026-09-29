#include "bench.h"

/* the classic byte sieve: array indexing and inner loops with a stride */
#define N 8192
static unsigned char flags[N];

int main(void)
{
    int i, k, iter, count = 0;

    for (iter = 0; iter < 10; iter++) {
        count = 0;
        for (i = 0; i < N; i++) flags[i] = 1;
        for (i = 2; i < N; i++) {
            if (flags[i]) {
                for (k = i + i; k < N; k += i) flags[k] = 0;
                count++;
            }
        }
    }
    report("sieve", count);
    return 0;
}
