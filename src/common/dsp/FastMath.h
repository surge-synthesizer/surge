#pragma once

#include <cmath>

/*
** Fast Math Approximations to various Functions
**
** Documentation on each one
*/


/*
** These are directly copied from JUCE6 modules/juce_dsp/mathos/juce_FastMathApproximations.h
**
** Since JUCE6 is GPL3 and Surge is GPL3 that copy is fine, but I did want to explicitly
** acknowledge it
*/

namespace Surge
{
namespace DSP
{

// JUCE6 Pade approximation of sin valid from -PI to PI with max error of 1e-5 and average error of 5e-7
inline float fastsin( float x ) noexcept
{
   auto x2 = x * x;
   auto numerator = -x * (-(float)11511339840 + x2 * ((float)1640635920 + x2 * (-(float)52785432 + x2 * (float)479249)));
   auto denominator = (float)11511339840 + x2 * ((float)277920720 + x2 * ((float)3177720 + x2 * (float)18361));
   return numerator / denominator;
}

// JUCE6 Pade approximation of cos valid from -PI to PI with max error of 1e-5 and average error of 5e-7
inline float fastcos( float x ) noexcept
{
   auto x2 = x * x;
   auto numerator = -(-(float)39251520 + x2 * ((float)18471600 + x2 * (-1075032 + 14615 * x2)));
   auto denominator = (float)39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
   return numerator / denominator;
}

/*
** Push x into -PI, PI periodically. There is probably a faster way to do this
*/
inline float clampToPiRange( float x )
{
    if( x <= M_PI && x >= -M_PI ) return x;
    float y = x + M_PI; // so now I am 0,2PI

    // float p = fmod( y, 2.0 * M_PI );

    constexpr float oo2p = 1.0 / ( 2.0 * M_PI );
    float p = y - 2.0 * M_PI * (int)( y * oo2p );

    if( p < 0 )
        p += 2.0 * M_PI;
    return p - M_PI;
}

inline float fasttanh (float x) noexcept
{
   auto x2 = x * x;
   auto numerator = x * (135135 + x2 * (17325 + x2 * (378 + x2)));
   auto denominator = 135135 + x2 * (62370 + x2 * (3150 + 28 * x2));
   return numerator / denominator;
}

inline __m128 fasttanhSSE( __m128 x )
{
   static const __m128
      m135135 = _mm_set_ps1( 135135 ),
      m17325 = _mm_set_ps1( 17325 ),
      m378 = _mm_set_ps1( 378 ),
      m62370 = _mm_set_ps1( 62370 ),
      m3150 = _mm_set_ps1( 3150 ),
      m28 = _mm_set_ps1( 28 );

#define M(a,b) _mm_mul_ps( a, b )
#define A(a,b) _mm_add_ps( a, b )
   
   auto x2 = M(x,x);
   auto num = M(x, A( m135135, M( x2, A( m17325, M( x2, A( m378, x2 ) ) ) ) ) );
   auto den = A( m135135, M( x2, A( m62370, M( x2, A( m3150, M( m28, x2 ) ) ) ) ) );

#undef M
#undef A
   
   return _mm_div_ps( num, den );
}

inline __m128 fasttanhSSEclamped( __m128 x )
{
   auto xc = _mm_min_ps( _mm_set_ps1( 5 ), _mm_max_ps( _mm_set_ps1( -5 ), x ) );
   return fasttanhSSE(xc);
}

}
}
