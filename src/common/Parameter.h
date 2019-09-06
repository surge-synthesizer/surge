//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "globals.h"
#include <memory>
#include <string>

union pdata
{
   int i;
   bool b;
   float f;
};

enum valtypes
{
   vt_int = 0,
   vt_bool,
   vt_float,
};

enum ctrltypes
{
   ct_none,
   ct_percent,
   ct_percent_bidirectional,
   ct_pitch_octave,
   ct_pitch_semi7bp,
   ct_pitch_semi7bp_absolutable,
   ct_pitch,
   ct_fmratio,
   ct_fmratio_int,
   ct_pbdepth,
   ct_syncpitch,
   ct_amplitude,
   ct_reverbshape,
   ct_decibel,
   ct_decibel_narrow,
   ct_decibel_narrow_extendable,
   ct_decibel_extra_narrow,
   ct_decibel_attenuation,
   ct_decibel_attenuation_large,
   ct_decibel_fmdepth,
   ct_decibel_extendable,
   ct_freq_audible,
   ct_freq_mod,
   ct_freq_hpf,
   ct_freq_shift,
   ct_freq_vocoder_low,
   ct_freq_vocoder_high,
   ct_bandwidth,
   ct_envtime,
   ct_envtime_lfodecay,
   ct_envshape,
   ct_envmode,
   ct_delaymodtime,
   ct_reverbtime,
   ct_portatime,
   ct_lforate,
   ct_lfoshape,
   ct_lfotrigmode,
   ct_detuning,
   ct_osctype,
   ct_fxtype,
   ct_fxbypass,
   ct_fbconfig,
   ct_fmconfig,
   ct_filtertype,
   ct_filtersubtype,
   ct_wstype,
   ct_wt2window,
   ct_osccount,
   ct_osccountWT,
   ct_oscspread,
   ct_scenemode,
   ct_scenesel,
   ct_polymode,
   ct_polylimit,
   ct_midikey,
   ct_bool,
   ct_bool_relative_switch,
   ct_bool_link_switch,
   ct_bool_keytrack,
   ct_bool_retrigger,
   ct_bool_unipolar,
   ct_bool_mute,
   ct_bool_solo,
   ct_oscroute,
   ct_stereowidth,
   ct_bool_fm,
   ct_character,
   ct_sineoscmode,
   ct_sinefmlegacy,
   ct_countedset_percent, // what % through a counted set are you
   ct_vocoder_bandcount,
   ct_distortion_waveshape,
   num_ctrltypes,
};

enum ControlStyle
{
   cs_off = 0,
};

enum ControlGroup
{
   cg_GLOBAL = 0,
   cg_OSC = 2,
   cg_MIX = 3,
   cg_FILTER = 4,
   cg_ENV = 5,
   cg_LFO = 6,
   cg_FX = 7,
};

struct ParamUserData
{
   virtual ~ParamUserData()
   {}
};

struct CountedSetUserData : public ParamUserData
{
   virtual int getCountedSetSize() = 0; // A constant time answer to the count of the set
};

/*
 * This is part of using a smarter mechanism than "++". Basically these look like a linked list.
 */
struct ParameterIDCounter
{
   struct ParameterIDPromise
   {
      std::shared_ptr<ParameterIDPromise> prior, next;
      ParameterIDPromise() : prior(nullptr), next(nullptr)
      {}
      long value = -1;
   };

   ParameterIDCounter()
   {
      head.reset(new ParameterIDPromise());
      tail = head;
   }

   typedef std::shared_ptr<ParameterIDPromise> promise_t;

   promise_t head, tail;

   // This is a post-increment operator
   promise_t next()
   {
      promise_t n(new ParameterIDPromise());
      n->prior = tail;
      tail->next = n;
      auto ret = tail;
      tail = n;
      return ret;
   };

   void resolve()
   {
      auto h = head;
      int val = 0;
      while (h.get())
      {
         h->value = val++;
         h = h->next;
      }
   }
};

class Parameter
{
public:
   Parameter();
   Parameter* assign(ParameterIDCounter::promise_t id,
                     int pid,
                     const char* name,
                     const char* dispname,
                     int ctrltype,
                     int posx,
                     int posy,
                     int scene = 0,
                     ControlGroup ctrlgroup = cg_GLOBAL,
                     int ctrlgroup_entry = 0,
                     bool modulateable = true,
                     int ctrlstyle = cs_off);

   Parameter* assign(ParameterIDCounter::promise_t id,
                     int pid,
                     const char* name,
                     const char* layoutID,
                     const char* dispname, // This is not a layout thing - it is aprt of the VST API
                                           // for instance but does need to be internationalized
                                           // somehow without causing a cycle
                     int ctrltype,
                     // int posx, comes from layout
                     // int posy, comes from layout
                     int scene = 0,
                     ControlGroup ctrlgroup = cg_GLOBAL,
                     int ctrlgroup_entry = 0,
                     bool modulateable = true
                     // int ctrlstyle = cs_off comes from layout
   );

   bool can_temposync();
   bool can_extend_range();
   bool can_be_absolute();
   bool can_snap();
   void clear_flags();
   void set_type(int ctrltype);
   void morph(Parameter* a, Parameter* b, float x);
   //	void morph(parameter *b, float x);
   pdata morph(Parameter* b, float x);
   const char* get_name();
   const char* get_full_name();
   void set_name(const char* n); // never change name_storage as it is used for storage/recall
   const char* get_internal_name();
   const char* get_storage_name();
   const wchar_t* getUnit() const;
   void get_display(char* txt, bool external = false, float ef = 0.f);
   char* get_storage_value(char*);
   void set_storage_value(int i);
   void set_storage_value(float f);
   float get_extended(float f);
   float get_value_f01();
   float normalized_to_value(float value);
   float value_to_normalized(float value);
   float get_default_value_f01();
   void set_value_f01(float v, bool force_integer = false);
   float
   get_modulation_f01(float mod);     // used by the gui to get the position of the modulated handle
   float set_modulation_f01(float v); // used by the gui to set the modulation to match the position
                                      // of the modulated handle
   void bound_value(bool force_integer = false);
   std::string tempoSyncNotationValue(float f);
   
   pdata val, val_default, val_min, val_max;
   ParameterIDCounter::promise_t id_promise;
   int id;

   std::string layoutEngineID = "";
   bool hasLayoutEngineID = false;

   char name[NAMECHARS], dispname[NAMECHARS], name_storage[NAMECHARS], fullname[NAMECHARS];
   bool modulateable;
   int valtype = 0;
   int scene; // 0 = patch, 1 = scene A, 2 = scene B
   int ctrltype, posx, posy, posy_offset;
   ControlGroup ctrlgroup = cg_GLOBAL;
   int ctrlgroup_entry = 0;
   int ctrlstyle = cs_off;
   int midictrl;
   int param_id_in_scene;
   bool affect_other_parameters;
   float moverate;
   bool per_voice_processing;
   bool temposync, extend_range, absolute, snap;

   ParamUserData* user_data;              // I know this is a bit gross but we have a runtime type
   void set_user_data(ParamUserData* ud); // I take a shallow copy and assume i am referencable
};
