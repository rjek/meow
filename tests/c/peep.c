#include "msim.h"

/* shapes the backend's peepholes rewrite: bit tests, n-- loops, empty
 * else branches, constants too big for an immediate inside loops */
static void show(int n) { print_int(n); putchar('\n'); }

unsigned crc_bits(const unsigned char *p, int n, unsigned crc)
{
    int i;
    while (n-- > 0) {
        crc ^= *p++;
        for (i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
    return crc;
}

int count_down(int n)
{
    int c = 0;
    while (n-- > 0) c += 2;
    return c * 100 + n;                 /* n ends at -1 */
}

int count_down_u(unsigned n)
{
    int c = 0;
    while (n-- != 0) c++;
    return c;
}

int count_up(int n)
{
    int c = 0;
    while (n++ < 10) c++;
    return c * 100 + n;
}

int bits(unsigned x)
{
    int r = 0;
    if (x & 1) r += 1;
    if ((x & 0x80000000u) == 0) r += 2;
    if (x & 0x100) r += 4; else r += 8;
    r += (x & 0x8000) ? 16 : 32;
    if ((x & 4) != 0 && (x & 8) != 0) r += 64;
    return r;
}

int bit_kept(unsigned x)
{
    unsigned b = x & 0x10;              /* the masked value is used again */
    if (b) return (int)b + 1;
    return (int)b;
}

int empty_else(int x)
{
    int y = 3;
    if (x > 5) y = x * 2; else ;
    if (x < 0) { } else y++;
    return y;
}

int big_add(int x)
{
    return x + 12345 - 1000000 + 2047;
}

unsigned big_cmp(unsigned *a, int n)
{
    int i;
    unsigned c = 0;
    for (i = 0; i < n; i++) {
        if (a[i] > 100000u) c++;
        if (a[i] == 0xdeadbeefu) c += 10;
        c += a[i] & 0x00ff0000u;
        c ^= 0x12345678u;
    }
    return c;
}

int main(void)
{
    static const unsigned char msg[] = "The quick brown fox";
    unsigned v[6] = { 1, 200000, 0xdeadbeefu, 0x00ab0000u, 3, 0x12345678u };

    show((int)crc_bits(msg, 19, 0xffffffffu));
    show(count_down(5)); show(count_down(0)); show(count_down(-3));
    show(count_down_u(7)); show(count_down_u(0));
    show(count_up(4)); show(count_up(12));
    show(bits(0)); show(bits(1)); show(bits(0x8100)); show(bits(0x8000010cu));
    show(bit_kept(0x30)); show(bit_kept(0x0f));
    show(empty_else(7)); show(empty_else(2)); show(empty_else(-1));
    show(big_add(1)); show(big_add(-5000));
    show((int)big_cmp(v, 6)); show((int)big_cmp(v, 0));
    return 0;
}
