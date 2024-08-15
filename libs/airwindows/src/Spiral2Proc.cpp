/* ========================================
 *  Spiral2 - Spiral2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Spiral2_H
#include "Spiral2.h"
#endif

namespace Spiral2 {


void Spiral2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double gain = pow(A*2.0,2.0);
	double iirAmount = pow(B,3.0)/overallscale;
	double presence = C;
	double output = D;
	double wet = E;

    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
			prevSampleL *= gain;
			prevSampleR *= gain;
		}
		
		if (flip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleAL;
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleBL;
			inputSampleR -= iirSampleBR;
		}
		//highpass section
		
		long double presenceSampleL = sin(inputSampleL * fabs(prevSampleL)) / ((prevSampleL == 0.0) ?1:fabs(prevSampleL));
		long double presenceSampleR = sin(inputSampleR * fabs(prevSampleR)) / ((prevSampleR == 0.0) ?1:fabs(prevSampleR));
		//change from first Spiral: delay of one sample on the scaling factor.
		inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
		inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));
		
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
			presenceSampleL *= output;
			presenceSampleR *= output;
		}
		if (presence > 0.0) {
			inputSampleL = (inputSampleL * (1.0-presence)) + (presenceSampleL * presence);
			inputSampleR = (inputSampleR * (1.0-presence)) + (presenceSampleR * presence);
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
			inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.
		
		prevSampleL = drySampleL;
		prevSampleR = drySampleR;
		flip = !flip;
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Spiral2

