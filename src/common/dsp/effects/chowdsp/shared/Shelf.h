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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_SHELF_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_SHELF_H

#include "BilinearUtils.h"
#include "chowdsp_IIR.h"

namespace chowdsp
{

/** A first order shelving filter, with a set gain at DC,
 * a set gain at high frequencies, and a transition frequency.
 */
template <typename T = float> class ShelfFilter : public IIRFilter<1, T>
{
  public:
    ShelfFilter() = default;

    /** Calculates the coefficients for the filter.
     * @param lowGain: the gain of the filter at low frequencies
     * @param highGain: the gain of the filter at high frequencies
     * @param fc: the transition frequency of the filter
     * @param fs: the sample rate for the filter
     *
     * For information on the filter coefficient derivation,
     * see Abel and Berners dsp4dae, pg. 249
     */
    void calcCoefs(T lowGain, T highGain, T fc, T fs)
    {
        // reduce to simple gain element
        if (lowGain == highGain)
        {
            this->b[0] = lowGain;
            this->b[1] = (T)0;
            this->a[0] = (T)1;
            this->a[1] = (T)0;
            return;
        }

        auto rho = std::sqrt(highGain / lowGain);
        auto K = (T)1 / std::tan(M_PI * fc / fs);

        T bs[2]{highGain / rho, lowGain};
        T as[2]{1.0f / rho, 1.0f};
        Bilinear::BilinearTransform<T, 2>::call(this->b, this->a, bs, as, K);
    }
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_SHELF_H
