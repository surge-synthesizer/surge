/* ========================================
 *  DrumSlam - DrumSlam.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DrumSlam_H
#include "DrumSlam.h"
#endif

namespace DrumSlam {


void DrumSlam::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double iirAmountL = 0.0819;
	iirAmountL /= overallscale;
	double iirAmountH = 0.377933067;
	iirAmountH /= overallscale;
	double drive = (A*3.0)+1.0;
	double out = B;
	double wet = C;
	double dry = 1.0 - wet;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		long double lowSampleL;
		long double lowSampleR;
		long double midSampleL;
		long double midSampleR;
		long double highSampleL;
		long double highSampleR;
		
		inputSampleL *= drive;
		inputSampleR *= drive;
		
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleBL = (iirSampleBL * (1 - iirAmountL)) + (iirSampleAL * iirAmountL);
			lowSampleL = iirSampleBL;
			
			iirSampleAR = (iirSampleAR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleBR = (iirSampleBR * (1 - iirAmountL)) + (iirSampleAR * iirAmountL);
			lowSampleR = iirSampleBR;

			iirSampleEL = (iirSampleEL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleFL = (iirSampleFL * (1 - iirAmountH)) + (iirSampleEL * iirAmountH);
			midSampleL = iirSampleFL - iirSampleBL;

			iirSampleER = (iirSampleER * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleFR = (iirSampleFR * (1 - iirAmountH)) + (iirSampleER * iirAmountH);
			midSampleR = iirSampleFR - iirSampleBR;

			highSampleL = inputSampleL - iirSampleFL;
			highSampleR = inputSampleR - iirSampleFR;
		}
		else
		{
			iirSampleCL = (iirSampleCL * (1 - iirAmountL)) + (inputSampleL * iirAmountL);
			iirSampleDL = (iirSampleDL * (1 - iirAmountL)) + (iirSampleCL * iirAmountL);
			lowSampleL = iirSampleDL;

			iirSampleCR = (iirSampleCR * (1 - iirAmountL)) + (inputSampleR * iirAmountL);
			iirSampleDR = (iirSampleDR * (1 - iirAmountL)) + (iirSampleCR * iirAmountL);
			lowSampleR = iirSampleDR;

			iirSampleGL = (iirSampleGL * (1 - iirAmountH)) + (inputSampleL * iirAmountH);
			iirSampleHL = (iirSampleHL * (1 - iirAmountH)) + (iirSampleGL * iirAmountH);
			midSampleL = iirSampleHL - iirSampleDL;

			iirSampleGR = (iirSampleGR * (1 - iirAmountH)) + (inputSampleR * iirAmountH);
			iirSampleHR = (iirSampleHR * (1 - iirAmountH)) + (iirSampleGR * iirAmountH);
			midSampleR = iirSampleHR - iirSampleDR;

			highSampleL = inputSampleL - iirSampleHL;
			highSampleR = inputSampleR - iirSampleHR;
		}
		fpFlip = !fpFlip;

		//generate the tone bands we're using
		if (lowSampleL > 1.0) {lowSampleL = 1.0;}
		if (lowSampleL < -1.0) {lowSampleL = -1.0;}
		if (lowSampleR > 1.0) {lowSampleR = 1.0;}
		if (lowSampleR < -1.0) {lowSampleR = -1.0;}
		lowSampleL -= (lowSampleL * (fabs(lowSampleL) * 0.448) * (fabs(lowSampleL) * 0.448) );
		lowSampleR -= (lowSampleR * (fabs(lowSampleR) * 0.448) * (fabs(lowSampleR) * 0.448) );
		lowSampleL *= drive;
		lowSampleR *= drive;
		
		if (highSampleL > 1.0) {highSampleL = 1.0;}
		if (highSampleL < -1.0) {highSampleL = -1.0;}
		if (highSampleR > 1.0) {highSampleR = 1.0;}
		if (highSampleR < -1.0) {highSampleR = -1.0;}
		highSampleL -= (highSampleL * (fabs(highSampleL) * 0.599) * (fabs(highSampleL) * 0.599) );
		highSampleR -= (highSampleR * (fabs(highSampleR) * 0.599) * (fabs(highSampleR) * 0.599) );
		highSampleL *= drive;
		highSampleR *= drive;
		
		midSampleL = midSampleL * drive;
		midSampleR = midSampleR * drive;
		
		long double skew = (midSampleL - lastSampleL);
		lastSampleL = midSampleL;
		//skew will be direction/angle
		long double bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleL;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleL);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleL > 0)
		{
			midSampleL = bridgerectifier;
		}
		else
		{
			midSampleL = -bridgerectifier;
		}
		//blend according to positive and negative controls, left
		
		skew = (midSampleR - lastSampleR);
		lastSampleR = midSampleR;
		//skew will be direction/angle
		bridgerectifier = fabs(skew);
		if (bridgerectifier > 3.1415926) bridgerectifier = 3.1415926;
		//for skew we want it to go to zero effect again, so we use full range of the sine
		bridgerectifier = sin(bridgerectifier);
		if (skew > 0) skew = bridgerectifier*3.1415926;
		else skew = -bridgerectifier*3.1415926;
		//skew is now sined and clamped and then re-amplified again
		skew *= midSampleR;
		//cools off sparkliness and crossover distortion
		skew *= 1.557079633;
		//crank up the gain on this so we can make it sing
		bridgerectifier = fabs(midSampleR);
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		bridgerectifier *= drive;
		bridgerectifier += skew;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (midSampleR > 0)
		{
			midSampleR = bridgerectifier;
		}
		else
		{
			midSampleR = -bridgerectifier;
		}
		//blend according to positive and negative controls, right
		
		inputSampleL = ((lowSampleL + midSampleL + highSampleL)/drive)*out;
		inputSampleR = ((lowSampleR + midSampleR + highSampleR)/drive)*out;
		
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

} // end namespace DrumSlam

