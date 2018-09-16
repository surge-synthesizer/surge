#if PPC

#define vFloat vector float

#define vAlign _MM_ALIGN16

#define vZero ((vector float)(0.f))

#define vAdd vec_add
#define vSub vec_sub
#define vMul(a,b) vec_madd(a,b,vZero)
#define vMAdd vec_madd
#define vNMSub vec_nmsub

#define vAnd vec_and
#define vOr vec_or

#define vCmpGE vec_cmpge

#define vMax vec_max
#define vMin vec_min

forceinline vFloat vLoad1(float Value)
{
	vAlign float f = Value;
	return vec_splat(vec_lde(0, &f), 0);
}

forceinline vFloat vLoad(float* f)
{
	return vec_ld(0, f);
}

forceinline vFloat vSqrtFast(vFloat v)
{
	return vec_re(vec_rsqrte(v));
}

forceinline float vSum(vFloat x)
{	
  	vAlign float f[4];
	vec_st(x,0,&f);
	return (f[0]+f[1])+(f[2]+f[3]);
}

#else

#define vFloat __m128

#define vZero _mm_setzero_ps()

#define vAdd _mm_add_ps
#define vSub _mm_sub_ps
#define vMul _mm_mul_ps
#define vMAdd(a,b,c) _mm_add_ps(_mm_mul_ps(a,b),c)
#define vNMSub(a,b,c) _mm_sub_ps(c,_mm_mul_ps(a,b))

#define vAnd _mm_and_ps
#define vOr _mm_or_ps

#define vCmpGE _mm_cmpge_ps

#define vMax _mm_max_ps
#define vMin _mm_min_ps

#define vLoad _mm_load_ps

forceinline vFloat vLoad1(float f)
{
	return _mm_load1_ps(&f);
}

forceinline vFloat vSqrtFast(vFloat v)
{
	return _mm_rcp_ps(_mm_rsqrt_ps(v));
}

forceinline float vSum(vFloat x)
{	
	__m128 a = _mm_add_ps(x,_mm_movehl_ps(x,x));
	a = _mm_add_ss(a,_mm_shuffle_ps(a,a,_MM_SHUFFLE(0,0,0,1)));
	float f;
	_mm_store_ss(&f, a);

	return f;
}

#define vAlign _MM_ALIGN16

#endif
