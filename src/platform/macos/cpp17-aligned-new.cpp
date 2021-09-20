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
