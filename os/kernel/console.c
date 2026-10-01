/* The console: the IOC's UART 0, driven by its interrupt.  Received
   bytes go into a ring that console_getc() waits on, and a break on
   the line is the end of input, which is how msim says its standard
   input has ended.  Output goes to the UART's FIFO while there is
   room and otherwise into a ring the transmit interrupt drains; only
   when that too is full does a writer wait, and then it drains the
   ring itself, so that output from anywhere, the interrupt handler
   included, gets out.  A machine without an IOC has no console. */
#include "kernel.h"

#define RING 64
#define TXRING 128

#define UART_RX     0x01
#define UART_ROOM   0x02
#define UART_BREAK  0x20
#define IEN_RX      1
#define IEN_TX      2
#define BAUD        115200

static unsigned char ring[RING];
static unsigned head, count;
static unsigned char txring[TXRING];
static unsigned txhead, txcount;
static int at_eof;
static struct waitq readers;
static volatile uint32_t *uart;         /* status, data, divisor, enable, clear */
static uint32_t ien;                    /* what the UART is told to raise its interrupt for */

/* The receive interrupt is wanted while the ring has room, the transmit
   interrupt while the transmit ring has bytes: each is a condition that
   persists, so it must not be listened for when nothing can be done */
static void set_enables(void)
{
    uint32_t want = (count < RING ? IEN_RX : 0) | (txcount != 0 ? IEN_TX : 0);

    if (want != ien) {
        ien = want;
        uart[3] = ien;
    }
}

void console_init(void)
{
    int n;

    waitq_init(&readers);
    for (n = 0; n < 32; n++) {
        if (CH_CS_DEVICE(n) == DEV_IOC) {
            volatile uint32_t *ioc = (volatile uint32_t *)((uint32_t)n << 27);

            if (((ioc[0] >> 8) & 0xff) >= 1) {
                uint32_t div = (ioc[1] / 16 + BAUD / 2) / BAUD;

                uart = ioc + 0x100 / 4;
                uart[2] = div != 0 ? div - 1 : 0;
                uart[4] = UART_BREAK;
            }
        }
    }
}

/* Move what the transmit ring holds into the UART while there is room */
static void console_push(void)
{
    while (txcount != 0 && (uart[0] & UART_ROOM) != 0) {
        uart[1] = txring[txhead];
        txhead = (txhead + 1) % TXRING;
        txcount--;
    }
    set_enables();
}

static void putc_raw(int c)
{
    if (c == '\n') {
        putc_raw('\r');
    }
    if (txcount == 0 && (uart[0] & UART_ROOM) != 0) {
        uart[1] = (uint32_t)c;
        return;
    }
    while (txcount == TXRING) {         /* full: drain it ourselves */
        console_push();
    }
    txring[(txhead + txcount) % TXRING] = (unsigned char)c;
    txcount++;
    console_push();
}

/* The rings are the kernel's data: a writer holds the kernel while it
   touches them, which also keeps one writer's bytes together */
void console_putc(int c)
{
    if (uart != NULL) {
        kenter();
        putc_raw(c);
        kexit();
    }
}

void console_puts(const char *s)
{
    console_out(s, strlen(s));
}

void console_out(const char *s, size_t n)
{
    if (uart != NULL) {
        kenter();
        while (n-- != 0) {
            putc_raw(*s++);
        }
        kexit();
    }
}

/* Everything written is out: for the end of the machine */
void console_flush(void)
{
    while (uart != NULL && txcount != 0) {
        console_push();
    }
}

/* The UART's interrupt, with the kernel's data in a consistent state.
   Returns a thread it woke. */
static struct thread *console_irq(void)
{
    uint32_t flags = uart[0];
    int got = 0;

    while ((flags & UART_RX) != 0 && count < RING) {
        ring[(head + count) % RING] = (unsigned char)uart[1];
        count++;
        got = 1;
        flags = uart[0];
    }
    if ((flags & UART_BREAK) != 0) {
        uart[4] = UART_BREAK;
        if (at_eof == 0) {
            at_eof = 1;
            got = 1;
        }
    }
    console_push();
    set_enables();
    if (got != 0) {
        struct thread *reader = waitq_wake_one(&readers), *poller = poll_wake();

        return reader != NULL ? reader : poller;
    }
    return NULL;
}

/* Once the scheduler runs: listen to the UART */
void console_start(void)
{
    if (uart != NULL) {
        irq_attach(IRQ_UART0, console_irq);
        set_enables();
    }
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
        set_enables();                  /* room again: listen again */
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
