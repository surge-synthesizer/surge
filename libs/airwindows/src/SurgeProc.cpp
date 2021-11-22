/* ========================================
 *  Surge - Surge.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Surge_H
#include "Surge.h"
#endif

namespace Surge {


void Surge::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	long double inputSampleL;
	long double inputSampleR;
	long double drySampleL;
	long double drySampleR;
	
	double chaseMax = 0.0;
	double intensity = (1.0-(pow((1.0-A),2)))*0.7;
	double attack = ((intensity+0.1)*0.0005)/overallscale;
	double decay = ((intensity+0.001)*0.00005)/overallscale;
	double wet = B;
	double dry = 1.0 - wet;
	double inputSense;
	
    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		inputSampleL *= 8.0;
		inputSampleR *= 8.0;
		inputSampleL *= intensity;
		inputSampleR *= intensity;
		
		inputSense = fabs(inputSampleL);
		if (fabs(inputSampleR) > inputSense)
			inputSense = fabs(inputSampleR);
		
		if (chaseMax < inputSense) chaseA += attack;
		if (chaseMax > inputSense) chaseA -= decay;
		
		if (chaseA > decay) chaseA = decay;
		if (chaseA < -attack) chaseA = -attack;
		
		chaseB += (chaseA/overallscale);
		if (chaseB > decay) chaseB = decay;
		if (chaseB < -attack) chaseB = -attack;
		
		chaseC += (chaseB/overallscale);
		if (chaseC > decay) chaseC = decay;
		if (chaseC < -attack) chaseC = -attack;
		
		chaseD += (chaseC/overallscale);
		if (chaseD > 1.0) chaseD = 1.0;
		if (chaseD < 0.0) chaseD = 0.0;
		
		chaseMax = chaseA;
		if (chaseMax < chaseB) chaseMax = chaseB;
		if (chaseMax < chaseC) chaseMax = chaseC;
		if (chaseMax < chaseD) chaseMax = chaseD;
		
		inputSampleL *= chaseMax;
		inputSampleL = drySampleL - (inputSampleL * intensity);
		inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
		
		inputSampleR *= chaseMax;
		inputSampleR = drySampleR - (inputSampleR * intensity);
		inputSampleR = (drySampleR * dry) + (inputSampleR * wet);
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Surge

