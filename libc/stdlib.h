/*
 * stdlib.h
 * This file is part of MEOW libc, a compact, non-complete C standard library
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

#ifndef _STDLIB_H
#define _STDLIB_H

#define NULL ((void *)0)
typedef int size_t;

typedef struct {
	int quot, rem;
} div_t;

typedef struct {
	long int quot, rem;
} ldiv_t;

//double atof(const char *s);
//int atoi(const char *s);
//long atol(const char *s);
//double strtod(const char *s, char **ends);

int rand(void);
int rand_r(unsigned int *seedp);
void strand(unsigned int seed);

//void *malloc(size_t size);
//void *calloc(size_t nmemb, size_t size);
//void free(void *ptr);
//void *realloc(void *ptr, size_t size);

//void abort(void);
void atexit(void (*func)(void));
void exit(int status);
char *getenv(const char *name);
int system(const char *com);

//void *bsearch(const void *key, const void *base, size_t nmemb, size_t size,
//		int (*compare)(const void *, const void *));
//void qsort(void *base, size_t nmemb, size_t size,
//		int (*compare)(const void *, const void *));
//

int abs(int i);
long int labs(long int i);
//div_t div(int num, int den);
//ldiv_t ldiv(long num, long den);

#endif
