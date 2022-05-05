#include <math.h>
#include "BiquadFilter.h"
#include "globals.h"
#include <complex>

using namespace std;

inline double square(double x) { return x * x; }

BiquadFilter::BiquadFilter()
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

BiquadFilter::BiquadFilter(SurgeStorage *storage)
{
    this->storage = storage;
    reg0.d[0] = 0;
    reg1.d[0] = 0;
    reg0.d[1] = 0;
    reg1.d[1] = 0;
    first_run = true;

    /*if(storage->SSE2)
    {
            a1.init_SSE2();
            a2.init_SSE2();
            b0.init_SSE2();
            b1.init_SSE2();
            b2.init_SSE2();
    }
    else*/
    {
        a1.init_x87();
        a2.init_x87();
        b0.init_x87();
        b1.init_x87();
        b2.init_x87();
    }
}

void BiquadFilter::coeff_LP(double omega, double Q)
{
    if (omega > M_PI)
        set_coef(1, 0, 0, 1, 0, 0);
    else
    {
        double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2 * Q), b0 = (1 - cosi) * 0.5,
               b1 = 1 - cosi, b2 = (1 - cosi) * 0.5, a0 = 1 + alpha, a1 = -2 * cosi, a2 = 1 - alpha;

        set_coef(a0, a1, a2, b0, b1, b2);
    }
}

void BiquadFilter::coeff_LP2B(double omega, double Q)
{
    if (omega > M_PI)
        set_coef(1, 0, 0, 1, 0, 0);
    else
    {
        double w_sq = omega * omega;
        double den =
            (w_sq * w_sq) + (M_PI * M_PI * M_PI * M_PI) + w_sq * (M_PI * M_PI) * (1 / Q - 2);
        double G1 = min(1.0, sqrt((w_sq * w_sq) / den) * 0.5);

        double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2 * Q),

               A = 2 * sqrt(G1) * sqrt(2 - G1), b0 = (1 - cosi + G1 * (1 + cosi) + A * sinu) * 0.5,
               b1 = (1 - cosi - G1 * (1 + cosi)),
               b2 = (1 - cosi + G1 * (1 + cosi) - A * sinu) * 0.5, a0 = (1 + alpha), a1 = -2 * cosi,
               a2 = 1 - alpha;

        set_coef(a0, a1, a2, b0, b1, b2);
    }
}

void BiquadFilter::coeff_HP(double omega, double Q)
{
    if (omega > M_PI)
        set_coef(1, 0, 0, 0, 0, 0);
    else
    {
        double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2 * Q), b0 = (1 + cosi) * 0.5,
               b1 = -(1 + cosi), b2 = (1 + cosi) * 0.5, a0 = 1 + alpha, a1 = -2 * cosi,
               a2 = 1 - alpha;

        set_coef(a0, a1, a2, b0, b1, b2);
    }
}

void BiquadFilter::coeff_BP(double omega, double Q)
{
    double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2.0 * Q), b0 = alpha, b2 = -alpha,
           a0 = 1 + alpha, a1 = -2 * cosi, a2 = 1 - alpha;

    set_coef(a0, a1, a2, b0, 0, b2);
}

void BiquadFilter::coeff_BP2A(double omega, double BW)
{
    double cosi = cos(omega), sinu = sin(omega), q = 1 / (0.02 + 30 * BW * BW),
           alpha = sinu / (2 * q), b0 = alpha, b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cosi,
           a2 = 1 - alpha;

    set_coef(a0, a1, a2, b0, 0, b2);
}

void BiquadFilter::coeff_PKA(double omega, double QQ)
{
    double cosi = cos(omega), sinu = sin(omega), reso = limit_range(QQ, 0.0, 1.0),
           q = 0.1 + 10 * reso * reso, alpha = sinu / (2 * q), b0 = q * alpha, b2 = -q * alpha,
           a0 = 1 + alpha, a1 = -2 * cosi, a2 = 1 - alpha;

    set_coef(a0, a1, a2, b0, 0, b2);
}

void BiquadFilter::coeff_NOTCH(double omega, double QQ)
{
    if (omega > M_PI)
        set_coef(1, 0, 0, 1, 0, 0);
    else
    {
        double cosi = cos(omega), sinu = sin(omega), reso = limit_range(QQ, 0.0, 1.0),
               q = 1 / (0.02 + 30 * reso * reso), alpha = sinu / (2 * q), b0 = 1.0,
               b1 = -2.0 * cosi, b2 = 1.0, a0 = 1.0 + alpha, a1 = -2.0 * cosi, a2 = 1.0 - alpha;

        set_coef(a0, a1, a2, b0, b1, b2);
    }
}

void BiquadFilter::coeff_LP_with_BW(double omega, double BW) { coeff_LP(omega, 1 / BW); }

void BiquadFilter::coeff_HP_with_BW(double omega, double BW) { coeff_HP(omega, 1 / BW); }

void BiquadFilter::coeff_LPHPmorph(double omega, double Q, double morph)
{
    double HP = limit_range(morph, 0.0, 1.0), LP = 1 - HP, BP = LP * HP;
    HP *= HP;
    LP *= LP;

    double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2 * Q), b0 = alpha, b1 = 0,
           b2 = -alpha, a0 = 1 + alpha, a1 = -2 * cosi, a2 = 1 - alpha;

    set_coef(a0, a1, a2, b0, b1, b2);
}

void BiquadFilter::coeff_APF(double omega, double Q)
{
    if ((omega < 0.0) || (omega > M_PI))
        set_coef(1, 0, 0, 1, 0, 0);
    else
    {
        double cosi = cos(omega), sinu = sin(omega), alpha = sinu / (2 * Q), b0 = (1 - alpha),
               b1 = -2 * cosi, b2 = (1 + alpha), a0 = (1 + alpha), a1 = -2 * cosi, a2 = (1 - alpha);

        set_coef(a0, a1, a2, b0, b1, b2);
    }
}

void BiquadFilter::coeff_peakEQ(double omega, double BW, double gain)
{
    coeff_orfanidisEQ(omega, BW, storage->db_to_linear(gain), storage->db_to_linear(gain * 0.5), 1);
}

void BiquadFilter::coeff_orfanidisEQ(double omega, double BW, double G, double GB, double G0)
{
    // For the curious http://eceweb1.rutgers.edu/~orfanidi/ece521/hpeq.pdf appears to be the source
    // of this
    double limit = 0.95;
    double w0 = omega; // min(M_PI-0.000001,omega);
    BW = max(minBW, BW);
    double Dww = 2 * w0 * sinh((log(2.0) / 2.0) * BW); // sinh = (e^x - e^-x)/2

    double gainscale = 1;
    // if(omega>M_PI) gainscale = 1 / (1 + (omega-M_PI)*(4/Dw));

    if (abs(G - G0) > 0.00001)
    {
        double F = abs(G * G - GB * GB);
        double G00 = abs(G * G - G0 * G0);
        double F00 = abs(GB * GB - G0 * G0);
        double num =
            G0 * G0 * square(w0 * w0 - (M_PI * M_PI)) + G * G * F00 * (M_PI * M_PI) * Dww * Dww / F;
        double den = square(w0 * w0 - M_PI * M_PI) + F00 * M_PI * M_PI * Dww * Dww / F;
        double G1 = sqrt(num / den);

        if (omega > M_PI)
        {
            G = G1 * 0.9999;
            w0 = M_PI - 0.00001;
            G00 = abs(G * G - G0 * G0);
            F00 = abs(GB * GB - G0 * G0);
        }

        double G01 = abs(G * G - G0 * G1);
        double G11 = abs(G * G - G1 * G1);
        double F01 = abs(GB * GB - G0 * G1);
        double F11 = abs(GB * GB - G1 * G1); // blir wacko ?
                                             // goes crazy (?)
        double W2 = sqrt(G11 / G00) * square(tan(w0 / 2));
        double w_lower = w0 * powf(2, -0.5 * BW);
        double w_upper =
            2 * atan(sqrt(F00 / F11) * sqrt(G11 / G00) * square(tan(w0 / 2)) / tan(w_lower / 2));
        double Dw = abs(w_upper - w_lower);
        double DW = (1 + sqrt(F00 / F11) * W2) * tan(Dw / 2);

        double C = F11 * DW * DW - 2 * W2 * (F01 - sqrt(F00 * F11));
        double D = 2 * W2 * (G01 - sqrt(G00 * G11));
        double A = sqrt((C + D) / F);
        double B = sqrt((G * G * C + GB * GB * D) / F);
        double a0 = (1 + W2 + A), a1 = -2 * (1 - W2), a2 = (1 + W2 - A), b0 = (G1 + G0 * W2 + B),
               b1 = -2 * (G1 - G0 * W2), b2 = (G1 - B + G0 * W2);

        // if (c) sprintf(c,"G0: %f G: %f GB: %f G1: %f
        // ",linear_to_dB(G0),linear_to_dB(G),linear_to_dB(GB),linear_to_dB(G1));

        set_coef(a0, a1, a2, b0, b1, b2);
    }
    else
    {
        set_coef(1, 0, 0, 1, 0, 0);
    }
}

void BiquadFilter::coeff_same_as_last_time()
{
    // If you want to change interpolation then set dv = 0 here
}

void BiquadFilter::coeff_instantize()
{
    a1.instantize();
    a2.instantize();
    b0.instantize();
    b1.instantize();
    b2.instantize();
}

void BiquadFilter::set_coef(double a0, double a1, double a2, double b0, double b1, double b2)
{
    double a0inv = 1 / a0;

    b0 *= a0inv;
    b1 *= a0inv;
    b2 *= a0inv;
    a1 *= a0inv;
    a2 *= a0inv;
    if (first_run)
    {
        this->a1.startValue(a1);
        this->a2.startValue(a2);
        this->b0.startValue(b0);
        this->b1.startValue(b1);
        this->b2.startValue(b2);
        first_run = false;
    }
    this->a1.newValue(a1);
    this->a2.newValue(a2);
    this->b0.newValue(b0);
    this->b1.newValue(b1);
    this->b2.newValue(b2);
}

void BiquadFilter::process_block(float *data)
{
    /*if(storage->SSE2) process_block_SSE2(data);
    else*/
    {
        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            a1.process();
            a2.process();
            b0.process();
            b1.process();
            b2.process();

            double input = data[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            data[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
    }
}

void BiquadFilter::process_block_to(float *__restrict data, float *__restrict dataout)
{
    /*if(storage->SSE2) process_block_SSE2(data);
    else*/
    {
        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            a1.process();
            a2.process();
            b0.process();
            b1.process();
            b2.process();

            double input = data[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            dataout[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
    }
}

void BiquadFilter::process_block_slowlag(float *__restrict dataL, float *__restrict dataR)
{
    /*if(storage->SSE2) process_block_slowlag_SSE2(dataL,dataR);
    else*/
    {
        a1.process();
        a2.process();
        b0.process();
        b1.process();
        b2.process();

        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            double input = dataL[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            dataL[k] = op;

            input = dataR[k];
            op = input * b0.v.d[0] + reg0.d[1];
            reg0.d[1] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
            reg1.d[1] = input * b2.v.d[0] - a2.v.d[0] * op;

            dataR[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
        flush_denormal(reg0.d[1]);
        flush_denormal(reg1.d[1]);
    }
}

void BiquadFilter::process_block(float *dataL, float *dataR)
{
    /*if(storage->SSE2) process_block_SSE2(dataL,dataR);
    else*/
    {
        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            a1.process();
            a2.process();
            b0.process();
            b1.process();
            b2.process();

            double input = dataL[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            dataL[k] = op;

            input = dataR[k];
            op = input * b0.v.d[0] + reg0.d[1];
            reg0.d[1] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
            reg1.d[1] = input * b2.v.d[0] - a2.v.d[0] * op;

            dataR[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
        flush_denormal(reg0.d[1]);
        flush_denormal(reg1.d[1]);
    }
}

void BiquadFilter::process_block_to(float *dataL, float *dataR, float *dstL, float *dstR)
{
    /*if(storage->SSE2) process_block_to_SSE2(dataL,dataR,dstL,dstR);
    else*/
    {
        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            a1.process();
            a2.process();
            b0.process();
            b1.process();
            b2.process();

            double input = dataL[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            dstL[k] = op;

            input = dataR[k];
            op = input * b0.v.d[0] + reg0.d[1];
            reg0.d[1] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[1];
            reg1.d[1] = input * b2.v.d[0] - a2.v.d[0] * op;

            dstR[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
        flush_denormal(reg0.d[1]);
        flush_denormal(reg1.d[1]);
    }
}

void BiquadFilter::process_block(double *data)
{
    /*if(storage->SSE2) process_block_SSE2(data);
    else*/
    {
        int k;
        for (k = 0; k < BLOCK_SIZE; k++)
        {
            a1.process();
            a2.process();
            b0.process();
            b1.process();
            b2.process();

            double input = data[k];
            double op;

            op = input * b0.v.d[0] + reg0.d[0];
            reg0.d[0] = input * b1.v.d[0] - a1.v.d[0] * op + reg1.d[0];
            reg1.d[0] = input * b2.v.d[0] - a2.v.d[0] * op;

            data[k] = op;
        }
        flush_denormal(reg0.d[0]);
        flush_denormal(reg1.d[0]);
    }
}

void BiquadFilter::setBlockSize(int bs)
{
    /*	a1.setBlockSize(bs);
            a2.setBlockSize(bs);
            b0.setBlockSize(bs);
            b1.setBlockSize(bs);
            b2.setBlockSize(bs);*/
}

using namespace std;

float BiquadFilter::plot_magnitude(float f)
{
    complex<double> ca0(1, 0), ca1(a1.v.d[0], 0), ca2(a2.v.d[0], 0), cb0(b0.v.d[0], 0),
        cb1(b1.v.d[0], 0), cb2(b2.v.d[0], 0);

    complex<double> i(0, 1);
    complex<double> z = exp(-2 * 3.1415 * f * i);

    complex<double> h = (cb0 + cb1 * z + cb2 * z * z) / (ca0 + ca1 * z + ca2 * z * z);

    double r = abs(h);
    return r;
}
