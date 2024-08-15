/* ========================================
 *  Mojo - Mojo.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Mojo_H
#include "Mojo.h"
#endif

namespace Mojo {


void Mojo::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double gain = pow(10.0,((A*24.0)-12.0)/20.0);
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
		}		

		long double mojo = pow(fabs(inputSampleL),0.25);
		if (mojo > 0.0) inputSampleL = (sin(inputSampleL * mojo * M_PI * 0.5) / mojo) * 0.987654321;
		//mojo is the one that flattens WAAAAY out very softly before wavefolding
		mojo = pow(fabs(inputSampleR),0.25);
		if (mojo > 0.0) inputSampleR = (sin(inputSampleR * mojo * M_PI * 0.5) / mojo) * 0.987654321;
		//mojo is the one that flattens WAAAAY out very softly before wavefolding
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Mojo

