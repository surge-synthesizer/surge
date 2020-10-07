/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

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
              MidiChannelState* voiceChannelState,
              bool mpeEnabled
       );
   ~SurgeVoice();

   void release();
   void uber_release();

   bool process_block(QuadFilterChainState&, int);
   void GetQFB(); // Get the updated registers from the QuadFB
   void legato(int key, int velocity, char detune);
   void switch_toggled();
   void freeAllocatedElements();
   int osctype[n_oscs];
   SurgeVoiceState state;
   int age, age_release;

   /*
   ** Given a note0 and an oscilator this returns the appropriate note.
   ** This is a pretty easy calculation in non-absolute mode. Just add.
   ** But in absolute mode you need to find the virtual note which would
   ** map to that frequency shift.
   */
   inline float noteShiftFromPitchParam( float note0 /* the key + octave */, int oscNum /* The osc for pitch diffs */)
   {
       if( scene->osc[oscNum].pitch.absolute )
       {
           // remember note_to_pitch is linear interpolation on storage->table_pitch from
           // position note + 256 % 512
           // OK so now what we are searching for is the pair which surrounds us plus the pitch drift... so
           float fqShift = 10 * localcopy[scene->osc[oscNum].pitch.param_id_in_scene].f *
               (scene->osc[oscNum].pitch.extend_range ? 12.f : 1.f);
           float tableNote0 = note0 + 256;

           int tableIdx = (int)tableNote0;
           if( tableIdx > 0x1fe )
               tableIdx = 0x1fe;
           float tableFrac = tableNote0 - tableIdx;
           
           // so just iterate up. Deal with negative also of course. Since we will always be close just
           // do it brute force for now but later we can do a binary or some such.
           float pitch0 = storage->table_pitch[tableIdx] * ( 1.0 - tableFrac ) + storage->table_pitch[tableIdx + 1] * tableFrac;
           float targetPitch = pitch0 + fqShift / Tunings::MIDI_0_FREQ;
           if( targetPitch < 0 )
              targetPitch = 0.01;
           
           if( fqShift > 0 )
           {
               while( tableIdx < 0x1fe )
               {
                   float pitch1 = storage->table_pitch[tableIdx + 1];
                   if( pitch0 <= targetPitch && pitch1 > targetPitch )
                   {
                       break;
                   }
                   pitch0 = pitch1;
                   tableIdx ++;
               }
           }
           else
           {
               while( tableIdx > 0 )
               {
                   float pitch1 = storage->table_pitch[tableIdx - 1];
                   if( pitch0 >= targetPitch && pitch1 < targetPitch )
                   {
                       tableIdx --;
                       break;
                   }
                   pitch0 = pitch1;
                   tableIdx --;
               }
           }

           // So what's the frac
           // (1-x) * [tableIdx] + x * [tableIdx+1] = targetPitch
           // Or: x = ( target - table) / ( [ table+1 ] - [table] );
           float frac = (targetPitch - storage->table_pitch[tableIdx]) /
               ( storage->table_pitch[tableIdx + 1] - storage->table_pitch[tableIdx] );
           // frac = 1 -> targetpitch = +1; frac = 0 -> targetPitch

           // std::cout << note0 << " " << tableIdx << " " << frac << " " << fqShift << " " << targetPitch << std::endl;
           return tableIdx + frac - 256;
       }
       else
       {
           return note0 + localcopy[scene->osc[oscNum].pitch.param_id_in_scene].f *
               (scene->osc[oscNum].pitch.extend_range ? 12.f : 1.f);
       }
   }
   
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

   float octaveSize = 12.0f;
   
   bool osc1, osc2, osc3, ring12, ring23, noise;
   int FMmode;
   float noisegenL[2], noisegenR[2];

   std::unique_ptr<Oscillator> osc[3];

   std::array<ModulationSource*, n_modsources> modsources;

   ModulationSource velocitySource, releaseVelocitySource;
   ModulationSource keytrackSource;
   ControllerModulationSource polyAftertouchSource;
   ControllerModulationSource monoAftertouchSource;
   ControllerModulationSource timbreSource;
   ModulationSource rndUni, rndBi, altUni, altBi;
   AdsrEnvelope ampEGSource;
   AdsrEnvelope filterEGSource;

   // filterblock stuff
   int id_cfa, id_cfb, id_kta, id_ktb, id_emoda, id_emodb, id_resoa, id_resob, id_drive, id_vca,
       id_vcavel, id_fbalance, id_feedback;

   // MPE special cases
   bool mpeEnabled;
};
