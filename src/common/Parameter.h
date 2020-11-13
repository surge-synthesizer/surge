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
#include "globals.h"
#include <string>
#include <memory>
#include <cstdint>
#include <functional>
#include <atomic>
#include "SkinModel.h"

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
   ct_percent_bidirectional_stereo, // a bidirectional with special string at -100% +100% and 0%
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
   ct_decibel_narrow_short_extendable, // shorter extend range
   ct_decibel_extra_narrow,
   ct_decibel_attenuation,
   ct_decibel_attenuation_large,
   ct_decibel_fmdepth,
   ct_decibel_extendable,
   ct_decibel_deactivatable,
   ct_freq_audible,
   ct_freq_audible_deactivatable,
   ct_freq_mod,
   ct_freq_hpf,
   ct_freq_shift,
   ct_freq_vocoder_low,
   ct_freq_vocoder_high,
   ct_bandwidth,
   ct_envtime,
   ct_envtime_lfodecay,
   ct_envshape,
   ct_envshape_attack,
   ct_envmode,
   ct_delaymodtime,
   ct_reverbtime,
   ct_reverbpredelaytime,
   ct_portatime,
   ct_lforate,
   ct_lforate_deactivatable,
   ct_lfodeform,
   ct_lfotype,
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
   ct_midikey_or_channel,
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
   ct_flangerpitch,
   ct_flangermode,
   ct_flangerwave,
   ct_flangervoices,
   ct_flangerspacing,
   ct_osc_feedback,
   ct_osc_feedback_negative,
   ct_chorusmodtime,
   ct_percent200,
   ct_rotarydrive,
   ct_sendlevel,
   ct_phaser_stages,
   ct_lfoamplitude,
   ct_vocoder_modulator_mode,
   ct_airwindow_fx,
   ct_airwindow_param,
   ct_airwindow_param_bipolar,
   ct_airwindow_param_integral,
   ct_amplitude_clipper,
   ct_phaser_spread,
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
   endCG
};

struct ParamUserData
{
   virtual ~ParamUserData() = default;
};

struct CountedSetUserData : public ParamUserData
{
   virtual int getCountedSetSize() = 0; // A constant time answer to the count of the set
};

struct ParameterExternalFormatter : public ParamUserData
{
   virtual void formatValue( float value, char *txt, int txtlen ) = 0;
   virtual bool stringToValue( const char* txt, float &outVal ) = 0;
};

struct ParameterDiscreteIndexRemapper : public ParamUserData
{
   virtual int remapStreamedIndexToDisplayIndex( int i ) = 0;
   virtual std::string nameAtStreamedIndex( int i ) = 0;
   virtual bool hasGroupNames() { return false; }
   virtual std::string groupNameAtStreamedIndex( int i ) { return ""; } // If you want menu grouping
};

/*
** It used to be parameters were assigned IDs in SurgePatch using an int which we ++ed along the
** way. If we want to 'add at the end' but 'cluster together' we need a different data strcture
** to allow clusters of parameters, so instead make SurgePatch use a linked list (or multiple lists)
** while constructing params and then resolve them all at the end. This little class lets us
** do that.
*/
struct ParameterIDCounter
{
   struct ParameterIDPromise
   {
      std::shared_ptr<ParameterIDPromise> next;
      ParameterIDPromise() : next(nullptr)
      {
      }
      ~ParameterIDPromise() = default;
      long value = -1;
   };

   ParameterIDCounter()
   {
      head = std::make_shared<ParameterIDPromise>();
      tail = head;
   }
   ~ParameterIDCounter()
   {
      head = nullptr;
      tail = nullptr;
   }

   typedef std::shared_ptr<ParameterIDPromise> promise_t;
   typedef ParameterIDPromise* promise_ptr_t; // use this for constant size carefully managed weak references

   promise_t head, tail;

   // This is a post-increment operator
   promise_t next()
   {
      promise_t n(new ParameterIDPromise());
      tail->next = n;
      auto ret = tail;
      tail = n;
      return ret;
   };

   void resolve() const
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

// used to make the infowindow
struct ModulationDisplayInfoWindowStrings {
   std::string val;
   std::string valplus;
   std::string valminus;
   std::string dvalplus;
   std::string dvalminus;
};

class SurgeStorage;

/*
** WARNING WARNING
**
** Parameter is copied with memcpy
** Don't have complex types as members therefore
*/
class Parameter
{
public:
   Parameter();
private:
   Parameter* assign(ParameterIDCounter::promise_t id,
                     int pid,
                     const char* name,
                     const char* dispname,
                     int ctrltype,

                     std::string ui_identifier,
                     int posx,
                     int posy,
                     
                     int scene = 0,
                     ControlGroup ctrlgroup = cg_GLOBAL,
                     int ctrlgroup_entry = 0,
                     bool modulateable = true,
                     int ctrlstyle = cs_off,
                     bool defaultDeactivation = true);
public:
   Parameter* assign(ParameterIDCounter::promise_t id,
                     int pid,
                     const char* name,
                     const char* dispname,
                     int ctrltype,

                     const Surge::Skin::Connector &c,

                     int scene = 0,
                     ControlGroup ctrlgroup = cg_GLOBAL,
                     int ctrlgroup_entry = 0,
                     bool modulateable = true,
                     int ctrlstyle = cs_off,
                     bool defaultDeactivation = true);
   virtual ~Parameter();

   bool can_temposync();
   bool can_extend_range();
   bool can_be_absolute();
   bool can_deactivate();
   bool can_setvalue_from_string();
   void clear_flags();
   bool has_portaoptions();
   bool has_deformoptions();
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
   enum ModulationDisplayMode {
      TypeIn,
      Menu,
      InfoWindow
   };

   void get_display_of_modulation_depth(char* txt, float modulationDepth, bool isBipolar, ModulationDisplayMode mode, ModulationDisplayInfoWindowStrings *iw = nullptr );
   void get_display_alt(char* txt, bool external = false, float ef = 0.f);
   char* get_storage_value(char*);
   void set_storage_value(int i);
   void set_storage_value(float f);
   float get_extended(float f);
   float get_value_f01();
   float normalized_to_value(float value);
   float value_to_normalized(float value);
   float get_default_value_f01();
   void set_value_f01(float v, bool force_integer = false);
   bool set_value_from_string(std::string s);
   float
   get_modulation_f01(float mod);     // used by the gui to get the position of the modulated handle
   float set_modulation_f01(float v); // used by the gui to set the modulation to match the position
                                      // of the modulated handle
   float calculate_modulation_value_from_string( const std::string &s, bool &valid );
   
   void bound_value(bool force_integer = false);
   std::string tempoSyncNotationValue(float f);
   float quantize_modulation(float modvalue); // given a mod-value hand it back rounded to a 'reasonable' step size (used in ctrl-drag)

   void create_fullname(const char* dn, char *fn, ControlGroup ctrlgroup, int ctrlgroup_entry, const char *lfoPrefixOverride = nullptr );
   
   pdata val{}, val_default{}, val_min{}, val_max{};
   // You might be tempted to use a non-fixed-size member here, like a std::string, but
   // this class gets pre-c++ memcopied so thats not an option which is why I do this wonky
   // pointer thing and strncpy from a string onto ui_identifier
   ParameterIDCounter::promise_ptr_t id_promise{};
   int id{};
   char name[NAMECHARS]{}, dispname[NAMECHARS]{}, name_storage[NAMECHARS]{}, fullname[NAMECHARS]{};
   char ui_identifier[NAMECHARS]{};
   bool modulateable{};
   int valtype = 0;
   int scene{}; // 0 = patch, 1 = scene A, 2 = scene B
   int ctrltype{};
   int posx, posy, posy_offset;
   ControlGroup ctrlgroup = cg_GLOBAL;
   int ctrlgroup_entry = 0;
   int ctrlstyle = cs_off;
   int midictrl{};
   int param_id_in_scene{};
   bool affect_other_parameters{};
   float moverate{};
   bool per_voice_processing{};
   bool temposync{}, extend_range{}, absolute{}, deactivated{};
   bool porta_constrate{}, porta_gliss{}, porta_retrigger{};
   int porta_curve{};
   int deform_type{};

   enum ParamDisplayType {
      Custom,
      LinearWithScale,
      ATwoToTheBx,
      Decibel,
      DelegatedToFormatter
   } displayType = Custom;

   enum ParamDisplayFeatures {
      kHasCustomMinString = 1U << 0U,
      kHasCustomMaxString = 1U << 1U,
      kHasCustomDefaultString = 1U << 2U,
      kHasCustomMinValue = 1U << 3U,
      kHasCustomMaxValue = 1U << 4U,
      kUnitsAreSemitonesOrKeys = 1U << 5U
   };
   
   struct DisplayInfo {
      char unit[128]{}, absoluteUnit[128]{};
      float scale = 1;
      float a = 1.0, b = 1.0;
      int decimals = 2;
      int64_t customFeatures = 0;

      float tempoSyncNotationMultiplier = 1.f;
      
      char minLabel[128]{}, maxLabel[128]{}, defLabel[128]{};
      float minLabelValue = 0.f, maxLabelValue = 0.f;

      float modulationCap = -1.f;
      
      float extendFactor = 1.0, absoluteFactor = 1.0; // set these to 1 in case we sneak by and divide by accident
   } displayInfo;
      
   ParamUserData* user_data = nullptr;    // I know this is a bit gross but we have a runtime type
   void set_user_data(ParamUserData* ud); // I take a shallow copy and don't assume ownership and assume i am referencable

   bool hasSkinConnector = false;

   /*
   ** Parameter has a pointer to the storage that manages the patch that contains it
   ** *if* this parameter was thus constructed. There are real and legitimate uses
   ** of the Parameter class where this pointer will be null so if you use it you
   ** have to check the nullity
   **
   ** if( storage && storage->isStandardTuning ) { }
   */
   SurgeStorage *storage = nullptr;
};

// I don't make this a member since param needs to be copyable with memcpy.
extern std::atomic<bool> parameterNameUpdated;
