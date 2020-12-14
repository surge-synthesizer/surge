#include "HeadlessUtils.h"
#include "Player.h"
#include <iostream>
#include <sstream>


namespace Surge
{
namespace Headless
{
namespace NonTest
{

void restreamTemplatesWithModifications()
{
   auto templatesDir = string_to_path("resources/data/patches_factory/Templates" );
   for( auto d : fs::directory_iterator(templatesDir))
   {
      std::cout << "ReStream " << path_to_string(d) << std::endl;
      auto surge = Surge::Headless::createSurge( 44100 );
      surge->loadPatchByPath(path_to_string(d).c_str(), -1, "Templates" );
      for( int i=0; i<10; ++i ) surge->process();

      auto oR = surge->storage.getPatch().streamingRevision;
      std::cout << "  Stream Revision : " << oR << " vs " << ff_revision << std::endl;
      if( oR < 15 )
      {
         std::cout << "  Fixing Comb Filter" << std::endl;
         surge->storage.getPatch().correctlyTuneCombFilter = true;
      }

      if( oR == ff_revision )
      {
         std::cout << "  Patch is streamed at latest; skipping" << std::endl;
      }
      else
      {
         for (int i = 0; i < 10; ++i)
            surge->process();

         surge->savePatchToPath(d);
      }
   }
}

void statsFromPlayingEveryPatch()
{
   /*
   ** This is a very clean use of the built in APIs, just making a surge
   ** and a scale then asking headless to map it onto every patch
   ** and call us back with a result
   */
   auto surge = Surge::Headless::createSurge(44100);

   Surge::Headless::playerEvents_t scale =
       Surge::Headless::make120BPMCMajorQuarterNoteScale(0, 44100);

   auto callBack = [](const Patch& p, const PatchCategory& pc, const float* data, int nSamples,
                      int nChannels) -> void {
     bool writeWav = false; // toggle this to true to write each sample to a wav file
     std::cout << "cat/patch = " <<  pc.name << " / " << std::left << std::setw(30) << p.name;

     if (nSamples * nChannels > 0)
     {
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

        std::cout << "  range = [" << std::setw(10)
                  << std::fixed << *mind << ", " << std::setw(10) << std::fixed << *maxd << "]"
                  << " L1=" << L1
                  << " rms=" << rms
                  << " samp=" << nSamples << " chan=" << nChannels;
        if (writeWav)
        {
           std::ostringstream oss;
           oss << "headless " << pc.name << " " << p.name << ".wav";
           Surge::Headless::writeToWav(data, nSamples, nChannels, 44100, oss.str());
        }
     }
     std::cout << std::endl;
   };

   Surge::Headless::playOnEveryPatch(surge, scale, callBack);
}


void playSomeBach()
{
   auto surge = Surge::Headless::createSurge(44100);

   std::string tmpdir = "/tmp";
   std::string fname = tmpdir + "/988-v05.mid";

   FILE* tf = fopen(fname.c_str(), "r");
   if (!tf)
   {
      std::string cmd = "curl -o " + fname + " http://www.jsbach.net/midi/bwv988/988-v05.mid";
      auto res = system(cmd.c_str());
   }
   else
      fclose(tf);

   std::string otp = "DX EP 1";
   for (int i = 0; i < surge->storage.patch_list.size(); ++i)
   {
      Patch p = surge->storage.patch_list[i];
      if (p.name == otp)
      {
         std::cout << "Found '" << otp << "' patch @" << i << std::endl;
         surge->loadPatch(i);
         break;
      }
   }
   Surge::Headless::renderMidiFileToWav(surge, fname, fname + ".wav");
}

void filterAnalyzer( int ft, int sft, float cut )
{
   std::cout << "# Analyzing filter " << ft << " " << sft << " " << cut  << std::endl
             << "# " << fut_names[ft] << std::endl;

   std::array<std::vector<float>,127> ampRatios;
   std::array<std::vector<float>,127> phases;
   std::vector<float> resonances;
   for( float res = 0; res <= 1.0; res += 0.2 )
   {
      res = limit_range( res, 0.f, 0.99f );
      std::cout << "RES is " << res << std::endl;
      resonances.push_back(res);
      /*
       * Our strategy here is to use a sin oscilllator in both in dual mode
       * read off the L chan audio from both scenes and then calculate the RMS
       * and phase difference. I hope
       */
      auto surge = Surge::Headless::createSurge(48000);
      surge->storage.getPatch().scenemode.val.i = sm_dual;
      surge->storage.getPatch().scene[0].filterunit[0].type.val.i = ft;
      surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = sft;
      surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = cut;
      surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = res;

      surge->storage.getPatch().scene[0].osc[0].type.val.i = ot_sine;

      surge->storage.getPatch().scene[1].filterunit[0].type.val.i = fut_none;
      surge->storage.getPatch().scene[1].filterunit[0].subtype.val.i = 0;
      surge->storage.getPatch().scene[1].osc[0].type.val.i = ot_sine;

      auto proc = [surge](int n) {
         for (int i = 0; i < n; ++i)
            surge->process();
      };
      proc(10);
      char txt[256];
      surge->storage.getPatch().scene[0].filterunit[0].cutoff.get_display(txt);
      std::cout << "# CUTOFF = " << txt << std::endl;

      for (int i = 0; i < 127; ++i)
      {
         proc(50); // let silence reign
         surge->playNote(0, i, 127, 0);

         // OK so we want probably 5000 samples or so
         const int n_blocks = 1024;
         const int n_samples = n_blocks * BLOCK_SIZE;
         float ablock[n_samples];
         float bblock[n_samples];

         for (int b = 0; b < n_blocks; ++b)
         {
            surge->process();
            memcpy(ablock + b * BLOCK_SIZE, (const void*)(&surge->sceneout[0][0][0]),
                   BLOCK_SIZE * sizeof(float));
            memcpy(bblock + b * BLOCK_SIZE, (const void*)(&surge->sceneout[1][0][0]),
                   BLOCK_SIZE * sizeof(float));
         }

         float rmsa = 0, rmsb = 0;
         for (int s = 0; s < n_samples; ++s)
         {
            rmsa += ablock[s] * ablock[s];
            rmsb += bblock[s] * bblock[s];
         }

         surge->releaseNote(0, i, 0);
         ampRatios[i].push_back(rmsa/rmsb);
      }
   }

#define fstream std::cout
                // auto fstream = std::cout;
   fstream << "Note";
   for( auto r : resonances ) fstream << ", res " << r;
   fstream << std::endl;

   for( int i=0; i<127; ++i )
   {
      fstream << i;
      for( auto r : ampRatios[i] ) fstream << ", " << r;
      fstream << std::endl;
   }
}

void generateNLFeedbackNorms()
{
   /*
    * This generates a normalization for the NL feedback. Here's how you use it
    *
    * In filters/NonlinearFeedback.cpp if there's a normalization table there
    * disable it and make sure the law is just 1/f^1/3
    *
    * Build and run this with surge-headless --non-test --generate-nlf-norms
    *
    * Copy and paste the resulting table back into NLFB.cpp
    *
    * What it does is: Play a C major chord on init into the OBXD-24 LP for 3 seconds
    * with resonance at 1, calculate the RMS
    *
    * Then do that for every subtype of NLF and calculate the RMS
    *
    * Then print out ratio basically.
    */

   bool genLowpass = false; // if false generate highpass

   auto setup = [](int type, int subtype) {
      auto surge = Surge::Headless::createSurge(48000);
      for (auto i = 0; i < 10; ++i)
      {
         surge->process();
      }
      surge->storage.getPatch().scene[0].filterunit[0].type.val.i = type;
      surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = subtype;
      surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = -9;
      surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = 0.95;
      for (auto i = 0; i < 10; ++i)
      {
         surge->process();
      }
      char td[256], sd[256], cd[256];

      surge->storage.getPatch().scene[0].filterunit[0].type.get_display(td);
      surge->storage.getPatch().scene[0].filterunit[0].subtype.get_display(sd);
      surge->storage.getPatch().scene[0].filterunit[0].cutoff.get_display(cd);
      std::cout << "// Created a filter: " << td << " / " << sd << " Cutoff=" << cd  << std::endl;
      return surge;
   };

   auto singleRMS = [genLowpass](std::shared_ptr<SurgeSynthesizer> surge)
   {
      for (auto i=0;i<10; ++i ) {
         surge->process();
      }

      std::vector<int> notes = {{ 48, 60, 65, 67 }};
      int noff = -12;
      if( ! genLowpass ) noff = +15;
      for( auto n : notes )
      {
         surge->playNote( 0, n-noff, 127, 0 );
         surge->process();
      }

      for( int i=0; i<100; ++i )
         surge->process();

      int blocks = (floor)( 2.0 * samplerate * BLOCK_SIZE_INV );
      double rms = 0; // double to avoid so much overflow risk
      for( int i=0; i<blocks; ++i )
      {
         surge->process();
         for( int s=0; s<BLOCK_SIZE; ++s )
         {
            rms += surge->output[0][s] * surge->output[0][s] + surge->output[1][s] * surge->output[1][s];
         }
      }

      for( auto n : notes )
      {
         surge->releaseNote(0,n-noff,0);
         surge->process();
      }
      for( int i=0; i<100; ++i )
         surge->process();

      return sqrt(rms);
   };

   auto playRMS = [singleRMS](std::shared_ptr<SurgeSynthesizer> surge)
   {
      float ra = 0;
      int nAvg = 20;

      for( int i=0; i<nAvg; ++i )
      {
         float lra = singleRMS(surge);
         std::cout << "LRA = " << lra << std::endl;
         ra += lra;
      }
      return ra / nAvg;
   };

   // Remember if you don't set the normalization to FALSE in NLF.cpp this will be bad
   auto basecaseRMS = playRMS(setup(genLowpass ? fut_obxd_4pole : fut_hp24,  genLowpass ? 3 : 0));
   std::cout << "// no filter RMS is " << basecaseRMS << std::endl;

   std::vector<float> results;
   for( int nlfst = 0; nlfst < 12; nlfst++ )
   {
      auto r = playRMS( setup( genLowpass ? fut_cutoffwarp_lp : fut_cutoffwarp_hp, nlfst ));
      //auto r = playRMS( setup( fut_cutoffwarp_hp, nlfst));
      std::cout << "// RMS=" << r << " Ratio=" << r / basecaseRMS << std::endl;
      results.push_back(r);
   }
   std::cout << "      bool useNormalization = true;\n"
             << "      float normNumerator = 1.0f;\n"
             << "      float " << ( genLowpass ? "lp" : "hp" ) << "NormTable[12] = {";
   std::string pfx = "\n         ";
   for( auto v : results ) {
      std::cout << pfx << basecaseRMS / v;
      pfx = ",\n         ";
   }
   std::cout << "\n       };\n"
             << "       if (useNormalization) normNumerator = lpNormTable[subtype];\n";
}

}
}
}

