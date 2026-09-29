/* Console output for programs run under msim; see rt/msim.s. */
int putchar(int c);
int puts(const char *s);
void print_int(int n);
void print_hex(unsigned n);
void exit(int status);

/* Packed structures, spelt so the host compiler can run the same tests:
 * PACKED_STRUCT tag { ... }; and PACKED_PTR(struct tag) p. */
#ifdef __meow
#define PACKED_STRUCT __packed struct
#define PACKED_PTR(t) __packed t *
#else
#define PACKED_STRUCT struct __attribute__((packed))
#define PACKED_PTR(t) t *
#endif
