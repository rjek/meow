# mld: the MEOW linker

`mld` combines ELF32 relocatable objects produced by `mas -f elf` into an
executable or a flat image.

```
mld [-o output] [-f elf|bin] [-b base] [-M map] [-e symbol] input.o...
```

| Option | Meaning |
|---|---|
| `-o output` | Output file.  Default `a.out`.  A name ending `.bin` or `.rom` selects a flat image |
| `-f elf` or `-f bin` | Output format: an ELF executable with final addresses and a symbol table, or a flat image |
| `-b base` | Address of the first byte.  Default 0 |
| `-M map` | Write a map of sections and symbols (`-` for standard output) |
| `-e symbol` | Entry point.  Otherwise `__entry` (which `mas` defines from `ENTRY`), then `start`, then `main`, then 0 |

Sections with the same name are merged in the order the inputs name them,
each contribution aligned as its object declares.  The output places code
sections first, then data, then BSS, consecutively from the base address,
each aligned to at least 4 bytes.  A flat image ends after the last data
section; BSS occupies no bytes in the file.

Every relocation is resolved: `ABS32`, `ABS16` and `ABS8` store the final
address of the symbol plus addend (the narrower ones must fit), and
`REL32` stores it relative to the relocated word.  A symbol defined in two
objects, or referenced but defined in none, is an error naming the objects
involved; nothing is written if there are errors.

`mobjdump file` shows what went into or came out of the linker, and
`mobjdump -d` disassembles the code sections at their final addresses.
