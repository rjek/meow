#include "msim.h"

static void showx(unsigned n) { print_hex(n); putchar('\n'); }

/* many distinct large constants and addresses in one function, so the
 * literal pool has to be dumped mid-function and referenced from afar */
unsigned g0, g1, g2, g3, g4, g5, g6, g7;

unsigned big(unsigned x)
{
    unsigned acc = x;
    g0 = acc ^ 0x12345678u; acc += g0;
    g1 = acc ^ 0x23456789u; acc += g1;
    g2 = acc ^ 0x3456789au; acc += g2;
    g3 = acc ^ 0x456789abu; acc += g3;
    g4 = acc ^ 0x56789abcu; acc += g4;
    g5 = acc ^ 0x6789abcdu; acc += g5;
    g6 = acc ^ 0x789abcdeu; acc += g6;
    g7 = acc ^ 0x89abcdefu; acc += g7;
    acc += 0x9abcdef0u; acc ^= 0xabcdef01u; acc += 0xbcdef012u; acc ^= 0xcdef0123u;
    acc += 0xdef01234u; acc ^= 0xef012345u; acc += 0xf0123456u; acc ^= 0x01234567u;
    acc += 0x11111111u; acc ^= 0x22222222u; acc += 0x33333333u; acc ^= 0x44444444u;
    acc += 0x55555555u; acc ^= 0x66666666u; acc += 0x77777777u; acc ^= 0x88888888u;
    acc += 0x99999999u; acc ^= 0xaaaaaaaau; acc += 0xbbbbbbbbu; acc ^= 0xccccccccu;
    acc += 0xddddddddu; acc ^= 0xeeeeeeeeu; acc += 0xffffffffu; acc ^= 0x0f0f0f0fu;
    acc += 0xf0f0f0f0u; acc ^= 0x00ff00ffu; acc += 0xff00ff00u; acc ^= 0x0000ffffu;
    acc += 0xffff0000u; acc ^= 0x01010101u; acc += 0x02020202u; acc ^= 0x04040404u;
    acc += 0x08080808u; acc ^= 0x10101010u; acc += 0x20202020u; acc ^= 0x40404040u;
    acc += 0x80808080u; acc ^= 0x13579bdfu; acc += 0x2468ace0u; acc ^= 0xfdb97531u;
    acc += 0x0eca8642u; acc ^= 0xdeadbeefu; acc += 0xfeedfaceu; acc ^= 0x8badf00du;
    acc += 0xcafebabeu; acc ^= 0x0badcafeu; acc += 0xabad1deau; acc ^= 0xb16b00b5u;
    acc += 0x1badb002u; acc ^= 0xd15ea5e5u; acc += 0xdeadc0deu; acc ^= 0xfacefeedu;
    acc += 0xffffff00u; acc ^= 0xffff00ffu; acc += 0xff00ffffu; acc ^= 0x00ffffffu;
    acc += g0 * 3; acc ^= g1 * 5; acc += g2 * 7; acc ^= g3 * 9;
    acc += g4 * 11; acc ^= g5 * 13; acc += g6 * 15; acc ^= g7 * 17;
    if (acc > 0x80000000u) acc -= 0x12345u; else acc += 0x54321u;
    if (acc > 0x40000000u) acc -= 0x23456u; else acc += 0x65432u;
    if (acc > 0x20000000u) acc -= 0x34567u; else acc += 0x76543u;
    if (acc > 0x10000000u) acc -= 0x45678u; else acc += 0x87654u;
    return acc;
}

int main(void)
{
    showx(big(0));
    showx(big(1));
    showx(big(0xffffffffu));
    showx(g0 + g7);
    return 0;
}
