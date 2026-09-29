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
| `-d base` | Run address of writable data and BSS, for an image that lives in ROM; read-only data stays with the code.  Without it they follow the code |
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

A `cfx` image is what Catflap loads: linked at 0 with data following
code, a six-word header (`CFX1`, image size, memory size including BSS,
entry offset, relocation count, stack size), the image, then the offset
of every word holding an address that the loader must add the load
address to.  Addresses of symbols that `-S` resolved are absolute and
left out of the list.

With `-d`, data and BSS are linked to run at that address, typically RAM,
while a flat image still stores the data initialisers straight after the
code.  Start-up code copies them into place using the symbols the linker
defines:

| Symbol | Value |
|---|---|
| `__data_load` | Where the initialised data is stored in the image (equal to `__data_start` in an ELF executable, which a loader places directly) |
| `__data_start`, `__data_end` | Where the initialised data runs |
| `__bss_start`, `__bss_end` | Uninitialised data, to be zeroed |

The runtime's `crt0.s` does exactly this before calling `main`.

Every relocation is resolved: `ABS32`, `ABS16` and `ABS8` store the final
address of the symbol plus addend (the narrower ones must fit), and
`REL32` stores it relative to the relocated word.  A symbol defined in two
objects, or referenced but defined in none, is an error naming the objects
involved; nothing is written if there are errors.

`mobjdump file` shows what went into or came out of the linker, and
`mobjdump -d` disassembles the code sections at their final addresses.
