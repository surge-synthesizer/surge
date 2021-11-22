/* ========================================
 *  DeBess - DeBess.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeBess_H
#include "DeBess.h"
#endif

namespace DeBess {


void DeBess::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double intensity = pow(A,5)*(8192/overallscale);
	double sharpness = B*40.0;
	if (sharpness < 2) sharpness = 2;
	double speed = 0.1 / sharpness;
	double depth = 1.0 / (C+0.0001);
	double iirAmount = D;
	float monitoring = E;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		
		sL[0] = inputSampleL; //set up so both [0] and [1] will be input sample
		sR[0] = inputSampleR; //set up so both [0] and [1] will be input sample
		//we only use the [1] so this is just where samples come in
		for (int x = sharpness; x > 0; x--) {
			sL[x] = sL[x-1];
			sR[x] = sR[x-1];
		} //building up a set of slews
		
		mL[1] = (sL[1]-sL[2])*((sL[1]-sL[2])/1.3);
		mR[1] = (sR[1]-sR[2])*((sR[1]-sR[2])/1.3);
		for (int x = sharpness-1; x > 1; x--) {
			mL[x] = (sL[x]-sL[x+1])*((sL[x-1]-sL[x])/1.3);
			mR[x] = (sR[x]-sR[x+1])*((sR[x-1]-sR[x])/1.3);
		} //building up a set of slews of slews
		
		double senseL = fabs(mL[1] - mL[2]) * sharpness * sharpness;
		double senseR = fabs(mR[1] - mR[2]) * sharpness * sharpness;
		for (int x = sharpness-1; x > 0; x--) {
			double multL = fabs(mL[x] - mL[x+1]) * sharpness * sharpness;
			if (multL < 1.0) senseL *= multL;
			double multR = fabs(mR[x] - mR[x+1]) * sharpness * sharpness;
			if (multR < 1.0) senseR *= multR;
		} //sense is slews of slews times each other
		
		senseL = 1.0+(intensity*intensity*senseL);
		if (senseL > intensity) {senseL = intensity;}
		senseR= 1.0+(intensity*intensity*senseR);
		if (senseR > intensity) {senseR = intensity;}
		
		if (flip) {
			iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			ratioAL = (ratioAL * (1.0-speed))+(senseL * speed);
			ratioAR = (ratioAR * (1.0-speed))+(senseR * speed);
			if (ratioAL > depth) ratioAL = depth;
			if (ratioAR > depth) ratioAR = depth;
			if (ratioAL > 1.0) inputSampleL = iirSampleAL+((inputSampleL-iirSampleAL)/ratioAL);
			if (ratioAR > 1.0) inputSampleR = iirSampleAR+((inputSampleR-iirSampleAR)/ratioAR);
		}
		else {
			iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);	
			iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);	
			ratioBL = (ratioBL * (1.0-speed))+(senseL * speed);			
			ratioBR = (ratioBR * (1.0-speed))+(senseR * speed);			
			if (ratioBL > depth) ratioBL = depth;
			if (ratioBR > depth) ratioBR = depth;
			if (ratioAL > 1.0) inputSampleL = iirSampleBL+((inputSampleL-iirSampleBL)/ratioBL);
			if (ratioAR > 1.0) inputSampleR = iirSampleBR+((inputSampleR-iirSampleBR)/ratioBR);
		}
		flip = !flip;
		
		if (monitoring > 0.49999) {
			inputSampleL = *in1 - inputSampleL;
			inputSampleR = *in2 - inputSampleR;
		}
		//sense monitoring
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace DeBess

