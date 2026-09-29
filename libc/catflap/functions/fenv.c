/* <fenv.h> for MEOW: rounding is always to nearest and no exception is
   ever raised, so every call is a formality.
   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/
#include <fenv.h>

int feclearexcept( int excepts ) { ( void )excepts; return 0; }
int fegetexceptflag( fexcept_t * flagp, int excepts ) { ( void )excepts; *flagp = 0; return 0; }
int feraiseexcept( int excepts ) { ( void )excepts; return 0; }
int fesetexceptflag( const fexcept_t * flagp, int excepts ) { ( void )flagp; ( void )excepts; return 0; }
int fetestexcept( int excepts ) { ( void )excepts; return 0; }
int fegetround( void ) { return FE_TONEAREST; }
int fesetround( int round ) { return round == FE_TONEAREST ? 0 : -1; }
int fegetenv( fenv_t * envp ) { envp->__cw = 0; return 0; }
int feholdexcept( fenv_t * envp ) { envp->__cw = 0; return 0; }
int fesetenv( const fenv_t * envp ) { ( void )envp; return 0; }
int feupdateenv( const fenv_t * envp ) { ( void )envp; return 0; }
