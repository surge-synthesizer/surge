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
#include <memory>
#include "SurgeSynthesizer.h"
#include "Player.h"
#include "catch2/catch_amalgamated.hpp"
#include <iostream>
#include <cstdio>
#include <string>

namespace Surge
{
namespace Test
{

double frequencyFromData(float *buffer, int nS, int nC, int audioChannel, int start, int trimTo,
                         float samplerate)
{
    float *leftTrimmed = new float[trimTo];

    for (int i = 0; i < trimTo; ++i)
        leftTrimmed[i] = buffer[(i + start) * 2 + audioChannel];

    // OK so now look for sample times between positive/negative crosses
    int v = -1;
    uint64_t dSample = 0, crosses = 0;
    const int minSamplesBetweenCrosses = 10;
    int lc = -1;
    const int upswingWindow = 5;
    for (int i = upswingWindow; i < trimTo - 1; ++i)
    {
        if (leftTrimmed[i - upswingWindow] < 0 && leftTrimmed[i] < 0 && leftTrimmed[i + 1] >= 0 &&
            i - lc > minSamplesBetweenCrosses)
        {
            lc = i;
            if (v > 0)
            {
                dSample += (i - v);
                crosses++;
            }
            v = i;
        }
    }

    float aSample = 1.f * dSample / crosses;

    float time = aSample / samplerate;
    float freq = 1.0 / time;

    delete[] leftTrimmed;
    return freq;
}

double RMSFromData(float *buffer, int nS, int nC, int audioChannel, int start, int trimTo)
{
    float *leftTrimmed = new float[trimTo];

    for (int i = 0; i < trimTo; ++i)
        leftTrimmed[i] = buffer[(i + start) * 2 + audioChannel];

    // OK so now look for sample times between positive/negative crosses
    double RMS = 0;
    int ct = 0;
    for (int i = 0; i < trimTo - 1; ++i)
    {
        RMS += leftTrimmed[i] * leftTrimmed[i];
        ct++;
    }
    RMS /= ct;
    RMS = sqrt(RMS);

    delete[] leftTrimmed;
    return RMS;
}

double frequencyForNote(std::shared_ptr<SurgeSynthesizer> surge, int note, int seconds,
                        int audioChannel, int midiChannel)
{
    auto events = Surge::Headless::makeHoldNoteFor(note, surge->storage.samplerate * seconds, 64,
                                                   midiChannel);
    float *buffer;
    int nS, nC;
    Surge::Headless::playAsConfigured(surge, events, &buffer, &nS, &nC);
    for (auto i = 0; i < 500; ++i)
        surge->process(); // Ring out any transients on this synth

    REQUIRE(nC == 2);
    REQUIRE(nS >= surge->storage.samplerate * seconds);
    REQUIRE(nS <= surge->storage.samplerate * seconds + 4 * BLOCK_SIZE);

    // Trim off the leading and trailing
    int nSTrim = (int)(nS / 2 * 0.8);
    int start = (int)(nS / 2 * 0.05);

    auto freq =
        frequencyFromData(buffer, nS, nC, audioChannel, start, nSTrim, surge->storage.samplerate);
    delete[] buffer;

    return freq;
}

std::pair<double, double> frequencyAndRMSForNote(std::shared_ptr<SurgeSynthesizer> surge, int note,
                                                 int seconds, int audioChannel, int midiChannel)
{
    auto events = Surge::Headless::makeHoldNoteFor(note, surge->storage.samplerate * seconds, 64,
                                                   midiChannel);
    float *buffer;
    int nS, nC;
    Surge::Headless::playAsConfigured(surge, events, &buffer, &nS, &nC);
    for (auto i = 0; i < 500; ++i)
        surge->process(); // Ring out any transients on this synth

    REQUIRE(nC == 2);
    REQUIRE(nS >= surge->storage.samplerate * seconds);
    REQUIRE(nS <= surge->storage.samplerate * seconds + 4 * BLOCK_SIZE);

    // Trim off the leading and trailing
    int nSTrim = (int)(nS / 2 * 0.8);
    int start = (int)(nS / 2 * 0.05);

    auto freq =
        frequencyFromData(buffer, nS, nC, audioChannel, start, nSTrim, surge->storage.samplerate);
    auto rms = RMSFromData(buffer, nS, nC, audioChannel, start, nSTrim);
    delete[] buffer;

    return std::make_pair(freq, rms);
}

double frequencyForEvents(std::shared_ptr<SurgeSynthesizer> surge,
                          Surge::Headless::playerEvents_t &events, int audioChannel,
                          int startSample, int endSample)
{
    float *buffer;
    int nS, nC;

    Surge::Headless::playAsConfigured(surge, events, &buffer, &nS, &nC);
    for (auto i = 0; i < 500; ++i)
        surge->process(); // Ring out any transients on this synth

    REQUIRE(startSample < nS);
    REQUIRE(endSample < nS);

    auto freq = frequencyFromData(buffer, nS, nC, audioChannel, startSample,
                                  endSample - startSample, surge->storage.samplerate);
    delete[] buffer;

    return freq;
}

void copyScenedataSubset(SurgeStorage *storage, int scene, int start, int end)
{
    int s = storage->getPatch().scene_start[scene];
    for (int i = start; i < end; ++i)
    {
        storage->getPatch().scenedata[scene][i - s].i = storage->getPatch().param_ptr[i]->val.i;
    }
}

void setupStorageRanges(Parameter *start, Parameter *endIncluding, int &storage_id_start,
                        int &storage_id_end)
{
    int min_id = 100000, max_id = -1;
    Parameter *oap = start;
    while (oap <= endIncluding)
    {
        if (oap->id >= 0)
        {
            if (oap->id > max_id)
                max_id = oap->id;
            if (oap->id < min_id)
                min_id = oap->id;
        }
        oap++;
    }

    storage_id_start = min_id;
    storage_id_end = max_id + 1;
}

/*
** Create a surge pointer on init sine
*/
std::shared_ptr<SurgeSynthesizer> surgeOnPatch(const std::string &otp)
{
    auto surge = Surge::Headless::createSurge(44100, true);

    bool foundInitSine = false;
    for (int i = 0; i < surge->storage.patch_list.size(); ++i)
    {
        Patch p = surge->storage.patch_list[i];
        if (p.name == otp)
        {
            surge->loadPatch(i);
            foundInitSine = true;
            break;
        }
    }
    if (!foundInitSine)
        return nullptr;
    else
        return surge;
}

std::shared_ptr<SurgeSynthesizer> surgeOnTemplate(const std::string &otp, float sr = 44100)
{
    auto surge = Surge::Headless::createSurge(sr);

    auto defaultPath = surge->storage.datapath;
    try
    {
        if (!fs::exists(defaultPath / fs::path{"patches_factory"}))
        {
            auto pt = fs::path{"resources/data/patches_factory"};
            if (fs::exists(pt) && fs::is_directory(pt))
            {
                defaultPath = fs::path{"resources/data"};
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
    }

    auto templatePath =
        defaultPath / fs::path{"patches_factory"} / fs::path{"Templates"} / fs::path{otp + ".fxp"};

    REQUIRE(fs::exists(templatePath));
    surge->loadPatchByPath(path_to_string(templatePath).c_str(), -1, "Test");

    return surge;
}

std::shared_ptr<SurgeSynthesizer> surgeOnSine(float sr = 44100)
{
    auto surge = Surge::Headless::createSurge(sr);

    auto pt = fs::path{"resources/test-data/patches"};
    REQUIRE(fs::is_directory(pt));
    auto isin = pt / "TestInitSine.fxp";
    REQUIRE(fs::exists(isin));
    surge->loadPatchByPath(path_to_string(isin).c_str(), -1, "Test");
    return surge;
}
std::shared_ptr<SurgeSynthesizer> surgeOnSaw(float sr = 44100)
{
    auto surge = Surge::Headless::createSurge(sr);

    auto pt = fs::path{"resources/test-data/patches"};
    REQUIRE(fs::is_directory(pt));
    auto isin = pt / "TestInitSaw.fxp";
    REQUIRE(fs::exists(isin));
    surge->loadPatchByPath(path_to_string(isin).c_str(), -1, "Test");
    return surge;
}

void makePlotPNGFromData(std::string pngFileName, std::string plotTitle, float *buffer, int nS,
                         int nC, int startSample, int endSample)
{
#if MAC
    if (nC != 2)
        std::cout << "This won't work really" << std::endl;

    if (startSample < 0)
        startSample = 0;
    if (endSample < 0)
        endSample = nS;

    auto csvnm = std::tmpnam(nullptr);
    std::cout << "Creating csvnm " << csvnm << std::endl;
    std::ofstream ofs(csvnm);
    for (int i = startSample; i < endSample; ++i)
    {
        ofs << i << ", " << buffer[i * nC] << ", " << buffer[i * nC + 1] << std::endl;
    }
    ofs.close();

    std::string cmd = "python3 scripts/misc/csvtoPlot.py \"" + std::string(csvnm) + "\" \"" +
                      pngFileName + "\" \"" + plotTitle + "\"";
    std::cout << "Running " << cmd << std::endl;
    system(cmd.c_str());
#else
    std::cout
        << "makePlotPNGFromData is only on Mac for now (since @baconpaul just uses it to debug)"
        << std::endl;
#endif
}
void setFX(std::shared_ptr<SurgeSynthesizer> surge, int slot, fx_type type)
{
    auto *pt = &(surge->storage.getPatch().fx[slot].type);
    auto awv = 1.f * float(type) / (pt->val_max.i - pt->val_min.i);

    auto did = surge->idForParameter(pt);
    surge->setParameter01(did, awv, false);

    for (int i = 0; i < 10; ++i)
        surge->process();
}
} // namespace Test
} // namespace Surge
