#include "basic_dsp.h"
#include <assert.h>
#if MAC
#include <algorithm>
#endif

int Min(int a, int b)
{
#if _M_X64 || PPC
	return min(a,b);
#else
	__asm
	{
		mov		eax, a
		mov		ecx, b
		cmp		eax, ecx
		cmovg	eax, ecx		
	}
#endif
}
int Max(int a, int b)
{
#if _M_X64 || PPC
	return max(a,b);
#else
	__asm
	{
		mov		eax, a
		mov		ecx, b
		cmp		eax, ecx
		cmovl	eax, ecx		
	}
#endif
}

double Max(double a, double b)
{
#if (_M_IX86_FP>1)
	_mm_store_sd(&a, _mm_max_sd(_mm_load_sd(&a),_mm_load_sd(&b)));
	return a;
#else
	if(a>b) return a;	
	return b;
#endif
}

unsigned int Min(unsigned int a, unsigned int b)
{
#if _M_X64 || PPC
	return min(a,b);
#else
	__asm
	{
		mov		eax, a
		mov		ecx, b
		cmp		eax, ecx
		cmova	eax, ecx		
	}
#endif
}
unsigned int Max(unsigned int a, unsigned int b)
{
#if _M_X64 || PPC
	return max(a,b);
#else
	__asm
	{
		mov		eax, a
		mov		ecx, b
		cmp		eax, ecx
		cmovb	eax, ecx		
	}
#endif
}

int limit_range(int x, int l, int h)
{	
#if _M_X64 || PPC
	return max(min(x,h),l);
#else
	__asm
	{
		mov		eax, x
		mov		edx, l
		cmp		eax, edx
		cmovl	eax, edx
		mov		edx, h
		cmp		eax, edx
		cmovg	eax, edx
	}
#endif
}

int Wrap(int x, int L, int H)
{
#if _M_X64 || PPC
	// fattar inte vad den gör längre..
	//int diff = H - L;
	//if(x > H) x = x-H;
	assert(0);
	return 0;
#else
	__asm
	{
		; load values
		mov		ecx, H
		mov		edx, ecx		
		mov		eax, x
		; calc diff		
		sub		edx, L		
		; prepare wrapped val 1	into ebx
		mov		ebx, eax
		sub		ebx, H	
		; compare with H
		cmp		eax, ecx
		cmovg	eax, ebx				
		; prepare wrapped val 2 into edx	
		add		edx, eax	
		; compare with L
		cmp		eax, L
		cmovl	eax, edx
	}	
#endif
}

int Sign(int x)
{
#if _M_X64 || PPC
	return (x<0)?-1:1;
#else
	__asm
	{		
		mov eax, 1
		mov edx, -1		
		cmp x, 0
		cmovle eax, edx
	};
#endif
}

unsigned int limit_range(unsigned int x, unsigned int l, unsigned int h)
{
#if _M_X64 || PPC
	return max(min(x,h),l);
#else
	__asm
	{
		mov		eax, x		
		mov		ecx, l	; load min
		cmp		eax, ecx
		cmovb	eax, ecx	; move if below
		mov		ecx, h	; load max
		cmp		eax, ecx
		cmova	eax, ecx	; move if above
	}	
#endif
}
/*
float limit_range(float x, float min, float max)
{	
   float result;
	_mm_store_ss(&result, _mm_min_ss(_mm_max_ss(_mm_load_ss(&x),_mm_load_ss(&min)),_mm_load_ss(&max)));
	return result;
}*/

double limit_range(double x, double min, double max)
{
/*#if !PPC
	if(SSE_VERSION >= 2)
	{
		_mm_store_sd(&x, _mm_min_sd(_mm_max_sd(_mm_load_sd(&x),_mm_load_sd(&min)),_mm_load_sd(&max)));
		return x;
	}
#endif*/
	
	if(x>max) return max;
	if(x<min) return min;
	return x;
}

int Float2Int(float x)
{
#if PPC
	return (int)x;
#else
	return _mm_cvt_ss2si(_mm_load_ss(&x));
#endif
}

void float2i15_block(float*f,short*s,int n)
{		
	for(int i=0; i<n; i++)
	{
		s[i] = (short)(int)limit_range((int)((float)f[i]*16384.f),-16384,16383);
	}
}

void i152float_block(short*s,float*f,int n)
{
	const float scale = 1.f/16384.f;
	for(int i=0; i<n; i++)
	{
		f[i] = (float)s[i]*scale;
	}
}

void hardclip_block(float *x, unsigned int nquads)
{
#if PPC
	vFloat	x_min = (vFloat)(-1.0), 
		x_max = (vFloat)(1.0f);		

	for(unsigned int i=0; i<(nquads<<4); i+=16)
	{
		vFloat a = vec_ld(i,x);
		vFloat b = vec_ld(i+16,x);
		vFloat c = vec_ld(i+32,x);
		vFloat d = vec_ld(i+48,x);
		a = vec_max(vec_min(a,x_max),x_min);
		b = vec_max(vec_min(b,x_max),x_min);
		c = vec_max(vec_min(c,x_max),x_min);
		d = vec_max(vec_min(d,x_max),x_min);
		vec_st(a,i,x);
		vec_st(b,i+16,x);
		vec_st(c,i+32,x);
		vec_st(d,i+48,x);
	}		
#else
	const __m128 x_min = _mm_set1_ps(-1.0f);
	const __m128 x_max = _mm_set1_ps(1.0f);
	for(unsigned int i=0; i<(nquads<<2); i+=8)
	{		
		_mm_store_ps(x+i,	_mm_max_ps(_mm_min_ps(_mm_load_ps(x + i),x_max),x_min));
		_mm_store_ps(x+i+4, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i + 4),x_max),x_min));		
	}
#endif
}

void hardclip_block8(float *x, unsigned int nquads)
{
#if PPC
	vFloat	x_min = (vFloat)(-8.0), 
		x_max = (vFloat)(8.0f);	

	for(unsigned int i=0; i<(nquads<<4); i+=64)
	{
		vFloat a = vec_ld(i,x);
		vFloat b = vec_ld(i+16,x);
		vFloat c = vec_ld(i+32,x);
		vFloat d = vec_ld(i+48,x);
		a = vec_max(vec_min(a,x_max),x_min);
		b = vec_max(vec_min(b,x_max),x_min);
		c = vec_max(vec_min(c,x_max),x_min);
		d = vec_max(vec_min(d,x_max),x_min);
		vec_st(a,i,x);
		vec_st(b,i+16,x);
		vec_st(c,i+32,x);
		vec_st(d,i+48,x);
	}	
#else
	const __m128 x_min = _mm_set1_ps(-8.0f);
	const __m128 x_max = _mm_set1_ps(8.0f);
	for(unsigned int i=0; i<(nquads<<2); i+=8)
	{
		_mm_store_ps(x+i,	_mm_max_ps(_mm_min_ps(_mm_load_ps(x + i),x_max),x_min));
		_mm_store_ps(x+i+4, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i + 4),x_max),x_min));		
	}
#endif
}

void softclip_block(float *in, unsigned int nquads)
{
#if PPC
	vFloat ca = (vFloat)(-4.f/27.f);
	vFloat x_min = (vFloat)(-1.5f);
	vFloat x_max = (vFloat)(1.5f);
	for(unsigned int i=0; i<(nquads<<4); i+=16)
	{
		vFloat x = vec_ld(i,in);
		x = vec_max(vec_min(x,x_max),x_min);
		vFloat xx = vec_madd(x,x,(vFloat)0);
		vFloat t = vec_madd(x,ca,(vFloat)0);
		t = vec_madd(t,xx,x);
		vec_st(x,i,in);
	}
#else		
	// y = x - (4/27)*x^3,  x Ä [-1.5 .. 1.5]
	const __m128 a = _mm_set1_ps(-4.f/27.f);

	const __m128 x_min = _mm_set1_ps(-1.5f);
	const __m128 x_max = _mm_set1_ps(1.5f);
	
	for(unsigned int i=0; i<nquads; i+=2)
	{
		__m128 x[2],xx[2],t[2];

		x[0] = _mm_min_ps(_mm_load_ps(in + (i<<2)),x_max);
		x[1] = _mm_min_ps(_mm_load_ps(in + ((i+1)<<2)),x_max);
		
		x[0] = _mm_max_ps(x[0],x_min);
		x[1] = _mm_max_ps(x[1],x_min);
		
		xx[0] = _mm_mul_ps(x[0],x[0]);
		xx[1] = _mm_mul_ps(x[1],x[1]);

		t[0] = _mm_mul_ps(x[0],a);
		t[1] = _mm_mul_ps(x[1],a);

		t[0] = _mm_mul_ps(t[0],xx[0]);
		t[1] = _mm_mul_ps(t[1],xx[1]);
		
		t[0] = _mm_add_ps(t[0],x[0]);
		t[1] = _mm_add_ps(t[1],x[1]);
		
		_mm_store_ps(in + (i<<2),t[0]);
		_mm_store_ps(in + ((i+1)<<2),t[1]);
	}
#endif
}

float get_squaremax(float *d, unsigned int nquads)
{
#if PPC		
	vFloat mx = (vFloat)0.f;

	for(unsigned int i=0; i<(nquads<<4); i+=16)
	{
		vFloat x = vec_ld(i,d);
		x = vec_madd(x,x,(vFloat)0.f);
		mx = vec_max(mx,x);
	}
	float fm[4];		
	vec_st(mx,0,fm);

	return std::max(std::max(fm[0],fm[1]),std::max(fm[2],fm[3]));
#else		
	__m128 mx1 = _mm_setzero_ps();
	__m128 mx2 = _mm_setzero_ps();

	for(unsigned int i=0; i<nquads; i+=2)
	{			
		mx1 = _mm_max_ps(mx1,_mm_mul_ps(((__m128*)d)[i],((__m128*)d)[i]));
		mx2 = _mm_max_ps(mx2,_mm_mul_ps(((__m128*)d)[i+1],((__m128*)d)[i+1]));
	}
	mx1 = _mm_max_ps(mx1,mx2);
	mx1 = max_ps_to_ss(mx1);
	float f;
	_mm_store_ss(&f,mx1);
	return f;
#endif
}

float get_absmax(float *d, unsigned int nquads)
{
#if PPC		
	vFloat mx = (vFloat)0.f;

	for(unsigned int i=0; i<(nquads<<4); i+=16)
	{
		vFloat x = vec_abs(vec_ld(i,d));
		mx = vec_max(mx,x);
	}
	float fm[4];		
	vec_st(mx,0,fm);

	return std::max(std::max(fm[0],fm[1]),std::max(fm[2],fm[3]));
#else		
	__m128 mx1 = _mm_setzero_ps();
	__m128 mx2 = _mm_setzero_ps();

	for(unsigned int i=0; i<nquads; i+=2)
	{			
		mx1 = _mm_max_ps(mx1,_mm_and_ps(((__m128*)d)[i],m128_mask_absval));
		mx2 = _mm_max_ps(mx2,_mm_and_ps(((__m128*)d)[i+1],m128_mask_absval));
	}
	mx1 = _mm_max_ps(mx1,mx2);
	mx1 = max_ps_to_ss(mx1);
	float f;
	_mm_store_ss(&f,mx1);
	return f;
#endif
}

float get_absmax_2(float * __restrict d1, float * __restrict d2, unsigned int nquads)
{
#if PPC
	vFloat mx = (vFloat)0.f;

	for(unsigned int i=0; i<(nquads<<4); i+=16)
	{
		vFloat x = vec_abs(vec_ld(i,d1));
		vFloat x2 = vec_abs(vec_ld(i,d2));
		mx = vec_max(mx,vec_max(x,x2));
	}
	float fm[4];		
	vec_st(mx,0,fm);

	return std::max(std::max(fm[0],fm[1]),std::max(fm[2],fm[3]));
#else		
	__m128 mx1 = _mm_setzero_ps();
	__m128 mx2 = _mm_setzero_ps();
	__m128 mx3 = _mm_setzero_ps();
	__m128 mx4 = _mm_setzero_ps();

	for(unsigned int i=0; i<nquads; i+=2)
	{			
		mx1 = _mm_max_ps(mx1,_mm_and_ps(((__m128*)d1)[i],m128_mask_absval));
		mx2 = _mm_max_ps(mx2,_mm_and_ps(((__m128*)d1)[i+1],m128_mask_absval));
		mx3 = _mm_max_ps(mx3,_mm_and_ps(((__m128*)d2)[i],m128_mask_absval));
		mx4 = _mm_max_ps(mx4,_mm_and_ps(((__m128*)d2)[i+1],m128_mask_absval));
	}		
	mx1 = _mm_max_ps(mx1,mx2);
	mx3 = _mm_max_ps(mx3,mx4);
	mx1 = _mm_max_ps(mx1,mx3);
	mx1 = max_ps_to_ss(mx1);
	float f;
	_mm_store_ss(&f,mx1);
	return f;
#endif
}

void tanh7_block(float *xb, unsigned int nquads)
{
#if PPC
	const vFloat a = (vFloat)(-1.f/3.f);
	const vFloat b = (vFloat)(2.f/15.f);
	const vFloat c = (vFloat)(-17.f/315.f);
	const vFloat zero = (vFloat)(0.f);
	const vFloat one = (vFloat)(1.f);
	const vFloat upper_bound = (vFloat)(1.139f);
	const vFloat lower_bound = (vFloat)(-1.139f);

	vFloat t[4],x[4],xx[4];
	for(unsigned int i=0; i<(nquads<<4); i+=64)
	{
		x[0] = vec_ld(i,xb);
		x[1] = vec_ld(i+16,xb);
		x[2] = vec_ld(i+32,xb);
		x[3] = vec_ld(i+48,xb);

		x[0] = vec_max(x[0],lower_bound);	x[1] = vec_max(x[1],lower_bound);	
		x[2] = vec_max(x[2],lower_bound);	x[3] = vec_max(x[3],lower_bound);
		x[0] = vec_min(x[0],upper_bound);	x[1] = vec_min(x[1],upper_bound);	
		x[2] = vec_min(x[2],upper_bound);	x[3] = vec_min(x[3],upper_bound);

		xx[0] = vec_madd(x[0],x[0],zero);
		xx[1] = vec_madd(x[1],x[1],zero);
		xx[2] = vec_madd(x[2],x[2],zero);
		xx[3] = vec_madd(x[3],x[3],zero);

		t[0] = vec_madd(xx[0],c,b);
		t[1] = vec_madd(xx[1],c,b);
		t[2] = vec_madd(xx[2],c,b);
		t[3] = vec_madd(xx[3],c,b);		

		t[0] = vec_madd(t[0],xx[0],a);
		t[1] = vec_madd(t[1],xx[1],a);
		t[2] = vec_madd(t[2],xx[2],a);
		t[3] = vec_madd(t[3],xx[3],a);

		t[0] = vec_madd(t[0],xx[0],one);
		t[1] = vec_madd(t[1],xx[1],one);
		t[2] = vec_madd(t[2],xx[2],one);
		t[3] = vec_madd(t[3],xx[3],one);			

		t[0] = vec_madd(t[0],x[0],zero);
		t[1] = vec_madd(t[1],x[1],zero);
		t[2] = vec_madd(t[2],x[2],zero);
		t[3] = vec_madd(t[3],x[3],zero);

		vec_st(t[0],i,xb);
		vec_st(t[1],i+16,xb);
		vec_st(t[2],i+32,xb);
		vec_st(t[3],i+48,xb);
	}

#else		
	const __m128 a = _mm_set1_ps(-1.f/3.f);
	const __m128 b = _mm_set1_ps(2.f/15.f);
	const __m128 c = _mm_set1_ps(-17.f/315.f);
	const __m128 one = _mm_set1_ps(1.f);
	const __m128 upper_bound = _mm_set1_ps(1.139f);
	const __m128 lower_bound = _mm_set1_ps(-1.139f);

	__m128 t[4],x[4],xx[4];

	for(unsigned int i=0; i<nquads; i+=4)
	{
		x[0] = ((__m128*)xb)[i];
		x[1] = ((__m128*)xb)[i+1];
		x[2] = ((__m128*)xb)[i+2];
		x[3] = ((__m128*)xb)[i+3];

		x[0] = _mm_max_ps(x[0],lower_bound);	x[1] = _mm_max_ps(x[1],lower_bound);	
		x[2] = _mm_max_ps(x[2],lower_bound);	x[3] = _mm_max_ps(x[3],lower_bound);
		x[0] = _mm_min_ps(x[0],upper_bound);	x[1] = _mm_min_ps(x[1],upper_bound);	
		x[2] = _mm_min_ps(x[2],upper_bound);	x[3] = _mm_min_ps(x[3],upper_bound);

		xx[0] = _mm_mul_ps(x[0],x[0]);
		xx[1] = _mm_mul_ps(x[1],x[1]);
		xx[2] = _mm_mul_ps(x[2],x[2]);
		xx[3] = _mm_mul_ps(x[3],x[3]);

		t[0] = _mm_mul_ps(xx[0],c);
		t[1] = _mm_mul_ps(xx[1],c);
		t[2] = _mm_mul_ps(xx[2],c);
		t[3] = _mm_mul_ps(xx[3],c);

		t[0] = _mm_add_ps(t[0],b);
		t[1] = _mm_add_ps(t[1],b);
		t[2] = _mm_add_ps(t[2],b);
		t[3] = _mm_add_ps(t[3],b);

		t[0] = _mm_mul_ps(t[0],xx[0]);
		t[1] = _mm_mul_ps(t[1],xx[1]);
		t[2] = _mm_mul_ps(t[2],xx[2]);
		t[3] = _mm_mul_ps(t[3],xx[3]);

		t[0] = _mm_add_ps(t[0],a);
		t[1] = _mm_add_ps(t[1],a);
		t[2] = _mm_add_ps(t[2],a);
		t[3] = _mm_add_ps(t[3],a);

		t[0] = _mm_mul_ps(t[0],xx[0]);
		t[1] = _mm_mul_ps(t[1],xx[1]);
		t[2] = _mm_mul_ps(t[2],xx[2]);
		t[3] = _mm_mul_ps(t[3],xx[3]);

		t[0] = _mm_add_ps(t[0],one);
		t[1] = _mm_add_ps(t[1],one);
		t[2] = _mm_add_ps(t[2],one);
		t[3] = _mm_add_ps(t[3],one);

		t[0] = _mm_mul_ps(t[0],x[0]);
		t[1] = _mm_mul_ps(t[1],x[1]);
		t[2] = _mm_mul_ps(t[2],x[2]);
		t[3] = _mm_mul_ps(t[3],x[3]);

		((__m128*)xb)[i] = t[0];
		((__m128*)xb)[i+1] = t[1];
		((__m128*)xb)[i+2] = t[2];
		((__m128*)xb)[i+3] = t[3];
	}
#endif
}

void clear_block(float *in, unsigned int nquads)
{		
#if PPC
	vFloat zero = (vFloat) 0.f;		
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{	
		vec_st(zero,i,in);
	}
#else		
	const __m128 zero = _mm_set1_ps(0.f);

	for(unsigned int i=0; i<nquads<<2; i+=4)
	{	
		_mm_store_ps((float*)&in[i],zero);			
	}
#endif
}

void clear_block_antidenormalnoise(float *in, unsigned int nquads)
{	
#if PPC
	// really needed on G4/5 ?
	// should be able to disable denormalisation completely
	vFloat zero = (vFloat) 0.f;		
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{	
		vec_st(zero,i,in);
	}
#else	

	const __m128 smallvalue = _mm_set_ps(
		0.000000000000001f, 0.000000000000001f,
		-0.000000000000001f, -0.000000000000001f);

	for(unsigned int i=0; i<(nquads<<2); i+=8)
	{	
		_mm_store_ps((float*)&in[i],smallvalue);
		_mm_store_ps((float*)&in[i+4],smallvalue);
	}
#endif
}

void accumulate_block(float * __restrict src, float * __restrict dst, unsigned int nquads)		// dst += src
{				
#if PPC
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vec_st(vec_add(vec_ld(i,src), vec_ld(i,dst)),i,dst);
	}		
#else
	for(unsigned int i=0; i<nquads; i+=4)
	{	
		((__m128*)dst)[i] = _mm_add_ps(((__m128*)dst)[i],((__m128*)src)[i]);
		((__m128*)dst)[i+1] = _mm_add_ps(((__m128*)dst)[i+1],((__m128*)src)[i+1]);
		((__m128*)dst)[i+2] = _mm_add_ps(((__m128*)dst)[i+2],((__m128*)src)[i+2]);
		((__m128*)dst)[i+3] = _mm_add_ps(((__m128*)dst)[i+3],((__m128*)src)[i+3]);
	}
#endif
}

void copy_block(float* __restrict src, float* __restrict dst, unsigned int nquads)
{		
#if PPC
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vec_st(vec_ld(i,src),i,dst);
	}	
#else
	float *fdst,*fsrc;
	fdst = (float*)dst;
	fsrc = (float*)src;

	for(unsigned int i=0; i<(nquads<<2); i+=(8<<2))
	{										
		_mm_store_ps(&fdst[i], _mm_load_ps(&fsrc[i]));
		_mm_store_ps(&fdst[i+4], _mm_load_ps(&fsrc[i+4]));
		_mm_store_ps(&fdst[i+8], _mm_load_ps(&fsrc[i+8]));
		_mm_store_ps(&fdst[i+12], _mm_load_ps(&fsrc[i+12]));
		_mm_store_ps(&fdst[i+16], _mm_load_ps(&fsrc[i+16]));
		_mm_store_ps(&fdst[i+20], _mm_load_ps(&fsrc[i+20]));
		_mm_store_ps(&fdst[i+24], _mm_load_ps(&fsrc[i+24]));
		_mm_store_ps(&fdst[i+28], _mm_load_ps(&fsrc[i+28]));
	}
#endif
}

void copy_block_US(float * __restrict src, float * __restrict dst, unsigned int nquads)
{
#if PPC
	memcpy(dst,src,nquads*sizeof(vFloat));
#else
	float *fdst,*fsrc;
	fdst = (float*)dst;
	fsrc = (float*)src;

	for(unsigned int i=0; i<(nquads<<2); i+=(8<<2))
	{										
		_mm_store_ps(&fdst[i], _mm_loadu_ps(&fsrc[i]));
		_mm_store_ps(&fdst[i+4], _mm_loadu_ps(&fsrc[i+4]));
		_mm_store_ps(&fdst[i+8], _mm_loadu_ps(&fsrc[i+8]));
		_mm_store_ps(&fdst[i+12], _mm_loadu_ps(&fsrc[i+12]));
		_mm_store_ps(&fdst[i+16], _mm_loadu_ps(&fsrc[i+16]));
		_mm_store_ps(&fdst[i+20], _mm_loadu_ps(&fsrc[i+20]));
		_mm_store_ps(&fdst[i+24], _mm_loadu_ps(&fsrc[i+24]));
		_mm_store_ps(&fdst[i+28], _mm_loadu_ps(&fsrc[i+28]));
	}
#endif
}

void copy_block_UD(float * __restrict src, float * __restrict dst, unsigned int nquads)
{
#if PPC
	memcpy(dst,src,nquads*sizeof(vFloat));
#else
	float *fdst,*fsrc;
	fdst = (float*)dst;
	fsrc = (float*)src;

	for(unsigned int i=0; i<(nquads<<2); i+=(8<<2))
	{										
		_mm_storeu_ps(&fdst[i], _mm_load_ps(&fsrc[i]));
		_mm_storeu_ps(&fdst[i+4], _mm_load_ps(&fsrc[i+4]));
		_mm_storeu_ps(&fdst[i+8], _mm_load_ps(&fsrc[i+8]));
		_mm_storeu_ps(&fdst[i+12], _mm_load_ps(&fsrc[i+12]));
		_mm_storeu_ps(&fdst[i+16], _mm_load_ps(&fsrc[i+16]));
		_mm_storeu_ps(&fdst[i+20], _mm_load_ps(&fsrc[i+20]));
		_mm_storeu_ps(&fdst[i+24], _mm_load_ps(&fsrc[i+24]));
		_mm_storeu_ps(&fdst[i+28], _mm_load_ps(&fsrc[i+28]));
	}
#endif
}
void copy_block_USUD(float * __restrict src, float * __restrict dst, unsigned int nquads)
{
#if PPC
	memcpy(dst,src,nquads*sizeof(vFloat));
#else
	float *fdst,*fsrc;
	fdst = (float*)dst;
	fsrc = (float*)src;

	for(unsigned int i=0; i<(nquads<<2); i+=(8<<2))
	{										
		_mm_storeu_ps(&fdst[i], _mm_loadu_ps(&fsrc[i]));
		_mm_storeu_ps(&fdst[i+4], _mm_loadu_ps(&fsrc[i+4]));
		_mm_storeu_ps(&fdst[i+8], _mm_loadu_ps(&fsrc[i+8]));
		_mm_storeu_ps(&fdst[i+12], _mm_loadu_ps(&fsrc[i+12]));
		_mm_storeu_ps(&fdst[i+16], _mm_loadu_ps(&fsrc[i+16]));
		_mm_storeu_ps(&fdst[i+20], _mm_loadu_ps(&fsrc[i+20]));
		_mm_storeu_ps(&fdst[i+24], _mm_loadu_ps(&fsrc[i+24]));
		_mm_storeu_ps(&fdst[i+28], _mm_loadu_ps(&fsrc[i+28]));
	}
#endif
}

void sum_ps_to_ss_block(__m128 *xb, unsigned int nquads)
{
#if PPC
	assert(0);
#else
	// finns en snabbare variant pÂ sidan 227 i AMDs optimization manual
	for(unsigned int i=0; i<nquads; i++)
	{	
		__m128 x = xb[i];
		__m128 a = _mm_add_ss(x,_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,1)));
		__m128 b = _mm_add_ss(_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,2)),_mm_shuffle_ps(x,x,_MM_SHUFFLE(0,0,0,3)));
		xb[i] = _mm_add_ss(a,b);
	}
#endif
}

void mul_block(float * __restrict src1, float * __restrict src2, float * __restrict dst, unsigned int nquads)
{
#if PPC
	const vFloat zero = (vFloat)0.f;
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vec_st(vec_madd(vec_ld(i,src1), vec_ld(i,src2),zero),i,dst);
	}
#else
	for(unsigned int i=0; i<nquads; i+=4)
	{	
		((__m128*)dst)[i] = _mm_mul_ps(((__m128*)src1)[i],((__m128*)src2)[i]);
		((__m128*)dst)[i+1] = _mm_mul_ps(((__m128*)src1)[i+1],((__m128*)src2)[i+1]);
		((__m128*)dst)[i+2] = _mm_mul_ps(((__m128*)src1)[i+2],((__m128*)src2)[i+2]);
		((__m128*)dst)[i+3] = _mm_mul_ps(((__m128*)src1)[i+3],((__m128*)src2)[i+3]);
	}
#endif
}

void encodeMS(float * __restrict L, float * __restrict R, float * __restrict M, float * __restrict S, unsigned int nquads)
{
#if PPC
	const vFloat half = (vFloat)0.5f;
	const vFloat zero = (vFloat)0.0f;
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vFloat aL = vec_ld(i,L);
		vFloat aR = vec_ld(i,R);			
		vec_st(vec_madd(vec_add(aL,aR),half,zero),i,M);
		vec_st(vec_madd(vec_sub(aL,aR),half,zero),i,M);
	}
#else
	const __m128 half = _mm_set1_ps(0.5f);
#define L ((__m128*)L)
#define R ((__m128*)R)
#define M ((__m128*)M)
#define S ((__m128*)S)

	for(unsigned int i=0; i<nquads; i+=4)
	{
		M[i] = _mm_mul_ps(_mm_add_ps(L[i],R[i]),half);
		S[i] = _mm_mul_ps(_mm_sub_ps(L[i],R[i]),half);
		M[i+1] = _mm_mul_ps(_mm_add_ps(L[i+1],R[i+1]),half);
		S[i+1] = _mm_mul_ps(_mm_sub_ps(L[i+1],R[i+1]),half);
		M[i+2] = _mm_mul_ps(_mm_add_ps(L[i+2],R[i+2]),half);
		S[i+2] = _mm_mul_ps(_mm_sub_ps(L[i+2],R[i+2]),half);
		M[i+3] = _mm_mul_ps(_mm_add_ps(L[i+3],R[i+3]),half);
		S[i+3] = _mm_mul_ps(_mm_sub_ps(L[i+3],R[i+3]),half);
	}
#undef L
#undef R
#undef M
#undef S
#endif
}
void decodeMS(float * __restrict M, float * __restrict S, float * __restrict L, float * __restrict R, unsigned int nquads)
{
#if PPC
	const vFloat half = (vFloat)0.5f;
	const vFloat zero = (vFloat)0.0f;
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vFloat aL = vec_ld(i,L);
		vFloat aR = vec_ld(i,R);			
		vec_st(vec_add(aL,aR),i,M);
		vec_st(vec_sub(aL,aR),i,M);
	}
#else
#define L ((__m128*)L)
#define R ((__m128*)R)
#define M ((__m128*)M)
#define S ((__m128*)S)
	for(unsigned int i=0; i<nquads; i+=4)
	{
		L[i] = _mm_add_ps(M[i],S[i]);
		R[i] = _mm_sub_ps(M[i],S[i]);
		L[i+1] = _mm_add_ps(M[i+1],S[i+1]);
		R[i+1] = _mm_sub_ps(M[i+1],S[i+1]);
		L[i+2] = _mm_add_ps(M[i+2],S[i+2]);
		R[i+2] = _mm_sub_ps(M[i+2],S[i+2]);
		L[i+3] = _mm_add_ps(M[i+3],S[i+3]);
		R[i+3] = _mm_sub_ps(M[i+3],S[i+3]);
	}
#undef L
#undef R
#undef M
#undef S
#endif
}

void add_block(float * __restrict src1, float * __restrict src2, float * __restrict dst, unsigned int nquads)
{
#if PPC
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vec_st(vec_add(vec_ld(i,src1), vec_ld(i,src2)),i,dst);
	}
#else
	for(unsigned int i=0; i<nquads; i+=4)
	{	
		((__m128*)dst)[i] = _mm_add_ps(((__m128*)src1)[i],((__m128*)src2)[i]);
		((__m128*)dst)[i+1] = _mm_add_ps(((__m128*)src1)[i+1],((__m128*)src2)[i+1]);
		((__m128*)dst)[i+2] = _mm_add_ps(((__m128*)src1)[i+2],((__m128*)src2)[i+2]);
		((__m128*)dst)[i+3] = _mm_add_ps(((__m128*)src1)[i+3],((__m128*)src2)[i+3]);
	}
#endif
}	

void subtract_block(float * __restrict src1, float * __restrict src2, float * __restrict dst, unsigned int nquads)
{
#if PPC
	for(unsigned int i=0; i<(nquads<<4); i+=16)		
	{			
		vec_st(vec_sub(vec_ld(i,src1), vec_ld(i,src2)),i,dst);
	}
#else
	for(unsigned int i=0; i<nquads; i+=4)
	{	
		((__m128*)dst)[i] = _mm_sub_ps(((__m128*)src1)[i],((__m128*)src2)[i]);
		((__m128*)dst)[i+1] = _mm_sub_ps(((__m128*)src1)[i+1],((__m128*)src2)[i+1]);
		((__m128*)dst)[i+2] = _mm_sub_ps(((__m128*)src1)[i+2],((__m128*)src2)[i+2]);
		((__m128*)dst)[i+3] = _mm_sub_ps(((__m128*)src1)[i+3],((__m128*)src2)[i+3]);
	}
#endif
}


#if !PPC
// Snabba sin(x) substitut
// work in progress
__forceinline float sine_float_nowrap(float x)
{
	// http://www.devmaster.net/forums/showthread.php?t=5784
	const float B = 4.f/M_PI;
	const float C = -4.f/(M_PI*M_PI);

	float y = B * x + C * x * ::abs(x);

	//EXTRA_PRECISION
	//  const float Q = 0.775;
	const float P = 0.225;

	return P * (y * fabs(y) - y) + y;   // Q * y + P * y * abs(y)    	 
}

__forceinline __m128 sine_ps_nowrap(__m128 x)
{
	const __m128 B = _mm_set1_ps(4.f/M_PI);
	const __m128 C = _mm_set1_ps(-4.f/(M_PI*M_PI));

	// todo wrap x [0..1] ?

	// y = B * x + C * x * abs(x);	
	__m128 y = _mm_add_ps(_mm_mul_ps(B,x),_mm_mul_ps(_mm_mul_ps(C,x), _mm_and_ps(m128_mask_absval,x)));

	//EXTRA_PRECISION
	//  const float Q = 0.775;
	const __m128 P = _mm_set1_ps(0.225f);
	return _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_and_ps(m128_mask_absval,y),y),y),P),y);	
}

__forceinline __m128 sine_xpi_ps_SSE2(__m128 x)	// sin(x*pi)
{
	const __m128 B = _mm_set1_ps(4.f);
	const __m128 premul = _mm_set1_ps(16777216.f);
	const __m128 postmul = _mm_set1_ps(1.f/16777216.f);
	const __m128i mask = _mm_set1_epi32(0x01ffffff);
	const __m128i offset = _mm_set1_epi32(0x01000000);
	
	// wrap x
	x = _mm_cvtepi32_ps(_mm_sub_epi32(_mm_and_si128(_mm_add_epi32(offset,_mm_cvttps_epi32(x)),mask), offset));
	
	__m128 y = _mm_mul_ps(B, _mm_sub_ps(x,_mm_mul_ps(x, _mm_and_ps(m128_mask_absval,x))));

	const __m128 P = _mm_set1_ps(0.225f);
	return _mm_add_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_and_ps(m128_mask_absval,y),y),y),P),y);	
}

float sine_ss(unsigned int x)	// using 24-bit range as [0..2PI] input
{
	return 0.f;
	/*x = x & 0x00FFFFFF;	// wrap

	float fx;
	_mm_cvtsi32_ss

	const float B = 4.f/M_PI;
	const float C = -4.f/(M_PI*M_PI);

	float y = B * x + C * x * abs(x);

	//EXTRA_PRECISION
	//  const float Q = 0.775;
	const float P = 0.225;

	return P * (y * abs(y) - y) + y;   // Q * y + P * y * abs(y)   */
}
#if !_M_X64
__m64 sine(__m64 x)
{			
	__m64 xabs = _mm_xor_si64(x,_mm_srai_pi16(x,15));
	__m64 y = _mm_subs_pi16(_mm_srai_pi16(x,1), _mm_mulhi_pi16(x,xabs));	
	y = _mm_slli_pi16(y,2);
	y = _mm_adds_pi16(y,y);
	const __m64 Q = _mm_set1_pi16(0x6333);
	const __m64 P = _mm_set1_pi16(0x1CCD);
	
	__m64 yabs = _mm_xor_si64(y,_mm_srai_pi16(y,15));
	__m64 y1 = _mm_mulhi_pi16(Q,y);
	__m64 y2 = _mm_mulhi_pi16(P,_mm_slli_pi16(_mm_mulhi_pi16(y,yabs),1));

	y = _mm_add_pi16(y1,y2);
	return _mm_adds_pi16(y,y);
}
#endif

int sine(int x)		// 16-bit sine
{	 		
	x = ((x + 0x8000) & 0xffff) - 0x8000;
	int y = ((x<<2) - ((abs(x>>1) * (x>>1))>>11));
	const int Q = (0.775f * 65536.f);
	const int P = (0.225f * 32768.f);
	y = ((Q*y) >> 16) + 
		(((((y>>2)*abs(y>>2))>>11)*P) >> 15);
	return y;
}
#endif
