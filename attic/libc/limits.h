/*
 * limits.h
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

#ifndef _LIMITS_H
#define _LIMITS_H

#define CHAR_BIT	8
#define SCHAR_MIN	(-127)
#define SCHAR_MAX	127
#define UCHAR_MAX	255
#define CHAR_MIN	0
#define CHAR_MAX	255

#define SHRT_MIN	(-32767)
#define SHRT_MAX	32767
#define USHRT_MAX	65535


#define INT_MIN		-2147483647
#define INT_MAX		2147483647
#define UINT_MAX	4294967295U

#define LONG_MIN	-2147483647
#define LONG_MAX	2147483647
#define ULONG_MAX	4294967295U

#endif

