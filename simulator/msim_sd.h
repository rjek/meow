/* An SD card in SPI mode, behind the IOC's SPI master, backed by a
 * file on the host. */
#ifndef MSIM_SD_H
#define MSIM_SD_H

#include <stdint.h>

struct msim_sd;

struct msim_sd *msim_sd_open(const char *path);
void msim_sd_close(struct msim_sd *sd);
/* One byte each way while the card is selected */
uint8_t msim_sd_transfer(struct msim_sd *sd, uint8_t in);
/* The chip select was released: whatever was half done is forgotten */
void msim_sd_deselect(struct msim_sd *sd);

#endif
