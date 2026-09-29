#include "bench.h"

/* a bytecode interpreter: a big switch, a stack and pointer chasing */
enum { PUSH, ADD, SUB, MUL, DUP, DROP, SWAP, JNZ, JMP, LOAD, STORE, HALT };
static int stack[64];
static int mem[16];

static const unsigned char prog[] = {
    PUSH, 0, STORE, 0,                   /* acc = 0 */
    PUSH, 200, STORE, 1,                 /* i = 200 */
    /* 8: */ LOAD, 0, LOAD, 1, DUP, MUL, ADD, PUSH, 3, SUB, STORE, 0,
    LOAD, 1, PUSH, 1, SUB, DUP, STORE, 1,
    JNZ, 8,
    LOAD, 0, HALT
};

static int run(void)
{
    const unsigned char *pc = prog;
    int *sp = stack;
    int t;
    for (;;) {
        switch (*pc++) {
        case PUSH: *sp++ = *pc++; break;
        case ADD: sp--; sp[-1] += *sp; break;
        case SUB: sp--; sp[-1] -= *sp; break;
        case MUL: sp--; sp[-1] *= *sp; break;
        case DUP: sp[0] = sp[-1]; sp++; break;
        case DROP: sp--; break;
        case SWAP: t = sp[-1]; sp[-1] = sp[-2]; sp[-2] = t; break;
        case JNZ: if (*--sp) pc = prog + *pc; else pc++; break;
        case JMP: pc = prog + *pc; break;
        case LOAD: *sp++ = mem[*pc++]; break;
        case STORE: mem[*pc++] = *--sp; break;
        case HALT: return sp[-1];
        }
    }
}

int main(void)
{
    unsigned sum = 0;
    int i;
    for (i = 0; i < 25; i++) sum = sum * 5 + (unsigned)run() + i;
    report("vm", sum);
    return 0;
}
