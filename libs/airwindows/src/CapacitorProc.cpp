/* ========================================
 *  Capacitor - Capacitor.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Capacitor_H
#include "Capacitor.h"
#endif

namespace Capacitor {


void Capacitor::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	lowpassChase = pow(A,2);
	highpassChase = pow(B,2);
	wetChase = C;
	//should not scale with sample rate, because values reaching 1 are important
	//to its ability to bypass when set to max
	double lowpassSpeed = 300 / (fabs( lastLowpass - lowpassChase)+1.0);
	double highpassSpeed = 300 / (fabs( lastHighpass - highpassChase)+1.0);
	double wetSpeed = 300 / (fabs( lastWet - wetChase)+1.0);
	lastLowpass = lowpassChase;
	lastHighpass = highpassChase;
	lastWet = wetChase;
	
	double invLowpass;
	double invHighpass;
	double dry;
	

	long double inputSampleL;
	long double inputSampleR;
	float drySampleL;
	float drySampleR;
	    
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


		lowpassAmount = (((lowpassAmount*lowpassSpeed)+lowpassChase)/(lowpassSpeed + 1.0)); invLowpass = 1.0 - lowpassAmount;
		highpassAmount = (((highpassAmount*highpassSpeed)+highpassChase)/(highpassSpeed + 1.0)); invHighpass = 1.0 - highpassAmount;
		wet = (((wet*wetSpeed)+wetChase)/(wetSpeed+1.0)); dry = 1.0 - wet;
		
		count++; if (count > 5) count = 0; switch (count)
		{
			case 0:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassDL;
				iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassDL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassDR;
				iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassDR;
				break;
			case 1:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassEL;
				iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassEL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassER;
				iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassER;
				break;
			case 2:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassFL;
				iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassFL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassFR;
				iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassFR;
				break;
			case 3:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassDL;
				iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassDL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassDR;
				iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassDR;
				break;
			case 4:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassEL;
				iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassEL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassER;
				iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassER;
				break;
			case 5:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassFL;
				iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassFL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassFR;
				iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassFR;
				break;
		}
		//Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
		//steepens the filter after minimizing artifacts.
		
		inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
		inputSampleR = (drySampleR * dry) + (inputSampleR * wet);

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

void Capacitor::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	lowpassChase = pow(A,2);
	highpassChase = pow(B,2);
	wetChase = C;
	//should not scale with sample rate, because values reaching 1 are important
	//to its ability to bypass when set to max
	double lowpassSpeed = 300 / (fabs( lastLowpass - lowpassChase)+1.0);
	double highpassSpeed = 300 / (fabs( lastHighpass - highpassChase)+1.0);
	double wetSpeed = 300 / (fabs( lastWet - wetChase)+1.0);
	lastLowpass = lowpassChase;
	lastHighpass = highpassChase;
	lastWet = wetChase;
	
	double invLowpass;
	double invHighpass;
	double dry;
	
	
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	

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

		lowpassAmount = (((lowpassAmount*lowpassSpeed)+lowpassChase)/(lowpassSpeed + 1.0)); invLowpass = 1.0 - lowpassAmount;
		highpassAmount = (((highpassAmount*highpassSpeed)+highpassChase)/(highpassSpeed + 1.0)); invHighpass = 1.0 - highpassAmount;
		wet = (((wet*wetSpeed)+wetChase)/(wetSpeed+1.0)); dry = 1.0 - wet;
		
		count++; if (count > 5) count = 0; switch (count)
		{
			case 0:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassDL;
				iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassDL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassDR;
				iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassDR;
				break;
			case 1:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassEL;
				iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassEL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassER;
				iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassER;
				break;
			case 2:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassFL;
				iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassFL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassFR;
				iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassFR;
				break;
			case 3:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassDL;
				iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassDL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassDR;
				iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassDR;
				break;
			case 4:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassBL;
				iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassBL;
				iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassEL;
				iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassEL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassBR;
				iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassBR;
				iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassER;
				iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassER;
				break;
			case 5:
				iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassAL;
				iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassAL;
				iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassCL;
				iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassCL;
				iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount); inputSampleL -= iirHighpassFL;
				iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount); inputSampleL = iirLowpassFL;
				iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassAR;
				iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassAR;
				iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassCR;
				iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassCR;
				iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount); inputSampleR -= iirHighpassFR;
				iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount); inputSampleR = iirLowpassFR;
				break;
		}
		//Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
		//steepens the filter after minimizing artifacts.
		
		inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
		inputSampleR = (drySampleR * dry) + (inputSampleR * wet);
		
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

} // end namespace Capacitor

