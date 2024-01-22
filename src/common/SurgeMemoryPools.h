/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_SURGEMEMORYPOOLS_H
#define SURGE_SRC_COMMON_SURGEMEMORYPOOLS_H

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
