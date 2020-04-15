#include "effect_defs.h"
#include <vt_dsp/halfratefilter.h>

/* distortion			*/

// feedback kan bli knepigt med sse-packed

const int dist_OS_bits = 2;
const int distortion_OS = 1 << dist_OS_bits;

DistortionEffect::DistortionEffect(SurgeStorage* storage, FxStorage* fxdata, pdata* pd)
    : Effect(storage, fxdata, pd), band1(storage), band2(storage), lp1(storage), lp2(storage),
      hr_a(3, false), hr_b(3, true)
{
   lp1.setBlockSize(BLOCK_SIZE * distortion_OS);
   lp2.setBlockSize(BLOCK_SIZE * distortion_OS);
   drive.set_blocksize(BLOCK_SIZE);
   outgain.set_blocksize(BLOCK_SIZE);
}

DistortionEffect::~DistortionEffect()
{}

void DistortionEffect::init()
{
   setvars(true);
   band1.suspend();
   band2.suspend();
   lp1.suspend();
   lp2.suspend();
   bi = 0;
   L = 0;
   R = 0;
}

void DistortionEffect::setvars(bool init)
{

   if (init)
   {
      float pregain = fxdata->p[0].get_extended(fxdata->p[0].val.f);
      float postgain = fxdata->p[6].get_extended(fxdata->p[6].val.f);
      band1.coeff_peakEQ(band1.calc_omega(fxdata->p[1].val.f / 12.f), fxdata->p[2].val.f,
                         pregain);
      band2.coeff_peakEQ(band2.calc_omega(fxdata->p[7].val.f / 12.f), fxdata->p[8].val.f,
                         postgain);
      drive.set_target(0.f);
      outgain.set_target(0.f);
   }
   else
   {
      float pregain = fxdata->p[0].get_extended(*f[0]);
      float postgain = fxdata->p[6].get_extended(*f[6]);
      band1.coeff_peakEQ(band1.calc_omega(*f[1] / 12.f), *f[2], pregain);
      band2.coeff_peakEQ(band2.calc_omega(*f[7] / 12.f), *f[8], postgain);
      lp1.coeff_LP2B(lp1.calc_omega((*f[3] / 12.0) - 2.f), 0.707);
      lp2.coeff_LP2B(lp2.calc_omega((*f[9] / 12.0) - 2.f), 0.707);
      lp1.coeff_instantize();
      lp2.coeff_instantize();
   }
}

void DistortionEffect::process(float* dataL, float* dataR)
{
   // TODO fix denormals!
   if (bi == 0)
      setvars(false);
   bi = (bi + 1) & slowrate_m1;

   band1.process_block(dataL, dataR);
   drive.set_target_smoothed(db_to_linear(fxdata->p[4].get_extended(*f[4])));
   outgain.set_target_smoothed(db_to_linear(*f[10]));
   float fb = *f[5];
   int ws = *pdata_ival[11];
   if( ws < 0 || ws >= n_ws_type )
      ws = 0;
   
   float bL alignas(16)[BLOCK_SIZE << dist_OS_bits];
   float bR alignas(16)[BLOCK_SIZE << dist_OS_bits];
   assert(dist_OS_bits == 2);

   drive.multiply_2_blocks(dataL, dataR, BLOCK_SIZE_QUAD);

   for (int k = 0; k < BLOCK_SIZE; k++)
   {
      float a = (k & 16) ? 0.00000001 : -0.00000001; // denormal thingy
      float Lin = dataL[k];
      float Rin = dataR[k];
      for (int s = 0; s < distortion_OS; s++)
      {
         L = Lin + fb * L;
         R = Rin + fb * R;
         lp1.process_sample_nolag(L, R);
         L = lookup_waveshape(ws, L);
         R = lookup_waveshape(ws, R);
         L += a;
         R += a; // denormal
         lp2.process_sample_nolag(L, R);
         bL[s + (k << dist_OS_bits)] = L;
         bR[s + (k << dist_OS_bits)] = R;
      }
      // dataL[k] = L*outgain;
      // dataR[k] = R*outgain;
   }

   /*for(int k=0; k<BLOCK_SIZE; k++)
   {
           int kOS = k<<dist_OS_bits;
           //dataL[k] *= drive;
           //dataR[k] *= drive;
           lp1.process_sample_nolag(dataL[k],dataR[k],L[kOS],R[kOS]);
           lp1.process_sample_nolag_noinput(L[kOS+1],R[kOS+1]);
           lp1.process_sample_nolag_noinput(L[kOS+2],R[kOS+2]);
           lp1.process_sample_nolag_noinput(L[kOS+3],R[kOS+3]);
           //lp1.process_sample_nolag_noinput(L[kOS+4],R[kOS+4]);
           //lp1.process_sample_nolag_noinput(L[kOS+5],R[kOS+5]);
           //lp1.process_sample_nolag_noinput(L[kOS+6],R[kOS+6]);
           //lp1.process_sample_nolag_noinput(L[kOS+7],R[kOS+7]);
   }

   tanh7_block(L,BLOCK_SIZE_QUAD << dist_OS_bits);
   tanh7_block(R,BLOCK_SIZE_QUAD << dist_OS_bits);*/

   hr_a.process_block_D2(bL, bR, 128);
   hr_b.process_block_D2(bL, bR, 64);

   outgain.multiply_2_blocks_to(bL, bR, dataL, dataR, BLOCK_SIZE_QUAD);

   // lp2.process_block(dataL,dataR);

   band2.process_block(dataL, dataR);
}

void DistortionEffect::suspend()
{
   init();
}

const char* DistortionEffect::group_label(int id)
{
   switch (id)
   {
   case 0:
      return "Pre-EQ";
   case 1:
      return "Distortion";
   case 2:
      return "Post-EQ";
   case 3:
      return "Output";
   }
   return 0;
}
int DistortionEffect::group_label_ypos(int id)
{
   switch (id)
   {
   case 0:
      return 1;
   case 1:
      return 11;
   case 2:
      return 19;
   case 3:
      return 29;
   }
   return 0;
}

void DistortionEffect::init_ctrltypes()
{
   Effect::init_ctrltypes();

   fxdata->p[0].set_name("Gain");
   fxdata->p[0].set_type(ct_decibel_extendable);
   fxdata->p[1].set_name("Frequency");
   fxdata->p[1].set_type(ct_freq_audible);
   fxdata->p[2].set_name("Bandwidth");
   fxdata->p[2].set_type(ct_bandwidth);
   fxdata->p[3].set_name("High Cut");
   fxdata->p[3].set_type(ct_freq_audible);

   fxdata->p[4].set_name("Drive");
   fxdata->p[4].set_type(ct_decibel_narrow_extendable);
   fxdata->p[5].set_name("Feedback");
   fxdata->p[5].set_type(ct_percent_bidirectional);
   fxdata->p[11].set_name("Waveshaper");
   fxdata->p[11].set_type(ct_distortion_waveshape);

   fxdata->p[6].set_name("Gain");
   fxdata->p[6].set_type(ct_decibel_extendable);
   fxdata->p[7].set_name("Frequency");
   fxdata->p[7].set_type(ct_freq_audible);
   fxdata->p[8].set_name("Bandwidth");
   fxdata->p[8].set_type(ct_bandwidth);
   fxdata->p[9].set_name("High Cut");
   fxdata->p[9].set_type(ct_freq_audible);

   fxdata->p[10].set_name("Gain");
   fxdata->p[10].set_type(ct_decibel_narrow);

   fxdata->p[0].posy_offset = 1;
   fxdata->p[1].posy_offset = 1;
   fxdata->p[2].posy_offset = 1;
   fxdata->p[3].posy_offset = 1;

   fxdata->p[4].posy_offset = 3;
   fxdata->p[5].posy_offset = 3;
   fxdata->p[11].posy_offset = -7;

   fxdata->p[6].posy_offset = 7;
   fxdata->p[7].posy_offset = 7;
   fxdata->p[8].posy_offset = 7;
   fxdata->p[9].posy_offset = 7;

   fxdata->p[10].posy_offset = 9;
}
void DistortionEffect::init_default_values()
{
   fxdata->p[0].val.f = 0.f;
   fxdata->p[1].val.f = 0.f;
   fxdata->p[2].val.f = 2.f;

   fxdata->p[6].val.f = 0.f;
   fxdata->p[7].val.f = 0.f;
   fxdata->p[8].val.f = 2.f;
   fxdata->p[10].val.f = 0.f;

   fxdata->p[11].val.f = 0.f;
}

void DistortionEffect::handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
{
    if( streamingRevision <= 11 )
    {
       fxdata->p[11].val.i = 0;
       fxdata->p[0].extend_range = false;
       fxdata->p[6].extend_range = false;
    }
}
