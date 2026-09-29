# mld: the MEOW linker

`mld` combines ELF32 relocatable objects produced by `mas -f elf` into an
executable or a flat image.

```
mld [-o output] [-f elf|bin|cfx] [-b base] [-d base] [-M map] [-e symbol]
    [-S executable] [-B] [-p] [-k stack] [-R start,end,file] input.o...
```

| Option | Meaning |
|---|---|
| `-o output` | Output file.  Default `a.out`.  A name ending `.bin` or `.rom` selects a flat image |
| `-f elf`, `-f bin` or `-f cfx` | Output format: an ELF executable with final addresses and a symbol table, a flat image, or a Catflap program (below) |
| `-b base` | Address of the first byte.  Default 0 |
| `-d base` | Run address of writable data and BSS, for an image that lives in ROM; read-only data stays with the code.  Without it they follow the code.  The base may be a symbol's name, looked up in the `-S` executables |
| `-M map` | Write a map of sections and symbols (`-` for standard output) |
| `-e symbol` | Entry point.  Otherwise `__entry` (which `mas` defines from `ENTRY`), then `start`, then `main`, then 0 |
| `-S executable` | Resolve whatever is still undefined from an ELF executable's global symbols, as absolute addresses.  A Catflap program links against the kernel and its C library this way |
| `-B` | Lay out BSS in reverse input order, so that the last inputs' BSS directly follows their data.  Catflap uses it to make the shared library's data and BSS one range |
| `-p` | Pad a flat image to a multiple of 4 bytes, for whatever is appended to it |
| `-k stack` | For `-f cfx`: the bytes of stack the program's main thread wants |
| `-R start,end,file` | Write to `file` the addresses of every word within the range from symbol `start` to symbol `end` that points into that range, for whoever copies the range elsewhere |

An input may be an `ar` archive of objects, as `ar rcs` makes one.  Its
members are loaded only when they define a symbol that is still undefined
once every object named on the command line is in, in as many passes as
that takes, so a library costs a program only what it uses.  Archive
members are named `archive(member)` in diagnostics and the map.

Sections with the same name are merged in the order the inputs name them,
each contribution aligned as its object declares.  The output places code
sections first, then data, then BSS, consecutively from the base address,
each aligned to at least 4 bytes.  A flat image ends after the last data
section; BSS occupies no bytes in the file.

A `cfx` image is what Catflap loads, and needs `-d`, conventionally
`-d __user_data_base` with the kernel as `-S`.  Code and read-only data
are linked at 0, writable data and BSS at the data base.  The file is a
twelve-word header (`CFX2`; code, data and BSS sizes; entry offset;
stack size; the address the code is linked for, 0 until `mkromfs -b`
prelinks it; the data base; and three counts), the code, the initialised
data, then three lists of offsets: words in the code holding code
addresses, words in the data holding code addresses, and words in the
data holding data addresses.  A word in the code holding a data address
is never listed: the objects must be compiled with `nmcc -zsb`, which
adds the process's static base at run time instead.  Addresses of
symbols that `-S` resolved are absolute and never listed.
