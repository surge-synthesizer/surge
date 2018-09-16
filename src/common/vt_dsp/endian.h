#pragma once

#define vt_read_int32LE vt_write_int32LE
#define vt_read_int32BE vt_write_int32BE
#define vt_read_int16LE vt_write_int16LE
#define vt_read_int16BE vt_write_int16BE
#define vt_read_float32LE vt_write_float32LE

__forceinline int swap_endian(int t)
{
#if (MAC || _M_X64)
	return ((t<<24)&0xff000000) | ((t<<8)&0x00ff0000) | ((t>>8)&0x0000ff00) | ((t>>24)&0x000000ff);
#else
	// gcc verkar inte respektera att eax måste stämma när den returneras.. lägg till: ret eax?
	__asm
	{
		mov		eax, t
		bswap	eax
	}	
#endif
}

__forceinline int vt_write_int32LE(int t)
{
#if PPC
	return swap_endian(t);
#else
	return t;
#endif
}

__forceinline float vt_write_float32LE(float f)
{	
#if PPC
	int t = *((int*)&f);
	t = swap_endian(t);
	return *((float*)&t);
#else
	return f;
#endif
}

__forceinline int vt_write_int32BE(int t)
{
#if !PPC
	return swap_endian(t);
#else
	return t;
#endif
}

__forceinline short vt_write_int16LE(short t)
{
#if PPC
	return ((t<<8)&0xff00) | ((t>>8)&0x00ff);
#else
	return t;
#endif
}

__forceinline short vt_write_int16BE(short t)
{
#if !PPC
	return ((t<<8)&0xff00) | ((t>>8)&0x00ff);
#else
	return t;
#endif
}

__forceinline void vt_copyblock_W_LE(short *dst, const short *src, size_t count)
{
#if PPC
	for(int i=0; i<count; i++)
	{
		dst[i] = vt_write_int16LE(src[i]);
	}
#else	
	memcpy(dst,src,count*sizeof(short));
#endif
}

__forceinline void vt_copyblock_DW_LE(int *dst, const int *src, size_t count)
{	
#if PPC
	for(int i=0; i<count; i++)
	{
		dst[i] = vt_write_int32LE(src[i]);
	}
#else	
	memcpy(dst,src,count*sizeof(int));
#endif
}