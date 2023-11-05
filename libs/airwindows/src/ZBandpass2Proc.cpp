/* ========================================
 *  ZBandpass2 - ZBandpass2.h
 *  Copyright (c) 2016 airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __ZBandpass2_H
#include "ZBandpass2.h"
#endif

namespace ZBandpass2 {


void ZBandpass2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	VstInt32 inFramesToProcess = sampleFrames; //vst doesn't give us this as a separate variable so we'll make it
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	biquadA[biq_freq] = ((pow(B,4)*14300.0)/getSampleRate())+0.00079;
	double clipFactor = 1.0-((1.0-B)*0.304);
    biquadA[biq_reso] = 0.314;
	biquadA[biq_aA0] = biquadA[biq_aB0];
	biquadA[biq_aA1] = biquadA[biq_aB1];
	biquadA[biq_aA2] = biquadA[biq_aB2];
	biquadA[biq_bA1] = biquadA[biq_bB1];
	biquadA[biq_bA2] = biquadA[biq_bB2];
	//previous run through the buffer is still in the filter, so we move it
	//to the A section and now it's the new starting point.
	double K = tan(M_PI * biquadA[biq_freq]);
	double norm = 1.0 / (1.0 + K / biquadA[biq_reso] + K * K);
	biquadA[biq_aB0] = K / biquadA[biq_reso] * norm;
	biquadA[biq_aB1] = 0.0;
	biquadA[biq_aB2] = -biquadA[biq_aB0];
	biquadA[biq_bB1] = 2.0 * (K * K - 1.0) * norm;
	biquadA[biq_bB2] = (1.0 - K / biquadA[biq_reso] + K * K) * norm;
	
	//opamp stuff
	inTrimA = inTrimB;
	inTrimB = A*10.0;
	inTrimB *= inTrimB; inTrimB *= inTrimB;
	outTrimA = outTrimB;
	outTrimB = C*10.0;
	wetA = wetB;
	wetB = pow(D,2);
	
	double iirAmountA = 0.00069/overallscale;
	fixA[fix_freq] = fixB[fix_freq] = 15500.0 / getSampleRate();
    fixA[fix_reso] = fixB[fix_reso] = 0.935;
	K = tan(M_PI * fixB[fix_freq]); //lowpass
	norm = 1.0 / (1.0 + K / fixB[fix_reso] + K * K);
	fixA[fix_a0] = fixB[fix_a0] = K * K * norm;
	fixA[fix_a1] = fixB[fix_a1] = 2.0 * fixB[fix_a0];
	fixA[fix_a2] = fixB[fix_a2] = fixB[fix_a0];
	fixA[fix_b1] = fixB[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixA[fix_b2] = fixB[fix_b2] = (1.0 - K / fixB[fix_reso] + K * K) * norm;
	//end opamp stuff
	
 	double trim = 0.1+(3.712*biquadA[biq_freq]);
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		double overallDrySampleL = inputSampleL;
		double overallDrySampleR = inputSampleR;
		
		double outSample = (double)sampleFrames/inFramesToProcess;
		biquadA[biq_a0] = (biquadA[biq_aA0]*outSample)+(biquadA[biq_aB0]*(1.0-outSample));
		biquadA[biq_a1] = (biquadA[biq_aA1]*outSample)+(biquadA[biq_aB1]*(1.0-outSample));
		biquadA[biq_a2] = (biquadA[biq_aA2]*outSample)+(biquadA[biq_aB2]*(1.0-outSample));
		biquadA[biq_b1] = (biquadA[biq_bA1]*outSample)+(biquadA[biq_bB1]*(1.0-outSample));
		biquadA[biq_b2] = (biquadA[biq_bA2]*outSample)+(biquadA[biq_bB2]*(1.0-outSample));
		for (int x = 0; x < 7; x++) {biquadD[x] = biquadC[x] = biquadB[x] = biquadA[x];}
		//this is the interpolation code for the biquad
		double inTrim = (inTrimA*outSample)+(inTrimB*(1.0-outSample));
		double outTrim = (outTrimA*outSample)+(outTrimB*(1.0-outSample));
		double wet = (wetA*outSample)+(wetB*(1.0-outSample));
		double aWet = 1.0;
		double bWet = 1.0;
		double cWet = 1.0;
		double dWet = wet*4.0;
		//four-stage wet/dry control using progressive stages that bypass when not engaged
		if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
		else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
		else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
		else {dWet -= 3.0;}
		//this is one way to make a little set of dry/wet stages that are successively added to the
		//output as the control is turned up. Each one independently goes from 0-1 and stays at 1
		//beyond that point: this is a way to progressively add a 'black box' sound processing
		//which lets you fall through to simpler processing at lower settings.
		if (inTrim != 1.0) {
			inputSampleL *= inTrim;
			inputSampleR *= inTrim;
		}
		if (inputSampleL > 1.0) inputSampleL = 1.0; if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0; if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleL *= trim; inputSampleR *= trim;
		inputSampleL /= clipFactor; inputSampleR /= clipFactor;
		outSample = (inputSampleL * biquadA[biq_a0]) + biquadA[biq_sL1];
		if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
		biquadA[biq_sL1] = (inputSampleL * biquadA[biq_a1]) - (outSample * biquadA[biq_b1]) + biquadA[biq_sL2];
		biquadA[biq_sL2] = (inputSampleL * biquadA[biq_a2]) - (outSample * biquadA[biq_b2]);
		drySampleL = inputSampleL = outSample;
		outSample = (inputSampleR * biquadA[biq_a0]) + biquadA[biq_sR1];
		if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
		biquadA[biq_sR1] = (inputSampleR * biquadA[biq_a1]) - (outSample * biquadA[biq_b1]) + biquadA[biq_sR2];
		biquadA[biq_sR2] = (inputSampleR * biquadA[biq_a2]) - (outSample * biquadA[biq_b2]);
		drySampleR = inputSampleR = outSample;
		
		if (bWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadB[biq_a0]) + biquadB[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadB[biq_sL1] = (inputSampleL * biquadB[biq_a1]) - (outSample * biquadB[biq_b1]) + biquadB[biq_sL2];
			biquadB[biq_sL2] = (inputSampleL * biquadB[biq_a2]) - (outSample * biquadB[biq_b2]);
			drySampleL = inputSampleL = (outSample * bWet) + (drySampleL * (1.0-bWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadB[biq_a0]) + biquadB[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadB[biq_sR1] = (inputSampleR * biquadB[biq_a1]) - (outSample * biquadB[biq_b1]) + biquadB[biq_sR2];
			biquadB[biq_sR2] = (inputSampleR * biquadB[biq_a2]) - (outSample * biquadB[biq_b2]);
			drySampleR = inputSampleR = (outSample * bWet) + (drySampleR * (1.0-bWet));
		}
		
		if (cWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadC[biq_a0]) + biquadC[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadC[biq_sL1] = (inputSampleL * biquadC[biq_a1]) - (outSample * biquadC[biq_b1]) + biquadC[biq_sL2];
			biquadC[biq_sL2] = (inputSampleL * biquadC[biq_a2]) - (outSample * biquadC[biq_b2]);
			drySampleL = inputSampleL = (outSample * cWet) + (drySampleL * (1.0-cWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadC[biq_a0]) + biquadC[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadC[biq_sR1] = (inputSampleR * biquadC[biq_a1]) - (outSample * biquadC[biq_b1]) + biquadC[biq_sR2];
			biquadC[biq_sR2] = (inputSampleR * biquadC[biq_a2]) - (outSample * biquadC[biq_b2]);
			drySampleR = inputSampleR = (outSample * cWet) + (drySampleR * (1.0-cWet));
		}
		
		if (dWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadD[biq_a0]) + biquadD[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadD[biq_sL1] = (inputSampleL * biquadD[biq_a1]) - (outSample * biquadD[biq_b1]) + biquadD[biq_sL2];
			biquadD[biq_sL2] = (inputSampleL * biquadD[biq_a2]) - (outSample * biquadD[biq_b2]);
			drySampleL = inputSampleL = (outSample * dWet) + (drySampleL * (1.0-dWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadD[biq_a0]) + biquadD[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadD[biq_sR1] = (inputSampleR * biquadD[biq_a1]) - (outSample * biquadD[biq_b1]) + biquadD[biq_sR2];
			biquadD[biq_sR2] = (inputSampleR * biquadD[biq_a2]) - (outSample * biquadD[biq_b2]);
			drySampleR = inputSampleR = (outSample * dWet) + (drySampleR * (1.0-dWet));
		}
		
		inputSampleL /= clipFactor;
		inputSampleR /= clipFactor;
		
		//opamp stage
		if (fabs(iirSampleAL)<1.18e-37) iirSampleAL = 0.0;
		iirSampleAL = (iirSampleAL * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
		inputSampleL -= iirSampleAL;
		if (fabs(iirSampleAR)<1.18e-37) iirSampleAR = 0.0;
		iirSampleAR = (iirSampleAR * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
		inputSampleR -= iirSampleAR;
		
		outSample = (inputSampleL * fixA[fix_a0]) + fixA[fix_sL1];
		fixA[fix_sL1] = (inputSampleL * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sL2];
		fixA[fix_sL2] = (inputSampleL * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixA[fix_a0]) + fixA[fix_sR1];
		fixA[fix_sR1] = (inputSampleR * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sR2];
		fixA[fix_sR2] = (inputSampleR * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (inputSampleL > 1.0) inputSampleL = 1.0; if (inputSampleL < -1.0) inputSampleL = -1.0;
		inputSampleL -= (inputSampleL*inputSampleL*inputSampleL*inputSampleL*inputSampleL*0.1768);
		if (inputSampleR > 1.0) inputSampleR = 1.0; if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleR -= (inputSampleR*inputSampleR*inputSampleR*inputSampleR*inputSampleR*0.1768);
		
		outSample = (inputSampleL * fixB[fix_a0]) + fixB[fix_sL1];
		fixB[fix_sL1] = (inputSampleL * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sL2];
		fixB[fix_sL2] = (inputSampleL * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixB[fix_a0]) + fixB[fix_sR1];
		fixB[fix_sR1] = (inputSampleR * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sR2];
		fixB[fix_sR2] = (inputSampleR * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (outTrim != 1.0) {
			inputSampleL *= outTrim;
			inputSampleR *= outTrim;
		}
		//end opamp stage
		
		if (aWet != 1.0) {
			inputSampleL = (inputSampleL*aWet) + (overallDrySampleL*(1.0-aWet));
			inputSampleR = (inputSampleR*aWet) + (overallDrySampleR*(1.0-aWet));
		}
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void ZBandpass2::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	VstInt32 inFramesToProcess = sampleFrames; //vst doesn't give us this as a separate variable so we'll make it
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	biquadA[biq_freq] = ((pow(B,4)*14300.0)/getSampleRate())+0.00079;
	double clipFactor = 1.0-((1.0-B)*0.304);
    biquadA[biq_reso] = 0.314;
	biquadA[biq_aA0] = biquadA[biq_aB0];
	biquadA[biq_aA1] = biquadA[biq_aB1];
	biquadA[biq_aA2] = biquadA[biq_aB2];
	biquadA[biq_bA1] = biquadA[biq_bB1];
	biquadA[biq_bA2] = biquadA[biq_bB2];
	//previous run through the buffer is still in the filter, so we move it
	//to the A section and now it's the new starting point.
	double K = tan(M_PI * biquadA[biq_freq]);
	double norm = 1.0 / (1.0 + K / biquadA[biq_reso] + K * K);
	biquadA[biq_aB0] = K / biquadA[biq_reso] * norm;
	biquadA[biq_aB1] = 0.0;
	biquadA[biq_aB2] = -biquadA[biq_aB0];
	biquadA[biq_bB1] = 2.0 * (K * K - 1.0) * norm;
	biquadA[biq_bB2] = (1.0 - K / biquadA[biq_reso] + K * K) * norm;
	
	//opamp stuff
	inTrimA = inTrimB;
	inTrimB = A*10.0;
	inTrimB *= inTrimB; inTrimB *= inTrimB;
	outTrimA = outTrimB;
	outTrimB = C*10.0;
	wetA = wetB;
	wetB = pow(D,2);
	
	double iirAmountA = 0.00069/overallscale;
	fixA[fix_freq] = fixB[fix_freq] = 15500.0 / getSampleRate();
    fixA[fix_reso] = fixB[fix_reso] = 0.935;
	K = tan(M_PI * fixB[fix_freq]); //lowpass
	norm = 1.0 / (1.0 + K / fixB[fix_reso] + K * K);
	fixA[fix_a0] = fixB[fix_a0] = K * K * norm;
	fixA[fix_a1] = fixB[fix_a1] = 2.0 * fixB[fix_a0];
	fixA[fix_a2] = fixB[fix_a2] = fixB[fix_a0];
	fixA[fix_b1] = fixB[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixA[fix_b2] = fixB[fix_b2] = (1.0 - K / fixB[fix_reso] + K * K) * norm;
	//end opamp stuff
	
 	double trim = 0.1+(3.712*biquadA[biq_freq]);
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		double overallDrySampleL = inputSampleL;
		double overallDrySampleR = inputSampleR;
		
		double outSample = (double)sampleFrames/inFramesToProcess;
		biquadA[biq_a0] = (biquadA[biq_aA0]*outSample)+(biquadA[biq_aB0]*(1.0-outSample));
		biquadA[biq_a1] = (biquadA[biq_aA1]*outSample)+(biquadA[biq_aB1]*(1.0-outSample));
		biquadA[biq_a2] = (biquadA[biq_aA2]*outSample)+(biquadA[biq_aB2]*(1.0-outSample));
		biquadA[biq_b1] = (biquadA[biq_bA1]*outSample)+(biquadA[biq_bB1]*(1.0-outSample));
		biquadA[biq_b2] = (biquadA[biq_bA2]*outSample)+(biquadA[biq_bB2]*(1.0-outSample));
		for (int x = 0; x < 7; x++) {biquadD[x] = biquadC[x] = biquadB[x] = biquadA[x];}
		//this is the interpolation code for the biquad
		double inTrim = (inTrimA*outSample)+(inTrimB*(1.0-outSample));
		double outTrim = (outTrimA*outSample)+(outTrimB*(1.0-outSample));
		double wet = (wetA*outSample)+(wetB*(1.0-outSample));
		double aWet = 1.0;
		double bWet = 1.0;
		double cWet = 1.0;
		double dWet = wet*4.0;
		//four-stage wet/dry control using progressive stages that bypass when not engaged
		if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
		else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
		else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
		else {dWet -= 3.0;}
		//this is one way to make a little set of dry/wet stages that are successively added to the
		//output as the control is turned up. Each one independently goes from 0-1 and stays at 1
		//beyond that point: this is a way to progressively add a 'black box' sound processing
		//which lets you fall through to simpler processing at lower settings.
		if (inTrim != 1.0) {
			inputSampleL *= inTrim;
			inputSampleR *= inTrim;
		}
		if (inputSampleL > 1.0) inputSampleL = 1.0; if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0; if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleL *= trim; inputSampleR *= trim;
		inputSampleL /= clipFactor; inputSampleR /= clipFactor;
		outSample = (inputSampleL * biquadA[biq_a0]) + biquadA[biq_sL1];
		if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
		biquadA[biq_sL1] = (inputSampleL * biquadA[biq_a1]) - (outSample * biquadA[biq_b1]) + biquadA[biq_sL2];
		biquadA[biq_sL2] = (inputSampleL * biquadA[biq_a2]) - (outSample * biquadA[biq_b2]);
		drySampleL = inputSampleL = outSample;
		outSample = (inputSampleR * biquadA[biq_a0]) + biquadA[biq_sR1];
		if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
		biquadA[biq_sR1] = (inputSampleR * biquadA[biq_a1]) - (outSample * biquadA[biq_b1]) + biquadA[biq_sR2];
		biquadA[biq_sR2] = (inputSampleR * biquadA[biq_a2]) - (outSample * biquadA[biq_b2]);
		drySampleR = inputSampleR = outSample;
		
		if (bWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadB[biq_a0]) + biquadB[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadB[biq_sL1] = (inputSampleL * biquadB[biq_a1]) - (outSample * biquadB[biq_b1]) + biquadB[biq_sL2];
			biquadB[biq_sL2] = (inputSampleL * biquadB[biq_a2]) - (outSample * biquadB[biq_b2]);
			drySampleL = inputSampleL = (outSample * bWet) + (drySampleL * (1.0-bWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadB[biq_a0]) + biquadB[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadB[biq_sR1] = (inputSampleR * biquadB[biq_a1]) - (outSample * biquadB[biq_b1]) + biquadB[biq_sR2];
			biquadB[biq_sR2] = (inputSampleR * biquadB[biq_a2]) - (outSample * biquadB[biq_b2]);
			drySampleR = inputSampleR = (outSample * bWet) + (drySampleR * (1.0-bWet));
		}
		
		if (cWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadC[biq_a0]) + biquadC[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadC[biq_sL1] = (inputSampleL * biquadC[biq_a1]) - (outSample * biquadC[biq_b1]) + biquadC[biq_sL2];
			biquadC[biq_sL2] = (inputSampleL * biquadC[biq_a2]) - (outSample * biquadC[biq_b2]);
			drySampleL = inputSampleL = (outSample * cWet) + (drySampleL * (1.0-cWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadC[biq_a0]) + biquadC[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadC[biq_sR1] = (inputSampleR * biquadC[biq_a1]) - (outSample * biquadC[biq_b1]) + biquadC[biq_sR2];
			biquadC[biq_sR2] = (inputSampleR * biquadC[biq_a2]) - (outSample * biquadC[biq_b2]);
			drySampleR = inputSampleR = (outSample * cWet) + (drySampleR * (1.0-cWet));
		}
		
		if (dWet > 0.0) {
			inputSampleL /= clipFactor;
			outSample = (inputSampleL * biquadD[biq_a0]) + biquadD[biq_sL1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadD[biq_sL1] = (inputSampleL * biquadD[biq_a1]) - (outSample * biquadD[biq_b1]) + biquadD[biq_sL2];
			biquadD[biq_sL2] = (inputSampleL * biquadD[biq_a2]) - (outSample * biquadD[biq_b2]);
			drySampleL = inputSampleL = (outSample * dWet) + (drySampleL * (1.0-dWet));
			inputSampleR /= clipFactor;
			outSample = (inputSampleR * biquadD[biq_a0]) + biquadD[biq_sR1];
			if (outSample > 1.0) outSample = 1.0; if (outSample < -1.0) outSample = -1.0;
			biquadD[biq_sR1] = (inputSampleR * biquadD[biq_a1]) - (outSample * biquadD[biq_b1]) + biquadD[biq_sR2];
			biquadD[biq_sR2] = (inputSampleR * biquadD[biq_a2]) - (outSample * biquadD[biq_b2]);
			drySampleR = inputSampleR = (outSample * dWet) + (drySampleR * (1.0-dWet));
		}
		
		inputSampleL /= clipFactor;
		inputSampleR /= clipFactor;
		
		//opamp stage
		if (fabs(iirSampleAL)<1.18e-37) iirSampleAL = 0.0;
		iirSampleAL = (iirSampleAL * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
		inputSampleL -= iirSampleAL;
		if (fabs(iirSampleAR)<1.18e-37) iirSampleAR = 0.0;
		iirSampleAR = (iirSampleAR * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
		inputSampleR -= iirSampleAR;
		
		outSample = (inputSampleL * fixA[fix_a0]) + fixA[fix_sL1];
		fixA[fix_sL1] = (inputSampleL * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sL2];
		fixA[fix_sL2] = (inputSampleL * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixA[fix_a0]) + fixA[fix_sR1];
		fixA[fix_sR1] = (inputSampleR * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sR2];
		fixA[fix_sR2] = (inputSampleR * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (inputSampleL > 1.0) inputSampleL = 1.0; if (inputSampleL < -1.0) inputSampleL = -1.0;
		inputSampleL -= (inputSampleL*inputSampleL*inputSampleL*inputSampleL*inputSampleL*0.1768);
		if (inputSampleR > 1.0) inputSampleR = 1.0; if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleR -= (inputSampleR*inputSampleR*inputSampleR*inputSampleR*inputSampleR*0.1768);
		
		outSample = (inputSampleL * fixB[fix_a0]) + fixB[fix_sL1];
		fixB[fix_sL1] = (inputSampleL * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sL2];
		fixB[fix_sL2] = (inputSampleL * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixB[fix_a0]) + fixB[fix_sR1];
		fixB[fix_sR1] = (inputSampleR * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sR2];
		fixB[fix_sR2] = (inputSampleR * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (outTrim != 1.0) {
			inputSampleL *= outTrim;
			inputSampleR *= outTrim;
		}
		//end opamp stage
		
		if (aWet != 1.0) {
			inputSampleL = (inputSampleL*aWet) + (overallDrySampleL*(1.0-aWet));
			inputSampleR = (inputSampleR*aWet) + (overallDrySampleR*(1.0-aWet));
		}
		
		//begin 64 bit stereo floating point dither
		//int expon; frexp((double)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		//inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//frexp((double)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		//inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace ZBandpass2

