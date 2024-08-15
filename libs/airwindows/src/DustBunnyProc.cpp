/* ========================================
 *  DustBunny - DustBunny.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DustBunny_H
#include "DustBunny.h"
#endif

namespace DustBunny {


void DustBunny::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	unsigned int bunny = (unsigned int)(pow((1.255-A),5)*1000);
	bunny = (bunny*bunny);

	float inputSampleL;
	float inputSampleR;
	    
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

		if (((*(unsigned int*)&LataHalfwaySample)&bunny)==0) LataHalfwaySample=0.0;
		
		//end first half
		//begin antialiasing section for halfway sample
		LataC = LataHalfwaySample - LataHalfDrySample;
		if (LataFlip) {LataA *= LataDecay; LataB *= LataDecay; LataA += LataC; LataB -= LataC; LataC = LataA;}
		else {LataB *= LataDecay; LataA *= LataDecay; LataB += LataC; LataA -= LataC; LataC = LataB;}
		LataHalfDiffSample = (LataC * LataDecay); LataFlip = !LataFlip;
		//end antialiasing section for halfway sample
		//begin second half- inputSample and LataDrySample handled separately here

		if (((*(unsigned int*)&inputSampleL)&bunny)==0) inputSampleL=0.0;
		
		//end second half
		//begin antialiasing section for input sample
		LataC = inputSampleL - LataDrySample;
		if (LataFlip) {LataA *= LataDecay; LataB *= LataDecay; LataA += LataC; LataB -= LataC; LataC = LataA;}
		else {LataB *= LataDecay; LataA *= LataDecay; LataB += LataC; LataA -= LataC; LataC = LataB;}
		LataDiffSample = (LataC * LataDecay); LataFlip = !LataFlip;
		//end antialiasing section for input sample
		inputSampleL = LataDrySample; inputSampleL += ((LataDiffSample + LataHalfDiffSample + LataPrevDiffSample) / 2.5);
		LataPrevDiffSample = LataDiffSample / 2.0;
		//apply processing as difference to non-oversampled raw input
		
		RataHalfDrySample = RataHalfwaySample = (inputSampleR + RataLast1Sample + ((-RataLast2Sample + RataLast3Sample) * RataUpsampleHighTweak)) / 2.0;
		RataLast3Sample = RataLast2Sample; RataLast2Sample = RataLast1Sample; RataLast1Sample = inputSampleR;
		//setting up oversampled special antialiasing
		//begin first half- change inputSample -> RataHalfwaySample, RataDrySample -> RataHalfDrySample

		if (((*(unsigned int*)&RataHalfwaySample)&bunny)==0) RataHalfwaySample=0.0;
		
		//end first half
		//begin antialiasing section for halfway sample
		RataC = RataHalfwaySample - RataHalfDrySample;
		if (RataFlip) {RataA *= RataDecay; RataB *= RataDecay; RataA += RataC; RataB -= RataC; RataC = RataA;}
		else {RataB *= RataDecay; RataA *= RataDecay; RataB += RataC; RataA -= RataC; RataC = RataB;}
		RataHalfDiffSample = (RataC * RataDecay); RataFlip = !RataFlip;
		//end antialiasing section for halfway sample
		//begin second half- inputSample and RataDrySample handled separately here

		if (((*(unsigned int*)&inputSampleR)&bunny)==0) inputSampleR=0.0;
		
		//end second half
		//begin antialiasing section for input sample
		RataC = inputSampleR - RataDrySample;
		if (RataFlip) {RataA *= RataDecay; RataB *= RataDecay; RataA += RataC; RataB -= RataC; RataC = RataA;}
		else {RataB *= RataDecay; RataA *= RataDecay; RataB += RataC; RataA -= RataC; RataC = RataB;}
		RataDiffSample = (RataC * RataDecay); RataFlip = !RataFlip;
		//end antialiasing section for input sample
		inputSampleR = RataDrySample; inputSampleR += ((RataDiffSample + RataHalfDiffSample + RataPrevDiffSample) / 2.5);
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

} // end namespace DustBunny

