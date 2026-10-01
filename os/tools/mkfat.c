/* mkfat: make a FAT16 or FAT32 image on the host, with files in its root
   directory, for an SD card image msim can hold.  The image's size and
   type are what the tests ask for; sectors and clusters are 512 bytes.

       mkfat [-32] image KB [file...]

   FAT16 needs 4085 clusters at least, so an image of 2.1 MB or more;
   FAT32 needs 65525, so 33 MB or more.  Names are the files' base
   names, which must fit 8.3. */
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEC 512

static FILE *out;
static uint32_t *fat;
static uint32_t nclusters, data_start, next_free = 2;
static int fat32;

static void put16(unsigned char *p, unsigned v)
{
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}

static void put32(unsigned char *p, uint32_t v)
{
    put16(p, v & 0xffff);
    put16(p + 2, v >> 16);
}

static void write_sector(uint32_t n, const unsigned char *d)
{
    fseek(out, (long)n * SEC, SEEK_SET);
    fwrite(d, 1, SEC, out);
}

static uint32_t eoc(void)
{
    return fat32 ? 0x0fffffff : 0xffff;
}

/* The next cluster of a chain, taken from the free ones in order */
static uint32_t alloc(uint32_t prev)
{
    uint32_t c = next_free++;

    if (c >= nclusters + 2) {
        fprintf(stderr, "mkfat: the image is full\n");
        exit(1);
    }
    fat[c] = eoc();
    if (prev != 0) {
        fat[prev] = c;
    }
    return c;
}

static int to83(const char *name, unsigned char *e)
{
    const char *dot = strrchr(name, '.'), *base = strrchr(name, '/');
    size_t n, i;

    base = base != NULL ? base + 1 : name;
    if (dot != NULL && dot < base) {
        dot = NULL;
    }
    memset(e, ' ', 11);
    n = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if (n == 0 || n > 8 || (dot != NULL && strlen(dot + 1) > 3)) {
        return -1;
    }
    for (i = 0; i < n; i++) {
        e[i] = (unsigned char)toupper((unsigned char)base[i]);
    }
    for (i = 0; dot != NULL && dot[1 + i] != '\0'; i++) {
        e[8 + i] = (unsigned char)toupper((unsigned char)dot[1 + i]);
    }
    return 0;
}

int main(int argc, char **argv)
{
    unsigned char sec[SEC], *root, *dir;
    uint32_t kb, total, reserved, fat_sectors, root_sectors, root_cluster = 0;
    uint32_t c, i, used = 0, root_entries;
    int a = 1;

    if (argc > 1 && strcmp(argv[1], "-32") == 0) {
        fat32 = 1;
        a++;
    }
    if (argc < a + 2) {
        fprintf(stderr, "usage: mkfat [-32] image KB [file...]\n");
        return 2;
    }
    kb = (uint32_t)strtoul(argv[a + 1], NULL, 0);
    total = kb * 2;
    reserved = fat32 ? 32 : 1;
    root_entries = fat32 ? 0 : 512;
    root_sectors = root_entries * 32 / SEC;
    fat_sectors = (total / 1 * (fat32 ? 4 : 2) + SEC - 1) / SEC;   /* generous */
    data_start = reserved + 2 * fat_sectors + root_sectors;
    nclusters = (total - data_start);
    if ((fat32 == 0 && (nclusters < 4085 || nclusters >= 65525)) ||
        (fat32 != 0 && nclusters < 65525)) {
        fprintf(stderr, "mkfat: %u KB does not make a %s\n", kb, fat32 ? "FAT32" : "FAT16");
        return 1;
    }
    out = fopen(argv[a], "w+b");
    if (out == NULL) {
        perror(argv[a]);
        return 1;
    }
    fat = calloc(nclusters + 2, sizeof *fat);
    fat[0] = fat32 ? 0x0ffffff8 : 0xfff8;
    fat[1] = eoc();

    /* the boot sector */
    memset(sec, 0, SEC);
    memcpy(sec, "\xeb\x3c\x90MSWIN4.1", 11);
    put16(sec + 11, SEC);
    sec[13] = 1;                        /* sectors per cluster */
    put16(sec + 14, reserved);
    sec[16] = 2;
    put16(sec + 17, root_entries);
    put16(sec + 19, total < 65536 && fat32 == 0 ? total : 0);
    sec[21] = 0xf8;
    put16(sec + 22, fat32 != 0 ? 0 : fat_sectors);
    put16(sec + 24, 32);
    put16(sec + 26, 2);
    if (total >= 65536 || fat32 != 0) {
        put32(sec + 32, total);
    }
    if (fat32 != 0) {
        put32(sec + 36, fat_sectors);
        root_cluster = alloc(0);
        put32(sec + 44, root_cluster);
        put16(sec + 48, 1);             /* FSInfo */
        put16(sec + 50, 6);             /* backup boot sector */
        sec[66] = 0x29;
        put32(sec + 67, 0x4d454f57);
        memcpy(sec + 71, "MEOW       FAT32   ", 19);
    } else {
        sec[38] = 0x29;
        put32(sec + 39, 0x4d454f57);
        memcpy(sec + 43, "MEOW       FAT16   ", 19);
    }
    sec[510] = 0x55;
    sec[511] = 0xaa;
    write_sector(0, sec);
    if (fat32 != 0) {
        write_sector(6, sec);
        memset(sec, 0, SEC);
        memcpy(sec, "RRaA", 4);
        memcpy(sec + 484, "rrAa\xff\xff\xff\xff\xff\xff\xff\xff", 12);
        sec[510] = 0x55;
        sec[511] = 0xaa;
        write_sector(1, sec);
    }

    /* the root directory, in memory until the files are placed */
    root = calloc(fat32 != 0 ? SEC * 64 : root_sectors * SEC, 1);
    dir = root;
    for (a += 2; a < argc; a++) {
        FILE *f = fopen(argv[a], "rb");
        uint32_t size = 0, first = 0, prev = 0;
        size_t n;

        if (f == NULL) {
            perror(argv[a]);
            return 1;
        }
        if (to83(argv[a], dir) < 0) {
            fprintf(stderr, "mkfat: %s does not fit 8.3\n", argv[a]);
            return 1;
        }
        while ((n = fread(sec, 1, SEC, f)) > 0) {
            memset(sec + n, 0, SEC - n);
            c = alloc(prev);
            if (first == 0) {
                first = c;
            }
            write_sector(data_start + c - 2, sec);
            prev = c;
            size += (uint32_t)n;
        }
        fclose(f);
        dir[11] = 0x20;
        put16(dir + 24, 0x0021);        /* 1980-01-01 */
        put16(dir + 18, 0x0021);
        put16(dir + 16, 0x0021);
        put16(dir + 26, first & 0xffff);
        put16(dir + 20, first >> 16);
        put32(dir + 28, size);
        dir += 32;
        used++;
        if (fat32 == 0 && used == root_entries) {
            fprintf(stderr, "mkfat: the root directory is full\n");
            return 1;
        }
    }
    if (fat32 != 0) {
        uint32_t prev = root_cluster, need = (used * 32 + SEC - 1) / SEC;

        for (i = 0; i < (need != 0 ? need : 1); i++) {
            if (i > 0) {
                prev = alloc(prev);
            }
            write_sector(data_start + prev - 2, root + i * SEC);
        }
    } else {
        for (i = 0; i < root_sectors; i++) {
            write_sector(reserved + 2 * fat_sectors + i, root + i * SEC);
        }
    }

    /* both FATs, and the last sector so the file has its size */
    for (i = 0; i < fat_sectors; i++) {
        memset(sec, 0, SEC);
        for (c = 0; c < SEC / (fat32 ? 4 : 2); c++) {
            uint32_t k = i * (SEC / (fat32 ? 4 : 2)) + c;

            if (k < nclusters + 2) {
                if (fat32 != 0) {
                    put32(sec + c * 4, fat[k]);
                } else {
                    put16(sec + c * 2, fat[k]);
                }
            }
        }
        write_sector(reserved + i, sec);
        write_sector(reserved + fat_sectors + i, sec);
    }
    memset(sec, 0, SEC);
    write_sector(total - 1, sec);
    fclose(out);
    return 0;
}
