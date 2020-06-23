#include "effect_defs.h"
#include <algorithm>

/* vocoder */

// Hz from http://www.sequencer.de/pix/moog/moog_vocoder_rack.jpg
// const float vocoder_freq_moog[n_vocoder_bands-1] = {50, 158, 200, 252, 317, 400, 504, 635, 800,
// 1008, 1270, 1600, 2016, 2504, 3200, 4032, 5080};
// const float vocoder_freq[n_vocoder_bands] =
// {100, 175, 230, 290, 360, 450, 580, 700, 900, 1100, 1400, 1800, 2300, 2800, 3600, 4700}; const

// float vocoder_freq[n_vocoder_bands] = {100, 175, 230, 290, 360, 450, 580, 700, 900, 1100, 1400,
// 1800, 2300, 3000, 4500, 8000};
// const float vocoder_freq[n_vocoder_bands] = {158, 200, 252, 317,
// 400, 504, 635, 800, 1008, 1270, 1600, 2016, 2504, 3200, 4032, 5080};

// const float vocoder_freq_vsm201[n_vocoder_bands-1] =
// {190, 290, 400, 510, 630, 760, 910, 1080, 1270, 1480, 1700, 2000, 2300, 2700, 3100, 3700,
// 4400, 5200, 6600, 8000};
// const float vocoder_freq_vsm201[n_vocoder_bands] = 	{170, 240, 340, 440,
// 560, 680, 820, 970, 1150, 1370, 1605, 1850, 2150, 2500, 2900, 3400, 4050, 4850, 5850, 7500};

const float vocoder_freq_vsm201[n_vocoder_bands] = {180,  219,  266,  324,  394,  480,  584,
                                                    711,  865,  1053, 1281, 1559, 1898, 2309,
                                                    2810, 3420, 4162, 5064, 6163, 7500};

//================================================================================================

//------------------------------------------------------------------------------------------------

VocoderEffect::VocoderEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), mBI(0)
{
   /*mVoicedDetect.storage = storage;
   mUnvoicedDetect.storage = storage;
   mVoicedLevel = 0.f;
   mUnvoicedLevel = 0.f;*/

   active_bands = n_vocoder_bands;
   mGain.set_blocksize(BLOCK_SIZE);

   for (int i = 0; i < NVocoderVec; i++)
   {
      mEnvF[i] = vZero;
   }
}

//------------------------------------------------------------------------------------------------

VocoderEffect::~VocoderEffect()
{}

//------------------------------------------------------------------------------------------------

void VocoderEffect::init()
{
   setvars(true);
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::setvars(bool init)
{
   float Freq[4], FreqM[4];

   const float Q = 20.f * (1.f + 0.5f * *f[KQuality]);
   const float Spread = 0.4f / Q;

   active_bands = *pdata_ival[kNumBands];
   active_bands = active_bands - ( active_bands % 4 ); // FIXME - adjust the UI to be chunks of 4

   // We need to clamp these in reasonable ranges
   float flo = limit_range( *f[kFreqLo], -36.f, 36.f );
   float fhi = limit_range( *f[kFreqHi], 0.f, 60.f );
   
   if( flo > fhi )
   {
       auto t = fhi;
       fhi = flo;
       flo = t;
   }
   float df = (fhi-flo)/(active_bands-1);

   float hzlo = 440.f * pow(2.f, flo / 12.f );
   float dhz = pow(2.f, df / 12.f );

   float fb = hzlo;

   float mb = fb;
   float mdhz = dhz;
   bool sepMod = false;

   float mC = *f[kModCenter];
   float mX = *f[kModExpand];

   if( mC != 0 || mX != 0 )
   {
       sepMod = true;
       auto fDist = fhi - flo;
       auto fDistHalf = fDist / 2.f;
       auto mMid = fDistHalf + flo + 0.3 * mC * fDistHalf; // that 0.3 is a tuning choice about how far we can move center
       auto mLo = mMid - fDistHalf * ( 1 + 0.7 * mX ); // as is that 0.7
       auto dM = fDistHalf * 2 * ( 1.0 + 0.7 * mX ) / (active_bands - 1);

       auto mHi = mLo + dM * (active_bands - 1);

       if( mHi > 60 )
           dM = ( 60 - mLo ) / (active_bands - 1);

       mHi = mLo + dM * (active_bands - 1);

       mb = 440.0 * pow(2.f, mLo / 12.f );
       mdhz = pow( 2.f, dM / 12.f );
   }
   
   for (int i = 0; i < active_bands && i < n_vocoder_bands; i++)
   {
      Freq[i & 3] = fb * samplerate_inv;
      FreqM[i & 3] = mb * samplerate_inv;
      
      if ((i & 3) == 3)
      {
         int j = i >> 2;
         mCarrierL[j].SetCoeff(Freq, Q, Spread);
         mCarrierR[j].CopyCoeff(mCarrierL[j]);
         if( sepMod )
         {
             mModulator[j].SetCoeff(FreqM, Q, Spread);
         }
         else
         {
             mModulator[j].CopyCoeff(mCarrierL[j]);
         }
      }
      fb *= dhz;
      mb *= mdhz;
   }

   /*mVoicedDetect.coeff_LP(BiquadFilter::calc_omega_from_Hz(1000.f), 0.707);
   mUnvoicedDetect.coeff_HP(BiquadFilter::calc_omega_from_Hz(5000.f), 0.707);

   if (init)
   {
      mVoicedDetect.coeff_instantize();
      mUnvoicedDetect.coeff_instantize();
   }*/
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::process(float* dataL, float* dataR)
{
   mBI = (mBI + 1) & 0x3f;

   if (mBI == 0)
   {
      setvars(false);
   }

   float modulator_in alignas(16)[BLOCK_SIZE];

   add_block(storage->audio_in_nonOS[0], storage->audio_in_nonOS[1], modulator_in, BLOCK_SIZE_QUAD);

   float Gain = *f[KGain] + 24.f;
   mGain.set_target_smoothed(db_to_linear(Gain));
   mGain.multiply_block(modulator_in, BLOCK_SIZE_QUAD);

   float EnvFRate = 0.001f * powf(2.f, 4.f * *f[KRate]);

   // Voiced / Unvoiced detection
/*   mVoicedDetect.process_block_to(modulator_in, modulator_tbuf);
   float a = min(4.f, get_squaremax(modulator_tbuf,BLOCK_SIZE_QUAD));
   mVoicedLevel = mVoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

   mUnvoicedDetect.process_block_to(modulator_in, modulator_tbuf);
   a = min(4.f, get_squaremax(modulator_tbuf, BLOCK_SIZE_QUAD));
   mUnvoicedLevel = mUnvoicedLevel * (1.f - EnvFRate) + a * EnvFRate;

   float Ratio = db_to_linear(*f[KUnvoicedThreshold]);

   if (mUnvoicedLevel > (mVoicedLevel * Ratio))
     // mix carrier with noise
     for(int i = 0; i < BLOCK_SIZE; i++)
     {
        float rand11 = (((float) rand() / RAND_MAX) * 2.f - 1.f);
        dataL[i] = rand11;
        rand11 = (((float) rand() / RAND_MAX) * 2.f - 1.f);
        dataR[i] = rand11;
     }*/

   const vFloat MaxLevel = vLoad1(6.f);

   vFloat Rate = vLoad1(EnvFRate);
   vFloat Ratem1 = vLoad1(1.f - EnvFRate);

   float Gate = db_to_linear(*f[KGateLevel] + Gain);
   vFloat GateLevel = vLoad1(Gate * Gate);

   for (int k = 0; k < BLOCK_SIZE; k++)
   {
      vFloat In = vLoad1(modulator_in[k]);
      vFloat Left = vLoad1(dataL[k]);
      vFloat Right = vLoad1(dataR[k]);

      vFloat LeftSum = vZero;
      vFloat RightSum = vZero;

      for (int j = 0; j < (active_bands >> 2) && j < ( n_vocoder_bands >> 2 ) /*(NVocoderVec)*/; j++)
      {
         vFloat Mod = mModulator[j].CalcBPF(In);
         Mod = vMin(vMul(Mod, Mod), MaxLevel);

         Mod = vAnd(Mod, vCmpGE(Mod, GateLevel));

         mEnvF[j] = vMAdd(mEnvF[j], Ratem1, vMul(Rate, Mod));

         Mod = vSqrtFast(mEnvF[j]);

         LeftSum = vAdd(LeftSum, mCarrierL[j].CalcBPF(vMul(Left, Mod)));
         RightSum = vAdd(RightSum, mCarrierR[j].CalcBPF(vMul(Right, Mod)));
      }

      dataL[k] = vSum(LeftSum) * 4.f;
      dataR[k] = vSum(RightSum) * 4.f;
   }
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::suspend()
{}

//------------------------------------------------------------------------------------------------

void VocoderEffect::init_default_values()
{
   fxdata->p[KGain].val.f = 0.f;
   fxdata->p[KGateLevel].val.f = -96.f;
   fxdata->p[KRate].val.f = 0.f;
   fxdata->p[KQuality].val.f = 0.f;

   fxdata->p[kNumBands].val.i = n_vocoder_bands;

   // freq = 440 * 2^(v/12)
   // log(freq/440)/log(2) = v/12
   // 12 * log(freq/440) / log(2) = v;
   fxdata->p[kFreqLo].val.f = 12.f * log(vocoder_freq_vsm201[0]/440.f)/log(2.f);
   fxdata->p[kFreqHi].val.f = 12.f * log(vocoder_freq_vsm201[n_vocoder_bands-1]/440.f)/log(2.f);
   
   fxdata->p[kModExpand].val.f = 0.f;
   fxdata->p[kModCenter].val.f = 0.f;
}

//------------------------------------------------------------------------------------------------

const char* VocoderEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Input";
   case 1:
      return "Filter Bank";
   case 2:
       return "Carrier";
   case 3:
       return "Modulator";
   }
   return 0;
}

//------------------------------------------------------------------------------------------------

int VocoderEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 7;
   case 2:
       return 13;
   case 3:
       return 21;
   }
   return 0;
}

//------------------------------------------------------------------------------------------------

void VocoderEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[KGain].set_name("Gain");
   fxdata->p[KGain].set_type(ct_decibel);
   fxdata->p[KGain].posy_offset = 1;

   fxdata->p[KGateLevel].set_name("Gate");
   fxdata->p[KGateLevel].set_type(ct_decibel_attenuation_large);
   fxdata->p[KGateLevel].posy_offset = 1;

   fxdata->p[KRate].set_name("Env Follow");
   fxdata->p[KRate].set_type(ct_percent);
   fxdata->p[KRate].posy_offset = 3;

   /*fxdata->p[KUnvoicedThreshold].set_name("Unvoiced Thresh");
   fxdata->p[KUnvoicedThreshold].set_type(ct_decibel);
   fxdata->p[KUnvoicedThreshold].posy_offset = 3;*/

   fxdata->p[KQuality].set_name("Q");
   fxdata->p[KQuality].set_type(ct_percent_bidirectional);
   fxdata->p[KQuality].posy_offset = 3;

   fxdata->p[kNumBands].set_name("Bands");
   fxdata->p[kNumBands].set_type(ct_vocoder_bandcount);
   fxdata->p[kNumBands].posy_offset = 3;

   fxdata->p[kFreqLo].set_name("Min Frequency");
   fxdata->p[kFreqLo].set_type(ct_freq_vocoder_low);
   fxdata->p[kFreqLo].posy_offset = 3;

   fxdata->p[kFreqHi].set_name("Max Frequency");
   fxdata->p[kFreqHi].set_type(ct_freq_vocoder_high);
   fxdata->p[kFreqHi].posy_offset = 3;

   fxdata->p[kModExpand].set_name("Mod Range");
   fxdata->p[kModExpand].set_type(ct_percent_bidirectional);
   fxdata->p[kModExpand].posy_offset = 5;
   
   fxdata->p[kModCenter].set_name("Mod Center");
   fxdata->p[kModCenter].set_type(ct_percent_bidirectional);
   fxdata->p[kModCenter].posy_offset = 5;

}

void VocoderEffect::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
    if( streamingRevision <= 10 )
    {
        fxdata->p[kNumBands].val.i = n_vocoder_bands;
        
        fxdata->p[kFreqLo].val.f = 12.f * log(vocoder_freq_vsm201[0]/440.f)/log(2.f);
        fxdata->p[kFreqHi].val.f = 12.f * log(vocoder_freq_vsm201[n_vocoder_bands-1]/440.f)/log(2.f);
        
        fxdata->p[kModExpand].val.f = 0.f;
        fxdata->p[kModCenter].val.f = 0.f;
    }
}
//------------------------------------------------------------------------------------------------
