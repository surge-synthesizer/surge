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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEPROCESSOR_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEPROCESSOR_H

#include "DegradeFilter.h"
#include "DegradeNoise.h"
#include <vembertech/lipol.h>

namespace chowdsp
{

class DegradeProcessor
{
  public:
    DegradeProcessor();

    void set_params(float depthParam, float amtParam, float varParam);
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void process_block(float *dataL, float *dataR);

  private:
    float depthParam;
    float amtParam;
    float varParam;

    DegradeNoise noiseProc[2];
    DegradeFilter filterProc[2];
    lipol_ps gain alignas(16);

    std::function<float()> urng; // A uniform -0.5,0.5 RNG

    float fs = 44100.0f;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_DEGRADEPROCESSOR_H
