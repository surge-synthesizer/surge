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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_LOSSFILTER_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_LOSSFILTER_H

#include "BiquadFilter.h"
#include "../shared/FIRFIlter.h"

namespace chowdsp
{

/*
** Filter to implement loss characteristics from
** the playhead of analog tape machine.
** Parameters include:
**   - Tape Speed (affects HF loss and head bump)
**   - Spacing between tape and head (affects HF loss)
**   - Gap in playhead (affects HF loss and head bump)
**   - Tape Thickness (affects HF loss)
*/
class LossFilter
{
  public:
    LossFilter(SurgeStorage *storage, size_t order = 64);

    void prepare(float sampleRate, int blockSize);
    void process(float *dataL, float *dataR);
    void set_params(float speed, float spacing, float gap, float thick);

  private:
    void calcCoefs();
    static void calcHeadBumpFilter(float speedIps, float gapMeters, float fs, BiquadFilter &filter);

    std::unique_ptr<FIRFilter> filters[2];
    BiquadFilter bumpFilter;

    // tools for smooth fading
    int activeFilter = 0;
    int fadeCount = 0;
    static constexpr int fadeLength = 1024;
    float fadeBufferL alignas(16)[BLOCK_SIZE], fadeBufferR alignas(16)[BLOCK_SIZE];

    // parameter values
    float curSpeed, prevSpeed;
    float curSpacing, prevSpacing;
    float curThick, prevThick;
    float curGap, prevGap;

    // filter design values
    float fs = 44100.0f;
    float fsFactor = 1.0f;
    float binWidth = fs = 100.0f;

    const int order;
    int curOrder;
    std::vector<float> currentCoefs;
    std::vector<float> HCoefs;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_LOSSFILTER_H
