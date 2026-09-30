#include "msim.h"

/* one function with more than twenty thousand virtual registers: the register
 * allocator's tables for it are larger than the compiler's unit of store */
#define S1(n) x += g[(x + n) & 7] ^ (y << (n & 7)); y = y * 5 + (x >> 3) + n;
#define S4(n) S1(n) S1(n + 1) S1(n + 2) S1(n + 3)
#define S16(n) S4(n) S4(n + 4) S4(n + 8) S4(n + 12)
#define S64(n) S16(n) S16(n + 16) S16(n + 32) S16(n + 48)
#define S256(n) S64(n) S64(n + 64) S64(n + 128) S64(n + 192)
#define S1024(n) S256(n) S256(n + 256) S256(n + 512) S256(n + 768)

static unsigned g[8] = { 3, 1, 4, 1, 5, 9, 2, 6 };

static unsigned big(unsigned x, unsigned y)
{
    S1024(0) S256(1024)
    return x ^ y;
}

int main(void)
{
    print_hex(big(1, 2)); putchar('\n');
    print_hex(big(0x12345678, 0x9abcdef0)); putchar('\n');
    return 0;
}
