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
		//note: this algorithm does goofy stuff with bit masks, so the 64-bit buss will use floats for processing to produce the same output.
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}
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

void DustBunny::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	unsigned int bunny = (unsigned int)(pow((1.255-A),5)*1000);
	bunny = (bunny*bunny);
	
	long double inputSampleL;
	long double inputSampleR;

    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		//note: this algorithm does goofy stuff with bit masks, so the 64-bit buss will use floats for processing to produce the same output.
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}
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
		inputSampleL = LataDrySample; inputSampleL += ((LataDiffSample + LataHalfDiffSample + LataPrevDiffSample) / 0.734);
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

} // end namespace DustBunny

