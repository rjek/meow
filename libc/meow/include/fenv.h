/* <fenv.h> for MEOW: the soft-float library rounds to nearest and raises
   nothing, so the environment is fixed and the exception flags stay clear. */
#ifndef _FENV_H
#define _FENV_H

#define FE_INVALID   1
#define FE_DIVBYZERO 4
#define FE_OVERFLOW  8
#define FE_UNDERFLOW 16
#define FE_INEXACT   32
#define FE_ALL_EXCEPT 63

#define FE_TONEAREST  0
#define FE_DOWNWARD   1
#define FE_UPWARD     2
#define FE_TOWARDZERO 3

typedef unsigned int fexcept_t;
typedef struct { unsigned int __cw; } fenv_t;
#define FE_DFL_ENV ((const fenv_t *)-1)

int feclearexcept(int);
int fegetexceptflag(fexcept_t *, int);
int feraiseexcept(int);
int fesetexceptflag(const fexcept_t *, int);
int fetestexcept(int);
int fegetround(void);
int fesetround(int);
int fegetenv(fenv_t *);
int feholdexcept(fenv_t *);
int fesetenv(const fenv_t *);
int feupdateenv(const fenv_t *);

#endif
