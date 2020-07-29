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

}
}
