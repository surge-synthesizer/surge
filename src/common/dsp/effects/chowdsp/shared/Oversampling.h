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

#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_OVERSAMPLING_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_OVERSAMPLING_H
#include <algorithm>
#include <memory>
#include <vembertech/basic_dsp.h>
#include <sst/filters/HalfRateFilter.h>
#include "sst/basic-blocks/mechanics/block-ops.h"

namespace chowdsp
{

/*
** Generalised oversampling class for use in effects processing.
** Uses anti-imaging filters for upsampling, and anti-aliasing
** filters for downsampling. Most of the oversampling parameters
** are set as template parameters:
**
** @param: OSFactor     sets the oversampling ratio as a power of two. i.e. Ratio = 2^OSFactor
** @param: block_size   size of the blocks of audio before upsampling
** @param: FilterOrd    sets the order of the anti-imaging/anti-aliasing filters
** @param: steep        sets whether to use the filters in "steep" mode (see
*vembertech/halfratefilter.h)
**
** The class should be used in the processign callback as follows:
** Then use the following code to process samples:
** @code
** Ovsampling<2, BLOCK_SIZE> os;
** void process(float* dataL, float* dataR)
** {
**     os.upsample(dataL, dataR);
**     process_samples(os.leftUp, os.rightUp, os.getUpBLockSize());
**     os.downsample(dataL, dataR);
** }
** @endcode
*/
template <size_t OSFactor, size_t block_size, size_t FilterOrd = 3, bool steep = false>
class Oversampling
{
    std::unique_ptr<sst::filters::HalfRate::HalfRateFilter> hr_filts_up alignas(16)[OSFactor];
    std::unique_ptr<sst::filters::HalfRate::HalfRateFilter> hr_filts_down alignas(16)[OSFactor];

    static constexpr size_t osRatio = 1 << OSFactor;
    static constexpr size_t up_block_size = block_size * osRatio;
    static constexpr size_t block_size_quad = block_size / 4;

  public:
    Oversampling()
    {
        for (size_t i = 0; i < OSFactor; ++i)
        {
            hr_filts_up[i] =
                std::make_unique<sst::filters::HalfRate::HalfRateFilter>(FilterOrd, steep);
            hr_filts_down[i] =
                std::make_unique<sst::filters::HalfRate::HalfRateFilter>(FilterOrd, steep);
        }
    }

    /** Resets the processing pipeline */
    void reset()
    {
        for (size_t i = 0; i < OSFactor; ++i)
        {
            hr_filts_up[i]->reset();
            hr_filts_down[i]->reset();
        }

        std::fill(leftUp, &leftUp[up_block_size], 0.0f);
        std::fill(rightUp, &rightUp[up_block_size], 0.0f);
    }

    /** Upsamples the audio in the input arrays, and stores the upsampled audio internally */
    inline void upsample(float *leftIn, float *rightIn) noexcept
    {
        sst::basic_blocks::mechanics::copy_from_to<block_size>(leftIn, leftUp);
        sst::basic_blocks::mechanics::copy_from_to<block_size>(rightIn, rightUp);

        for (size_t i = 0; i < OSFactor; ++i)
        {
            auto numSamples = block_size * (1 << (i + 1));
            hr_filts_up[i]->process_block_U2(leftUp, rightUp, leftUp, rightUp, numSamples);
        }
    }

    /** Downsamples that audio in the internal buffers, and stores the downsampled audio in the
     * input arrays */
    inline void downsample(float *leftOut, float *rightOut) noexcept
    {
        for (size_t i = OSFactor; i > 0; --i)
        {
            auto numSamples = block_size * (1 << i);
            hr_filts_down[i - 1]->process_block_D2(leftUp, rightUp, numSamples);
        }

        sst::basic_blocks::mechanics::copy_from_to<block_size>(leftUp, leftOut);
        sst::basic_blocks::mechanics::copy_from_to<block_size>(rightUp, rightOut);
    }

    /** Returns the size of the upsampled blocks */
    static inline constexpr size_t getUpBlockSize() { return up_block_size; }

    /** Returns the oversampling ratio */
    inline constexpr size_t getOSRatio() const noexcept { return osRatio; }

    float leftUp alignas(16)[up_block_size];
    float rightUp alignas(16)[up_block_size];
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_OVERSAMPLING_H
