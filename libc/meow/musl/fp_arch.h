/* Included by musl's libm.h in place of an architecture's own: there are
   no floating-point barriers, so the generic fallbacks in libm.h apply. */

/* POSIX constants musl's sources use; PDCLib's <math.h> is standard C
   and has none. */
#define M_PI     3.14159265358979323846
#define M_PI_2   1.57079632679489661923
#define M_PI_4   0.78539816339744830962
#define M_1_PI   0.31830988618379067154
#define M_2_PI   0.63661977236758134308
#define M_E      2.7182818284590452354
#define M_LN2    0.69314718055994530942
#define M_LN10   2.30258509299404568402
#define M_SQRT2  1.41421356237309504880
