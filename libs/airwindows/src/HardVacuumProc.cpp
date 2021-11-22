/* ========================================
 *  HardVacuum - HardVacuum.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __HardVacuum_H
#include "HardVacuum.h"
#endif

namespace HardVacuum {


void HardVacuum::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double multistage = A*2.0;
	if (multistage > 1) multistage *= multistage;
	//WE MAKE LOUD NOISE! RAWWWK!
	double countdown;
	double warmth = B;
	double invwarmth = 1.0-warmth;
	warmth /= 1.57079633;
	double aura = C*3.1415926;
	double out = D;
	double wet = E;
	double dry = 1.0-wet;
	double drive;
	double positive;
	double negative;
	double bridgerectifierL;
	double bridgerectifierR;
	double skewL;
	double skewR;


	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		
		skewL = (inputSampleL - lastSampleL);
		skewR = (inputSampleR - lastSampleR);
		lastSampleL = inputSampleL;
		lastSampleR = inputSampleR;
		//skew will be direction/angle
		bridgerectifierL = fabs(skewL);
		bridgerectifierR = fabs(skewR);
		if (bridgerectifierL > 3.1415926) bridgerectifierL = 3.1415926;
		if (bridgerectifierR > 3.1415926) bridgerectifierR = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		
		bridgerectifierL = sin(bridgerectifierL);
		bridgerectifierR = sin(bridgerectifierR);
		if (skewL > 0) skewL = bridgerectifierL*aura;
		else skewL = -bridgerectifierL*aura;
		if (skewR > 0) skewR = bridgerectifierR*aura;
		else skewR = -bridgerectifierR*aura;
		//skew is now sined and clamped and then re-amplified again
		skewL *= inputSampleL;
		skewR *= inputSampleR;
		//cools off sparkliness and crossover distortion
		skewL *= 1.557079633;
		skewR *= 1.557079633;
		//crank up the gain on this so we can make it sing
		//We're doing all this here so skew isn't incremented by each stage
		
		countdown = multistage;
		//begin the torture
		
		while (countdown > 0)
		{
			if (countdown > 1.0) drive = 1.557079633;
			else drive = countdown * (1.0+(0.557079633*invwarmth));
			//full crank stages followed by the proportional one
			//whee. 1 at full warmth to 1.5570etc at no warmth
			positive = drive - warmth;
			negative = drive + warmth;
			//set up things so we can do repeated iterations, assuming that
			//wet is always going to be 0-1 as in the previous plug.
			bridgerectifierL = fabs(inputSampleL);
			bridgerectifierR = fabs(inputSampleR);
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//apply it here so we don't overload
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			//the distortion section.
			bridgerectifierL *= drive;
			bridgerectifierR *= drive;
			bridgerectifierL += skewL;
			bridgerectifierR += skewR;
			//again
			if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
			if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
			bridgerectifierL = sin(bridgerectifierL);
			bridgerectifierR = sin(bridgerectifierR);
			if (inputSampleL > 0)
			{
				inputSampleL = (inputSampleL*(1-positive+skewL))+(bridgerectifierL*(positive+skewL));
			}
			else
			{
				inputSampleL = (inputSampleL*(1-negative+skewL))-(bridgerectifierL*(negative+skewL));
			}
			if (inputSampleR > 0)
			{
				inputSampleR = (inputSampleR*(1-positive+skewR))+(bridgerectifierR*(positive+skewR));
			}
			else
			{
				inputSampleR = (inputSampleR*(1-negative+skewR))-(bridgerectifierR*(negative+skewR));
			}
			//blend according to positive and negative controls
			countdown -= 1.0;
			//step down a notch and repeat.
		}

		if (out != 1.0) {
			inputSampleL *= out;
			inputSampleR *= out;
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

} // end namespace HardVacuum

