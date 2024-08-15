/* ========================================
 *  Slew2 - Slew2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Slew2_H
#include "Slew2.h"
#endif

namespace Slew2 {


void Slew2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 2.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double clamp;
	double threshold = pow((1-A),4)/overallscale;
	double inputSampleL;
	double inputSampleR;
	    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		LataDrySample = inputSampleL;
		RataDrySample = inputSampleR;
		
		LataHalfDrySample = LataHalfwaySample = (inputSampleL + LataLast1Sample + ((-LataLast2Sample + LataLast3Sample) * LataUpsampleHighTweak)) / 2.0;
		LataLast3Sample = LataLast2Sample; LataLast2Sample = LataLast1Sample; LataLast1Sample = inputSampleL;
		//setting up oversampled special antialiasing
		//begin first half- change inputSample -> LataHalfwaySample, LataDrySample -> LataHalfDrySample
		clamp = LataHalfwaySample - LataHalfDrySample;
		if (clamp > threshold)
			LataHalfwaySample = lastSampleL + threshold;
		if (-clamp > threshold)
			LataHalfwaySample = lastSampleL - threshold;
		lastSampleL = LataHalfwaySample;
		//end first half
		//begin antialiasing section for halfway sample
		LataC = LataHalfwaySample - LataHalfDrySample;
		if (LataFlip) {LataA *= LataDecay; LataB *= LataDecay; LataA += LataC; LataB -= LataC; LataC = LataA;}
		else {LataB *= LataDecay; LataA *= LataDecay; LataB += LataC; LataA -= LataC; LataC = LataB;}
		LataHalfDiffSample = (LataC * LataDecay); LataFlip = !LataFlip;
		//end antialiasing section for halfway sample
		//begin second half- inputSample and LataDrySample handled separately here
		clamp = inputSampleL - lastSampleL;
		if (clamp > threshold)
			inputSampleL = lastSampleL + threshold;
		if (-clamp > threshold)
			inputSampleL = lastSampleL - threshold;
		lastSampleL = inputSampleL;
		//end second half
		//begin antialiasing section for input sample
		LataC = inputSampleL - LataDrySample;
		if (LataFlip) {LataA *= LataDecay; LataB *= LataDecay; LataA += LataC; LataB -= LataC; LataC = LataA;}
		else {LataB *= LataDecay; LataA *= LataDecay; LataB += LataC; LataA -= LataC; LataC = LataB;}
		LataDiffSample = (LataC * LataDecay); LataFlip = !LataFlip;
		//end antialiasing section for input sample
		inputSampleL = LataDrySample; inputSampleL += ((LataDiffSample + LataHalfDiffSample + LataPrevDiffSample) / 0.734);
		LataPrevDiffSample = LataDiffSample / 2.0;
		//apply processing as difference to non-oversampled raw input

		
		
		
		RataHalfDrySample = RataHalfwaySample = (inputSampleR + RataLast1Sample + ((-RataLast2Sample + RataLast3Sample) * RataUpsampleHighTweak)) / 2.0;
		RataLast3Sample = RataLast2Sample; RataLast2Sample = RataLast1Sample; RataLast1Sample = inputSampleR;
		//setting up oversampled special antialiasing
		//begin first half- change inputSample -> RataHalfwaySample, RataDrySample -> RataHalfDrySample
		clamp = RataHalfwaySample - RataHalfDrySample;
		if (clamp > threshold)
			RataHalfwaySample = lastSampleR + threshold;
		if (-clamp > threshold)
			RataHalfwaySample = lastSampleR - threshold;
		lastSampleR = RataHalfwaySample;
		//end first half
		//begin antialiasing section for halfway sample
		RataC = RataHalfwaySample - RataHalfDrySample;
		if (RataFlip) {RataA *= RataDecay; RataB *= RataDecay; RataA += RataC; RataB -= RataC; RataC = RataA;}
		else {RataB *= RataDecay; RataA *= RataDecay; RataB += RataC; RataA -= RataC; RataC = RataB;}
		RataHalfDiffSample = (RataC * RataDecay); RataFlip = !RataFlip;
		//end antialiasing section for halfway sample
		//begin second half- inputSample and RataDrySample handled separately here
		clamp = inputSampleR - lastSampleR;
		if (clamp > threshold)
			inputSampleR = lastSampleR + threshold;
		if (-clamp > threshold)
			inputSampleR = lastSampleR - threshold;
		lastSampleR = inputSampleR;
		//end second half
		//begin antialiasing section for input sample
		RataC = inputSampleR - RataDrySample;
		if (RataFlip) {RataA *= RataDecay; RataB *= RataDecay; RataA += RataC; RataB -= RataC; RataC = RataA;}
		else {RataB *= RataDecay; RataA *= RataDecay; RataB += RataC; RataA -= RataC; RataC = RataB;}
		RataDiffSample = (RataC * RataDecay); RataFlip = !RataFlip;
		//end antialiasing section for input sample
		inputSampleR = RataDrySample; inputSampleR += ((RataDiffSample + RataHalfDiffSample + RataPrevDiffSample) / 0.734);
		RataPrevDiffSample = RataDiffSample / 2.0;
		//apply processing as difference to non-oversampled raw input
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Slew2

