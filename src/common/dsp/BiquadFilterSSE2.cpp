#if 0
#include <math.h>
#include "BiquadFilter.h"
#include "globals.h"
#include <complex>

// these were slower than the corresponding x87 variant (2006)
// do not use until profiled to be quicker

void biquadunit::process_block_SSE2(double *data)
{	
	for(int k=0; k<BLOCK_SIZE; k+=2)
	{		
		__m128d input = _mm_load_sd(data+k);
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op0 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op0));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op0));
		_mm_store_sd(data+k,op0);

		input = _mm_load_sd(data+k);
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		op0 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op0));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op0));
		_mm_store_sd(data+k,op0);
	}
}

void biquadunit::process_block_SSE2(float *data)
{	
	for(int k=0; k<BLOCK_SIZE; k+=4)
	{
		// load
		__m128 vl = _mm_load_ps(data + k);						
		// first
		__m128d input = _mm_cvtss_sd (input, vl);
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op0 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op0));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op0));
		
		// second
		input = _mm_cvtss_sd (input, _mm_shuffle_ps(vl,vl,_MM_SHUFFLE(0, 0, 0, 1)));
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op1 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op1));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op1));

		// third
		input = _mm_cvtss_sd (input, _mm_shuffle_ps(vl,vl,_MM_SHUFFLE(0, 0, 0, 2)));
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op2 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op2));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op2));

		// fourth
		input = _mm_cvtss_sd (input, _mm_shuffle_ps(vl,vl,_MM_SHUFFLE(0, 0, 0, 3)));
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op3 = _mm_add_sd(reg0.v,_mm_mul_sd(b0.v.v, input));
		reg0.v = _mm_sub_sd(_mm_add_sd(_mm_mul_sd(b1.v.v, input), reg1.v), _mm_mul_sd(a1.v.v, op3));
		reg1.v = _mm_sub_sd(_mm_mul_sd(b2.v.v, input), _mm_mul_sd(a2.v.v, op3));

		// store
		__m128 sl = _mm_cvtpd_ps(_mm_unpacklo_pd(op0,op1)); 
		sl = _mm_movelh_ps(sl,_mm_cvtpd_ps(_mm_unpacklo_pd(op2,op3))); 
		_mm_store_ps(data+k,sl);		
	}
}

void biquadunit::process_block_SSE2(float *dataL,float *dataR)
{	
	for(int k=0; k<BLOCK_SIZE; k+=4)
	{
		// load
		__m128 vl = _mm_load_ps(dataL + k);
		__m128 vr = _mm_load_ps(dataR + k);
		__m128 v0 = _mm_unpacklo_ps(vl,vr);

		// first
		__m128d input = _mm_cvtps_pd(v0);
		v0 = _mm_movehl_ps(v0,v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();

		__m128d op0 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op0));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op0));

		// second
		input = _mm_cvtps_pd(v0);
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op1 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op1));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op1));

		// third
		v0 = _mm_unpackhi_ps(vl,vr);
		input = _mm_cvtps_pd(v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op2 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op2));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op2));

		// fourth
		v0 = _mm_movehl_ps(v0,v0);
		input = _mm_cvtps_pd(v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op3 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op3));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op3));

		// store
		__m128 sl = _mm_cvtpd_ps(_mm_unpacklo_pd(op0,op1)); 
		sl = _mm_movelh_ps(sl,_mm_cvtpd_ps(_mm_unpacklo_pd(op2,op3))); 
		_mm_store_ps(dataL+k,sl);

		__m128 sr = _mm_cvtpd_ps(_mm_unpackhi_pd(op0,op1)); 
		sr = _mm_movelh_ps(sr,_mm_cvtpd_ps(_mm_unpackhi_pd(op2,op3))); 
		_mm_store_ps(dataR+k,sr);
	}
}

void biquadunit::process_block_slowlag_SSE2(float *dataL,float *dataR)
{	
	a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();

	for(int k=0; k<BLOCK_SIZE; k+=4)
	{
		// load
		__m128 vl = _mm_load_ps(dataL + k);
		__m128 vr = _mm_load_ps(dataR + k);
		__m128 v0 = _mm_unpacklo_ps(vl,vr);

		// first
		__m128d input = _mm_cvtps_pd(v0);
		v0 = _mm_movehl_ps(v0,v0);				

		__m128d op0 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op0));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op0));

		// second
		input = _mm_cvtps_pd(v0);		
		__m128d op1 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op1));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op1));

		// third
		v0 = _mm_unpackhi_ps(vl,vr);
		input = _mm_cvtps_pd(v0);				
		__m128d op2 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op2));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op2));

		// fourth
		v0 = _mm_movehl_ps(v0,v0);
		input = _mm_cvtps_pd(v0);				
		__m128d op3 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op3));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op3));

		// store
		__m128 sl = _mm_cvtpd_ps(_mm_unpacklo_pd(op0,op1)); 
		sl = _mm_movelh_ps(sl,_mm_cvtpd_ps(_mm_unpacklo_pd(op2,op3))); 
		_mm_store_ps(dataL+k,sl);

		__m128 sr = _mm_cvtpd_ps(_mm_unpackhi_pd(op0,op1)); 
		sr = _mm_movelh_ps(sr,_mm_cvtpd_ps(_mm_unpackhi_pd(op2,op3))); 
		_mm_store_ps(dataR+k,sr);
	}
}

void biquadunit::process_block_to_SSE2(float *dataL,float *dataR, float *dstL,float *dstR)
{	
	for(int k=0; k<BLOCK_SIZE; k+=4)
	{
		// load
		__m128 vl = _mm_load_ps(dataL + k);
		__m128 vr = _mm_load_ps(dataR + k);
		__m128 v0 = _mm_unpacklo_ps(vl,vr);

		// first
		__m128d input = _mm_cvtps_pd(v0);
		v0 = _mm_movehl_ps(v0,v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();

		__m128d op0 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op0));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op0));

		// second
		input = _mm_cvtps_pd(v0);
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op1 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op1));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op1));

		// third
		v0 = _mm_unpackhi_ps(vl,vr);
		input = _mm_cvtps_pd(v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op2 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op2));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op2));

		// fourth
		v0 = _mm_movehl_ps(v0,v0);
		input = _mm_cvtps_pd(v0);		
		a1.process_SSE2();	a2.process_SSE2();	b0.process_SSE2();	b1.process_SSE2();	b2.process_SSE2();
		__m128d op3 = _mm_add_pd(reg0.v,_mm_mul_pd(b0.v.v, input));
		reg0.v = _mm_sub_pd(_mm_add_pd(_mm_mul_pd(b1.v.v, input), reg1.v), _mm_mul_pd(a1.v.v, op3));
		reg1.v = _mm_sub_pd(_mm_mul_pd(b2.v.v, input), _mm_mul_pd(a2.v.v, op3));

		// store
		__m128 sl = _mm_cvtpd_ps(_mm_unpacklo_pd(op0,op1)); 
		sl = _mm_movelh_ps(sl,_mm_cvtpd_ps(_mm_unpacklo_pd(op2,op3))); 
		_mm_store_ps(dstL+k,sl);

		__m128 sr = _mm_cvtpd_ps(_mm_unpackhi_pd(op0,op1)); 
		sr = _mm_movelh_ps(sr,_mm_cvtpd_ps(_mm_unpackhi_pd(op2,op3))); 
		_mm_store_ps(dstR+k,sr);
	}
}
#endif
