/* ========================================
 *  Drive - Drive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Drive_H
#include "Drive.h"
#endif

namespace Drive {


void Drive::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double driveone = pow(A*2.0,2);
	double iirAmount = pow(B,3)/overallscale;
	double output = C;
	double wet = D;
	double dry = 1.0-wet;
	double glitch = 0.60;
	double out;
	

	long double inputSampleL;
	long double inputSampleR;
	long double drySampleL;
	long double drySampleR;
	    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleAL;
			iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleBL;
			iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleBR;
		}
		//highpass section
		fpFlip = !fpFlip;
		
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		
		out = driveone;
		while (out > glitch)
		{
			out -= glitch;
			inputSampleL -= (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch) );
			inputSampleL *= (1.0+glitch);
			inputSampleR -= (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch) );
			inputSampleR *= (1.0+glitch);
		}
		//that's taken care of the really high gain stuff
		
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * out) * (fabs(inputSampleL) * out) );
		inputSampleL *= (1.0+out);
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * out) * (fabs(inputSampleR) * out) );
		inputSampleR *= (1.0+out);
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
			inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
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

} // end namespace Drive

