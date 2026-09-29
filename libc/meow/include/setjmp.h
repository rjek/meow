/* <setjmp.h> for MEOW: PDCLib leaves it to the platform. */
#ifndef _SETJMP_H
#define _SETJMP_H

/* v1-v6, sp, lr */
typedef int jmp_buf[8];

int setjmp( jmp_buf env );
void longjmp( jmp_buf env, int val );

#endif
