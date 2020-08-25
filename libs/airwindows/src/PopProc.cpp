/* ========================================
 *  Pop - Pop.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Pop_H
#include "Pop.h"
#endif

namespace Pop {


void Pop::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double highGainOffset = pow(A,2)*0.023;
	double threshold = 1.001 - (1.0-pow(1.0-A,5));
	double muMakeupGain = sqrt(1.0 / threshold);
	//gain settings around threshold
	double release = (A*100000.0) + 300000.0;
	int maxdelay = (int)(1450.0 * overallscale);
	if (maxdelay > 9999) maxdelay = 9999;
	release /= overallscale;
	double fastest = sqrt(release);
	//speed settings around release
	double output = B;
	double wet = C;
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
		//effectively digital black, we'll subtract it aPop. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		dL[delay] = inputSampleL;
		dR[delay] = inputSampleR;
		delay--;
		if (delay < 0 || delay > maxdelay) {delay = maxdelay;}
		//yes this is a second bounds check. it's cheap, check EVERY time
		inputSampleL = (inputSampleL * thickenL) + (dL[delay] * (1.0-thickenL));
		inputSampleR = (inputSampleR * thickenR) + (dR[delay] * (1.0-thickenR));
				
		long double lowestSampleL = inputSampleL;
		if (fabs(inputSampleL) > fabs(previousL)) lowestSampleL = previousL;
		if (fabs(lowestSampleL) > fabs(previous2L)) lowestSampleL = (lowestSampleL + previous2L) / 1.99;
		if (fabs(lowestSampleL) > fabs(previous3L)) lowestSampleL = (lowestSampleL + previous3L) / 1.98;
		if (fabs(lowestSampleL) > fabs(previous4L)) lowestSampleL = (lowestSampleL + previous4L) / 1.97;
		if (fabs(lowestSampleL) > fabs(previous5L)) lowestSampleL = (lowestSampleL + previous5L) / 1.96;
		previous5L = previous4L;
		previous4L = previous3L;
		previous3L = previous2L;
		previous2L = previousL;
		previousL = inputSampleL;
		inputSampleL *= muMakeupGain;
		double punchinessL = 0.95-fabs(inputSampleL*0.08);
		if (punchinessL < 0.65) punchinessL = 0.65;
		
		long double lowestSampleR = inputSampleR;
		if (fabs(inputSampleR) > fabs(previousR)) lowestSampleR = previousR;
		if (fabs(lowestSampleR) > fabs(previous2R)) lowestSampleR = (lowestSampleR + previous2R) / 1.99;
		if (fabs(lowestSampleR) > fabs(previous3R)) lowestSampleR = (lowestSampleR + previous3R) / 1.98;
		if (fabs(lowestSampleR) > fabs(previous4R)) lowestSampleR = (lowestSampleR + previous4R) / 1.97;
		if (fabs(lowestSampleR) > fabs(previous5R)) lowestSampleR = (lowestSampleR + previous5R) / 1.96;
		previous5R = previous4R;
		previous4R = previous3R;
		previous3R = previous2R;
		previous2R = previousR;
		previousR = inputSampleR;
		inputSampleR *= muMakeupGain;
		double punchinessR = 0.95-fabs(inputSampleR*0.08);
		if (punchinessR < 0.65) punchinessR = 0.65;
		
		//adjust coefficients for L
		if (flip)
		{
			if (fabs(lowestSampleL) > threshold)
			{
				muVaryL = threshold / fabs(lowestSampleL);
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
			muNewSpeedL = muNewSpeedL + fabs(lowestSampleL*release)+fastest;
			muSpeedAL = muNewSpeedL / muSpeedAL;
		}
		else
		{
			if (fabs(lowestSampleL) > threshold)
			{
				muVaryL = threshold / fabs(lowestSampleL);
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
			muNewSpeedL = muNewSpeedL + fabs(lowestSampleL*release)+fastest;
			muSpeedBL = muNewSpeedL / muSpeedBL;
		}
		//got coefficients, adjusted speeds for L
		
		//adjust coefficients for R
		if (flip)
		{
			if (fabs(lowestSampleR) > threshold)
			{
				muVaryR = threshold / fabs(lowestSampleR);
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
			muNewSpeedR = muNewSpeedR + fabs(lowestSampleR*release)+fastest;
			muSpeedAR = muNewSpeedR / muSpeedAR;
		}
		else
		{
			if (fabs(lowestSampleR) > threshold)
			{
				muVaryR = threshold / fabs(lowestSampleR);
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
			muNewSpeedR = muNewSpeedR + fabs(lowestSampleR*release)+fastest;
			muSpeedBR = muNewSpeedR / muSpeedBR;
		}
		//got coefficients, adjusted speeds for R
		
		long double coefficientL = highGainOffset;
		if (flip) coefficientL += pow(muCoefficientAL,2);
		else coefficientL += pow(muCoefficientBL,2);
		inputSampleL *= coefficientL;
		thickenL = (coefficientL/5)+punchinessL;//0.80;
		thickenL = (1.0-wet)+(wet*thickenL);

		long double coefficientR = highGainOffset;
		if (flip) coefficientR += pow(muCoefficientAR,2);
		else coefficientR += pow(muCoefficientBR,2);
		inputSampleR *= coefficientR;
		thickenR = (coefficientR/5)+punchinessR;//0.80;
		thickenR = (1.0-wet)+(wet*thickenR);
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		
		long double bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.2533141373155) bridgerectifier = 1.2533141373155;
		bridgerectifier = sin(bridgerectifier * fabs(bridgerectifier)) / ((bridgerectifier == 0.0) ?1:fabs(bridgerectifier));
		//using Spiral instead of Density algorithm
		if (inputSampleL > 0) inputSampleL = (inputSampleL*coefficientL)+(bridgerectifier*(1-coefficientL));
		else inputSampleL = (inputSampleL*coefficientL)-(bridgerectifier*(1-coefficientL));
		//second stage of overdrive to prevent overs and allow bloody loud extremeness

		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.2533141373155) bridgerectifier = 1.2533141373155;
		bridgerectifier = sin(bridgerectifier * fabs(bridgerectifier)) / ((bridgerectifier == 0.0) ?1:fabs(bridgerectifier));
		//using Spiral instead of Density algorithm
		if (inputSampleR > 0) inputSampleR = (inputSampleR*coefficientR)+(bridgerectifier*(1-coefficientR));
		else inputSampleR = (inputSampleR*coefficientR)-(bridgerectifier*(1-coefficientR));
		//second stage of overdrive to prevent overs and allow bloody loud extremeness
		
		flip = !flip;
		
		if (output < 1.0) {inputSampleL *= output;inputSampleR *= output;}
		if (wet<1.0) {
			inputSampleL = (drySampleL*(1.0-wet))+(inputSampleL*wet);
			inputSampleR = (drySampleR*(1.0-wet))+(inputSampleR*wet);
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

void Pop::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double highGainOffset = pow(A,2)*0.023;
	double threshold = 1.001 - (1.0-pow(1.0-A,5));
	double muMakeupGain = sqrt(1.0 / threshold);
	//gain settings around threshold
	double release = (A*100000.0) + 300000.0;
	int maxdelay = (int)(1450.0 * overallscale);
	if (maxdelay > 9999) maxdelay = 9999;
	release /= overallscale;
	double fastest = sqrt(release);
	//speed settings around release
	double output = B;
	double wet = C;
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
		//effectively digital black, we'll subtract it aPop. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		dL[delay] = inputSampleL;
		dR[delay] = inputSampleR;
		delay--;
		if (delay < 0 || delay > maxdelay) {delay = maxdelay;}
		//yes this is a second bounds check. it's cheap, check EVERY time
		inputSampleL = (inputSampleL * thickenL) + (dL[delay] * (1.0-thickenL));
		inputSampleR = (inputSampleR * thickenR) + (dR[delay] * (1.0-thickenR));
		
		long double lowestSampleL = inputSampleL;
		if (fabs(inputSampleL) > fabs(previousL)) lowestSampleL = previousL;
		if (fabs(lowestSampleL) > fabs(previous2L)) lowestSampleL = (lowestSampleL + previous2L) / 1.99;
		if (fabs(lowestSampleL) > fabs(previous3L)) lowestSampleL = (lowestSampleL + previous3L) / 1.98;
		if (fabs(lowestSampleL) > fabs(previous4L)) lowestSampleL = (lowestSampleL + previous4L) / 1.97;
		if (fabs(lowestSampleL) > fabs(previous5L)) lowestSampleL = (lowestSampleL + previous5L) / 1.96;
		previous5L = previous4L;
		previous4L = previous3L;
		previous3L = previous2L;
		previous2L = previousL;
		previousL = inputSampleL;
		inputSampleL *= muMakeupGain;
		double punchinessL = 0.95-fabs(inputSampleL*0.08);
		if (punchinessL < 0.65) punchinessL = 0.65;
		
		long double lowestSampleR = inputSampleR;
		if (fabs(inputSampleR) > fabs(previousR)) lowestSampleR = previousR;
		if (fabs(lowestSampleR) > fabs(previous2R)) lowestSampleR = (lowestSampleR + previous2R) / 1.99;
		if (fabs(lowestSampleR) > fabs(previous3R)) lowestSampleR = (lowestSampleR + previous3R) / 1.98;
		if (fabs(lowestSampleR) > fabs(previous4R)) lowestSampleR = (lowestSampleR + previous4R) / 1.97;
		if (fabs(lowestSampleR) > fabs(previous5R)) lowestSampleR = (lowestSampleR + previous5R) / 1.96;
		previous5R = previous4R;
		previous4R = previous3R;
		previous3R = previous2R;
		previous2R = previousR;
		previousR = inputSampleR;
		inputSampleR *= muMakeupGain;
		double punchinessR = 0.95-fabs(inputSampleR*0.08);
		if (punchinessR < 0.65) punchinessR = 0.65;
		
		//adjust coefficients for L
		if (flip)
		{
			if (fabs(lowestSampleL) > threshold)
			{
				muVaryL = threshold / fabs(lowestSampleL);
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
			muNewSpeedL = muNewSpeedL + fabs(lowestSampleL*release)+fastest;
			muSpeedAL = muNewSpeedL / muSpeedAL;
		}
		else
		{
			if (fabs(lowestSampleL) > threshold)
			{
				muVaryL = threshold / fabs(lowestSampleL);
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
			muNewSpeedL = muNewSpeedL + fabs(lowestSampleL*release)+fastest;
			muSpeedBL = muNewSpeedL / muSpeedBL;
		}
		//got coefficients, adjusted speeds for L
		
		//adjust coefficients for R
		if (flip)
		{
			if (fabs(lowestSampleR) > threshold)
			{
				muVaryR = threshold / fabs(lowestSampleR);
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
			muNewSpeedR = muNewSpeedR + fabs(lowestSampleR*release)+fastest;
			muSpeedAR = muNewSpeedR / muSpeedAR;
		}
		else
		{
			if (fabs(lowestSampleR) > threshold)
			{
				muVaryR = threshold / fabs(lowestSampleR);
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
			muNewSpeedR = muNewSpeedR + fabs(lowestSampleR*release)+fastest;
			muSpeedBR = muNewSpeedR / muSpeedBR;
		}
		//got coefficients, adjusted speeds for R
		
		long double coefficientL = highGainOffset;
		if (flip) coefficientL += pow(muCoefficientAL,2);
		else coefficientL += pow(muCoefficientBL,2);
		inputSampleL *= coefficientL;
		thickenL = (coefficientL/5)+punchinessL;//0.80;
		thickenL = (1.0-wet)+(wet*thickenL);
		
		long double coefficientR = highGainOffset;
		if (flip) coefficientR += pow(muCoefficientAR,2);
		else coefficientR += pow(muCoefficientBR,2);
		inputSampleR *= coefficientR;
		thickenR = (coefficientR/5)+punchinessR;//0.80;
		thickenR = (1.0-wet)+(wet*thickenR);
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		
		long double bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.2533141373155) bridgerectifier = 1.2533141373155;
		bridgerectifier = sin(bridgerectifier * fabs(bridgerectifier)) / ((bridgerectifier == 0.0) ?1:fabs(bridgerectifier));
		//using Spiral instead of Density algorithm
		if (inputSampleL > 0) inputSampleL = (inputSampleL*coefficientL)+(bridgerectifier*(1-coefficientL));
		else inputSampleL = (inputSampleL*coefficientL)-(bridgerectifier*(1-coefficientL));
		//second stage of overdrive to prevent overs and allow bloody loud extremeness
		
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.2533141373155) bridgerectifier = 1.2533141373155;
		bridgerectifier = sin(bridgerectifier * fabs(bridgerectifier)) / ((bridgerectifier == 0.0) ?1:fabs(bridgerectifier));
		//using Spiral instead of Density algorithm
		if (inputSampleR > 0) inputSampleR = (inputSampleR*coefficientR)+(bridgerectifier*(1-coefficientR));
		else inputSampleR = (inputSampleR*coefficientR)-(bridgerectifier*(1-coefficientR));
		//second stage of overdrive to prevent overs and allow bloody loud extremeness
		
		flip = !flip;
		
		if (output < 1.0) {inputSampleL *= output;inputSampleR *= output;}
		if (wet<1.0) {
			inputSampleL = (drySampleL*(1.0-wet))+(inputSampleL*wet);
			inputSampleR = (drySampleR*(1.0-wet))+(inputSampleR*wet);
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


} // end namespace Pop

