/* ========================================
 *  PurestDrive - PurestDrive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PurestDrive_H
#include "PurestDrive.h"
#endif

namespace PurestDrive {


void PurestDrive::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];


	double intensity = A;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	double apply;
	
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
		
		inputSampleL = sin(inputSampleL);
		//basic distortion factor
		apply = (fabs(previousSampleL + inputSampleL) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.
		inputSampleL = (drySampleL * (1.0 - apply)) + (inputSampleL * apply);		
		//dry-wet control for intensity also has FM modulation to clean up highs
		previousSampleL = sin(drySampleL);
		//apply the sine while storing previous sample
		
		inputSampleR = sin(inputSampleR);
		//basic distortion factor
		apply = (fabs(previousSampleR + inputSampleR) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.
		inputSampleR = (drySampleR * (1.0 - apply)) + (inputSampleR * apply);		
		//dry-wet control for intensity also has FM modulation to clean up highs
		previousSampleR = sin(drySampleR);
		//apply the sine while storing previous sample
		
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

void PurestDrive::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	
	double intensity = A;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	double apply;


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
		
		inputSampleL = sin(inputSampleL);
		//basic distortion factor
		apply = (fabs(previousSampleL + inputSampleL) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.
		inputSampleL = (drySampleL * (1.0 - apply)) + (inputSampleL * apply);		
		//dry-wet control for intensity also has FM modulation to clean up highs
		previousSampleL = sin(drySampleL);
		//apply the sine while storing previous sample
		
		inputSampleR = sin(inputSampleR);
		//basic distortion factor
		apply = (fabs(previousSampleR + inputSampleR) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.
		inputSampleR = (drySampleR * (1.0 - apply)) + (inputSampleR * apply);		
		//dry-wet control for intensity also has FM modulation to clean up highs
		previousSampleR = sin(drySampleR);
		//apply the sine while storing previous sample
		
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

} // end namespace PurestDrive

