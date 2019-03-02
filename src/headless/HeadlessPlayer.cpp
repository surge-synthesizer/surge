#include "HeadlessPlayer.h"

namespace Surge
{
namespace Headless
{
namespace Player
{
void grabNBlocks(SurgeSynthesizer* surge, int nBlocks, std::vector<float>& output)
{
   int no = surge->getNumOutputs();
   for (auto i = 0; i < nBlocks; ++i)
   {
      surge->process();
      for (int sm = 0; sm < BLOCK_SIZE; ++sm)
      {
         for (int io = 0; io < no; ++io)
         {
            output.push_back(surge->output[io][sm]);
         }
      }
   }
}

void play120BPMCMajorQuarterNoteScale(SurgeSynthesizer* surge, std::vector<float>& output)
{
   // int sr = surge->getSampleRate(); // really? That's not there
   // FIXME
   int sr = 44100;

   int samplesPerNote = 60.0 / 120.0 * sr;
   int blocksPerNote = samplesPerNote / BLOCK_SIZE;
   int currSamp = 0;

   auto notes = {60, 62, 64, 65, 67, 69, 71, 72};
   for (auto n : notes)
   {
      surge->playNote((char)0, (char)n, (char)100, 0);
      grabNBlocks(surge, blocksPerNote - 1, output);
      surge->playNote((char)0, (char)n, (char)100, 0);
      grabNBlocks(surge, 1, output);
   }
   return;
}

} // namespace Player
} // namespace Headless
} // namespace Surge
