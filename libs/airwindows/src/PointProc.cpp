/* ========================================
 *  Point - Point.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Point_H
#include "Point.h"
#endif

namespace Point {


void Point::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double gaintrim = pow(10.0,((A*24.0)-12.0)/20);
	double nibDiv = 1 / pow(C+0.2,7);
	nibDiv /= overallscale;
	double nobDiv;
	if (((B*2.0)-1.0) > 0) nobDiv = nibDiv / (1.001-((B*2.0)-1.0));
	else nobDiv = nibDiv * (1.001-pow(((B*2.0)-1.0)*0.75,2));
	double nibnobFactor = 0.0; //start with the fallthrough value, why not
	double absolute;
	
	long double inputSampleL;
	long double inputSampleR;
	    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		inputSampleL *= gaintrim;
		absolute = fabs(inputSampleL);
		if (fpFlip)
		{
			nibAL = nibAL + (absolute / nibDiv);
			nibAL = nibAL / (1 + (1/nibDiv));
			nobAL = nobAL + (absolute / nobDiv);
			nobAL = nobAL / (1 + (1/nobDiv));
			if (nobAL > 0)
			{
				nibnobFactor = nibAL / nobAL;
			}
		}
		else
		{
			nibBL = nibBL + (absolute / nibDiv);
			nibBL = nibBL / (1 + (1/nibDiv));
			nobBL = nobBL + (absolute / nobDiv);
			nobBL = nobBL / (1 + (1/nobDiv));
			if (nobBL > 0)
			{
				nibnobFactor = nibBL / nobBL;
			}		
		}
		inputSampleL *= nibnobFactor;
		
		
		inputSampleR *= gaintrim;
		absolute = fabs(inputSampleR);
		if (fpFlip)
		{
			nibAR = nibAR + (absolute / nibDiv);
			nibAR = nibAR / (1 + (1/nibDiv));
			nobAR = nobAR + (absolute / nobDiv);
			nobAR = nobAR / (1 + (1/nobDiv));
			if (nobAR > 0)
			{
				nibnobFactor = nibAR / nobAR;
			}
		}
		else
		{
			nibBR = nibBR + (absolute / nibDiv);
			nibBR = nibBR / (1 + (1/nibDiv));
			nobBR = nobBR + (absolute / nobDiv);
			nobBR = nobBR / (1 + (1/nobDiv));
			if (nobBR > 0)
			{
				nibnobFactor = nibBR / nobBR;
			}		
		}
		inputSampleR *= nibnobFactor;
		fpFlip = !fpFlip;
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Point

