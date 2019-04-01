#include "Stress.h"
#include "Player.h"
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>

void Surge::Headless::createAndDestroyWithScaleAndRandomPatch(int timesToTry)
{
   SurgeSynthesizer* synth = Surge::Headless::createSurge(44100);
   std::default_random_engine generator;
   generator.seed(17);
   std::uniform_int_distribution<int> distribution(0, synth->storage.patch_list.size() - 1);
   delete synth;

   int faultCount = 0;
   
   playerEvents_t scale = make120BPMCMajorQuarterNoteScale();
   for (int i = 0; i < timesToTry && faultCount < 3; ++i)
   {
      SurgeSynthesizer* synth = Surge::Headless::createSurge(44100);
      int patch = distribution(generator);
      std::cout << std::setw(5) << i << ": Patch " << std::setw(5) << patch << " = " << std::setw(50)
                << synth->storage.patch_list[patch].name;
      synth->loadPatch(patch);

      float* data = nullptr;
      int nSamples, nChannels;
      Surge::Headless::playAsConfigured(synth, scale, &data, &nSamples, &nChannels);

      const auto minmaxres = std::minmax_element(data, data + nSamples * nChannels);
      auto mind = minmaxres.first;
      auto maxd = minmaxres.second;

      std::cout << "   Range: [" << *mind << ", " << *maxd << "]" << std::endl;
      if (fabs(*mind) > 5 || fabs(*maxd) > 5)
      {
         std::cout << " **** FAULT " << faultCount << " **** " << std::endl;
         for( int j=0; j<nSamples*nChannels; ++j )
             if(data[j] == *maxd)
                 std::cout << "Element " << j << " " << j / nChannels << " " << data[j] << std::endl;
         
         std::ostringstream oss;
         oss << "faultWav" << faultCount << ".csv";
         std::ofstream of(oss.str().c_str());
         for( int j=0; j<nSamples; ++j)
         {
             of << j << "," << data[j * nChannels ] << ", " << data[ j*nChannels + 1 ] << std::endl;
         }
         of.close();
         faultCount ++;
      }

      delete[] data;
      delete synth;
   }
}
