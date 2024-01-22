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

#ifndef SURGE_SRC_SURGE_XT_UTIL_LOCKFREESTACK_H
#define SURGE_SRC_SURGE_XT_UTIL_LOCKFREESTACK_H

#include "juce_core/juce_core.h"

#include <array>

template <typename T, int qSize = 4096> class LockFreeStack
{
  public:
    LockFreeStack() : af(qSize) {}
    bool push(const T &ad)
    {
        auto ret = false;
        int start1, size1, start2, size2;
        af.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0)
        {
            dq[start1] = ad;
            ret = true;
        }
        af.finishedWrite(size1 + size2);
        return ret;
    }
    bool pop(T &ad)
    {
        bool ret = false;
        int start1, size1, start2, size2;
        af.prepareToRead(1, start1, size1, start2, size2);
        if (size1 > 0)
        {
            ad = dq[start1];
            ret = true;
        }
        af.finishedRead(size1 + size2);
        return ret;
    }
    juce::AbstractFifo af;
    std::array<T, qSize> dq;
};

#endif // SURGE_XT_LOCKFREESTACK_H
