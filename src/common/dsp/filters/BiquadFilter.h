#pragma once

#include "DSPUtils.h"
#include <math.h>
#include "SurgeStorage.h"

const double minBW = 0.0001;

/*const __m128d vl_lp = _mm_set1_pd(0.004);
const __m128d vl_lpinv = _mm_set1_pd(1.0 - 0.004);*/
const double d_lp = 0.004;
const double d_lpinv = 1.0 - 0.004;

union vdouble
{
    //__m128d v;
    double d[2];
};

class vlag
{
  public:
    vdouble v alignas(16), target_v alignas(16);
    vlag() {}
    void init_x87()
    {
        v.d[0] = 0;
        v.d[1] = 0;
        target_v.d[0] = 0;
        target_v.d[1] = 0;
    }
    /*void init_SSE2()
    {
            v.v = _mm_setzero_pd();
            target_v.v = _mm_setzero_pd();
    }*/

    inline void process() { v.d[0] = v.d[0] * d_lpinv + target_v.d[0] * d_lp; }
    /*inline void process_SSE2()
    {
            v.v = _mm_add_sd(_mm_mul_sd(v.v,vl_lpinv),_mm_mul_sd(target_v.v,vl_lp));
            v.v = _mm_unpacklo_pd(v.v,v.v);
    }*/

    inline void newValue(double f) { target_v.d[0] = f; }
    inline void instantize() { v = target_v; }
    inline void startValue(double f)
    {
        target_v.d[0] = f;
        v.d[0] = f;
    }
};

class alignas(16) BiquadFilter
{
    // alignas(16) lag<double,false> a1,a2,b0,b1,b2;
    vlag a1 alignas(16), a2 alignas(16), b0 alignas(16), b1 alignas(16), b2 alignas(16);
    vdouble reg0 alignas(16), reg1 alignas(16);
    // alignas(16) double reg0R,reg1R;
  public:
    BiquadFilter();
    BiquadFilter(SurgeStorage *storage);
    void coeff_LP(double omega, double Q);
    /** Compared to coeff_LP, this version adds a small boost at high frequencies */
    void coeff_LP2B(double omega, double Q);
    void coeff_HP(double omega, double Q);
    void coeff_BP(double omega, double Q);
    void coeff_LP_with_BW(double omega, double BW);
    void coeff_HP_with_BW(double omega, double BW);
    void coeff_BP2A(double omega, double Q);
    void coeff_PKA(double omega, double Q);
    void coeff_NOTCH(double omega, double Q);
    void coeff_peakEQ(double omega, double BW, double gain);
    void coeff_LPHPmorph(double omega, double Q, double morph);
    void coeff_APF(double omega, double Q);
    void coeff_orfanidisEQ(double omega, double BW, double pgaindb, double bgaindb, double zgain);
    void coeff_same_as_last_time();
    void coeff_instantize();

    void process_block(float *data);
    // void process_block_SSE2(float *data);
    void process_block(float *dataL, float *dataR);
    // void process_block_SSE2(float *dataL,float *dataR);
    void process_block_to(float *, float *);
    void process_block_to(float *dataL, float *dataR, float *dstL, float *dstR);
    // void process_block_to_SSE2(float *dataL,float *dataR, float *dstL,float *dstR);
    void process_block_slowlag(float *dataL, float *dataR);
    // void process_block_slowlag_SSE2(float *dataL,float *dataR);
    void process_block(double *data);
    // void process_block_SSE2(double *data);

    inline float process_sample(float input)
    {
        a1.process();
        a2.process();
        b0.process();
        b1.process();
        b2.process();

        double op;

        op = input * b0.v.d[0] + reg0.d[0];
        reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
        reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

        return (float)op;
    }

    inline void process_sample(float L, float R, float &lOut, float &rOut)
    {
        a1.process();
        a2.process();
        b0.process();
        b1.process();
        b2.process();

        double op;
        double input = L;
        op = input * b0.v.d[0] + reg0.d[0];
        reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
        reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

        lOut = op;

        input = R;
        op = input * b0.v.d[0] + reg0.d[1];
        reg0.d[1] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
        reg1.d[1] = input * b2.v.d[0] - a2.v.d[0] * op;

        rOut = op;
    }

    inline void process_sample_nolag(float &L, float &R)
    {
        double op;

        op = L * b0.v.d[0] + reg0.d[0];
        reg0.d[0] = L * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
        reg1.d[0] = L * b2.v.d[0] - a2.v.d[0] * op;
        L = (float)op;

        op = R * b0.v.d[0] + reg0.d[1];
        reg0.d[1] = R * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
        reg1.d[1] = R * b2.v.d[0] - a2.v.d[0] * op;
        R = (float)op;
    }

    inline void process_sample_nolag(float &L, float &R, float &Lout, float &Rout)
    {
        double op;

        op = L * b0.v.d[0] + reg0.d[0];
        reg0.d[0] = L * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
        reg1.d[0] = L * b2.v.d[0] - a2.v.d[0] * op;
        Lout = (float)op;

        op = R * b0.v.d[0] + reg0.d[1];
        reg0.d[1] = R * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
        reg1.d[1] = R * b2.v.d[0] - a2.v.d[0] * op;
        Rout = (float)op;
    }

    inline void process_sample_nolag_noinput(float &Lout, float &Rout)
    {
        double op;

        op = reg0.d[0];
        reg0.d[0] = -a1.v.d[0] * op + reg1.d[0];
        reg1.d[0] = -a2.v.d[0] * op;
        Lout = (float)op;

        op = reg0.d[1];
        reg0.d[1] = -a1.v.d[0] * op + reg1.d[1];
        reg1.d[1] = -a2.v.d[0] * op;
        Rout = (float)op;
    }

    // static double calc_omega(double scfreq){ return (2*3.14159265358979323846) * min(0.499,
    // 440*powf(2,scfreq)*samplerate_inv); }
    double calc_omega(double scfreq)
    {
        return (2 * 3.14159265358979323846) * 440 *
               storage->note_to_pitch_ignoring_tuning((float)(12.f * scfreq)) *
               storage->dsamplerate_inv;
    }
    double calc_omega_from_Hz(double Hz)
    {
        return (2 * 3.14159265358979323846) * Hz * storage->dsamplerate_inv;
    }
    double calc_v1_Q(double reso) { return 1 / (1.02 - limit_range(reso, 0.0, 1.0)); }
    // inline void process_block_stereo(float *dataL,float *dataR);
    // inline void process_block(double *data);
    // inline double process_sample(double sample);
    void setBlockSize(int bs);
    void suspend()
    {
        reg0.d[0] = 0;
        reg1.d[0] = 0;
        reg0.d[1] = 0;
        reg1.d[1] = 0;
        first_run = true;
        a1.init_x87();
        a2.init_x87();
        b0.init_x87();
        b1.init_x87();
        b2.init_x87();
    }

    float plot_magnitude(float f);
    SurgeStorage *storage;

  protected:
    void set_coef(double a0, double a1, double a2, double b0, double b1, double b2);
    bool first_run;
};
