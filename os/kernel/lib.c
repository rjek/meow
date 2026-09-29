/* What the kernel needs of a C library. */
#include "kernel.h"

void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *dp = d;
    const unsigned char *sp = s;

    while (n-- > 0) {
        *dp++ = *sp++;
    }
    return d;
}

void *memset(void *d, int c, size_t n)
{
    unsigned char *dp = d;

    while (n-- > 0) {
        *dp++ = (unsigned char)c;
    }
    return d;
}

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p != '\0') {
        p++;
    }
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    while (n > 0 && *a != '\0' && *a == *b) {
        a++;
        b++;
        n--;
    }
    return n == 0 ? 0 : (unsigned char)*a - (unsigned char)*b;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *pa = a, *pb = b;

    while (n-- > 0) {
        if (*pa != *pb) {
            return *pa - *pb;
        }
        pa++;
        pb++;
    }
    return 0;
}

char *strcat(char *d, const char *s)
{
    strcpy(d + strlen(d), s);
    return d;
}

char *strchr(const char *s, int c)
{
    while (*s != (char)c) {
        if (*s == '\0') {
            return NULL;
        }
        s++;
    }
    return (char *)s;
}

char *strcpy(char *d, const char *s)
{
    char *r = d;

    while ((*d++ = *s++) != '\0') {
    }
    return r;
}

static void put_num(unsigned long v, unsigned base, int upper, int width,
                    int zero, int neg)
{
    char buf[12];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";

    do {
        buf[n++] = digits[v % base];
        v /= base;
    } while (v != 0);
    if (neg != 0) {
        buf[n++] = '-';
    }
    while (width > n) {
        console_putc(zero ? '0' : ' ');
        width--;
    }
    while (n > 0) {
        console_putc(buf[--n]);
    }
}

/* %d %i %u %x %X %p %s %c %%, with a width and a 0 flag; l is accepted
   and means nothing, since long is int here */
void kvprintf(const char *fmt, va_list ap)
{
    while (*fmt != '\0') {
        int width = 0, zero = 0;
        const char *s;
        long v;

        if (*fmt != '%') {
            console_putc(*fmt++);
            continue;
        }
        fmt++;
        if (*fmt == '0') {
            zero = 1;
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }
        if (*fmt == 'l') {
            fmt++;
        }
        switch (*fmt++) {
        case 'd':
        case 'i':
            v = va_arg(ap, int);
            put_num(v < 0 ? -(unsigned long)v : (unsigned long)v, 10, 0,
                    width, zero, v < 0);
            break;
        case 'u':
            put_num(va_arg(ap, unsigned), 10, 0, width, zero, 0);
            break;
        case 'x':
            put_num(va_arg(ap, unsigned), 16, 0, width, zero, 0);
            break;
        case 'X':
            put_num(va_arg(ap, unsigned), 16, 1, width, zero, 0);
            break;
        case 'p':
            put_num((unsigned long)va_arg(ap, void *), 16, 0, 8, 1, 0);
            break;
        case 's':
            s = va_arg(ap, const char *);
            if (s == NULL) {
                s = "(null)";
            }
            width -= (int)strlen(s);
            while (width-- > 0) {
                console_putc(' ');
            }
            console_puts(s);
            break;
        case 'c':
            console_putc(va_arg(ap, int));
            break;
        case '%':
            console_putc('%');
            break;
        case '\0':
            return;
        default:
            console_putc('?');
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kpanic(const char *fmt, ...)
{
    va_list ap;

    console_puts("panic: ");
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
    console_putc('\n');
    kernel_halt(70);
}
