//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "Effect.h"
#include "BiquadFilter.h"
#include "DspUtilities.h"
#include "AllpassFilter.h"
#include "CSurgeVuMeter.h"

#include "VectorizedSvfFilter.h"

#include <vt_dsp/halfratefilter.h>
#include <vt_dsp/lipol.h>

const int max_delay_length = 1 << 18;

const int slowrate = 8;
const int slowrate_m1 = slowrate - 1;

class DualDelayEffect : public Effect
{
   _MM_ALIGN16 lipol_ps feedback, crossfeed, pan, mix, width;
   _MM_ALIGN16 float buffer[2][max_delay_length + FIRipol_N];

public:
   DualDelayEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~DualDelayEffect();
   virtual const char* get_effectname()
   {
      return "dualdelay";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);
   virtual int get_ringout_decay()
   {
      return ringout_time;
   }

private:
   lag<float, true> timeL, timeR;
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase;
   float LFOval;
   bool LFOdirection;
   int ringout_time;
};

template <int v> class ChorusEffect : public Effect
{
   _MM_ALIGN16 lipol_ps feedback, mix, width;
   _MM_ALIGN16 __m128 voicepanL4[v], voicepanR4[v];
   _MM_ALIGN16 float
       buffer[max_delay_length + FIRipol_N]; // s� kan den interpoleras med SSE utan wrap
public:
   ChorusEffect<v>(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~ChorusEffect();
   virtual const char* get_effectname()
   {
      return "chorus";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   lag<float, true> time[v];
   float voicepan[v][2];
   float envf;
   int wpos;
   BiquadFilter lp, hp;
   double lfophase[v];
};

class FreqshiftEffect : public Effect
{
public:
   _MM_ALIGN16 halfrate_stereo fr, fi;
   _MM_ALIGN16 lipol_ps mix;
   FreqshiftEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~FreqshiftEffect();
   virtual const char* get_effectname()
   {
      return "freqshift";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);
   virtual int get_ringout_decay()
   {
      return ringout_time;
   }

private:
   lipol<float, true> feedback;
   lag<float, true> time, shiftL, shiftR;
   float buffer[2][max_delay_length];
   int wpos;
   // CHalfBandFilter<6> frL,fiL,frR,fiR;
   quadr_osc o1L, o2L, o1R, o2R;
   int ringout_time;
};

class Eq3BandEffect : public Effect
{
   _MM_ALIGN16 lipol_ps gain;

public:
   Eq3BandEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Eq3BandEffect();
   virtual const char* get_effectname()
   {
      return "EQ";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual int get_ringout_decay()
   {
      return 500;
   }
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   BiquadFilter band1, band2, band3;
   int bi; // block increment (to keep track of events not occurring every n blocks)
};

class PhaserEffect : public Effect
{
   _MM_ALIGN16 lipol_ps mix;
   _MM_ALIGN16 float L[block_size], R[block_size];

public:
   PhaserEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~PhaserEffect();
   virtual const char* get_effectname()
   {
      return "phaser";
   }
   virtual void init();
   virtual void process_only_control();
   virtual void process(float* dataL, float* dataR);
   virtual int get_ringout_decay()
   {
      return 1000;
   }
   virtual void suspend();
   void setvars();
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   lipol<float, true> feedback;
   static const int n_bq = 4;
   static const int n_bq_units = n_bq << 1;
   float dL, dR;
   BiquadFilter* biquad[8];
   float lfophase;
   int bi; // block increment (to keep track of events not occurring every n blocks)
};

/*	rotary_speaker			*/
class RotarySpeakerEffect : public Effect
{
public:
   RotarySpeakerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~RotarySpeakerEffect();
   virtual void process_only_control();
   virtual const char* get_effectname()
   {
      return "rotary";
   }
   virtual void process(float* dataL, float* dataR);
   virtual int get_ringout_decay()
   {
      return max_delay_length >> 5;
   }
   virtual void suspend();
   virtual void init();
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

protected:
   float buffer[max_delay_length];
   int wpos;
   // filter *lp[2],*hp[2];
   // biquadunit rotor_lpL,rotor_lpR;
   BiquadFilter xover, lowbass;
   // float
   // f_rotor_lp[2][n_filter_parameters],f_xover[n_filter_parameters],f_lowbass[n_filter_parameters];
   quadr_osc lfo;
   quadr_osc lf_lfo;
   lipol<float> dL, dR, drive, hornamp[2];
   bool first_run;
};

class DistortionEffect : public Effect
{
   _MM_ALIGN16 halfrate_stereo hr_a, hr_b;
   _MM_ALIGN16 lipol_ps drive, outgain;

public:
   DistortionEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~DistortionEffect();
   virtual const char* get_effectname()
   {
      return "distortion";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   virtual int get_ringout_decay()
   {
      return 1000;
   }
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   BiquadFilter band1, band2, lp1, lp2;
   int bi; // block increment (to keep track of events not occurring every n blocks)
   float L, R;
};

const int n_vocoder_bands = 20;
const int NVocoderVec = n_vocoder_bands >> 2;

class VocoderEffect : public Effect
{
public:
   enum
   {
      KGain,
      KGateLevel,
      KRate,
      // KUnvoicedThreshold,
      KQuality,
      KShift,
   };

   VocoderEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~VocoderEffect();
   virtual const char* get_effectname()
   {
      return "vocoder";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   virtual int get_ringout_decay()
   {
      return 500;
   }
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   _MM_ALIGN16 VectorizedSvfFilter mCarrierL[NVocoderVec];
   _MM_ALIGN16 VectorizedSvfFilter mCarrierR[NVocoderVec];
   _MM_ALIGN16 VectorizedSvfFilter mModulator[NVocoderVec];
   _MM_ALIGN16 vFloat mEnvF[NVocoderVec];
   _MM_ALIGN16 lipol_ps mGain;

   int mBI; // block increment (to keep track of events not occurring every n blocks)

   /*
   float mVoicedLevel;
   float mUnvoicedLevel;

   biquadunit
           mVoicedDetect,
           mUnvoicedDetect;
   */
};

class emphasize : public Effect
{
   _MM_ALIGN16 halfrate_stereo pre, post;
   _MM_ALIGN16 lipol_ps type, outgain;

public:
   emphasize(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~emphasize();
   virtual const char* get_effectname()
   {
      return "emphasize";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   virtual int get_ringout_decay()
   {
      return 50;
   }
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   BiquadFilter EQ;
   int bi; // block increment (to keep track of events not occurring every n blocks)
   float L, R;
};

const int lookahead_bits = 7;
const int lookahead = 1 << 7;

class ConditionerEffect : public Effect
{
   _MM_ALIGN16 lipol_ps ampL, ampR, width, postamp;

public:
   ConditionerEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~ConditionerEffect();
   virtual const char* get_effectname()
   {
      return "conditioner";
   }
   virtual void init();
   virtual void process_only_control();
   virtual void process(float* dataL, float* dataR);
   virtual int get_ringout_decay()
   {
      return 100;
   }
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual int vu_type(int id);
   virtual int vu_ypos(int id);
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);

private:
   BiquadFilter band1, band2;
   float ef;
   lipol<float, true> a_rate, r_rate;
   float lamax[lookahead << 1];
   float delayed[2][lookahead];
   int bufpos;
   float filtered_lamax, filtered_lamax2, gain;
};

const int revbits = 15;
const int max_rev_dly = 1 << revbits;
const int rev_tap_bits = 4;
const int rev_taps = 1 << rev_tap_bits;

class Reverb1Effect : public Effect
{
   _MM_ALIGN16 float delay_pan_L[rev_taps], delay_pan_R[rev_taps];
   _MM_ALIGN16 float delay_fb[rev_taps];
   _MM_ALIGN16 float delay[rev_taps * max_rev_dly];
   _MM_ALIGN16 float out_tap[rev_taps];
   _MM_ALIGN16 float predelay[max_rev_dly];
   _MM_ALIGN16 int delay_time[rev_taps];
   _MM_ALIGN16 lipol_ps mix, width;

public:
   Reverb1Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Reverb1Effect();
   virtual const char* get_effectname()
   {
      return "reverb";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   void setvars(bool init);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);
   virtual int get_ringout_decay()
   {
      return ringout_time;
   }

private:
   void update_rtime();
   void update_rsize();
   void clear_buffers();
   void loadpreset(int id);
   /*int delay_time_mod[rev_taps];
   int delay_time_dv[rev_taps];*/
   int delay_pos;
   /*	float noise[rev_taps];
           float noise_target[rev_taps];*/
   double modphase;
   int shape;
   float lastf[n_fx_params];
   BiquadFilter band1, locut, hicut;
   int ringout_time;
   int b;
};

class Reverb2Effect : public Effect
{
   enum
   {
      NUM_BLOCKS = 4,
      NUM_INPUT_ALLPASSES = 4,
      NUM_ALLPASSES_PER_BLOCK = 2,
      MAX_ALLPASS_LEN = 16384,
      MAX_DELAY_LEN = 16384,
      DELAY_LEN_MASK = MAX_DELAY_LEN - 1,
      DELAY_SUBSAMPLE_BITS = 8,
      DELAY_SUBSAMPLE_RANGE = (1 << DELAY_SUBSAMPLE_BITS),
   };

   class allpass
   {
   public:
      allpass();
      float process(float x, float coeff);
      void setLen(int len);

   private:
      int _len;
      int _k;
      float _data[MAX_ALLPASS_LEN];
   };

   class delay
   {
   public:
      delay();
      float process(float x, int tap1, float& tap_out1, int tap2, float& tap_out2, int modulation);
      void setLen(int len);

   private:
      int _len;
      int _k;
      float _data[MAX_DELAY_LEN];
   };

   class onepole_filter
   {
   public:
      onepole_filter();
      float process_lowpass(float x, float c0);
      float process_highpass(float x, float c0);

   private:
      float a0;
   };

   _MM_ALIGN16 lipol_ps mix, width;

public:
   Reverb2Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd);
   virtual ~Reverb2Effect();
   virtual const char* get_effectname()
   {
      return "reverb2";
   }
   virtual void init();
   virtual void process(float* dataL, float* dataR);
   virtual void suspend();
   void setvars(bool init);
   void calc_size(float scale);
   virtual void init_ctrltypes();
   virtual void init_default_values();
   virtual const char* group_label(int id);
   virtual int group_label_ypos(int id);
   virtual int get_ringout_decay()
   {
      return ringout_time;
   }

private:
   int ringout_time;
   allpass _input_allpass[NUM_INPUT_ALLPASSES];
   allpass _allpass[NUM_BLOCKS][NUM_ALLPASSES_PER_BLOCK];
   onepole_filter _hf_damper[NUM_BLOCKS];
   onepole_filter _lf_damper[NUM_BLOCKS];
   delay _delay[NUM_BLOCKS];
   int _tap_timeL[NUM_BLOCKS];
   int _tap_timeR[NUM_BLOCKS];
   float _tap_gainL[NUM_BLOCKS];
   float _tap_gainR[NUM_BLOCKS];
   float _state;
   lipol<float, true> _decay_multiply;
   lipol<float, true> _diffusion;
   lipol<float, true> _buildup;
   lipol<float, true> _hf_damp_coefficent;
   lipol<float, true> _lf_damp_coefficent;
   lipol<float, true> _modulation;
   quadr_osc _lfo;
};