# Which of the imported sources make the C library, for the msim build in
# this directory and Catflap's in ../os.  LIBC is where libc/ is.
LIBC ?= .
P := $(if $(filter .,$(LIBC)),,$(LIBC)/)
MUSL_INC = -Dhidden= -D'weak_alias(o,n)=extern int __musl_no_weak_alias' \
           -I$(P)meow/musl -I$(P)musl/src/internal
# What is left out and what stands in:
#  - PDCLib's remove (wants unlink), dlmalloc, and its zoneinfo-parsing
#    time zone code: common/ has malloc and a UTC gmtime, localtime and
#    mktime instead.
#  - PDCLib's strtod, strtof and strtold, which are approximate in
#    decimal and hang in hexadecimal: common/strtod.c has them as musl
#    does, through its floatscan, which is exact.
#  - musl's float functions, which common/mathf.c has as wrappers of the
#    double ones, all but fmaf, nextafterf and nexttowardf; its
#    long double internals, since long double is double; the
#    fabs/fdim/fmax/fmin family PDCLib has; and the functions that are
#    not C at all: the Bessel functions, exp10, scalb, significand,
#    sincos, finite.
#  - musl's table-driven exp, log, log2, log10 and pow: fdlibm/ has the
#    versions musl used before 2018, which have no tables.
PDCLIB_DROP := stdio/remove _dlmalloc/malloc time/gmtime time/gmtime_s time/localtime \
               time/localtime_s time/mktime stdlib/strtod stdlib/strtof stdlib/strtold \
               _PDCLIB/_PDCLIB_strtod_prelim _PDCLIB/_PDCLIB_naive_etod _PDCLIB/_PDCLIB_naive_ptod
PDCLIB_SRCS := $(filter-out $(addprefix $(P)pdclib/functions/,$(addsuffix .c,$(PDCLIB_DROP))) \
                            $(wildcard $(P)pdclib/functions/_tzcode/*.c), \
                            $(wildcard $(P)pdclib/functions/*/*.c))
MUSL_KEEP_F := fmaf nextafterf nexttowardf nanf erf modf
MUSL_DROP := lgammaf_r fabs fabsl fdim fdiml fmax fmaxl fmin fminl exp10 exp10l finite j0 j1 jn scalb \
             significand sincos sincosl __polevll __invtrigl __cosl __sinl __tanl __rem_pio2l \
             __math_invalidl __signbit __signbitl __fpclassify __fpclassifyl \
             exp2f_data logf_data log2f_data powf_data exp log log2 log10 pow log_data log2_data pow_data
MUSL_SRCS := $(filter-out $(addprefix $(P)musl/src/math/,$(addsuffix .c,$(MUSL_DROP))),\
                          $(filter-out %f.c,$(wildcard $(P)musl/src/math/*.c))) \
             $(addprefix $(P)musl/src/math/,$(addsuffix .c,$(MUSL_KEEP_F))) \
             $(P)musl/src/internal/floatscan.c
COMMON_SRCS := $(wildcard $(P)common/*.c)
FDLIBM_SRCS := $(wildcard $(P)fdlibm/*.c)
