#pragma once

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
