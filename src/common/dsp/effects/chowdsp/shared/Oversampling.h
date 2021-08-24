/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once
#include <algorithm>
#include <memory>
#include <vembertech/basic_dsp.h>
#include <vembertech/halfratefilter.h>

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
    std::unique_ptr<HalfRateFilter> hr_filts_up alignas(16)[OSFactor];
    std::unique_ptr<HalfRateFilter> hr_filts_down alignas(16)[OSFactor];

    static constexpr size_t osRatio = 1 << OSFactor;
    static constexpr size_t up_block_size = block_size * osRatio;
    static constexpr size_t block_size_quad = block_size / 4;

  public:
    Oversampling()
    {
        for (size_t i = 0; i < OSFactor; ++i)
        {
            hr_filts_up[i] = std::make_unique<HalfRateFilter>(FilterOrd, steep);
            hr_filts_down[i] = std::make_unique<HalfRateFilter>(FilterOrd, steep);
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
        copy_block(leftIn, leftUp, block_size_quad);
        copy_block(rightIn, rightUp, block_size_quad);

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

        copy_block(leftUp, leftOut, block_size_quad);
        copy_block(rightUp, rightOut, block_size_quad);
    }

    /** Returns the size of the upsampled blocks */
    static inline constexpr size_t getUpBlockSize() { return up_block_size; }

    /** Returns the oversampling ratio */
    inline constexpr size_t getOSRatio() const noexcept { return osRatio; }

    float leftUp alignas(16)[up_block_size];
    float rightUp alignas(16)[up_block_size];
};

} // namespace chowdsp
