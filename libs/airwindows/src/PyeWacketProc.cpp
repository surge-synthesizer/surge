/* ========================================
 *  Pyewacket - Pyewacket.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Pyewacket_H
#include "PyeWacket.h"
#endif

namespace PyeWacket {


void Pyewacket::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;
	long double inputSampleL;
	long double inputSampleR;
	long double drySampleL;
	long double drySampleR;
	double bridgerectifier;
	double temprectifier;
	double inputSense;
	
	double inputGain = pow(10.0,((A*24.0)-12.0)/20.0);
	double attack = ((B+0.5)*0.006)/overallscale;
	double decay = ((B+0.01)*0.0004)/overallscale;
	double outputGain = pow(10.0,((C*24.0)-12.0)/20.0);
	double wet;
	double maxblur;
	double blurdry;
	double out;
	double dry;
	
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		
		if (inputGain != 1.0) {
			inputSampleL *= inputGain;
			inputSampleR *= inputGain;
		}
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		inputSense = fabs(inputSampleL);
		if (fabs(inputSampleR) > inputSense)
			inputSense = fabs(inputSampleR);
		//we will take the greater of either channel and just use that, then apply the result
		//to both stereo channels.
		if (chase < inputSense) chase += attack;
		if (chase > 1.0) chase = 1.0;
		if (chase > inputSense) chase -= decay;
		if (chase < 0.0) chase = 0.0;
		//chase will be between 0 and ? (if input is super hot)
		out = wet = chase;
		if (wet > 1.0) wet = 1.0;
		maxblur = wet * fpNew;
		blurdry = 1.0 - maxblur;
		//scaled back so that blur remains balance of both
		if (out > fpOld) out = fpOld - (out - fpOld);
		if (out < 0.0) out = 0.0;
		dry = 1.0 - wet;
		
		if (inputSampleL > 1.57079633) inputSampleL = 1.57079633;
		if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
		if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
		if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
		
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		temprectifier = 1-cos(bridgerectifier);
		bridgerectifier = ((lastrectifierL*maxblur) + (temprectifier*blurdry));
		lastrectifierL = temprectifier;
		//starved version is also blurred by one sample
		if (inputSampleL > 0) inputSampleL = (inputSampleL*dry)+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*dry)-(bridgerectifier*out);
		
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		temprectifier = 1-cos(bridgerectifier);
		bridgerectifier = ((lastrectifierR*maxblur) + (temprectifier*blurdry));
		lastrectifierR = temprectifier;
		//starved version is also blurred by one sample
		if (inputSampleR > 0) inputSampleR = (inputSampleR*dry)+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*dry)-(bridgerectifier*out);
		
		if (outputGain != 1.0) {
			inputSampleL *= outputGain;
			inputSampleR *= outputGain;
		}
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace PyeWacket

