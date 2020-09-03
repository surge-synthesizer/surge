/* ========================================
 *  Noise - Noise.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Noise_H
#include "Noise.h"
#endif

namespace Noise {


void Noise::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double cutoffL;
	double cutoffR;
	double cutofftarget = (A*3.5);
	double rumblecutoff = cutofftarget * 0.005;
	double invcutoffL;
	double invcutoffR;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	double highpass = C*38.0;
	int lowcut = floor(highpass);
	int dcut;
	if (lowcut > 37) {dcut= 1151;}
	if (lowcut == 37) {dcut= 1091;}
	if (lowcut == 36) {dcut= 1087;}
	if (lowcut == 35) {dcut= 1031;}
	if (lowcut == 34) {dcut= 1013;}
	if (lowcut == 33) {dcut= 971;}
	if (lowcut == 32) {dcut= 907;}
	if (lowcut == 31) {dcut= 839;}
	if (lowcut == 30) {dcut= 797;}
	if (lowcut == 29) {dcut= 733;}
	if (lowcut == 28) {dcut= 719;}
	if (lowcut == 27) {dcut= 673;}
	if (lowcut == 26) {dcut= 613;}
	if (lowcut == 25) {dcut= 593;}
	if (lowcut == 24) {dcut= 541;}
	if (lowcut == 23) {dcut= 479;}
	if (lowcut == 22) {dcut= 431;}
	if (lowcut == 21) {dcut= 419;}
	if (lowcut == 20) {dcut= 373;}
	if (lowcut == 19) {dcut= 311;}
	if (lowcut == 18) {dcut= 293;}
	if (lowcut == 17) {dcut= 233;}
	if (lowcut == 16) {dcut= 191;}
	if (lowcut == 15) {dcut= 173;}
	if (lowcut == 14) {dcut= 131;}
	if (lowcut == 13) {dcut= 113;}
	if (lowcut == 12) {dcut= 71;}
	if (lowcut == 11) {dcut= 53;}
	if (lowcut == 10) {dcut= 31;}
	if (lowcut == 9) {dcut= 27;}
	if (lowcut == 8) {dcut= 23;}
	if (lowcut == 7) {dcut= 19;}
	if (lowcut == 6) {dcut= 17;}
	if (lowcut == 5) {dcut= 13;}
	if (lowcut == 4) {dcut= 11;}
	if (lowcut == 3) {dcut= 7;}
	if (lowcut == 2) {dcut= 5;}
	if (lowcut < 2) {dcut= 3;}
	highpass = B * 22.0;
	lowcut = floor(highpass)+1;
	
	double decay = 0.001 - ((1.0-pow(1.0-D,3))*0.001);
	if (decay == 0.001) decay = 0.1;
	double wet = F;
	double dry = 1.0 - wet;
	wet *= 0.01; //correct large gain issue
	double correctionSample;
	double accumulatorSampleL;
	double accumulatorSampleR;
	double overallscale = (E*9.0)+1.0;
	double gain = overallscale;
	
	if (gain > 1.0) {f[0] = 1.0; gain -= 1.0;} else {f[0] = gain; gain = 0.0;}
	if (gain > 1.0) {f[1] = 1.0; gain -= 1.0;} else {f[1] = gain; gain = 0.0;}
	if (gain > 1.0) {f[2] = 1.0; gain -= 1.0;} else {f[2] = gain; gain = 0.0;}
	if (gain > 1.0) {f[3] = 1.0; gain -= 1.0;} else {f[3] = gain; gain = 0.0;}
	if (gain > 1.0) {f[4] = 1.0; gain -= 1.0;} else {f[4] = gain; gain = 0.0;}
	if (gain > 1.0) {f[5] = 1.0; gain -= 1.0;} else {f[5] = gain; gain = 0.0;}
	if (gain > 1.0) {f[6] = 1.0; gain -= 1.0;} else {f[6] = gain; gain = 0.0;}
	if (gain > 1.0) {f[7] = 1.0; gain -= 1.0;} else {f[7] = gain; gain = 0.0;}
	if (gain > 1.0) {f[8] = 1.0; gain -= 1.0;} else {f[8] = gain; gain = 0.0;}
	if (gain > 1.0) {f[9] = 1.0; gain -= 1.0;} else {f[9] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders
	
	if (overallscale < 1.0) overallscale = 1.0;
	f[0] /= overallscale;
	f[1] /= overallscale;
	f[2] /= overallscale;
	f[3] /= overallscale;
	f[4] /= overallscale;
	f[5] /= overallscale;
	f[6] /= overallscale;
	f[7] /= overallscale;
	f[8] /= overallscale;
	f[9] /= overallscale;
	//and now it's neatly scaled, too
	
	
	
	
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
		
		if (surgeL<fabs(inputSampleL))
		{
			surgeL += (rand()/(double)RAND_MAX)*(fabs(inputSampleL)-surgeL);
			if (surgeL > 1.0) surgeL = 1.0;
		}
		else
		{
			surgeL -= ((rand()/(double)RAND_MAX)*(surgeL-fabs(inputSampleL))*decay);
			if (surgeL < 0.0) surgeL = 0.0;
		}
		
		cutoffL = pow((cutofftarget*surgeL),5);
		if (cutoffL > 1.0) cutoffL = 1.0;
		invcutoffL = 1.0 - cutoffL;
		//set up modified cutoff L
		
		if (surgeR<fabs(inputSampleR))
		{
			surgeR += (rand()/(double)RAND_MAX)*(fabs(inputSampleR)-surgeR);
			if (surgeR > 1.0) surgeR = 1.0;
		}
		else
		{
			surgeR -= ((rand()/(double)RAND_MAX)*(surgeR-fabs(inputSampleR))*decay);
			if (surgeR < 0.0) surgeR = 0.0;
		}
		
		cutoffR = pow((cutofftarget*surgeR),5);
		if (cutoffR > 1.0) cutoffR = 1.0;
		invcutoffR = 1.0 - cutoffR;
		//set up modified cutoff R
		
		flipL = !flipL;
		flipR = !flipR;
		filterflip = !filterflip;
		quadratic -= 1;
		if (quadratic < 0)
		{
			position += 1;		
			quadratic = position * position;
			quadratic = quadratic % 170003; //% is C++ mod operator
			quadratic *= quadratic;
			quadratic = quadratic % 17011; //% is C++ mod operator
			quadratic *= quadratic;
			//quadratic = quadratic % 1709; //% is C++ mod operator
			//quadratic *= quadratic;
			quadratic = quadratic % dcut; //% is C++ mod operator
			quadratic *= quadratic;
			quadratic = quadratic % lowcut;
			//sets density of the centering force
			if (noiseAL < 0) {flipL = true;}
			else {flipL = false;}
			if (noiseAR < 0) {flipR = true;}
			else {flipR = false;}
		}
		
		
		if (flipL) noiseAL += (rand()/(double)RAND_MAX);
		else noiseAL -= (rand()/(double)RAND_MAX);
		if (flipR) noiseAR += (rand()/(double)RAND_MAX);
		else noiseAR -= (rand()/(double)RAND_MAX);
		
		if (filterflip)
		{
			noiseBL *= invcutoffL; noiseBL += (noiseAL*cutoffL);
			inputSampleL = noiseBL+noiseCL;
			rumbleAL *= (1.0-rumblecutoff);
			rumbleAL += (inputSampleL*rumblecutoff);
			
			noiseBR *= invcutoffR; noiseBR += (noiseAR*cutoffR);
			inputSampleR = noiseBR+noiseCR;
			rumbleAR *= (1.0-rumblecutoff);
			rumbleAR += (inputSampleR*rumblecutoff);
		}
		else 
		{
			noiseCL *= invcutoffL; noiseCL += (noiseAL*cutoffL);
			inputSampleL = noiseBL+noiseCL;
			rumbleBL *= (1.0-rumblecutoff);
			rumbleBL += (inputSampleL*rumblecutoff);
			
			noiseCR *= invcutoffR; noiseCR += (noiseAR*cutoffR);
			inputSampleR = noiseBR+noiseCR;
			rumbleBR *= (1.0-rumblecutoff);
			rumbleBR += (inputSampleR*rumblecutoff);
		}
		
		inputSampleL -= (rumbleAL+rumbleBL);
		inputSampleL *= (1.0-rumblecutoff);

		inputSampleR -= (rumbleAR+rumbleBR);
		inputSampleR *= (1.0-rumblecutoff);

		inputSampleL *= wet;
		inputSampleL += (drySampleL * dry);
		
		inputSampleR *= wet;
		inputSampleR += (drySampleR * dry);
		//apply the dry to the noise
		
		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = accumulatorSampleL = inputSampleL;
		
		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = accumulatorSampleR = inputSampleR;
		
		accumulatorSampleL *= f[0];
		accumulatorSampleL += (bL[1] * f[1]);
		accumulatorSampleL += (bL[2] * f[2]);
		accumulatorSampleL += (bL[3] * f[3]);
		accumulatorSampleL += (bL[4] * f[4]);
		accumulatorSampleL += (bL[5] * f[5]);
		accumulatorSampleL += (bL[6] * f[6]);
		accumulatorSampleL += (bL[7] * f[7]);
		accumulatorSampleL += (bL[8] * f[8]);
		accumulatorSampleL += (bL[9] * f[9]);
		//we are doing our repetitive calculations on a separate value
		accumulatorSampleR *= f[0];
		accumulatorSampleR += (bR[1] * f[1]);
		accumulatorSampleR += (bR[2] * f[2]);
		accumulatorSampleR += (bR[3] * f[3]);
		accumulatorSampleR += (bR[4] * f[4]);
		accumulatorSampleR += (bR[5] * f[5]);
		accumulatorSampleR += (bR[6] * f[6]);
		accumulatorSampleR += (bR[7] * f[7]);
		accumulatorSampleR += (bR[8] * f[8]);
		accumulatorSampleR += (bR[9] * f[9]);
		//we are doing our repetitive calculations on a separate value
		
		correctionSample = inputSampleL - accumulatorSampleL;
		//we're gonna apply the total effect of all these calculations as a single subtract
		//(formerly a more complicated algorithm)
		inputSampleL -= correctionSample;
		//applying the distance calculation to both the dry AND the noise output to blend them
		correctionSample = inputSampleR - accumulatorSampleR;
		//we're gonna apply the total effect of all these calculations as a single subtract
		//(formerly a more complicated algorithm)
		inputSampleR -= correctionSample;
		//applying the distance calculation to both the dry AND the noise output to blend them
		//sometimes I'm really tired and can't do stuff, and I remember trying to simplify this
		//and breaking it somehow. So, there ya go, strange obtuse code.
		
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

void Noise::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double cutoffL;
	double cutoffR;
	double cutofftarget = (A*3.5);
	double rumblecutoff = cutofftarget * 0.005;
	double invcutoffL;
	double invcutoffR;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	double highpass = C*38.0;
	int lowcut = floor(highpass);
	int dcut;
	if (lowcut > 37) {dcut= 1151;}
	if (lowcut == 37) {dcut= 1091;}
	if (lowcut == 36) {dcut= 1087;}
	if (lowcut == 35) {dcut= 1031;}
	if (lowcut == 34) {dcut= 1013;}
	if (lowcut == 33) {dcut= 971;}
	if (lowcut == 32) {dcut= 907;}
	if (lowcut == 31) {dcut= 839;}
	if (lowcut == 30) {dcut= 797;}
	if (lowcut == 29) {dcut= 733;}
	if (lowcut == 28) {dcut= 719;}
	if (lowcut == 27) {dcut= 673;}
	if (lowcut == 26) {dcut= 613;}
	if (lowcut == 25) {dcut= 593;}
	if (lowcut == 24) {dcut= 541;}
	if (lowcut == 23) {dcut= 479;}
	if (lowcut == 22) {dcut= 431;}
	if (lowcut == 21) {dcut= 419;}
	if (lowcut == 20) {dcut= 373;}
	if (lowcut == 19) {dcut= 311;}
	if (lowcut == 18) {dcut= 293;}
	if (lowcut == 17) {dcut= 233;}
	if (lowcut == 16) {dcut= 191;}
	if (lowcut == 15) {dcut= 173;}
	if (lowcut == 14) {dcut= 131;}
	if (lowcut == 13) {dcut= 113;}
	if (lowcut == 12) {dcut= 71;}
	if (lowcut == 11) {dcut= 53;}
	if (lowcut == 10) {dcut= 31;}
	if (lowcut == 9) {dcut= 27;}
	if (lowcut == 8) {dcut= 23;}
	if (lowcut == 7) {dcut= 19;}
	if (lowcut == 6) {dcut= 17;}
	if (lowcut == 5) {dcut= 13;}
	if (lowcut == 4) {dcut= 11;}
	if (lowcut == 3) {dcut= 7;}
	if (lowcut == 2) {dcut= 5;}
	if (lowcut < 2) {dcut= 3;}
	highpass = B * 22.0;
	lowcut = floor(highpass)+1;
	
	double decay = 0.001 - ((1.0-pow(1.0-D,3))*0.001);
	if (decay == 0.001) decay = 0.1;
	double wet = F;
	double dry = 1.0 - wet;
	wet *= 0.01; //correct large gain issue
	double correctionSample;
	double accumulatorSampleL;
	double accumulatorSampleR;
	double overallscale = (E*9.0)+1.0;
	double gain = overallscale;
	
	if (gain > 1.0) {f[0] = 1.0; gain -= 1.0;} else {f[0] = gain; gain = 0.0;}
	if (gain > 1.0) {f[1] = 1.0; gain -= 1.0;} else {f[1] = gain; gain = 0.0;}
	if (gain > 1.0) {f[2] = 1.0; gain -= 1.0;} else {f[2] = gain; gain = 0.0;}
	if (gain > 1.0) {f[3] = 1.0; gain -= 1.0;} else {f[3] = gain; gain = 0.0;}
	if (gain > 1.0) {f[4] = 1.0; gain -= 1.0;} else {f[4] = gain; gain = 0.0;}
	if (gain > 1.0) {f[5] = 1.0; gain -= 1.0;} else {f[5] = gain; gain = 0.0;}
	if (gain > 1.0) {f[6] = 1.0; gain -= 1.0;} else {f[6] = gain; gain = 0.0;}
	if (gain > 1.0) {f[7] = 1.0; gain -= 1.0;} else {f[7] = gain; gain = 0.0;}
	if (gain > 1.0) {f[8] = 1.0; gain -= 1.0;} else {f[8] = gain; gain = 0.0;}
	if (gain > 1.0) {f[9] = 1.0; gain -= 1.0;} else {f[9] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders
	
	if (overallscale < 1.0) overallscale = 1.0;
	f[0] /= overallscale;
	f[1] /= overallscale;
	f[2] /= overallscale;
	f[3] /= overallscale;
	f[4] /= overallscale;
	f[5] /= overallscale;
	f[6] /= overallscale;
	f[7] /= overallscale;
	f[8] /= overallscale;
	f[9] /= overallscale;
	//and now it's neatly scaled, too
	
	
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
		
		if (surgeL<fabs(inputSampleL))
		{
			surgeL += (rand()/(double)RAND_MAX)*(fabs(inputSampleL)-surgeL);
			if (surgeL > 1.0) surgeL = 1.0;
		}
		else
		{
			surgeL -= ((rand()/(double)RAND_MAX)*(surgeL-fabs(inputSampleL))*decay);
			if (surgeL < 0.0) surgeL = 0.0;
		}
		
		cutoffL = pow((cutofftarget*surgeL),5);
		if (cutoffL > 1.0) cutoffL = 1.0;
		invcutoffL = 1.0 - cutoffL;
		//set up modified cutoff L
		
		if (surgeR<fabs(inputSampleR))
		{
			surgeR += (rand()/(double)RAND_MAX)*(fabs(inputSampleR)-surgeR);
			if (surgeR > 1.0) surgeR = 1.0;
		}
		else
		{
			surgeR -= ((rand()/(double)RAND_MAX)*(surgeR-fabs(inputSampleR))*decay);
			if (surgeR < 0.0) surgeR = 0.0;
		}
		
		cutoffR = pow((cutofftarget*surgeR),5);
		if (cutoffR > 1.0) cutoffR = 1.0;
		invcutoffR = 1.0 - cutoffR;
		//set up modified cutoff R
		
		flipL = !flipL;
		flipR = !flipR;
		filterflip = !filterflip;
		quadratic -= 1;
		if (quadratic < 0)
		{
			position += 1;		
			quadratic = position * position;
			quadratic = quadratic % 170003; //% is C++ mod operator
			quadratic *= quadratic;
			quadratic = quadratic % 17011; //% is C++ mod operator
			quadratic *= quadratic;
			//quadratic = quadratic % 1709; //% is C++ mod operator
			//quadratic *= quadratic;
			quadratic = quadratic % dcut; //% is C++ mod operator
			quadratic *= quadratic;
			quadratic = quadratic % lowcut;
			//sets density of the centering force
			if (noiseAL < 0) {flipL = true;}
			else {flipL = false;}
			if (noiseAR < 0) {flipR = true;}
			else {flipR = false;}
		}
		
		
		if (flipL) noiseAL += (rand()/(double)RAND_MAX);
		else noiseAL -= (rand()/(double)RAND_MAX);
		if (flipR) noiseAR += (rand()/(double)RAND_MAX);
		else noiseAR -= (rand()/(double)RAND_MAX);
		
		if (filterflip)
		{
			noiseBL *= invcutoffL; noiseBL += (noiseAL*cutoffL);
			inputSampleL = noiseBL+noiseCL;
			rumbleAL *= (1.0-rumblecutoff);
			rumbleAL += (inputSampleL*rumblecutoff);
			
			noiseBR *= invcutoffR; noiseBR += (noiseAR*cutoffR);
			inputSampleR = noiseBR+noiseCR;
			rumbleAR *= (1.0-rumblecutoff);
			rumbleAR += (inputSampleR*rumblecutoff);
		}
		else 
		{
			noiseCL *= invcutoffL; noiseCL += (noiseAL*cutoffL);
			inputSampleL = noiseBL+noiseCL;
			rumbleBL *= (1.0-rumblecutoff);
			rumbleBL += (inputSampleL*rumblecutoff);
			
			noiseCR *= invcutoffR; noiseCR += (noiseAR*cutoffR);
			inputSampleR = noiseBR+noiseCR;
			rumbleBR *= (1.0-rumblecutoff);
			rumbleBR += (inputSampleR*rumblecutoff);
		}
		
		inputSampleL -= (rumbleAL+rumbleBL);
		inputSampleL *= (1.0-rumblecutoff);
		
		inputSampleR -= (rumbleAR+rumbleBR);
		inputSampleR *= (1.0-rumblecutoff);
		
		inputSampleL *= wet;
		inputSampleL += (drySampleL * dry);
		
		inputSampleR *= wet;
		inputSampleR += (drySampleR * dry);
		//apply the dry to the noise
		
		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = accumulatorSampleL = inputSampleL;
		
		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = accumulatorSampleR = inputSampleR;
		
		accumulatorSampleL *= f[0];
		accumulatorSampleL += (bL[1] * f[1]);
		accumulatorSampleL += (bL[2] * f[2]);
		accumulatorSampleL += (bL[3] * f[3]);
		accumulatorSampleL += (bL[4] * f[4]);
		accumulatorSampleL += (bL[5] * f[5]);
		accumulatorSampleL += (bL[6] * f[6]);
		accumulatorSampleL += (bL[7] * f[7]);
		accumulatorSampleL += (bL[8] * f[8]);
		accumulatorSampleL += (bL[9] * f[9]);
		//we are doing our repetitive calculations on a separate value
		accumulatorSampleR *= f[0];
		accumulatorSampleR += (bR[1] * f[1]);
		accumulatorSampleR += (bR[2] * f[2]);
		accumulatorSampleR += (bR[3] * f[3]);
		accumulatorSampleR += (bR[4] * f[4]);
		accumulatorSampleR += (bR[5] * f[5]);
		accumulatorSampleR += (bR[6] * f[6]);
		accumulatorSampleR += (bR[7] * f[7]);
		accumulatorSampleR += (bR[8] * f[8]);
		accumulatorSampleR += (bR[9] * f[9]);
		//we are doing our repetitive calculations on a separate value
		
		correctionSample = inputSampleL - accumulatorSampleL;
		//we're gonna apply the total effect of all these calculations as a single subtract
		//(formerly a more complicated algorithm)
		inputSampleL -= correctionSample;
		//applying the distance calculation to both the dry AND the noise output to blend them
		correctionSample = inputSampleR - accumulatorSampleR;
		//we're gonna apply the total effect of all these calculations as a single subtract
		//(formerly a more complicated algorithm)
		inputSampleR -= correctionSample;
		//applying the distance calculation to both the dry AND the noise output to blend them
		//sometimes I'm really tired and can't do stuff, and I remember trying to simplify this
		//and breaking it somehow. So, there ya go, strange obtuse code.
		
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

} // end namespace Noise

