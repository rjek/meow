/* The time zone state and the static struct tm, kept out of
   _PDCLIB_stdinit.c so that a program using stdio but not time does not
   carry them: the two states are the largest objects in the library.
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <time.h>
#include "pdclib/_PDCLIB_tzcode.h"

struct state _PDCLIB_lclmem;
struct state _PDCLIB_gmtmem;
struct tm _PDCLIB_tm;
