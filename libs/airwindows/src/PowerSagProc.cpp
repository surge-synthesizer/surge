/* ========================================
 *  PowerSag - PowerSag.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PowerSag_H
#include "PowerSag.h"
#endif

namespace PowerSag
{

void PowerSag::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float *in1 = inputs[0];
    float *in2 = inputs[1];
    float *out1 = outputs[0];
    float *out2 = outputs[1];

    double intensity = pow(A, 5) * 80.0;
    double depthA = pow(B, 2);
    int offsetA = (int)(depthA * 3900) + 1;
    double clamp;
    double thickness;
    double out;
    double bridgerectifier;

    double inputSampleL;
    double inputSampleR;

    while (--sampleFrames >= 0)
    {
        inputSampleL = *in1;
        inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-23)
            inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR) < 1.18e-23)
            inputSampleR = fpdR * 1.18e-17;

        if (gcount < 0 || gcount > 4000)
        {
            gcount = 4000;
        }

        // doing L
        dL[gcount + 4000] = dL[gcount] = fabs(inputSampleL) * intensity;
        controlL += (dL[gcount] / offsetA);
        controlL -= (dL[gcount + offsetA] / offsetA);
        controlL -= 0.000001;
        clamp = 1;
        if (controlL < 0)
        {
            controlL = 0;
        }
        if (controlL > 1)
        {
            clamp -= (controlL - 1);
            controlL = 1;
        }
        if (clamp < 0.5)
        {
            clamp = 0.5;
        }
        thickness = ((1.0 - controlL) * 2.0) - 1.0;
        out = fabs(thickness);
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        if (thickness > 0)
            bridgerectifier = sin(bridgerectifier);
        else
            bridgerectifier = 1 - cos(bridgerectifier);
        // produce either boosted or starved version
        if (inputSampleL > 0)
            inputSampleL = (inputSampleL * (1 - out)) + (bridgerectifier * out);
        else
            inputSampleL = (inputSampleL * (1 - out)) - (bridgerectifier * out);
        // blend according to density control
        inputSampleL *= clamp;
        // end L

        // doing R
        dR[gcount + 4000] = dR[gcount] = fabs(inputSampleR) * intensity;
        controlR += (dR[gcount] / offsetA);
        controlR -= (dR[gcount + offsetA] / offsetA);
        controlR -= 0.000001;
        clamp = 1;
        if (controlR < 0)
        {
            controlR = 0;
        }
        if (controlR > 1)
        {
            clamp -= (controlR - 1);
            controlR = 1;
        }
        if (clamp < 0.5)
        {
            clamp = 0.5;
        }
        thickness = ((1.0 - controlR) * 2.0) - 1.0;
        out = fabs(thickness);
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        if (thickness > 0)
            bridgerectifier = sin(bridgerectifier);
        else
            bridgerectifier = 1 - cos(bridgerectifier);
        // produce either boosted or starved version
        if (inputSampleR > 0)
            inputSampleR = (inputSampleR * (1 - out)) + (bridgerectifier * out);
        else
            inputSampleR = (inputSampleR * (1 - out)) - (bridgerectifier * out);
        // blend according to density control
        inputSampleR *= clamp;
        // end R

        gcount--;

        // begin 32 bit stereo floating point dither
        int expon;
        frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13;
        fpdL ^= fpdL >> 17;
        fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13;
        fpdR ^= fpdR >> 17;
        fpdR ^= fpdR << 5;
        inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        // end 32 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

void PowerSag::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double *in1 = inputs[0];
    double *in2 = inputs[1];
    double *out1 = outputs[0];
    double *out2 = outputs[1];

    double intensity = pow(A, 5) * 80.0;
    double depthA = pow(B, 2);
    int offsetA = (int)(depthA * 3900) + 1;
    double clamp;
    double thickness;
    double out;
    double bridgerectifier;

    double inputSampleL;
    double inputSampleR;

    while (--sampleFrames >= 0)
    {
        inputSampleL = *in1;
        inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-23)
            inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR) < 1.18e-23)
            inputSampleR = fpdR * 1.18e-17;

        if (gcount < 0 || gcount > 4000)
        {
            gcount = 4000;
        }

        // doing L
        dL[gcount + 4000] = dL[gcount] = fabs(inputSampleL) * intensity;
        controlL += (dL[gcount] / offsetA);
        controlL -= (dL[gcount + offsetA] / offsetA);
        controlL -= 0.000001;
        clamp = 1;
        if (controlL < 0)
        {
            controlL = 0;
        }
        if (controlL > 1)
        {
            clamp -= (controlL - 1);
            controlL = 1;
        }
        if (clamp < 0.5)
        {
            clamp = 0.5;
        }
        thickness = ((1.0 - controlL) * 2.0) - 1.0;
        out = fabs(thickness);
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        if (thickness > 0)
            bridgerectifier = sin(bridgerectifier);
        else
            bridgerectifier = 1 - cos(bridgerectifier);
        // produce either boosted or starved version
        if (inputSampleL > 0)
            inputSampleL = (inputSampleL * (1 - out)) + (bridgerectifier * out);
        else
            inputSampleL = (inputSampleL * (1 - out)) - (bridgerectifier * out);
        // blend according to density control
        inputSampleL *= clamp;
        // end L

        // doing R
        dR[gcount + 4000] = dR[gcount] = fabs(inputSampleR) * intensity;
        controlR += (dR[gcount] / offsetA);
        controlR -= (dR[gcount + offsetA] / offsetA);
        controlR -= 0.000001;
        clamp = 1;
        if (controlR < 0)
        {
            controlR = 0;
        }
        if (controlR > 1)
        {
            clamp -= (controlR - 1);
            controlR = 1;
        }
        if (clamp < 0.5)
        {
            clamp = 0.5;
        }
        thickness = ((1.0 - controlR) * 2.0) - 1.0;
        out = fabs(thickness);
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        if (thickness > 0)
            bridgerectifier = sin(bridgerectifier);
        else
            bridgerectifier = 1 - cos(bridgerectifier);
        // produce either boosted or starved version
        if (inputSampleR > 0)
            inputSampleR = (inputSampleR * (1 - out)) + (bridgerectifier * out);
        else
            inputSampleR = (inputSampleR * (1 - out)) - (bridgerectifier * out);
        // blend according to density control
        inputSampleR *= clamp;
        // end R

        gcount--;

        // begin 64 bit stereo floating point dither
        // int expon; frexp((double)inputSampleL, &expon);
        fpdL ^= fpdL << 13;
        fpdL ^= fpdL >> 17;
        fpdL ^= fpdL << 5;
        // inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        // frexp((double)inputSampleR, &expon);
        fpdR ^= fpdR << 13;
        fpdR ^= fpdR >> 17;
        fpdR ^= fpdR << 5;
        // inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        // end 64 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

} // end namespace PowerSag
