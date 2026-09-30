# The C library

`libc/` holds a C library for programs compiled with `nmcc`: PDCLib for
everything but the maths and the reading of numbers, musl for those, and
a platform layer for MEOW under `msim`.  `make` builds `libc/libc.a`, an `ar` archive that
`mld` reads, taking only the members a program needs.

## What is imported

| Directory | From | Licence |
|---|---|---|
| `libc/pdclib/` | PDCLib, `master` as of 29 September 2026 (`functions/`, `include/` and the example platform, kept for reference) | CC0 1.0, `libc/pdclib/COPYING.CC0` |
| `libc/musl/` | musl 1.2.6: `src/math/`, `libm.h`, `floatscan.c` and `floatscan.h` from `src/internal/`, and `COPYRIGHT` | MIT, `libc/musl/COPYRIGHT` |
| `libc/fdlibm/` | musl 1.1.19: `exp`, `log`, `log2`, `log10` and `pow`, the FreeBSD msun versions of fdlibm's | Sun's notice in each file |
| `libc/meow/` | The platform layer for `msim`, written for this repository | Public domain, as PDCLib's platform layers are |
| `libc/catflap/` | The platform layer for Catflap, likewise | Public domain |
| `libc/common/` | What both platforms share: `malloc`, the float maths functions, `strtod` and its relations, and `gmtime`, `localtime` and `mktime` | Public domain |

The imports are as they come, so that a newer release can be dropped in;
the few changes made to them are listed at the end.  Not all of them are
built.  `libc/sources.mk`, which both builds include, leaves out:

- PDCLib's `remove`, which wants a POSIX `unlink`; dlmalloc, 13 KB
  against the 1 KB of the K&R allocator in `common/`; and the time zone
  code, 12 KB of zoneinfo parsing for a machine with no zoneinfo, in
  favour of the UTC `gmtime`, `localtime` and `mktime` in `common/`.
- PDCLib's `strtod`, `strtof` and `strtold`, which are placeholders; see
  "Numbers in and out" below.
- musl's float functions.  `common/mathf.c` has every one of them as the
  double function rounded, which on a machine where both are software is
  no slower, correctly rounded wherever the double function is, and 40 KB
  shorter.  `fmaf`, `nextafterf` and `nexttowardf` cannot be had that way
  and stay musl's.
- musl's `exp`, `log`, `log2`, `log10` and `pow`, the ARM
  optimized-routines versions with 2 to 4 KB of tables each, in favour
  of the table-free fdlibm ones in `libc/fdlibm/`.  `exp2` stays: its
  table is shared with the new `exp`, and the old `exp2`'s was bigger.
- musl's long double internals, since `long double` is `double`, and the
  functions that are not C: the Bessel functions, `exp10`, `scalb`,
  `significand`, `sincos`, `finite`.

Both builds define `NDEBUG`, so PDCLib's own assertions, a dozen in the
big integer division, `strftime` and `atexit`, and the text they would
print, are left out; a program's own `assert` is its own business.

Every function of C99's `<math.h>` and `<time.h>` is still there, and
`atof`, which PDCLib declares and does not define, is there now.

For Catflap, `<math.h>` is a library of its own: `os/obj/libm.a`,
compiled like the rest but linked into each program that calls it rather
than into the ROM, which keeps only what its own code needs of it
(`fmod`, `scalbn`, `copysign`, `fabs` and the classification functions
behind the macros).  A program's link names the archive after its
objects and takes only the functions it calls, which is what a program
built later does too.  Under `msim`, `libc.a` still holds everything.

## Building and using it

```
make -C libc                            # needs nmcc at ../norcroft-ng/bin
nmcc -std=c99 -Ilibc/pdclib/include -Ilibc/meow/include -c hello.c
mld -f bin -d 0x08000000 -o hello.bin rt/crt0.o rt/mul.o rt/div.o rt/ll.o rt/softfp.o hello.o libc/libc.a
simulator/msim -q -r hello.bin
```

Three things about that command line:

- `-std=c99` or later.  PDCLib is C99 and the compiler's default is C90.
- Both `-I` directories.  `nmcc` has the RISC OS C headers compiled into
  it, but searches `-I` directories first, so PDCLib's win.  The MEOW
  directory adds `<float.h>`, `<signal.h>`, `<fenv.h>` and the internal
  configuration.
- Neither `rt/msim.o` nor `rt/exit.o`: the library has its own `putchar`,
  `puts` and `exit`.  `exit` runs the `atexit` handlers and flushes the
  streams before halting; the runtime's does not.

`tests/runlibc.sh` compiles each `tests/libc/*.c` this way and checks its
output against what the host's C library printed for the same program:
stdio, input, `scanf`, the maths functions to six digits, the calendar,
and number conversion to the last bit.

## The platform layer

`libc/meow/functions/` implements PDCLib's glue interface for a machine
with a console and nothing else:

- Standard output and standard error go to the console through `msim`'s
  character call, and standard input comes from it a line at a time.  No
  file can be opened; `fopen` fails with `ENOENT`, `tmpfile` returns
  `NULL`, and `fseek` fails with `ESPIPE`.
- The heap is `libc/common/malloc.c`, the allocator of Kernighan and
  Ritchie's book: an address-ordered free list, first fit, neighbours
  merged on free.  It replaces PDCLib's dlmalloc, which is 13 KB against
  its 1 KB and is built for workloads this machine does not have.  It is
  fed by `sbrk()`, which hands
  out the memory between the end of `.bss` and 16 KB below the top of
  RAM, which is the stack's.  `crt0` and `sbrk` both read the size of
  the RAM from the Chairman's chip-select table, so `msim -m` decides
  how much there is.
- `exit` halts `msim` with the status.  `signal` and `raise` are PDCLib's
  example ones: nothing raises a signal but `raise` itself.
- `time` is the host's clock, through `msim`'s `BNV #-14`, and `clock`
  is the number of instructions executed, through `BNV #-16`, so
  `CLOCKS_PER_SEC` is 1000000 as if the machine ran at 1 MHz.  Local
  time is UTC, and `time_t` is 64 bits, so 2038 is not the end.

- `getenv` finds nothing and `system` has no command processor.
- `<setjmp.h>` is the platform's, since PDCLib has none: `setjmp` saves
  v1 to v6, sp and lr.
- `remove` and `rename` fail: PDCLib's own `remove` wants a POSIX
  `unlink`, so the platform supplies one that goes through its hook.
- `<fenv.h>` reports round-to-nearest and never sees an exception, which
  is what the soft-float library does.

`libc/meow/include/pdclib/_PDCLIB_config.h` is PDCLib's configuration
with every value written out: `char` is unsigned, `wchar_t` is `int`,
`long double` is `double`, every argument takes whole words on the stack
so the `va_arg` macros step by words, and there are no threads.

## Numbers in and out

A `double` printed and read back must be the same `double`, and a
numeral must become the nearest `double` to what it says; Lua depends on
both.  What does each job:

| | Done by | How |
|---|---|---|
| `printf` of `%e`, `%f`, `%g` | PDCLib | Exactly, in big integer arithmetic, so every digit asked for is right |
| `printf` of `%a` | PDCLib, rewritten here | The bits of the value, rounded to the precision if one is given |
| `strtod`, `strtof`, `strtold`, `atof` | musl's `__floatscan`, from `common/strtod.c` | Exactly: decimal in base 10^9 arithmetic as long as the numeral needs, hexadecimal directly, `inf`, `nan` and `nan(...)`, `ERANGE` on overflow and underflow |

PDCLib's own `strtod` is, in its author's words, "nowhere good enough,
just a quick approximation".  It accumulates the digits in floating
point and scales by repeated multiplication, so `1e100`, `8.41e21` and
`DBL_MAX` came out a place or more wrong and `5e-324` as zero; it accepted
`1e` as a number; and given any hexadecimal digit it never returned,
since the loop that reads them does not advance.  musl's scanner wants
to read from a `FILE`, real or made up from a string.  Here only
strings are ever read, so `libc/meow/musl/shgetc.h` stands in for
musl's header of that name and makes `FILE`, in those two source files
alone, a pointer into a string.  The scanner is 5.7 KB, which is 3.7 KB
more than what it replaces.

`tests/libc/strtod.c` holds numerals chosen to be hard, halfway cases
and their neighbours, subnormals, the largest and smallest values, and
malformed ones, and prints the bits of each result, where it stopped,
and values through `%a` in every form of the conversion.

## Sizes

A program that uses `printf` with floating conversions, `malloc`,
`qsort`, `strtod` and a few maths functions, `tests/libc/hello.c`, links
to 67 KB of code and 4 KB of read-only data, most of the code being the
exact number conversions, the soft-float runtime and musl's maths.  Its
initialised and zero-initialised data together are 5 KB.  A program
that only uses `puts` and `strlen` is 17 KB, stdio and the arithmetic
runtime, which is linked whole; the archive is pulled in a member at a
time.

## Changes to the imported sources

PDCLib:

- `include/math.h`: `ilogb` returns `int`, `scalbln` takes a `long`, and
  `nexttoward` takes a `long double`, as the standard says.  The upstream
  header has them wrong.
- `include/math.h`: `remquo` and its float and long double forms were
  declared as `rmquo` with no `quo` argument, so nothing calling them
  had a prototype and their results were taken as `int`.  The comparison
  macros `isgreater` and friends were empty.
- `functions/_PDCLIB/_PDCLIB_print_fp.c` and `_PDCLIB_print_fp_deci.c`:
  `%g` and `%e` of zero printed ` .00000e-325`; `%g` kept the point when
  it dropped every digit after it, generated one digit too many and
  truncated it instead of rounding, and ignored `#`; a carry that ran
  off the front of the digits gained an extra power of ten.
- `include/stdio.h`: `stdin`, `stdout` and `stderr` are calls to a new
  `_PDCLIB_stdstream()` rather than the addresses of the stream objects,
  so that a program under Catflap, whose own code is not compiled with
  `-zsb`, reaches its process's copies of them.
- `functions/_PDCLIB/_PDCLIB_scan.c`: `%s` treated leading whitespace
  as a failure to match rather than skipping it, so a second `scanf`
  after a line had been read found nothing.
- `functions/stdio/remove.c` is left out of the build: it calls `unlink`
  rather than the `_PDCLIB_remove` hook the rest of the glue uses.
- `functions/_PDCLIB/_PDCLIB_print_fp_hexa.c`: `%a` is rewritten from
  the digits on.  Upstream dropped the first digit after the point and
  repeated the second, so that 1.5 printed as `0x1.0p+0`; rounded at 5
  rather than 8, as if the digits were decimal; read past the digits it
  had when the precision asked for more; and printed zero with a
  precision as `0x0p+0`.  The output now matches glibc's.
- `functions/stdlib/strtod.c`, `strtof.c`, `strtold.c` and the three
  `_PDCLIB_naive_*` and `_PDCLIB_strtod_prelim` files behind them are
  left out of the build, in favour of `libc/common/strtod.c`.  The
  first three still carry a patch from when they were used: a null end
  pointer is allowed, as the standard requires.
- `functions/_dlmalloc/malloc.c` is left out of the build, in favour of
  `libc/common/malloc.c`.  It still carries a patch from when it was
  used: `USE_LOCKS` is 0 when `__STDC_NO_THREADS__` is defined.

musl:

- `src/internal/floatscan.c`: a hexadecimal numeral keeps one more
  digit exactly, `LDBL_MANT_DIG/4+2` rather than `+1`.  Digits beyond
  those kept are remembered as half a unit of the last, which is sound
  only if the kept digits run past the precision of the result.  With
  a 64-bit `long double` they do, by a bit.  With a 53-bit one and a
  leading digit of 1, the fourteen digits kept are exactly the 53 bits,
  so any tail at all looked like a tie: `0x1.00000000000011p0` rounded
  up, to even, and `0x1.000000000000081p0` down.  The test has both.
- Nothing else.  Everything musl assumes from its own headers, `hidden`,
  `weak_alias`, the `M_PI` family, `<endian.h>`, `<fenv.h>`, `fp_arch.h`,
  `a_clz_64` and `shgetc.h`, comes from `libc/meow/musl/` and the
  compiler options in `libc/sources.mk`.  The two `weak_alias` uses
  that matter, `lgamma_r` and `lgammaf_r`, are ordinary functions in
  `libc/meow/functions/`.  `fabs`, `fdim`, `fmax` and `fmin` come from
  PDCLib, which has them in every precision, so musl's are left out of
  the build.

## Known limitations

- `INFINITY`, `NAN` and `HUGE_VAL` are the overflowing and dividing
  constant expressions PDCLib's `<math.h>` defines, so every use draws a
  warning from the compiler's constant folder.  The values are right.
- `<math.h>` is PDCLib's, which is strictly standard C: no `M_PI`.
- `scanf` has no floating conversions: `%f` and friends are accepted and
  read nothing.  That is PDCLib as it stands; `strtod` on a line read
  with `fgets` does the job.
- The `-std=c99` requirement above.
