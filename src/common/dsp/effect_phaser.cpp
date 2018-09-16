#include "effect_defs.h"

/* phaser			*/

using namespace vt_dsp;

enum phaseparam
{
	pp_base=0,
	//pp_spread,
	//pp_distribution,
	//pp_count,
	pp_feedback,
	pp_q,
	pp_lforate,
	pp_lfodepth,
	pp_stereo,
	pp_mix,	
	pp_nparams,
};

float bend(float x, float b)
{	
	return (1.f + b)*x - b*x*x*x;
}

phaser::phaser(sub3_storage *storage, sub3_fx *fxdata, pdata* pd) 
: baseeffect(storage,fxdata,pd)
{
	for(int i=0; i<n_bq_units; i++)
	{
		biquad[i] = (biquadunit*) _aligned_malloc(sizeof(biquadunit),16);
		memset(biquad[i],0,sizeof(biquadunit));
		new(biquad[i]) biquadunit(storage);		
	}
	mix.set_blocksize(block_size);	
	feedback.setBlockSize(block_size*slowrate);
	bi = 0;
}

phaser::~phaser()
{
	for(int i=0; i<n_bq_units; i++)	_aligned_free(biquad[i]);
}

void phaser::init()
{
	lfophase = 0.25f;
	//setvars(true);	
	for(int i=0; i<n_bq_units; i++) 
	{
		//notch[i]->coeff_LP(1.0,1.0);
		biquad[i]->suspend();						
	}
	clear_block(L,block_size_quad);
	clear_block(R,block_size_quad);
	mix.set_target(1.f);
	mix.instantize();
	bi = 0;
	dL = 0;
	dR = 0;
	bi = 0;
}

void phaser::process_only_control()
{
	float rate = envelope_rate_linear(-*f[pp_lforate]) * (fxdata->p[pp_lforate].temposync?storage->temposyncratio:1.f);
	lfophase += (float)slowrate*rate;
	if(lfophase>1) lfophase -= 1;	
	float lfophaseR = lfophase + 0.5 * *f[pp_stereo];
	if(lfophaseR>1) lfophaseR -= 1;	
}

float basefreq[4] = {1.5/12, 19.5/12, 35/12, 50/12};
float basespan[4] = {2.0, 1.5, 1.0, 0.5};

void phaser::setvars()
{
	double rate = envelope_rate_linear(-*f[pp_lforate]) * (fxdata->p[pp_lforate].temposync?storage->temposyncratio:1.f);
	lfophase += (float)slowrate*rate;
	if(lfophase>1) lfophase -= 1;	
	float lfophaseR = lfophase + 0.5 * *f[pp_stereo];
	if(lfophaseR>1) lfophaseR -= 1;	
	double lfoout = 1.f - fabs(2.0-4.0*lfophase);
	double lfooutR = 1.f - fabs(2.0-4.0*lfophaseR);

	for(int i=0; i<n_bq; i++)
	{
		double omega = biquad[0]->calc_omega(2.0 * *f[pp_base] + basefreq[i] + basespan[i] * lfoout * *f[pp_lfodepth]);		
		biquad[i]->coeff_APF(omega,1.0 + 0.8* *f[pp_q]);
		omega = biquad[0]->calc_omega(2.0 * *f[pp_base] + basefreq[i] + basespan[i] * lfooutR * *f[pp_lfodepth]);		
		biquad[i+n_bq]->coeff_APF(omega,1.0 + 0.8* *f[pp_q]);
	}

	feedback.newValue(0.95f * *f[pp_feedback]);	
}

void phaser::process(float *dataL, float *dataR)
{		
	if(bi==0) setvars();	
	bi = (bi+1)&slowrate_m1;
	//feedback.multiply_2_blocks((__m128*)L,(__m128*)R, block_size_quad);
	//accumulate_block((__m128*)dataL, (__m128*)L, block_size_quad);
	//accumulate_block((__m128*)dataR, (__m128*)R, block_size_quad);		
	for(int i=0; i<block_size; i++)
	{
		feedback.process();
		dL = dataL[i] + dL*feedback.v;
		dR = dataR[i] + dR*feedback.v;
		dL = limit_range(dL,-32.f,32.f);
		dR = limit_range(dR,-32.f,32.f);
		dL = biquad[0]->process_sample(dL);
		dL = biquad[1]->process_sample(dL);
		dL = biquad[2]->process_sample(dL);
		dL = biquad[3]->process_sample(dL);
		dR = biquad[4]->process_sample(dR);
		dR = biquad[5]->process_sample(dR);
		dR = biquad[6]->process_sample(dR);
		dR = biquad[7]->process_sample(dR);
		L[i] = dL;
		R[i] = dR;
	}
		
	mix.set_target_smoothed(limit_range(*f[pp_mix],0.f,1.f));	
	mix.fade_2_blocks_to(dataL,L,dataR,R,dataL,dataR,block_size_quad);	
}

void phaser::suspend()
{ 
	init();	
}

const char* phaser::group_label(int id)
{
	switch (id)
	{
	case 0:
		return "Phaser";	
	case 1:
		return "Modulation";
	case 2:
		return "Mix";
	}
	return 0;
}
int phaser::group_label_ypos(int id)
{
	switch (id)
	{
	case 0:
		return 1;		
	case 1:
		return 9;	
	case 2:
		return 17;	
	}
	return 0;
}

void phaser::init_ctrltypes()
{
	baseeffect::init_ctrltypes();

	fxdata->p[pp_base].set_name("Base Freq");				fxdata->p[pp_base].set_type(ct_percent_bidirectional);	
	fxdata->p[pp_feedback].set_name("Feedback");			fxdata->p[pp_feedback].set_type(ct_percent_bidirectional);
	fxdata->p[pp_q].set_name("Q");							fxdata->p[pp_q].set_type(ct_percent_bidirectional);

	fxdata->p[pp_lforate].set_name("Rate");			fxdata->p[pp_lforate].set_type(ct_lforate);
	fxdata->p[pp_lfodepth].set_name("Depth");		fxdata->p[pp_lfodepth].set_type(ct_percent);
	fxdata->p[pp_stereo].set_name("Stereo");			fxdata->p[pp_stereo].set_type(ct_percent);	

	fxdata->p[pp_mix].set_name("Mix");				fxdata->p[pp_mix].set_type(ct_percent);	

	for(int i=0; i<pp_nparams; i++) fxdata->p[i].posy_offset = 1 + ((i>=pp_lforate)?2:0) + ((i>=pp_mix)?2:0);
}
void phaser::init_default_values()
{
	//fxdata->p[0].val.f = 0.f;
}
