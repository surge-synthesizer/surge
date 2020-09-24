#include "Reverb2Effect.h"

/* reverb 2			*/
/* allpass loop design */

enum revparam
{
   r2p_predelay = 0,

   r2p_room_size,
   r2p_decay_time,
   r2p_diffusion,
   r2p_buildup,
   r2p_modulation,

   r2p_lf_damping,
   r2p_hf_damping,

   r2p_width,
   r2p_mix,
   r2p_num_params,
};

const float db60 = powf(10.f, 0.05f * -60.f);

Reverb2Effect::allpass::allpass()
{
   _k = 0;
   _len = 1;
   memset(_data, 0, MAX_ALLPASS_LEN * sizeof(float));
}

void Reverb2Effect::allpass::setLen(int len)
{
   _len = len;
}

float Reverb2Effect::allpass::process(float in, float coeff)
{
   _k++;
   if (_k >= _len)
      _k = 0;
   float delay_in = in - coeff * _data[_k];
   float result = _data[_k] + coeff * delay_in;
   _data[_k] = delay_in;
   return result;
}

Reverb2Effect::delay::delay()
{
   _k = 0;
   _len = 1;
   memset(_data, 0, MAX_DELAY_LEN * sizeof(float));
}

void Reverb2Effect::delay::setLen(int len)
{
   _len = len;
}

float Reverb2Effect::delay::process(
    float in, int tap1, float& tap_out1, int tap2, float& tap_out2, int modulation)
{
   _k = (_k + 1) & DELAY_LEN_MASK;

   tap_out1 = _data[(_k - tap1) & DELAY_LEN_MASK];
   tap_out2 = _data[(_k - tap2) & DELAY_LEN_MASK];

   int modulation_int = modulation >> DELAY_SUBSAMPLE_BITS;
   int modulation_frac1 = modulation & (DELAY_SUBSAMPLE_RANGE - 1);
   int modulation_frac2 = DELAY_SUBSAMPLE_RANGE - modulation_frac1;

   float d1 = _data[(_k - _len + modulation_int + 1) & DELAY_LEN_MASK];
   float d2 = _data[(_k - _len + modulation_int) & DELAY_LEN_MASK];
   const float multiplier = 1.f / (float)(DELAY_SUBSAMPLE_RANGE);

   float result = (d1 * (float)modulation_frac1 + d2 * (float)modulation_frac2) * multiplier;
   _data[_k] = in;

   return result;
}

Reverb2Effect::onepole_filter::onepole_filter()
{
   a0 = 0.f;
}

float Reverb2Effect::onepole_filter::process_lowpass(float x, float c0)
{
   a0 = a0 * c0 + x * (1.f - c0);
   return a0;
}

float Reverb2Effect::onepole_filter::process_highpass(float x, float c0)
{
   a0 = a0 * (1.f - c0) + x * c0;
   return x - a0;
}

Reverb2Effect::Reverb2Effect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd)
{
   _state = 0.f;
}

Reverb2Effect::~Reverb2Effect()
{}

void Reverb2Effect::init()
{
   setvars(true);
}

int msToSamples(float ms, float scale)
{
   float a = samplerate * ms * 0.001f;

   float b = a * scale;

   return (int)(b);
}

void Reverb2Effect::calc_size(float scale)
{
   float m = scale;

   _tap_timeL[0] = msToSamples(80.3, m);
   _tap_timeL[1] = msToSamples(59.3, m);
   _tap_timeL[2] = msToSamples(97.7, m);
   _tap_timeL[3] = msToSamples(122.6, m);
   _tap_timeR[0] = msToSamples(35.5, m);
   _tap_timeR[1] = msToSamples(101.6, m);
   _tap_timeR[2] = msToSamples(73.9, m);
   _tap_timeR[3] = msToSamples(80.3, m);

   _input_allpass[0].setLen(msToSamples(4.76, m));
   _input_allpass[1].setLen(msToSamples(6.81, m));
   _input_allpass[2].setLen(msToSamples(10.13, m));
   _input_allpass[3].setLen(msToSamples(16.72, m));

   _allpass[0][0].setLen(msToSamples(38.2, m));
   _allpass[0][1].setLen(msToSamples(53.4, m));
   _delay[0].setLen(msToSamples(178.8, m));

   _allpass[1][0].setLen(msToSamples(44.0, m));
   _allpass[1][1].setLen(msToSamples(41, m));
   _delay[1].setLen(msToSamples(126.5, m));

   _allpass[2][0].setLen(msToSamples(48.3, m));
   _allpass[2][1].setLen(msToSamples(60.5, m));
   _delay[2].setLen(msToSamples(106.1, m));

   _allpass[3][0].setLen(msToSamples(38.9, m));
   _allpass[3][1].setLen(msToSamples(42.2, m));
   _delay[3].setLen(msToSamples(139.4, m));
}

void Reverb2Effect::setvars(bool init)
{
   // TODO, balance the gains from the calculated decay coefficient?

   _tap_gainL[0] = 1.5f / 4.f;
   _tap_gainL[1] = 1.2f / 4.f;
   _tap_gainL[2] = 1.0f / 4.f;
   _tap_gainL[3] = 0.8f / 4.f;
   _tap_gainR[0] = 1.5f / 4.f;
   _tap_gainR[1] = 1.2f / 4.f;
   _tap_gainR[2] = 1.0f / 4.f;
   _tap_gainR[3] = 0.8f / 4.f;

   calc_size(1.f);
}

void Reverb2Effect::update_rtime()
{
   float t = BLOCK_SIZE_INV * ( samplerate *
                                ( std::max( 1.0f, powf(2.f, *f[r2p_decay_time]) ) * 2.f +
                                  std::max( 0.1f, powf(2.f, *f[r2p_predelay]) * ( fxdata->p[r2p_predelay].temposync ? storage->temposyncratio_inv : 1.f ) ) * 2.f
                                   ) ); // *2 is to get the db120 time
   ringout_time = (int)t;
}

void Reverb2Effect::process(float* dataL, float* dataR)
{
   float scale = powf(2.f, 1.f * *f[r2p_room_size]);
   calc_size(scale);

   if (fabs(*f[r2p_decay_time] - last_decay_time) > 0.001f)
      update_rtime();

   last_decay_time = *f[r2p_decay_time];

   float wetL alignas(16)[BLOCK_SIZE],
         wetR alignas(16)[BLOCK_SIZE];

   float loop_time_s = 0.5508 * scale;
   float decay = powf(db60, loop_time_s / (4.f * (powf(2.f, *f[r2p_decay_time]))));

   _decay_multiply.newValue(decay);
   _diffusion.newValue(0.7f * *f[r2p_diffusion]);
   _buildup.newValue(0.7f * *f[r2p_buildup]);
   _hf_damp_coefficent.newValue(0.8 * *f[r2p_hf_damping]);
   _lf_damp_coefficent.newValue(0.2 * *f[r2p_lf_damping]);
   _modulation.newValue(*f[r2p_modulation] * samplerate * 0.001f * 5.f);

   width.set_target_smoothed(db_to_linear(*f[r2p_width]));
   mix.set_target_smoothed(*f[r2p_mix]);

   _lfo.set_rate(2.0 * M_PI * powf(2, -2.f) * dsamplerate_inv);

   int pdt = limit_range( (int)( samplerate * pow( 2.0, *f[r2p_predelay] ) *
                                 ( fxdata->p[r2p_predelay].temposync ? storage->temposyncratio_inv : 1.f ) ),
                          1, PREDELAY_BUFFER_SIZE_LIMIT - 1 );


   for (int k = 0; k < BLOCK_SIZE; k++)
   {
      float in = (dataL[k] + dataR[k]) * 0.5f;

      in = _predelay.process( in, pdt );
      
      in = _input_allpass[0].process(in, _diffusion.v);
      in = _input_allpass[1].process(in, _diffusion.v);
      in = _input_allpass[2].process(in, _diffusion.v);
      in = _input_allpass[3].process(in, _diffusion.v);
      float x = _state;

      float outL = 0.f;
      float outR = 0.f;

      float lfos[NUM_BLOCKS];
      lfos[0] = _lfo.r;
      lfos[1] = _lfo.i;
      lfos[2] = -_lfo.r;
      lfos[3] = -_lfo.i;

      auto hdc = limit_range( _hf_damp_coefficent.v, 0.01f, 0.99f );
      auto ldc = limit_range( _lf_damp_coefficent.v, 0.01f, 0.99f );
      for (int b = 0; b < NUM_BLOCKS; b++)
      {
         x = x + in;
         for (int c = 0; c < NUM_ALLPASSES_PER_BLOCK; c++)
         {
            x = _allpass[b][c].process(x, _buildup.v);
         }

         x = _hf_damper[b].process_lowpass(x, hdc );
         x = _lf_damper[b].process_highpass(x, ldc );

         int modulation = (int)(_modulation.v * lfos[b] * (float)DELAY_SUBSAMPLE_RANGE);
         float tap_outL = 0.f;
         float tap_outR = 0.f;
         x = _delay[b].process(x, _tap_timeL[b], tap_outL, _tap_timeR[b], tap_outR, modulation);
         outL += tap_outL * _tap_gainL[b];
         outR += tap_outR * _tap_gainR[b];

         x *= _decay_multiply.v;
      }

      wetL[k] = outL;
      wetR[k] = outR;
      _state = x;
      _decay_multiply.process();
      _diffusion.process();
      _buildup.process();
      _hf_damp_coefficent.process();
      _lfo.process();
      _modulation.process();
   }

   // scale width
   float M alignas(16)[BLOCK_SIZE],
         S alignas(16)[BLOCK_SIZE];
   encodeMS(wetL, wetR, M, S, BLOCK_SIZE_QUAD);
   width.multiply_block(S, BLOCK_SIZE_QUAD);
   decodeMS(M, S, wetL, wetR, BLOCK_SIZE_QUAD);

   mix.fade_2_blocks_to(dataL, wetL, dataR, wetR, dataL, dataR, BLOCK_SIZE_QUAD);
}

void Reverb2Effect::suspend()
{
   init();
}

const char* Reverb2Effect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Pre-Delay";
   case 1:
      return "Reverb";
   case 2:
      return "EQ";
   case 3:
      return "Output";
   }
   return 0;
}
int Reverb2Effect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 5;
   case 2:
      return 17;
   case 3:
      return 23;
   }
   return 0;
}

void Reverb2Effect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[r2p_predelay].set_name("Pre-Delay");
   fxdata->p[r2p_predelay].set_type(ct_reverbpredelaytime);

   fxdata->p[r2p_room_size].set_name("Room Size");
   fxdata->p[r2p_room_size].set_type(ct_percent_bidirectional);
   fxdata->p[r2p_decay_time].set_name("Decay Time");
   fxdata->p[r2p_decay_time].set_type(ct_reverbtime);
   fxdata->p[r2p_diffusion].set_name("Diffusion");
   fxdata->p[r2p_diffusion].set_type(ct_percent);
   fxdata->p[r2p_buildup].set_name("Buildup");
   fxdata->p[r2p_buildup].set_type(ct_percent);
   fxdata->p[r2p_modulation].set_name("Modulation");
   fxdata->p[r2p_modulation].set_type(ct_percent);

   fxdata->p[r2p_hf_damping].set_name("HF Damping");
   fxdata->p[r2p_hf_damping].set_type(ct_percent);
   fxdata->p[r2p_lf_damping].set_name("LF Damping");
   fxdata->p[r2p_lf_damping].set_type(ct_percent);

   fxdata->p[r2p_width].set_name("Width");
   fxdata->p[r2p_width].set_type(ct_decibel_narrow);
   fxdata->p[r2p_mix].set_name("Mix");
   fxdata->p[r2p_mix].set_type(ct_percent);

   for( int i=r2p_predelay; i<r2p_num_params; ++i )
   {
      auto a = 1;
      if( i >= r2p_room_size ) a += 2;
      if( i >= r2p_lf_damping ) a += 2;
      if( i >= r2p_width ) a += 2;
      fxdata->p[i].posy_offset = a;
   }
}

void Reverb2Effect::init_default_values()
{
   fxdata->p[r2p_predelay].val.f = -4.f;
   fxdata->p[r2p_decay_time].val.f = 0.75f;
   fxdata->p[r2p_mix].val.f = 0.33f;
   fxdata->p[r2p_width].val.f = 0.0f;
   fxdata->p[r2p_diffusion].val.f = 1.0f;
   fxdata->p[r2p_buildup].val.f = 1.0f;
   fxdata->p[r2p_modulation].val.f = 0.5f;
   fxdata->p[r2p_hf_damping].val.f = 0.2f;
   fxdata->p[r2p_lf_damping].val.f = 0.2f;
   fxdata->p[r2p_room_size].val.f = 0.f;
}
