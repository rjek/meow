/*
 * string.h
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

#ifndef _STRING_H
#define _STRING_H

#include "stdlib.h"

extern void *memcpy(void *dest, const void *src, size_t n);
extern void *memmove(void *dest, const void *src, size_t n);
extern void *memchr(const void *s, int c, size_t n);
//extern int memcmp(const void *s1, const void *s2, size_t n);
//extern void *memset(void *, int, size_t);

extern char *strcat(char *dest, const char *src);
//extern char *strncat(char *, const char *, size_t);
extern char *strchr(const char *s, int c);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, size_t n);
//extern int strcoll(const char *, const char *);
extern char *strcpy(char *dest, const char *src);
extern char *strncpy(char *dest, const char *src, size_t n);
//extern char *strerror(int);
extern size_t strlen(const char *s);
//extern size_t strspn(const char *s, const char *accept);
//extern size_t strcspn(const char *s, const char *reject);
//extern char *strpbrk(const char *s, const char *accept);
//extern char *strstr(const char *haystack, const char *needle);
//extern char *strtok(char *, const char *);
//extern size_t strxfrm(char *dest, const char *src, size_t n);

//extern char *strdup(const char *);

#endif
