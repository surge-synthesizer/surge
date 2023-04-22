/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_DSP_VEMBERTECH_LIPOL_H
#define SURGE_SRC_COMMON_DSP_VEMBERTECH_LIPOL_H

#include "globals.h"

/*
** lipol_ps is a small utility class for generating small line segments
** between values
**
** usage would be
**
** ```
** lipol_ps mypol;
**
** ...
**
** mypol.set_target(13.23);
** if( init )
**   mypol.instantize();
** ```
**
** then later in the code
**
** ```
** float values alignas(16)[SIZE]
** mypol.store_block(values, SIZE_OVER_FOUR);
** ```
**
** and block would contain the linear interpolation between the
** last queried value and the target.
*/
class lipol_ps
{
  public:
    __m128 target, currentval, coef, coef_m1;
    __m128 lipol_BLOCK_SIZE;
    __m128 m128_lipolstarter;
    __m128 m128_bs4_inv;

    lipol_ps();

    inline void set_target(float t)
    {
        currentval = target;
        target = _mm_set_ss(t);
    }

    inline void set_target(__m128 t)
    {
        currentval = target;
        target = t;
    }
    inline void set_target_instantize(float t)
    {
        target = _mm_set_ss(t);
        currentval = target;
    }
    inline void set_target_smoothed(float t)
    {
        currentval = target;
        __m128 p1 = _mm_mul_ss(coef, _mm_set_ss(t));
        __m128 p2 = _mm_mul_ss(coef_m1, target);
        target = _mm_add_ss(p1, p2);
    }
    void set_blocksize(int bs);

    // inline void set_target(__m128 t) { currentval = target; target = t; }
    inline void instantize() { currentval = target; }
    float get_target()
    {
        float f;
        _mm_store_ss(&f, target);
        return f;
    }
    void store_block(float *dst, unsigned int nquads);
    void multiply_block(float *src, unsigned int nquads);
    void multiply_block_sat1(
        float *src,
        unsigned int nquads); // saturates the interpolator each step (for amp envelopes)
    void add_block(float *src, unsigned int nquads);
    void fade_block_to(float *src1, float *src2, float *dst, unsigned int nquads);
    void fade_2_blocks_to(float *src11, float *src12, float *src21, float *src22, float *dst1,
                          float *dst2, unsigned int nquads);
    void subtract_block(float *src, unsigned int nquads);
    void trixpan_blocks(float *L, float *R, float *dL, float *dR,
                        unsigned int nquads); // panning that always lets both channels through
                                              // unattenuated (separate hard-panning)
    void multiply_block_to(float *src, float *dst, unsigned int nquads);
    void multiply_2_blocks(float *src1, float *src2, unsigned int nquads);
    void multiply_2_blocks_to(float *src1, float *src2, float *dst1, float *dst2,
                              unsigned int nquads);
    void MAC_block_to(float *src, float *dst, unsigned int nquads);
    void MAC_2_blocks_to(float *src1, float *src2, float *dst1, float *dst2, unsigned int nquads);

  private:
    inline void initblock(__m128 &y, __m128 &dy)
    {
        dy = _mm_sub_ss(target, currentval);
        dy = _mm_mul_ss(dy, m128_bs4_inv);
        dy = _mm_shuffle_ps(dy, dy, _MM_SHUFFLE(0, 0, 0, 0));
        y = _mm_shuffle_ps(currentval, currentval, _MM_SHUFFLE(0, 0, 0, 0));
        y = _mm_add_ps(y, _mm_mul_ps(dy, m128_lipolstarter));
    }
};

#endif // SURGE_SRC_COMMON_DSP_VEMBERTECH_LIPOL_H
