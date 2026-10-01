/* The IOC of the reference's section 6, as msim models it. */
#ifndef MSIM_IOC_H
#define MSIM_IOC_H

#include "msim_core.h"

void msim_add_ioc(struct msim_ctx *ctx, int area);
void msim_del_ioc(struct msim_ctx *ctx, int area);
/* Decode a software UART driven out of GPIO line at baud, printing the
 * bytes to standard output */
void msim_ioc_decode(struct msim_ctx *ctx, int area, int line, unsigned baud);
/* Put an SD card, the image file, on the SPI master */
int msim_ioc_sd(struct msim_ctx *ctx, int area, const char *image);

#endif
