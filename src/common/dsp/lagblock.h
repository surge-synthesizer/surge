//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PPC
#include <vt_dsp/macspecific.h>
#else
#include <xmmintrin.h>
#endif
#include "globals.h"

#pragma pack(push,16)

const float lagrate = 0.004f;

#if PPC
const vFloat c1 = (vFloat)(-1.f - lagrate);
const vFloat c2 = (vFloat)(lagrate);
#else
const __m128 c1 = _mm_set1_ps(1.f-lagrate);
const __m128 c2 = _mm_set1_ps(lagrate);
#endif

template<int n> class lagblock	
{	
public:
	_MM_ALIGN16  float v[n<<2],v_target[n<<2];

	lagblock(){ 
		
	}
	~lagblock()
	{
	}
	inline void interpolate()
	{		
		/*for(int i=0; i<(n<<2); i++)
		{							
			v[i] = (1-lagrate)*v[i] + lagrate*v_target[i];
		}*/
#if PPC
		const vFloat zero = (vFloat) 0.f;
		for(int i=0; i<n; i++)
		{	
			vFloat p1 = vec_madd(vec_ld(i<<4,v),c1,zero);
			vFloat p2 = vec_madd(vec_ld(i<<4,v_target),c2,p1);
			vec_st(p2,i<<4,v);
		}
#else
		for(int i=0; i<n; i++)
		{										 
			__m128 p1 = _mm_mul_ps(_mm_load_ps(&v[i<<2]),c1);						
			__m128 p2 = _mm_mul_ps(_mm_load_ps(&v_target[i<<2]),c2);			
			_mm_store_ps(&v[i<<2],_mm_add_ps(p1,p2));
		}
#endif
	}
	//sse-interpolation honk
	template<bool first> inline void set_target(unsigned int i, float val)
	{
		v_target[i] = val;
		if (first) v[i] = val;
	}
	
	inline float& operator[](unsigned i)
	{
		return v[i];
	}
};

template<int n> class lipolblock_os	
{	
public:
	_MM_ALIGN16  float v[n<<2],v_target[n<<2],dv[n<<2];

	lipolblock_os(){ 
		
	}
	~lipolblock_os()
	{
	}
	__forceinline void interpolate()
	{		
		/*for(int i=0; i<(n<<2); i++)
		{							
			v[i] = v[i] + dv[i];
		}*/
#if PPC
		for(int i=0; i<n; i++)
		{	
			vec_st(vec_add(vec_ld(i<<4,v),vec_ld(i<<4,dv)),i<<4,v);
		}
#else
		for(int i=0; i<n; i++)
		{										 			
			_mm_store_ps(&v[i<<2],_mm_add_ps(_mm_load_ps(&v[i<<2]),_mm_load_ps(&dv[i<<2])));
		}
#endif
	}	
	template<bool first> inline void set_target(unsigned int i, float val)
	{
		dv[i] = (val - v[i]) * (block_size_os_inv);
		v_target[i] = val;
		if (first)
		{
			v[i] = val;			
			dv[i] = 0.f;
		}
	}
	
	__forceinline float& operator[](unsigned i)
	{
		return v[i];
	}
};


#pragma pack(pop)