/* ========================================
 *  Console6Buss - Console6Buss.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Console6Buss_H
#include "Console6Buss.h"
#endif

namespace Console6Buss {


void Console6Buss::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	double gain = A;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
		if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
		
		
		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
		}		
		
		//encode/decode courtesy of torridgristle under the MIT license
		//Inverse Square 1-(1-x)^2 and 1-(1-x)^0.5
		
		if (inputSampleL > 1.0) inputSampleL= 1.0;
		else if (inputSampleL > 0.0) inputSampleL = 1.0 - pow(1.0-inputSampleL,0.5);
		
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		else if (inputSampleL < 0.0) inputSampleL = -1.0 + pow(1.0+inputSampleL,0.5);
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		else if (inputSampleR > 0.0) inputSampleR = 1.0 - pow(1.0-inputSampleR,0.5);
		
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		else if (inputSampleR < 0.0) inputSampleR = -1.0 + pow(1.0+inputSampleR,0.5);
		
		
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

void Console6Buss::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double gain = A;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
		if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
		
		
		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
		}		
		
		//encode/decode courtesy of torridgristle under the MIT license
		//Inverse Square 1-(1-x)^2 and 1-(1-x)^0.5
		
		if (inputSampleL > 1.0) inputSampleL= 1.0;
		else if (inputSampleL > 0.0) inputSampleL = 1.0 - pow(1.0-inputSampleL,0.5);
		
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		else if (inputSampleL < 0.0) inputSampleL = -1.0 + pow(1.0+inputSampleL,0.5);
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		else if (inputSampleR > 0.0) inputSampleR = 1.0 - pow(1.0-inputSampleR,0.5);
		
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		else if (inputSampleR < 0.0) inputSampleR = -1.0 + pow(1.0+inputSampleR,0.5);
		
		
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


} // end namespace Console6Buss

