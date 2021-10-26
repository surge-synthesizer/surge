#include <random>
#include "SpringReverbProc.h"
#include "utilities/FastMath.h"

namespace chowdsp
{
SpringReverbProc::SpringReverbProc()
{
    std::random_device rd;
    auto gen01 = std::minstd_rand(rd());
    std::uniform_real_distribution<float> distro01(0.0f, 1.0f);
    urng01 = std::bind(distro01, gen01);
}

void SpringReverbProc::prepare(float sampleRate, int samplesPerBlock)
{
    fs = sampleRate;

    delay.prepare({sampleRate, (juce::uint32)samplesPerBlock, 2});

    dcBlocker.prepare(sampleRate, 2);
    dcBlocker.setCutoffFrequency(40.0f);

    for (auto &apf : vecAPFs)
        apf.prepare(sampleRate);

    lpf.prepare(sampleRate, 2);

    reflectionNetwork.prepare(sampleRate, samplesPerBlock);

    chaosSmooth.reset((double)sampleRate, 0.05);

    for (auto &val : simdState)
        val = 0.0f;
}

void SpringReverbProc::setParams(const Params &params, int numSamples)
{
    auto msToSamples = [=](float ms) { return (ms / 1000.0f) * fs; };

    constexpr float lowT60 = 0.5f;
    constexpr float highT60 = 4.5f;
    float t60Seconds =
        lowT60 *
        std::pow(highT60 / lowT60, params.decay - 0.7f * (1.0f - std::pow(params.size, 2.0f)));

    float delaySamples = 1000.0f + std::pow(params.size * 0.099f, 1.0f) * fs;
    chaosSmooth.setTargetValue(urng01() * delaySamples * 0.07f);
    delaySamples += std::pow(params.chaos, 3.0f) * chaosSmooth.skip(numSamples);
    delay.setDelay(delaySamples);

    feedbackGain = std::pow(0.001f, delaySamples / (t60Seconds * fs));

    auto apfG = 0.5f - 0.4f * params.spin;
    float apfGVec alignas(16)[4] = {apfG, -apfG, apfG, -apfG};
    for (auto &apf : vecAPFs)
        apf.setParams(msToSamples(0.35f + 3.0f * params.size), _mm_load_ps(apfGVec));

    constexpr float dampFreqLow = 4000.0f;
    constexpr float dampFreqHigh = 18000.0f;
    auto dampFreq = dampFreqLow * std::pow(dampFreqHigh / dampFreqLow, 1.0f - params.damping);
    lpf.setCutoffFrequency(dampFreq);

    reflectionNetwork.setParams(params.size, t60Seconds, params.reflections * 0.5f, params.damping);
}

void SpringReverbProc::processBlock(float *left, float *right, const int numSamples)
{
    auto doSpringInput = [=](int ch, float input) -> float {
        auto output = Surge::DSP::fasttanh(input - feedbackGain * delay.popSample(ch));
        return dcBlocker.processSample<StateVariableFilterType::Highpass>(ch, output);
    };

    auto doAPFProcess = [&]() {
        auto yVec = VecType::fromRawArray(simdState);
        for (auto &apf : vecAPFs)
            yVec = apf.processSample(yVec);
        yVec.copyToRawArray(simdState);
    };

    auto doSpringOutput = [&](int ch) {
        auto chIdx = 2 * ch;
        delay.pushSample(ch, simdState[1 + chIdx]);

        simdState[chIdx] -= reflectionNetwork.popSample(ch);
        reflectionNetwork.pushSample(ch, simdState[chIdx]);

        constexpr auto filterType = StateVariableFilterType::Lowpass;
        simdState[chIdx] = lpf.processSample<filterType>(ch, simdState[chIdx]);
    };

    for (int n = 0; n < numSamples; ++n)
    {
        simdState[0] = doSpringInput(0, left[n]);
        simdState[2] = doSpringInput(1, right[n]);

        doAPFProcess();

        doSpringOutput(0);
        doSpringOutput(1);

        left[n] = simdState[0];      // write to output
        simdState[1] = simdState[0]; // write to feedback path
        right[n] = simdState[2];
        simdState[3] = simdState[2];
    }
}

} // namespace chowdsp
