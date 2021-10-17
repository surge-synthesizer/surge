#include <random>
#include "SpringReverbProc.h"

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

    delay.prepare(sampleRate, samplesPerBlock);

    dcBlocker.prepare(sampleRate, 2);
    dcBlocker.setCutoffFrequency(40.0f);

    for (auto &apf : vecAPFs)
        apf.prepare(sampleRate);

    lpf.prepare(sampleRate, 2);

    reflectionNetwork.prepare(sampleRate, samplesPerBlock);

    chaosSmooth.reset((double)sampleRate, 0.05);

    z[0] = 0.0f;
    z[1] = 0.0f;
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
        apf.setParams(msToSamples(0.35f + params.size), _mm_load_ps(apfGVec));

    constexpr float dampFreqLow = 4000.0f;
    constexpr float dampFreqHigh = 18000.0f;
    auto dampFreq = dampFreqLow * std::pow(dampFreqHigh / dampFreqLow, 1.0f - params.damping);
    lpf.setCutoffFrequency(dampFreq);

    reflectionNetwork.setParams(params.size, t60Seconds, params.reflections * 0.5f, params.damping);
}

void SpringReverbProc::processBlock(float *left, float *right, const int numSamples)
{
    // SIMD register for parallel allpass filter
    // format:
    //   [0]: y_left (feedforward path output)
    //   [1]: z_left (feedback path)
    //   [2-3]: equivalents for right channel, or zeros
    float simdReg alignas(16)[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    auto doSpringInput = [=](int ch, float input) -> float {
        auto output = std::tanh(input - feedbackGain * delay.popSample(ch));
        return dcBlocker.processSample<StateVariableFilterType::Highpass>(ch, output);
    };

    auto doAPFProcess = [&]() {
        auto yVec = _mm_load_ps(simdReg);
        for (auto &apf : vecAPFs)
            yVec = apf.processSample(yVec);
        _mm_store_ps(simdReg, yVec);
    };

    auto doSpringOutput = [&](int ch) {
        auto chIdx = 2 * ch;
        delay.pushSample(ch, simdReg[1 + chIdx]);

        simdReg[chIdx] -= reflectionNetwork.popSample(ch);
        reflectionNetwork.pushSample(ch, simdReg[chIdx]);
        simdReg[chIdx] = lpf.processSample<StateVariableFilterType::Lowpass>(ch, simdReg[chIdx]);
    };

    simdReg[1] = z[0];
    simdReg[3] = z[1];

    for (int n = 0; n < numSamples; ++n)
    {
        simdReg[0] = doSpringInput(0, left[n]);
        simdReg[2] = doSpringInput(1, right[n]);

        doAPFProcess();

        doSpringOutput(0);
        doSpringOutput(1);

        left[n] = simdReg[0];    // write to output
        simdReg[1] = simdReg[0]; // write to feedback path
        right[n] = simdReg[2];
        simdReg[3] = simdReg[2];
    }

    z[0] = simdReg[1];
    z[1] = simdReg[3];
}

} // namespace chowdsp
