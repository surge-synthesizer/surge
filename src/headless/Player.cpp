#include "Player.h"

namespace Surge
{
namespace Headless
{

playerEvents_t makeHoldMiddleC(int forSamples, int withTail)
{
   playerEvents_t result;

   Event on;
   on.type = Event::NOTE_ON;
   on.channel = 0;
   on.data1 = 60;
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

void playAsConfigured(SurgeSynthesizer* surge,
                      const playerEvents_t& events,
                      float** data,
                      int* nSamples,
                      int* nChannels)
{
   if (events.size() == 0)
      return;

   int desiredSamples = events.back().atSample;
   int blockCount = desiredSamples / BLOCK_SIZE;
   int currEvt = 0;

   *nChannels = 2;
   *nSamples = desiredSamples;
   float* ldata = new float[desiredSamples * 2];
   *data = ldata;

   size_t flidx = 0;

   for (auto i = 0; i < blockCount; ++i)
   {
      int cs = i * BLOCK_SIZE;
      while (events[currEvt].atSample <= cs + BLOCK_SIZE - 1 && currEvt < events.size())
      {
         Event e = events[currEvt];
         if (e.type == Event::NOTE_ON)
            surge->playNote(e.channel, e.data1, e.data2, 0);
         if (e.type == Event::NOTE_OFF)
            surge->releaseNote(e.channel, e.data1, e.data2);

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

void playOnPatch(SurgeSynthesizer* surge,
                 int patch,
                 const playerEvents_t& events,
                 float** data,
                 int* nSamples,
                 int* nChannels)
{
   surge->loadPatch(patch);
   playAsConfigured(surge, events, data, nSamples, nChannels);
}

void playOnEveryPatch(
    SurgeSynthesizer* surge,
    const playerEvents_t& events,
    std::function<void(
        const Patch& p, const PatchCategory& c, const float* data, int nSamples, int nChannels)> cb)
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

            float* data = NULL;
            int nSamples, nChannels;

            playOnPatch(surge, i, events, &data, &nSamples, &nChannels);
            cb(p, pc, data, nSamples, nChannels);

            if (data)
               delete[] data;
         }
      }
   }
}

} // namespace Headless
} // namespace Surge
