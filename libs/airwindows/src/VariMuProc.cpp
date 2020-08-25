/* ========================================
 *  VariMu - VariMu.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __VariMu_H
#include "VariMu.h"
#endif

namespace VariMu {


void VariMu::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 2.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double threshold = 1.001 - (1.0-pow(1.0-A,3));
	double muMakeupGain = sqrt(1.0 / threshold);
	muMakeupGain = (muMakeupGain + sqrt(muMakeupGain))/2.0;
	muMakeupGain = sqrt(muMakeupGain);
	double outGain = sqrt(muMakeupGain);
	//gain settings around threshold
	double release = pow((1.15-B),5)*32768.0;
	release /= overallscale;
	double fastest = sqrt(release);
	//speed settings around release
	double coefficient;
	double output = outGain * C;
	double wet = D;
	long double squaredSampleL;
	long double squaredSampleR;
	
	// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;
		
		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}
		
		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is 
		//effectively digital black, we'll subtract it aVariMu. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		if (fabs(inputSampleL) > fabs(previousL)) squaredSampleL = previousL * previousL;
		else squaredSampleL = inputSampleL * inputSampleL;
		previousL = inputSampleL;
		inputSampleL *= muMakeupGain;

		if (fabs(inputSampleR) > fabs(previousR)) squaredSampleR = previousR * previousR;
		else squaredSampleR = inputSampleR * inputSampleR;
		previousR = inputSampleR;
		inputSampleR *= muMakeupGain;
		
		//adjust coefficients for L
		if (flip)
		{
			if (fabs(squaredSampleL) > threshold)
			{
				muVaryL = threshold / fabs(squaredSampleL);
				muAttackL = sqrt(fabs(muSpeedAL));
				muCoefficientAL = muCoefficientAL * (muAttackL-1.0);
				if (muVaryL < threshold)
				{
					muCoefficientAL = muCoefficientAL + threshold;
				}
				else
				{
					muCoefficientAL = muCoefficientAL + muVaryL;
				}
				muCoefficientAL = muCoefficientAL / muAttackL;
			}
			else
			{
				muCoefficientAL = muCoefficientAL * ((muSpeedAL * muSpeedAL)-1.0);
				muCoefficientAL = muCoefficientAL + 1.0;
				muCoefficientAL = muCoefficientAL / (muSpeedAL * muSpeedAL);
			}
			muNewSpeedL = muSpeedAL * (muSpeedAL-1);
			muNewSpeedL = muNewSpeedL + fabs(squaredSampleL*release)+fastest;
			muSpeedAL = muNewSpeedL / muSpeedAL;
		}
		else
		{
			if (fabs(squaredSampleL) > threshold)
			{
				muVaryL = threshold / fabs(squaredSampleL);
				muAttackL = sqrt(fabs(muSpeedBL));
				muCoefficientBL = muCoefficientBL * (muAttackL-1);
				if (muVaryL < threshold)
				{
					muCoefficientBL = muCoefficientBL + threshold;
				}
				else
				{
					muCoefficientBL = muCoefficientBL + muVaryL;
				}
				muCoefficientBL = muCoefficientBL / muAttackL;
			}
			else
			{
				muCoefficientBL = muCoefficientBL * ((muSpeedBL * muSpeedBL)-1.0);
				muCoefficientBL = muCoefficientBL + 1.0;
				muCoefficientBL = muCoefficientBL / (muSpeedBL * muSpeedBL);
			}
			muNewSpeedL = muSpeedBL * (muSpeedBL-1);
			muNewSpeedL = muNewSpeedL + fabs(squaredSampleL*release)+fastest;
			muSpeedBL = muNewSpeedL / muSpeedBL;
		}
		//got coefficients, adjusted speeds for L
		
		//adjust coefficients for R
		if (flip)
		{
			if (fabs(squaredSampleR) > threshold)
			{
				muVaryR = threshold / fabs(squaredSampleR);
				muAttackR = sqrt(fabs(muSpeedAR));
				muCoefficientAR = muCoefficientAR * (muAttackR-1.0);
				if (muVaryR < threshold)
				{
					muCoefficientAR = muCoefficientAR + threshold;
				}
				else
				{
					muCoefficientAR = muCoefficientAR + muVaryR;
				}
				muCoefficientAR = muCoefficientAR / muAttackR;
			}
			else
			{
				muCoefficientAR = muCoefficientAR * ((muSpeedAR * muSpeedAR)-1.0);
				muCoefficientAR = muCoefficientAR + 1.0;
				muCoefficientAR = muCoefficientAR / (muSpeedAR * muSpeedAR);
			}
			muNewSpeedR = muSpeedAR * (muSpeedAR-1);
			muNewSpeedR = muNewSpeedR + fabs(squaredSampleR*release)+fastest;
			muSpeedAR = muNewSpeedR / muSpeedAR;
		}
		else
		{
			if (fabs(squaredSampleR) > threshold)
			{
				muVaryR = threshold / fabs(squaredSampleR);
				muAttackR = sqrt(fabs(muSpeedBR));
				muCoefficientBR = muCoefficientBR * (muAttackR-1);
				if (muVaryR < threshold)
				{
					muCoefficientBR = muCoefficientBR + threshold;
				}
				else
				{
					muCoefficientBR = muCoefficientBR + muVaryR;
				}
				muCoefficientBR = muCoefficientBR / muAttackR;
			}
			else
			{
				muCoefficientBR = muCoefficientBR * ((muSpeedBR * muSpeedBR)-1.0);
				muCoefficientBR = muCoefficientBR + 1.0;
				muCoefficientBR = muCoefficientBR / (muSpeedBR * muSpeedBR);
			}
			muNewSpeedR = muSpeedBR * (muSpeedBR-1);
			muNewSpeedR = muNewSpeedR + fabs(squaredSampleR*release)+fastest;
			muSpeedBR = muNewSpeedR / muSpeedBR;
		}
		//got coefficients, adjusted speeds for R
				
		if (flip)
		{
			coefficient = (muCoefficientAL + pow(muCoefficientAL,2))/2.0;
			inputSampleL *= coefficient;
			coefficient = (muCoefficientAR + pow(muCoefficientAR,2))/2.0;
			inputSampleR *= coefficient;
		}
		else
		{
			coefficient = (muCoefficientBL + pow(muCoefficientBL,2))/2.0;
			inputSampleL *= coefficient;
			coefficient = (muCoefficientBR + pow(muCoefficientBR,2))/2.0;
			inputSampleR *= coefficient;
		}
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		flip = !flip;		

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
			inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
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

void VariMu::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 2.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double threshold = 1.001 - (1.0-pow(1.0-A,3));
	double muMakeupGain = sqrt(1.0 / threshold);
	muMakeupGain = (muMakeupGain + sqrt(muMakeupGain))/2.0;
	muMakeupGain = sqrt(muMakeupGain);
	double outGain = sqrt(muMakeupGain);
	//gain settings around threshold
	double release = pow((1.15-B),5)*32768.0;
	release /= overallscale;
	double fastest = sqrt(release);
	//speed settings around release
	double coefficient;
	double output = outGain * C;
	double wet = D;
	long double squaredSampleL;
	long double squaredSampleR;
	
	// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;
		
		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}
		
		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is 
		//effectively digital black, we'll subtract it aVariMu. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		if (fabs(inputSampleL) > fabs(previousL)) squaredSampleL = previousL * previousL;
		else squaredSampleL = inputSampleL * inputSampleL;
		previousL = inputSampleL;
		inputSampleL *= muMakeupGain;
		
		if (fabs(inputSampleR) > fabs(previousR)) squaredSampleR = previousR * previousR;
		else squaredSampleR = inputSampleR * inputSampleR;
		previousR = inputSampleR;
		inputSampleR *= muMakeupGain;
		
		//adjust coefficients for L
		if (flip)
		{
			if (fabs(squaredSampleL) > threshold)
			{
				muVaryL = threshold / fabs(squaredSampleL);
				muAttackL = sqrt(fabs(muSpeedAL));
				muCoefficientAL = muCoefficientAL * (muAttackL-1.0);
				if (muVaryL < threshold)
				{
					muCoefficientAL = muCoefficientAL + threshold;
				}
				else
				{
					muCoefficientAL = muCoefficientAL + muVaryL;
				}
				muCoefficientAL = muCoefficientAL / muAttackL;
			}
			else
			{
				muCoefficientAL = muCoefficientAL * ((muSpeedAL * muSpeedAL)-1.0);
				muCoefficientAL = muCoefficientAL + 1.0;
				muCoefficientAL = muCoefficientAL / (muSpeedAL * muSpeedAL);
			}
			muNewSpeedL = muSpeedAL * (muSpeedAL-1);
			muNewSpeedL = muNewSpeedL + fabs(squaredSampleL*release)+fastest;
			muSpeedAL = muNewSpeedL / muSpeedAL;
		}
		else
		{
			if (fabs(squaredSampleL) > threshold)
			{
				muVaryL = threshold / fabs(squaredSampleL);
				muAttackL = sqrt(fabs(muSpeedBL));
				muCoefficientBL = muCoefficientBL * (muAttackL-1);
				if (muVaryL < threshold)
				{
					muCoefficientBL = muCoefficientBL + threshold;
				}
				else
				{
					muCoefficientBL = muCoefficientBL + muVaryL;
				}
				muCoefficientBL = muCoefficientBL / muAttackL;
			}
			else
			{
				muCoefficientBL = muCoefficientBL * ((muSpeedBL * muSpeedBL)-1.0);
				muCoefficientBL = muCoefficientBL + 1.0;
				muCoefficientBL = muCoefficientBL / (muSpeedBL * muSpeedBL);
			}
			muNewSpeedL = muSpeedBL * (muSpeedBL-1);
			muNewSpeedL = muNewSpeedL + fabs(squaredSampleL*release)+fastest;
			muSpeedBL = muNewSpeedL / muSpeedBL;
		}
		//got coefficients, adjusted speeds for L
		
		//adjust coefficients for R
		if (flip)
		{
			if (fabs(squaredSampleR) > threshold)
			{
				muVaryR = threshold / fabs(squaredSampleR);
				muAttackR = sqrt(fabs(muSpeedAR));
				muCoefficientAR = muCoefficientAR * (muAttackR-1.0);
				if (muVaryR < threshold)
				{
					muCoefficientAR = muCoefficientAR + threshold;
				}
				else
				{
					muCoefficientAR = muCoefficientAR + muVaryR;
				}
				muCoefficientAR = muCoefficientAR / muAttackR;
			}
			else
			{
				muCoefficientAR = muCoefficientAR * ((muSpeedAR * muSpeedAR)-1.0);
				muCoefficientAR = muCoefficientAR + 1.0;
				muCoefficientAR = muCoefficientAR / (muSpeedAR * muSpeedAR);
			}
			muNewSpeedR = muSpeedAR * (muSpeedAR-1);
			muNewSpeedR = muNewSpeedR + fabs(squaredSampleR*release)+fastest;
			muSpeedAR = muNewSpeedR / muSpeedAR;
		}
		else
		{
			if (fabs(squaredSampleR) > threshold)
			{
				muVaryR = threshold / fabs(squaredSampleR);
				muAttackR = sqrt(fabs(muSpeedBR));
				muCoefficientBR = muCoefficientBR * (muAttackR-1);
				if (muVaryR < threshold)
				{
					muCoefficientBR = muCoefficientBR + threshold;
				}
				else
				{
					muCoefficientBR = muCoefficientBR + muVaryR;
				}
				muCoefficientBR = muCoefficientBR / muAttackR;
			}
			else
			{
				muCoefficientBR = muCoefficientBR * ((muSpeedBR * muSpeedBR)-1.0);
				muCoefficientBR = muCoefficientBR + 1.0;
				muCoefficientBR = muCoefficientBR / (muSpeedBR * muSpeedBR);
			}
			muNewSpeedR = muSpeedBR * (muSpeedBR-1);
			muNewSpeedR = muNewSpeedR + fabs(squaredSampleR*release)+fastest;
			muSpeedBR = muNewSpeedR / muSpeedBR;
		}
		//got coefficients, adjusted speeds for R
		
		if (flip)
		{
			coefficient = (muCoefficientAL + pow(muCoefficientAL,2))/2.0;
			inputSampleL *= coefficient;
			coefficient = (muCoefficientAR + pow(muCoefficientAR,2))/2.0;
			inputSampleR *= coefficient;
		}
		else
		{
			coefficient = (muCoefficientBL + pow(muCoefficientBL,2))/2.0;
			inputSampleL *= coefficient;
			coefficient = (muCoefficientBR + pow(muCoefficientBR,2))/2.0;
			inputSampleR *= coefficient;
		}
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		flip = !flip;		
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
			inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
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


} // end namespace VariMu

