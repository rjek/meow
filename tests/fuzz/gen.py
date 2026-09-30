#!/usr/bin/env python3
"""gen.py SEED: a random C program for differential testing of nmcc against
the host compiler; see run.sh.  The program avoids undefined behaviour, but
counts on signed arithmetic wrapping (the host is given -fwrapv), on right
shifts of negative numbers being arithmetic and on narrowing conversions
keeping the low bits."""
import random, sys

class Gen:
    def __init__(self, seed):
        self.r = random.Random(seed)
        self.nfuncs = self.r.randint(6, 14)
        self.out = []

    def const(self):
        r = self.r
        k = r.random()
        if k < 0.35: return str(r.randint(0, 15))
        if k < 0.55: return str(r.randint(16, 255))
        if k < 0.70: return str(r.randint(256, 4095))
        if k < 0.80: return "0x%xu" % (1 << r.randint(0, 31))
        if k < 0.88: return "0x%xu" % (r.randint(1, 4095) << r.randint(1, 20) & 0xffffffff)
        return "0x%08xu" % r.getrandbits(32)

    def lval_u(self, depth_vars):
        r = self.r
        k = r.random()
        if k < 0.35: return r.choice(depth_vars[:4])
        if k < 0.50: return "g[%d]" % r.randint(0, 7)
        if k < 0.60: return "g[(%s) & 7]" % r.choice(depth_vars)
        if k < 0.72: return "p->%s" % r.choice(["a", "d", "arr[%d]" % r.randint(0, 5), "u16", "u8"])
        if k < 0.80: return "s[%d].%s" % (r.randint(0, 3), r.choice(["a", "d", "arr[%d]" % r.randint(0, 5)]))
        if k < 0.88: return "bytes[(%s) & 63]" % r.choice(depth_vars)
        if k < 0.94: return "p->next->%s" % r.choice(["a", "d", "u16"])
        return "gs[(%s) & 7]" % r.choice(depth_vars)

    def expr(self, vars_, depth=0):
        r = self.r
        if depth > 3 or r.random() < 0.25:
            k = r.random()
            if k < 0.45: return r.choice(vars_)
            if k < 0.65: return self.const()
            return "(unsigned)" + self.lval_u(vars_)
        k = r.random()
        a = self.expr(vars_, depth + 1)
        b = self.expr(vars_, depth + 1)
        if k < 0.18: return "(%s + %s)" % (a, b)
        if k < 0.30: return "(%s - %s)" % (a, b)
        if k < 0.38: return "(%s * %s)" % (a, self.const())
        if k < 0.46: return "(%s & %s)" % (a, b)
        if k < 0.52: return "(%s | %s)" % (a, b)
        if k < 0.60: return "(%s ^ %s)" % (a, b)
        if k < 0.66: return "(%s << (%s & 31))" % (a, b)
        if k < 0.72: return "(%s >> (%s & 31))" % (a, b)
        if k < 0.75: return "(unsigned)((int)%s >> (%s & 31))" % (a, b)
        if k < 0.79: return "(%s / (%s | 1))" % (a, b)
        if k < 0.82: return "(%s %% (%s | 1))" % (a, b)
        if k < 0.85: return "(unsigned)((int)%s / (int)((%s & 1023) + 1))" % (a, b)
        if k < 0.89: return "(%s %s %s ? %s : %s)" % (a, r.choice(["<", ">", "<=", ">=", "==", "!="]), b, self.expr(vars_, depth + 1), self.expr(vars_, depth + 1))
        if k < 0.91: return "((int)%s %s (int)%s)" % (a, r.choice(["<", ">", "<=", ">="]), b)
        if k < 0.93: return "(unsigned)(signed char)%s" % a
        if k < 0.95: return "(unsigned)(short)%s" % a
        if k < 0.97: return "(unsigned)(%s << %d >> %d)" % (a, r.randint(1, 30), r.randint(1, 30))
        return "(~%s)" % a

    def stmt(self, vars_, fidx, depth, emit, loopvar=0):
        r = self.r
        k = r.random()
        ind = "    " * (depth + 1)
        if k < 0.40:
            emit(ind + "%s = %s;" % (self.lval_u(vars_), self.expr(vars_)))
        elif k < 0.47:
            emit(ind + "%s %s= %s;" % (self.lval_u(vars_), r.choice(["+", "-", "^", "|", "&", "*"]), self.expr(vars_)))
        elif k < 0.57 and depth < 3:
            emit(ind + "if (%s %s %s) {" % (self.expr(vars_), r.choice(["<", ">", "==", "!=", "<=", ">="]), self.expr(vars_)))
            for _ in range(r.randint(1, 3)): self.stmt(vars_, fidx, depth + 1, emit, loopvar)
            if r.random() < 0.5:
                emit(ind + "} else {")
                for _ in range(r.randint(1, 3)): self.stmt(vars_, fidx, depth + 1, emit, loopvar)
            emit(ind + "}")
        elif k < 0.63 and depth < 2:
            v = "i%d" % loopvar
            emit(ind + "for (%s = 0; %s < %d; %s++) {" % (v, v, r.randint(1, 6), v))
            for _ in range(r.randint(1, 3)): self.stmt(vars_ + [v], fidx, depth + 1, emit, loopvar + 1)
            emit(ind + "}")
        elif k < 0.69 and depth < 2:
            n = r.randint(3, 9)
            emit(ind + "switch ((%s) %% %d) {" % (self.expr(vars_), n))
            for c in range(n):
                if r.random() < 0.8:
                    emit(ind + "case %d:" % c)
                    for _ in range(r.randint(1, 2)): self.stmt(vars_, fidx, depth + 1, emit, loopvar)
                    if r.random() < 0.85: emit(ind + "    break;")
            if r.random() < 0.5:
                emit(ind + "default:")
                self.stmt(vars_, fidx, depth + 1, emit, loopvar)
            emit(ind + "}")
        elif k < 0.80:
            # a call: earlier, later (forward), or itself with a lower budget
            callee = r.randint(0, self.nfuncs - 1)
            emit(ind + "if (budget > 0) { budget--; %s = f%d(%s, %s, %s); }" % (r.choice(vars_[:4]), callee, self.expr(vars_), self.expr(vars_), r.choice(["p", "p->next", "&s[%d]" % r.randint(0, 3)])))
        elif k < 0.85:
            emit(ind + "p->e = p->e * %dLL + (long long)%s - gl[%d];" % (r.randint(2, 99), self.expr(vars_), r.randint(0, 3)))
            emit(ind + "gl[%d] ^= p->e >> %d; %s ^= (unsigned)(gl[%d] >> %d) + (unsigned)(p->e < gl[%d]);" % (r.randint(0, 3), r.randint(0, 63), r.choice(vars_[:4]), r.randint(0, 3), r.randint(0, 40), r.randint(0, 3)))
        elif k < 0.90:
            emit(ind + "p->f = p->f * %s + (double)(int)(%s & 0xffff) - gd[%d] / %d.0;" % (r.choice(["0.5", "1.25", "0.75", "2.0", "-1.5"]), self.expr(vars_), r.randint(0, 3), r.randint(1, 9)))
            emit(ind + "if (p->f > 1e9 || p->f < -1e9) p->f = %d.5;" % r.randint(0, 9))
            emit(ind + "gd[%d] = p->f + %d.0; %s += (unsigned)(int)p->f + (p->f < gd[%d]);" % (r.randint(0, 3), r.randint(0, 99), r.choice(vars_[:4]), r.randint(0, 3)))
        elif k < 0.94:
            emit(ind + "s[%d] = *p; p = p->next;" % r.randint(0, 3))
        elif k < 0.97:
            emit(ind + "{ struct S *q = p->next; q->b = (short)%s; q->c = (signed char)%s; %s += (unsigned)q->b + (unsigned)q->c + q->arr[%d]; }" % (self.expr(vars_), self.expr(vars_), r.choice(vars_[:4]), r.randint(0, 5)))
        else:
            emit(ind + "gf = gf * 0.5f + (float)(int)(%s & 255); %s ^= (unsigned)(int)gf;" % (self.expr(vars_), r.choice(vars_[:4])))

    def program(self):
        r = self.r
        o = self.out.append
        o('#include "msim.h"')
        o("struct S { int a; short b; signed char c; unsigned char u8; unsigned short u16; unsigned d; long long e; double f; int arr[6]; struct S *next; };")
        o("static struct S s[4]; static unsigned g[8]; static unsigned char bytes[64]; static unsigned short gs[8];")
        o("static long long gl[4]; static double gd[4]; static float gf; static int budget;")
        for i in range(self.nfuncs):
            o("%sunsigned f%d(unsigned a, unsigned b, struct S *p);" % ("static " if i % 3 == 0 else "", i))
        for i in range(self.nfuncs):
            o("%sunsigned f%d(unsigned a, unsigned b, struct S *p)" % ("static " if i % 3 == 0 else "", i))
            o("{")
            o("    unsigned x = a ^ %s, y = b + %s; int i0, i1, i2;" % (self.const(), self.const()))
            vars_ = ["a", "b", "x", "y"]
            n = r.choice([3, 5, 8, 12, 20, 40, 90]) if i else 60
            for _ in range(n): self.stmt(vars_, i, 0, o)
            if r.random() < 0.3:
                o("    if (budget > 0) { budget--; return f%d(x, y, p); }" % r.randint(0, self.nfuncs - 1))
            o("    return x ^ (y << 3) ^ a ^ b;")
            o("}")
        o("int main(void)")
        o("{")
        o("    unsigned acc = 0; int i, j;")
        o("    for (i = 0; i < 4; i++) { s[i].next = &s[(i + 1) & 3]; s[i].a = i * 77; s[i].d = i + 0x1000; s[i].e = i * 1000003LL; s[i].f = i * 1.5; for (j = 0; j < 6; j++) s[i].arr[j] = i * j; }")
        o("    for (i = 0; i < 8; i++) { g[i] = i * 0x01010101u; gs[i] = i * 771; }")
        o("    for (i = 0; i < 64; i++) bytes[i] = i * 7;")
        o("    for (i = 0; i < 4; i++) { gl[i] = i * 0x100000001LL; gd[i] = i * 0.25; }")
        o("    for (i = 0; i < %d; i++) {" % r.randint(3, 8))
        o("        budget = %d;" % r.randint(5, 40))
        for i in range(self.nfuncs):
            o("        acc = acc * 31 + f%d(acc + i, i * %s, &s[i & 3]);" % (i, self.const()))
        o("    }")
        o("    for (i = 0; i < 8; i++) acc = acc * 31 + g[i] + gs[i];")
        o("    for (i = 0; i < 64; i++) acc = acc * 31 + bytes[i];")
        o("    for (i = 0; i < 4; i++) { acc = acc * 31 + (unsigned)s[i].a + s[i].d + (unsigned)s[i].b + (unsigned)s[i].c + s[i].u8 + s[i].u16 + (unsigned)s[i].e + (unsigned)(s[i].e >> 32) + (unsigned)(int)s[i].f; for (j = 0; j < 6; j++) acc = acc * 31 + s[i].arr[j]; }")
        o("    for (i = 0; i < 4; i++) acc = acc * 31 + (unsigned)gl[i] + (unsigned)(gl[i] >> 32) + (unsigned)(int)gd[i];")
        o("    print_hex(acc); putchar('\\n');")
        o("    return 0;")
        o("}")
        return "\n".join(self.out) + "\n"

if __name__ == "__main__":
    sys.stdout.write(Gen(int(sys.argv[1])).program())
