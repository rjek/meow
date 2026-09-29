/*
 * ctype.h
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

#ifndef _CTYPE_H
#define _CTYPE_H

#define _CTYPE_ALPHA 1<<0
#define _CTYPE_CNTRL 1<<1
#define _CTYPE_DIGIT 1<<2
/* GRAPH is unsupported */
#define _CTYPE_LOWER 1<<3
#define _CTYPE_PRINT 1<<4
/* PUNCT is a special-case macro */
#define _CTYPE_SPACE 1<<5
#define _CTYPE_UPPER 1<<6
#define _CTYPE_XDIGIT 1<<7

extern const char _ctype_table[];
extern int isalnum(int c);
extern int ispunct(int c);

#define isalpha(c) (_ctype_table[c] & _CTYPE_ALPHA)
#define iscntrl(c) (_ctype_table[c] & _CTYPE_CNTRL)
#define isdigit(c) (_ctype_table[c] & _CTYPE_DIGIT)
#define islower(c) (_ctype_table[c] & _CTYPE_LOWER)
#define isprint(c) (_ctype_table[c] & _CTYPE_PRINT)
#define isspace(c) (_ctype_table[c] & _CTYPE_SPACE)
#define isupper(c) (_ctype_table[c] & _CTYPE_UPPER)
#define isxdigit(c) (_ctype_table[c] & _CTYPE_XDIGIT)

#endif

