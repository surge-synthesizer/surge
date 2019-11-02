//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

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
   // ms_arpeggiator,
   ms_timbre,
   ms_releasevelocity,
   n_modsources,
   /*ms_stepseq1,
   ms_stepseq2,
   ms_feg1,
   ms_feg2,*/

};

const int n_customcontrollers = 8; // TODO remove this one
const int num_metaparameters = n_customcontrollers;

const char modsource_abberations_button[n_modsources][32] = {
    "Off",       "Velocity", "Keytrack", "Poly AT", "Channel AT", "Pitchbend", "Modwheel", "Ctrl 1",
    "Ctrl 2",    "Ctrl 3",   "Ctrl 4",   "Ctrl 5",  "Ctrl 6",   "Ctrl 7",    "Ctrl 8",   "Amp EG",
    "Filter EG", "LFO 1",    "LFO 2",    "LFO 3",   "LFO 4",    "LFO 5",     "LFO 6",    "SLFO 1",
    "SLFO 2",    "SLFO 3",   "SLFO 4",   "SLFO 5",  "SLFO 6",   "Timbre", "Rel Velocity" /*,"Arpeggio"*/};

const char modsource_abberations[n_modsources][32] = {"Off",
                                                      "Velocity",
                                                      "Keytrack",
                                                      "Polyphonic Aftertouch",
                                                      "Channel Aftertouch",
                                                      "Pitch Bend",
                                                      "Modulation Wheel",
                                                      "Control 1",
                                                      "Control 2",
                                                      "Control 3",
                                                      "Control 4",
                                                      "Control 5",
                                                      "Control 6",
                                                      "Control 7",
                                                      "Control 8",
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
                                                      "Release Velocity"
                                                      /*,"Arpeggio"*/};

const char modsource_abberations_short[n_modsources][32] = {
    "Off", "Velocity", "Keytrack", "Poly AT", "Channel AT", "Pitch Bend", "Modwheel", "Ctrl 1",
    "Ctrl 2", "Ctrl 3", "Ctrl 4", "Ctrl 5", "Ctrl 6", "Ctrl 7", "Ctrl 8", "Amp EG",
    "Filter EG", "LFO 1", "LFO 2", "LFO 3", "LFO 4", "LFO 5", "LFO 6", "SLFO 1",
    "SLFO 2", "SLFO 3", "SLFO 4", "SLFO 5", "SLFO 6", "Timbre", "Rel Vel" /*,"Arpeggio"*/};

const int modsource_grid_xy[n_modsources][2] = {
    {0, 0}, {0, 0}, {1, 0}, {2, 0},  {3, 0}, {4, 0}, {5, 0},          // vel -> mw
    {7, 0}, {8, 0}, {9, 0}, {10, 0}, {7, 3}, {8, 3}, {9, 3}, {10, 3}, // ctrl 1-8
    {6, 2}, {6, 4},                                                   // EGs
    {0, 2}, {1, 2}, {2, 2}, {3, 2},  {4, 2}, {5, 2},                  // LFO
    {0, 4}, {1, 4}, {2, 4}, {3, 4},  {4, 4}, {5, 4},                  // SLFO
    {6, 0}, {0, 0}                                                            // Timbre, relvel is special
};

inline bool isScenelevel(modsources ms)
{
   return ((ms <= ms_ctrl8) || ((ms >= ms_slfo1) && (ms <= ms_slfo6))) && (ms != ms_velocity) &&
      (ms != ms_keytrack) && (ms != ms_polyaftertouch) && (ms != ms_timbre) && (ms != ms_releasevelocity);
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
   return /*(ms <= ms_ctrl8)&&*/ (ms != ms_ampeg) &&
          (ms != ms_filtereg); //&&(ms!=ms_velocity)&&(ms!=ms_keytrack)&&(ms!=ms_polyaftertouch);
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
   ControllerModulationSource()
   {
      target = 0.f;
      output = 0.f;
      bipolar = false;
      changed = true;
   }
   virtual ~ControllerModulationSource()
   {}
   void set_target(float f)
   {
      target = f;
      changed = true;
   }

   void init(float f)
   {
      target = f;
      output = f;
      changed = true;
   }

   void set_target01(float f, bool updatechanged = true)
   {
      if (bipolar)
         target = 2.f * f - 1.f;
      else
         target = f;

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
   virtual void process_block() override
   {
      float b = fabs(target - output);
      float a = 0.4f * b;
      output = (1 - a) * output + a * target;
   }

   virtual bool process_block_until_close(float sigma)
   {
      float b = fabs(target - output);
      if (b < sigma)
      {
         output = target;
         return false; // this interpolator has reached it's target and is no longer needed
      }
      float a = 0.4f * b;
      output = (1 - a) * output + a * target;
      return true; // continue
   }

   virtual bool is_bipolar() override
   {
      return bipolar;
   }
   virtual void set_bipolar(bool b) override
   {
      bipolar = b;
   }

   float target;
   int id; // can be used to assign the controller to a parameter id
   bool bipolar;
   bool changed;
};
