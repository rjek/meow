/* A peripheral written in C: a thread with CPU 1 to itself, running from
   that CPU's local memory, sends a line as a software UART on an IOC
   GPIO line, timing each bit from the IOC counter.  msim -G decodes
   it. */
#include <stdio.h>
#include <string.h>
#include "catflap.h"

#define BAUD 9600
#define LINE 0

struct job {
    volatile unsigned *gpio_set, *gpio_clear, *counter;
    unsigned freq;
    const char *text;
    volatile unsigned took;             /* cycles the whole line took */
};

/* Everything this touches is in its registers, its stack, the IOC and
   the text, which is in ROM. */
static int sender(void *arg)
{
    struct job *j = arg;
    const char *s = j->text;
    unsigned bit_time = j->freq / BAUD;
    unsigned next = *j->counter + bit_time;
    unsigned t0 = next;

    for (; *s != '\0'; s++) {
        unsigned frame = ((unsigned)(unsigned char)*s << 1) | 0x200u; /* start 0, 8 data, stop 1 */
        int b;

        for (b = 0; b < 10; b++) {
            while ((int)(*j->counter - next) < 0) {
            }
            if (((frame >> b) & 1u) != 0) {
                *j->gpio_set = 1u << LINE;
            } else {
                *j->gpio_clear = 1u << LINE;
            }
            next += bit_time;
        }
    }
    while ((int)(*j->counter - next) < 0) {
    }
    j->took = next - t0;
    return 0;
}

static int sender_end(void *arg)
{
    (void)arg;
    return 0;
}

int main(int argc, char **argv)
{
    unsigned n, base = 0;
    volatile unsigned *ioc;
    struct job j;
    int rc, status;

    for (n = 0; n < 32; n++) {
        if (CF_CS_DEVICE(n) == CF_DEV_IOC) {
            base = CF_CS_BASE(n);
        }
    }
    if (base == 0) {
        printf("no IOC\n");
        return 1;
    }
    ioc = (volatile unsigned *)base;
    if (((ioc[0] >> 16) & 0xff) <= LINE) {
        printf("no GPIO line %d\n", LINE);
        return 1;
    }
    j.freq = ioc[1];
    j.gpio_set = ioc + 0x40c / 4;
    j.gpio_clear = ioc + 0x410 / 4;
    j.counter = ioc + 0x508 / 4;
    j.text = argc > 1 ? argv[1] : "hello\n";
    j.took = 0;
    *j.gpio_set = 1u << LINE;           /* idle is high */
    ioc[0x400 / 4] |= 1u << LINE;       /* an output */
    rc = thread_spawn_local(sender, (unsigned)((char *)sender_end - (char *)sender), &j, 5,
                            1);
    if (rc < 0) {
        printf("thread_spawn_local: %d\n", rc);
        return 1;
    }
    while (j.took == 0) {
        thread_yield();
    }
    printf("sent %u bytes in %u cycles at %u baud\n", (unsigned)strlen(j.text), j.took,
           j.freq / (j.freq / BAUD));
    (void)status;
    return 0;
}
