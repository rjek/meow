; The romfs image, built by mkromfs from os/root, lives in the ROM
        AREA    |.rodata|, DATA, READONLY
        EXPORT  romfs_image
        ALIGN   4
romfs_image
        INCBIN  romfs.img
