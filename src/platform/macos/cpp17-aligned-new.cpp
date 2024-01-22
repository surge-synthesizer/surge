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

#include <cstdlib>
#include <new>

/*
 * Since C++17, new'ing objects respects their alignas() requirements. If the required alignment
 * exceeds that provided by operator new, alignment-aware overloads are called instead. On macOS,
 * these are only present on 10.14 "Mojave" and up, so we ship our own implementation.
 *
 * Relevant discussion in GitHub issues #171, #188, #3589
 */

void *operator new(std::size_t size, std::align_val_t align)
{
    void *result;
    if (posix_memalign(&result, std::size_t(align), size))
        throw std::bad_alloc{};
    return result;
}

void operator delete(void *p, std::align_val_t) noexcept { free(p); }
