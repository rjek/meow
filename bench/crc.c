#include "bench.h"

/* CRC32 table driven and bit by bit: shifts, masks, table lookups */
static unsigned table[256];
static unsigned char buf[4096];

static void make_table(void)
{
    unsigned i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++) c = (c & 1) ? (c >> 1) ^ 0xedb88320u : c >> 1;
        table[i] = c;
    }
}

static unsigned crc_table(const unsigned char *p, int n, unsigned crc)
{
    while (n-- > 0) crc = table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return crc;
}

static unsigned crc_bits(const unsigned char *p, int n, unsigned crc)
{
    int i;
    while (n-- > 0) {
        crc ^= *p++;
        for (i = 0; i < 8; i++) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
    }
    return crc;
}

int main(void)
{
    unsigned crc = 0xffffffffu, i;

    make_table();
    for (i = 0; i < sizeof buf; i++) buf[i] = (unsigned char)(i * 7 + (i >> 5));
    for (i = 0; i < 6; i++) crc = crc_table(buf, sizeof buf, crc);
    crc = crc_bits(buf, 1024, crc);
    report("crc", ~crc);
    return 0;
}
