/* ========================================
 *  DeRez2 - DeRez2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeRez2_H
#include "DeRez2.h"
#endif

namespace DeRez2 {


void DeRez2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	double targetA = pow(A,3)+0.0005;
	if (targetA > 1.0) targetA = 1.0;
	double soften = (1.0 + targetA)/2;
	double targetB = pow(1.0-B,3) / 3;
	double hard = C;
	double wet = D;	

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	targetA /= overallscale;	
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
		if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		
		incrementA = ((incrementA*999.0)+targetA)/1000.0;
		incrementB = ((incrementB*999.0)+targetB)/1000.0;
		//incrementA is the frequency derez
		//incrementB is the bit depth derez
		position += incrementA;
		
		long double outputSampleL = heldSampleL;
		long double outputSampleR = heldSampleR;
		if (position > 1.0)
		{
			position -= 1.0;
			heldSampleL = (lastSampleL * position) + (inputSampleL * (1.0-position));
			outputSampleL = (outputSampleL * (1.0-soften)) + (heldSampleL * soften);
			//softens the edge of the derez
			heldSampleR = (lastSampleR * position) + (inputSampleR * (1.0-position));
			outputSampleR = (outputSampleR * (1.0-soften)) + (heldSampleR * soften);
			//softens the edge of the derez
		}
		inputSampleL = outputSampleL;
		inputSampleR = outputSampleR;
		
		long double tempL = inputSampleL;
		long double tempR = inputSampleR;
		
		if (inputSampleL != lastOutputSampleL) {
			tempL = inputSampleL;
			inputSampleL = (inputSampleL * hard) + (lastDrySampleL * (1.0-hard));
			//transitions get an intermediate dry sample
			lastOutputSampleL = tempL; //only one intermediate sample
		} else {
			lastOutputSampleL = inputSampleL;
		}

		if (inputSampleR != lastOutputSampleR) {
			tempR = inputSampleR;
			inputSampleR = (inputSampleR * hard) + (lastDrySampleR * (1.0-hard));
			//transitions get an intermediate dry sample
			lastOutputSampleR = tempR; //only one intermediate sample
		} else {
			lastOutputSampleR = inputSampleR;
		}
		
		lastDrySampleL = drySampleL;
		lastDrySampleR = drySampleR;
		//freq section of soft/hard interpolates dry samples
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		
		if (inputSampleL > 0) inputSampleL = log(1.0+(255*fabs(inputSampleL))) / log(256);
		if (inputSampleL < 0) inputSampleL = -log(1.0+(255*fabs(inputSampleL))) / log(256);
		
		if (inputSampleR > 0) inputSampleR = log(1.0+(255*fabs(inputSampleR))) / log(256);
		if (inputSampleR < 0) inputSampleR = -log(1.0+(255*fabs(inputSampleR))) / log(256);
		
		inputSampleL = (tempL * hard) + (inputSampleL * (1.0-hard));
		inputSampleR = (tempR * hard) + (inputSampleR * (1.0-hard)); //uLaw encode as part of soft/hard
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (incrementB > 0.0005)
		{
			if (inputSampleL > 0)
			{
				tempL = inputSampleL;
				while (tempL > 0) {tempL -= incrementB;}
				inputSampleL -= tempL;
				//it's below 0 so subtracting adds the remainder
			}
			if (inputSampleR > 0)
			{
				tempR = inputSampleR;
				while (tempR > 0) {tempR -= incrementB;}
				inputSampleR -= tempR;
				//it's below 0 so subtracting adds the remainder
			}
			
			if (inputSampleL < 0)
			{
				tempL = inputSampleL;
				while (tempL < 0) {tempL += incrementB;}
				inputSampleL -= tempL;
				//it's above 0 so subtracting subtracts the remainder
			}
			if (inputSampleR < 0)
			{
				tempR = inputSampleR;
				while (tempR < 0) {tempR += incrementB;}
				inputSampleR -= tempR;
				//it's above 0 so subtracting subtracts the remainder
			}
			
			inputSampleL *= (1.0 - incrementB);
			inputSampleR *= (1.0 - incrementB);
		}
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		
		if (inputSampleL > 0) inputSampleL = (pow(256,fabs(inputSampleL))-1.0) / 255;
		if (inputSampleL < 0) inputSampleL = -(pow(256,fabs(inputSampleL))-1.0) / 255;
		
		if (inputSampleR > 0) inputSampleR = (pow(256,fabs(inputSampleR))-1.0) / 255;
		if (inputSampleR < 0) inputSampleR = -(pow(256,fabs(inputSampleR))-1.0) / 255;
		
		inputSampleL = (tempL * hard) + (inputSampleL * (1.0-hard));
		inputSampleR = (tempR * hard) + (inputSampleR * (1.0-hard)); //uLaw decode as part of soft/hard
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		
		lastSampleL = drySampleL;
		lastSampleR = drySampleR;
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void DeRez2::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double targetA = pow(A,3)+0.0005;
	if (targetA > 1.0) targetA = 1.0;
	double soften = (1.0 + targetA)/2;
	double targetB = pow(1.0-B,3) / 3;
	double hard = C;
	double wet = D;	
	
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	targetA /= overallscale;	
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
		if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		
		incrementA = ((incrementA*999.0)+targetA)/1000.0;
		incrementB = ((incrementB*999.0)+targetB)/1000.0;
		//incrementA is the frequency derez
		//incrementB is the bit depth derez
		position += incrementA;
		
		long double outputSampleL = heldSampleL;
		long double outputSampleR = heldSampleR;
		if (position > 1.0)
		{
			position -= 1.0;
			heldSampleL = (lastSampleL * position) + (inputSampleL * (1.0-position));
			outputSampleL = (outputSampleL * (1.0-soften)) + (heldSampleL * soften);
			//softens the edge of the derez
			heldSampleR = (lastSampleR * position) + (inputSampleR * (1.0-position));
			outputSampleR = (outputSampleR * (1.0-soften)) + (heldSampleR * soften);
			//softens the edge of the derez
		}
		inputSampleL = outputSampleL;
		inputSampleR = outputSampleR;
		
		long double tempL = inputSampleL;
		long double tempR = inputSampleR;
		
		if (inputSampleL != lastOutputSampleL) {
			tempL = inputSampleL;
			inputSampleL = (inputSampleL * hard) + (lastDrySampleL * (1.0-hard));
			//transitions get an intermediate dry sample
			lastOutputSampleL = tempL; //only one intermediate sample
		} else {
			lastOutputSampleL = inputSampleL;
		}
		
		if (inputSampleR != lastOutputSampleR) {
			tempR = inputSampleR;
			inputSampleR = (inputSampleR * hard) + (lastDrySampleR * (1.0-hard));
			//transitions get an intermediate dry sample
			lastOutputSampleR = tempR; //only one intermediate sample
		} else {
			lastOutputSampleR = inputSampleR;
		}
		
		lastDrySampleL = drySampleL;
		lastDrySampleR = drySampleR;
		//freq section of soft/hard interpolates dry samples
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		
		if (inputSampleL > 0) inputSampleL = log(1.0+(255*fabs(inputSampleL))) / log(256);
		if (inputSampleL < 0) inputSampleL = -log(1.0+(255*fabs(inputSampleL))) / log(256);
		
		if (inputSampleR > 0) inputSampleR = log(1.0+(255*fabs(inputSampleR))) / log(256);
		if (inputSampleR < 0) inputSampleR = -log(1.0+(255*fabs(inputSampleR))) / log(256);
		
		inputSampleL = (tempL * hard) + (inputSampleL * (1.0-hard));
		inputSampleR = (tempR * hard) + (inputSampleR * (1.0-hard)); //uLaw encode as part of soft/hard
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (incrementB > 0.0005)
		{
			if (inputSampleL > 0)
			{
				tempL = inputSampleL;
				while (tempL > 0) {tempL -= incrementB;}
				inputSampleL -= tempL;
				//it's below 0 so subtracting adds the remainder
			}
			if (inputSampleR > 0)
			{
				tempR = inputSampleR;
				while (tempR > 0) {tempR -= incrementB;}
				inputSampleR -= tempR;
				//it's below 0 so subtracting adds the remainder
			}
			
			if (inputSampleL < 0)
			{
				tempL = inputSampleL;
				while (tempL < 0) {tempL += incrementB;}
				inputSampleL -= tempL;
				//it's above 0 so subtracting subtracts the remainder
			}
			if (inputSampleR < 0)
			{
				tempR = inputSampleR;
				while (tempR < 0) {tempR += incrementB;}
				inputSampleR -= tempR;
				//it's above 0 so subtracting subtracts the remainder
			}
			
			inputSampleL *= (1.0 - incrementB);
			inputSampleR *= (1.0 - incrementB);
		}
		
		tempL = inputSampleL;
		tempR = inputSampleR;
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		
		if (inputSampleL > 0) inputSampleL = (pow(256,fabs(inputSampleL))-1.0) / 255;
		if (inputSampleL < 0) inputSampleL = -(pow(256,fabs(inputSampleL))-1.0) / 255;
		
		if (inputSampleR > 0) inputSampleR = (pow(256,fabs(inputSampleR))-1.0) / 255;
		if (inputSampleR < 0) inputSampleR = -(pow(256,fabs(inputSampleR))-1.0) / 255;
		
		inputSampleL = (tempL * hard) + (inputSampleL * (1.0-hard));
		inputSampleR = (tempR * hard) + (inputSampleR * (1.0-hard)); //uLaw decode as part of soft/hard
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		
		lastSampleL = drySampleL;
		lastSampleR = drySampleR;
		
		//begin 64 bit stereo floating point dither
		int expon; frexp((double)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		frexp((double)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


} // end namespace DeRez2

