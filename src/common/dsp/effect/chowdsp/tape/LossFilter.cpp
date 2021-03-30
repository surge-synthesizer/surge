#include "LossFilter.h"

namespace chowdsp
{

LossFilter::LossFilter(SurgeStorage *storage, size_t order) : order(order), bumpFilter(storage)
{
    bumpFilter.setBlockSize(BLOCK_SIZE);
    bumpFilter.suspend();
    bumpFilter.coeff_peakEQ(bumpFilter.calc_omega_from_Hz(100.0), 0.5, 0.0);
    bumpFilter.coeff_instantize();
}

void LossFilter::prepare(float sampleRate, int blockSize)
{
    fs = sampleRate;
    fsFactor = fs / 44100.0f;
    curOrder = int(order * fsFactor);
    currentCoefs.resize(curOrder);
    HCoefs.resize(curOrder);

    set_params(30.0f, 0.1f, 1.0f, 0.1f);
    calcCoefs();

    for (int i = 0; i < 2; ++i)
    {
        filters[i] = std::make_unique<FIRFilter>(order);
        filters[i]->reset();
        filters[i]->setCoefs(currentCoefs.data());
    }

    // initialise parameters
    prevSpeed = curSpeed;
    prevSpacing = curSpacing;
    prevGap = curGap;
    prevThick = curThick;
}

void LossFilter::calcHeadBumpFilter(float speedIps, float gapMeters, float fs, BiquadFilter &filter)
{
    auto bumpFreq = speedIps * 0.0254f / (gapMeters * 500.0f);
    auto gain = std::max(1.5f * (1000.0f - std::abs(bumpFreq - 100.0f)) / 100.0f, 1.0f);
    filter.coeff_peakEQ(filter.calc_omega_from_Hz(bumpFreq), 0.5, linear_to_dB(gain));
}

void LossFilter::set_params(float speed, float spacing, float gap, float thick)
{
    curSpeed = speed;
    curSpacing = spacing;
    curGap = gap;
    curThick = thick;
}

void LossFilter::calcCoefs()
{
    // Set freq domain multipliers
    binWidth = fs / (float)curOrder;
    for (int k = 0; k < curOrder / 2; k++)
    {
        const auto freq = (float)k * binWidth;
        const auto waveNumber = 2.0f * M_PI * std::max(freq, 20.0f) / (curSpeed * 0.0254f);
        const auto thickTimesK = waveNumber * (curThick * (float)1.0e-6);
        const auto kGapOverTwo = waveNumber * (curGap * (float)1.0e-6) / 2.0f;

        HCoefs[k] = std::exp(-waveNumber * (curSpacing * (float)1.0e-6)); // Spacing loss
        HCoefs[k] *= (1.0f - std::exp(-thickTimesK)) / thickTimesK;       // Thickness loss
        HCoefs[k] *= std::sin(kGapOverTwo) / kGapOverTwo;                 // Gap loss
        HCoefs[curOrder - k - 1] = HCoefs[k];
    }

    // Create time domain filter signal
    for (int n = 0; n < curOrder / 2; n++)
    {
        const size_t idx = curOrder / 2 + n;
        for (int k = 0; k < curOrder; k++)
            currentCoefs[idx] +=
                HCoefs[k] * std::cos(2.0f * M_PI * (float)k * (float)n / (float)curOrder);

        currentCoefs[idx] /= (float)curOrder;
        currentCoefs[curOrder / 2 - n] = currentCoefs[idx];
    }

    // compute head bump filters
    calcHeadBumpFilter(curSpeed, curGap * (float)1.0e-6, (double)fs, bumpFilter);
}

void LossFilter::process(float *dataL, float *dataR)
{
    if ((curSpeed != prevSpeed || curSpacing != prevSpacing || curThick != prevThick ||
         curGap != prevGap) &&
        fadeCount == 0)
    {
        calcCoefs();
        filters[!activeFilter]->setCoefs(currentCoefs.data());

        fadeCount = fadeLength;
        prevSpeed = curSpeed;
        prevSpacing = curSpacing;
        prevThick = curThick;
        prevGap = curGap;
    }

    if (fadeCount > 0)
    {
        copy_block(dataL, fadeBufferL, BLOCK_SIZE_QUAD);
        copy_block(dataR, fadeBufferR, BLOCK_SIZE_QUAD);
    }
    else
    {
        filters[!activeFilter]->processBypassed(dataL, dataR, BLOCK_SIZE);
    }

    // normal processing here...
    filters[activeFilter]->process(dataL, dataR, BLOCK_SIZE);

    if (fadeCount > 0)
    {
        filters[!activeFilter]->process(fadeBufferL, fadeBufferR, BLOCK_SIZE);

        // fade between buffers
        auto startGain = (float)fadeCount / (float)fadeLength;
        auto samplesToFade = std::min(fadeCount, BLOCK_SIZE);
        fadeCount -= samplesToFade;
        auto endGain = (float)fadeCount / (float)fadeLength;
        auto increment = (endGain - startGain) / (float)BLOCK_SIZE;

        auto curGain = startGain;
        auto fadeGain = 1.0f - startGain;
        for (int i = 0; i < BLOCK_SIZE; ++i)
        {
            dataL[i] = dataL[i] * curGain + fadeBufferL[i] * fadeGain;
            dataR[i] = dataR[i] * curGain + fadeBufferR[i] * fadeGain;
            curGain += increment;
            fadeGain = 1.0f - curGain;
        }

        if (fadeCount == 0)
            activeFilter = !activeFilter;
    }

    bumpFilter.process_block(dataL, dataR);
}

} // namespace chowdsp
