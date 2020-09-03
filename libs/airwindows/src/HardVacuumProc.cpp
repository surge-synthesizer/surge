/* ========================================
 *  HardVacuum - HardVacuum.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __HardVacuum_H
#include "HardVacuum.h"
#endif

namespace HardVacuum {


void HardVacuum::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double multistage = A*2.0;
	if (multistage > 1) multistage *= multistage;
	//WE MAKE LOUD NOISE! RAWWWK!
	double countdown;
	double warmth = B;
	double invwarmth = 1.0-warmth;
	warmth /= 1.57079633;
	double aura = C*3.1415926;
	double out = D;
	double wet = E;
	double dry = 1.0-wet;
	double drive;
	double positive;
	double negative;
	double bridgerectifierL;
	double bridgerectifierR;
	double skewL;
	double skewR;


	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	    
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
		
		skewL = (inputSampleL - lastSampleL);
		skewR = (inputSampleR - lastSampleR);
		lastSampleL = inputSampleL;
		lastSampleR = inputSampleR;
		//skew will be direction/angle
		bridgerectifierL = fabs(skewL);
		bridgerectifierR = fabs(skewR);
		if (bridgerectifierL > 3.1415926) bridgerectifierL = 3.1415926;
		if (bridgerectifierR > 3.1415926) bridgerectifierR = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		
		bridgerectifierL = sin(bridgerectifierL);
		bridgerectifierR = sin(bridgerectifierR);
		if (skewL > 0) skewL = bridgerectifierL*aura;
		else skewL = -bridgerectifierL*aura;
		if (skewR > 0) skewR = bridgerectifierR*aura;
		else skewR = -bridgerectifierR*aura;
		//skew is now sined and clamped and then re-amplified again
		skewL *= inputSampleL;
		skewR *= inputSampleR;
		//cools off sparkliness and crossover distortion
		skewL *= 1.557079633;
		skewR *= 1.557079633;
		//crank up the gain on this so we can make it sing
		//We're doing all this here so skew isn't incremented by each stage
		
		countdown = multistage;
		//begin the torture
		
		while (countdown > 0)
		{
			if (countdown > 1.0) drive = 1.557079633;
			else drive = countdown * (1.0+(0.557079633*invwarmth));
			//full crank stages followed by the proportional one
			//whee. 1 at full warmth to 1.5570etc at no warmth
			positive = drive - warmth;
			negative = drive + warmth;
			//set up things so we can do repeated iterations, assuming that
			//wet is always going to be 0-1 as in the previous plug.
			bridgerectifierL = fabs(inputSampleL);
			bridgerectifierR = fabs(inputSampleR);
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//apply it here so we don't overload
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			//the distortion section.
			bridgerectifierL *= drive;
			bridgerectifierR *= drive;
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//again
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			if (inputSampleL > 0)
			{
				inputSampleL = (inputSampleL*(1-positive+skewL))+(bridgerectifierL*(positive+skewL));
			}
			else
			{
				inputSampleL = (inputSampleL*(1-negative+skewL))-(bridgerectifierL*(negative+skewL));
			}
			if (inputSampleR > 0)
			{
				inputSampleR = (inputSampleR*(1-positive+skewR))+(bridgerectifierR*(positive+skewR));
			}
			else
			{
				inputSampleR = (inputSampleR*(1-negative+skewR))-(bridgerectifierR*(negative+skewR));
			}
			//blend according to positive and negative controls
			countdown -= 1.0;
			//step down a notch and repeat.
		}

		if (out != 1.0) {
			inputSampleL *= out;
			inputSampleR *= out;
		}
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}
		
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

void HardVacuum::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double multistage = A*2.0;
	if (multistage > 1) multistage *= multistage;
	//WE MAKE LOUD NOISE! RAWWWK!
	double countdown;
	double warmth = B;
	double invwarmth = 1.0-warmth;
	warmth /= 1.57079633;
	double aura = C*3.1415926;
	double out = D;
	double wet = E;
	double dry = 1.0-wet;
	double drive;
	double positive;
	double negative;
	double bridgerectifierL;
	double bridgerectifierR;
	double skewL;
	double skewR;
	
	
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;


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
		
		skewL = (inputSampleL - lastSampleL);
		skewR = (inputSampleR - lastSampleR);
		lastSampleL = inputSampleL;
		lastSampleR = inputSampleR;
		//skew will be direction/angle
		bridgerectifierL = fabs(skewL);
		bridgerectifierR = fabs(skewR);
		if (bridgerectifierL > 3.1415926) bridgerectifierL = 3.1415926;
		if (bridgerectifierR > 3.1415926) bridgerectifierR = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		
		bridgerectifierL = sin(bridgerectifierL);
		bridgerectifierR = sin(bridgerectifierR);
		if (skewL > 0) skewL = bridgerectifierL*aura;
		else skewL = -bridgerectifierL*aura;
		if (skewR > 0) skewR = bridgerectifierR*aura;
		else skewR = -bridgerectifierR*aura;
		//skew is now sined and clamped and then re-amplified again
		skewL *= inputSampleL;
		skewR *= inputSampleR;
		//cools off sparkliness and crossover distortion
		skewL *= 1.557079633;
		skewR *= 1.557079633;
		//crank up the gain on this so we can make it sing
		//We're doing all this here so skew isn't incremented by each stage
		
		countdown = multistage;
		//begin the torture
		
		while (countdown > 0)
		{
			if (countdown > 1.0) drive = 1.557079633;
			else drive = countdown * (1.0+(0.557079633*invwarmth));
			//full crank stages followed by the proportional one
			//whee. 1 at full warmth to 1.5570etc at no warmth
			positive = drive - warmth;
			negative = drive + warmth;
			//set up things so we can do repeated iterations, assuming that
			//wet is always going to be 0-1 as in the previous plug.
			bridgerectifierL = fabs(inputSampleL);
			bridgerectifierR = fabs(inputSampleR);
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//apply it here so we don't overload
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			//the distortion section.
			bridgerectifierL *= drive;
			bridgerectifierR *= drive;
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//again
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			if (inputSampleL > 0)
			{
				inputSampleL = (inputSampleL*(1-positive+skewL))+(bridgerectifierL*(positive+skewL));
			}
			else
			{
				inputSampleL = (inputSampleL*(1-negative+skewL))-(bridgerectifierL*(negative+skewL));
			}
			if (inputSampleR > 0)
			{
				inputSampleR = (inputSampleR*(1-positive+skewR))+(bridgerectifierR*(positive+skewR));
			}
			else
			{
				inputSampleR = (inputSampleR*(1-negative+skewR))-(bridgerectifierR*(negative+skewR));
			}
			//blend according to positive and negative controls
			countdown -= 1.0;
			//step down a notch and repeat.
		}
		
		if (out != 1.0) {
		 inputSampleL *= out;
		 inputSampleR *= out;
		 }
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}
		
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

} // end namespace HardVacuum

