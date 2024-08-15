/* ========================================
 *  BrightAmbience2 - BrightAmbience2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BrightAmbience2_H
#include "BrightAmbience2.h"
#endif

namespace BrightAmbience2 {


void BrightAmbience2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	int start = (int)(pow(A,2) * 480)+1;
	int length = (int)(pow(B,2) * 480)+1;
	if (start + length > 488) start = 488-length;
	double feedbackAmount = 1.0-(pow(1.0-C,2));
	double wet = D;
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		long double tempL = 0.0;
		long double tempR = 0.0;
		
		if (gcount < 0 || gcount > 32767) gcount = 32767;
		int count = gcount;
		
		pL[count] = inputSampleL + ((sin(feedbackA)/sqrt(length+1))*feedbackAmount);
		pR[count] = inputSampleR + ((sin(feedbackB)/sqrt(length+1))*feedbackAmount);
		
		for(int offset = start; offset < start+length; offset++) {
			tempL += pL[count+primeL[offset] - ((count+primeL[offset] > 32767)?32768:0)];
			tempR += pR[count+primeR[offset] - ((count+primeR[offset] > 32767)?32768:0)];
		}
		
		inputSampleL = tempL/sqrt(length);
		inputSampleR = tempR/sqrt(length);
		
		feedbackA = (feedbackA * (1.0-feedbackAmount)) + (inputSampleR * feedbackAmount);
		feedbackB = (feedbackB * (1.0-feedbackAmount)) + (inputSampleL * feedbackAmount);
		
		gcount--;
		
		if (wet != 1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace BrightAmbience2

