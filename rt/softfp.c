/* Software floating point for MEOW, in plain C compiled with nmcc.
 *
 * IEEE single and double precision, round to nearest even, denormals kept,
 * infinities and NaNs handled the obvious way, no exceptions.  A number is
 * unpacked into a sign, a binary exponent and a 64-bit mantissa with its
 * leading one at bit 62, so the same arithmetic serves both sizes; packing
 * rounds to 24 or 53 bits.  Nothing here is fast, and nothing here may use
 * a float or double operation, since that would call back into this file. */

typedef unsigned int u32;
typedef unsigned long long u64;

union du { double d; u32 w[2]; };      /* w[0] is the low word */

enum { ZERO, NORMAL, INF, NAN };

struct fp {
    int cls;
    int sign;
    int exp;                            /* value = mant * 2^exp */
    u64 mant;                           /* bit 62 set when NORMAL */
};

#define TOP ((u64)1 << 62)

/* The compiler keeps a 64-bit value in memory and calls the library for
 * most things done to it, so the hot loops below work on pairs of 32-bit
 * words, which live in registers. */

/* the runtime's 32 x 32 -> 64 multiply, in rt/mul.s */
unsigned long long __umull(unsigned a, unsigned b);

static int clz32(u32 x)
{
    int n = 0;
    if (x == 0) return 32;
    if ((x >> 16) == 0) { n += 16; x <<= 16; }
    if ((x >> 24) == 0) { n += 8; x <<= 8; }
    if ((x >> 28) == 0) { n += 4; x <<= 4; }
    if ((x >> 30) == 0) { n += 2; x <<= 2; }
    if ((x >> 31) == 0) { n += 1; }
    return n;
}

static int ctz32(u32 x)
{
    int n = 0;
    if (x == 0) return 32;
    if ((x & 0xffff) == 0) { n += 16; x >>= 16; }
    if ((x & 0xff) == 0) { n += 8; x >>= 8; }
    if ((x & 0xf) == 0) { n += 4; x >>= 4; }
    if ((x & 3) == 0) { n += 2; x >>= 2; }
    if ((x & 1) == 0) { n += 1; }
    return n;
}

/* ---- unpacking ---------------------------------------------------------- */

static void normalise(struct fp *x)
{
    u32 hi = (u32)(x->mant >> 32), lo = (u32)x->mant;
    int n;

    if (hi == 0 && lo == 0) {
        x->cls = ZERO;
        return;
    }
    n = (hi != 0 ? clz32(hi) : 32 + clz32(lo)) - 1;    /* leading one to bit 62 */
    if (n > 0) {
        x->mant <<= n;
        x->exp -= n;
    }
}

static struct fp unpack_f(u32 bits)
{
    struct fp x;
    u32 e = (bits >> 23) & 0xff;
    u32 m = bits & 0x7fffff;

    x.sign = (int)(bits >> 31);
    x.cls = NORMAL;
    x.mant = (u64)m << 39;
    x.exp = (int)e - 127 - 23 - 39;
    if (e == 0xff) {
        x.cls = m != 0 ? NAN : INF;
    } else if (e == 0) {
        x.exp += 1;                     /* denormal: exponent of 1, no hidden bit */
        normalise(&x);
    } else {
        x.mant |= TOP;
    }
    return x;
}

static struct fp unpack_d(double d)
{
    union du u;
    struct fp x;
    u32 e, m_hi;

    u.d = d;
    e = (u.w[1] >> 20) & 0x7ff;
    m_hi = u.w[1] & 0xfffff;
    x.sign = (int)(u.w[1] >> 31);
    x.cls = NORMAL;
    x.mant = (((u64)m_hi << 32) | u.w[0]) << 10;
    x.exp = (int)e - 1023 - 52 - 10;
    if (e == 0x7ff) {
        x.cls = x.mant != 0 ? NAN : INF;
    } else if (e == 0) {
        x.exp += 1;
        normalise(&x);
    } else {
        x.mant |= TOP;
    }
    return x;
}

/* ---- packing ------------------------------------------------------------ */

/* mant >> shift, rounded to nearest even. */
static u64 round_shift(u64 mant, int shift)
{
    u64 m, rem, half;

    if (shift <= 0) {
        return mant;
    }
    if (shift > 63) {
        return 0;                       /* below half the smallest denormal */
    }
    m = mant >> shift;
    rem = mant & (((u64)1 << shift) - 1);
    half = (u64)1 << (shift - 1);
    if (rem > half || (rem == half && (m & 1) != 0)) {
        m += 1;
    }
    return m;
}

/* Common tail of packing: the mantissa rounded to the width wanted, the
 * biased exponent, with denormals and overflow sorted out.  Returns the
 * exponent field and leaves the mantissa in *m. */
static int pack(const struct fp *x, int width, int bias, int maxexp, u64 *m)
{
    int shift = 62 - (width - 1);
    int e = x->exp + shift + (width - 1) + bias;

    if (e <= 0) {
        shift += 1 - e;                 /* denormal: shift more, exponent 0 */
        e = 0;
    }
    *m = round_shift(x->mant, shift);
    if (*m == ((u64)1 << width)) {      /* rounding carried out of the top */
        *m >>= 1;
        e += 1;
    }
    if (e == 0 && *m >= ((u64)1 << (width - 1))) {
        e = 1;                          /* rounded up into the normal range */
        *m &= ((u64)1 << (width - 1)) - 1;
    }
    if (e >= maxexp) {
        e = maxexp;
        *m = 0;                         /* infinity */
    }
    return e;
}

static u32 pack_f(struct fp x)
{
    u32 s = (u32)x.sign << 31;
    u64 m;
    int e;

    if (x.cls == NAN) return 0x7fc00000;
    if (x.cls == INF) return s | 0x7f800000;
    if (x.cls == ZERO) return s;
    e = pack(&x, 24, 127, 255, &m);
    return s | ((u32)e << 23) | ((u32)m & 0x7fffff);
}

static double pack_d(struct fp x)
{
    union du u;
    u32 s = (u32)x.sign << 31;
    u64 m;
    int e;

    if (x.cls == NAN) {
        u.w[0] = 0; u.w[1] = 0x7ff80000;
    } else if (x.cls == INF) {
        u.w[0] = 0; u.w[1] = s | 0x7ff00000;
    } else if (x.cls == ZERO) {
        u.w[0] = 0; u.w[1] = s;
    } else {
        e = pack(&x, 53, 1023, 2047, &m);
        u.w[0] = (u32)m;
        u.w[1] = s | ((u32)e << 20) | ((u32)(m >> 32) & 0xfffff);
    }
    return u.d;
}

/* ---- arithmetic on unpacked numbers ------------------------------------- */

static struct fp make_nan(void)
{
    struct fp x;
    x.cls = NAN; x.sign = 0; x.exp = 0; x.mant = 0;
    return x;
}

static struct fp make_zero(int sign)
{
    struct fp x;
    x.cls = ZERO; x.sign = sign; x.exp = 0; x.mant = 0;
    return x;
}

static struct fp make_inf(int sign)
{
    struct fp x;
    x.cls = INF; x.sign = sign; x.exp = 0; x.mant = 0;
    return x;
}

/* mant >> n keeping a sticky bit at the bottom for anything lost */
static u64 shift_sticky(u64 mant, int n)
{
    u64 lost;

    if (n <= 0) return mant;
    if (n > 63) return mant != 0 ? 1 : 0;
    lost = mant & (((u64)1 << n) - 1);
    return (mant >> n) | (lost != 0 ? 1 : 0);
}

static struct fp fp_add(struct fp a, struct fp b)
{
    struct fp r;
    struct fp t;
    int d;

    if (a.cls == NAN || b.cls == NAN) return make_nan();
    if (a.cls == INF && b.cls == INF) {
        return a.sign == b.sign ? a : make_nan();
    }
    if (a.cls == INF) return a;
    if (b.cls == INF) return b;
    if (a.cls == ZERO && b.cls == ZERO) return make_zero(a.sign & b.sign);
    if (a.cls == ZERO) return b;
    if (b.cls == ZERO) return a;

    if (a.exp < b.exp || (a.exp == b.exp && a.mant < b.mant)) {
        t = a; a = b; b = t;            /* a is the larger in magnitude */
    }
    d = a.exp - b.exp;
    /* one bit of headroom for the carry out of an addition */
    a.mant >>= 1;
    b.mant = shift_sticky(b.mant, d + 1);
    r.cls = NORMAL;
    r.sign = a.sign;
    r.exp = a.exp + 1;
    if (a.sign == b.sign) {
        r.mant = a.mant + b.mant;
    } else {
        r.mant = a.mant - b.mant;
    }
    normalise(&r);
    if (r.cls == ZERO) r.sign = 0;
    return r;
}

static struct fp fp_neg(struct fp a)
{
    if (a.cls != NAN) a.sign ^= 1;
    return a;
}

/* Multiplying costs a step per bit of the smaller operand, so the
 * trailing zeros of each mantissa are shifted out first (a float has 39
 * of them, a double 10) and put back through the exponent. */
static void strip_low_zeros(struct fp *x)
{
    u32 lo = (u32)x->mant, hi = (u32)(x->mant >> 32);
    int n;

    if (lo == 0) {
        n = 32 + ctz32(hi);
        lo = hi >> (n - 32);
        hi = 0;
    } else {
        n = ctz32(lo);
        if (n != 0) {
            lo = (lo >> n) | (hi << (32 - n));
            hi >>= n;
        }
    }
    x->mant = ((u64)hi << 32) | lo;
    x->exp += n;
}

static struct fp fp_mul(struct fp a, struct fp b)
{
    struct fp r;
    u32 a_lo, a_hi, b_lo, b_hi, w0, w1, w2, w3;
    u64 p0, p1, p2, p3, mid, hi;
    int k, n;

    r.sign = a.sign ^ b.sign;
    if (a.cls == NAN || b.cls == NAN) return make_nan();
    if ((a.cls == INF && b.cls == ZERO) || (a.cls == ZERO && b.cls == INF)) {
        return make_nan();
    }
    if (a.cls == INF || b.cls == INF) return make_inf(r.sign);
    if (a.cls == ZERO || b.cls == ZERO) return make_zero(r.sign);

    strip_low_zeros(&a);
    strip_low_zeros(&b);
    a_lo = (u32)a.mant; a_hi = (u32)(a.mant >> 32);
    b_lo = (u32)b.mant; b_hi = (u32)(b.mant >> 32);
    if (a_hi == 0 && b_hi == 0) {
        p0 = __umull(a_lo, b_lo);
        w0 = (u32)p0; w1 = (u32)(p0 >> 32); w2 = 0; w3 = 0;
    } else {
        /* 64 x 64 -> 128 in 32-bit pieces */
        p0 = __umull(a_lo, b_lo);
        p1 = __umull(a_lo, b_hi);
        p2 = __umull(a_hi, b_lo);
        p3 = __umull(a_hi, b_hi);
        mid = (p0 >> 32) + (u32)p1 + (u32)p2;
        hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
        w0 = (u32)p0; w1 = (u32)mid; w2 = (u32)hi; w3 = (u32)(hi >> 32);
    }
    /* shift the leading one up to bit 127, then the top 63 bits are the
     * mantissa and everything below them the sticky bit */
    n = w3 != 0 ? clz32(w3) : w2 != 0 ? 32 + clz32(w2) :
        w1 != 0 ? 64 + clz32(w1) : 96 + clz32(w0);
    k = n;
    while (k >= 32) { w3 = w2; w2 = w1; w1 = w0; w0 = 0; k -= 32; }
    if (k != 0) {
        w3 = (w3 << k) | (w2 >> (32 - k));
        w2 = (w2 << k) | (w1 >> (32 - k));
        w1 = (w1 << k) | (w0 >> (32 - k));
        w0 <<= k;
    }
    r.cls = NORMAL;
    r.exp = a.exp + b.exp + 65 - n;
    r.mant = ((u64)w3 << 31) | (w2 >> 1);
    if ((w2 & 1) != 0 || w1 != 0 || w0 != 0) r.mant |= 1;
    return r;
}

static struct fp fp_div(struct fp a, struct fp b)
{
    struct fp r;
    u64 q, rem;
    int i;

    r.sign = a.sign ^ b.sign;
    if (a.cls == NAN || b.cls == NAN) return make_nan();
    if (a.cls == INF && b.cls == INF) return make_nan();
    if (a.cls == ZERO && b.cls == ZERO) return make_nan();
    if (a.cls == INF || b.cls == ZERO) return make_inf(r.sign);
    if (a.cls == ZERO || b.cls == INF) return make_zero(r.sign);

    /* long division, one quotient bit a time, on 32-bit words */
    {   u32 qh = 0, ql = 0;
        u32 rh = (u32)(a.mant >> 32), rl = (u32)a.mant;
        u32 bh = (u32)(b.mant >> 32), bl = (u32)b.mant;

        for (i = 0; i < 64; i++) {
            qh = (qh << 1) | (ql >> 31);
            ql <<= 1;
            if (rh > bh || (rh == bh && rl >= bl)) {
                if (rl < bl) rh -= 1;
                rl -= bl;
                rh -= bh;
                ql |= 1;
            }
            rh = (rh << 1) | (rl >> 31);
            rl <<= 1;
        }
        q = ((u64)qh << 32) | ql;
        rem = ((u64)rh << 32) | rl;
    }
    r.cls = NORMAL;
    r.exp = a.exp - b.exp - 63;
    r.mant = q | (rem != 0 ? 1 : 0);
    if ((r.mant >> 63) != 0) {
        r.mant = shift_sticky(r.mant, 1);
        r.exp += 1;
    }
    normalise(&r);
    return r;
}

#define UNORDERED 2

/* -1, 0, 1, or UNORDERED when a NaN is involved */
static int fp_cmp(struct fp a, struct fp b)
{
    if (a.cls == NAN || b.cls == NAN) return UNORDERED;
    if (a.cls == ZERO && b.cls == ZERO) return 0;
    if (a.cls == ZERO) return b.sign != 0 ? 1 : -1;
    if (b.cls == ZERO) return a.sign != 0 ? -1 : 1;
    if (a.sign != b.sign) return a.sign != 0 ? -1 : 1;
    if (a.cls == INF && b.cls == INF) return 0;
    if (a.cls == INF) return a.sign != 0 ? -1 : 1;
    if (b.cls == INF) return b.sign != 0 ? 1 : -1;
    if (a.exp != b.exp) {
        return (a.exp > b.exp) != (a.sign != 0) ? 1 : -1;
    }
    if (a.mant == b.mant) return 0;
    return (a.mant > b.mant) != (a.sign != 0) ? 1 : -1;
}

/* ---- conversions -------------------------------------------------------- */

static struct fp from_u64(u64 n, int sign)
{
    struct fp x;
    x.cls = NORMAL;
    x.sign = sign;
    x.exp = 0;
    x.mant = n;
    if (n == 0) return make_zero(0);
    while ((x.mant & ~(TOP | (TOP - 1))) != 0) {   /* bit 63 set */
        x.mant = shift_sticky(x.mant, 1);
        x.exp += 1;
    }
    normalise(&x);
    return x;
}

/* Truncate towards zero to an unsigned magnitude no wider than 'bits'.
 * C leaves out-of-range conversions undefined; here they saturate and a
 * NaN gives zero. */
static u64 to_u64(struct fp x, int bits, int *sign)
{
    u64 limit = bits == 64 ? ~(u64)0 : (((u64)1 << bits) - 1);
    u64 v;

    *sign = x.sign;
    if (x.cls == NAN || x.cls == ZERO) return 0;
    if (x.cls == INF) return limit;
    if (x.exp > 1) return limit;        /* the leading one is at bit 62 */
    if (x.exp >= 0) {
        v = x.mant << x.exp;
    } else if (x.exp < -63) {
        v = 0;
    } else {
        v = x.mant >> -x.exp;
    }
    return v > limit ? limit : v;
}

static int to_int(struct fp x)
{
    int sign;
    u64 m = to_u64(x, 31, &sign);
    if (sign != 0) {
        if (m == 0x7fffffff && to_u64(x, 32, &sign) > m) {
            return (int)0x80000000;     /* INT_MIN fits although 2^31 does not */
        }
        return -(int)m;
    }
    return (int)m;
}

static long long to_ll(struct fp x)
{
    int sign;
    u64 m = to_u64(x, 63, &sign);
    if (sign != 0) {
        if (m == 0x7fffffffffffffffull && to_u64(x, 64, &sign) > m) {
            return (long long)0x8000000000000000ull;
        }
        return -(long long)m;
    }
    return (long long)m;
}

/* ---- the entry points the compiler calls -------------------------------- */

unsigned _fadd(unsigned a, unsigned b) { return pack_f(fp_add(unpack_f(a), unpack_f(b))); }
unsigned _fsub(unsigned a, unsigned b) { return pack_f(fp_add(unpack_f(a), fp_neg(unpack_f(b)))); }
unsigned _frsb(unsigned a, unsigned b) { return pack_f(fp_add(unpack_f(b), fp_neg(unpack_f(a)))); }
unsigned _fmul(unsigned a, unsigned b) { return pack_f(fp_mul(unpack_f(a), unpack_f(b))); }
unsigned _fdiv(unsigned a, unsigned b) { return pack_f(fp_div(unpack_f(a), unpack_f(b))); }
unsigned _frdiv(unsigned a, unsigned b) { return pack_f(fp_div(unpack_f(b), unpack_f(a))); }
unsigned _fneg(unsigned a) { return pack_f(fp_neg(unpack_f(a))); }

int _fgr(unsigned a, unsigned b) { return fp_cmp(unpack_f(a), unpack_f(b)) == 1; }
int _fgeq(unsigned a, unsigned b) { int c = fp_cmp(unpack_f(a), unpack_f(b)); return c == 1 || c == 0; }
int _fls(unsigned a, unsigned b) { return fp_cmp(unpack_f(a), unpack_f(b)) == -1; }
int _fleq(unsigned a, unsigned b) { int c = fp_cmp(unpack_f(a), unpack_f(b)); return c == -1 || c == 0; }
int _feq(unsigned a, unsigned b) { return fp_cmp(unpack_f(a), unpack_f(b)) == 0; }
int _fneq(unsigned a, unsigned b) { return fp_cmp(unpack_f(a), unpack_f(b)) != 0; }

unsigned _fflt(int n) { return pack_f(from_u64(n < 0 ? -(u64)n : (u64)n, n < 0)); }
unsigned _ffltu(unsigned n) { return pack_f(from_u64(n, 0)); }
int _ffix(unsigned a) { return to_int(unpack_f(a)); }
unsigned _ffixu(unsigned a) { int s; u64 m = to_u64(unpack_f(a), 32, &s); return s != 0 ? 0 : (unsigned)m; }

double _dadd(double a, double b) { return pack_d(fp_add(unpack_d(a), unpack_d(b))); }
double _dsub(double a, double b) { return pack_d(fp_add(unpack_d(a), fp_neg(unpack_d(b)))); }
double _drsb(double a, double b) { return pack_d(fp_add(unpack_d(b), fp_neg(unpack_d(a)))); }
double _dmul(double a, double b) { return pack_d(fp_mul(unpack_d(a), unpack_d(b))); }
double _ddiv(double a, double b) { return pack_d(fp_div(unpack_d(a), unpack_d(b))); }
double _drdiv(double a, double b) { return pack_d(fp_div(unpack_d(b), unpack_d(a))); }
double _dneg(double a) { return pack_d(fp_neg(unpack_d(a))); }

int _dgr(double a, double b) { return fp_cmp(unpack_d(a), unpack_d(b)) == 1; }
int _dgeq(double a, double b) { int c = fp_cmp(unpack_d(a), unpack_d(b)); return c == 1 || c == 0; }
int _dls(double a, double b) { return fp_cmp(unpack_d(a), unpack_d(b)) == -1; }
int _dleq(double a, double b) { int c = fp_cmp(unpack_d(a), unpack_d(b)); return c == -1 || c == 0; }
int _deq(double a, double b) { return fp_cmp(unpack_d(a), unpack_d(b)) == 0; }
int _dneq(double a, double b) { return fp_cmp(unpack_d(a), unpack_d(b)) != 0; }

double _dflt(int n) { return pack_d(from_u64(n < 0 ? -(u64)n : (u64)n, n < 0)); }
double _dfltu(unsigned n) { return pack_d(from_u64(n, 0)); }
int _dfix(double a) { return to_int(unpack_d(a)); }
unsigned _dfixu(double a) { int s; u64 m = to_u64(unpack_d(a), 32, &s); return s != 0 ? 0 : (unsigned)m; }

unsigned _d2f(double a) { return pack_f(unpack_d(a)); }
double _f2d(unsigned a) { return pack_d(unpack_f(a)); }

long long _ll_sfrom_d(double a) { return to_ll(unpack_d(a)); }
long long _ll_sfrom_f(unsigned a) { return to_ll(unpack_f(a)); }
unsigned long long _ll_ufrom_d(double a) { int s; u64 m = to_u64(unpack_d(a), 64, &s); return s != 0 ? 0 : m; }
unsigned long long _ll_ufrom_f(unsigned a) { int s; u64 m = to_u64(unpack_f(a), 64, &s); return s != 0 ? 0 : m; }
double _ll_sto_d(long long n) { return pack_d(from_u64(n < 0 ? -(u64)n : (u64)n, n < 0)); }
unsigned _ll_sto_f(long long n) { return pack_f(from_u64(n < 0 ? -(u64)n : (u64)n, n < 0)); }
double _ll_uto_d(unsigned long long n) { return pack_d(from_u64(n, 0)); }
unsigned _ll_uto_f(unsigned long long n) { return pack_f(from_u64(n, 0)); }
