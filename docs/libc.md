# The C library

`libc/` holds a C library for programs compiled with `nmcc`: PDCLib for
everything but the maths, musl for the maths, and a platform layer for
MEOW under `msim`.  `make` builds `libc/libc.a`, an `ar` archive that
`mld` reads, taking only the members a program needs.

## What is imported

| Directory | From | Licence |
|---|---|---|
| `libc/pdclib/` | PDCLib, `master` as of 29 September 2026 (`functions/`, `include/` and the example platform, kept for reference) | CC0 1.0, `libc/pdclib/COPYING.CC0` |
| `libc/musl/` | musl 1.2.6: `src/math/`, `src/internal/libm.h` and `COPYRIGHT` | MIT, `libc/musl/COPYRIGHT` |
| `libc/meow/` | The MEOW platform layer, written for this repository | Public domain, as PDCLib's platform layers are |

Both are imported as they come, so that a newer release can be dropped in.
The few changes made to them are listed at the end.

## Building and using it

```
make -C libc                            # needs nmcc at ../norcroft-ng/bin
nmcc -std=c99 -Jlibc/pdclib/include -Ilibc/meow/include -c hello.c
mld -f bin -d 0x08000000 -o hello.bin rt/crt0.o rt/mul.o rt/div.o rt/ll.o rt/softfp.o hello.o libc/libc.a
simulator/msim -q -r hello.bin
```

Three things about that command line:

- `-std=c99` or later.  PDCLib is C99 and the compiler's default is C90.
- `-J`, not `-I`, for PDCLib's headers.  `nmcc` has the RISC OS C headers
  compiled into it and consults them before any `-I` directory; `-J`
  replaces them.  `-I` is right for the MEOW headers, which add
  `<float.h>`, `<signal.h>`, `<fenv.h>` and the internal configuration.
- Neither `rt/msim.o` nor `rt/exit.o`: the library has its own `putchar`,
  `puts` and `exit`.  `exit` runs the `atexit` handlers and flushes the
  streams before halting; the runtime's does not.

`tests/runlibc.sh` compiles each `tests/libc/*.c` this way and checks its
output against what the host's C library printed for the same program.

## The platform layer

`libc/meow/functions/` implements PDCLib's glue interface for a machine
with a console and nothing else:

- Standard output and standard error go to the console through `msim`'s
  character call, and standard input comes from it a line at a time.  No
  file can be opened; `fopen` fails with `ENOENT`, `tmpfile` returns
  `NULL`, and `fseek` fails with `ESPIPE`.
- The heap is dlmalloc, PDCLib's allocator, fed by `sbrk()`, which hands
  out the memory between the end of `.bss` and `0x0800E000`, leaving the
  top 8 KB of `msim`'s 64 KB of RAM for the stack that `crt0` starts at
  the top.
- `exit` halts `msim` with the status.  `signal` and `raise` are PDCLib's
  example ones: nothing raises a signal but `raise` itself.
- There is no clock: `time` and `clock` return -1, `timespec_get` fails.
  The time zone tables are cut from 2000 transitions to 200, and the zone
  state lives in its own object so that a program that prints but never
  asks the time does not carry 30 KB of it.
- `getenv` finds nothing and `system` has no command processor.
- `<fenv.h>` reports round-to-nearest and never sees an exception, which
  is what the soft-float library does.

`libc/meow/include/pdclib/_PDCLIB_config.h` is PDCLib's configuration
with every value written out: `char` is unsigned, `wchar_t` is `int`,
`long double` is `double`, every argument takes whole words on the stack
so the `va_arg` macros step by words, and there are no threads.

## Sizes

A program that uses `printf` with floating conversions, `malloc`,
`qsort`, `strtod` and a few maths functions links to about 83 KB of code
and 16 KB of read-only data, most of it PDCLib's exact decimal conversion
and musl's tables.  Its zero-initialised data is under 4 KB.  A program
that only uses `puts` and `strlen` is far smaller, since the archive is
pulled in a member at a time.

## Changes to the imported sources

PDCLib:

- `include/math.h`: `ilogb` returns `int`, `scalbln` takes a `long`, and
  `nexttoward` takes a `long double`, as the standard says.  The upstream
  header has them wrong.
- `functions/stdlib/strtod.c`, `strtof.c`, `strtold.c`: a null end
  pointer is allowed, as the standard requires.  Upstream reads through
  it.
- `functions/_dlmalloc/malloc.c`: `USE_LOCKS` is 0 when
  `__STDC_NO_THREADS__` is defined.  Upstream sets it to 1 whatever the
  configuration says.
- `functions/time/strftime.c` and the rest are untouched.  The build
  passes dlmalloc its configuration on the command line.

musl:

- `src/internal/libm.h`: `asuint64` and `asdouble` are `static inline`
  functions instead of compound literals.  Norcroft cannot yet initialise
  a compound literal from a 64-bit expression.
- `src/math/lrint.c`: the test `LONG_MAX < 1U<<53` is written as
  `LONG_MAX == 0x7fffffffL`.  Norcroft's preprocessor evaluates `#if` in
  32 bits.
- Everything else musl assumes from its own headers, `hidden`,
  `weak_alias`, the `M_PI` family, `<endian.h>`, `<fenv.h>`, `fp_arch.h`
  and `a_clz_64`, comes from `libc/meow/musl/` and the compiler options
  in `libc/Makefile`.  The two `weak_alias` uses that matter, `lgamma_r`
  and `lgammaf_r`, are ordinary functions in `libc/meow/functions/`.
  `fabs`, `fdim`, `fmax` and `fmin` come from PDCLib, which has them in
  every precision, so musl's are left out of the build.

## Known limitations

- `INFINITY`, `NAN` and `HUGE_VAL` are the overflowing and dividing
  constant expressions PDCLib's `<math.h>` defines, so every use draws a
  warning from the compiler's constant folder.  The values are right.
- `<math.h>` is PDCLib's, which is strictly standard C: no `M_PI`.
- `scanf` has no floating conversions: `%f` and friends are accepted and
  read nothing.  That is PDCLib as it stands; `strtod` on a line read
  with `fgets` does the job.
- The `-J` and `-std=c99` requirements above.  When `nmcc` gains a way to
  name a header directory as its default, they can go.
