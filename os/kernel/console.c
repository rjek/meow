/* The Chairman's serial console.  Output is direct; input is polled by
   the tick from stage 2 on. */
#include "kernel.h"

void console_init(void)
{
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
