/* ========================================
 *  SingleEndedTriode - SingleEndedTriode.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SingleEndedTriode_H
#include "SingleEndedTriode.h"
#endif

namespace SingleEndedTriode {


void SingleEndedTriode::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	double intensity = pow(A,2)*8.0;
	double triode = intensity;
	intensity +=0.001;
	double softcrossover = pow(B,3)/8.0;
	double hardcrossover = pow(C,7)/8.0;
	double wet = D;
	double dry = 1.0 - wet;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		if (triode > 0.0)
		{
			inputSampleL *= intensity;
			inputSampleR *= intensity;
			inputSampleL -= 0.5;
			inputSampleR -= 0.5;
			
			long double bridgerectifier = fabs(inputSampleL);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0) inputSampleL = bridgerectifier;
			else inputSampleL = -bridgerectifier;

			bridgerectifier = fabs(inputSampleR);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0) inputSampleR = bridgerectifier;
			else inputSampleR = -bridgerectifier;

			inputSampleL += postsine;
			inputSampleR += postsine;
			inputSampleL /= intensity;
			inputSampleR /= intensity;
		}
		
		if (softcrossover > 0.0)
		{
			long double bridgerectifier = fabs(inputSampleL);
			if (bridgerectifier > 0.0) bridgerectifier -= (softcrossover*(bridgerectifier+sqrt(bridgerectifier)));
			if (bridgerectifier < 0.0) bridgerectifier = 0;
			if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
			else inputSampleL = -bridgerectifier;				

			bridgerectifier = fabs(inputSampleR);
			if (bridgerectifier > 0.0) bridgerectifier -= (softcrossover*(bridgerectifier+sqrt(bridgerectifier)));
			if (bridgerectifier < 0.0) bridgerectifier = 0;
			if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
			else inputSampleR = -bridgerectifier;				
		}
		
		
		if (hardcrossover > 0.0)
		{
			long double bridgerectifier = fabs(inputSampleL);
			bridgerectifier -= hardcrossover;
			if (bridgerectifier < 0.0) bridgerectifier = 0.0;
			if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
			else inputSampleL = -bridgerectifier;				

			bridgerectifier = fabs(inputSampleR);
			bridgerectifier -= hardcrossover;
			if (bridgerectifier < 0.0) bridgerectifier = 0.0;
			if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
			else inputSampleR = -bridgerectifier;				
		}
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;
		
		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace SingleEndedTriode

