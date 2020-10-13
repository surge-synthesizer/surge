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

#include <random>

enum modsrctype
{
   mst_undefined = 0,
   mst_controller,
   mst_adsr,
   mst_lfo,
   mst_stepseq,
};

enum modsources
{
   ms_original = 0,
   ms_velocity,
   ms_keytrack,
   ms_polyaftertouch,
   ms_aftertouch,
   ms_pitchbend,
   ms_modwheel,
   ms_ctrl1,
   ms_ctrl2,
   ms_ctrl3,
   ms_ctrl4,
   ms_ctrl5,
   ms_ctrl6,
   ms_ctrl7,
   ms_ctrl8,
   ms_ampeg,
   ms_filtereg,
   ms_lfo1,
   ms_lfo2,
   ms_lfo3,
   ms_lfo4,
   ms_lfo5,
   ms_lfo6,
   ms_slfo1,
   ms_slfo2,
   ms_slfo3,
   ms_slfo4,
   ms_slfo5,
   ms_slfo6,
   ms_timbre,
   ms_releasevelocity,
   ms_random_bipolar,
   ms_random_unipolar,
   ms_alternate_bipolar,
   ms_alternate_unipolar,
   ms_breath,
   ms_expression,
   ms_sustain,
   ms_lowest_key,
   ms_highest_key,
   ms_latest_key,
   n_modsources,
};

const int modsource_display_order[n_modsources] =
{
    ms_original,
    ms_velocity,
    ms_releasevelocity,
    ms_keytrack,
    ms_lowest_key,
    ms_highest_key,
    ms_latest_key,
    ms_polyaftertouch,
    ms_aftertouch,
    ms_modwheel,
    ms_breath,
    ms_expression,
    ms_sustain,
    ms_pitchbend,
    ms_timbre,
    ms_alternate_bipolar,
    ms_alternate_unipolar,
    ms_random_bipolar,
    ms_random_unipolar,
    ms_filtereg,
    ms_ampeg,
    ms_lfo1,
    ms_lfo2,
    ms_lfo3,
    ms_lfo4,
    ms_lfo5,
    ms_lfo6,
    ms_slfo1,
    ms_slfo2,
    ms_slfo3,
    ms_slfo4,
    ms_slfo5,
    ms_slfo6,
    ms_ctrl1,
    ms_ctrl2,
    ms_ctrl3,
    ms_ctrl4,
    ms_ctrl5,
    ms_ctrl6,
    ms_ctrl7,
    ms_ctrl8,
};

const int n_customcontrollers = 8;
const int num_metaparameters = n_customcontrollers;
extern float samplerate_inv;
extern float samplerate;

const char modsource_names_button[n_modsources][32] =
{
   "Off",
   "Velocity",
   "Keytrack",
   "Poly AT",
   "Channel AT",
   "Pitch Bend",
   "Modwheel",
   "Macro 1",
   "Macro 2",
   "Macro 3",
   "Macro 4",
   "Macro 5",
   "Macro 6",
   "Macro 7",
   "Macro 8",
   "Amp EG",
   "Filter EG",
   "LFO 1",
   "LFO 2",
   "LFO 3",
   "LFO 4",
   "LFO 5",
   "LFO 6",
   "S-LFO 1",
   "S-LFO 2",
   "S-LFO 3",
   "S-LFO 4",
   "S-LFO 5",
   "S-LFO 6",
   "Timbre",
   "Rel Velocity",
   "Random",
   "Random Uni",
   "Alternate",
   "Alternate Uni",
   "Breath",
   "Expression",
   "Sustain",
   "Lowest Key",
   "Highest Key",
   "Latest Key",
};

const char modsource_names[n_modsources][32] =
{
   "Off",
   "Velocity",
   "Keytrack",
   "Polyphonic Aftertouch",
   "Channel Aftertouch",
   "Pitch Bend",
   "Modulation Wheel",
   "Macro 1",
   "Macro 2",
   "Macro 3",
   "Macro 4",
   "Macro 5",
   "Macro 6",
   "Macro 7",
   "Macro 8",
   "Amp EG",
   "Filter EG",
   "Voice LFO 1",
   "Voice LFO 2",
   "Voice LFO 3",
   "Voice LFO 4",
   "Voice LFO 5",
   "Voice LFO 6",
   "Scene LFO 1",
   "Scene LFO 2",
   "Scene LFO 3",
   "Scene LFO 4",
   "Scene LFO 5",
   "Scene LFO 6",
   "Timbre",
   "Release Velocity",
   "Random Bipolar",
   "Random Unipolar",
   "Alternate Bipolar",
   "Alternate Unipolar",
   "Breath",
   "Expression",
   "Sustain Pedal",
   "Lowest Key",
   "Highest Key",
   "Latest Key",
};

const char modsource_names_tag[n_modsources][32] =
{
   "off",
   "vel",
   "keytrack",
   "poly_at",
   "chan_at",
   "pbend",
   "mwheel",
   "macro1",
   "macro2",
   "macro3",
   "macro4",
   "macro5",
   "macro6",
   "macro7",
   "macro8",
   "amp_eg",
   "filter_eg",
   "lfo1",
   "lfo2",
   "lfo3",
   "lfo4",
   "lfo5",
   "lfo6",
   "slfo1",
   "slfo2",
   "slfo3",
   "slfo4",
   "slfo5",
   "slfo6",
   "timbre",
   "release_vel",
   "random",
   "random_uni",
   "alt",
   "alt_uni",
   "breath",
   "expr",
   "sustain",
   "lowest_key",
   "highest_key",
   "latest_key",
};

const int modsource_grid_xy[n_modsources][2] = {
    {0, 0}, {0, 3}, {6, 7}, {2, 3}, {3, 3}, {4, 3}, {5, 3},           // Velocity, Keytrack, Poly AT, Channel AT, Pitch Bend, Modwheel
    {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}, {5, 0}, {6, 0}, {7, 0},   // Macro 1-8
    {7, 5}, {6, 5},                                                   // AEG, FEG
    {0, 5}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {5, 5},                   // LFO 1-6
    {0, 7}, {1, 7}, {2, 7}, {3, 7}, {4, 7}, {5, 7},                   // S-LFO 1-6
    {9, 3}, {1, 3}, {8, 5}, {8, 5}, {9, 5}, {9, 5},                   // Timbre, Release Velocity, Random Bi/Uni, Alternate Bi/Uni
    {6, 3}, {7, 3}, {8, 3}, {7, 7}, {8, 7}, {9, 7},                   // Breath, Expression, Sustain, Lowest/Highest/Latest Key
};

inline bool isScenelevel(modsources ms)
{
   return (((ms <= ms_ctrl8) || ((ms >= ms_slfo1) && (ms <= ms_slfo6))) && ((ms != ms_velocity) &&
          (ms != ms_keytrack) && (ms != ms_polyaftertouch) && (ms != ms_releasevelocity))) ||
          ((ms >= ms_breath) && (ms <= ms_latest_key));
}

inline bool canModulateMonophonicTarget(modsources ms)
{
   return isScenelevel(ms) || ms == ms_aftertouch;
}

inline bool isCustomController(modsources ms)
{
   return (ms >= ms_ctrl1) && (ms <= ms_ctrl8);
}

inline bool isEnvelope(modsources ms)
{
   return (ms == ms_ampeg) || (ms == ms_filtereg);
}

inline bool isLFO(modsources ms)
{
   return (ms >= ms_lfo1) && (ms <= ms_slfo6);
}

inline bool canModulateModulators(modsources ms)
{
   return (ms != ms_ampeg) && (ms != ms_filtereg);
}

inline bool isVoiceModulator(modsources ms)
{
   return !((ms >= ms_slfo1) && (ms <= ms_slfo6));
}

inline bool canModulateVoiceModulators(modsources ms)
{
   return (ms <= ms_ctrl8) || ms == ms_timbre;
}

struct ModulationRouting
{
   int source_id;
   int destination_id;
   float depth;
};

class ModulationSource
{
public:
   ModulationSource()
   {}
   virtual ~ModulationSource()
   {}
   virtual const char* get_title()
   {
      return 0;
   }
   virtual int get_type()
   {
      return mst_undefined;
   }
   virtual void process_block()
   {}
   virtual void attack(){};
   virtual void release(){};
   virtual void reset(){};
   virtual float get_output()
   {
      return output;
   }
   virtual float get_output01()
   {
      return output;
   }
   virtual bool per_voice()
   {
      return false;
   }
   virtual bool is_bipolar()
   {
      return false;
   }
   virtual void set_bipolar(bool b)
   {}
   float output;
};

class ControllerModulationSource : public ModulationSource
{
public:
   // Smoothing and Shaping Behaviors
   enum SmoothingMode {
      LEGACY=-1, // This is (1) the exponential backoff and (2) not streamed.
      SLOW_EXP, // Legacy with a sigma clamp
      FAST_EXP, // Faster Legacy with a sigma clamp
      FAST_LINE, // Linearly move
      DIRECT  // Apply the value directly
   } smoothingMode = LEGACY;

   ControllerModulationSource()
   {
      target = 0.f;
      output = 0.f;
      bipolar = false;
      changed = true;
      smoothingMode = LEGACY;
   }
   ControllerModulationSource(SmoothingMode mode) : ControllerModulationSource()
   {
      smoothingMode = mode;
   }

   virtual ~ControllerModulationSource()
   {}
   void set_target(float f)
   {
      target = f;
      startingpoint = output;
      changed = true;
   }

   void init(float f)
   {
      target = f;
      output = f;
      startingpoint = f;
      changed = true;
   }

   void set_target01(float f, bool updatechanged = true)
   {
      if (bipolar)
         target = 2.f * f - 1.f;
      else
         target = f;
      startingpoint = output;
      if (updatechanged)
         changed = true;
   }

   virtual float get_output01() override
   {
      if (bipolar)
         return 0.5f + 0.5f * output;
      return output;
   }

   virtual float get_target01()
   {
      if (bipolar)
         return 0.5f + 0.5f * target;
      return target;
   }

   virtual bool has_changed(bool reset)
   {
      if (changed)
      {
         if (reset)
            changed = false;
         return true;
      }
      return false;
   }

   virtual void reset() override
   {
      target = 0.f;
      output = 0.f;
      bipolar = false;
   }
   inline void processSmoothing( SmoothingMode mode, float sigma )
   {
      if (mode == LEGACY || mode == SLOW_EXP || mode == FAST_EXP)
      {
         float b = fabs(target - output);
         if (b < sigma && mode != LEGACY)
         {
            output = target;
         }
         else
         {
            float a = (mode == FAST_EXP ? 0.99f : 0.9f) * 44100 * samplerate_inv * b;
            output = (1 - a) * output + a * target;
         }
         return;
      };
      if (mode == FAST_LINE)
      {
         /*
          * Apply a constant change until we get there.
          * Rate is set so we cover the entire range (0,1)
          * in 50 blocks at 44k
          */
         float sampf = samplerate / 44100;
         float da = ( target - startingpoint ) / ( 50 * sampf );
         float b = target - output;
         if( fabs( b ) < fabs( da ) )
         {
            output = target;
         }
         else
         {
            output += da;
         }
      }
      if( mode == DIRECT )
      {
         output = target;
      }
   }
   virtual void process_block() override
   {
      processSmoothing( smoothingMode, smoothingMode == FAST_EXP ? 0.005f : 0.0025f );
   }

   virtual bool process_block_until_close(float sigma)
   {
      if( smoothingMode == LEGACY )
         processSmoothing(SLOW_EXP, sigma);
      else
         processSmoothing(smoothingMode, sigma);

      return (output != target ); // continue
   }

   virtual bool is_bipolar() override
   {
      return bipolar;
   }
   virtual void set_bipolar(bool b) override
   {
      bipolar = b;
   }

   float target, startingpoint;
   int id; // can be used to assign the controller to a parameter id
   bool bipolar;
   bool changed;


};

class RandomModulationSource : public ModulationSource
{
public:
   RandomModulationSource( bool bp ) : bipolar(bp) {
      std::random_device rd;
      gen = std::minstd_rand(rd());
      if( bp ) dis = std::uniform_real_distribution<float>(-1.0,1.0);
      else dis = std::uniform_real_distribution<float>(0.0,1.0);
   }

   virtual void attack() override {
      output = dis(gen);
   }
   
   bool bipolar;
   std::minstd_rand gen;
   std::uniform_real_distribution<float> dis;
};

class AlternateModulationSource : public ModulationSource
{
public:
   AlternateModulationSource(bool bp) : state(false) {
      if( bp ) nv = -1;
      else nv = 0;

      pv = 1;
   }

   virtual void attack() override{
      if( state )
         output = pv;
      else
         output = nv;
      state = ! state;
   }
   
   bool state;
   float nv, pv;
};
