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
#include "Player.h"

namespace Surge
{
namespace Headless
{

playerEvents_t makeHoldMiddleC(int forSamples, int withTail)
{
    return makeHoldNoteFor(60, forSamples, withTail);
}

playerEvents_t makeHoldNoteFor(int note, int forSamples, int withTail, int midiChannel)
{
    playerEvents_t result;

    Event on;
    on.type = Event::NOTE_ON;
    on.channel = midiChannel;
    on.data1 = note;
    on.data2 = 100;
    on.atSample = 0;

    result.push_back(on);

    on.atSample = forSamples;
    on.type = Event::NOTE_OFF;
    on.data2 = 0;
    result.push_back(on);

    if (withTail != 0)
    {
        on.type = Event::NO_EVENT;
        on.atSample = forSamples + withTail;
        result.push_back(on);
    }

    return result;
}

playerEvents_t make120BPMCMajorQuarterNoteScale(long s0, int sr)
{
    int samplesPerNote = 60.0 / 120.0 * sr;
    int currSamp = s0;
    auto notes = {60, 62, 64, 65, 67, 69, 71, 72};

    playerEvents_t result;
    for (auto n : notes)
    {
        Event e;
        e.type = Event::NOTE_ON;
        e.channel = 0;
        e.data1 = n;
        e.data2 = 100;
        e.atSample = currSamp;

        result.push_back(e);

        e.type = Event::NOTE_OFF;
        e.data2 = 0;
        e.atSample += 0.95 * samplesPerNote;
        result.push_back(e);

        currSamp += samplesPerNote;
    }
    return result;
}

void playAsConfigured(std::shared_ptr<SurgeSynthesizer> surge, const playerEvents_t &events,
                      float **data, int *nSamples, int *nChannels)
{
    if (events.size() == 0)
        return;

    int desiredSamples = events.back().atSample;
    int blockCount = desiredSamples / BLOCK_SIZE + 1;
    int currEvt = 0;

    *nChannels = 2;
    *nSamples = blockCount * BLOCK_SIZE;
    size_t dataSize = *nChannels * *nSamples;
    float *ldata = new float[dataSize];
    memset(ldata, 0, dataSize);
    *data = ldata;

    size_t flidx = 0;

    surge->process();

    for (auto i = 0; i < blockCount; ++i)
    {
        int cs = i * BLOCK_SIZE;
        while (currEvt < events.size() && events[currEvt].atSample <= cs + BLOCK_SIZE - 1)
        {
            Event e = events[currEvt];
            switch (e.type)
            {
            case Event::NOTE_ON:
                surge->playNote(e.channel, e.data1, e.data2, 0);
                break;
            case Event::NOTE_OFF:
                surge->releaseNote(e.channel, e.data1, e.data2);
                break;
            case Event::LAMBDA_EVENT:
                e.surgeLambda(surge);
                break;
            case Event::NO_EVENT:
                break;
            }

            currEvt++;
        }

        surge->process();
        for (int sm = 0; sm < BLOCK_SIZE; ++sm)
        {
            for (int oi = 0; oi < surge->getNumOutputs(); ++oi)
            {
                ldata[flidx++] = surge->output[oi][sm];
            }
        }
    }
}

void playOnPatch(std::shared_ptr<SurgeSynthesizer> surge, int patch, const playerEvents_t &events,
                 float **data, int *nSamples, int *nChannels)
{
    surge->loadPatch(patch);
    playAsConfigured(surge, events, data, nSamples, nChannels);
}

void playOnEveryPatch(std::shared_ptr<SurgeSynthesizer> surge, const playerEvents_t &events,
                      std::function<void(const Patch &p, const PatchCategory &c, const float *data,
                                         int nSamples, int nChannels)>
                          cb)
{
    int nPresets = surge->storage.patch_list.size();
    int nCats = surge->storage.patch_category.size();

    for (auto c : surge->storage.patchCategoryOrdering)
    {
        for (auto i = 0; i < nPresets; ++i)
        {
            int idx = surge->storage.patchOrdering[i];
            Patch p = surge->storage.patch_list[idx];
            if (p.category == c)
            {
                PatchCategory pc = surge->storage.patch_category[p.category];

                float *data = NULL;
                int nSamples, nChannels;

                playOnPatch(surge, i, events, &data, &nSamples, &nChannels);
                cb(p, pc, data, nSamples, nChannels);

                if (data)
                    delete[] data;
            }
        }
    }
}

void playOnNRandomPatches(std::shared_ptr<SurgeSynthesizer> surge, const playerEvents_t &events,
                          int nPlays,
                          std::function<void(const Patch &p, const PatchCategory &c,
                                             const float *data, int nSamples, int nChannels)>
                              cb)
{
    int nPresets = surge->storage.patch_list.size();
    int nCats = surge->storage.patch_category.size();

    for (auto i = 0; i < nPlays; ++i)
    {
        int rp = (int)(1.0 * rand() / RAND_MAX * (surge->storage.patch_list.size() - 1));
        Patch p = surge->storage.patch_list[rp];
        PatchCategory pc = surge->storage.patch_category[p.category];

        float *data = NULL;
        int nSamples, nChannels;

        playOnPatch(surge, i, events, &data, &nSamples, &nChannels);
        cb(p, pc, data, nSamples, nChannels);

        if (data)
            delete[] data;
    }
}

} // namespace Headless
} // namespace Surge
