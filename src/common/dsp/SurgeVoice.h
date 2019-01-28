//-------------------------------------------------------------------------------------------------------
//		Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "SurgeStorage.h"
#include "Oscillator.h"
#include "SurgeVoiceState.h"
#include "AdsrEnvelope.h"
#include "LfoModulationSource.h"
#include <vt_dsp/lipol.h>
#include "FilterCoefficientMaker.h"
#include "QuadFilterChain.h"
#include <array>

struct QuadFilterChainState;

class alignas(16) SurgeVoice
{
public:
   float output alignas(16)[2][BLOCK_SIZE_OS];
   lipol_ps osclevels alignas(16)[7];
   pdata localcopy alignas(16)[n_scene_params];
   float fmbuffer alignas(16)[BLOCK_SIZE_OS];
   // used for the 2>1<3 FM-mode (Needs the pointer earlier)

   SurgeVoice();
   SurgeVoice(SurgeStorage* storage,
              SurgeSceneStorage* scene,
              pdata* params,
              int key,
              int velocity,
              int channel,
              int scene_id,
              float detune,
              MidiKeyState* keyState,
              MidiChannelState* mainChannelState,
              MidiChannelState* voiceChannelState);
   ~SurgeVoice();

   void release();
   void uber_release();

   bool process_block(QuadFilterChainState&, int);
   void GetQFB(); // Get the updated registers from the QuadFB
   void legato(int key, int velocity, char detune);
   void switch_toggled();
   int osctype[n_oscs];
   SurgeVoiceState state;
   int age, age_release;

private:
   template <bool first> void calc_ctrldata(QuadFilterChainState*, int);
   void update_portamento();
   void set_path(bool osc1, bool osc2, bool osc3, int FMmode, bool ring12, bool ring23, bool noise);
   int routefilter(int);

   LfoModulationSource lfo[6];

   // Filterblock state storage
   void SetQFB(QuadFilterChainState*, int); // Set the parameters & registers
   QuadFilterChainState* fbq;
   int fbqi;

   struct
   {
      float Gain, FB, Mix1, Mix2, OutL, OutR, Out2L, Out2R, Drive, wsLPF, FBlineL, FBlineR;
      float Delay[4][MAX_FB_COMB + FIRipol_N];
      struct
      {
         float C[n_cm_coeffs], R[n_filter_registers];
         unsigned int WP;
         int type, subtype; // used for comparison with the last run
      } FU[4];
   } FBP;
   FilterCoefficientMaker CM[2];

   // data
   int lag_id[8], pitch_id, octave_id, volume_id, pan_id, width_id;
   SurgeStorage* storage;
   SurgeSceneStorage *scene, *origscene;
   pdata* paramptr;
   int route[6];

   bool osc1, osc2, osc3, ring12, ring23, noise;
   int FMmode;
   float noisegenL[2], noisegenR[2];

   std::unique_ptr<Oscillator> osc[3];

   std::array<ModulationSource*, n_modsources> modsources;

   ModulationSource velocitySource;
   ModulationSource keytrackSource;
   ControllerModulationSource polyAftertouchSource;
   ModulationSource monoAftertouchSource;
   ModulationSource timbreSource;
   AdsrEnvelope ampEGSource;
   AdsrEnvelope filterEGSource;

   // filterblock stuff
   int id_cfa, id_cfb, id_kta, id_ktb, id_emoda, id_emodb, id_resoa, id_resob, id_drive, id_vca,
       id_vcavel, id_fbalance, id_feedback;
};
