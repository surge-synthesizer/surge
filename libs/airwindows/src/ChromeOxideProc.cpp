/* ========================================
 *  ChromeOxide - ChromeOxide.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ChromeOxide_H
#include "ChromeOxide.h"
#endif

namespace ChromeOxide
{

void ChromeOxide::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float *in1 = inputs[0];
    float *in2 = inputs[1];
    float *out1 = outputs[0];
    float *out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double bassSampleL;
    double bassSampleR;
    double randy;
    double bias = B / 1.31578947368421;
    double intensity = 0.9 + pow(A, 2);
    double iirAmount = pow(1.0 - (intensity / (10 + (bias * 4.0))), 2) / overallscale;
    // make 10 higher for less trashy sound in high settings
    double bridgerectifier;
    double trebleGainTrim = 1.0;
    double indrive = 1.0;
    double densityA = (intensity * 80) + 1.0;
    double noise = intensity / (1.0 + bias);
    double bassGainTrim = 1.0;
    double glitch = 0.0;
    bias *= overallscale;
    noise *= overallscale;

    if (intensity > 1.0)
    {
        glitch = intensity - 1.0;
        indrive = intensity * intensity;
        bassGainTrim /= (intensity * intensity);
        trebleGainTrim = (intensity + 1.0) / 2.0;
    }
    // everything runs off Intensity now

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-37)
            inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR) < 1.18e-37)
            inputSampleR = fpd * 1.18e-37;

        inputSampleL *= indrive;
        inputSampleR *= indrive;
        bassSampleL = inputSampleL;
        bassSampleR = inputSampleR;

        if (flip)
        {
            iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleAL;
            inputSampleR -= iirSampleAR;
        }
        else
        {
            iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleBL;
            inputSampleR -= iirSampleBR;
        }
        // highpass section

        bassSampleL -=
            (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch));
        bassSampleR -=
            (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch));
        // overdrive the bass channel

        if (flip)
        {
            iirSampleCL = (iirSampleCL * (1 - iirAmount)) + (bassSampleL * iirAmount);
            iirSampleCR = (iirSampleCR * (1 - iirAmount)) + (bassSampleR * iirAmount);
            bassSampleL = iirSampleCL;
            bassSampleR = iirSampleCR;
        }
        else
        {
            iirSampleDL = (iirSampleDL * (1 - iirAmount)) + (bassSampleL * iirAmount);
            iirSampleDR = (iirSampleDR * (1 - iirAmount)) + (bassSampleR * iirAmount);
            bassSampleL = iirSampleDL;
            bassSampleR = iirSampleDR;
        }
        // bass filter same as high but only after the clipping
        flip = !flip;

        bridgerectifier = inputSampleL;
        // insanity check
        randy = bias + ((rand() / (double)RAND_MAX) * noise);
        if ((randy >= 0.0) && (randy < 1.0))
            bridgerectifier = (inputSampleL * randy) + (secondSampleL * (1.0 - randy));
        if ((randy >= 1.0) && (randy < 2.0))
            bridgerectifier = (secondSampleL * (randy - 1.0)) + (thirdSampleL * (2.0 - randy));
        if ((randy >= 2.0) && (randy < 3.0))
            bridgerectifier = (thirdSampleL * (randy - 2.0)) + (fourthSampleL * (3.0 - randy));
        if ((randy >= 3.0) && (randy < 4.0))
            bridgerectifier = (fourthSampleL * (randy - 3.0)) + (fifthSampleL * (4.0 - randy));
        fifthSampleL = fourthSampleL;
        fourthSampleL = thirdSampleL;
        thirdSampleL = secondSampleL;
        secondSampleL = inputSampleL;
        // high freq noise/flutter section

        inputSampleL = bridgerectifier;
        // apply overall, or just to the distorted bit? if the latter, below says 'fabs
        // bridgerectifier'

        bridgerectifier = inputSampleR;
        // insanity check
        randy = bias + ((rand() / (double)RAND_MAX) * noise);
        if ((randy >= 0.0) && (randy < 1.0))
            bridgerectifier = (inputSampleR * randy) + (secondSampleR * (1.0 - randy));
        if ((randy >= 1.0) && (randy < 2.0))
            bridgerectifier = (secondSampleR * (randy - 1.0)) + (thirdSampleR * (2.0 - randy));
        if ((randy >= 2.0) && (randy < 3.0))
            bridgerectifier = (thirdSampleR * (randy - 2.0)) + (fourthSampleR * (3.0 - randy));
        if ((randy >= 3.0) && (randy < 4.0))
            bridgerectifier = (fourthSampleR * (randy - 3.0)) + (fifthSampleR * (4.0 - randy));
        fifthSampleR = fourthSampleR;
        fourthSampleR = thirdSampleR;
        thirdSampleR = secondSampleR;
        secondSampleR = inputSampleR;
        // high freq noise/flutter section

        inputSampleR = bridgerectifier;
        // apply overall, or just to the distorted bit? if the latter, below says 'fabs
        // bridgerectifier'

        bridgerectifier = fabs(inputSampleL) * densityA;
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0)
            inputSampleL = bridgerectifier / densityA;
        else
            inputSampleL = -bridgerectifier / densityA;
        // blend according to densityA control

        bridgerectifier = fabs(inputSampleR) * densityA;
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0)
            inputSampleR = bridgerectifier / densityA;
        else
            inputSampleR = -bridgerectifier / densityA;
        // blend according to densityA control

        inputSampleL *= trebleGainTrim;
        inputSampleR *= trebleGainTrim;
        bassSampleL *= bassGainTrim;
        bassSampleR *= bassGainTrim;
        inputSampleL += bassSampleL;
        inputSampleR += bassSampleR;
        // that simple.

        // begin 32 bit stereo floating point dither
        int expon;
        frexpf((float)inputSampleL, &expon);
        fpd ^= fpd << 13;
        fpd ^= fpd >> 17;
        fpd ^= fpd << 5;
        inputSampleL += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        frexpf((float)inputSampleR, &expon);
        fpd ^= fpd << 13;
        fpd ^= fpd >> 17;
        fpd ^= fpd << 5;
        inputSampleR += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        // end 32 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

void ChromeOxide::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double *in1 = inputs[0];
    double *in2 = inputs[1];
    double *out1 = outputs[0];
    double *out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    double bassSampleL;
    double bassSampleR;
    double randy;
    double bias = B / 1.31578947368421;
    double intensity = 0.9 + pow(A, 2);
    double iirAmount = pow(1.0 - (intensity / (10 + (bias * 4.0))), 2) / overallscale;
    // make 10 higher for less trashy sound in high settings
    double bridgerectifier;
    double trebleGainTrim = 1.0;
    double indrive = 1.0;
    double densityA = (intensity * 80) + 1.0;
    double noise = intensity / (1.0 + bias);
    double bassGainTrim = 1.0;
    double glitch = 0.0;
    bias *= overallscale;
    noise *= overallscale;

    if (intensity > 1.0)
    {
        glitch = intensity - 1.0;
        indrive = intensity * intensity;
        bassGainTrim /= (intensity * intensity);
        trebleGainTrim = (intensity + 1.0) / 2.0;
    }
    // everything runs off Intensity now

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-43)
            inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR) < 1.18e-43)
            inputSampleR = fpd * 1.18e-43;

        inputSampleL *= indrive;
        inputSampleR *= indrive;
        bassSampleL = inputSampleL;
        bassSampleR = inputSampleR;

        if (flip)
        {
            iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleAL;
            inputSampleR -= iirSampleAR;
        }
        else
        {
            iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
            iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
            inputSampleL -= iirSampleBL;
            inputSampleR -= iirSampleBR;
        }
        // highpass section

        bassSampleL -=
            (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch));
        bassSampleR -=
            (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch));
        // overdrive the bass channel

        if (flip)
        {
            iirSampleCL = (iirSampleCL * (1 - iirAmount)) + (bassSampleL * iirAmount);
            iirSampleCR = (iirSampleCR * (1 - iirAmount)) + (bassSampleR * iirAmount);
            bassSampleL = iirSampleCL;
            bassSampleR = iirSampleCR;
        }
        else
        {
            iirSampleDL = (iirSampleDL * (1 - iirAmount)) + (bassSampleL * iirAmount);
            iirSampleDR = (iirSampleDR * (1 - iirAmount)) + (bassSampleR * iirAmount);
            bassSampleL = iirSampleDL;
            bassSampleR = iirSampleDR;
        }
        // bass filter same as high but only after the clipping
        flip = !flip;

        bridgerectifier = inputSampleL;
        // insanity check
        randy = bias + ((rand() / (double)RAND_MAX) * noise);
        if ((randy >= 0.0) && (randy < 1.0))
            bridgerectifier = (inputSampleL * randy) + (secondSampleL * (1.0 - randy));
        if ((randy >= 1.0) && (randy < 2.0))
            bridgerectifier = (secondSampleL * (randy - 1.0)) + (thirdSampleL * (2.0 - randy));
        if ((randy >= 2.0) && (randy < 3.0))
            bridgerectifier = (thirdSampleL * (randy - 2.0)) + (fourthSampleL * (3.0 - randy));
        if ((randy >= 3.0) && (randy < 4.0))
            bridgerectifier = (fourthSampleL * (randy - 3.0)) + (fifthSampleL * (4.0 - randy));
        fifthSampleL = fourthSampleL;
        fourthSampleL = thirdSampleL;
        thirdSampleL = secondSampleL;
        secondSampleL = inputSampleL;
        // high freq noise/flutter section

        inputSampleL = bridgerectifier;
        // apply overall, or just to the distorted bit? if the latter, below says 'fabs
        // bridgerectifier'

        bridgerectifier = inputSampleR;
        // insanity check
        randy = bias + ((rand() / (double)RAND_MAX) * noise);
        if ((randy >= 0.0) && (randy < 1.0))
            bridgerectifier = (inputSampleR * randy) + (secondSampleR * (1.0 - randy));
        if ((randy >= 1.0) && (randy < 2.0))
            bridgerectifier = (secondSampleR * (randy - 1.0)) + (thirdSampleR * (2.0 - randy));
        if ((randy >= 2.0) && (randy < 3.0))
            bridgerectifier = (thirdSampleR * (randy - 2.0)) + (fourthSampleR * (3.0 - randy));
        if ((randy >= 3.0) && (randy < 4.0))
            bridgerectifier = (fourthSampleR * (randy - 3.0)) + (fifthSampleR * (4.0 - randy));
        fifthSampleR = fourthSampleR;
        fourthSampleR = thirdSampleR;
        thirdSampleR = secondSampleR;
        secondSampleR = inputSampleR;
        // high freq noise/flutter section

        inputSampleR = bridgerectifier;
        // apply overall, or just to the distorted bit? if the latter, below says 'fabs
        // bridgerectifier'

        bridgerectifier = fabs(inputSampleL) * densityA;
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0)
            inputSampleL = bridgerectifier / densityA;
        else
            inputSampleL = -bridgerectifier / densityA;
        // blend according to densityA control

        bridgerectifier = fabs(inputSampleR) * densityA;
        if (bridgerectifier > 1.57079633)
            bridgerectifier = 1.57079633;
        // max value for sine function
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0)
            inputSampleR = bridgerectifier / densityA;
        else
            inputSampleR = -bridgerectifier / densityA;
        // blend according to densityA control

        inputSampleL *= trebleGainTrim;
        inputSampleR *= trebleGainTrim;
        bassSampleL *= bassGainTrim;
        bassSampleR *= bassGainTrim;
        inputSampleL += bassSampleL;
        inputSampleR += bassSampleR;
        // that simple.

        // begin 64 bit stereo floating point dither
        int expon;
        frexp((double)inputSampleL, &expon);
        fpd ^= fpd << 13;
        fpd ^= fpd >> 17;
        fpd ^= fpd << 5;
        inputSampleL += ((double(fpd) - uint32_t(0x7fffffff)) * 1.1e-44l * pow(2, expon + 62));
        frexp((double)inputSampleR, &expon);
        fpd ^= fpd << 13;
        fpd ^= fpd >> 17;
        fpd ^= fpd << 5;
        inputSampleR += ((double(fpd) - uint32_t(0x7fffffff)) * 1.1e-44l * pow(2, expon + 62));
        // end 64 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

} // end namespace ChromeOxide
