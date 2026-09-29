/* The Chairman's serial console.  Output is direct.  Input has no
   interrupt, so the tick polls it into a ring that console_getc()
   waits on. */
#include "kernel.h"

#define RING 64

static unsigned char ring[RING];
static unsigned head, count;
static int at_eof;
static struct waitq readers;

void console_init(void)
{
    waitq_init(&readers);
}

void console_putc(int c)
{
    if (c == '\n') {
        CH_SERIAL_OUT = '\r';
    }
    CH_SERIAL_OUT = (uint32_t)c;
}

void console_puts(const char *s)
{
    while (*s != '\0') {
        console_putc(*s++);
    }
}

/* From the tick, with the kernel quiet.  Returns a thread it woke. */
struct thread *console_poll(void)
{
    uint32_t flags = CH_SERIAL_FLAGS;
    int got = 0;

    while ((flags & 1) != 0 && count < RING) {
        ring[(head + count) % RING] = (unsigned char)CH_SERIAL_IN;
        count++;
        got = 1;
        flags = CH_SERIAL_FLAGS;
    }
    if ((flags & 2) != 0 && at_eof == 0) {
        at_eof = 1;
        got = 1;
    }
    return got != 0 ? waitq_wake_one(&readers) : NULL;
}

/* The next byte, or -1 once the input has ended. */
int console_getc(void)
{
    int c;

    kenter();
    while (count == 0 && at_eof == 0) {
        waitq_wait(&readers);
    }
    if (count == 0) {
        c = -1;
    } else {
        c = ring[head];
        head = (head + 1) % RING;
        count--;
        if (count != 0) {
            preempt_if(waitq_wake_one(&readers));   /* pass it on */
        }
    }
    kexit();
    return c;
}

/* Bytes waiting, for a reader that must not block */
int console_pending(void)
{
    return (int)count;
}

/* A line, without its newline, terminated.  Returns its length, or -1
   at the end of input with nothing read. */
int console_gets(char *buf, size_t size)
{
    size_t n = 0;
    int c;

    for (;;) {
        c = console_getc();
        if (c == -1) {
            if (n == 0) {
                return -1;
            }
            break;
        }
        if (c == '\n' || c == '\r') {
            break;
        }
        if (n + 1 < size) {
            buf[n++] = (char)c;
        }
    }
    buf[n] = '\0';
    return (int)n;
}
