/* ========================================
 *  Loud - Loud.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Loud_H
#include "Loud.h"
#endif

namespace Loud {


void Loud::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double boost = pow(A+1.0,5);
	double output = B;
	double wet = C;
	double dry = 1.0-wet;
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	double clamp;
	
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		//begin L
		inputSampleL *= boost;
		clamp = inputSampleL - lastSampleL;
		
		if (clamp > 0)
		{
			inputSampleL = -(inputSampleL - 1.0);
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = sin(inputSampleL) * overallscale;
			if (clamp > inputSampleL) clamp = inputSampleL;
		}
		
		if (clamp < 0)
		{
			inputSampleL += 1.0;
			inputSampleL *= 1.2566108;
			if (inputSampleL < 0.0) inputSampleL = 0.0;
			if (inputSampleL > 3.141527) inputSampleL = 3.141527;
			inputSampleL = -sin(inputSampleL) * overallscale;
			if (clamp < inputSampleL) clamp = inputSampleL;
		}
		
		inputSampleL = lastSampleL + clamp;
		lastSampleL = inputSampleL;
		//finished L
		
		//begin R
		inputSampleR *= boost;
		clamp = inputSampleR - lastSampleR;
		
		if (clamp > 0)
		{
			inputSampleR = -(inputSampleR - 1.0);
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = sin(inputSampleR) * overallscale;
			if (clamp > inputSampleR) clamp = inputSampleR;
		}
		
		if (clamp < 0)
		{
			inputSampleR += 1.0;
			inputSampleR *= 1.2566108;
			if (inputSampleR < 0.0) inputSampleR = 0.0;
			if (inputSampleR > 3.141527) inputSampleR = 3.141527;
			inputSampleR = -sin(inputSampleR) * overallscale;
			if (clamp < inputSampleR) clamp = inputSampleR;
		}
		
		inputSampleR = lastSampleR + clamp;
		lastSampleR = inputSampleR;
		//finished R
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL*dry)+(inputSampleL*wet);
			inputSampleR = (drySampleR*dry)+(inputSampleR*wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Loud

