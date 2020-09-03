/* ========================================
 *  DrumSlam - DrumSlam.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DrumSlam_H
#include "DrumSlam.h"
#endif

namespace DrumSlam {


void DrumSlam::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double iirAmountL = 0.0819;
	iirAmountL /= overallscale;
	double iirAmountH = 0.377933067;
	iirAmountH /= overallscale;
	double drive = (A*3.0)+1.0;
	double out = B;
	double wet = C;
	double dry = 1.0 - wet;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
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
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		long double lowSampleL;
		long double lowSampleR;
		long double midSampleL;
		long double midSampleR;
		long double highSampleL;
		long double highSampleR;
		

		inputSampleL *= drive;
		inputSampleR *= drive;
		
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleBL = (iirSampleBL * (1 - iirAmountL)) + (iirSampleAL * iirAmountL);
			lowSampleL = iirSampleBL;
			
			iirSampleAR = (iirSampleAR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleBR = (iirSampleBR * (1 - iirAmountL)) + (iirSampleAR * iirAmountL);
			lowSampleR = iirSampleBR;

			iirSampleEL = (iirSampleEL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleFL = (iirSampleFL * (1 - iirAmountH)) + (iirSampleEL * iirAmountH);
			midSampleL = iirSampleFL - iirSampleBL;

			iirSampleER = (iirSampleER * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleFR = (iirSampleFR * (1 - iirAmountH)) + (iirSampleER * iirAmountH);
			midSampleR = iirSampleFR - iirSampleBR;

			highSampleL = inputSampleL - iirSampleFL;
			highSampleR = inputSampleR - iirSampleFR;
		}
		else
		{
			iirSampleCL = (iirSampleCL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleDL = (iirSampleDL * (1 - iirAmountL)) + (iirSampleCL * iirAmountL);
			lowSampleL = iirSampleDL;

			iirSampleCR = (iirSampleCR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleDR = (iirSampleDR * (1 - iirAmountL)) + (iirSampleCR * iirAmountL);
			lowSampleR = iirSampleDR;

			iirSampleGL = (iirSampleGL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleHL = (iirSampleHL * (1 - iirAmountH)) + (iirSampleGL * iirAmountH);
			midSampleL = iirSampleHL - iirSampleDL;

			iirSampleGR = (iirSampleGR * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleHR = (iirSampleHR * (1 - iirAmountH)) + (iirSampleGR * iirAmountH);
			midSampleR = iirSampleHR - iirSampleDR;

			highSampleL = inputSampleL - iirSampleHL;
			highSampleR = inputSampleR - iirSampleHR;
		}
		fpFlip = !fpFlip;

		//generate the tone bands we're using
		if (lowSampleL > 1.0) {lowSampleL = 1.0;}
		if (lowSampleL < -1.0) {lowSampleL = -1.0;}
		if (lowSampleR > 1.0) {lowSampleR = 1.0;}
		if (lowSampleR < -1.0) {lowSampleR = -1.0;}
		lowSampleL -= (lowSampleL * (fabs(lowSampleL) * 0.448) * (fabs(lowSampleL) * 0.448) );
		lowSampleR -= (lowSampleR * (fabs(lowSampleR) * 0.448) * (fabs(lowSampleR) * 0.448) );
		lowSampleL *= drive;
		lowSampleR *= drive;
		
		if (highSampleL > 1.0) {highSampleL = 1.0;}
		if (highSampleL < -1.0) {highSampleL = -1.0;}
		if (highSampleR > 1.0) {highSampleR = 1.0;}
		if (highSampleR < -1.0) {highSampleR = -1.0;}
		highSampleL -= (highSampleL * (fabs(highSampleL) * 0.599) * (fabs(highSampleL) * 0.599) );
		highSampleR -= (highSampleR * (fabs(highSampleR) * 0.599) * (fabs(highSampleR) * 0.599) );
		highSampleL *= drive;
		highSampleR *= drive;
		
		midSampleL = midSampleL * drive;
		midSampleR = midSampleR * drive;
		
		long double skew = (midSampleL - lastSampleL);
		lastSampleL = midSampleL;
		//skew will be direction/angle
		long double bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleL;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleL);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleL > 0)
		{
			midSampleL = bridgerectifier;
		}
		else
		{
			midSampleL = -bridgerectifier;
		}
		//blend according to positive and negative controls, left
		
		skew = (midSampleR - lastSampleR);
		lastSampleR = midSampleR;
		//skew will be direction/angle
		bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleR;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleR);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleR > 0)
		{
			midSampleR = bridgerectifier;
		}
		else
		{
			midSampleR = -bridgerectifier;
		}
		//blend according to positive and negative controls, right
		
		inputSampleL = ((lowSampleL + midSampleL + highSampleL)/drive)*out;
		inputSampleR = ((lowSampleR + midSampleR + highSampleR)/drive)*out;
		
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

void DrumSlam::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double iirAmountL = 0.0819;
	iirAmountL /= overallscale;
	double iirAmountH = 0.377933067;
	iirAmountH /= overallscale;
	double drive = (A*3.0)+1.0;
	double out = B;
	double wet = C;
	double dry = 1.0 - wet;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
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
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		long double lowSampleL;
		long double lowSampleR;
		long double midSampleL;
		long double midSampleR;
		long double highSampleL;
		long double highSampleR;
		
		
		inputSampleL *= drive;
		inputSampleR *= drive;
		
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleBL = (iirSampleBL * (1 - iirAmountL)) + (iirSampleAL * iirAmountL);
			lowSampleL = iirSampleBL;
			
			iirSampleAR = (iirSampleAR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleBR = (iirSampleBR * (1 - iirAmountL)) + (iirSampleAR * iirAmountL);
			lowSampleR = iirSampleBR;
			
			iirSampleEL = (iirSampleEL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleFL = (iirSampleFL * (1 - iirAmountH)) + (iirSampleEL * iirAmountH);
			midSampleL = iirSampleFL - iirSampleBL;
			
			iirSampleER = (iirSampleER * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleFR = (iirSampleFR * (1 - iirAmountH)) + (iirSampleER * iirAmountH);
			midSampleR = iirSampleFR - iirSampleBR;
			
			highSampleL = inputSampleL - iirSampleFL;
			highSampleR = inputSampleR - iirSampleFR;
		}
		else
		{
			iirSampleCL = (iirSampleCL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleDL = (iirSampleDL * (1 - iirAmountL)) + (iirSampleCL * iirAmountL);
			lowSampleL = iirSampleDL;
			
			iirSampleCR = (iirSampleCR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleDR = (iirSampleDR * (1 - iirAmountL)) + (iirSampleCR * iirAmountL);
			lowSampleR = iirSampleDR;
			
			iirSampleGL = (iirSampleGL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleHL = (iirSampleHL * (1 - iirAmountH)) + (iirSampleGL * iirAmountH);
			midSampleL = iirSampleHL - iirSampleDL;
			
			iirSampleGR = (iirSampleGR * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleHR = (iirSampleHR * (1 - iirAmountH)) + (iirSampleGR * iirAmountH);
			midSampleR = iirSampleHR - iirSampleDR;
			
			highSampleL = inputSampleL - iirSampleHL;
			highSampleR = inputSampleR - iirSampleHR;
		}
		fpFlip = !fpFlip;

		//generate the tone bands we're using
		if (lowSampleL > 1.0) {lowSampleL = 1.0;}
		if (lowSampleL < -1.0) {lowSampleL = -1.0;}
		if (lowSampleR > 1.0) {lowSampleR = 1.0;}
		if (lowSampleR < -1.0) {lowSampleR = -1.0;}
		lowSampleL -= (lowSampleL * (fabs(lowSampleL) * 0.448) * (fabs(lowSampleL) * 0.448) );
		lowSampleR -= (lowSampleR * (fabs(lowSampleR) * 0.448) * (fabs(lowSampleR) * 0.448) );
		lowSampleL *= drive;
		lowSampleR *= drive;
		
		if (highSampleL > 1.0) {highSampleL = 1.0;}
		if (highSampleL < -1.0) {highSampleL = -1.0;}
		if (highSampleR > 1.0) {highSampleR = 1.0;}
		if (highSampleR < -1.0) {highSampleR = -1.0;}
		highSampleL -= (highSampleL * (fabs(highSampleL) * 0.599) * (fabs(highSampleL) * 0.599) );
		highSampleR -= (highSampleR * (fabs(highSampleR) * 0.599) * (fabs(highSampleR) * 0.599) );
		highSampleL *= drive;
		highSampleR *= drive;
		
		midSampleL = midSampleL * drive;
		midSampleR = midSampleR * drive;
		
		long double skew = (midSampleL - lastSampleL);
		lastSampleL = midSampleL;
		//skew will be direction/angle
		long double bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleL;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleL);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleL > 0)
		{
			midSampleL = bridgerectifier;
		}
		else
		{
			midSampleL = -bridgerectifier;
		}
		//blend according to positive and negative controls, left
		
		skew = (midSampleR - lastSampleR);
		lastSampleR = midSampleR;
		//skew will be direction/angle
		bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleR;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleR);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleR > 0)
		{
			midSampleR = bridgerectifier;
		}
		else
		{
			midSampleR = -bridgerectifier;
		}
		//blend according to positive and negative controls, right
		
		inputSampleL = ((lowSampleL + midSampleL + highSampleL)/drive)*out;
		inputSampleR = ((lowSampleR + midSampleR + highSampleR)/drive)*out;
		
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

} // end namespace DrumSlam

