/* ========================================
 *  Fracture - Fracture.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Fracture_H
#include "Fracture.h"
#endif

namespace Fracture {


void Fracture::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	
	double density = A*4;
	double fracture = (((B*2.999)+1)*3.14159265358979);
	double output = C;
	double wet = D;
	double dry = 1.0-wet;
	double bridgerectifier;
	density = density * fabs(density);
	
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

		inputSampleL *= density;
		inputSampleR *= density;

		bridgerectifier = fabs(inputSampleL)*fracture;
		if (bridgerectifier > fracture) bridgerectifier = fracture;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		//blend according to density control

		bridgerectifier = fabs(inputSampleR)*fracture;
		if (bridgerectifier > fracture) bridgerectifier = fracture;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//blend according to density control
		
		inputSampleL *= output;
		inputSampleR *= output;
		
		inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
		inputSampleR = (drySampleR * dry)+(inputSampleR * wet);		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Fracture

