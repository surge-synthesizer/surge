/* ========================================
 *  BlockParty - BlockParty.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BlockParty_H
#include "BlockParty.h"
#endif

namespace BlockParty {


void BlockParty::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double targetthreshold = 1.01 - (1.0-pow(1.0-(A*0.5),4));
	double wet = B;
	double voicing = 0.618033988749894848204586;
	if (overallscale > 0.0) voicing /= overallscale;
	//translate to desired sample rate, 44.1K is the base
	if (voicing < 0.0) voicing = 0.0;
	if (voicing > 1.0) voicing = 1.0;
	//some insanity checking
    
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
		//effectively digital black, we'll subtract it again. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		double muMakeupGainL = 1.0 / thresholdL;
		double outMakeupGainL = sqrt(muMakeupGainL);
		muMakeupGainL += outMakeupGainL;
		muMakeupGainL *= 0.5;
		//gain settings around threshold
		double releaseL = mergedCoefficientsL * 32768.0;
		releaseL /= overallscale;
		double fastestL = sqrt(releaseL);
		//speed settings around release
		double lastCorrectionL = mergedCoefficientsL;
		// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~

		double muMakeupGainR = 1.0 / thresholdR;
		double outMakeupGainR = sqrt(muMakeupGainR);
		muMakeupGainR += outMakeupGainR;
		muMakeupGainR *= 0.5;
		//gain settings around threshold
		double releaseR = mergedCoefficientsR * 32768.0;
		releaseR /= overallscale;
		double fastestR = sqrt(releaseR);
		//speed settings around release
		double lastCorrectionR = mergedCoefficientsR;
		// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~
		
		if (muMakeupGainL != 1.0) inputSampleL = inputSampleL * muMakeupGainL;
		if (muMakeupGainR != 1.0) inputSampleR = inputSampleR * muMakeupGainR;
		
		if (count < 1 || count > 3) count = 1;
		switch (count)
		{
			case 1:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedAL));
					muCoefficientAL = muCoefficientAL * (muAttackL-1.0);
					if (muVaryL < thresholdL)
					{
						muCoefficientAL = muCoefficientAL + targetthreshold;
					}
					else
					{
						muCoefficientAL = muCoefficientAL + muVaryL;
					}
					muCoefficientAL = muCoefficientAL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientAL = muCoefficientAL * ((muSpeedAL * muSpeedAL)-1.0);
					muCoefficientAL = muCoefficientAL + 1.0;
					muCoefficientAL = muCoefficientAL / (muSpeedAL * muSpeedAL);
				}
				muNewSpeedL = muSpeedAL * (muSpeedAL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedAL = muNewSpeedL / muSpeedAL;
				lastCoefficientAL = pow(muCoefficientAL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientAL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedAR));
					muCoefficientAR = muCoefficientAR * (muAttackR-1.0);
					if (muVaryR < thresholdR)
					{
						muCoefficientAR = muCoefficientAR + targetthreshold;
					}
					else
					{
						muCoefficientAR = muCoefficientAR + muVaryR;
					}
					muCoefficientAR = muCoefficientAR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientAR = muCoefficientAR * ((muSpeedAR * muSpeedAR)-1.0);
					muCoefficientAR = muCoefficientAR + 1.0;
					muCoefficientAR = muCoefficientAR / (muSpeedAR * muSpeedAR);
				}
				muNewSpeedR = muSpeedAR * (muSpeedAR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedAR = muNewSpeedR / muSpeedAR;
				lastCoefficientAR = pow(muCoefficientAR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientAR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
								
				break;
			case 2:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedBL));
					muCoefficientBL = muCoefficientBL * (muAttackL-1);
					if (muVaryL < thresholdL)
					{
						muCoefficientBL = muCoefficientBL + targetthreshold;
					}
					else
					{
						muCoefficientBL = muCoefficientBL + muVaryL;
					}
					muCoefficientBL = muCoefficientBL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientBL = muCoefficientBL * ((muSpeedBL * muSpeedBL)-1.0);
					muCoefficientBL = muCoefficientBL + 1.0;
					muCoefficientBL = muCoefficientBL / (muSpeedBL * muSpeedBL);
				}
				muNewSpeedL = muSpeedBL * (muSpeedBL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedBL = muNewSpeedL / muSpeedBL;
				lastCoefficientAL = pow(muCoefficientBL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientBL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedBR));
					muCoefficientBR = muCoefficientBR * (muAttackR-1);
					if (muVaryR < thresholdR)
					{
						muCoefficientBR = muCoefficientBR + targetthreshold;
					}
					else
					{
						muCoefficientBR = muCoefficientBR + muVaryR;
					}
					muCoefficientBR = muCoefficientBR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientBR = muCoefficientBR * ((muSpeedBR * muSpeedBR)-1.0);
					muCoefficientBR = muCoefficientBR + 1.0;
					muCoefficientBR = muCoefficientBR / (muSpeedBR * muSpeedBR);
				}
				muNewSpeedR = muSpeedBR * (muSpeedBR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedBR = muNewSpeedR / muSpeedBR;
				lastCoefficientAR = pow(muCoefficientBR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientBR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
				
				break;
			case 3:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedCL));
					muCoefficientCL = muCoefficientCL * (muAttackL-1);
					if (muVaryL < thresholdL)
					{
						muCoefficientCL = muCoefficientCL + targetthreshold;
					}
					else
					{
						muCoefficientCL = muCoefficientCL + muVaryL;
					}
					muCoefficientCL = muCoefficientCL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientCL = muCoefficientCL * ((muSpeedCL * muSpeedCL)-1.0);
					muCoefficientCL = muCoefficientCL + 1.0;
					muCoefficientCL = muCoefficientCL / (muSpeedCL * muSpeedCL);
				}
				muNewSpeedL = muSpeedCL * (muSpeedCL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedCL = muNewSpeedL / muSpeedCL;
				lastCoefficientAL = pow(muCoefficientCL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientCL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedCR));
					muCoefficientCR = muCoefficientCR * (muAttackR-1);
					if (muVaryR < thresholdR)
					{
						muCoefficientCR = muCoefficientCR + targetthreshold;
					}
					else
					{
						muCoefficientCR = muCoefficientCR + muVaryR;
					}
					muCoefficientCR = muCoefficientCR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientCR = muCoefficientCR * ((muSpeedCR * muSpeedCR)-1.0);
					muCoefficientCR = muCoefficientCR + 1.0;
					muCoefficientCR = muCoefficientCR / (muSpeedCR * muSpeedCR);
				}
				muNewSpeedR = muSpeedCR * (muSpeedCR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedCR = muNewSpeedR / muSpeedCR;
				lastCoefficientAR = pow(muCoefficientCR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientCR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
				
				break;
		}		
		count++;
		
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		
		if (fpFlip) {
			//begin L
			if (fabs(inputSampleL) > thresholdBL)
			{
				if (inputSampleL > 0.0) {
					inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				} else {
					inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				}
				muVaryL = targetthreshold / fabs(inputSampleL);
				muAttackL = sqrt(fabs(muSpeedDL));
				muCoefficientDL = muCoefficientDL * (muAttackL-1.0);
				if (muVaryL < thresholdBL)
				{
					muCoefficientDL = muCoefficientDL + targetthreshold;
				}
				else
				{
					muCoefficientDL = muCoefficientDL + muVaryL;
				}
				muCoefficientDL = muCoefficientDL / muAttackL;
			}
			else
			{
				thresholdBL = targetthreshold;
				muCoefficientDL = muCoefficientDL * ((muSpeedDL * muSpeedDL)-1.0);
				muCoefficientDL = muCoefficientDL + 1.0;
				muCoefficientDL = muCoefficientDL / (muSpeedDL * muSpeedDL);
			}
			muNewSpeedL = muSpeedDL * (muSpeedDL-1);
			muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
			muSpeedDL = muNewSpeedL / muSpeedDL;
			lastCoefficientCL = pow(muCoefficientEL,2);
			mergedCoefficientsL += lastCoefficientDL;
			mergedCoefficientsL += lastCoefficientCL;
			lastCoefficientCL *= (1.0-lastCorrectionL);
			lastCoefficientCL += (muCoefficientDL * lastCorrectionL);
			lastCoefficientDL = lastCoefficientCL;
			//end L
			
			//begin R
			if (fabs(inputSampleR) > thresholdBR)
			{
				if (inputSampleR > 0.0) {
					inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				} else {
					inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				}
				muVaryR = targetthreshold / fabs(inputSampleR);
				muAttackR = sqrt(fabs(muSpeedDR));
				muCoefficientDR = muCoefficientDR * (muAttackR-1.0);
				if (muVaryR < thresholdBR)
				{
					muCoefficientDR = muCoefficientDR + targetthreshold;
				}
				else
				{
					muCoefficientDR = muCoefficientDR + muVaryR;
				}
				muCoefficientDR = muCoefficientDR / muAttackR;
			}
			else
			{
				thresholdBR = targetthreshold;
				muCoefficientDR = muCoefficientDR * ((muSpeedDR * muSpeedDR)-1.0);
				muCoefficientDR = muCoefficientDR + 1.0;
				muCoefficientDR = muCoefficientDR / (muSpeedDR * muSpeedDR);
			}
			muNewSpeedR = muSpeedDR * (muSpeedDR-1);
			muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
			muSpeedDR = muNewSpeedR / muSpeedDR;
			lastCoefficientCR = pow(muCoefficientER,2);
			mergedCoefficientsR += lastCoefficientDR;
			mergedCoefficientsR += lastCoefficientCR;
			lastCoefficientCR *= (1.0-lastCorrectionR);
			lastCoefficientCR += (muCoefficientDR * lastCorrectionR);
			lastCoefficientDR = lastCoefficientCR;
			//end R
			
		} else {
			//begin L
			if (fabs(inputSampleL) > thresholdBL)
			{
				if (inputSampleL > 0.0) {
					inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				} else {
					inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				}
				muVaryL = targetthreshold / fabs(inputSampleL);
				muAttackL = sqrt(fabs(muSpeedEL));
				muCoefficientEL = muCoefficientEL * (muAttackL-1.0);
				if (muVaryL < thresholdBL)
				{
					muCoefficientEL = muCoefficientEL + targetthreshold;
				}
				else
				{
					muCoefficientEL = muCoefficientEL + muVaryL;
				}
				muCoefficientEL = muCoefficientEL / muAttackL;
			}
			else
			{
				thresholdBL = targetthreshold;
				muCoefficientEL = muCoefficientEL * ((muSpeedEL * muSpeedEL)-1.0);
				muCoefficientEL = muCoefficientEL + 1.0;
				muCoefficientEL = muCoefficientEL / (muSpeedEL * muSpeedEL);
			}
			muNewSpeedL = muSpeedEL * (muSpeedEL-1);
			muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
			muSpeedEL = muNewSpeedL / muSpeedEL;
			lastCoefficientCL = pow(muCoefficientEL,2);
			mergedCoefficientsL += lastCoefficientDL;
			mergedCoefficientsL += lastCoefficientCL;
			lastCoefficientCL *= (1.0-lastCorrectionL);
			lastCoefficientCL += (muCoefficientEL * lastCorrectionL);
			lastCoefficientDL = lastCoefficientCL;
			//end L
			
			//begin R
			if (fabs(inputSampleR) > thresholdBR)
			{
				if (inputSampleR > 0.0) {
					inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				} else {
					inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				}
				muVaryR = targetthreshold / fabs(inputSampleR);
				muAttackR = sqrt(fabs(muSpeedER));
				muCoefficientER = muCoefficientER * (muAttackR-1.0);
				if (muVaryR < thresholdBR)
				{
					muCoefficientER = muCoefficientER + targetthreshold;
				}
				else
				{
					muCoefficientER = muCoefficientER + muVaryR;
				}
				muCoefficientER = muCoefficientER / muAttackR;
			}
			else
			{
				thresholdBR = targetthreshold;
				muCoefficientER = muCoefficientER * ((muSpeedER * muSpeedER)-1.0);
				muCoefficientER = muCoefficientER + 1.0;
				muCoefficientER = muCoefficientER / (muSpeedER * muSpeedER);
			}
			muNewSpeedR = muSpeedER * (muSpeedER-1);
			muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
			muSpeedER = muNewSpeedR / muSpeedER;
			lastCoefficientCR = pow(muCoefficientER,2);
			mergedCoefficientsR += lastCoefficientDR;
			mergedCoefficientsR += lastCoefficientCR;
			lastCoefficientCR *= (1.0-lastCorrectionR);
			lastCoefficientCR += (muCoefficientER * lastCorrectionR);
			lastCoefficientDR = lastCoefficientCR;
			//end R
			
		}
		mergedCoefficientsL *= 0.25;
		inputSampleL *= mergedCoefficientsL;

		mergedCoefficientsR *= 0.25;
		inputSampleR *= mergedCoefficientsR;

		if (outMakeupGainL != 1.0) inputSampleL = inputSampleL * outMakeupGainL;
		if (outMakeupGainR != 1.0) inputSampleR = inputSampleR * outMakeupGainR;
		
		fpFlip = !fpFlip;
		
		if (wet < 1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		
		if (inputSampleL > 0.999) inputSampleL = 0.999;
		if (inputSampleL < -0.999) inputSampleL = -0.999;
		//iron bar clip comes after the dry/wet: alternate way to clean things up
		if (inputSampleR > 0.999) inputSampleR = 0.999;
		if (inputSampleR < -0.999) inputSampleR = -0.999;
		//iron bar clip comes after the dry/wet: alternate way to clean things up
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += static_cast<int32_t>(fpd) * 5.960464655174751e-36L * pow(2,expon+62);
		frexpf((float)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += static_cast<int32_t>(fpd) * 5.960464655174751e-36L * pow(2,expon+62);
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void BlockParty::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double targetthreshold = 1.01 - (1.0-pow(1.0-(A*0.5),4));
	double wet = B;
	double voicing = 0.618033988749894848204586;
	if (overallscale > 0.0) voicing /= overallscale;
	//translate to desired sample rate, 44.1K is the base
	if (voicing < 0.0) voicing = 0.0;
	if (voicing > 1.0) voicing = 1.0;
	//some insanity checking
    
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
		//effectively digital black, we'll subtract it again. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;

		double muMakeupGainL = 1.0 / thresholdL;
		double outMakeupGainL = sqrt(muMakeupGainL);
		muMakeupGainL += outMakeupGainL;
		muMakeupGainL *= 0.5;
		//gain settings around threshold
		double releaseL = mergedCoefficientsL * 32768.0;
		releaseL /= overallscale;
		double fastestL = sqrt(releaseL);
		//speed settings around release
		double lastCorrectionL = mergedCoefficientsL;
		// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~
		
		double muMakeupGainR = 1.0 / thresholdR;
		double outMakeupGainR = sqrt(muMakeupGainR);
		muMakeupGainR += outMakeupGainR;
		muMakeupGainR *= 0.5;
		//gain settings around threshold
		double releaseR = mergedCoefficientsR * 32768.0;
		releaseR /= overallscale;
		double fastestR = sqrt(releaseR);
		//speed settings around release
		double lastCorrectionR = mergedCoefficientsR;
		// µ µ µ µ µ µ µ µ µ µ µ µ is the kitten song o/~
		
		if (muMakeupGainL != 1.0) inputSampleL = inputSampleL * muMakeupGainL;
		if (muMakeupGainR != 1.0) inputSampleR = inputSampleR * muMakeupGainR;
		
		if (count < 1 || count > 3) count = 1;
		switch (count)
		{
			case 1:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedAL));
					muCoefficientAL = muCoefficientAL * (muAttackL-1.0);
					if (muVaryL < thresholdL)
					{
						muCoefficientAL = muCoefficientAL + targetthreshold;
					}
					else
					{
						muCoefficientAL = muCoefficientAL + muVaryL;
					}
					muCoefficientAL = muCoefficientAL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientAL = muCoefficientAL * ((muSpeedAL * muSpeedAL)-1.0);
					muCoefficientAL = muCoefficientAL + 1.0;
					muCoefficientAL = muCoefficientAL / (muSpeedAL * muSpeedAL);
				}
				muNewSpeedL = muSpeedAL * (muSpeedAL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedAL = muNewSpeedL / muSpeedAL;
				lastCoefficientAL = pow(muCoefficientAL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientAL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedAR));
					muCoefficientAR = muCoefficientAR * (muAttackR-1.0);
					if (muVaryR < thresholdR)
					{
						muCoefficientAR = muCoefficientAR + targetthreshold;
					}
					else
					{
						muCoefficientAR = muCoefficientAR + muVaryR;
					}
					muCoefficientAR = muCoefficientAR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientAR = muCoefficientAR * ((muSpeedAR * muSpeedAR)-1.0);
					muCoefficientAR = muCoefficientAR + 1.0;
					muCoefficientAR = muCoefficientAR / (muSpeedAR * muSpeedAR);
				}
				muNewSpeedR = muSpeedAR * (muSpeedAR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedAR = muNewSpeedR / muSpeedAR;
				lastCoefficientAR = pow(muCoefficientAR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientAR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
				
				break;
			case 2:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedBL));
					muCoefficientBL = muCoefficientBL * (muAttackL-1);
					if (muVaryL < thresholdL)
					{
						muCoefficientBL = muCoefficientBL + targetthreshold;
					}
					else
					{
						muCoefficientBL = muCoefficientBL + muVaryL;
					}
					muCoefficientBL = muCoefficientBL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientBL = muCoefficientBL * ((muSpeedBL * muSpeedBL)-1.0);
					muCoefficientBL = muCoefficientBL + 1.0;
					muCoefficientBL = muCoefficientBL / (muSpeedBL * muSpeedBL);
				}
				muNewSpeedL = muSpeedBL * (muSpeedBL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedBL = muNewSpeedL / muSpeedBL;
				lastCoefficientAL = pow(muCoefficientBL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientBL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedBR));
					muCoefficientBR = muCoefficientBR * (muAttackR-1);
					if (muVaryR < thresholdR)
					{
						muCoefficientBR = muCoefficientBR + targetthreshold;
					}
					else
					{
						muCoefficientBR = muCoefficientBR + muVaryR;
					}
					muCoefficientBR = muCoefficientBR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientBR = muCoefficientBR * ((muSpeedBR * muSpeedBR)-1.0);
					muCoefficientBR = muCoefficientBR + 1.0;
					muCoefficientBR = muCoefficientBR / (muSpeedBR * muSpeedBR);
				}
				muNewSpeedR = muSpeedBR * (muSpeedBR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedBR = muNewSpeedR / muSpeedBR;
				lastCoefficientAR = pow(muCoefficientBR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientBR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
				
				break;
			case 3:
				//begin L
				if (fabs(inputSampleL) > thresholdL)
				{
					if (inputSampleL > 0.0) {
						inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					} else {
						inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
						thresholdL = fabs(inputSampleL);
					}
					muVaryL = targetthreshold / fabs(inputSampleL);
					muAttackL = sqrt(fabs(muSpeedCL));
					muCoefficientCL = muCoefficientCL * (muAttackL-1);
					if (muVaryL < thresholdL)
					{
						muCoefficientCL = muCoefficientCL + targetthreshold;
					}
					else
					{
						muCoefficientCL = muCoefficientCL + muVaryL;
					}
					muCoefficientCL = muCoefficientCL / muAttackL;
				}
				else
				{
					thresholdL = targetthreshold;
					muCoefficientCL = muCoefficientCL * ((muSpeedCL * muSpeedCL)-1.0);
					muCoefficientCL = muCoefficientCL + 1.0;
					muCoefficientCL = muCoefficientCL / (muSpeedCL * muSpeedCL);
				}
				muNewSpeedL = muSpeedCL * (muSpeedCL-1);
				muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
				muSpeedCL = muNewSpeedL / muSpeedCL;
				lastCoefficientAL = pow(muCoefficientCL,2);
				mergedCoefficientsL = lastCoefficientBL;
				mergedCoefficientsL += lastCoefficientAL;
				lastCoefficientAL *= (1.0-lastCorrectionL);
				lastCoefficientAL += (muCoefficientCL * lastCorrectionL);
				lastCoefficientBL = lastCoefficientAL;
				//end L
				
				//begin R
				if (fabs(inputSampleR) > thresholdR)
				{
					if (inputSampleR > 0.0) {
						inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					} else {
						inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
						thresholdR = fabs(inputSampleR);
					}
					muVaryR = targetthreshold / fabs(inputSampleR);
					muAttackR = sqrt(fabs(muSpeedCR));
					muCoefficientCR = muCoefficientCR * (muAttackR-1);
					if (muVaryR < thresholdR)
					{
						muCoefficientCR = muCoefficientCR + targetthreshold;
					}
					else
					{
						muCoefficientCR = muCoefficientCR + muVaryR;
					}
					muCoefficientCR = muCoefficientCR / muAttackR;
				}
				else
				{
					thresholdR = targetthreshold;
					muCoefficientCR = muCoefficientCR * ((muSpeedCR * muSpeedCR)-1.0);
					muCoefficientCR = muCoefficientCR + 1.0;
					muCoefficientCR = muCoefficientCR / (muSpeedCR * muSpeedCR);
				}
				muNewSpeedR = muSpeedCR * (muSpeedCR-1);
				muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
				muSpeedCR = muNewSpeedR / muSpeedCR;
				lastCoefficientAR = pow(muCoefficientCR,2);
				mergedCoefficientsR = lastCoefficientBR;
				mergedCoefficientsR += lastCoefficientAR;
				lastCoefficientAR *= (1.0-lastCorrectionR);
				lastCoefficientAR += (muCoefficientCR * lastCorrectionR);
				lastCoefficientBR = lastCoefficientAR;
				//end R
				
				break;
		}		
		count++;
		
		//applied compression with vari-vari-µ-µ-µ-µ-µ-µ-is-the-kitten-song o/~
		//applied gain correction to control output level- tends to constrain sound rather than inflate it
		
		if (fpFlip) {
			//begin L
			if (fabs(inputSampleL) > thresholdBL)
			{
				if (inputSampleL > 0.0) {
					inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				} else {
					inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				}
				muVaryL = targetthreshold / fabs(inputSampleL);
				muAttackL = sqrt(fabs(muSpeedDL));
				muCoefficientDL = muCoefficientDL * (muAttackL-1.0);
				if (muVaryL < thresholdBL)
				{
					muCoefficientDL = muCoefficientDL + targetthreshold;
				}
				else
				{
					muCoefficientDL = muCoefficientDL + muVaryL;
				}
				muCoefficientDL = muCoefficientDL / muAttackL;
			}
			else
			{
				thresholdBL = targetthreshold;
				muCoefficientDL = muCoefficientDL * ((muSpeedDL * muSpeedDL)-1.0);
				muCoefficientDL = muCoefficientDL + 1.0;
				muCoefficientDL = muCoefficientDL / (muSpeedDL * muSpeedDL);
			}
			muNewSpeedL = muSpeedDL * (muSpeedDL-1);
			muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
			muSpeedDL = muNewSpeedL / muSpeedDL;
			lastCoefficientCL = pow(muCoefficientEL,2);
			mergedCoefficientsL += lastCoefficientDL;
			mergedCoefficientsL += lastCoefficientCL;
			lastCoefficientCL *= (1.0-lastCorrectionL);
			lastCoefficientCL += (muCoefficientDL * lastCorrectionL);
			lastCoefficientDL = lastCoefficientCL;
			//end L
			
			//begin R
			if (fabs(inputSampleR) > thresholdBR)
			{
				if (inputSampleR > 0.0) {
					inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				} else {
					inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				}
				muVaryR = targetthreshold / fabs(inputSampleR);
				muAttackR = sqrt(fabs(muSpeedDR));
				muCoefficientDR = muCoefficientDR * (muAttackR-1.0);
				if (muVaryR < thresholdBR)
				{
					muCoefficientDR = muCoefficientDR + targetthreshold;
				}
				else
				{
					muCoefficientDR = muCoefficientDR + muVaryR;
				}
				muCoefficientDR = muCoefficientDR / muAttackR;
			}
			else
			{
				thresholdBR = targetthreshold;
				muCoefficientDR = muCoefficientDR * ((muSpeedDR * muSpeedDR)-1.0);
				muCoefficientDR = muCoefficientDR + 1.0;
				muCoefficientDR = muCoefficientDR / (muSpeedDR * muSpeedDR);
			}
			muNewSpeedR = muSpeedDR * (muSpeedDR-1);
			muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
			muSpeedDR = muNewSpeedR / muSpeedDR;
			lastCoefficientCR = pow(muCoefficientER,2);
			mergedCoefficientsR += lastCoefficientDR;
			mergedCoefficientsR += lastCoefficientCR;
			lastCoefficientCR *= (1.0-lastCorrectionR);
			lastCoefficientCR += (muCoefficientDR * lastCorrectionR);
			lastCoefficientDR = lastCoefficientCR;
			//end R
			
		} else {
			//begin L
			if (fabs(inputSampleL) > thresholdBL)
			{
				if (inputSampleL > 0.0) {
					inputSampleL = (inputSampleL * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				} else {
					inputSampleL = (inputSampleL * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBL = fabs(inputSampleL);
				}
				muVaryL = targetthreshold / fabs(inputSampleL);
				muAttackL = sqrt(fabs(muSpeedEL));
				muCoefficientEL = muCoefficientEL * (muAttackL-1.0);
				if (muVaryL < thresholdBL)
				{
					muCoefficientEL = muCoefficientEL + targetthreshold;
				}
				else
				{
					muCoefficientEL = muCoefficientEL + muVaryL;
				}
				muCoefficientEL = muCoefficientEL / muAttackL;
			}
			else
			{
				thresholdBL = targetthreshold;
				muCoefficientEL = muCoefficientEL * ((muSpeedEL * muSpeedEL)-1.0);
				muCoefficientEL = muCoefficientEL + 1.0;
				muCoefficientEL = muCoefficientEL / (muSpeedEL * muSpeedEL);
			}
			muNewSpeedL = muSpeedEL * (muSpeedEL-1);
			muNewSpeedL = muNewSpeedL + fabs(inputSampleL*releaseL)+fastestL;
			muSpeedEL = muNewSpeedL / muSpeedEL;
			lastCoefficientCL = pow(muCoefficientEL,2);
			mergedCoefficientsL += lastCoefficientDL;
			mergedCoefficientsL += lastCoefficientCL;
			lastCoefficientCL *= (1.0-lastCorrectionL);
			lastCoefficientCL += (muCoefficientEL * lastCorrectionL);
			lastCoefficientDL = lastCoefficientCL;
			//end L
			
			//begin R
			if (fabs(inputSampleR) > thresholdBR)
			{
				if (inputSampleR > 0.0) {
					inputSampleR = (inputSampleR * voicing) + (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				} else {
					inputSampleR = (inputSampleR * voicing) - (targetthreshold * (1.0-voicing));
					thresholdBR = fabs(inputSampleR);
				}
				muVaryR = targetthreshold / fabs(inputSampleR);
				muAttackR = sqrt(fabs(muSpeedER));
				muCoefficientER = muCoefficientER * (muAttackR-1.0);
				if (muVaryR < thresholdBR)
				{
					muCoefficientER = muCoefficientER + targetthreshold;
				}
				else
				{
					muCoefficientER = muCoefficientER + muVaryR;
				}
				muCoefficientER = muCoefficientER / muAttackR;
			}
			else
			{
				thresholdBR = targetthreshold;
				muCoefficientER = muCoefficientER * ((muSpeedER * muSpeedER)-1.0);
				muCoefficientER = muCoefficientER + 1.0;
				muCoefficientER = muCoefficientER / (muSpeedER * muSpeedER);
			}
			muNewSpeedR = muSpeedER * (muSpeedER-1);
			muNewSpeedR = muNewSpeedR + fabs(inputSampleR*releaseR)+fastestR;
			muSpeedER = muNewSpeedR / muSpeedER;
			lastCoefficientCR = pow(muCoefficientER,2);
			mergedCoefficientsR += lastCoefficientDR;
			mergedCoefficientsR += lastCoefficientCR;
			lastCoefficientCR *= (1.0-lastCorrectionR);
			lastCoefficientCR += (muCoefficientER * lastCorrectionR);
			lastCoefficientDR = lastCoefficientCR;
			//end R
			
		}
		mergedCoefficientsL *= 0.25;
		inputSampleL *= mergedCoefficientsL;
		
		mergedCoefficientsR *= 0.25;
		inputSampleR *= mergedCoefficientsR;
		
		if (outMakeupGainL != 1.0) inputSampleL = inputSampleL * outMakeupGainL;
		if (outMakeupGainR != 1.0) inputSampleR = inputSampleR * outMakeupGainR;
		
		fpFlip = !fpFlip;
		
		if (wet < 1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		
		if (inputSampleL > 0.999) inputSampleL = 0.999;
		if (inputSampleL < -0.999) inputSampleL = -0.999;
		//iron bar clip comes after the dry/wet: alternate way to clean things up
		if (inputSampleR > 0.999) inputSampleR = 0.999;
		if (inputSampleR < -0.999) inputSampleR = -0.999;
		//iron bar clip comes after the dry/wet: alternate way to clean things up
		
		//begin 64 bit stereo floating point dither
		int expon; frexp((double)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += static_cast<int32_t>(fpd) * 1.110223024625156e-44L * pow(2,expon+62);
		frexp((double)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += static_cast<int32_t>(fpd) * 1.110223024625156e-44L * pow(2,expon+62);
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


} // end namespace BlockParty

