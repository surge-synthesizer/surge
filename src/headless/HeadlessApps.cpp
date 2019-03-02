#include "HeadlessApps.h"
#include "HeadlessTools.h"
#include "HeadlessPlayer.h"
#include <iostream>
#include <iomanip>

namespace Surge
{
namespace Headless
{

void runSimpleSynthToStdout(SurgeSynthesizer* surge)
{
   /*
   ** Change a parameter in the scene. Do this by traversing the
   ** graph in the current patch (which is in surge->storage).
   **
   ** Clearly a more fulsome headless API would provide wrappers around
   ** this for common activities. This sets up a pair of detuned saw waves
   ** both active.
   */
   surge->storage.getPatch().scene[0].osc[0].pitch.set_value_f01(4);
   surge->storage.getPatch().scene[0].mute_o2.set_value_f01(0, true);
   surge->storage.getPatch().scene[0].osc[1].pitch.set_value_f01(1);

   /*
   ** Play a note. channel, note, velocity, detune
   */
   surge->playNote((char)0, (char)60, (char)100, 0);

   /*
   ** Strip off some processing first to avoid the attach transient
   */
   for (auto i = 0; i < 20; ++i)
      surge->process();

   /*
   ** Then run the sampler
   */
   int blockCount = 30;
   int overSample = 8; // we want to include n samples per printed row.
   float overS = 0;
   int sampleCount = 0;
   for (auto i = 0; i < blockCount; ++i)
   {
      surge->process();

      for (int sm = 0; sm < BLOCK_SIZE; ++sm)
      {
         float avgOut = 0;
         for (int oi = 0; oi < surge->getNumOutputs(); ++oi)
         {
            avgOut += surge->output[oi][sm];
         }

         overS += avgOut;
         sampleCount++;

         if (((sampleCount) % overSample) == 0)
         {
            overS /= overSample;
            int gWidth = (int)((overS + 1) * 30);
            std::cout << "Sample: " << std::setw(15) << overS << std::setw(gWidth) << "X"
                      << std::endl;
            ;
            overS = 0.0; // start accumulating again
         }
      }
   }
}

void scanAllPresets(SurgeSynthesizer* surge)
{
   /*
   ** The patches are not sorted in category order so traverse an outer loop of categories and
   ** inner of patches. We should clean up these data structures one day...
   */
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

            std::cout << "idx= " << std::right << std::setw(4) << i << "; cat = " << std::left
                      << std::setw(30) << pc.name << "; patch = " << std::left << std::setw(30)
                      << p.name << "";

            loadPatchByIndex(surge, i);

            /*
            ** So lets play some notes and gather some waveforms
            */
            std::vector<float> data;
            Surge::Headless::Player::play120BPMCMajorQuarterNoteScale(surge, data);
            if (data.size() > 0)
            {
               const auto [mind, maxd] = std::minmax_element(begin(data), end(data));

               std::cout << "; data = [" << std::setw(12) << *mind << ", " << std::setw(12) << *maxd
                         << "] in " << data.size() << " samples";
            }
            std::cout << std::endl;
         }
      }
   }
}

} // namespace Headless
} // namespace Surge
