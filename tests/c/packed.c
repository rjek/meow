#include "msim.h"

static void show(int n) { print_int(n); putchar('\n'); }

PACKED_STRUCT hdr { unsigned char tag; unsigned short len; unsigned int addr; short off; };

unsigned char raw[16] = { 1, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0xfe, 0xff, 9, 9, 9, 9, 9, 9, 9 };

unsigned int get_addr(PACKED_PTR(struct hdr) h) { return h->addr; }
int get_len(PACKED_PTR(struct hdr) h) { return h->len; }
int get_off(PACKED_PTR(struct hdr) h) { return h->off; }
void set_addr(PACKED_PTR(struct hdr) h, unsigned int a) { h->addr = a; }
void set_len(PACKED_PTR(struct hdr) h, unsigned short l) { h->len = l; }

int main(void)
{
    PACKED_PTR(struct hdr) h = (PACKED_PTR(struct hdr))raw;
    int i;

    show(sizeof(struct hdr));
    show(h->tag);
    show(get_len(h));
    print_hex(get_addr(h)); putchar('\n');
    show(get_off(h));
    set_addr(h, 0xa1b2c3d4u);
    set_len(h, 0xbeef);
    h->off = -2;
    for (i = 0; i < 9; i++) { print_hex(raw[i]); putchar(' '); }
    putchar('\n');
    show(get_off(h));
    return 0;
}
