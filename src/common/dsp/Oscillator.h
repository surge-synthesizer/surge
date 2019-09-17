#include "SurgeStorage.h"
#include "DspUtilities.h"
#include <vt_dsp/lipol.h>
#include "BiquadFilter.h"

class alignas(16) Oscillator
{
public:
   // The data blocks processed by the SIMD instructions (e.g. SSE2), which must
   // always be before any other variables in the class, in order to be properly
   // aligned to 16 bytes.
   float output alignas(16)[BLOCK_SIZE_OS];
   float outputR alignas(16)[BLOCK_SIZE_OS];

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
      // Wondering about that constant 16.35? It is the frequency of C0
      return (float)(M_PI * (16.35159783) * storage->note_to_pitch(x) * dsamplerate_os_inv);
   }

   virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
   {
       // No-op here.
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
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual void process_block_legacy(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f);
   virtual ~osc_sine();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;

   quadr_osc sinus;
   double phase;
   float driftlfo, driftlfo2;
   lag<double> FMdepth;
   lag<double> FB;

   int id_mode, id_fb, id_fmlegacy;
   float lastvalue = 0;

   float valueFromSinAndCos(float svalue, float cvalue);
   virtual void handleStreamingMismatches(int s, int synths) override;
};

class FMOscillator : public Oscillator
{
public:
   FMOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~FMOscillator();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   double phase, lastoutput;
   quadr_osc RM1, RM2, AM;
   float driftlfo, driftlfo2;
   lag<double> FMdepth, AbsModDepth, RelModDepth1, RelModDepth2, FeedbackDepth;
};

class FM2Oscillator : public Oscillator
{
public:
   FM2Oscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~FM2Oscillator();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   double phase, lastoutput;
   quadr_osc RM1, RM2;
   float driftlfo, driftlfo2;
   lag<double> FMdepth, RelModDepth1, RelModDepth2, FeedbackDepth, PhaseOffset;
};

class osc_audioinput : public Oscillator
{
public:
   osc_audioinput(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~osc_audioinput();
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual bool allow_display() override
   {
      return false;
   }
};

class AbstractBlitOscillator : public Oscillator
{
public:
   AbstractBlitOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);

protected:
   float oscbuffer alignas(16)[OB_LENGTH + FIRipol_N];
   float oscbufferR alignas(16)[OB_LENGTH + FIRipol_N];
   float dcbuffer alignas(16)[OB_LENGTH + FIRipol_N];
   __m128 osc_out, osc_out2, osc_outR, osc_out2R;
   void prepare_unison(int voices);
   float integrator_hpf;
   float pitchmult, pitchmult_inv;
   int n_unison;
   int bufpos;
   float out_attenuation, out_attenuation_inv, detune_bias, detune_offset;
   float oscstate[MAX_UNISON], syncstate[MAX_UNISON], rate[MAX_UNISON];
   float driftlfo[MAX_UNISON], driftlfo2[MAX_UNISON];
   float panL[MAX_UNISON], panR[MAX_UNISON];
   int state[MAX_UNISON];
};

class SurgeSuperOscillator : public AbstractBlitOscillator
{
private:
   lipol_ps li_hpf, li_DC, li_integratormult;
   float FMphase alignas(16)[BLOCK_SIZE_OS + 4];

public:
   SurgeSuperOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   template <bool FM> void convolute(int voice, bool stereo);
   virtual ~SurgeSuperOscillator();

private:
   bool first_run;
   float dc, dc_uni[MAX_UNISON], elapsed_time[MAX_UNISON], last_level[MAX_UNISON],
       pwidth[MAX_UNISON], pwidth2[MAX_UNISON];
   template <bool is_init> void update_lagvals();
   float pitch;
   lag<float> FMdepth, integrator_mult, l_pw, l_pw2, l_shape, l_sub, l_sync;
   int id_pw, id_pw2, id_shape, id_smooth, id_sub, id_sync, id_detune;
   int FMdelay;
   float FMmul_inv;
   float CoefB0, CoefB1, CoefA1;
};

#if 0
class osc_pluck : public Oscillator
{
public:
   osc_pluck(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void process_block(float pitch, float drift) override;
   virtual void process_block_fm(float pitch, float depth, float drift) override;
   void process_blockT(float pitch, bool FM, float depth, float drift = 0);
   virtual ~osc_pluck();
};
#endif

class SampleAndHoldOscillator : public AbstractBlitOscillator
{
private:
   lipol_ps li_hpf, li_DC, li_integratormult;

public:
   SampleAndHoldOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~SampleAndHoldOscillator();

private:
   void convolute(int voice, bool FM, bool stereo);
   template <bool FM> void process_blockT(float pitch, float depth, float drift = 0);
   template <bool is_init> void update_lagvals();
   bool first_run;
   float dc, dc_uni[MAX_UNISON], elapsed_time[MAX_UNISON], last_level[MAX_UNISON],
       last_level2[MAX_UNISON], pwidth[MAX_UNISON];
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
   virtual void init(float pitch, bool is_display = false) override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override; 
   virtual ~WavetableOscillator();

private:
   void convolute(int voice, bool FM, bool stereo);
   template <bool is_init> void update_lagvals();
   inline float distort_level(float);
   bool first_run;
   float oscpitch[MAX_UNISON];
   float dc, dc_uni[MAX_UNISON], last_level[MAX_UNISON];
   float pitch;
   int mipmap[MAX_UNISON], mipmap_ofs[MAX_UNISON];
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
   int IOutputL alignas(16)[BLOCK_SIZE_OS];
   int IOutputR alignas(16)[BLOCK_SIZE_OS];
   struct
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
   } Sub alignas(16);

public:
   WindowOscillator(SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
   virtual void init(float pitch, bool is_display = false) override;
   virtual void init_ctrltypes() override;
   virtual void init_default_values() override;
   virtual void process_block(
       float pitch, float drift = 0.f, bool stereo = false, bool FM = false, float FMdepth = 0.f) override;
   virtual ~WindowOscillator();

private:
   void ProcessSubOscs(bool);
   float OutAttenuation;
   float DetuneBias, DetuneOffset;
   int ActiveSubOscs;
};

Oscillator*
spawn_osc(int osctype, SurgeStorage* storage, OscillatorStorage* oscdata, pdata* localcopy);
