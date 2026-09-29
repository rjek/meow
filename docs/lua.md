# Lua on MEOW

`lua/` holds the source of Lua 5.4.7 as released (MIT licence, in
`lua.h`), less `onelua.c` and the test build `ltests.c`, plus a Makefile
that compiles it with `nmcc` against `libc/` and links `lua.bin` for
`msim`.  Nothing in the Lua sources is changed.

## Building and running

```
make                    # from the top: the toolchain, the library, then lua/lua.bin
make -C lua run         # msim -q -r lua/lua.bin -m 1024
```

`lua.c`'s interactive loop reads a line at a time from `msim`'s standard
input, so the prompt behaves as it does anywhere else: type an
expression, get its value.  End the session with end-of-file.  There are
no command line arguments, so `lua.bin` is always interactive; to run a
script from a file, feed it on standard input after a line that reads
the rest as one chunk:

```
(echo "assert(load(io.read('a')))()"; cat script.lua) | simulator/msim -q -r lua/lua.bin -m 1024
```

which is what `tests/runlua.sh` does with each `tests/lua/*.lua`, and
then compares against the output Lua 5.4 on the host gives for the same
input run with `-i`.

## Memory and speed

| | |
|---|---|
| Code | 284 KB, the C library's included |
| Read-only data | 22 KB |
| Data and bss | 18 KB |
| RAM to start | 64 KB is not enough; a fresh state takes 15 KB of heap |
| RAM to be useful | 192 KB; `make run` gives 1024 KB |
| `fib(25)` | 204 million instructions, about 830 per Lua call |
| The sieve test | 15 million instructions |

Integers are 64 bits and numbers are doubles, both in software, which is
a good part of the cost.  `LUA_32BITS` in `luaconf.h` makes integers
The table below was measured before a compiler fix that stopped branch
islands repeating every pending branch.  That fix took 77 KB out of the
VM's code and three quarters of its instructions out of dispatch, so
the absolute figures are stale, but the comparison between the two
builds is not.
32-bit and numbers single-precision floats.  Measured with it on:

| | 64-bit | `LUA_32BITS` | |
|---|---|---|---|
| Code | 369400 | 313428 | -15% |
| Read-only data | 21920 | 9584 | -56% |
| Heap after 2000 short strings, KB | 94.3 | 83.8 | -11% |
| `fib(25)` | 857 M | 584 M | -32% |
| 300000 integer multiply-adds | 518 M | 386 M | -26% |
| 100000 float divide-adds | 654 M | 566 M | -14% |
| Sort 10000 integers | 119 M | 77 M | -35% |
| 20000 `tostring` and lengths | 440 M | 412 M | -6% |
| `gsub` over 4000 bytes, 20 times | 49 M | 43 M | -13% |
| 3000 `string.format` and a `concat` | 84 M | 73 M | -13% |

Integer-heavy code gains a third, string code around a tenth; the float
saving is small because the soft-float library's single precision is
not much cheaper than its double.  The price is the language: integers
wrap at 2^31, so `2147483647 + 1` is negative, `1 << 40` is 0, and
numbers carry seven digits, so `1666683333.3333` prints as
`1.666665e+09`.  The stock 64-bit build stays the default: the point is
to run the real thing, and the tests compare against host Lua, which
`LUA_32BITS` output would not match.

## What works and what does not

Everything a program can do with a console works: the string, table,
maths, utf8 and coroutine libraries, `pcall` and error messages with
tracebacks, `os.time` and `os.clock` (the host's clock and the
instruction count, through `msim`), `os.date`, `collectgarbage`.

`io.open`, `dofile`, `require` of a file, `os.remove`, `os.rename`,
`os.getenv` and `os.execute` fail cleanly, because there is no
filesystem and no shell; `io.read` and `io.write` on the standard
streams are all the I/O there is.  `os.exit` halts `msim`.

## What it took

The Lua sources compiled first time.  Running them found:

- The compiler could allocate a block copy's operand to `at`, which the
  copy itself counts through, so a `TValue` copied in a loop with every
  other register busy copied a pointer over itself.  Lua moves function
  results with exactly that loop.
- The middle end gave the 64-bit helper calls a signed result whatever
  the operands, so `(a - b) >> 63` on unsigned values shifted in sign
  bits; musl's `fmod`, `log` and `pow` all depend on that.
- PDCLib misspells `remquo` in `<math.h>`, leaves the comparison macros
  empty, and prints zero with `%g` as ` .00000e-325`.
- The runtime had no `setjmp`, no `argv`, no clock, and 64 KB of RAM with
  the stack placed by a constant.

`docs/libc.md` and `docs/decisions.md` have the details.
