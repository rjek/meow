/* What musl's fma() takes from its internal <atomic.h>: a leading-zero
   count, from musl's own generic version (MIT, see ../../musl/COPYRIGHT). */
#ifndef _MEOW_MUSL_ATOMIC_H
#define _MEOW_MUSL_ATOMIC_H
#include <stdint.h>

static inline int a_clz_64(uint64_t x)
{
	uint32_t y;
	int r;
	if (x>>32) y=x>>32, r=0; else y=x, r=32;
	if (y>>16) y>>=16; else r |= 16;
	if (y>>8) y>>=8; else r |= 8;
	if (y>>4) y>>=4; else r |= 4;
	if (y>>2) y>>=2; else r |= 2;
	return r | !(y>>1);
}
#endif
