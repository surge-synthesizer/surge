/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_JUCEHELPERS_H
#define SURGE_XT_JUCEHELPERS_H

#include <JuceHeader.h>

namespace Surge
{
namespace GUI
{
struct WheelAccumulationHelper
{
    float accum{0};

    int accumulate(const juce::MouseWheelDetails &wheel, bool X = true, bool Y = true)
    {
        accum += X * wheel.deltaX - (wheel.isReversed ? 1 : -1) * Y * wheel.deltaY;
        // This is calibrated to be reasonable on a Mac but I'm still not sure of the units
        // On my MBP I get deltas which are 0.0019 all the time.

        float accumLimit = 0.08;

        if (accum > accumLimit || accum < -accumLimit)
        {
            int mul = -1;

            if (accum > 0)
            {
                mul = 1;
            }

            accum = 0;

            return mul;
        }

        return 0;
    }
};
} // namespace GUI
} // namespace Surge

#endif // SURGE_XT_JUCEHELPERS_H
