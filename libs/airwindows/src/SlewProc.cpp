/* ========================================
 *  Slew - Slew.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Slew_H
#include "Slew.h"
#endif

namespace Slew {


void Slew::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double inputSampleL;
	double inputSampleR;
	double outputSampleL;
	double outputSampleR;
	
	double clamp;
	double threshold = pow((1-A),4)/overallscale;


    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;

		clamp = inputSampleL - lastSampleL;
		outputSampleL = inputSampleL;
		if (clamp > threshold)
			outputSampleL = lastSampleL + threshold;
		if (-clamp > threshold)
			outputSampleL = lastSampleL - threshold;
		lastSampleL = outputSampleL;

		clamp = inputSampleR - lastSampleR;
		outputSampleR = inputSampleR;
		if (clamp > threshold)
			outputSampleR = lastSampleR + threshold;
		if (-clamp > threshold)
			outputSampleR = lastSampleR - threshold;
		lastSampleR = outputSampleR;
		
		*out1 = outputSampleL;
		*out2 = outputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Slew

