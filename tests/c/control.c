#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

/* a dense switch becomes a branch table */
int classify(int n)
{
    switch (n) {
    case 0: return 100;
    case 1: return 101;
    case 2: return 102;
    case 3: return 103;
    case 4: return 104;
    case 5: return 105;
    case 6: return 106;
    case 7: return 107;
    default: return -1;
    }
}

/* long enough that conditional branches need islands */
int longfn(int n)
{
    int acc = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (i % 3 == 0) acc += i * 7;
        if (i % 5 == 0) acc -= i * 3;
        if (i % 7 == 0) acc ^= i;
        if (i % 11 == 0) acc += 1000;
        if (i % 13 == 0) acc -= 999;
        if (i % 17 == 0) acc += i << 2;
        if (i % 19 == 0) acc -= i >> 1;
        if (i % 23 == 0) acc += i * i;
        if (i % 29 == 0) acc -= i * 5;
        if (i % 31 == 0) acc += 12345;
        if (i % 37 == 0) acc -= 54321;
        if (i % 41 == 0) acc += i * 9;
        if (i % 43 == 0) acc ^= 0x5555;
        if (i % 47 == 0) acc += i * 11;
        if (i % 53 == 0) acc -= i * 13;
        if (i % 59 == 0) acc += i * 15;
        if (i % 61 == 0) acc -= i * 17;
        if (i % 67 == 0) acc += i * 19;
        if (i % 71 == 0) acc -= i * 21;
        if (i % 73 == 0) acc += i * 23;
        if (i % 79 == 0) acc -= i * 25;
        if (i % 83 == 0) acc += i * 27;
        if (i % 89 == 0) acc -= i * 29;
        if (i % 97 == 0) acc += i * 31;
        if (acc > 100000) acc -= 100000;
        if (acc < -100000) acc += 100000;
    }
    return acc;
}

int main(void)
{
    int i;
    int x = 0;

    for (i = -1; i < 10; i++) show(classify(i));
    show(longfn(200));
    show(longfn(1000));
    do { x += 3; } while (x < 20);
    show(x);
    while (x > 0) x -= 7;
    show(x);
    for (i = 0; i < 40; i++) { if (i == 17) break; if (i & 1) continue; x += i; }
    show(x);
    return x & 0xff;
}
