#include "Player.h"

#if LIBMIDIFILE
#include "MidiFile.h"
#endif
#if LIBSNDFILE
#include <sndfile.h>
#endif

namespace Surge
{
namespace Headless
{

playerEvents_t makeHoldMiddleC(int forSamples, int withTail)
{
   return makeHoldNoteFor(60, forSamples, withTail );
}

playerEvents_t makeHoldNoteFor( int note, int forSamples, int withTail, int midiChannel )
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

void playAsConfigured(std::shared_ptr<SurgeSynthesizer> surge,
                      const playerEvents_t& events,
                      float** data,
                      int* nSamples,
                      int* nChannels)
{
   if (events.size() == 0)
      return;

   int desiredSamples = events.back().atSample;
   int blockCount = desiredSamples / BLOCK_SIZE + 1;
   int currEvt = 0;

   *nChannels = 2;
   *nSamples = blockCount * BLOCK_SIZE;
   size_t dataSize = *nChannels * *nSamples;
   float* ldata = new float[dataSize];
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
         switch( e.type )
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

void playOnPatch(std::shared_ptr<SurgeSynthesizer> surge,
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
    std::shared_ptr<SurgeSynthesizer> surge,
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

void playOnNRandomPatches(
    std::shared_ptr<SurgeSynthesizer> surge,
    const playerEvents_t& events,
    int nPlays,
    std::function<void(
        const Patch& p, const PatchCategory& c, const float* data, int nSamples, int nChannels)> cb)
{
   int nPresets = surge->storage.patch_list.size();
   int nCats = surge->storage.patch_category.size();

   for (auto i = 0; i < nPlays; ++i)
   {
      int rp = (int)( 1.0 * rand() / RAND_MAX * ( surge->storage.patch_list.size() - 1 ) );
      Patch p = surge->storage.patch_list[rp];
      PatchCategory pc = surge->storage.patch_category[p.category];

      float* data = NULL;
      int nSamples, nChannels;

      playOnPatch(surge, i, events, &data, &nSamples, &nChannels);
      cb(p, pc, data, nSamples, nChannels);
      
      if (data)
         delete[] data;
   }
}

void playMidiFile(std::shared_ptr<SurgeSynthesizer> synth,
                  std::string midiFileName,
                  long callBackEvery,
                  std::function<void(float* data, int nSamples, int nChannels)> dataCB)
{
#if LIBMIDIFILE
   smf::MidiFile mf;
   mf.read(midiFileName.c_str());
   mf.doTimeAnalysis();
   mf.linkNotePairs();
   mf.joinTracks();

   // float sampleRate = synth->getSampleRate();
   float sampleRate = 44100.0;

   int tracks = mf.getTrackCount();
   if (tracks != 1)
   {
      std::cerr << "Track Join failed\n";
      return;
   }

   int currentEvent = 0;
   double currentTime = mf[0][currentEvent].seconds;

   if ((callBackEvery) % (BLOCK_SIZE * 2) != 0)
   {
      std::cerr << "Please make callBackEvery a multiple of " << BLOCK_SIZE << "*2" << std::endl;
      return;
   }

   float* ldata = new float[callBackEvery * 2];
   long flidx = 0;
   double deltaT = BLOCK_SIZE / sampleRate;

   while (currentEvent < mf[0].size() ||
          (synth->getNonUltrareleaseVoices(0) > 0 || synth->getNonUltrareleaseVoices(1) > 0))
   {
      while (currentEvent < mf[0].size() && mf[0][currentEvent].seconds < currentTime + deltaT)
      {
         smf::MidiEvent& evt = mf[0][currentEvent];
         if (evt.isNoteOn())
            synth->playNote(1, evt.getKeyNumber(), evt.getVelocity(), 0);
         if (evt.isNoteOff())
            synth->releaseNote(1, evt.getKeyNumber(), evt.getVelocity());

         currentEvent++;
      }
      currentTime += deltaT;

      synth->process();
      for (int sm = 0; sm < BLOCK_SIZE; ++sm)
      {
         for (int oi = 0; oi < synth->getNumOutputs(); ++oi)
         {
            ldata[flidx++] = synth->output[oi][sm];
         }
      }
      if (flidx >= callBackEvery * 2)
      {
         flidx = 0;
         dataCB(ldata, callBackEvery, 2);
      }
   }

#else
   std::cout << "LIB_MIDIFILE not included on this platform." << std::endl;
#endif
}

void renderMidiFileToWav(std::shared_ptr<SurgeSynthesizer> surge,
                         std::string midiFileName,
                         std::string outputWavFile)
{
#if LIBSNDFILE
   SF_INFO sfinfo;
   sfinfo.channels = 2;
   sfinfo.samplerate = 44100; // FIX
   sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;

   SNDFILE* of = sf_open(outputWavFile.c_str(), SFM_WRITE, &sfinfo);

   playMidiFile(surge, midiFileName,
                1024 * 100, // write every 100k
                [of](float* data, int nSamples, int nChannels) {
                   sf_write_float(of, &data[0], nSamples * nChannels);
                   sf_write_sync(of);
                });
   sf_close(of);

#else
   std::cout << "renderMidiFileToWav requires libsndfile which is not available on your platform."
             << std::endl;
#endif
}

} // namespace Headless
} // namespace Surge
