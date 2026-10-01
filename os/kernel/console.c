/* The console: the IOC's UART 0 when there is one, else the Chairman's
   serial registers.  Output is direct.  Input is polled by the tick
   into a ring that console_getc() waits on, so the UART's interrupt is
   not used; a break on the line is the end of input, which is how msim
   says its standard input has ended. */
#include "kernel.h"

#define RING 64

#define UART_RX     0x01
#define UART_ROOM   0x02
#define UART_BREAK  0x20

static unsigned char ring[RING];
static unsigned head, count;
static int at_eof;
static struct waitq readers;
static volatile uint32_t *uart;         /* status, data, divisor, enable, clear */

void console_init(void)
{
    int n;

    waitq_init(&readers);
    for (n = 0; n < 32; n++) {
        if (CH_CS_DEVICE(n) == DEV_IOC) {
            volatile uint32_t *ioc = (volatile uint32_t *)((uint32_t)n << 27);

            if (((ioc[0] >> 8) & 0xff) >= 1) {
                uart = ioc + 0x100 / 4;
            }
        }
    }
}

void console_putc(int c)
{
    if (c == '\n') {
        console_putc('\r');
    }
    if (uart != NULL) {
        while ((uart[0] & UART_ROOM) == 0) {
        }
        uart[1] = (uint32_t)c;
    } else {
        CH_SERIAL_OUT = (uint32_t)c;
    }
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
    uint32_t flags;
    int got = 0, ended = 0;

    if (uart != NULL) {
        flags = uart[0];
        while ((flags & UART_RX) != 0 && count < RING) {
            ring[(head + count) % RING] = (unsigned char)uart[1];
            count++;
            got = 1;
            flags = uart[0];
        }
        if ((flags & UART_BREAK) != 0) {
            uart[4] = UART_BREAK;
            ended = 1;
        }
    } else {
        flags = CH_SERIAL_FLAGS;
        while ((flags & 1) != 0 && count < RING) {
            ring[(head + count) % RING] = (unsigned char)CH_SERIAL_IN;
            count++;
            got = 1;
            flags = CH_SERIAL_FLAGS;
        }
        ended = (flags & 2) != 0;
    }
    if (ended != 0 && at_eof == 0) {
        at_eof = 1;
        got = 1;
    }
    if (got != 0) {
        struct thread *reader = waitq_wake_one(&readers), *poller = poll_wake();

        return reader != NULL ? reader : poller;
    }
    return NULL;
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

/* Whether a read would return at once: with a byte, or with the end */
int console_readable(void)
{
    return count != 0 || at_eof != 0;
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
