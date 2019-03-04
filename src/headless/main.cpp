#include <iostream>
#include <iomanip>

#include "HeadlessUtils.h"
#include "Player.h"

void simpleOscillatorToStdOut()
{
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);

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

   Surge::Headless::playerEvents_t terryRiley = Surge::Headless::makeHoldMiddleC(4410);

   float* data = NULL;
   int nSamples, nChannels;

   Surge::Headless::playAsConfigured(surge, terryRiley, &data, &nSamples, &nChannels);
   Surge::Headless::writeToStream(data, nSamples, nChannels, std::cout);

   if (data)
      delete[] data;
   delete surge;
}

void statsFromPlayingEveryPatch()
{
   /*
   ** This is a very clean use of the built in APIs, just making a surge
   ** and a scale then asking headless to map it onto every patch
   ** and call us back with a result
   */
   SurgeSynthesizer* surge = Surge::Headless::createSurge(44100);

   Surge::Headless::playerEvents_t scale =
       Surge::Headless::make120BPMCMajorQuarterNoteScale(0, 44100);

   auto callBack = [](const Patch& p, const PatchCategory& pc, const float* data, int nSamples,
                      int nChannels) -> void {
      std::cout << "cat/patch = " <<  pc.name << " / " << std::left << std::setw(30) << p.name;

      if (nSamples * nChannels > 0)
      {
         const auto [mind, maxd] = std::minmax_element(data, data + nSamples * nChannels);

         float rms=0, L1=0;
         for( int i=0; i<nSamples*nChannels; ++i)
         {
            rms += data[i]*data[i];
            L1 += fabs(data[i]);
         }
         L1 = L1 / (nChannels*nSamples);
         rms = sqrt(rms / nChannels / nSamples );
         
         std::cout << "  range = [" << std::setw(10)
                   << std::fixed << *mind << ", " << std::setw(10) << std::fixed << *maxd << "]"
                   << " L1=" << L1
                   << " rms=" << rms
                   << " samp=" << nSamples << " chan=" << nChannels;
      }
      std::cout << std::endl;
   };

   Surge::Headless::playOnEveryPatch(surge, scale, callBack);
}

/*
** This is a simple main that, for now, you make call out to the application
** stub of your choosing. We have two in Applications and we call them both
*/
int main(int argc, char** argv)
{
   simpleOscillatorToStdOut();
   statsFromPlayingEveryPatch();

   return 0;
}
