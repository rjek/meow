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
nmcc -std=c99 -Ilibc/pdclib/include -Ilibc/meow/include -c hello.c
mld -f bin -d 0x08000000 -o hello.bin rt/crt0.o rt/mul.o rt/div.o rt/ll.o rt/softfp.o hello.o libc/libc.a
simulator/msim -q -r hello.bin
```

Two things about that command line:

- `-std=c99` or later.  PDCLib is C99 and the compiler's default is C90.
- Both `-I` directories.  `nmcc` has the RISC OS C headers compiled into
  it, but searches `-I` directories first, so PDCLib's win.  The MEOW
  directory adds `<float.h>`, `<signal.h>`, `<fenv.h>` and the internal
  configuration.
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
  out the memory between the end of `.bss` and 16 KB below the top of
  RAM, which is the stack's.  `crt0` and `sbrk` both read the size of
  the RAM from the Chairman's chip-select table, so `msim -m` decides
  how much there is.
- `exit` halts `msim` with the status.  `signal` and `raise` are PDCLib's
  example ones: nothing raises a signal but `raise` itself.
- `time` is the host's clock, through `msim`'s `BNV #-14`, and `clock`
  is the number of instructions executed, through `BNV #-16`, so
  `CLOCKS_PER_SEC` is 1000000 as if the machine ran at 1 MHz.
  The time zone tables are cut from 2000 transitions to 200, and the zone
  state lives in its own object so that a program that prints but never
  asks the time does not carry 30 KB of it.
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
- `functions/stdlib/strtod.c`, `strtof.c`, `strtold.c`: a null end
  pointer is allowed, as the standard requires.  Upstream reads through
  it.
- `functions/_dlmalloc/malloc.c`: `USE_LOCKS` is 0 when
  `__STDC_NO_THREADS__` is defined.  Upstream sets it to 1 whatever the
  configuration says.
- `functions/time/strftime.c` and the rest are untouched.  The build
  passes dlmalloc its configuration on the command line.

musl:

- Nothing.  Everything musl assumes from its own headers, `hidden`,
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
- The `-std=c99` requirement above.
