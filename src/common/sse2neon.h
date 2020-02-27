#ifndef SSE2NEON_H
#define SSE2NEON_H

// This header file provides a simple API translation layer
// between SSE intrinsics to their corresponding ARM NEON versions
//
// This header file does not yet translate all of the SSE intrinsics.
//
// Contributors to this work are:
//   John W. Ratcliff     : jratcliffscarab@gmail.com
//   Brandon Rowlett      : browlett@nvidia.com
//   Ken Fast             : kfast@gdeb.com
//   Eric van Beurden     : evanbeurden@nvidia.com
//   Alexander Potylitsin : apotylitsin@nvidia.com
//   Hasindu Gamaarachchi : hasindu2008@gmail.com
//   Jim Huang            : jserv@biilabs.io
//
// A struct is now defined in this header file called 'SIMDVec' which can be
// used by applications which attempt to access the contents of an _m128 struct
// directly.  It is important to note that accessing the __m128 struct directly
// is bad coding practice by Microsoft: @see:
// https://msdn.microsoft.com/en-us/library/ayeb3ayc.aspx
//
// However, some legacy source code may try to access the contents of an __m128
// struct directly so the developer can use the SIMDVec as an alias for it.  Any
// casting must be done manually by the developer, as you cannot cast or
// otherwise alias the base NEON data type for intrinsic operations.
//
// A bug was found with the _mm_shuffle_ps intrinsic.  If the shuffle
// permutation was not one of the ones with a custom/unique implementation
// causing it to fall through to the default shuffle implementation it was
// failing to return the correct value.  This is now fixed.
//
// A bug was found with the _mm_cvtps_epi32 intrinsic.  This converts floating
// point values to integers. It was not honoring the correct rounding mode.  In
// SSE the default rounding mode when converting from float to int is to use
// 'round to even' otherwise known as 'bankers rounding'.  ARMv7 did not support
// this feature but ARMv8 does. As it stands today, this header file assumes
// ARMv8.  If you are trying to target really old ARM devices, you may get a
// build error.

/*
 * The MIT license:
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if defined(__GNUC__) || defined(__clang__)

#pragma push_macro("FORCE_INLINE")
#pragma push_macro("ALIGN_STRUCT")
#define FORCE_INLINE static inline __attribute__((always_inline))
#define ALIGN_STRUCT(x) __attribute__((aligned(x)))

#else

#error "Macro name collisions may happens with unknown compiler"
#ifdef FORCE_INLINE
#undef FORCE_INLINE
#endif
#define FORCE_INLINE static inline
#ifndef ALIGN_STRUCT
#define ALIGN_STRUCT(x) __declspec(align(x))
#endif

#endif

#include <stdint.h>
#include "arm_neon.h"

/**
 * MACRO for shuffle parameter for _mm_shuffle_ps().
 * Argument fp3 is a digit[0123] that represents the fp from argument "b"
 * of mm_shuffle_ps that will be placed in fp3 of result. fp2 is the same
 * for fp2 in result. fp1 is a digit[0123] that represents the fp from
 * argument "a" of mm_shuffle_ps that will be places in fp1 of result.
 * fp0 is the same for fp0 of result.
 */
#define _MM_SHUFFLE(fp3, fp2, fp1, fp0) \
    (((fp3) << 6) | ((fp2) << 4) | ((fp1) << 2) | ((fp0)))

/* indicate immediate constant argument in a given range */
#define __constrange(a, b) const

typedef float32x2_t __m64;
typedef float32x4_t __m128;
typedef int32x4_t __m128i;

// ******************************************
// type-safe casting between types
// ******************************************

#define vreinterpretq_m128_f16(x) vreinterpretq_f32_f16(x)
#define vreinterpretq_m128_f32(x) (x)
#define vreinterpretq_m128_f64(x) vreinterpretq_f32_f64(x)

#define vreinterpretq_m128_u8(x) vreinterpretq_f32_u8(x)
#define vreinterpretq_m128_u16(x) vreinterpretq_f32_u16(x)
#define vreinterpretq_m128_u32(x) vreinterpretq_f32_u32(x)
#define vreinterpretq_m128_u64(x) vreinterpretq_f32_u64(x)

#define vreinterpretq_m128_s8(x) vreinterpretq_f32_s8(x)
#define vreinterpretq_m128_s16(x) vreinterpretq_f32_s16(x)
#define vreinterpretq_m128_s32(x) vreinterpretq_f32_s32(x)
#define vreinterpretq_m128_s64(x) vreinterpretq_f32_s64(x)

#define vreinterpretq_f16_m128(x) vreinterpretq_f16_f32(x)
#define vreinterpretq_f32_m128(x) (x)
#define vreinterpretq_f64_m128(x) vreinterpretq_f64_f32(x)

#define vreinterpretq_u8_m128(x) vreinterpretq_u8_f32(x)
#define vreinterpretq_u16_m128(x) vreinterpretq_u16_f32(x)
#define vreinterpretq_u32_m128(x) vreinterpretq_u32_f32(x)
#define vreinterpretq_u64_m128(x) vreinterpretq_u64_f32(x)

#define vreinterpretq_s8_m128(x) vreinterpretq_s8_f32(x)
#define vreinterpretq_s16_m128(x) vreinterpretq_s16_f32(x)
#define vreinterpretq_s32_m128(x) vreinterpretq_s32_f32(x)
#define vreinterpretq_s64_m128(x) vreinterpretq_s64_f32(x)

#define vreinterpretq_m128i_s8(x) vreinterpretq_s32_s8(x)
#define vreinterpretq_m128i_s16(x) vreinterpretq_s32_s16(x)
#define vreinterpretq_m128i_s32(x) (x)
#define vreinterpretq_m128i_s64(x) vreinterpretq_s32_s64(x)

#define vreinterpretq_m128i_u8(x) vreinterpretq_s32_u8(x)
#define vreinterpretq_m128i_u16(x) vreinterpretq_s32_u16(x)
#define vreinterpretq_m128i_u32(x) vreinterpretq_s32_u32(x)
#define vreinterpretq_m128i_u64(x) vreinterpretq_s32_u64(x)

#define vreinterpretq_s8_m128i(x) vreinterpretq_s8_s32(x)
#define vreinterpretq_s16_m128i(x) vreinterpretq_s16_s32(x)
#define vreinterpretq_s32_m128i(x) (x)
#define vreinterpretq_s64_m128i(x) vreinterpretq_s64_s32(x)

#define vreinterpretq_u8_m128i(x) vreinterpretq_u8_s32(x)
#define vreinterpretq_u16_m128i(x) vreinterpretq_u16_s32(x)
#define vreinterpretq_u32_m128i(x) vreinterpretq_u32_s32(x)
#define vreinterpretq_u64_m128i(x) vreinterpretq_u64_s32(x)

// union intended to allow direct access to an __m128 variable using the names
// that the MSVC compiler provides.  This union should really only be used when
// trying to access the members of the vector as integer values.  GCC/clang
// allow native access to the float members through a simple array access
// operator (in C since 4.6, in C++ since 4.8).
//
// Ideally direct accesses to SIMD vectors should not be used since it can cause
// a performance hit.  If it really is needed however, the original __m128
// variable can be aliased with a pointer to this union and used to access
// individual components.  The use of this union should be hidden behind a macro
// that is used throughout the codebase to access the members instead of always
// declaring this type of variable.
typedef union ALIGN_STRUCT(16) SIMDVec {
    float
        m128_f32[4];  // as floats - do not to use this.  Added for convenience.
    int8_t m128_i8[16];    // as signed 8-bit integers.
    int16_t m128_i16[8];   // as signed 16-bit integers.
    int32_t m128_i32[4];   // as signed 32-bit integers.
    int64_t m128_i64[2];   // as signed 64-bit integers.
    uint8_t m128_u8[16];   // as unsigned 8-bit integers.
    uint16_t m128_u16[8];  // as unsigned 16-bit integers.
    uint32_t m128_u32[4];  // as unsigned 32-bit integers.
    uint64_t m128_u64[2];  // as unsigned 64-bit integers.
} SIMDVec;

// ******************************************
// Backwards compatibility for compilers with lack of specific type support
// ******************************************

// Older gcc does not define vld1q_u8_x4 type
#if defined(__GNUC__) && !defined(__clang__)
#if __GNUC__ < 9 || (__GNUC__ == 9 && (__GNUC_MINOR__ <= 2))
FORCE_INLINE uint8x16x4_t vld1q_u8_x4(const uint8_t *p)
{
    uint8x16x4_t ret;
    ret.val[0] = vld1q_u8(p + 0);
    ret.val[1] = vld1q_u8(p + 16);
    ret.val[2] = vld1q_u8(p + 32);
    ret.val[3] = vld1q_u8(p + 48);
    return ret;
}
#endif
#endif


// ******************************************
// Set/get methods
// ******************************************

// Loads one cache line of data from address p to a location closer to the
// processor. https://msdn.microsoft.com/en-us/library/84szxsww(v=vs.100).aspx
FORCE_INLINE void _mm_prefetch(const void *p, int i)
{
    __builtin_prefetch(p);
}

// extracts the lower order floating point value from the parameter :
// https://msdn.microsoft.com/en-us/library/bb514059%28v=vs.120%29.aspx?f=255&MSPPError=-2147217396
FORCE_INLINE float _mm_cvtss_f32(__m128 a)
{
    return vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
}

// Sets the 128-bit value to zero
// https://msdn.microsoft.com/en-us/library/vstudio/ys7dw0kh(v=vs.100).aspx
FORCE_INLINE __m128i _mm_setzero_si128(void)
{
    return vreinterpretq_m128i_s32(vdupq_n_s32(0));
}

// Clears the four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/tk1t2tbz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_setzero_ps(void)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(0));
}

// Sets the four single-precision, floating-point values to w.
//
//   r0 := r1 := r2 := r3 := w
//
// https://msdn.microsoft.com/en-us/library/vstudio/2x1se8ha(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set1_ps(float _w)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(_w));
}

// Sets the four single-precision, floating-point values to w.
// https://msdn.microsoft.com/en-us/library/vstudio/2x1se8ha(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set_ps1(float _w)
{
    return vreinterpretq_m128_f32(vdupq_n_f32(_w));
}

// Sets the four single-precision, floating-point values to the four inputs.
// https://msdn.microsoft.com/en-us/library/vstudio/afh0zf75(v=vs.100).aspx
FORCE_INLINE __m128 _mm_set_ps(float w, float z, float y, float x)
{
    float __attribute__((aligned(16))) data[4] = {x, y, z, w};
    return vreinterpretq_m128_f32(vld1q_f32(data));
}

// Sets the four single-precision, floating-point values to the four inputs in
// reverse order.
// https://msdn.microsoft.com/en-us/library/vstudio/d2172ct3(v=vs.100).aspx
FORCE_INLINE __m128 _mm_setr_ps(float w, float z, float y, float x)
{
    float __attribute__((aligned(16))) data[4] = {w, z, y, x};
    return vreinterpretq_m128_f32(vld1q_f32(data));
}

// Sets the 8 signed 16-bit integer values in reverse order.
//
// Return Value
//   r0 := w0
//   r1 := w1
//   ...
//   r7 := w7
FORCE_INLINE __m128i _mm_setr_epi16(short w0,
                                    short w1,
                                    short w2,
                                    short w3,
                                    short w4,
                                    short w5,
                                    short w6,
                                    short w7)
{
    int16_t __attribute__((aligned(16)))
    data[8] = {w0, w1, w2, w3, w4, w5, w6, w7};
    return vreinterpretq_m128i_s16(vld1q_s16((int16_t *) data));
}

// Sets the 4 signed 32-bit integer values in reverse order
// https://technet.microsoft.com/en-us/library/security/27yb3ee5(v=vs.90).aspx
FORCE_INLINE __m128i _mm_setr_epi32(int i3, int i2, int i1, int i0)
{
    int32_t __attribute__((aligned(16))) data[4] = {i3, i2, i1, i0};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Sets the 16 signed 8-bit integer values to b.
//
//   r0 := b
//   r1 := b
//   ...
//   r15 := b
//
// https://msdn.microsoft.com/en-us/library/6e14xhyf(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set1_epi8(char w)
{
    return vreinterpretq_m128i_s8(vdupq_n_s8(w));
}

// Sets the 8 signed 16-bit integer values to w.
//
//   r0 := w
//   r1 := w
//   ...
//   r7 := w
//
// https://msdn.microsoft.com/en-us/library/k0ya3x0e(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set1_epi16(short w)
{
    return vreinterpretq_m128i_s16(vdupq_n_s16(w));
}

// Sets the 16 signed 8-bit integer values.
// https://msdn.microsoft.com/en-us/library/x0cx8zd3(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi8(char b15,
                                  char b14,
                                  char b13,
                                  char b12,
                                  char b11,
                                  char b10,
                                  char b9,
                                  char b8,
                                  char b7,
                                  char b6,
                                  char b5,
                                  char b4,
                                  char b3,
                                  char b2,
                                  char b1,
                                  char b0)
{
    int8_t __attribute__((aligned(16)))
    data[16] = {(int8_t) b0,  (int8_t) b1,  (int8_t) b2,  (int8_t) b3,
                (int8_t) b4,  (int8_t) b5,  (int8_t) b6,  (int8_t) b7,
                (int8_t) b8,  (int8_t) b9,  (int8_t) b10, (int8_t) b11,
                (int8_t) b12, (int8_t) b13, (int8_t) b14, (int8_t) b15};
    return (__m128i) vld1q_s8(data);
}

// Sets the 8 signed 16-bit integer values.
// https://msdn.microsoft.com/en-au/library/3e0fek84(v=vs.90).aspx
FORCE_INLINE __m128i _mm_set_epi16(short i7,
                                   short i6,
                                   short i5,
                                   short i4,
                                   short i3,
                                   short i2,
                                   short i1,
                                   short i0)
{
    int16_t __attribute__((aligned(16)))
    data[8] = {i0, i1, i2, i3, i4, i5, i6, i7};
    return vreinterpretq_m128i_s16(vld1q_s16(data));
}

// Sets the 16 signed 8-bit integer values in reverse order.
// https://msdn.microsoft.com/en-us/library/2khb9c7k(v=vs.90).aspx
FORCE_INLINE __m128i _mm_setr_epi8(char b0,
                                   char b1,
                                   char b2,
                                   char b3,
                                   char b4,
                                   char b5,
                                   char b6,
                                   char b7,
                                   char b8,
                                   char b9,
                                   char b10,
                                   char b11,
                                   char b12,
                                   char b13,
                                   char b14,
                                   char b15)
{
    int8_t __attribute__((aligned(16)))
    data[16] = {(int8_t) b0,  (int8_t) b1,  (int8_t) b2,  (int8_t) b3,
                (int8_t) b4,  (int8_t) b5,  (int8_t) b6,  (int8_t) b7,
                (int8_t) b8,  (int8_t) b9,  (int8_t) b10, (int8_t) b11,
                (int8_t) b12, (int8_t) b13, (int8_t) b14, (int8_t) b15};
    return (__m128i) vld1q_s8(data);
}

// Sets the 4 signed 32-bit integer values to i.
//
//   r0 := i
//   r1 := i
//   r2 := i
//   r3 := I
//
// https://msdn.microsoft.com/en-us/library/vstudio/h4xscxat(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set1_epi32(int _i)
{
    return vreinterpretq_m128i_s32(vdupq_n_s32(_i));
}

// Sets the 4 signed 64-bit integer values to i.
// https://msdn.microsoft.com/en-us/library/vstudio/h4xscxat(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set1_epi64(int64_t _i)
{
    return vreinterpretq_m128i_s64(vdupq_n_s64(_i));
}

// Sets the 4 signed 32-bit integer values.
// https://msdn.microsoft.com/en-us/library/vstudio/019beekt(v=vs.100).aspx
FORCE_INLINE __m128i _mm_set_epi32(int i3, int i2, int i1, int i0)
{
    int32_t __attribute__((aligned(16))) data[4] = {i0, i1, i2, i3};
    return vreinterpretq_m128i_s32(vld1q_s32(data));
}

// Returns the __m128i structure with its two 64-bit integer values
// initialized to the values of the two 64-bit integers passed in.
// https://msdn.microsoft.com/en-us/library/dk2sdw0h(v=vs.120).aspx
FORCE_INLINE __m128i _mm_set_epi64x(int64_t i1, int64_t i2)
{
    int64_t __attribute__((aligned(16))) data[2] = {i2, i1};
    return vreinterpretq_m128i_s64(vld1q_s64(data));
}

// Stores four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/s3h4ay6y(v=vs.100).aspx
FORCE_INLINE void _mm_store_ps(float *p, __m128 a)
{
    vst1q_f32(p, vreinterpretq_f32_m128(a));
}

// Stores four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/44e30x22(v=vs.100).aspx
FORCE_INLINE void _mm_storeu_ps(float *p, __m128 a)
{
    vst1q_f32(p, vreinterpretq_f32_m128(a));
}

// Stores four 32-bit integer values as (as a __m128i value) at the address p.
// https://msdn.microsoft.com/en-us/library/vstudio/edk11s13(v=vs.100).aspx
FORCE_INLINE void _mm_store_si128(__m128i *p, __m128i a)
{
    vst1q_s32((int32_t *) p, vreinterpretq_s32_m128i(a));
}

// Stores four 32-bit integer values as (as a __m128i value) at the address p.
// https://msdn.microsoft.com/en-us/library/vstudio/edk11s13(v=vs.100).aspx
FORCE_INLINE void _mm_storeu_si128(__m128i *p, __m128i a)
{
    vst1q_s32((int32_t *) p, vreinterpretq_s32_m128i(a));
}

// Stores the lower single - precision, floating - point value.
// https://msdn.microsoft.com/en-us/library/tzz10fbx(v=vs.100).aspx
FORCE_INLINE void _mm_store_ss(float *p, __m128 a)
{
    vst1q_lane_f32(p, vreinterpretq_f32_m128(a), 0);
}

// Reads the lower 64 bits of b and stores them into the lower 64 bits of a.
// https://msdn.microsoft.com/en-us/library/hhwf428f%28v=vs.90%29.aspx
FORCE_INLINE void _mm_storel_epi64(__m128i *a, __m128i b)
{
    uint64x1_t hi = vget_high_u64(vreinterpretq_u64_m128i(*a));
    uint64x1_t lo = vget_low_u64(vreinterpretq_u64_m128i(b));
    *a = vreinterpretq_m128i_u64(vcombine_u64(lo, hi));
}

// Stores the lower two single-precision floating point values of a to the
// address p.
//
//   *p0 := b0
//   *p1 := b1
//
// https://msdn.microsoft.com/en-us/library/h54t98ks(v=vs.90).aspx
FORCE_INLINE void _mm_storel_pi(__m64 *p, __m128 a)
{
    *p = vget_low_f32(a);
}

// Loads a single single-precision, floating-point value, copying it into all
// four words
// https://msdn.microsoft.com/en-us/library/vstudio/5cdkf716(v=vs.100).aspx
FORCE_INLINE __m128 _mm_load1_ps(const float *p)
{
    return vreinterpretq_m128_f32(vld1q_dup_f32(p));
}
#define _mm_load_ps1 _mm_load1_ps

// Sets the lower two single-precision, floating-point values with 64
// bits of data loaded from the address p; the upper two values are passed
// through from a.
//
// Return Value
//   r0 := *p0
//   r1 := *p1
//   r2 := a2
//   r3 := a3
//
// https://msdn.microsoft.com/en-us/library/s57cyak2(v=vs.100).aspx
FORCE_INLINE __m128 _mm_loadl_pi(__m128 a, __m64 const *b)
{
    return vreinterpretq_m128_f32(
        vcombine_f32(vld1_f32((const float32_t *) b), vget_high_f32(a)));
}

// Loads four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/zzd50xxt(v=vs.100).aspx
FORCE_INLINE __m128 _mm_load_ps(const float *p)
{
    return vreinterpretq_m128_f32(vld1q_f32(p));
}

// Loads four single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/x1b16s7z%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_loadu_ps(const float *p)
{
    // for neon, alignment doesn't matter, so _mm_load_ps and _mm_loadu_ps are
    // equivalent for neon
    return vreinterpretq_m128_f32(vld1q_f32(p));
}

// Loads an single - precision, floating - point value into the low word and
// clears the upper three words.
// https://msdn.microsoft.com/en-us/library/548bb9h4%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_load_ss(const float *p)
{
    return vreinterpretq_m128_f32(vsetq_lane_f32(*p, vdupq_n_f32(0), 0));
}

FORCE_INLINE __m128i _mm_loadl_epi64(__m128i const *p)
{
    /* Load the lower 64 bits of the value pointed to by p into the
     * lower 64 bits of the result, zeroing the upper 64 bits of the result.
     */
    return vcombine_s32(vld1_s32((int32_t const *) p), vcreate_s32(0));
}

// ******************************************
// Logic/Binary operations
// ******************************************

// Compares for inequality.
// https://msdn.microsoft.com/en-us/library/sf44thbx(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpneq_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(vmvnq_u32(
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b))));
}

// Computes the bitwise AND-NOT of the four single-precision, floating-point
// values of a and b.
//
//   r0 := ~a0 & b0
//   r1 := ~a1 & b1
//   r2 := ~a2 & b2
//   r3 := ~a3 & b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/68h7wd02(v=vs.100).aspx
FORCE_INLINE __m128 _mm_andnot_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vbicq_s32(vreinterpretq_s32_m128(b),
                  vreinterpretq_s32_m128(a)));  // *NOTE* argument swap
}

// Computes the bitwise AND of the 128-bit value in b and the bitwise NOT of the
// 128-bit value in a.
//
//   r := (~a) & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/1beaceh8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_andnot_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vbicq_s32(vreinterpretq_s32_m128i(b),
                  vreinterpretq_s32_m128i(a)));  // *NOTE* argument swap
}

// Computes the bitwise AND of the 128-bit value in a and the 128-bit value in
// b.
//
//   r := a & b
//
// https://msdn.microsoft.com/en-us/library/vstudio/6d1txsa8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_and_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vandq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Computes the bitwise AND of the four single-precision, floating-point values
// of a and b.
//
//   r0 := a0 & b0
//   r1 := a1 & b1
//   r2 := a2 & b2
//   r3 := a3 & b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/73ck1xc5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_and_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vandq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

// Computes the bitwise OR of the four single-precision, floating-point values
// of a and b.
// https://msdn.microsoft.com/en-us/library/vstudio/7ctdsyy0(v=vs.100).aspx
FORCE_INLINE __m128 _mm_or_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        vorrq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

// Computes bitwise EXOR (exclusive-or) of the four single-precision,
// floating-point values of a and b.
// https://msdn.microsoft.com/en-us/library/ss6k3wk8(v=vs.100).aspx
FORCE_INLINE __m128 _mm_xor_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_s32(
        veorq_s32(vreinterpretq_s32_m128(a), vreinterpretq_s32_m128(b)));
}

// Computes the bitwise OR of the 128-bit value in a and the 128-bit value in b.
//
//   r := a | b
//
// https://msdn.microsoft.com/en-us/library/vstudio/ew8ty0db(v=vs.100).aspx
FORCE_INLINE __m128i _mm_or_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vorrq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Computes the bitwise XOR of the 128-bit value in a and the 128-bit value in
// b.  https://msdn.microsoft.com/en-us/library/fzt08www(v=vs.100).aspx
FORCE_INLINE __m128i _mm_xor_si128(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        veorq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Moves the upper two values of B into the lower two values of A.
//
//   r3 := a3
//   r2 := a2
//   r1 := b3
//   r0 := b2
FORCE_INLINE __m128 _mm_movehl_ps(__m128 __A, __m128 __B)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(b32, a32));
}

// Moves the lower two values of B into the upper two values of A.
//
//   r3 := b1
//   r2 := b0
//   r1 := a1
//   r0 := a0
FORCE_INLINE __m128 _mm_movelh_ps(__m128 __A, __m128 __B)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(__A));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(__B));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b10));
}

// NEON does not provide this method
// Creates a 4-bit mask from the most significant bits of the four
// single-precision, floating-point values.
// https://msdn.microsoft.com/en-us/library/vstudio/4490ys29(v=vs.100).aspx
FORCE_INLINE int _mm_movemask_ps(__m128 a)
{
#if 0 /* C version */
    uint32x4_t &ia = *(uint32x4_t *) &a;
    return (ia[0] >> 31) | ((ia[1] >> 30) & 2) | ((ia[2] >> 29) & 4) |
           ((ia[3] >> 28) & 8);
#endif
    static const uint32x4_t movemask = {1, 2, 4, 8};
    static const uint32x4_t highbit = {0x80000000, 0x80000000, 0x80000000,
                                       0x80000000};
    uint32x4_t t0 = vreinterpretq_u32_m128(a);
    uint32x4_t t1 = vtstq_u32(t0, highbit);
    uint32x4_t t2 = vandq_u32(t1, movemask);
    uint32x2_t t3 = vorr_u32(vget_low_u32(t2), vget_high_u32(t2));
    return vget_lane_u32(t3, 0) | vget_lane_u32(t3, 1);
}

FORCE_INLINE __m128i _mm_abs_epi32(__m128i a)
{
    return vqabsq_s32(a);
}

FORCE_INLINE __m128i _mm_abs_epi16(__m128i a)
{
    return vreinterpretq_s32_s16(vqabsq_s16(vreinterpretq_s16_s32(a)));
}

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of b and places it into the high end of the result.
FORCE_INLINE __m128 _mm_shuffle_ps_1032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a32, b10));
}

// takes the lower two 32-bit values from a and swaps them and places in high
// end of result takes the higher two 32 bit values from b and swaps them and
// places in low end of result.
FORCE_INLINE __m128 _mm_shuffle_ps_2301(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b23 = vrev64_f32(vget_high_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b23));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0321(__m128 a, __m128 b)
{
    float32x2_t a21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a21, b03));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2103(__m128 a, __m128 b)
{
    float32x2_t a03 = vget_low_f32(
        vextq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a), 3));
    float32x2_t b21 = vget_high_f32(
        vextq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b), 3));
    return vreinterpretq_m128_f32(vcombine_f32(a03, b21));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1010(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b10));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b10));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0101(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32x2_t b01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vcombine_f32(a01, b01));
}

// keeps the low 64 bits of b in the low and puts the high 64 bits of a in the
// high
FORCE_INLINE __m128 _mm_shuffle_ps_3210(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a10, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0011(__m128 a, __m128 b)
{
    float32x2_t a11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a11, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_0022(__m128 a, __m128 b)
{
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a22, b00));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2200(__m128 a, __m128 b)
{
    float32x2_t a00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t b22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(vcombine_f32(a00, b22));
}

FORCE_INLINE __m128 _mm_shuffle_ps_3202(__m128 a, __m128 b)
{
    float32_t a0 = vgetq_lane_f32(vreinterpretq_f32_m128(a), 0);
    float32x2_t a22 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 0);
    float32x2_t a02 = vset_lane_f32(a0, a22, 1); /* TODO: use vzip ?*/
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(vcombine_f32(a02, b32));
}

FORCE_INLINE __m128 _mm_shuffle_ps_1133(__m128 a, __m128 b)
{
    float32x2_t a33 =
        vdup_lane_f32(vget_high_f32(vreinterpretq_f32_m128(a)), 1);
    float32x2_t b11 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 1);
    return vreinterpretq_m128_f32(vcombine_f32(a33, b11));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2010(__m128 a, __m128 b)
{
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(vreinterpretq_f32_m128(b), 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a10, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2001(__m128 a, __m128 b)
{
    float32x2_t a01 = vrev64_f32(vget_low_f32(vreinterpretq_f32_m128(a)));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a01, b20));
}

FORCE_INLINE __m128 _mm_shuffle_ps_2032(__m128 a, __m128 b)
{
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32_t b2 = vgetq_lane_f32(b, 2);
    float32x2_t b00 = vdup_lane_f32(vget_low_f32(vreinterpretq_f32_m128(b)), 0);
    float32x2_t b20 = vset_lane_f32(b2, b00, 1);
    return vreinterpretq_m128_f32(vcombine_f32(a32, b20));
}

// NEON does not support a general purpose permute intrinsic
// Selects four specific single-precision, floating-point values from a and b,
// based on the mask i.
// https://msdn.microsoft.com/en-us/library/vstudio/5f0858x0(v=vs.100).aspx
#if 0 /* C version */
FORCE_INLINE __m128 _mm_shuffle_ps_default(__m128 a,
                                           __m128 b,
                                           __constrange(0, 255) int imm)
{
    __m128 ret;
    ret[0] = a[imm & 0x3];
    ret[1] = a[(imm >> 2) & 0x3];
    ret[2] = b[(imm >> 4) & 0x03];
    ret[3] = b[(imm >> 6) & 0x03];
    return ret;
}
#endif
#define _mm_shuffle_ps_default(a, b, imm)                                  \
    ({                                                                     \
        float32x4_t ret;                                                   \
        ret = vmovq_n_f32(                                                 \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), (imm) &0x3));        \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 4) & 0x3), \
            ret, 2);                                                       \
        ret = vsetq_lane_f32(                                              \
            vgetq_lane_f32(vreinterpretq_f32_m128(b), ((imm) >> 6) & 0x3), \
            ret, 3);                                                       \
        vreinterpretq_m128_f32(ret);                                       \
    })

// FORCE_INLINE __m128 _mm_shuffle_ps(__m128 a, __m128 b, __constrange(0,255)
// int imm)
#define _mm_shuffle_ps(a, b, imm)                          \
    ({                                                     \
        __m128 ret;                                        \
        switch (imm) {                                     \
        case _MM_SHUFFLE(1, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_1032((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 3, 0, 1):                      \
            ret = _mm_shuffle_ps_2301((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 3, 2, 1):                      \
            ret = _mm_shuffle_ps_0321((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 1, 0, 3):                      \
            ret = _mm_shuffle_ps_2103((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 1, 0):                      \
            ret = _mm_movelh_ps((a), (b));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_1001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 1, 0, 1):                      \
            ret = _mm_shuffle_ps_0101((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 1, 0):                      \
            ret = _mm_shuffle_ps_3210((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 1, 1):                      \
            ret = _mm_shuffle_ps_0011((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(0, 0, 2, 2):                      \
            ret = _mm_shuffle_ps_0022((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 2, 0, 0):                      \
            ret = _mm_shuffle_ps_2200((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 0, 2):                      \
            ret = _mm_shuffle_ps_3202((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(3, 2, 3, 2):                      \
            ret = _mm_movehl_ps((b), (a));                 \
            break;                                         \
        case _MM_SHUFFLE(1, 1, 3, 3):                      \
            ret = _mm_shuffle_ps_1133((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 1, 0):                      \
            ret = _mm_shuffle_ps_2010((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 0, 1):                      \
            ret = _mm_shuffle_ps_2001((a), (b));           \
            break;                                         \
        case _MM_SHUFFLE(2, 0, 3, 2):                      \
            ret = _mm_shuffle_ps_2032((a), (b));           \
            break;                                         \
        default:                                           \
            ret = _mm_shuffle_ps_default((a), (b), (imm)); \
            break;                                         \
        }                                                  \
        ret;                                               \
    })

// Takes the upper 64 bits of a and places it in the low end of the result
// Takes the lower 64 bits of a and places it into the high end of the result.
FORCE_INLINE __m128i _mm_shuffle_epi_1032(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a10));
}

// takes the lower two 32-bit values from a and swaps them and places in low end
// of result takes the higher two 32 bit values from a and swaps them and places
// in high end of result.
FORCE_INLINE __m128i _mm_shuffle_epi_2301(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a23 = vrev64_s32(vget_high_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a23));
}

// rotates the least significant 32 bits into the most signficant 32 bits, and
// shifts the rest down
FORCE_INLINE __m128i _mm_shuffle_epi_0321(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 1));
}

// rotates the most significant 32 bits into the least signficant 32 bits, and
// shifts the rest up
FORCE_INLINE __m128i _mm_shuffle_epi_2103(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vextq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(a), 3));
}

// gets the lower 64 bits of a, and places it in the upper 64 bits
// gets the lower 64 bits of a and places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1010(__m128i a)
{
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a10, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements, and places it in the
// lower 64 bits gets the lower 64 bits of a, and places it in the upper 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_1001(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    int32x2_t a10 = vget_low_s32(vreinterpretq_s32_m128i(a));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a10));
}

// gets the lower 64 bits of a, swaps the 0 and 1 elements and places it in the
// upper 64 bits gets the lower 64 bits of a, swaps the 0 and 1 elements, and
// places it in the lower 64 bits
FORCE_INLINE __m128i _mm_shuffle_epi_0101(__m128i a)
{
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a01, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_2211(__m128i a)
{
    int32x2_t a11 = vdup_lane_s32(vget_low_s32(vreinterpretq_s32_m128i(a)), 1);
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    return vreinterpretq_m128i_s32(vcombine_s32(a11, a22));
}

FORCE_INLINE __m128i _mm_shuffle_epi_0122(__m128i a)
{
    int32x2_t a22 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 0);
    int32x2_t a01 = vrev64_s32(vget_low_s32(vreinterpretq_s32_m128i(a)));
    return vreinterpretq_m128i_s32(vcombine_s32(a22, a01));
}

FORCE_INLINE __m128i _mm_shuffle_epi_3332(__m128i a)
{
    int32x2_t a32 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t a33 = vdup_lane_s32(vget_high_s32(vreinterpretq_s32_m128i(a)), 1);
    return vreinterpretq_m128i_s32(vcombine_s32(a32, a33));
}

// Shuffle packed 8-bit integers in a according to shuffle control mask in the
// corresponding 8-bit element of b, and store the results in dst.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_shuffle_epi8&expand=5146
FORCE_INLINE __m128i _mm_shuffle_epi8(__m128i a, __m128i b)
{
#if __aarch64__
    int8x16_t tbl = vreinterpretq_s8_m128i(a);   // input a
    uint8x16_t idx = vreinterpretq_u8_m128i(b);  // input b
    uint8_t __attribute__((aligned(16)))
    mask[16] = {0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F,
                0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F, 0x8F};
    uint8x16_t idx_masked =
        vandq_u8(idx, vld1q_u8(mask));  // avoid using meaningless bits

    return vreinterpretq_m128i_s8(vqtbl1q_s8(tbl, idx_masked));
#else
    uint8_t *tbl = (uint8_t *) &a;  // input a
    uint8_t *idx = (uint8_t *) &b;  // input b
    int32_t r[4];

    r[0] = ((idx[3] & 0x80) ? 0 : tbl[idx[3] % 16]) << 24;
    r[0] |= ((idx[2] & 0x80) ? 0 : tbl[idx[2] % 16]) << 16;
    r[0] |= ((idx[1] & 0x80) ? 0 : tbl[idx[1] % 16]) << 8;
    r[0] |= ((idx[0] & 0x80) ? 0 : tbl[idx[0] % 16]);

    r[1] = ((idx[7] & 0x80) ? 0 : tbl[idx[7] % 16]) << 24;
    r[1] |= ((idx[6] & 0x80) ? 0 : tbl[idx[6] % 16]) << 16;
    r[1] |= ((idx[5] & 0x80) ? 0 : tbl[idx[5] % 16]) << 8;
    r[1] |= ((idx[4] & 0x80) ? 0 : tbl[idx[4] % 16]);

    r[2] = ((idx[11] & 0x80) ? 0 : tbl[idx[11] % 16]) << 24;
    r[2] |= ((idx[10] & 0x80) ? 0 : tbl[idx[10] % 16]) << 16;
    r[2] |= ((idx[9] & 0x80) ? 0 : tbl[idx[9] % 16]) << 8;
    r[2] |= ((idx[8] & 0x80) ? 0 : tbl[idx[8] % 16]);

    r[3] = ((idx[15] & 0x80) ? 0 : tbl[idx[15] % 16]) << 24;
    r[3] |= ((idx[14] & 0x80) ? 0 : tbl[idx[14] % 16]) << 16;
    r[3] |= ((idx[13] & 0x80) ? 0 : tbl[idx[13] % 16]) << 8;
    r[3] |= ((idx[12] & 0x80) ? 0 : tbl[idx[12] % 16]);

    return vld1q_s32(r);
#endif
}


#if 0 /* C version */
FORCE_INLINE __m128i _mm_shuffle_epi32_default(__m128i a,
                                               __constrange(0, 255) int imm)
{
    __m128i ret;
    ret[0] = a[imm & 0x3];
    ret[1] = a[(imm >> 2) & 0x3];
    ret[2] = a[(imm >> 4) & 0x03];
    ret[3] = a[(imm >> 6) & 0x03];
    return ret;
}
#endif
#define _mm_shuffle_epi32_default(a, imm)                                   \
    ({                                                                      \
        int32x4_t ret;                                                      \
        ret = vmovq_n_s32(                                                  \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm) &0x3));        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 2) & 0x3), \
            ret, 1);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 4) & 0x3), \
            ret, 2);                                                        \
        ret = vsetq_lane_s32(                                               \
            vgetq_lane_s32(vreinterpretq_s32_m128i(a), ((imm) >> 6) & 0x3), \
            ret, 3);                                                        \
        vreinterpretq_m128i_s32(ret);                                       \
    })

// FORCE_INLINE __m128i _mm_shuffle_epi32_splat(__m128i a, __constrange(0,255)
// int imm)
#if defined(__aarch64__)
#define _mm_shuffle_epi32_splat(a, imm)                          \
    ({                                                           \
        vreinterpretq_m128i_s32(                                 \
            vdupq_laneq_s32(vreinterpretq_s32_m128i(a), (imm))); \
    })
#else
#define _mm_shuffle_epi32_splat(a, imm)                                      \
    ({                                                                       \
        vreinterpretq_m128i_s32(                                             \
            vdupq_n_s32(vgetq_lane_s32(vreinterpretq_s32_m128i(a), (imm)))); \
    })
#endif

// Shuffles the 4 signed or unsigned 32-bit integers in a as specified by imm.
// https://msdn.microsoft.com/en-us/library/56f67xbk%28v=vs.90%29.aspx
// FORCE_INLINE __m128i _mm_shuffle_epi32(__m128i a, __constrange(0,255) int
// imm)
#define _mm_shuffle_epi32(a, imm)                        \
    ({                                                   \
        __m128i ret;                                     \
        switch (imm) {                                   \
        case _MM_SHUFFLE(1, 0, 3, 2):                    \
            ret = _mm_shuffle_epi_1032((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 3, 0, 1):                    \
            ret = _mm_shuffle_epi_2301((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 3, 2, 1):                    \
            ret = _mm_shuffle_epi_0321((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 1, 0, 3):                    \
            ret = _mm_shuffle_epi_2103((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 1, 0):                    \
            ret = _mm_shuffle_epi_1010((a));             \
            break;                                       \
        case _MM_SHUFFLE(1, 0, 0, 1):                    \
            ret = _mm_shuffle_epi_1001((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 0, 1):                    \
            ret = _mm_shuffle_epi_0101((a));             \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 1, 1):                    \
            ret = _mm_shuffle_epi_2211((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 1, 2, 2):                    \
            ret = _mm_shuffle_epi_0122((a));             \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 2):                    \
            ret = _mm_shuffle_epi_3332((a));             \
            break;                                       \
        case _MM_SHUFFLE(0, 0, 0, 0):                    \
            ret = _mm_shuffle_epi32_splat((a), 0);       \
            break;                                       \
        case _MM_SHUFFLE(1, 1, 1, 1):                    \
            ret = _mm_shuffle_epi32_splat((a), 1);       \
            break;                                       \
        case _MM_SHUFFLE(2, 2, 2, 2):                    \
            ret = _mm_shuffle_epi32_splat((a), 2);       \
            break;                                       \
        case _MM_SHUFFLE(3, 3, 3, 3):                    \
            ret = _mm_shuffle_epi32_splat((a), 3);       \
            break;                                       \
        default:                                         \
            ret = _mm_shuffle_epi32_default((a), (imm)); \
            break;                                       \
        }                                                \
        ret;                                             \
    })

// Shuffles the upper 4 signed or unsigned 16 - bit integers in a as specified
// by imm.  https://msdn.microsoft.com/en-us/library/13ywktbs(v=vs.100).aspx
// FORCE_INLINE __m128i _mm_shufflehi_epi16_function(__m128i a,
// __constrange(0,255) int imm)
#define _mm_shufflelo_epi16_function(a, imm)                                  \
    ({                                                                        \
        int16x8_t ret = vreinterpretq_s16_s32(a);                             \
        int16x4_t lowBits = vget_low_s16(ret);                                \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, (imm) &0x3), ret, 4);     \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 2) & 0x3), ret, \
                             5);                                              \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 4) & 0x3), ret, \
                             6);                                              \
        ret = vsetq_lane_s16(vget_lane_s16(lowBits, ((imm) >> 6) & 0x3), ret, \
                             7);                                              \
        vreinterpretq_s32_s16(ret);                                           \
    })

// FORCE_INLINE __m128i _mm_shufflehi_epi16(__m128i a, __constrange(0,255) int
// imm)
#define _mm_shufflelo_epi16(a, imm) _mm_shufflehi_epi16_function((a), (imm))

// Shuffles the upper 4 signed or unsigned 16 - bit integers in a as specified
// by imm.  https://msdn.microsoft.com/en-us/library/13ywktbs(v=vs.100).aspx
// FORCE_INLINE __m128i _mm_shufflehi_epi16_function(__m128i a,
// __constrange(0,255) int imm)
#define _mm_shufflehi_epi16_function(a, imm)                                   \
    ({                                                                         \
        int16x8_t ret = vreinterpretq_s16_s32(a);                              \
        int16x4_t highBits = vget_high_s16(ret);                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, (imm) &0x3), ret, 4);     \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 2) & 0x3), ret, \
                             5);                                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 4) & 0x3), ret, \
                             6);                                               \
        ret = vsetq_lane_s16(vget_lane_s16(highBits, ((imm) >> 6) & 0x3), ret, \
                             7);                                               \
        vreinterpretq_s32_s16(ret);                                            \
    })

// FORCE_INLINE __m128i _mm_shufflehi_epi16(__m128i a, __constrange(0,255) int
// imm)
#define _mm_shufflehi_epi16(a, imm) _mm_shufflehi_epi16_function((a), (imm))

// Shifts the 4 signed 32-bit integers in a right by count bits while shifting
// in the sign bit.
//
//   r0 := a0 >> count
//   r1 := a1 >> count
//   r2 := a2 >> count
//   r3 := a3 >> count immediate
FORCE_INLINE __m128i _mm_srai_epi32(__m128i a, int count)
{
    return vshlq_s32(a, vdupq_n_s32(-count));
}

// Shifts the 8 signed 16-bit integers in a right by count bits while shifting
// in the sign bit.
//
//   r0 := a0 >> count
//   r1 := a1 >> count
//   ...
//   r7 := a7 >> count
FORCE_INLINE __m128i _mm_srai_epi16(__m128i a, int count)
{
    return (__m128i) vshlq_s16((int16x8_t) a, vdupq_n_s16(-count));
}

// Shifts the 8 signed or unsigned 16-bit integers in a left by count bits while
// shifting in zeros.
//
//   r0 := a0 << count
//   r1 := a1 << count
//   ...
//   r7 := a7 << count
//
// https://msdn.microsoft.com/en-us/library/es73bcsy(v=vs.90).aspx
#define _mm_slli_epi16(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 31) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_s16(                       \
                vshlq_n_s16(vreinterpretq_s16_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shifts the 4 signed or unsigned 32-bit integers in a left by count bits while
// shifting in zeros. :
// https://msdn.microsoft.com/en-us/library/z2k3bbtb%28v=vs.90%29.aspx
// FORCE_INLINE __m128i _mm_slli_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_slli_epi32(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 31) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_s32(                       \
                vshlq_n_s32(vreinterpretq_s32_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shift packed 64-bit integers in a left by imm8 while shifting in zeros, and
// store the results in dst.
#define _mm_slli_epi64(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 63) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_s64(                       \
                vshlq_n_s64(vreinterpretq_s64_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shifts the 8 signed or unsigned 16-bit integers in a right by count bits
// while shifting in zeros.
//
//   r0 := srl(a0, count)
//   r1 := srl(a1, count)
//   ...
//   r7 := srl(a7, count)
//
// https://msdn.microsoft.com/en-us/library/6tcwd38t(v=vs.90).aspx
#define _mm_srli_epi16(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 31) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_u16(                       \
                vshrq_n_u16(vreinterpretq_u16_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shifts the 4 signed or unsigned 32-bit integers in a right by count bits
// while shifting in zeros.
// https://msdn.microsoft.com/en-us/library/w486zcfa(v=vs.100).aspx FORCE_INLINE
// __m128i _mm_srli_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_srli_epi32(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 31) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_u32(                       \
                vshrq_n_u32(vreinterpretq_u32_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shift packed 64-bit integers in a right by imm8 while shifting in zeros, and
// store the results in dst.
#define _mm_srli_epi64(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 63) {                                 \
            ret = _mm_setzero_si128();                           \
        } else {                                                 \
            ret = vreinterpretq_m128i_u64(                       \
                vshrq_n_u64(vreinterpretq_u64_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shifts the 4 signed 32 - bit integers in a right by count bits while shifting
// in the sign bit.
// https://msdn.microsoft.com/en-us/library/z1939387(v=vs.100).aspx
// FORCE_INLINE __m128i _mm_srai_epi32(__m128i a, __constrange(0,255) int imm)
#define _mm_srai_epi32(a, imm)                                   \
    ({                                                           \
        __m128i ret;                                             \
        if ((imm) <= 0) {                                        \
            ret = a;                                             \
        } else if ((imm) > 31) {                                 \
            ret = vreinterpretq_m128i_s32(                       \
                vshrq_n_s32(vreinterpretq_s32_m128i(a), 16));    \
            ret = vreinterpretq_m128i_s32(                       \
                vshrq_n_s32(vreinterpretq_s32_m128i(ret), 16));  \
        } else {                                                 \
            ret = vreinterpretq_m128i_s32(                       \
                vshrq_n_s32(vreinterpretq_s32_m128i(a), (imm))); \
        }                                                        \
        ret;                                                     \
    })

// Shifts the 128 - bit value in a right by imm bytes while shifting in
// zeros.imm must be an immediate.
//
//   r := srl(a, imm*8)
//
// https://msdn.microsoft.com/en-us/library/305w28yz(v=vs.100).aspx
// FORCE_INLINE _mm_srli_si128(__m128i a, __constrange(0,255) int imm)
#define _mm_srli_si128(a, imm)                                              \
    ({                                                                      \
        __m128i ret;                                                        \
        if ((imm) <= 0) {                                                   \
            ret = a;                                                        \
        } else if ((imm) > 15) {                                            \
            ret = _mm_setzero_si128();                                      \
        } else {                                                            \
            ret = vreinterpretq_m128i_s8(                                   \
                vextq_s8(vreinterpretq_s8_m128i(a), vdupq_n_s8(0), (imm))); \
        }                                                                   \
        ret;                                                                \
    })

// Shifts the 128-bit value in a left by imm bytes while shifting in zeros. imm
// must be an immediate.
//
//   r := a << (imm * 8)
//
// https://msdn.microsoft.com/en-us/library/34d3k2kt(v=vs.100).aspx
// FORCE_INLINE __m128i _mm_slli_si128(__m128i a, __constrange(0,255) int imm)
#define _mm_slli_si128(a, imm)                                          \
    ({                                                                  \
        __m128i ret;                                                    \
        if ((imm) <= 0) {                                               \
            ret = a;                                                    \
        } else if ((imm) > 15) {                                        \
            ret = _mm_setzero_si128();                                  \
        } else {                                                        \
            ret = vreinterpretq_m128i_s8(vextq_s8(                      \
                vdupq_n_s8(0), vreinterpretq_s8_m128i(a), 16 - (imm))); \
        }                                                               \
        ret;                                                            \
    })

// NEON does not provide a version of this function, here is an article about
// some ways to repro the results.
// http://stackoverflow.com/questions/11870910/sse-mm-movemask-epi8-equivalent-method-for-arm-neon
// Creates a 16-bit mask from the most significant bits of the 16 signed or
// unsigned 8-bit integers in a and zero extends the upper bits.
// https://msdn.microsoft.com/en-us/library/vstudio/s090c8fk(v=vs.100).aspx
FORCE_INLINE int _mm_movemask_epi8(__m128i _a)
{
    uint8x16_t input = vreinterpretq_u8_m128i(_a);
    static const int8_t __attribute__((aligned(16)))
    xr[8] = {-7, -6, -5, -4, -3, -2, -1, 0};
    uint8x8_t mask_and = vdup_n_u8(0x80);
    int8x8_t mask_shift = vld1_s8(xr);

    uint8x8_t lo = vget_low_u8(input);
    uint8x8_t hi = vget_high_u8(input);

    lo = vand_u8(lo, mask_and);
    lo = vshl_u8(lo, mask_shift);

    hi = vand_u8(hi, mask_and);
    hi = vshl_u8(hi, mask_shift);

    lo = vpadd_u8(lo, lo);
    lo = vpadd_u8(lo, lo);
    lo = vpadd_u8(lo, lo);

    hi = vpadd_u8(hi, hi);
    hi = vpadd_u8(hi, hi);
    hi = vpadd_u8(hi, hi);

    return ((hi[0] << 8) | (lo[0] & 0xFF));
}

// Compute the bitwise AND of 128 bits (representing integer data) in a and
// mask, and return 1 if the result is zero, otherwise return 0.
// https://software.intel.com/sites/landingpage/IntrinsicsGuide/#text=_mm_test_all_zeros&expand=5871
FORCE_INLINE int _mm_test_all_zeros(__m128i a, __m128i mask)
{
    int64x2_t a_and_mask =
        vandq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(mask));
    return (vgetq_lane_s64(a_and_mask, 0) | vgetq_lane_s64(a_and_mask, 1)) ? 0
                                                                           : 1;
}

// ******************************************
// Math operations
// ******************************************

// Subtracts the four single-precision, floating-point values of a and b.
//
//   r0 := a0 - b0
//   r1 := a1 - b1
//   r2 := a2 - b2
//   r3 := a3 - b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/1zad2k61(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sub_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vsubq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Subtract 2 packed 64-bit integers in b from 2 packed 64-bit integers in a,
// and store the results in dst.
//    r0 := a0 - b0
//    r1 := a1 - b1
FORCE_INLINE __m128i _mm_sub_epi64(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s64(
        vsubq_s64(vreinterpretq_s64_m128i(a), vreinterpretq_s64_m128i(b)));
}

// Subtracts the 4 signed or unsigned 32-bit integers of b from the 4 signed or
// unsigned 32-bit integers of a.
//
//   r0 := a0 - b0
//   r1 := a1 - b1
//   r2 := a2 - b2
//   r3 := a3 - b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/fhh866h0(v=vs.100).aspx
FORCE_INLINE __m128i _mm_sub_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128_f32(
        vsubq_s32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

FORCE_INLINE __m128i _mm_sub_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vsubq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

FORCE_INLINE __m128i _mm_sub_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vsubq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Subtracts the 8 unsigned 16-bit integers of bfrom the 8 unsigned 16-bit
// integers of a and saturates..
// https://technet.microsoft.com/en-us/subscriptions/index/f44y0s19(v=vs.90).aspx
FORCE_INLINE __m128i _mm_subs_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vqsubq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

// Subtracts the 16 unsigned 8-bit integers of b from the 16 unsigned 8-bit
// integers of a and saturates.
//
//   r0 := UnsignedSaturate(a0 - b0)
//   r1 := UnsignedSaturate(a1 - b1)
//   ...
//   r15 := UnsignedSaturate(a15 - b15)
//
// https://technet.microsoft.com/en-us/subscriptions/yadkxc18(v=vs.90)
FORCE_INLINE __m128i _mm_subs_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vqsubq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Subtracts the 8 signed 16-bit integers of b from the 8 signed 16-bit integers
// of a and saturates.
//
//   r0 := SignedSaturate(a0 - b0)
//   r1 := SignedSaturate(a1 - b1)
//   ...
//   r7 := SignedSaturate(a7 - b7)
FORCE_INLINE __m128i _mm_subs_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vqsubq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

FORCE_INLINE __m128i _mm_adds_epu16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vqaddq_u16(vreinterpretq_u16_m128i(a), vreinterpretq_u16_m128i(b)));
}

FORCE_INLINE __m128i _mm_sign_epi32(__m128i a, __m128i b)
{
    __m128i zer0 = vdupq_n_s32(0);
    __m128i ltMask = vreinterpretq_s32_u32(vcltq_s32(b, zer0));
    __m128i gtMask = vreinterpretq_s32_u32(vcgtq_s32(b, zer0));
    __m128i neg = vnegq_s32(a);
    __m128i tmp = vandq_s32(a, gtMask);
    return vorrq_s32(tmp, vandq_s32(neg, ltMask));
}

FORCE_INLINE __m128i _mm_sign_epi16(__m128i a, __m128i b)
{
    int16x8_t zer0 = vdupq_n_s16(0);
    int16x8_t ltMask =
        vreinterpretq_s16_u16(vcltq_s16(vreinterpretq_s16_s32(b), zer0));
    int16x8_t gtMask =
        vreinterpretq_s16_u16(vcgtq_s16(vreinterpretq_s16_s32(b), zer0));
    int16x8_t neg = vnegq_s16(vreinterpretq_s16_s32(a));
    int16x8_t tmp = vandq_s16(vreinterpretq_s16_s32(a), gtMask);
    return vreinterpretq_s32_s16(vorrq_s16(tmp, vandq_s16(neg, ltMask)));
}

// Adds the four single-precision, floating-point values of a and b.
//
//   r0 := a0 + b0
//   r1 := a1 + b1
//   r2 := a2 + b2
//   r3 := a3 + b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/c9848chc(v=vs.100).aspx
FORCE_INLINE __m128 _mm_add_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vaddq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// adds the scalar single-precision floating point values of a and b.
// https://msdn.microsoft.com/en-us/library/be94x2y6(v=vs.100).aspx
FORCE_INLINE __m128 _mm_add_ss(__m128 a, __m128 b)
{
    float32_t b0 = vgetq_lane_f32(vreinterpretq_f32_m128(b), 0);
    float32x4_t value = vsetq_lane_f32(b0, vdupq_n_f32(0), 0);
    // the upper values in the result must be the remnants of <a>.
    return vreinterpretq_m128_f32(vaddq_f32(a, value));
}

// Adds the 4 signed or unsigned 64-bit integers in a to the 4 signed or
// unsigned 32-bit integers in b.
// https://msdn.microsoft.com/en-us/library/vstudio/09xs4fkk(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi64(__m128i a, __m128i b)
{
    return vreinterpretq_s32_s64(
        vaddq_s64(vreinterpretq_s64_s32(a), vreinterpretq_s64_s32(b)));
}

// Adds the 4 signed or unsigned 32-bit integers in a to the 4 signed or
// unsigned 32-bit integers in b.
//
//   r0 := a0 + b0
//   r1 := a1 + b1
//   r2 := a2 + b2
//   r3 := a3 + b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/09xs4fkk(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vaddq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Adds the 8 signed or unsigned 16-bit integers in a to the 8 signed or
// unsigned 16-bit integers in b.
// https://msdn.microsoft.com/en-us/library/fceha5k4(v=vs.100).aspx
FORCE_INLINE __m128i _mm_add_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vaddq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Adds the 16 signed or unsigned 8-bit integers in a to the 16 signed or
// unsigned 8-bit integers in b.
// https://technet.microsoft.com/en-us/subscriptions/yc7tcyzs(v=vs.90)
FORCE_INLINE __m128i _mm_add_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vaddq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Adds the 8 signed 16-bit integers in a to the 8 signed 16-bit integers in b
// and saturates.
//
//   r0 := SignedSaturate(a0 + b0)
//   r1 := SignedSaturate(a1 + b1)
//   ...
//   r7 := SignedSaturate(a7 + b7)
//
// https://msdn.microsoft.com/en-us/library/1a306ef8(v=vs.100).aspx
FORCE_INLINE __m128i _mm_adds_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vqaddq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Adds the 16 unsigned 8-bit integers in a to the 16 unsigned 8-bit integers in
// b and saturates..
// https://msdn.microsoft.com/en-us/library/9hahyddy(v=vs.100).aspx
FORCE_INLINE __m128i _mm_adds_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vqaddq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Multiplies the 8 signed or unsigned 16-bit integers from a by the 8 signed or
// unsigned 16-bit integers from b.
//
//   r0 := (a0 * b0)[15:0]
//   r1 := (a1 * b1)[15:0]
//   ...
//   r7 := (a7 * b7)[15:0]
//
// https://msdn.microsoft.com/en-us/library/vstudio/9ks1472s(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mullo_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vmulq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Multiplies the 4 signed or unsigned 32-bit integers from a by the 4 signed or
// unsigned 32-bit integers from b.
// https://msdn.microsoft.com/en-us/library/vstudio/bb531409(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mullo_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vmulq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Multiplies the four single-precision, floating-point values of a and b.
//
//   r0 := a0 * b0
//   r1 := a1 * b1
//   r2 := a2 * b2
//   r3 := a3 * b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/22kbk6t9(v=vs.100).aspx
FORCE_INLINE __m128 _mm_mul_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vmulq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Multiply the low unsigned 32-bit integers from each packed 64-bit element in
// a and b, and store the unsigned 64-bit results in dst.
//
//   r0 :=  (uint32_t*)a0 * (uint32_t*)b0
//   r1 :=  (uint32_t*)a3 * (uint32_t*)b3
FORCE_INLINE __m128i _mm_mul_epu32(__m128i a, __m128i b)
{
    // Mask out the high 32-bit integers and then multiply them as 64-bit
    uint32_t __attribute__((aligned(16)))
    data[4] = {0xFFFFFFFF, 0, 0xFFFFFFFF, 0};
    __m128i mask = vreinterpretq_m128i_u32(vld1q_u32(data));
    return vreinterpretq_m128i_u32(
        vmulq_u32(vreinterpretq_u32_m128i(_mm_and_si128(a, mask)),
                  vreinterpretq_u32_m128i(_mm_and_si128(b, mask))));
}

// Multiplies the 8 signed 16-bit integers from a by the 8 signed 16-bit
// integers from b.
//
//   r0 := (a0 * b0) + (a1 * b1)
//   r1 := (a2 * b2) + (a3 * b3)
//   r2 := (a4 * b4) + (a5 * b5)
//   r3 := (a6 * b6) + (a7 * b7)
// https://msdn.microsoft.com/en-us/library/yht36sa6(v=vs.90).aspx
FORCE_INLINE __m128i _mm_madd_epi16(__m128i a, __m128i b)
{
    int32x4_t low = vmull_s16(vget_low_s16(vreinterpretq_s16_m128i(a)),
                              vget_low_s16(vreinterpretq_s16_m128i(b)));
    int32x4_t high = vmull_s16(vget_high_s16(vreinterpretq_s16_m128i(a)),
                               vget_high_s16(vreinterpretq_s16_m128i(b)));

    int32x2_t low_sum = vpadd_s32(vget_low_s32(low), vget_high_s32(low));
    int32x2_t high_sum = vpadd_s32(vget_low_s32(high), vget_high_s32(high));

    return vreinterpretq_s32_m128i(vcombine_s32(low_sum, high_sum));
}

// Computes the absolute difference of the 16 unsigned 8-bit integers from a
// and the 16 unsigned 8-bit integers from b.
//
// Return Value
// Sums the upper 8 differences and lower 8 differences and packs the
// resulting 2 unsigned 16-bit integers into the upper and lower 64-bit
// elements.
//
//   r0 := abs(a0 - b0) + abs(a1 - b1) +...+ abs(a7 - b7)
//   r1 := 0x0
//   r2 := 0x0
//   r3 := 0x0
//   r4 := abs(a8 - b8) + abs(a9 - b9) +...+ abs(a15 - b15)
//   r5 := 0x0
//   r6 := 0x0
//   r7 := 0x0
FORCE_INLINE __m128i _mm_sad_epu8(__m128i a, __m128i b)
{
    uint16x8_t t = vpaddlq_u8(vabdq_u8((uint8x16_t) a, (uint8x16_t) b));
    uint16_t r0 = t[0] + t[1] + t[2] + t[3];
    uint16_t r4 = t[4] + t[5] + t[6] + t[7];
    uint16x8_t r = vsetq_lane_u16(r0, vdupq_n_u16(0), 0);
    return (__m128i) vsetq_lane_u16(r4, r, 4);
}

// Divides the four single-precision, floating-point values of a and b.
//
//   r0 := a0 / b0
//   r1 := a1 / b1
//   r2 := a2 / b2
//   r3 := a3 / b3
//
// https://msdn.microsoft.com/en-us/library/edaw8147(v=vs.100).aspx
FORCE_INLINE __m128 _mm_div_ps(__m128 a, __m128 b)
{
    float32x4_t recip0 = vrecpeq_f32(vreinterpretq_f32_m128(b));
    float32x4_t recip1 =
        vmulq_f32(recip0, vrecpsq_f32(recip0, vreinterpretq_f32_m128(b)));
    return vreinterpretq_m128_f32(vmulq_f32(vreinterpretq_f32_m128(a), recip1));
}

// Divides the scalar single-precision floating point value of a by b.
// https://msdn.microsoft.com/en-us/library/4y73xa49(v=vs.100).aspx
FORCE_INLINE __m128 _mm_div_ss(__m128 a, __m128 b)
{
    float32_t value =
        vgetq_lane_f32(vreinterpretq_f32_m128(_mm_div_ps(a, b)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// This version does additional iterations to improve accuracy.  Between 1 and 4
// recommended. Computes the approximations of reciprocals of the four
// single-precision, floating-point values of a.
// https://msdn.microsoft.com/en-us/library/vstudio/796k1tty(v=vs.100).aspx
FORCE_INLINE __m128 recipq_newton(__m128 in, int n)
{
    int i;
    float32x4_t recip = vrecpeq_f32(vreinterpretq_f32_m128(in));
    for (i = 0; i < n; ++i) {
        recip =
            vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(in)));
    }
    return vreinterpretq_m128_f32(recip);
}

// Computes the approximations of reciprocals of the four single-precision,
// floating-point values of a.
// https://msdn.microsoft.com/en-us/library/vstudio/796k1tty(v=vs.100).aspx
FORCE_INLINE __m128 _mm_rcp_ps(__m128 in)
{
    float32x4_t recip = vrecpeq_f32(vreinterpretq_f32_m128(in));
    recip = vmulq_f32(recip, vrecpsq_f32(recip, vreinterpretq_f32_m128(in)));
    return vreinterpretq_m128_f32(recip);
}

// Computes the approximations of square roots of the four single-precision,
// floating-point values of a. First computes reciprocal square roots and then
// reciprocals of the four values.
//
//   r0 := sqrt(a0)
//   r1 := sqrt(a1)
//   r2 := sqrt(a2)
//   r3 := sqrt(a3)
//
// https://msdn.microsoft.com/en-us/library/vstudio/8z67bwwk(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sqrt_ps(__m128 in)
{
    float32x4_t recipsq = vrsqrteq_f32(vreinterpretq_f32_m128(in));
    float32x4_t sq = vrecpeq_f32(recipsq);
    // ??? use step versions of both sqrt and recip for better accuracy?
    return vreinterpretq_m128_f32(sq);
}

// Computes the approximation of the square root of the scalar single-precision
// floating point value of in.
// https://msdn.microsoft.com/en-us/library/ahfsc22d(v=vs.100).aspx
FORCE_INLINE __m128 _mm_sqrt_ss(__m128 in)
{
    float32_t value =
        vgetq_lane_f32(vreinterpretq_f32_m128(_mm_sqrt_ps(in)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(in), 0));
}

// Computes the approximations of the reciprocal square roots of the four
// single-precision floating point values of in.
// https://msdn.microsoft.com/en-us/library/22hfsh53(v=vs.100).aspx
FORCE_INLINE __m128 _mm_rsqrt_ps(__m128 in)
{
    return vreinterpretq_m128_f32(vrsqrteq_f32(vreinterpretq_f32_m128(in)));
}

// Computes the maximums of the four single-precision, floating-point values of
// a and b.
// https://msdn.microsoft.com/en-us/library/vstudio/ff5d607a(v=vs.100).aspx
FORCE_INLINE __m128 _mm_max_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vmaxq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Computes the minima of the four single-precision, floating-point values of a
// and b.
// https://msdn.microsoft.com/en-us/library/vstudio/wh13kadz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_min_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_f32(
        vminq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Computes the maximum of the two lower scalar single-precision floating point
// values of a and b.
// https://msdn.microsoft.com/en-us/library/s6db5esz(v=vs.100).aspx
FORCE_INLINE __m128 _mm_max_ss(__m128 a, __m128 b)
{
    float32_t value = vgetq_lane_f32(
        vmaxq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// Computes the minimum of the two lower scalar single-precision floating point
// values of a and b.
// https://msdn.microsoft.com/en-us/library/0a9y7xaa(v=vs.100).aspx
FORCE_INLINE __m128 _mm_min_ss(__m128 a, __m128 b)
{
    float32_t value = vgetq_lane_f32(
        vminq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)), 0);
    return vreinterpretq_m128_f32(
        vsetq_lane_f32(value, vreinterpretq_f32_m128(a), 0));
}

// Computes the pairwise maxima of the 16 unsigned 8-bit integers from a and the
// 16 unsigned 8-bit integers from b.
// https://msdn.microsoft.com/en-us/library/st6634za(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vmaxq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Computes the pairwise minima of the 16 unsigned 8-bit integers from a and the
// 16 unsigned 8-bit integers from b.
// https://msdn.microsoft.com/ko-kr/library/17k8cf58(v=vs.100).aspxx
FORCE_INLINE __m128i _mm_min_epu8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vminq_u8(vreinterpretq_u8_m128i(a), vreinterpretq_u8_m128i(b)));
}

// Computes the pairwise minima of the 8 signed 16-bit integers from a and the 8
// signed 16-bit integers from b.
// https://msdn.microsoft.com/en-us/library/vstudio/6te997ew(v=vs.100).aspx
FORCE_INLINE __m128i _mm_min_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vminq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Computes the pairwise maxima of the 8 signed 16-bit integers from a and the 8
// signed 16-bit integers from b.
// https://msdn.microsoft.com/en-us/LIBRary/3x060h7c(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vmaxq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// epi versions of min/max
// Computes the pariwise maximums of the four signed 32-bit integer values of a
// and b.
//
// A 128-bit parameter that can be defined with the following equations:
//   r0 := (a0 > b0) ? a0 : b0
//   r1 := (a1 > b1) ? a1 : b1
//   r2 := (a2 > b2) ? a2 : b2
//   r3 := (a3 > b3) ? a3 : b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/bb514055(v=vs.100).aspx
FORCE_INLINE __m128i _mm_max_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vmaxq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Computes the pariwise minima of the four signed 32-bit integer values of a
// and b.
//
// A 128-bit parameter that can be defined with the following equations:
//   r0 := (a0 < b0) ? a0 : b0
//   r1 := (a1 < b1) ? a1 : b1
//   r2 := (a2 < b2) ? a2 : b2
//   r3 := (a3 < b3) ? a3 : b3
//
// https://msdn.microsoft.com/en-us/library/vstudio/bb531476(v=vs.100).aspx
FORCE_INLINE __m128i _mm_min_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s32(
        vminq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Multiplies the 8 signed 16-bit integers from a by the 8 signed 16-bit
// integers from b.
//
//   r0 := (a0 * b0)[31:16]
//   r1 := (a1 * b1)[31:16]
//   ...
//   r7 := (a7 * b7)[31:16]
//
// https://msdn.microsoft.com/en-us/library/vstudio/59hddw1d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_mulhi_epi16(__m128i a, __m128i b)
{
    /* FIXME: issue with large values because of result saturation */
    // int16x8_t ret = vqdmulhq_s16(vreinterpretq_s16_m128i(a),
    // vreinterpretq_s16_m128i(b)); /* =2*a*b */ return
    // vreinterpretq_m128i_s16(vshrq_n_s16(ret, 1));
    int16x4_t a3210 = vget_low_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b3210 = vget_low_s16(vreinterpretq_s16_m128i(b));
    int32x4_t ab3210 = vmull_s16(a3210, b3210); /* 3333222211110000 */
    int16x4_t a7654 = vget_high_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b7654 = vget_high_s16(vreinterpretq_s16_m128i(b));
    int32x4_t ab7654 = vmull_s16(a7654, b7654); /* 7777666655554444 */
    uint16x8x2_t r =
        vuzpq_u16(vreinterpretq_u16_s32(ab3210), vreinterpretq_u16_s32(ab7654));
    return vreinterpretq_m128i_u16(r.val[1]);
}

// Computes pairwise add of each argument as single-precision, floating-point
// values a and b.
// https://msdn.microsoft.com/en-us/library/yd9wecaa.aspx
FORCE_INLINE __m128 _mm_hadd_ps(__m128 a, __m128 b)
{
#if defined(__aarch64__)
    return vreinterpretq_m128_f32(vpaddq_f32(
        vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));  // AArch64
#else
    float32x2_t a10 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t a32 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b10 = vget_low_f32(vreinterpretq_f32_m128(b));
    float32x2_t b32 = vget_high_f32(vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_f32(
        vcombine_f32(vpadd_f32(a10, a32), vpadd_f32(b10, b32)));
#endif
}

// ******************************************
// Compare operations
// ******************************************

// Compares for less than
// https://msdn.microsoft.com/en-us/library/vstudio/f330yhc8(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmplt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for greater than.
//
//   r0 := (a0 > b0) ? 0xffffffff : 0x0
//   r1 := (a1 > b1) ? 0xffffffff : 0x0
//   r2 := (a2 > b2) ? 0xffffffff : 0x0
//   r3 := (a3 > b3) ? 0xffffffff : 0x0
//
// https://msdn.microsoft.com/en-us/library/vstudio/11dy102s(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpgt_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcgtq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for greater than or equal.
// https://msdn.microsoft.com/en-us/library/vstudio/fs813y2t(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpge_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcgeq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for less than or equal.
//
//   r0 := (a0 <= b0) ? 0xffffffff : 0x0
//   r1 := (a1 <= b1) ? 0xffffffff : 0x0
//   r2 := (a2 <= b2) ? 0xffffffff : 0x0
//   r3 := (a3 <= b3) ? 0xffffffff : 0x0
//
// https://msdn.microsoft.com/en-us/library/vstudio/1s75w83z(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmple_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vcleq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares for equality.
// https://msdn.microsoft.com/en-us/library/vstudio/36aectz5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cmpeq_ps(__m128 a, __m128 b)
{
    return vreinterpretq_m128_u32(
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
}

// Compares the 16 signed or unsigned 8-bit integers in a and the 16 signed or
// unsigned 8-bit integers in b for equality.
// https://msdn.microsoft.com/en-us/library/windows/desktop/bz5xk21a(v=vs.90).aspx
FORCE_INLINE __m128i _mm_cmpeq_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vceqq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compares the 8 signed or unsigned 16-bit integers in a and the 8 signed or
// unsigned 16-bit integers in b for equality.
// https://msdn.microsoft.com/en-us/library/2ay060te(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpeq_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vceqq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}

// Compare packed 32-bit integers in a and b for equality, and store the results
// in dst
FORCE_INLINE __m128i _mm_cmpeq_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vceqq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the 16 signed 8-bit integers in a and the 16 signed 8-bit integers
// in b for lesser than.
// https://msdn.microsoft.com/en-us/library/windows/desktop/9s46csht(v=vs.90).aspx
FORCE_INLINE __m128i _mm_cmplt_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcltq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compares the 16 signed 8-bit integers in a and the 16 signed 8-bit integers
// in b for greater than.
//
//   r0 := (a0 > b0) ? 0xff : 0x0
//   r1 := (a1 > b1) ? 0xff : 0x0
//   ...
//   r15 := (a15 > b15) ? 0xff : 0x0
//
// https://msdn.microsoft.com/zh-tw/library/wf45zt2b(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi8(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcgtq_s8(vreinterpretq_s8_m128i(a), vreinterpretq_s8_m128i(b)));
}

// Compares the 8 signed 16-bit integers in a and the 8 signed 16-bit integers
// in b for greater than.
//
//   r0 := (a0 > b0) ? 0xffff : 0x0
//   r1 := (a1 > b1) ? 0xffff : 0x0
//   ...
//   r7 := (a7 > b7) ? 0xffff : 0x0
//
// https://technet.microsoft.com/en-us/library/xd43yfsa(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u16(
        vcgtq_s16(vreinterpretq_s16_m128i(a), vreinterpretq_s16_m128i(b)));
}


// Compares the 4 signed 32-bit integers in a and the 4 signed 32-bit integers
// in b for less than.
// https://msdn.microsoft.com/en-us/library/vstudio/4ak0bf5d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmplt_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vcltq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the 4 signed 32-bit integers in a and the 4 signed 32-bit integers
// in b for greater than.
// https://msdn.microsoft.com/en-us/library/vstudio/1s9f2z0y(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cmpgt_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_u32(
        vcgtq_s32(vreinterpretq_s32_m128i(a), vreinterpretq_s32_m128i(b)));
}

// Compares the four 32-bit floats in a and b to check if any values are NaN.
// Ordered compare between each value returns true for "orderable" and false for
// "not orderable" (NaN).
// https://msdn.microsoft.com/en-us/library/vstudio/0h9w00fx(v=vs.100).aspx see
// also:
// http://stackoverflow.com/questions/8627331/what-does-ordered-unordered-comparison-mean
// http://stackoverflow.com/questions/29349621/neon-isnanval-intrinsics
FORCE_INLINE __m128 _mm_cmpord_ps(__m128 a, __m128 b)
{
    // Note: NEON does not have ordered compare builtin
    // Need to compare a eq a and b eq b to check for NaN
    // Do AND of results to get final
    uint32x4_t ceqaa =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t ceqbb =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    return vreinterpretq_m128_u32(vandq_u32(ceqaa, ceqbb));
}

// Compares the lower single-precision floating point scalar values of a and b
// using a less than operation. :
// https://msdn.microsoft.com/en-us/library/2kwe606b(v=vs.90).aspx Important
// note!! The documentation on MSDN is incorrect!  If either of the values is a
// NAN the docs say you will get a one, but in fact, it will return a zero!!
FORCE_INLINE int _mm_comilt_ss(__m128 a, __m128 b)
{
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_or_b_nan = vmvnq_u32(vandq_u32(a_not_nan, b_not_nan));
    uint32x4_t a_lt_b =
        vcltq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return (vgetq_lane_u32(vorrq_u32(a_or_b_nan, a_lt_b), 0) != 0) ? 1 : 0;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a greater than operation. :
// https://msdn.microsoft.com/en-us/library/b0738e0t(v=vs.100).aspx
FORCE_INLINE int _mm_comigt_ss(__m128 a, __m128 b)
{
    // return vgetq_lane_u32(vcgtq_f32(vreinterpretq_f32_m128(a),
    // vreinterpretq_f32_m128(b)), 0);
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_and_b_not_nan = vandq_u32(a_not_nan, b_not_nan);
    uint32x4_t a_gt_b =
        vcgtq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return (vgetq_lane_u32(vandq_u32(a_and_b_not_nan, a_gt_b), 0) != 0) ? 1 : 0;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a less than or equal operation. :
// https://msdn.microsoft.com/en-us/library/1w4t7c57(v=vs.90).aspx
FORCE_INLINE int _mm_comile_ss(__m128 a, __m128 b)
{
    // return vgetq_lane_u32(vcleq_f32(vreinterpretq_f32_m128(a),
    // vreinterpretq_f32_m128(b)), 0);
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_or_b_nan = vmvnq_u32(vandq_u32(a_not_nan, b_not_nan));
    uint32x4_t a_le_b =
        vcleq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return (vgetq_lane_u32(vorrq_u32(a_or_b_nan, a_le_b), 0) != 0) ? 1 : 0;
}

// Compares the lower single-precision floating point scalar values of a and b
// using a greater than or equal operation. :
// https://msdn.microsoft.com/en-us/library/8t80des6(v=vs.100).aspx
FORCE_INLINE int _mm_comige_ss(__m128 a, __m128 b)
{
    // return vgetq_lane_u32(vcgeq_f32(vreinterpretq_f32_m128(a),
    // vreinterpretq_f32_m128(b)), 0);
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_and_b_not_nan = vandq_u32(a_not_nan, b_not_nan);
    uint32x4_t a_ge_b =
        vcgeq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return (vgetq_lane_u32(vandq_u32(a_and_b_not_nan, a_ge_b), 0) != 0) ? 1 : 0;
}

// Compares the lower single-precision floating point scalar values of a and b
// using an equality operation. :
// https://msdn.microsoft.com/en-us/library/93yx2h2b(v=vs.100).aspx
FORCE_INLINE int _mm_comieq_ss(__m128 a, __m128 b)
{
    // return vgetq_lane_u32(vceqq_f32(vreinterpretq_f32_m128(a),
    // vreinterpretq_f32_m128(b)), 0);
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_or_b_nan = vmvnq_u32(vandq_u32(a_not_nan, b_not_nan));
    uint32x4_t a_eq_b =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b));
    return (vgetq_lane_u32(vorrq_u32(a_or_b_nan, a_eq_b), 0) != 0) ? 1 : 0;
}

// Compares the lower single-precision floating point scalar values of a and b
// using an inequality operation. :
// https://msdn.microsoft.com/en-us/library/bafh5e0a(v=vs.90).aspx
FORCE_INLINE int _mm_comineq_ss(__m128 a, __m128 b)
{
    // return !vgetq_lane_u32(vceqq_f32(vreinterpretq_f32_m128(a),
    // vreinterpretq_f32_m128(b)), 0);
    uint32x4_t a_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(a));
    uint32x4_t b_not_nan =
        vceqq_f32(vreinterpretq_f32_m128(b), vreinterpretq_f32_m128(b));
    uint32x4_t a_and_b_not_nan = vandq_u32(a_not_nan, b_not_nan);
    uint32x4_t a_neq_b = vmvnq_u32(
        vceqq_f32(vreinterpretq_f32_m128(a), vreinterpretq_f32_m128(b)));
    return (vgetq_lane_u32(vandq_u32(a_and_b_not_nan, a_neq_b), 0) != 0) ? 1
                                                                         : 0;
}

// according to the documentation, these intrinsics behave the same as the
// non-'u' versions.  We'll just alias them here.
#define _mm_ucomilt_ss _mm_comilt_ss
#define _mm_ucomile_ss _mm_comile_ss
#define _mm_ucomigt_ss _mm_comigt_ss
#define _mm_ucomige_ss _mm_comige_ss
#define _mm_ucomieq_ss _mm_comieq_ss
#define _mm_ucomineq_ss _mm_comineq_ss

// ******************************************
// Conversions
// ******************************************

// Converts the four single-precision, floating-point values of a to signed
// 32-bit integer values using truncate.
// https://msdn.microsoft.com/en-us/library/vstudio/1h005y6x(v=vs.100).aspx
FORCE_INLINE __m128i _mm_cvttps_epi32(__m128 a)
{
    return vreinterpretq_m128i_s32(vcvtq_s32_f32(vreinterpretq_f32_m128(a)));
}

// Converts the four signed 32-bit integer values of a to single-precision,
// floating-point values
// https://msdn.microsoft.com/en-us/library/vstudio/36bwxcx5(v=vs.100).aspx
FORCE_INLINE __m128 _mm_cvtepi32_ps(__m128i a)
{
    return vreinterpretq_m128_f32(vcvtq_f32_s32(vreinterpretq_s32_m128i(a)));
}

// Converts the four unsigned 8-bit integers in the lower 32 bits to four
// unsigned 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb531467%28v=vs.100%29.aspx
FORCE_INLINE __m128i _mm_cvtepu8_epi32(__m128i a)
{
    uint8x16_t u8x16 = vreinterpretq_u8_s32(a);        /* xxxx xxxx xxxx DCBA */
    uint16x8_t u16x8 = vmovl_u8(vget_low_u8(u8x16));   /* 0x0x 0x0x 0D0C 0B0A */
    uint32x4_t u32x4 = vmovl_u16(vget_low_u16(u16x8)); /* 000D 000C 000B 000A */
    return vreinterpretq_s32_u32(u32x4);
}

// Converts the four signed 16-bit integers in the lower 64 bits to four signed
// 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb514079%28v=vs.100%29.aspx
FORCE_INLINE __m128i _mm_cvtepi16_epi32(__m128i a)
{
    return vreinterpretq_m128i_s32(
        vmovl_s16(vget_low_s16(vreinterpretq_s16_m128i(a))));
}

// Converts the four single-precision, floating-point values of a to signed
// 32-bit integer values.
//
//   r0 := (int) a0
//   r1 := (int) a1
//   r2 := (int) a2
//   r3 := (int) a3
//
// https://msdn.microsoft.com/en-us/library/vstudio/xdc42k5e(v=vs.100).aspx
// *NOTE*. The default rounding mode on SSE is 'round to even', which ArmV7-A
// does not support! It is supported on ARMv8-A however.
FORCE_INLINE __m128i _mm_cvtps_epi32(__m128 a)
{
#if defined(__aarch64__)
    return vcvtnq_s32_f32(a);
#else
    uint32x4_t signmask = vdupq_n_u32(0x80000000);
    float32x4_t half = vbslq_f32(signmask, vreinterpretq_f32_m128(a),
                                 vdupq_n_f32(0.5f)); /* +/- 0.5 */
    int32x4_t r_normal = vcvtq_s32_f32(vaddq_f32(
        vreinterpretq_f32_m128(a), half)); /* round to integer: [a + 0.5]*/
    int32x4_t r_trunc =
        vcvtq_s32_f32(vreinterpretq_f32_m128(a)); /* truncate to integer: [a] */
    int32x4_t plusone = vreinterpretq_s32_u32(vshrq_n_u32(
        vreinterpretq_u32_s32(vnegq_s32(r_trunc)), 31)); /* 1 or 0 */
    int32x4_t r_even = vbicq_s32(vaddq_s32(r_trunc, plusone),
                                 vdupq_n_s32(1)); /* ([a] + {0,1}) & ~1 */
    float32x4_t delta = vsubq_f32(
        vreinterpretq_f32_m128(a),
        vcvtq_f32_s32(r_trunc)); /* compute delta: delta = (a - [a]) */
    uint32x4_t is_delta_half = vceqq_f32(delta, half); /* delta == +/- 0.5 */
    return vreinterpretq_m128i_s32(vbslq_s32(is_delta_half, r_even, r_normal));
#endif
}

// Moves the least significant 32 bits of a to a 32-bit integer.
// https://msdn.microsoft.com/en-us/library/5z7a9642%28v=vs.90%29.aspx
FORCE_INLINE int _mm_cvtsi128_si32(__m128i a)
{
    return vgetq_lane_s32(vreinterpretq_s32_m128i(a), 0);
}

// Extracts the low order 64-bit integer from the parameter.
// https://msdn.microsoft.com/en-us/library/bb531384(v=vs.120).aspx
FORCE_INLINE uint64_t _mm_cvtsi128_si64(__m128i a)
{
    return vgetq_lane_s64(vreinterpretq_s64_m128i(a), 0);
}

// Moves 32-bit integer a to the least significant 32 bits of an __m128 object,
// zero extending the upper bits.
//
//   r0 := a
//   r1 := 0x0
//   r2 := 0x0
//   r3 := 0x0
//
// https://msdn.microsoft.com/en-us/library/ct3539ha%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_cvtsi32_si128(int a)
{
    return vreinterpretq_m128i_s32(vsetq_lane_s32(a, vdupq_n_s32(0), 0));
}

// Applies a type cast to reinterpret four 32-bit floating point values passed
// in as a 128-bit parameter as packed 32-bit integers.
// https://msdn.microsoft.com/en-us/library/bb514099.aspx
FORCE_INLINE __m128i _mm_castps_si128(__m128 a)
{
    return vreinterpretq_m128i_s32(vreinterpretq_s32_m128(a));
}

// Applies a type cast to reinterpret four 32-bit integers passed in as a
// 128-bit parameter as packed 32-bit floating point values.
// https://msdn.microsoft.com/en-us/library/bb514029.aspx
FORCE_INLINE __m128 _mm_castsi128_ps(__m128i a)
{
    return vreinterpretq_m128_s32(vreinterpretq_s32_m128i(a));
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/en-us/library/atzzad1h(v=vs.80).aspx
FORCE_INLINE __m128i _mm_load_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// Loads 128-bit value. :
// https://msdn.microsoft.com/zh-cn/library/f4k12ae8(v=vs.90).aspx
FORCE_INLINE __m128i _mm_loadu_si128(const __m128i *p)
{
    return vreinterpretq_m128i_s32(vld1q_s32((const int32_t *) p));
}

// ******************************************
// Miscellaneous Operations
// ******************************************

// Packs the 16 signed 16-bit integers from a and b into 8-bit integers and
// saturates.
// https://msdn.microsoft.com/en-us/library/k4y4f7w5%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_packs_epi16(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s8(
        vcombine_s8(vqmovn_s16(vreinterpretq_s16_m128i(a)),
                    vqmovn_s16(vreinterpretq_s16_m128i(b))));
}

// Packs the 16 signed 16 - bit integers from a and b into 8 - bit unsigned
// integers and saturates.
//
//   r0 := UnsignedSaturate(a0)
//   r1 := UnsignedSaturate(a1)
//   ...
//   r7 := UnsignedSaturate(a7)
//   r8 := UnsignedSaturate(b0)
//   r9 := UnsignedSaturate(b1)
//   ...
//   r15 := UnsignedSaturate(b7)
//
// https://msdn.microsoft.com/en-us/library/07ad1wx4(v=vs.100).aspx
FORCE_INLINE __m128i _mm_packus_epi16(const __m128i a, const __m128i b)
{
    return vreinterpretq_m128i_u8(
        vcombine_u8(vqmovun_s16(vreinterpretq_s16_m128i(a)),
                    vqmovun_s16(vreinterpretq_s16_m128i(b))));
}

// Packs the 8 signed 32-bit integers from a and b into signed 16-bit integers
// and saturates.
//
//   r0 := SignedSaturate(a0)
//   r1 := SignedSaturate(a1)
//   r2 := SignedSaturate(a2)
//   r3 := SignedSaturate(a3)
//   r4 := SignedSaturate(b0)
//   r5 := SignedSaturate(b1)
//   r6 := SignedSaturate(b2)
//   r7 := SignedSaturate(b3)
//
// https://msdn.microsoft.com/en-us/library/393t56f9%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_packs_epi32(__m128i a, __m128i b)
{
    return vreinterpretq_m128i_s16(
        vcombine_s16(vqmovn_s32(vreinterpretq_s32_m128i(a)),
                     vqmovn_s32(vreinterpretq_s32_m128i(b))));
}

// Interleaves the lower 8 signed or unsigned 8-bit integers in a with the lower
// 8 signed or unsigned 8-bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//   ...
//   r14 := a7
//   r15 := b7
//
// https://msdn.microsoft.com/en-us/library/xf7k860c%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_unpacklo_epi8(__m128i a, __m128i b)
{
    int8x8_t a1 = vreinterpret_s8_s16(vget_low_s16(vreinterpretq_s16_m128i(a)));
    int8x8_t b1 = vreinterpret_s8_s16(vget_low_s16(vreinterpretq_s16_m128i(b)));
    int8x8x2_t result = vzip_s8(a1, b1);
    return vreinterpretq_m128i_s8(vcombine_s8(result.val[0], result.val[1]));
}

// Interleaves the lower 4 signed or unsigned 16-bit integers in a with the
// lower 4 signed or unsigned 16-bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//   r4 := a2
//   r5 := b2
//   r6 := a3
//   r7 := b3
//
// https://msdn.microsoft.com/en-us/library/btxb17bw%28v=vs.90%29.aspx
FORCE_INLINE __m128i _mm_unpacklo_epi16(__m128i a, __m128i b)
{
    int16x4_t a1 = vget_low_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b1 = vget_low_s16(vreinterpretq_s16_m128i(b));
    int16x4x2_t result = vzip_s16(a1, b1);
    return vreinterpretq_m128i_s16(vcombine_s16(result.val[0], result.val[1]));
}

// Interleaves the lower 2 signed or unsigned 32 - bit integers in a with the
// lower 2 signed or unsigned 32 - bit integers in b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//
// https://msdn.microsoft.com/en-us/library/x8atst9d(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpacklo_epi32(__m128i a, __m128i b)
{
    int32x2_t a1 = vget_low_s32(vreinterpretq_s32_m128i(a));
    int32x2_t b1 = vget_low_s32(vreinterpretq_s32_m128i(b));
    int32x2x2_t result = vzip_s32(a1, b1);
    return vreinterpretq_m128i_s32(vcombine_s32(result.val[0], result.val[1]));
}

FORCE_INLINE __m128i _mm_unpacklo_epi64(__m128i a, __m128i b)
{
    int64x1_t a_l = vget_low_s64(vreinterpretq_s64_m128i(a));
    int64x1_t b_l = vget_low_s64(vreinterpretq_s64_m128i(b));
    return vreinterpretq_m128i_s64(vcombine_s64(a_l, b_l));
}

// Selects and interleaves the lower two single-precision, floating-point values
// from a and b.
//
//   r0 := a0
//   r1 := b0
//   r2 := a1
//   r3 := b1
//
// https://msdn.microsoft.com/en-us/library/25st103b%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_unpacklo_ps(__m128 a, __m128 b)
{
    float32x2_t a1 = vget_low_f32(vreinterpretq_f32_m128(a));
    float32x2_t b1 = vget_low_f32(vreinterpretq_f32_m128(b));
    float32x2x2_t result = vzip_f32(a1, b1);
    return vreinterpretq_m128_f32(vcombine_f32(result.val[0], result.val[1]));
}

// Selects and interleaves the upper two single-precision, floating-point values
// from a and b.
//
//   r0 := a2
//   r1 := b2
//   r2 := a3
//   r3 := b3
//
// https://msdn.microsoft.com/en-us/library/skccxx7d%28v=vs.90%29.aspx
FORCE_INLINE __m128 _mm_unpackhi_ps(__m128 a, __m128 b)
{
    float32x2_t a1 = vget_high_f32(vreinterpretq_f32_m128(a));
    float32x2_t b1 = vget_high_f32(vreinterpretq_f32_m128(b));
    float32x2x2_t result = vzip_f32(a1, b1);
    return vreinterpretq_m128_f32(vcombine_f32(result.val[0], result.val[1]));
}

// Interleaves the upper 8 signed or unsigned 8-bit integers in a with the upper
// 8 signed or unsigned 8-bit integers in b.
//
//   r0 := a8
//   r1 := b8
//   r2 := a9
//   r3 := b9
//   ...
//   r14 := a15
//   r15 := b15
//
// https://msdn.microsoft.com/en-us/library/t5h7783k(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi8(__m128i a, __m128i b)
{
    int8x8_t a1 =
        vreinterpret_s8_s16(vget_high_s16(vreinterpretq_s16_m128i(a)));
    int8x8_t b1 =
        vreinterpret_s8_s16(vget_high_s16(vreinterpretq_s16_m128i(b)));
    int8x8x2_t result = vzip_s8(a1, b1);
    return vreinterpretq_m128i_s8(vcombine_s8(result.val[0], result.val[1]));
}

// Interleaves the upper 4 signed or unsigned 16-bit integers in a with the
// upper 4 signed or unsigned 16-bit integers in b.
//
//   r0 := a4
//   r1 := b4
//   r2 := a5
//   r3 := b5
//   r4 := a6
//   r5 := b6
//   r6 := a7
//   r7 := b7
//
// https://msdn.microsoft.com/en-us/library/03196cz7(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi16(__m128i a, __m128i b)
{
    int16x4_t a1 = vget_high_s16(vreinterpretq_s16_m128i(a));
    int16x4_t b1 = vget_high_s16(vreinterpretq_s16_m128i(b));
    int16x4x2_t result = vzip_s16(a1, b1);
    return vreinterpretq_m128i_s16(vcombine_s16(result.val[0], result.val[1]));
}

// Interleaves the upper 2 signed or unsigned 32-bit integers in a with the
// upper 2 signed or unsigned 32-bit integers in b.
// https://msdn.microsoft.com/en-us/library/65sa7cbs(v=vs.100).aspx
FORCE_INLINE __m128i _mm_unpackhi_epi32(__m128i a, __m128i b)
{
    int32x2_t a1 = vget_high_s32(vreinterpretq_s32_m128i(a));
    int32x2_t b1 = vget_high_s32(vreinterpretq_s32_m128i(b));
    int32x2x2_t result = vzip_s32(a1, b1);
    return vreinterpretq_m128i_s32(vcombine_s32(result.val[0], result.val[1]));
}

// Interleaves the upper signed or unsigned 64-bit integer in a with the
// upper signed or unsigned 64-bit integer in b.
//
//   r0 := a1
//   r1 := b1
FORCE_INLINE __m128i _mm_unpackhi_epi64(__m128i a, __m128i b)
{
    int64x1_t a_h = vget_high_s64(vreinterpretq_s64_m128i(a));
    int64x1_t b_h = vget_high_s64(vreinterpretq_s64_m128i(b));
    return vreinterpretq_m128i_s64(vcombine_s64(a_h, b_h));
}

// shift to right
// https://msdn.microsoft.com/en-us/library/bb514041(v=vs.120).aspx
// http://blog.csdn.net/hemmingway/article/details/44828303
FORCE_INLINE __m128i _mm_alignr_epi8(__m128i a, __m128i b, const int c)
{
    return (__m128i) vextq_s8((int8x16_t) a, (int8x16_t) b, c);
}

// Extracts the selected signed or unsigned 16-bit integer from a and zero
// extends.  https://msdn.microsoft.com/en-us/library/6dceta0c(v=vs.100).aspx
// FORCE_INLINE int _mm_extract_epi16(__m128i a, __constrange(0,8) int imm)
#define _mm_extract_epi16(a, imm) \
    ({ (vgetq_lane_s16(vreinterpretq_s16_m128i(a), (imm)) & 0x0000ffffUL); })

// Inserts the least significant 16 bits of b into the selected 16-bit integer
// of a. https://msdn.microsoft.com/en-us/library/kaze8hz1%28v=vs.100%29.aspx
// FORCE_INLINE __m128i _mm_insert_epi16(__m128i a, const int b,
// __constrange(0,8) int imm)
#define _mm_insert_epi16(a, b, imm)                                  \
    ({                                                               \
        vreinterpretq_m128i_s16(                                     \
            vsetq_lane_s16((b), vreinterpretq_s16_m128i(a), (imm))); \
    })

// ******************************************
// Crypto Extensions
// ******************************************
#if !defined(__ARM_FEATURE_CRYPTO) && defined(__aarch64__)
// In the absence of crypto extensions, implement aesenc using regular neon
// intrinsics instead. See:
// http://www.workofard.com/2017/01/accelerated-aes-for-the-arm64-linux-kernel/
// http://www.workofard.com/2017/07/ghash-for-low-end-cores/ and
// https://github.com/ColinIanKing/linux-next-mirror/blob/b5f466091e130caaf0735976648f72bd5e09aa84/crypto/aegis128-neon-inner.c#L52
// for more information Reproduced with permission of the author.
FORCE_INLINE __m128i _mm_aesenc_si128(__m128i EncBlock, __m128i RoundKey)
{
    static const uint8_t crypto_aes_sbox[256] = {
        0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
        0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
        0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
        0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
        0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
        0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
        0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
        0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
        0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
        0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
        0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
        0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
        0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
        0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
        0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
        0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
        0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
        0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
        0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
        0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
        0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
        0xb0, 0x54, 0xbb, 0x16};
    static const uint8_t shift_rows[] = {0x0, 0x5, 0xa, 0xf, 0x4, 0x9,
                                         0xe, 0x3, 0x8, 0xd, 0x2, 0x7,
                                         0xc, 0x1, 0x6, 0xb};
    static const uint8_t ror32by8[] = {0x1, 0x2, 0x3, 0x0, 0x5, 0x6, 0x7, 0x4,
                                       0x9, 0xa, 0xb, 0x8, 0xd, 0xe, 0xf, 0xc};

    uint8x16_t v;
    uint8x16_t w = vreinterpretq_u8_m128i(EncBlock);

    // shift rows
    w = vqtbl1q_u8(w, vld1q_u8(shift_rows));

    // sub bytes
    v = vqtbl4q_u8(vld1q_u8_x4(crypto_aes_sbox), w);
    v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0x40), w - 0x40);
    v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0x80), w - 0x80);
    v = vqtbx4q_u8(v, vld1q_u8_x4(crypto_aes_sbox + 0xc0), w - 0xc0);

    // mix columns
    w = (v << 1) ^ (uint8x16_t)(((int8x16_t) v >> 7) & 0x1b);
    w ^= (uint8x16_t) vrev32q_u16((uint16x8_t) v);
    w ^= vqtbl1q_u8(v ^ w, vld1q_u8(ror32by8));

    //  add round key
    return vreinterpretq_m128i_u8(w) ^ RoundKey;
}
#elif defined(__ARM_FEATURE_CRYPTO)
// Implements equivalent of 'aesenc' by combining AESE (with an empty key) and
// AESMC and then manually applying the real key as an xor operation This
// unfortunately means an additional xor op; the compiler should be able to
// optimise this away for repeated calls however See
// https://blog.michaelbrase.com/2018/05/08/emulating-x86-aes-intrinsics-on-armv8-a
// for more details.
inline __m128i _mm_aesenc_si128(__m128i a, __m128i b)
{
    return vreinterpretq_s32_u8(
        vaesmcq_u8(vaeseq_u8(vreinterpretq_u8_s32(a), uint8x16_t{})) ^
        vreinterpretq_u8_s32(b));
}
#endif

// ******************************************
// Streaming Extensions
// ******************************************

// Guarantees that every preceding store is globally visible before any
// subsequent store.
// https://msdn.microsoft.com/en-us/library/5h2w73d1%28v=vs.90%29.aspx
FORCE_INLINE void _mm_sfence(void)
{
    __sync_synchronize();
}

// Stores the data in a to the address p without polluting the caches.  If the
// cache line containing address p is already in the cache, the cache will be
// updated.Address p must be 16 - byte aligned.
// https://msdn.microsoft.com/en-us/library/ba08y07y%28v=vs.90%29.aspx
FORCE_INLINE void _mm_stream_si128(__m128i *p, __m128i a)
{
    vst1q_s32((int32_t *) p, a);
}

// Cache line containing p is flushed and invalidated from all caches in the
// coherency domain. :
// https://msdn.microsoft.com/en-us/library/ba08y07y(v=vs.100).aspx
FORCE_INLINE void _mm_clflush(void const *p)
{
    // no corollary for Neon?
}

#if defined(__GNUC__) || defined(__clang__)
#pragma pop_macro("ALIGN_STRUCT")
#pragma pop_macro("FORCE_INLINE")
#endif

#endif
