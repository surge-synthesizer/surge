#include "effect_defs.h"
#include <vt_dsp/halfratefilter.h>

using namespace vt_dsp;

//Done better with FFT (Do this better with FFT? Should be done better with FFT?) görs bättre med fft

emphasize::emphasize(sub3_storage *storage, sub3_fx *fxdata, pdata* pd) 
: baseeffect(storage,fxdata,pd), EQ(storage), pre(3,true), post(3,true)
{	
}

emphasize::~emphasize()
{
}

void emphasize::init()
{		
	EQ.suspend();
	bi = 0;	
	outgain.set_target(0);
	outgain.instantize();
	setvars(true);	
}

void emphasize::setvars(bool init)
{
	if(init)
	{		
		EQ.coeff_orfanidisEQ(EQ.calc_omega(fxdata->p[1].val.f / 12.0), fxdata->p[2].val.f, 1.0, 0.5, 0.0);
	}
	else
	{
		EQ.coeff_orfanidisEQ(EQ.calc_omega(*f[1] / 12.0), *f[2], 1.0, 0.5, 0.0);		
	}
}

void emphasize::process(float *dataL, float *dataR)
{		
	// TODO fix denormals!
	if(bi==0) setvars(false);	
	bi = (bi+1) & slowrate_m1;	
	outgain.set_target(storage->db_to_linear(*f[0]));

	_MM_ALIGN16 float bL[block_size << 1];
	_MM_ALIGN16 float bR[block_size << 1];

	EQ.process_block_to(dataL,dataR,bL,bR);	
	
	pre.process_block_U2((__m128*)bL,(__m128*)bR,(__m128*)bL,(__m128*)bR,block_size_os);	

	__m128 type = _mm_load1_ps(f[3]);
	__m128 typem1 = _mm_sub_ps(m128_one,type);
	for(int k=0; k<block_size_os_quad; k++)
	{
		// y = x*x*(t + (1-t)*x)
		// y = x*x*(t + x - t*x) 
		// midre dependancies (noes, 1-t är precalc så det blir det inte alls
		// TRANSLATE: Less dependencies (does, 1-t is precal so it will not be at all
		__m128 L = _mm_load_ps(bL + (k<<2));
		__m128 LL = _mm_mul_ps(L,L);
		L = _mm_mul_ps(LL,_mm_sub_ps(_mm_add_ps(type,L), _mm_mul_ps(type,L)));
		_mm_store_ps(bL + (k<<2), L);

		__m128 R = _mm_load_ps(bR + (k<<2));
		__m128 RR = _mm_mul_ps(R,R);
		R = _mm_mul_ps(RR,_mm_sub_ps(_mm_add_ps(type,R), _mm_mul_ps(type,R)));
		_mm_store_ps(bR + (k<<2), R);
	}		

	post.process_block_D2((__m128*)bL,(__m128*)bR,block_size_os);		

	outgain.MAC_2_blocks_to((__m128*)bL,(__m128*)bR,(__m128*)dataL,(__m128*)dataR,block_size_quad);

}

void emphasize::suspend()
{ 
	init();
}

char* emphasize::group_label(int id)
{
	switch (id)
	{
	case 0:
		return "EMPHASIS";		
	}
	return 0;
}
int emphasize::group_label_ypos(int id)
{
	switch (id)
	{
	case 0:
		return 1;		
	}
	return 0;
}

void emphasize::init_ctrltypes()
{
	baseeffect::init_ctrltypes();

	fxdata->p[0].set_name("amount");	fxdata->p[0].set_type(ct_decibel);
	fxdata->p[1].set_name("freq");		fxdata->p[1].set_type(ct_freq_audible);
	fxdata->p[2].set_name("BW");		fxdata->p[2].set_type(ct_bandwidth);			
	fxdata->p[3].set_name("type");		fxdata->p[3].set_type(ct_percent);

	fxdata->p[0].posy_offset = 1;
	fxdata->p[1].posy_offset = 1;
	fxdata->p[2].posy_offset = 1;
	fxdata->p[3].posy_offset = 1;
}
void emphasize::init_default_values()
{
	fxdata->p[0].val.f = 0.f;
}
