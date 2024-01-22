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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_TONECONTROL_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_TONECONTROL_H

#include "../shared/SmoothedValue.h"
#include "../shared/Shelf.h"

namespace chowdsp
{

struct ToneStage
{
    using SmoothGain = SmoothedValue<float, ValueSmoothingTypes::Multiplicative>;
    ShelfFilter<float> tone[2];
    SmoothGain lowGain[2], highGain[2], tFreq[2];
    float fs = 44100.0f;

    ToneStage();

    void prepare(double sampleRate);
    void processBlock(float *dataL, float *dataR);
    void setLowGain(float lowGainDB);
    void setHighGain(float highGainDB);
    void setTransFreq(float newTFreq);
};

class ToneControl
{
  public:
    ToneControl() = default;

    void set_params(float tone);
    void prepare(double sampleRate);

    void processBlockIn(float *dataL, float *dataR);
    void processBlockOut(float *dataL, float *dataR);

  private:
    ToneStage toneIn, toneOut;
    static constexpr float dbScale = 18.0f;
    float bass, treble;
};

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_TAPE_TONECONTROL_H
