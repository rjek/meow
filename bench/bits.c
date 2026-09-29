#include "bench.h"

/* bit twiddling: masks, variable shifts, rotates and byte swaps */
static unsigned pop(unsigned x) { unsigned n = 0; while (x) { x &= x - 1; n++; } return n; }
static unsigned rev(unsigned x)
{
    unsigned r = 0; int i;
    for (i = 0; i < 32; i++) { r = (r << 1) | (x & 1); x >>= 1; }
    return r;
}
static unsigned rotl(unsigned x, int n) { return (x << n) | (x >> (32 - n)); }
static unsigned bswap(unsigned x) { return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24); }
static unsigned parity(unsigned x) { x ^= x >> 16; x ^= x >> 8; x ^= x >> 4; x ^= x >> 2; x ^= x >> 1; return x & 1; }
static int clz(unsigned x) { int n = 0; if (x == 0) return 32; while ((x & 0x80000000u) == 0) { x <<= 1; n++; } return n; }

int main(void)
{
    unsigned sum = 0, x = 0x12345678u;
    int i;
    for (i = 0; i < 3000; i++) {
        x = x * 1664525u + 1013904223u;
        sum += pop(x) + rev(x) + rotl(x, i & 31) + bswap(x) + parity(x) + clz(x >> (i & 15));
        sum ^= (x & 0x00ff0000u) | (sum & 0x80000001u);
    }
    report("bits", sum);
    return 0;
}
