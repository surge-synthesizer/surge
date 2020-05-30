#pragma once

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
   auto numerator = -x * (-11511339840 + x2 * (1640635920 + x2 * (-52785432 + x2 * 479249)));
   auto denominator = 11511339840 + x2 * (277920720 + x2 * (3177720 + x2 * 18361));
   return numerator / denominator;
}

// JUCE6 Pade approximation of cos valid from -PI to PI with max error of 1e-5 and average error of 5e-7
inline float fastcos( float x ) noexcept
{
   auto x2 = x * x;
   auto numerator = -(-39251520 + x2 * (18471600 + x2 * (-1075032 + 14615 * x2)));
   auto denominator = 39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
   return numerator / denominator;
}

}
}
