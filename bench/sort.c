#include "bench.h"

/* quicksort with an insertion sort tail: recursion, compares, swaps */
#define N 2000
static int v[N];

static unsigned next(unsigned *s) { *s = *s * 1103515245u + 12345u; return *s >> 8; }

static void isort(int *a, int n)
{
    int i, j;
    for (i = 1; i < n; i++) {
        int t = a[i];
        for (j = i; j > 0 && a[j - 1] > t; j--) a[j] = a[j - 1];
        a[j] = t;
    }
}

static void qsort_(int *a, int n)
{
    int p, i, j, t;
    while (n > 12) {
        p = a[n / 2];
        i = 0; j = n - 1;
        while (i <= j) {
            while (a[i] < p) i++;
            while (a[j] > p) j--;
            if (i <= j) { t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
        }
        if (j + 1 < n - i) { qsort_(a, j + 1); a += i; n -= i; }
        else { qsort_(a + i, n - i); n = j + 1; }
    }
    isort(a, n);
}

int main(void)
{
    unsigned seed = 1, sum = 0;
    int i, iter;

    for (iter = 0; iter < 3; iter++) {
        for (i = 0; i < N; i++) v[i] = (int)(next(&seed) % 100000) - 50000;
        qsort_(v, N);
        for (i = 1; i < N; i++) if (v[i - 1] > v[i]) sum += 1000000;
        sum = sum * 31 + (unsigned)v[N / 3] + (unsigned)v[0] - (unsigned)v[N - 1];
    }
    report("sort", sum);
    return 0;
}
