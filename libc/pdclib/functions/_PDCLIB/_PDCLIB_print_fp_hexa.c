/* _PDCLIB_print_fp_hexa( _PDCLIB_bigint_t *, struct _PDCLIB_status_t *, char )

   This file is part of the Public Domain C Library (PDCLib).
   Permission is granted to use, modify, and / or redistribute at will.
*/

#ifndef REGTEST

#include "pdclib/_PDCLIB_print.h"

#include <float.h>
#include <stdbool.h>
#include <stdlib.h>

/* MEOW: the rest of this file is rewritten.  Upstream lost the first
   digit after the point and repeated the second, rounded at 5 as if the
   digits were decimal, read digits it had not written when the precision
   asked for more than there were, and gave zero no digits at all.
*/

static void round_up( char * buffer, size_t index )
{
    /* The leading digit is 0 or 1, so the carry stops there at the latest */
    while ( buffer[ index ] == '\017' )
    {
        buffer[ index-- ] = '\0';
    }

    ++buffer[ index ];
}

/* Round the digits to buffer[ 0 ] .. buffer[ prec ], given that one of
   those after them is not zero.
*/
static void round( char * buffer, size_t last_non_zero, size_t prec, char sign )
{
    switch ( FLT_ROUNDS )
    {
        case 0: /*FE_TOWARDZERO*/
            break;
        default: /*FE_TONEAREST*/
        case 1:
            if ( buffer[ prec + 1 ] > '\010' )
            {
                round_up( buffer, prec );
            }
            else if ( buffer[ prec + 1 ] == '\010' )
            {
                if ( last_non_zero > ( prec + 1 ) || ( buffer[ prec ] % 2 ) )
                {
                    round_up( buffer, prec );
                }
            }
            break;
        case 2: /*FE_UPWARD*/
            if ( sign != '-' )
            {
               round_up( buffer, prec );
            }
            break;
        case 3: /*FE_DOWNWARD*/
            if ( sign == '-' )
            {
                round_up( buffer, prec );
            }
            break;
    }
}

static char * print_exp( char * buffer, int exp )
{
    div_t dv = div( exp, 10 );

    if ( dv.quot > 0 )
    {
        buffer = print_exp( buffer, dv.quot );
    }

    *buffer = _PDCLIB_digits[ dv.rem ];
    return ++buffer;
}

/* The digits of the mantissa, as values 0 to 15, the one before the point
   first.  Returns how many are to be printed: those the precision asks
   for, or without one, all up to the last that is not zero.
*/
static size_t print_mant( _PDCLIB_fp_t * fp, struct _PDCLIB_status_t * status, char * buffer )
{
    size_t i;
    size_t count;
    size_t last_non_zero = 0;
    size_t mant_dig = ( ( status->flags & E_ldouble ) ? _PDCLIB_LDBL_MANT_DIG : _PDCLIB_DBL_MANT_DIG ) - 1;
    size_t log2 = _PDCLIB_bigint_log2( &fp->mantissa );
    char * bufend = buffer;

    if ( ( mant_dig % 4 ) > 0 )
    {
        /* alignment */
        int shift = 4 - ( mant_dig % 4 );
        _PDCLIB_bigint_shl( &fp->mantissa, shift );
        mant_dig += shift;
        log2 += shift;
    }

    if ( mant_dig > log2 )
    {
        /* subnormal, leading zeroes */
        for ( i = 0; i <= ( mant_dig - log2 - 1 ) / 4; ++i )
        {
            *bufend++ = '\0';
        }
    }

    for ( i = log2 / 4 + 1; i > 0; --i, ++bufend )
    {
        /* data nibbles */
        div_t dv = div( i - 1, _PDCLIB_BIGINT_DIGIT_BITS / 4 );

        if ( ( *bufend = ( fp->mantissa.data[ dv.quot ] >> ( dv.rem * 4 ) ) & 0xfu ) > 0 )
        {
            last_non_zero = bufend - buffer;
        }
    }

    count = bufend - buffer;

    if ( status->prec < 0 )
    {
        /* no precision given */
        return last_non_zero + 1;
    }

    if ( (size_t)status->prec + 1 < count )
    {
        count = status->prec + 1;

        if ( last_non_zero >= count )
        {
            round( buffer, last_non_zero, status->prec, fp->sign );
        }
    }

    return count;
}

void _PDCLIB_print_fp_hexa( _PDCLIB_fp_t * fp,
                            struct _PDCLIB_status_t * status,
                            char * buffer )
{
    _PDCLIB_static_assert( _PDCLIB_FLT_RADIX == 2, "Assuming 2-based Floating Point" );

    char const * digits = ( status->flags & E_lower ) ? _PDCLIB_digits : _PDCLIB_Xdigits;
    int exp = (_PDCLIB_bigint_sdigit_t)fp->exponent;
    char exp_buffer[ 8 ];
    char * exp_end = exp_buffer;
    size_t count;  /* digits of the mantissa in the buffer, never none */
    size_t zeroes; /* those the precision asks for beyond them */
    size_t length;
    size_t padding;
    size_t i;

    /* significant */
    if ( fp->mantissa.size == 0 )
    {
        buffer[ 0 ] = '\0';
        count = 1;
        exp = 0;
    }
    else
    {
        count = print_mant( fp, status, buffer );
    }

    zeroes = ( status->prec >= 0 && (size_t)status->prec + 1 > count ) ? ( status->prec + 1 - count ) : 0;

    /* exponent */
    *exp_end++ = ( status->flags & E_lower ) ? 'p' : 'P';

    if ( exp < 0 )
    {
        *exp_end++ = '-';
        exp *= -1;
    }
    else
    {
        *exp_end++ = '+';
    }

    exp_end = print_exp( exp_end, exp );

    /* output */
    length = ( ( fp->sign == '\0' ) ? 2 : 3 ) + count + zeroes + ( exp_end - exp_buffer );

    if ( count + zeroes > 1 || status->flags & E_alt )
    {
        ++length; /* TODO: decimal point */
    }

    padding = ( status->width > length ) ? ( status->width - length ) : 0;

    if ( ! ( status->flags & E_minus ) && ! ( status->flags & E_zero ) )
    {
        for ( i = 0; i < padding; ++i )
        {
            PUT( ' ' );
            status->current++;
        }
    }

    if ( fp->sign != '\0' )
    {
        PUT( fp->sign );
        status->current++;
    }

    PUT( '0' );
    PUT( ( status->flags & E_lower ) ? 'x' : 'X' );
    status->current += 2;

    if ( ! ( status->flags & E_minus ) && ( status->flags & E_zero ) )
    {
        for ( i = 0; i < padding; ++i )
        {
            PUT( '0' );
            status->current++;
        }
    }

    PUT( digits[ (size_t)buffer[ 0 ] ] );
    status->current++;

    if ( count + zeroes > 1 || status->flags & E_alt )
    {
        PUT( '.' );
        status->current++;
    }

    for ( i = 1; i < count; ++i )
    {
        PUT( digits[ (size_t)buffer[ i ] ] );
        status->current++;
    }

    for ( i = 0; i < zeroes; ++i )
    {
        PUT( '0' );
        status->current++;
    }

    for ( i = 0; exp_buffer + i < exp_end; ++i )
    {
        PUT( exp_buffer[ i ] );
        status->current++;
    }

    if ( status->flags & E_minus )
    {
        for ( i = 0; i < padding; ++i )
        {
            PUT( ' ' );
            status->current++;
        }
    }
}

#endif

#ifdef TEST

#include "_PDCLIB_test.h"

int main( int argc, char * argv[] )
{
    /* Tested by _PDCLIB_print testdriver */
#ifndef REGTEST
    char buffer[100];
    *(print_exp( buffer, 0 )) = '\0';
    TESTCASE( strcmp( buffer, "0" ) == 0 );

    *(print_exp( buffer, 9 )) = '\0';
    TESTCASE( strcmp( buffer, "9" ) == 0 );

    *(print_exp( buffer, 10 )) = '\0';
    TESTCASE( strcmp( buffer, "10" ) == 0 );

    *(print_exp( buffer, 100 )) = '\0';
    TESTCASE( strcmp( buffer, "100" ) == 0 );
#endif
    return TEST_RESULTS;
}

#endif
