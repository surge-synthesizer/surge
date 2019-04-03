#include "Stress.h"
#include "Player.h"
#include <random>
#include <iostream>
#include <iomanip>
#include <sstream>

void Surge::Headless::pullInitSamplesWithNoNotes(int sampleCount)
{
   SurgeSynthesizer* synth = Surge::Headless::createSurge(44100);
   Surge::Headless::playerEvents_t events;
   Surge::Headless::Event on;
   on.type = Surge::Headless::Event::NO_EVENT;
   on.atSample = sampleCount;
   events.push_back(on);

   float *data;
   int nSamples, nChannels;
   Surge::Headless::playAsConfigured(synth, events, &data, &nSamples, &nChannels);

   float sumAbs = 0;
   for(int i=0;i<nSamples*nChannels; ++i )
   {
       sumAbs += fabs(data[i]);
       std::cout << i << " " << data[i] << std::endl;
   }
   std::cout << "Sum of Absolute Value of play is " << sumAbs << std::endl;
   
   delete[] data;
   delete synth;
}

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

      float rms=0, L1=0;
      for( int i=0; i<nSamples*nChannels; ++i)
      {
          rms += data[i]*data[i];
          L1 += fabs(data[i]);
      }
      L1 = L1 / (nChannels*nSamples);
      rms = sqrt(rms / nChannels / nSamples );

      std::cout << "   Range: [" << *mind << ", " << *maxd << "] RMS=" << rms << " RMS/MAX=" << rms / *maxd << "  L1=" << L1 << std::endl;
      if ((fabs(*mind) > 5 || fabs(*maxd) > 5) && (rms / *maxd) < 0.5 / 8.0)
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

         std::ostringstream woss;
         woss << "faultWav" << faultCount << ".wav";
         Surge::Headless::writeToWav(data, nSamples, nChannels, 44100, woss.str());

         faultCount ++;
      }

      delete[] data;
      delete synth;
   }
}
