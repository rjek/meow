/*
 * ctype.c
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
 
#include "ctype.h"

#define ALPHA _CTYPE_ALPHA
#define CNTRL _CTYPE_CNTRL
#define DIGIT _CTYPE_DIGIT
/* GRAPH is unsupported */
#define LOWER _CTYPE_LOWER
#define PRINT _CTYPE_PRINT
/* PUNCT is a special-case function */
#define SPACE _CTYPE_SPACE
#define UPPER _CTYPE_UPPER
#define XDIGIT _CTYPE_XDIGIT

int isalnum(int c)
{
	return isalpha(c) || isdigit(c);
}

int ispunct(int c)
{
	return isprint(c) && !isalpha(c) && !isdigit(c);
}

const char _ctype_table[] = {
	/* 0 - 31, control codes */
	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
	CNTRL | PRINT | SPACE, /* h tab */
	CNTRL | PRINT | SPACE, /* new line */
	CNTRL | PRINT | SPACE, /* v tab */
	CNTRL | PRINT | SPACE, /* form feed */
	CNTRL | PRINT | SPACE, /* carriage return */
	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
	CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL, CNTRL,
	CNTRL,
	
	/* 32 - 47, space and punctuation */
	SPACE | PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
	PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
	
	/* 48 - 57, numbers 0 - 9 */
	PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT,
	PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT, PRINT | DIGIT, 
	PRINT | DIGIT, PRINT | DIGIT,
	
	/* 58 - 64, :;<=>?@ */
	PRINT, PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
	
	/* 65 - 90, upper-case alphabet */
	PRINT | ALPHA | UPPER | XDIGIT, PRINT | ALPHA | UPPER | XDIGIT,
	PRINT | ALPHA | UPPER | XDIGIT, PRINT | ALPHA | UPPER | XDIGIT,
	PRINT | ALPHA | UPPER | XDIGIT, PRINT | ALPHA | UPPER | XDIGIT, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER, 
	PRINT | ALPHA | UPPER, PRINT | ALPHA | UPPER,
	
	/* 91 - 96, misc punctuation */
	PRINT, PRINT, PRINT, PRINT, PRINT, PRINT,
	
	/* 97 - 122, lower-case alphabet */
	PRINT | ALPHA | LOWER | XDIGIT, PRINT | ALPHA | LOWER | XDIGIT,
	PRINT | ALPHA | LOWER | XDIGIT, PRINT | ALPHA | LOWER | XDIGIT,
	PRINT | ALPHA | LOWER | XDIGIT, PRINT | ALPHA | LOWER | XDIGIT, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER, 
	PRINT | ALPHA | LOWER, PRINT | ALPHA | LOWER,
	
	/* 123 - 126, { | } ~ */
	PRINT, PRINT, PRINT, PRINT, PRINT,
	
	/* 127, delete */
	CNTRL
};

