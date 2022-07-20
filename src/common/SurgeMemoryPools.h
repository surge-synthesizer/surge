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

#ifndef SURGE_SURGEMEMORYPOOLS_H
#define SURGE_SURGEMEMORYPOOLS_H

#include "SurgeStorage.h"
#include "MemoryPool.h"
#include "SSESincDelayLine.h"

namespace Surge
{
namespace Memory
{
struct SurgeMemoryPools
{
    SurgeMemoryPools(SurgeStorage *s) : stringDelayLines(s->sinctable) {}

    /*
     * The largest number of oscillator instances of a particular
     * type are scenes * oscs * max voices, but add some pad
     */
    static constexpr int maxosc = n_scenes * n_oscs * (MAX_VOICES + 8);

    /*
     * The string needs 2 delay lines per oscillator
     */
    MemoryPool<SSESincDelayLine<16384>, 8, 4, 2 * maxosc + 100> stringDelayLines;
    void resetAllPools(SurgeStorage *storage) { resetOscillatorPools(storage); }
    void resetOscillatorPools(SurgeStorage *storage)
    {
        bool hasString{false}, hasTwist{false};
        int nString{0};
        for (int s = 0; s < n_scenes; ++s)
        {
            for (int os = 0; os < n_oscs; ++os)
            {
                auto ot = storage->getPatch().scene[s].osc[os].type.val.i;

                if (ot == ot_string)
                {
                    hasString = true;
                    nString++;
                }
                hasTwist |= (ot == ot_twist);
            }
        }

        if (hasString)
        {
            int maxUsed = nString * 2 * storage->getPatch().polylimit.val.i;
            stringDelayLines.setupPoolToSize((int)(maxUsed * 0.5), storage->sinctable);
        }
        else
        {
            stringDelayLines.returnToPreAllocSize();
        }
    }
};

} // namespace Memory
} // namespace Surge
#endif // SURGE_SURGEMEMORYPOOLS_H
