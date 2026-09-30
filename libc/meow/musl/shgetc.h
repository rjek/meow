/* musl's floatscan reads its characters through shgetc and friends, which
 * in musl work on a FILE or on a string made up as one.  Here there are
 * only strings, PDCLib's scanf having no floating conversions to feed, so
 * for floatscan.c and common/strtod.c, which are all that include this,
 * FILE is a place in a string.  As in musl, reading goes on past the
 * string's end, returning the null there, and everything read can be put
 * back; shlim(f, 0) starts the count of characters read afresh, which is
 * how floatscan says that nothing was converted. */
#ifndef SHGETC_H
#define SHGETC_H

#include <stdio.h>

struct _sh_string {
    const unsigned char *rpos;  /* the next character */
    const unsigned char *from;  /* where shcnt counts from */
};
#define FILE struct _sh_string

#define sh_fromstring(f, s) ((f)->rpos = (const unsigned char *)(s))
#define shlim(f, lim) ((void)((f)->from = (f)->rpos))
#define shcnt(f) ((f)->rpos - (f)->from)
#define shgetc(f) (*(f)->rpos++)
#define shunget(f) ((void)(f)->rpos--)

#endif
