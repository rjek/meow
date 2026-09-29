#include "bench.h"

/* integer matrix multiply: nested loops, address arithmetic, __mul */
#define N 20
static int a[N][N], b[N][N], c[N][N];

int main(void)
{
    int i, j, k, iter;
    unsigned sum = 0;

    for (i = 0; i < N; i++)
        for (j = 0; j < N; j++) { a[i][j] = i * 3 - j; b[i][j] = (i + 1) * (j - 2); }
    for (iter = 0; iter < 4; iter++) {
        for (i = 0; i < N; i++)
            for (j = 0; j < N; j++) {
                int s = 0;
                for (k = 0; k < N; k++) s += a[i][k] * b[k][j];
                c[i][j] = s;
            }
        for (i = 0; i < N; i++) for (j = 0; j < N; j++) { sum = sum * 7 + (unsigned)c[i][j]; a[i][j] ^= sum & 3; }
    }
    report("matmul", sum);
    return 0;
}
