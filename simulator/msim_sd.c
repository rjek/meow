/* msim_sd.c: an SD card in SPI mode, enough of one for a driver to
 * bring up and read and write blocks: CMD0, CMD8, CMD55 and ACMD41,
 * CMD58, CMD9, CMD10, CMD16, CMD17 and CMD24.  The card is always of the
 * high-capacity kind, addressed by block, whatever the image's size,
 * which is the file's, rounded down to whole 512 KB, the unit its CSD
 * counts in.  Multiple block transfers are answered as illegal
 * commands. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "msim_sd.h"

#define BLOCK		512
#define UNIT		(512 * 1024)	/* the CSD counts capacity in these */
#define NCR		1		/* 0xff bytes before a response */
#define BUSY_BYTES	2		/* 0x00 bytes after a write */

enum state { S_CMD, S_RESP, S_WRITE_WAIT, S_WRITE_DATA, S_BUSY };

struct msim_sd {
	FILE		*f;
	uint32_t	blocks;
	enum state	state;
	int		idle;		/* until ACMD41 has been answered twice */
	int		acmd;		/* the last command was CMD55 */
	int		acmd41s;
	uint8_t		cmd[6];
	int		cmdlen;
	uint8_t		resp[NCR + 2 + 2 + BLOCK + 2];
	int		resplen, respi;
	uint8_t		data[BLOCK + 2];
	int		datai;
	uint32_t	addr;
	int		busy;
	int		accepted;	/* the data response is the next byte */
};

struct msim_sd *msim_sd_open(const char *path)
{
	struct msim_sd *sd = calloc(1, sizeof *sd);
	struct stat st;

	if (sd == NULL) {
		return NULL;
	}
	sd->f = fopen(path, "r+b");
	if (sd->f == NULL || stat(path, &st) != 0 || st.st_size < UNIT) {
		fprintf(stderr, "msim: %s: not an SD card image, which is a file"
			" of at least 512 KB\n", path);
		if (sd->f != NULL) {
			fclose(sd->f);
		}
		free(sd);
		return NULL;
	}
	sd->blocks = (uint32_t)(st.st_size / UNIT) * (UNIT / BLOCK);
	sd->idle = 1;
	return sd;
}

void msim_sd_close(struct msim_sd *sd)
{
	if (sd != NULL) {
		fclose(sd->f);
		free(sd);
	}
}

void msim_sd_deselect(struct msim_sd *sd)
{
	if (sd != NULL && sd->state != S_BUSY) {
		sd->state = S_CMD;
		sd->cmdlen = 0;
		sd->resplen = 0;
	}
}

static void respond(struct msim_sd *sd, const uint8_t *bytes, int n)
{
	int i;

	for (i = 0; i < NCR; i++) {
		sd->resp[i] = 0xff;
	}
	memcpy(sd->resp + NCR, bytes, (size_t)n);
	sd->resplen = NCR + n;
	sd->respi = 0;
	sd->state = S_RESP;
}

static void respond_r1(struct msim_sd *sd, uint8_t r1)
{
	respond(sd, &r1, 1);
}

/* CSD version 2 for a card of sd->blocks blocks, as the SD
 * specification lays it out, with a CRC the host is unlikely to check */
static void csd(struct msim_sd *sd, uint8_t *out)
{
	uint32_t c_size = sd->blocks / 1024 - 1;	/* in 512 KB units, less one */

	memset(out, 0, 16);
	out[0] = 0x40;				/* CSD structure 1.0: version 2 */
	out[1] = 0x0e;				/* TAAC */
	out[3] = 0x32;				/* transfer speed 25 MHz */
	out[4] = 0x5b;				/* command classes */
	out[5] = 0x59;				/* and read block length 512 */
	out[7] = (uint8_t)((c_size >> 16) & 0x3f);
	out[8] = (uint8_t)(c_size >> 8);
	out[9] = (uint8_t)c_size;
	out[10] = 0x7f;				/* erase sector size */
	out[11] = 0x80;
	out[12] = 0x0a;				/* write speed factor */
	out[13] = 0x40;				/* write block length 512 */
	out[15] = 0x01;				/* no CRC, stop bit */
}

static void command(struct msim_sd *sd)
{
	uint8_t cmd = sd->cmd[0] & 0x3f;
	uint32_t arg = ((uint32_t)sd->cmd[1] << 24) | ((uint32_t)sd->cmd[2] << 16) |
		       ((uint32_t)sd->cmd[3] << 8) | sd->cmd[4];
	uint8_t r1 = sd->idle != 0 ? 0x01 : 0x00;
	uint8_t bytes[2 + BLOCK + 2];
	int acmd = sd->acmd;

	sd->acmd = 0;
	sd->busy = 0;
	if (acmd != 0 && cmd == 41) {
		sd->acmd41s++;
		if (sd->acmd41s >= 2) {
			sd->idle = 0;
		}
		respond_r1(sd, sd->idle != 0 ? 0x01 : 0x00);
		return;
	}
	switch (cmd) {
	case 0:
		sd->idle = 1;
		sd->acmd41s = 0;
		respond_r1(sd, 0x01);
		break;
	case 8:				/* R7: R1, then the voltage and the check pattern */
		bytes[0] = r1;
		bytes[1] = 0;
		bytes[2] = 0;
		bytes[3] = 0x01;
		bytes[4] = (uint8_t)arg;
		respond(sd, bytes, 5);
		break;
	case 55:
		sd->acmd = 1;
		respond_r1(sd, r1);
		break;
	case 58:			/* R3: R1, then the OCR: ready, high capacity, 3.3 V */
		bytes[0] = r1;
		bytes[1] = sd->idle != 0 ? 0x40 : 0xc0;
		bytes[2] = 0xff;
		bytes[3] = 0x80;
		bytes[4] = 0x00;
		respond(sd, bytes, 5);
		break;
	case 9:				/* the CSD, as a data block */
	case 10:			/* the CID, likewise */
		bytes[0] = r1;
		bytes[1] = 0xfe;
		if (cmd == 9) {
			csd(sd, bytes + 2);
		} else {
			memset(bytes + 2, 0, 16);
			memcpy(bytes + 2, "\x01MSIMSDCARD", 11);
			bytes[17] = 0x01;
		}
		bytes[18] = 0;
		bytes[19] = 0;
		respond(sd, bytes, 20);
		break;
	case 16:
		respond_r1(sd, arg == BLOCK ? r1 : (uint8_t)(r1 | 0x40));
		break;
	case 12:
		respond_r1(sd, r1);
		break;
	case 17:
		if (sd->idle != 0) {
			respond_r1(sd, 0x01);
		} else if (arg >= sd->blocks) {
			respond_r1(sd, 0x40);	/* parameter error */
		} else {
			bytes[0] = 0x00;
			bytes[1] = 0xfe;
			fseek(sd->f, (long)arg * BLOCK, SEEK_SET);
			if (fread(bytes + 2, 1, BLOCK, sd->f) != BLOCK) {
				memset(bytes + 2, 0, BLOCK);
			}
			bytes[2 + BLOCK] = 0;
			bytes[3 + BLOCK] = 0;
			respond(sd, bytes, 2 + BLOCK + 2);
		}
		break;
	case 24:
		if (sd->idle != 0) {
			respond_r1(sd, 0x01);
		} else if (arg >= sd->blocks) {
			respond_r1(sd, 0x40);
		} else {
			sd->addr = arg;
			respond_r1(sd, 0x00);
			sd->busy = -1;		/* the data block follows the response */
		}
		break;
	default:
		respond_r1(sd, (uint8_t)(r1 | 0x04));	/* illegal command */
		break;
	}
}

uint8_t msim_sd_transfer(struct msim_sd *sd, uint8_t in)
{
	uint8_t out = 0xff;

	if (sd == NULL) {
		return 0xff;
	}
	switch (sd->state) {
	case S_CMD:
		if (sd->cmdlen == 0 && (in & 0xc0) != 0x40) {
			break;			/* not a command's first byte */
		}
		sd->cmd[sd->cmdlen++] = in;
		if (sd->cmdlen == 6) {
			sd->cmdlen = 0;
			command(sd);
		}
		break;
	case S_RESP:
		out = sd->resp[sd->respi++];
		if (sd->respi == sd->resplen) {
			sd->resplen = 0;
			if (sd->busy < 0) {
				sd->state = S_WRITE_WAIT;
				sd->busy = 0;
			} else {
				sd->state = S_CMD;
			}
		}
		break;
	case S_WRITE_WAIT:
		if (in == 0xfe) {
			sd->state = S_WRITE_DATA;
			sd->datai = 0;
		}
		break;
	case S_WRITE_DATA:
		sd->data[sd->datai++] = in;
		if (sd->datai == BLOCK + 2) {
			fseek(sd->f, (long)sd->addr * BLOCK, SEEK_SET);
			fwrite(sd->data, 1, BLOCK, sd->f);
			fflush(sd->f);
			sd->state = S_BUSY;
			sd->accepted = 1;
			sd->busy = BUSY_BYTES;
		}
		break;
	case S_BUSY:
		if (sd->accepted != 0) {
			out = 0x05;		/* data accepted */
			sd->accepted = 0;
		} else if (sd->busy > 0) {
			out = 0x00;
			sd->busy--;
		} else {
			sd->state = S_CMD;
			sd->cmdlen = 0;
		}
		break;
	}
	return out;
}
