/* ========================================
 *  OneCornerClip - OneCornerClip.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __OneCornerClip_H
#include "OneCornerClip.h"
#endif

namespace OneCornerClip {


void OneCornerClip::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double inputGain = pow(10.0,(((A*36.0)-12.0)/20.0));
	double posThreshold = B;
	double posTargetL = posThreshold;
	double posTargetR = posThreshold;
	double negThreshold = -C;
	double negTargetL = negThreshold;
	double negTargetR = negThreshold;
	double voicing = D;
	if (voicing == 0.618) voicing = 0.618033988749894848204586;
	//special case: we will do a perfect golden ratio as the default 0.618
	//just 'cos magic universality sauce (seriously, it seems a sweetspot)
	if (overallscale > 0.0) voicing /= overallscale;
	//translate to desired sample rate, 44.1K is the base
	if (voicing < 0.0) voicing = 0.0;
	if (voicing > 1.0) voicing = 1.0;
	//some insanity checking
	double inverseHardness = 1.0 - voicing;
	bool clipEngage = false;	

	double wet = E;
	double dry = 1.0 - wet;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;
	
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		
		if (inputGain != 1.0)
		{
			inputSampleL *= inputGain;
			inputSampleR *= inputGain;
			clipEngage = true;
			//if we are altering gain we will always process
		}
		else
		{
			clipEngage = false;
			//if we are not touching gain, we will bypass unless
			//a clip is actively being softened.
		}
		
		
		if (inputSampleL > posTargetL)
		{
			inputSampleL = (lastSampleL * voicing) + (posThreshold * inverseHardness);
			posTargetL = inputSampleL;
			clipEngage = true;
		}
		else
		{
			posTargetL = posThreshold;
		}
		
		if (inputSampleR > posTargetR)
		{
			inputSampleR = (lastSampleR * voicing) + (posThreshold * inverseHardness);
			posTargetR = inputSampleR;
			clipEngage = true;
		}
		else
		{
			posTargetR = posThreshold;
		}
		
		if (inputSampleL < negTargetL)
		{
			inputSampleL = (lastSampleL * voicing) + (negThreshold * inverseHardness);
			negTargetL = inputSampleL;
			clipEngage = true;
		}
		else {
			negTargetL = negThreshold;
		}
		
		if (inputSampleR < negTargetR)
		{
			inputSampleR = (lastSampleR * voicing) + (negThreshold * inverseHardness);
			negTargetR = inputSampleR;
			clipEngage = true;
		}
		else {
			negTargetR = negThreshold;
		}
		
		lastSampleL = inputSampleL;
		lastSampleR = inputSampleR;
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}
		
		//stereo 32 bit dither, made small and tidy.
		int expon; frexpf((float)inputSampleL, &expon);
		long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
		frexpf((float)inputSampleR, &expon);
		dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
		//end 32 bit dither
		
		if (clipEngage == false)
		{
			inputSampleL = *in1;
			inputSampleR = *in2;
		}
		//fall back to raw passthrough if at all possible

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void OneCornerClip::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double inputGain = pow(10.0,(((A*36.0)-12.0)/20.0));
	double posThreshold = B;
	double posTargetL = posThreshold;
	double posTargetR = posThreshold;
	double negThreshold = -C;
	double negTargetL = negThreshold;
	double negTargetR = negThreshold;
	double voicing = D;
	if (voicing == 0.618) voicing = 0.618033988749894848204586;
	//special case: we will do a perfect golden ratio as the default 0.618
	//just 'cos magic universality sauce (seriously, it seems a sweetspot)
	if (overallscale > 0.0) voicing /= overallscale;
	//translate to desired sample rate, 44.1K is the base
	if (voicing < 0.0) voicing = 0.0;
	if (voicing > 1.0) voicing = 1.0;
	//some insanity checking
	double inverseHardness = 1.0 - voicing;
	bool clipEngage = false;	
	
	double wet = E;
	double dry = 1.0 - wet;
	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;


    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;
		
		if (inputGain != 1.0)
		{
			inputSampleL *= inputGain;
			inputSampleR *= inputGain;
			clipEngage = true;
			//if we are altering gain we will always process
		}
		else
		{
			clipEngage = false;
			//if we are not touching gain, we will bypass unless
			//a clip is actively being softened.
		}
		
		
		if (inputSampleL > posTargetL)
		{
			inputSampleL = (lastSampleL * voicing) + (posThreshold * inverseHardness);
			posTargetL = inputSampleL;
			clipEngage = true;
		}
		else
		{
			posTargetL = posThreshold;
		}
		
		if (inputSampleR > posTargetR)
		{
			inputSampleR = (lastSampleR * voicing) + (posThreshold * inverseHardness);
			posTargetR = inputSampleR;
			clipEngage = true;
		}
		else
		{
			posTargetR = posThreshold;
		}
		
		if (inputSampleL < negTargetL)
		{
			inputSampleL = (lastSampleL * voicing) + (negThreshold * inverseHardness);
			negTargetL = inputSampleL;
			clipEngage = true;
		}
		else {
			negTargetL = negThreshold;
		}
		
		if (inputSampleR < negTargetR)
		{
			inputSampleR = (lastSampleR * voicing) + (negThreshold * inverseHardness);
			negTargetR = inputSampleR;
			clipEngage = true;
		}
		else {
			negTargetR = negThreshold;
		}
		
		lastSampleL = inputSampleL;
		lastSampleR = inputSampleR;
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}
		
		//stereo 64 bit dither, made small and tidy.
		int expon; frexp((double)inputSampleL, &expon);
		long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		dither /= 536870912.0; //needs this to scale to 64 bit zone
		inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
		frexp((double)inputSampleR, &expon);
		dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		dither /= 536870912.0; //needs this to scale to 64 bit zone
		inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
		//end 64 bit dither
		
		if (clipEngage == false)
		{
			inputSampleL = *in1;
			inputSampleR = *in2;
		}
		//fall back to raw passthrough if at all possible

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace OneCornerClip

