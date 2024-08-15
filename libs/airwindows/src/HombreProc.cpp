/* ========================================
 *  Hombre - Hombre.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Hombre_H
#include "Hombre.h"
#endif

namespace Hombre {


void Hombre::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double target = A;
	double offsetA;
	double offsetB;
	int widthA = (int)(1.0*overallscale);
	int widthB = (int)(7.0*overallscale); //max 364 at 44.1, 792 at 96K
	double wet = B;
	double dry = 1.0 - wet;
	double totalL;
	double totalR;
	int count;
	
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		slide = (slide * 0.9997)+(target*0.0003);
		
		offsetA = ((pow(slide,2)) * 77)+3.2;
		offsetB = (3.85 * offsetA)+41;
		offsetA *= overallscale;
		offsetB *= overallscale;
		//adjust for sample rate
		
		if (gcount < 1 || gcount > 2000) {gcount = 2000;}
		count = gcount;
		
		pL[count+2000] = pL[count] = inputSampleL;
		pR[count+2000] = pR[count] = inputSampleR;
		//double buffer
		
		count = (int)(gcount+floor(offsetA));
		
		totalL = pL[count] * 0.391; //less as value moves away from .0
		totalL += pL[count+widthA]; //we can assume always using this in one way or another?
		totalL += pL[count+widthA+widthA] * 0.391; //greater as value moves away from .0
		
		totalR = pR[count] * 0.391; //less as value moves away from .0
		totalR += pR[count+widthA]; //we can assume always using this in one way or another?
		totalR += pR[count+widthA+widthA] * 0.391; //greater as value moves away from .0
		
		inputSampleL += ((totalL * 0.274));
		inputSampleR += ((totalR * 0.274));
		
		count = (int)(gcount+floor(offsetB));
		
		totalL = pL[count] * 0.918; //less as value moves away from .0
		totalL += pL[count+widthB]; //we can assume always using this in one way or another?
		totalL += pL[count+widthB+widthB] * 0.918; //greater as value moves away from .0
		
		totalR = pR[count] * 0.918; //less as value moves away from .0
		totalR += pR[count+widthB]; //we can assume always using this in one way or another?
		totalR += pR[count+widthB+widthB] * 0.918; //greater as value moves away from .0
		
		inputSampleL -= ((totalL * 0.629));
		inputSampleR -= ((totalR * 0.629));
		
		inputSampleL /= 4;
		inputSampleR /= 4;
		
		gcount--;
		//still scrolling through the samples, remember
		
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

} // end namespace Hombre

