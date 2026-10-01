/* sd: an SD card on the IOC's SPI master, served as /dev/sd0, a block
   device read and written at any byte offset.  Run it in the
   background at boot; it ends quietly if there is no card.

       sd &

   The card is brought up in SPI mode as the SD specification says
   (CMD0, CMD8, ACMD41 until ready, CMD58 for the OCR, CMD9 for the
   size) and only cards addressed by block, SDHC and larger, are
   served.  A partial block is read, changed and written back. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "catflap.h"

#define BLOCK 512
#define SD0 1

static volatile unsigned *spi;          /* control, data, status */
static unsigned control;                /* enable, the divisor; chip select added */
static unsigned blocks;
static unsigned char buf[BLOCK];

static unsigned char xfer(unsigned char out)
{
    spi[1] = out;
    while ((spi[2] & 1u) != 0) {
    }
    return (unsigned char)spi[1];
}

static void select_card(int on)
{
    spi[0] = on != 0 ? control | 8u : control;
    xfer(0xff);                         /* a clock with the select settled */
}

/* Send a command and return R1, or 0xff if the card never answered */
static unsigned char command(unsigned char cmd, unsigned arg, unsigned char crc)
{
    int i;
    unsigned char r;

    xfer(0xff);
    xfer((unsigned char)(0x40 | cmd));
    xfer((unsigned char)(arg >> 24));
    xfer((unsigned char)(arg >> 16));
    xfer((unsigned char)(arg >> 8));
    xfer((unsigned char)arg);
    xfer(crc);
    for (i = 0; i < 8; i++) {
        r = xfer(0xff);
        if ((r & 0x80) == 0) {
            return r;
        }
    }
    return 0xff;
}

/* A data block after a command that gives one: wait for the token */
static int read_data(unsigned char *out, unsigned n)
{
    int i;
    unsigned k;

    for (i = 0; i < 100; i++) {
        if (xfer(0xff) == 0xfe) {
            for (k = 0; k < n; k++) {
                out[k] = xfer(0xff);
            }
            xfer(0xff);                 /* the CRC */
            xfer(0xff);
            return 0;
        }
    }
    return -1;
}

static int card_init(void)
{
    unsigned char r, ocr[4], csd[16];
    unsigned c_size;
    int i;

    spi[0] = 0;
    control = 1u | (3u << 8);           /* enabled, clock / 8 */
    spi[0] = control;
    for (i = 0; i < 10; i++) {
        xfer(0xff);                     /* 80 clocks with the card deselected */
    }
    select_card(1);
    r = command(0, 0, 0x95);
    if (r != 0x01) {
        select_card(0);
        return -1;
    }
    r = command(8, 0x1aa, 0x87);
    if (r != 0x01) {
        select_card(0);
        return -1;                      /* an old card: not served */
    }
    for (i = 0; i < 4; i++) {
        ocr[i] = xfer(0xff);            /* R7's tail: the echo */
    }
    if (ocr[3] != 0xaa) {
        select_card(0);
        return -1;
    }
    for (i = 0; i < 1000; i++) {
        command(55, 0, 0x01);
        r = command(41, 0x40000000, 0x01);
        if (r == 0x00) {
            break;
        }
    }
    if (r != 0x00) {
        select_card(0);
        return -1;
    }
    r = command(58, 0, 0x01);
    for (i = 0; i < 4; i++) {
        ocr[i] = xfer(0xff);
    }
    if (r != 0x00 || (ocr[0] & 0x40) == 0) {
        select_card(0);
        return -1;                      /* not addressed by block */
    }
    r = command(9, 0, 0x01);
    if (r != 0x00 || read_data(csd, 16) < 0 || (csd[0] >> 6) != 1) {
        select_card(0);
        return -1;
    }
    c_size = ((unsigned)(csd[7] & 0x3f) << 16) | ((unsigned)csd[8] << 8) | csd[9];
    blocks = (c_size + 1) * 1024;
    select_card(0);
    return 0;
}

static int read_block(unsigned n, unsigned char *out)
{
    int rc;

    select_card(1);
    rc = command(17, n, 0x01) == 0x00 && read_data(out, BLOCK) == 0 ? 0 : -EIO;
    select_card(0);
    return rc;
}

static int write_block(unsigned n, const unsigned char *in)
{
    unsigned k;
    unsigned char r;
    int i, rc = -EIO;

    select_card(1);
    if (command(24, n, 0x01) == 0x00) {
        xfer(0xff);
        xfer(0xfe);
        for (k = 0; k < BLOCK; k++) {
            xfer(in[k]);
        }
        xfer(0xff);
        xfer(0xff);
        r = xfer(0xff);
        if ((r & 0x1f) == 0x05) {
            for (i = 0; i < 100000 && xfer(0xff) == 0x00; i++) {
            }                           /* busy while it writes */
            rc = 0;
        }
    }
    select_card(0);
    return rc;
}

/* Bytes at off, a block at a time; a partial block through buf */
static int transfer(unsigned char *data, unsigned len, unsigned off, int writing)
{
    unsigned done = 0, size = blocks * BLOCK;

    if (off >= size) {
        return 0;
    }
    if (len > size - off) {
        len = size - off;
    }
    while (done < len) {
        unsigned n = (off + done) / BLOCK, at = (off + done) % BLOCK;
        unsigned chunk = BLOCK - at < len - done ? BLOCK - at : len - done;
        int rc;

        if (writing != 0 && chunk == BLOCK) {
            rc = write_block(n, data + done);
        } else if (writing != 0) {
            rc = read_block(n, buf);
            if (rc == 0) {
                memcpy(buf + at, data + done, chunk);
                rc = write_block(n, buf);
            }
        } else {
            rc = read_block(n, buf);
            memcpy(data + done, buf + at, chunk);
        }
        if (rc < 0) {
            return done != 0 ? (int)done : rc;
        }
        done += chunk;
    }
    return (int)done;
}

static int serve(struct cf_req *r)
{
    switch (r->op) {
    case CF_OP_READ:
        return transfer(r->buf, r->len, r->off, 0);
    case CF_OP_WRITE:
        return transfer(r->buf, r->len, r->off, 1);
    case CF_OP_IOCTL:
        if (r->off == CF_BLK_SIZE) {
            *(unsigned *)r->buf = blocks * BLOCK;
            return 0;
        }
        return -ENOTTY;
    default:
        return -EINVAL;
    }
}

int main(void)
{
    struct cf_req *r;
    unsigned n, base = 0;
    volatile unsigned *ioc;
    int port, rc;

    for (n = 0; n < 32; n++) {
        if (CF_CS_DEVICE(n) == CF_DEV_IOC) {
            base = CF_CS_BASE(n);
        }
    }
    ioc = (volatile unsigned *)base;
    if (base == 0 || (ioc[0] & (1u << 24)) == 0) {
        return 1;                       /* no IOC, or no SPI master on it */
    }
    spi = ioc + 0x300 / 4;
    if (card_init() < 0) {
        return 1;                       /* no card: nothing to serve */
    }
    port = srv_create(0, 0);
    rc = port < 0 ? port : srv_dev(port, "sd0", SD0);
    if (rc < 0) {
        fprintf(stderr, "sd: cannot serve /dev/sd0: error %d\n", -rc);
        return 1;
    }
    printf("sd: %u KB card at /dev/sd0\n", blocks / 2);
    fflush(stdout);
    while (srv_recv(port, &r, -1) > 0) {
        srv_reply(port, r, serve(r));
    }
    return 0;
}
