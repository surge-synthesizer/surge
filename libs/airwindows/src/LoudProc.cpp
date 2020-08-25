/* ========================================
 *  Loud - Loud.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Loud_H
#include "Loud.h"
#endif

namespace Loud {


void Loud::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double boost = pow(A+1.0,5);
	double output = B;
	double wet = C;
	double dry = 1.0-wet;
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	double clamp;
	
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		//begin L
		inputSampleL *= boost;
		clamp = inputSampleL - lastSampleL;
		
		if (clamp > 0)
		{
			inputSampleL = -(inputSampleL - 1.0);
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = sin(inputSampleL) * overallscale;
			if (clamp > inputSampleL) clamp = inputSampleL;
		}
		
		if (clamp < 0)
		{
			inputSampleL += 1.0;
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = -sin(inputSampleL) * overallscale;
			if (clamp < inputSampleL) clamp = inputSampleL;
		}
		
		inputSampleL = lastSampleL + clamp;
		lastSampleL = inputSampleL;
		//finished L
		
		//begin R
		inputSampleR *= boost;
		clamp = inputSampleR - lastSampleR;
		
		if (clamp > 0)
		{
			inputSampleR = -(inputSampleR - 1.0);
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = sin(inputSampleR) * overallscale;
			if (clamp > inputSampleR) clamp = inputSampleR;
		}
		
		if (clamp < 0)
		{
			inputSampleR += 1.0;
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = -sin(inputSampleR) * overallscale;
			if (clamp < inputSampleR) clamp = inputSampleR;
		}
		
		inputSampleR = lastSampleR + clamp;
		lastSampleR = inputSampleR;
		//finished R
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL*dry)+(inputSampleL*wet);
			inputSampleR = (drySampleR*dry)+(inputSampleR*wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.
		
		
		//stereo 32 bit dither, made small and tidy.
		int expon; frexpf((float)inputSampleL, &expon);
		long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
		frexpf((float)inputSampleR, &expon);
		dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
		//end 32 bit dither

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void Loud::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double boost = pow(A+1.0,5);
	double output = B;
	double wet = C;
	double dry = 1.0-wet;
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	double clamp;
	
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		
		//begin L
		inputSampleL *= boost;
		clamp = inputSampleL - lastSampleL;
		
		if (clamp > 0)
		{
			inputSampleL = -(inputSampleL - 1.0);
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = sin(inputSampleL) * overallscale;
			if (clamp > inputSampleL) clamp = inputSampleL;
		}
		
		if (clamp < 0)
		{
			inputSampleL += 1.0;
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = -sin(inputSampleL) * overallscale;
			if (clamp < inputSampleL) clamp = inputSampleL;
		}
		
		inputSampleL = lastSampleL + clamp;
		lastSampleL = inputSampleL;
		//finished L
		
		//begin R
		inputSampleR *= boost;
		clamp = inputSampleR - lastSampleR;
		
		if (clamp > 0)
		{
			inputSampleR = -(inputSampleR - 1.0);
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = sin(inputSampleR) * overallscale;
			if (clamp > inputSampleR) clamp = inputSampleR;
		}
		
		if (clamp < 0)
		{
			inputSampleR += 1.0;
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = -sin(inputSampleR) * overallscale;
			if (clamp < inputSampleR) clamp = inputSampleR;
		}
		
		inputSampleR = lastSampleR + clamp;
		lastSampleR = inputSampleR;
		//finished R
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL*dry)+(inputSampleL*wet);
			inputSampleR = (drySampleR*dry)+(inputSampleR*wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.
		
		//stereo 64 bit dither, made small and tidy.
		int expon; frexp((double)inputSampleL, &expon);
		long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		dither /= 536870912.0; //needs this to scale to 64 bit zone
		inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
		frexp((double)inputSampleR, &expon);
		dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		dither /= 536870912.0; //needs this to scale to 64 bit zone
		inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
		//end 64 bit dither

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Loud

