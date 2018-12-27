#include "SurgeStorage.h"
#include "DspUtilities.h"
#include <vt_dsp/lipol.h>
#include "BiquadFilter.h"

class Oscillator
{
public:
   float output alignas(16)[block_size_os],
         outputR alignas(16)[block_size_os];
   Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual ~Oscillator();
   virtual void init(float pitch, bool is_display = false){};
   virtual void init_ctrltypes(){};
   virtual void init_default_values(){};
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f)
   {}
   virtual void assign_fm(float* master_osc)
   {
      this->master_osc = master_osc;
   }
   virtual bool allow_display()
   {
      return true;
   }
   inline float pitch_to_omega(float x)
   {
      return (float)(M_PI * (16.35159783) * note_to_pitch(x) * dsamplerate_os_inv);
   }

protected:
   SurgeStorage* storage;
   OscillatorStorage* oscdata;
   pdata* localcopy;
   float* __restrict master_osc;
   float drift;
   int ticker;
};

class osc_sine : public Oscillator
{
public:
   osc_sine(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~osc_sine();
   quadr_osc sinus;
   double phase;
   float driftlfo, driftlfo2;
   lag<double> FMdepth;
};

class FMOscillator : public Oscillator
{
public:
   FMOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~FMOscillator();
   virtual void init_ctrltypes();
   virtual void init_default_values();
   double phase, lastoutput;
   quadr_osc RM1, RM2, AM;
   float driftlfo, driftlfo2;
   lag<double> FMdepth, AbsModDepth, RelModDepth1, RelModDepth2, FeedbackDepth;
};

class FM2Oscillator : public Oscillator
{
public:
   FM2Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~FM2Oscillator();
   virtual void init_ctrltypes();
   virtual void init_default_values();
   double phase, lastoutput;
   quadr_osc RM1, RM2;
   float driftlfo, driftlfo2;
   lag<double> FMdepth, RelModDepth1, RelModDepth2, FeedbackDepth, PhaseOffset;
};

class osc_audioinput : public Oscillator
{
public:
   osc_audioinput(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~osc_audioinput();
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual bool allow_display()
   {
      return false;
   }
};

class AbstractBlitOscillator : public Oscillator
{
public:
   AbstractBlitOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);

protected:
   float oscbuffer alignas(16)[ob_length + FIRipol_N];
   float oscbufferR alignas(16)[ob_length + FIRipol_N];
   float dcbuffer alignas(16)[ob_length + FIRipol_N];
   __m128 osc_out, osc_out2, osc_outR, osc_out2R;
   void prepare_unison(int voices);
   float integrator_hpf;
   float pitchmult, pitchmult_inv;
   int n_unison;
   int bufpos;
   float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
   float oscstate[max_unison], syncstate[max_unison], rate[max_unison];
   float driftlfo[max_unison], driftlfo2[max_unison];
   float panL[max_unison], panR[max_unison];
   int state[max_unison];
};

class SurgeSuperOscillator : public AbstractBlitOscillator
{
private:
   lipol_ps li_hpf, li_DC, li_integratormult;
   float FMphase alignas(16)[block_size_os + 4];

public:
   SurgeSuperOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   template <bool FM> void convolute(int voice, bool stereo);
   virtual ~SurgeSuperOscillator();

private:
   bool first_run;
   float dc, dc_uni[max_unison], elapsed_time[max_unison], last_level[max_unison],
       pwidth[max_unison], pwidth2[max_unison];
   template <bool is_init> void update_lagvals();
   float pitch;
   lag<float> FMdepth, integrator_mult, l_pw, l_pw2, l_shape, l_sub, l_sync;
   int id_pw, id_pw2, id_shape, id_smooth, id_sub, id_sync, id_detune;
   int FMdelay;
   float FMmul_inv;
   float CoefB0, CoefB1, CoefA1;
};

class osc_pluck : public Oscillator
{
public:
   osc_pluck(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void process_block(float pitch, float drift);
   virtual void process_block_fm(float pitch, float depth, float drift);
   void process_blockT(float pitch, bool FM, float depth, float drift = 0);
   virtual ~osc_pluck();
};

class SampleAndHoldOscillator : public AbstractBlitOscillator
{
private:
   lipol_ps li_hpf, li_DC, li_integratormult;

public:
   SampleAndHoldOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~SampleAndHoldOscillator();

private:
   void convolute(int voice, bool FM, bool stereo);
   template <bool FM> void process_blockT(float pitch, float depth, float drift = 0);
   template <bool is_init> void update_lagvals();
   bool first_run;
   float dc, dc_uni[max_unison], elapsed_time[max_unison], last_level[max_unison],
       last_level2[max_unison], pwidth[max_unison];
   float pitch;
   lag<double> FMdepth, hpf_coeff, integrator_mult, l_pw, l_shape, l_smooth, l_sub, l_sync;
   int id_pw, id_shape, id_smooth, id_sub, id_sync, id_detune;
   int FMdelay;
   float FMmul_inv;
};

class WavetableOscillator : public AbstractBlitOscillator
{
public:
   lipol_ps li_hpf, li_DC, li_integratormult;
   WavetableOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~WavetableOscillator();

private:
   void convolute(int voice, bool FM, bool stereo);
   template <bool is_init> void update_lagvals();
   inline float distort_level(float);
   bool first_run;
   float oscpitch[max_unison];
   float dc, dc_uni[max_unison], last_level[max_unison];
   float pitch;
   int mipmap[max_unison], mipmap_ofs[max_unison];
   lag<float> FMdepth, hpf_coeff, integrator_mult, l_hskew, l_vskew, l_clip, l_shape;
   float formant_t, formant_last, pitch_last, pitch_t;
   float tableipol, last_tableipol;
   float hskew, last_hskew;
   int id_shape, id_vskew, id_hskew, id_clip, id_detune, id_formant, tableid, last_tableid;
   int FMdelay;
   float FMmul_inv;
   int sampleloop;
   // biquadunit FMfilter;
   // float wavetable[wavetable_steps];
};

const int wt2_suboscs = 8;

class WindowOscillator : public Oscillator
{
private:
   int IOutputL alignas(16)[block_size_os];
   int IOutputR alignas(16)[block_size_os];
   _MM_ALIGN16 struct
   {
      unsigned int Pos[wt2_suboscs];
      unsigned int SubPos[wt2_suboscs];
      unsigned int Ratio[wt2_suboscs];
      unsigned int Table[wt2_suboscs];
      unsigned int FormantMul[wt2_suboscs];
      unsigned int DispatchDelay[wt2_suboscs]; // samples until playback should start (for
                                               // per-sample scheduling)
      unsigned char Gain[wt2_suboscs][2];
      float DriftLFO[wt2_suboscs][2];
   } Sub;

public:
   WindowOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~WindowOscillator();

private:
   void ProcessSubOscs(bool);
   float OutAttenuation;
   float DetuneBias, DetuneOffset;
   int ActiveSubOscs;
};

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
