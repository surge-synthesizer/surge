#pragma once

#include <JuceHeader.h>
#include "BBDDelayLine.h"
#include "BBDCompander.h"

class BBDDelay : public chowdsp::PluginBase<BBDDelay>
{
  public:
    BBDDelay();

    static void addParameters(Parameters &params);
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processAudioBlock(AudioBuffer<float> &buffer) override;

  private:
    std::atomic<float> *delayMsParam = nullptr;
    std::atomic<float> *nStagesParam = nullptr;
    std::atomic<float> *reconstructParam = nullptr;
    std::atomic<float> *driveParam = nullptr;
    std::atomic<float> *freqParam = nullptr;
    std::atomic<float> *compandParam = nullptr;

    BBDDelayLine<512> del512[2];
    BBDDelayLine<1024> del1024[2];
    BBDDelayLine<2048> del2048[2];
    BBDDelayLine<4096> del4096[2];
    BBDDelayLine<8192> del8192[2];
    float fs = 48000.0f;

    BBDCompressor compressor[2];
    BBDExpander expander[2];

    SmoothedValue<float, ValueSmoothingTypes::Linear> delaySmooth[2];
    SmoothedValue<float, ValueSmoothingTypes::Linear> freqSmooth[2];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BBDDelay)
};
