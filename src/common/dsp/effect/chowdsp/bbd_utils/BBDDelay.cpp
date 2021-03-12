#include "BBDDelay.h"

BBDDelay::BBDDelay()
{
    delayMsParam = vts.getRawParameterValue("delay_ms");
    nStagesParam = vts.getRawParameterValue("n_stages");
    reconstructParam = vts.getRawParameterValue("reconstruct");
    driveParam = vts.getRawParameterValue("drive");
    freqParam = vts.getRawParameterValue("freq");
    compandParam = vts.getRawParameterValue("compand");
}

void BBDDelay::addParameters(Parameters &params)
{
    NormalisableRange<float> delayRange(5.0f, 500.0f);
    delayRange.setSkewForCentre(50.0f);
    params.push_back(
        std::make_unique<AudioParameterFloat>("delay_ms", "Delay Time [ms]", delayRange, 10.0f));

    params.push_back(std::make_unique<AudioParameterChoice>(
        "n_stages", "Stages", StringArray{"512", "1024", "2048", "4096", "8192"}, 2));

    params.push_back(std::make_unique<AudioParameterBool>("reconstruct", "Reconstruct", true));

    params.push_back(std::make_unique<AudioParameterFloat>("drive", "Drive", 0.0f, 1.0f, 0.5f));

    NormalisableRange<float> freqRange(100.0f, 20000.0f);
    freqRange.setSkewForCentre(2000.0f);
    params.push_back(std::make_unique<AudioParameterFloat>("freq", "Freq.", freqRange, 10000.0f));

    params.push_back(std::make_unique<AudioParameterBool>("compand", "Companding", true));
}

void BBDDelay::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    fs = (float)sampleRate;

    auto prepareDelay = [=](auto &delay) { delay.prepare(sampleRate); };

    for (int ch = 0; ch < 2; ++ch)
    {
        prepareDelay(del512[ch]);
        prepareDelay(del1024[ch]);
        prepareDelay(del2048[ch]);
        prepareDelay(del4096[ch]);
        prepareDelay(del8192[ch]);

        compressor[ch].prepare(sampleRate, samplesPerBlock);
        compressor[ch].setCutoff(20.0f);
        expander[ch].prepare(sampleRate, samplesPerBlock);
        expander[ch].setCutoff(20.0f);

        delaySmooth[ch].reset(sampleRate, 0.05);
        freqSmooth[ch].reset(sampleRate, 0.05);
    }
}

void BBDDelay::releaseResources() {}

void BBDDelay::processAudioBlock(AudioBuffer<float> &buffer)
{
    const auto reconstruct = (bool)reconstructParam->load();
    const auto nStagesType = (int)nStagesParam->load();
    const auto numSamples = buffer.getNumSamples();
    const auto compand = (bool)compandParam->load();

    auto processDelayBlock = [=](int ch, auto *x, auto &delay) {
        delay.setWaveshapeParams(driveParam->load());
        if (delaySmooth[ch].isSmoothing() || freqSmooth[ch].isSmoothing())
        {
            for (int n = 0; n < numSamples; ++n)
            {
                delay.setParameters(delaySmooth[ch].getNextValue(), freqSmooth[ch].getNextValue());
                x[n] = delay.process(x[n], reconstruct);
            }
        }
        else
        {
            delay.setParameters(delaySmooth[ch].getCurrentValue(),
                                freqSmooth[ch].getCurrentValue());
            for (int n = 0; n < numSamples; ++n)
                x[n] = delay.process(x[n], reconstruct);
        }
    };

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        delaySmooth[ch].setTargetValue(delayMsParam->load());
        freqSmooth[ch].setTargetValue(freqParam->load());

        auto *x = buffer.getWritePointer(ch);

        if (compand)
        {
            for (int n = 0; n < numSamples; ++n)
                x[n] = compressor[ch].process(x[n]);

            FloatVectorOperations::multiply(x, Decibels::decibelsToGain(-12.0f), numSamples);
        }

        if (nStagesType == 0) // 512
            processDelayBlock(ch, x, del512[ch]);
        else if (nStagesType == 1) // 1024
            processDelayBlock(ch, x, del1024[ch]);
        else if (nStagesType == 2) // 2048
            processDelayBlock(ch, x, del2048[ch]);
        else if (nStagesType == 3) // 4096
            processDelayBlock(ch, x, del4096[ch]);
        else if (nStagesType == 4) // 8192
            processDelayBlock(ch, x, del8192[ch]);

        if (compand)
        {
            for (int n = 0; n < numSamples; ++n)
                x[n] = expander[ch].process(x[n]);

            FloatVectorOperations::multiply(x, Decibels::decibelsToGain(12.0f), numSamples);
        }
        else
        {
            FloatVectorOperations::multiply(x, Decibels::decibelsToGain(-7.5f), numSamples);
        }
    }
}

// This creates new instances of the plugin
AudioProcessor *JUCE_CALLTYPE createPluginFilter() { return new BBDDelay(); }
