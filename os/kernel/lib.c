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

char *strrchr(const char *s, int c)
{
    const char *last = NULL;

    do {
        if (*s == (char)c) {
            last = s;
        }
    } while (*s++ != '\0');
    return (char *)last;
}

void strncpy_(char *d, const char *s, size_t size)
{
    size_t i;

    for (i = 0; i + 1 < size && s[i] != '\0'; i++) {
        d[i] = s[i];
    }
    d[i] = '\0';
}

char *strcpy(char *d, const char *s)
{
    char *r = d;

    while ((*d++ = *s++) != '\0') {
    }
    return r;
}

/* Where the formatter's characters go: the console, or a buffer for
   ksnprintf.  The kernel is never preempted, so one sink will do. */
static char *sink_buf;
static size_t sink_size, sink_len;

static void emit(int c)
{
    if (sink_buf == NULL) {
        console_putc(c);
        return;
    }
    if (sink_len + 1 < sink_size) {
        sink_buf[sink_len] = (char)c;
    }
    sink_len++;
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
        emit(zero ? '0' : ' ');
        width--;
    }
    while (n > 0) {
        emit(buf[--n]);
    }
}

/* %d %i %u %x %X %p %s %c %%, with a width and a 0 flag; l is accepted
   and means nothing, since long is int here */
static void format(const char *fmt, va_list ap)
{
    while (*fmt != '\0') {
        int width = 0, zero = 0;
        const char *s;
        long v;

        if (*fmt != '%') {
            emit(*fmt++);
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
                emit(' ');
            }
            while (*s != '\0') {
                emit(*s++);
            }
            break;
        case 'c':
            emit(va_arg(ap, int));
            break;
        case '%':
            emit('%');
            break;
        case '\0':
            return;
        default:
            emit('?');
            break;
        }
    }
}

void kvprintf(const char *fmt, va_list ap)
{
    char *saved = sink_buf;             /* a panic while formatting still reaches the console */

    sink_buf = NULL;
    kenter();                           /* one message, in one piece */
    format(fmt, ap);
    kexit();
    sink_buf = saved;
}

/* As snprintf: what would have been written, less the terminator */
int ksnprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    size_t n;

    sink_buf = buf;
    sink_size = size;
    sink_len = 0;
    va_start(ap, fmt);
    format(fmt, ap);
    va_end(ap);
    if (size > 0) {
        buf[sink_len < size ? sink_len : size - 1] = '\0';
    }
    n = sink_len;
    sink_buf = NULL;
    return (int)n;
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

void kernel_halt(int status)
{
    console_flush();
    cpu_halt(status);
}
