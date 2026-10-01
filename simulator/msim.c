/*
 * msim.c
 * This file is part of MSIM, a MEOW Simulator
 *
 * Copyright (C) 2007 - Rob Kendrick <rjek@rjek.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to
 * do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>

#include "config.h"

#include "msim_core.h"
#include "msim_chairman.h"
#include "msim_hostfs.h"
#include "msim_ioc.h"

static inline void chomp(char *c)
{
	while(c[strlen(c) - 1] == '\n' || c[strlen(c) - 1] == '\r')
		c[strlen(c) - 1] = '\0';
}

static bool parse_chip_ram(struct msim_ctx *ctx, int lino, int chip)
{
	char *b = strtok(NULL, " \t");
	size_t size = 128*1024*1024;	/* default to maximum size */
	
	if (b != NULL)
		size = strtol(b, NULL, 0);
	
	msim_add_ram(ctx, chip, size);
	
	return true;
}

static bool parse_chip_rom(struct msim_ctx *ctx, int lino, int chip)
{
	char *b = strtok(NULL, " \t");
	FILE *f;
	
	if (b == NULL) {
		printf("%d: no ROM file name specified\n", lino);
		return false;
	}
	
	chomp(b);
	
	f = fopen(b, "r");
	if (f == NULL) {
		printf("%d: unable to open ROM file '%s'\n", lino, b);
		return false;
	}
	fclose(f);
	
	msim_add_rom_from_file(ctx, chip, b);
	
	return true;
}

static bool parse_chip_sys(struct msim_ctx *ctx, int lino, int chip)
{
	msim_add_sys(ctx, chip);
	
	return true;
}

static int ioc_area = -1;

static bool parse_chip_ioc(struct msim_ctx *ctx, int lino, int chip)
{
	msim_add_ioc(ctx, chip);
	ioc_area = chip;
	
	return true;
}

static bool parse_chip(struct msim_ctx *ctx, char *line, int lino)
{
	char *b;
	int chip;
	
	b = strtok(NULL, " \t");
	
	if (b == NULL) {
		printf("%d: no chip number specified\n", lino);
		return false;
	}
	
	chip = atoi(b);
	
	b = strtok(NULL, " \t");
	chomp(b);
	
	if (b == NULL) {
		printf("%d: no chip type specified\n", lino);
		return false;
	}
	
	if (!strcmp(b, "ram"))
		return parse_chip_ram(ctx, lino, chip);
	
	if (!strcmp(b, "rom"))
		return parse_chip_rom(ctx, lino, chip);
		
	if (!strcmp(b, "sys"))
		return parse_chip_sys(ctx, lino, chip);
		
	if (!strcmp(b, "ioc"))
		return parse_chip_ioc(ctx, lino, chip);
		
	printf("%d: unknown chip type '%s' specified\n", lino, b);
	return false;
}

static bool parse_spec(struct msim_ctx *ctx, const char *specfile)
{
	FILE *f = fopen(specfile, "r");
	char line[255];
	int lino = 0;
	
	if (f == NULL) {
		printf("Unable to open %s for reading.\n", specfile);
		return false;
	}
	
	while (!feof(f)) {
		if(fgets(line, 255, f) != NULL) {
			char *token;
			lino++;
			
			if (line[0] == ';') continue;
			token = strtok(line, " \t");
			if (token[0] == '\0') continue;
			
			if (!strcmp(token, "chip"))
				if (parse_chip(ctx, line, lino) == false)
					return false;
		}
	}
	
	return true;
}

static void display_help(const char *argv0)
{
	printf("Usage: %s [-vhiqs] {-f spec file | -r rom} [-c cycles]\n", argv0);
	printf("  -r rom   ROM image at chip select 0, RAM at 1, IOC at 2, Chairman at 31\n");
	printf("  -m KB    size of that RAM, default 64\n");
	printf("  -n CPUs  how many CPUs, default 1; the others start held in reset\n");
	printf("  -l KB    local memory per CPU at chip selects 29 and 30, default none\n");
	printf("  -j seed  stall CPUs at random, from the seed, to vary their interleaving\n");
	printf("  -G n,baud decode a software UART sent on IOC GPIO line n at baud\n");
	printf("  -D image an SD card on the IOC's SPI master, holding the image file\n");
	printf("  -T file  write a line per instruction CPU 0 executes, for the hardware's testbench\n");
	printf("  -H dir   lend a host directory to the program through BNV #-18\n");
	printf("  -q       no banner\n");
	printf("  -s       report the instructions executed on exit\n");
	printf("  -P file  write an execution count for every address run to file\n");
}

static struct msim_ctx *stats_ctx;
static const char *profile_file;

static void report_stats(void)
{
	unsigned int n, total = 0;

	for (n = 0; n < stats_ctx->ncpus; n++) {
		total += msim_cpu(stats_ctx, n)->cyclecount;
	}
	fprintf(stderr, "msim: %u instructions\n", total);
}

static void write_profile(void)
{
	FILE *f = fopen(profile_file, "w");
	unsigned int a;

	if (f == NULL) {
		fprintf(stderr, "msim: cannot write %s\n", profile_file);
		return;
	}
	for (a = 0; a < MSIM_PROFILE_BYTES / 2; a++) {
		if (stats_ctx->profile[a] != 0) {
			fprintf(f, "%08x %u\n", a * 2, stats_ctx->profile[a]);
		}
	}
	fclose(f);
}

int main(int argc, char *argv[])
{
	static char optstring[] = "vhiqsf:r:c:P:m:H:n:l:j:G:D:T:";
	int optch, cycles = 0;
	bool verbose = false, interactive = false, opterr = false;
	bool quiet = false, stats = false;
	char *specfile = NULL;
	char *romfile = NULL;
	char *hostdir = NULL;
	size_t ramsize = 65536;
	size_t localsize = 0;
	unsigned int ncpus = 1, jitter = 0, n;
	int decode_line = -1;
	unsigned decode_baud = 0;
	const char *sd_image = NULL;
	const char *trace_file = NULL;
	struct msim_ctx *ctx;
	
	while ((optch = getopt(argc, argv, optstring)) != -1) {
		switch(optch) {
		default:
			opterr = true;
		case 'h':
			display_help(argv[0]);
			exit(0);
			break;
			
		case 'v':
			verbose = true;
			break;
			
		case 'f':
			specfile = malloc(strlen(optarg));
			strcpy(specfile, optarg);
			break;
		
		case 'c':
			cycles = atoi(optarg);
			break;
			
		case 'i':
			interactive = true;
			break;
		case 'q':
			quiet = true;
			break;
		case 's':
			stats = true;
			break;
		case 'P':
			profile_file = optarg;
			break;
		case 'r':
			romfile = optarg;
			break;
		case 'H':
			hostdir = optarg;
			break;
		case 'm':
			ramsize = (size_t)atoi(optarg) * 1024;
			break;
		case 'n':
			ncpus = (unsigned int)atoi(optarg);
			if (ncpus < 1 || ncpus > 32) {
				fprintf(stderr, "msim: 1 to 32 CPUs\n");
				opterr = true;
			}
			break;
		case 'l':
			localsize = (size_t)atoi(optarg) * 1024;
			break;
		case 'j':
			jitter = (unsigned int)atoi(optarg);
			break;
		case 'D':
			sd_image = optarg;
			break;
		case 'T':
			trace_file = optarg;
			break;
		case 'G':
			if (sscanf(optarg, "%d,%u", &decode_line, &decode_baud) != 2 ||
			    decode_line < 0 || decode_line > 31 || decode_baud == 0) {
				fprintf(stderr, "msim: -G wants line,baud\n");
				opterr = true;
			}
			break;
		}
	}
	
	if (specfile == NULL && romfile == NULL) {
		printf("no specfile specified\n");
		display_help(argv[0]);
		opterr = true;
	}
	
	if (opterr == true)
		exit(1);
		
	ctx = msim_init();
	stats_ctx = ctx;
	if (stats == true) {
		atexit(report_stats);	/* the halt call exits from inside the core */
	}
	if (profile_file != NULL) {
		ctx->profile = calloc(MSIM_PROFILE_BYTES / 2, sizeof *ctx->profile);
		atexit(write_profile);
	}
	
	if (romfile != NULL) {
		FILE *f = fopen(romfile, "r");

		if (f == NULL) {
			fprintf(stderr, "msim: cannot open ROM %s\n", romfile);
			exit(1);
		}
		fclose(f);
		msim_add_rom_from_file(ctx, 0, romfile);
		msim_add_ram(ctx, 1, ramsize);
		msim_add_sys(ctx, 31);
		msim_add_ioc(ctx, 2);	/* after the Chairman: it takes the console */
		ioc_area = 2;
	} else if (parse_spec(ctx, specfile) == false) {
		msim_destroy(ctx);
		exit(2);
	}
	
	msim_add_hostfs(ctx, hostdir);
	if (decode_line >= 0) {
		if (ioc_area < 0) {
			fprintf(stderr, "msim: -G needs an IOC\n");
			exit(1);
		}
		msim_ioc_decode(ctx, ioc_area, decode_line, decode_baud);
	}
	if (sd_image != NULL) {
		if (ioc_area < 0) {
			fprintf(stderr, "msim: -D needs an IOC\n");
			exit(1);
		}
		if (msim_ioc_sd(ctx, ioc_area, sd_image) < 0) {
			exit(1);
		}
	}
	if (localsize != 0) {
		msim_add_local(ctx, localsize);
	}
	if (trace_file != NULL) {
		ctx->trace = fopen(trace_file, "w");
		if (ctx->trace == NULL) {
			fprintf(stderr, "msim: cannot write %s\n", trace_file);
			exit(1);
		}
	}
	msim_add_cpus(ctx, ncpus);	/* last: the others copy the devices */
	ctx->jitter = jitter;

	if (quiet == false) {
		printf("msim %s - The MEOW Simulator and Debugger\n", MSIM_VERSION);
		printf("Copyright (c) 2006-2007 Rob Kendrick\n\n");
	}
	
	if (interactive == true) {
		msim_debugger(ctx);
	} else {
		if (cycles == 0) {
			if (verbose) {
				while (true) {
					msim_run(ctx, 1, true);
					for (n = 0; n < ctx->ncpus; n++) {
						if (msim_cpu(ctx, n)->running == true) {
							msim_print_state(msim_cpu(ctx, n));
						}
					}
				}
			} else {
				while (true) {
					msim_run(ctx, 1000, false);
				}
			}
		}
	
		if (verbose) {
			for (; cycles > 0; cycles--) {
				msim_run(ctx, 1, true);
				msim_print_state(ctx);
			}
		} else {
			msim_run(ctx, cycles, false);
		}
		fprintf(stderr, "msim: stopped after %d cycles\n", cycles);
		exit(3);
	}
	
	msim_destroy(ctx);
	
	exit(0);
}
