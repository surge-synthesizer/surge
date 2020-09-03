/* ========================================
 *  CrunchyGrooveWear - CrunchyGrooveWear.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __CrunchyGrooveWear_H
#include "CrunchyGrooveWear.h"
#endif

namespace CrunchyGrooveWear {


void CrunchyGrooveWear::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	
	double overallscale = (pow(A,2)*19.0)+1.0;
	double gain = overallscale;
	//mid groove wear
	if (gain > 1.0) {fMid[0] = 1.0; gain -= 1.0;} else {fMid[0] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[1] = 1.0; gain -= 1.0;} else {fMid[1] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[2] = 1.0; gain -= 1.0;} else {fMid[2] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[3] = 1.0; gain -= 1.0;} else {fMid[3] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[4] = 1.0; gain -= 1.0;} else {fMid[4] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[5] = 1.0; gain -= 1.0;} else {fMid[5] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[6] = 1.0; gain -= 1.0;} else {fMid[6] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[7] = 1.0; gain -= 1.0;} else {fMid[7] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[8] = 1.0; gain -= 1.0;} else {fMid[8] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[9] = 1.0; gain -= 1.0;} else {fMid[9] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[10] = 1.0; gain -= 1.0;} else {fMid[10] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[11] = 1.0; gain -= 1.0;} else {fMid[11] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[12] = 1.0; gain -= 1.0;} else {fMid[12] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[13] = 1.0; gain -= 1.0;} else {fMid[13] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[14] = 1.0; gain -= 1.0;} else {fMid[14] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[15] = 1.0; gain -= 1.0;} else {fMid[15] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[16] = 1.0; gain -= 1.0;} else {fMid[16] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[17] = 1.0; gain -= 1.0;} else {fMid[17] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[18] = 1.0; gain -= 1.0;} else {fMid[18] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[19] = 1.0; gain -= 1.0;} else {fMid[19] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders, in stereo	
	
	if (overallscale < 1.0) overallscale = 1.0;
	fMid[0] /= overallscale;
	fMid[1] /= overallscale;
	fMid[2] /= overallscale;
	fMid[3] /= overallscale;
	fMid[4] /= overallscale;
	fMid[5] /= overallscale;
	fMid[6] /= overallscale;
	fMid[7] /= overallscale;
	fMid[8] /= overallscale;
	fMid[9] /= overallscale;
	fMid[10] /= overallscale;
	fMid[11] /= overallscale;
	fMid[12] /= overallscale;
	fMid[13] /= overallscale;
	fMid[14] /= overallscale;
	fMid[15] /= overallscale;
	fMid[16] /= overallscale;
	fMid[17] /= overallscale;
	fMid[18] /= overallscale;
	fMid[19] /= overallscale;
	//and now it's neatly scaled, too
	
	double accumulatorSampleL;
	double correctionL;
	double accumulatorSampleR;
	double correctionR;
	
	
	double aWet = 1.0;
	double bWet = 1.0;
	double cWet = 1.0;
	double dWet = B*4.0;
	//four-stage wet/dry control using progressive stages that bypass when not engaged
	if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
	else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
	else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
	else {dWet -= 3.0;}
	//this is one way to make a little set of dry/wet stages that are successively added to the
	//output as the control is turned up. Each one independently goes from 0-1 and stays at 1
	//beyond that point: this is a way to progressively add a 'black box' sound processing
	//which lets you fall through to simpler processing at lower settings.
	
	//now we set them up so each full intensity one is blended evenly with dry for each stage.
	//That's because the GrooveWear algorithm works best combined with dry.
	//aWet *= 0.5;
	//bWet *= 0.5; This was the tweak which caused GrooveWear to be dark instead of distorty
	//cWet *= 0.5; Disabling this causes engaged stages to take on an edge, but 0.5 settings
	//dWet *= 0.5; for any stage will still produce a darker tone.
	// This will make the behavior of the plugin more complex
	//if you are using a more typical algorithm (like a sin() or something) you won't use this part
	
	double aDry = 1.0 - aWet;
	double bDry = 1.0 - bWet;
	double cDry = 1.0 - cWet;
	double dDry = 1.0 - dWet;
	
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
		
		if (aWet > 0.0) {
			aMidL[19] = aMidL[18]; aMidL[18] = aMidL[17]; aMidL[17] = aMidL[16]; aMidL[16] = aMidL[15];
			aMidL[15] = aMidL[14]; aMidL[14] = aMidL[13]; aMidL[13] = aMidL[12]; aMidL[12] = aMidL[11];
			aMidL[11] = aMidL[10]; aMidL[10] = aMidL[9];
			aMidL[9] = aMidL[8]; aMidL[8] = aMidL[7]; aMidL[7] = aMidL[6]; aMidL[6] = aMidL[5];
			aMidL[5] = aMidL[4]; aMidL[4] = aMidL[3]; aMidL[3] = aMidL[2]; aMidL[2] = aMidL[1];
			aMidL[1] = aMidL[0]; aMidL[0] = accumulatorSampleL = (inputSampleL-aMidPrevL);
			//this is different from Aura because that is accumulating rates of change OF the rate of change
			//this is just averaging slews directly, and we have two stages of it.
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (aMidL[1] * fMid[1]);
			accumulatorSampleL += (aMidL[2] * fMid[2]);
			accumulatorSampleL += (aMidL[3] * fMid[3]);
			accumulatorSampleL += (aMidL[4] * fMid[4]);
			accumulatorSampleL += (aMidL[5] * fMid[5]);
			accumulatorSampleL += (aMidL[6] * fMid[6]);
			accumulatorSampleL += (aMidL[7] * fMid[7]);
			accumulatorSampleL += (aMidL[8] * fMid[8]);
			accumulatorSampleL += (aMidL[9] * fMid[9]);
			accumulatorSampleL += (aMidL[10] * fMid[10]);
			accumulatorSampleL += (aMidL[11] * fMid[11]);
			accumulatorSampleL += (aMidL[12] * fMid[12]);
			accumulatorSampleL += (aMidL[13] * fMid[13]);
			accumulatorSampleL += (aMidL[14] * fMid[14]);
			accumulatorSampleL += (aMidL[15] * fMid[15]);
			accumulatorSampleL += (aMidL[16] * fMid[16]);
			accumulatorSampleL += (aMidL[17] * fMid[17]);
			accumulatorSampleL += (aMidL[18] * fMid[18]);
			accumulatorSampleL += (aMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-aMidPrevL) - accumulatorSampleL;
			aMidPrevL = inputSampleL;		
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * aWet) + (drySampleL * aDry);
			drySampleL = inputSampleL;		
			
			aMidR[19] = aMidR[18]; aMidR[18] = aMidR[17]; aMidR[17] = aMidR[16]; aMidR[16] = aMidR[15];
			aMidR[15] = aMidR[14]; aMidR[14] = aMidR[13]; aMidR[13] = aMidR[12]; aMidR[12] = aMidR[11];
			aMidR[11] = aMidR[10]; aMidR[10] = aMidR[9];
			aMidR[9] = aMidR[8]; aMidR[8] = aMidR[7]; aMidR[7] = aMidR[6]; aMidR[6] = aMidR[5];
			aMidR[5] = aMidR[4]; aMidR[4] = aMidR[3]; aMidR[3] = aMidR[2]; aMidR[2] = aMidR[1];
			aMidR[1] = aMidR[0]; aMidR[0] = accumulatorSampleR = (inputSampleR-aMidPrevR);
			//this is different from Aura because that is accumulating rates of change OF the rate of change
			//this is just averaging slews directly, and we have two stages of it.
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (aMidR[1] * fMid[1]);
			accumulatorSampleR += (aMidR[2] * fMid[2]);
			accumulatorSampleR += (aMidR[3] * fMid[3]);
			accumulatorSampleR += (aMidR[4] * fMid[4]);
			accumulatorSampleR += (aMidR[5] * fMid[5]);
			accumulatorSampleR += (aMidR[6] * fMid[6]);
			accumulatorSampleR += (aMidR[7] * fMid[7]);
			accumulatorSampleR += (aMidR[8] * fMid[8]);
			accumulatorSampleR += (aMidR[9] * fMid[9]);
			accumulatorSampleR += (aMidR[10] * fMid[10]);
			accumulatorSampleR += (aMidR[11] * fMid[11]);
			accumulatorSampleR += (aMidR[12] * fMid[12]);
			accumulatorSampleR += (aMidR[13] * fMid[13]);
			accumulatorSampleR += (aMidR[14] * fMid[14]);
			accumulatorSampleR += (aMidR[15] * fMid[15]);
			accumulatorSampleR += (aMidR[16] * fMid[16]);
			accumulatorSampleR += (aMidR[17] * fMid[17]);
			accumulatorSampleR += (aMidR[18] * fMid[18]);
			accumulatorSampleR += (aMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-aMidPrevR) - accumulatorSampleR;
			aMidPrevR = inputSampleR;		
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * aWet) + (drySampleR * aDry);
			drySampleR = inputSampleR;
		}		
		
		if (bWet > 0.0) {
			bMidL[19] = bMidL[18]; bMidL[18] = bMidL[17]; bMidL[17] = bMidL[16]; bMidL[16] = bMidL[15];
			bMidL[15] = bMidL[14]; bMidL[14] = bMidL[13]; bMidL[13] = bMidL[12]; bMidL[12] = bMidL[11];
			bMidL[11] = bMidL[10]; bMidL[10] = bMidL[9];
			bMidL[9] = bMidL[8]; bMidL[8] = bMidL[7]; bMidL[7] = bMidL[6]; bMidL[6] = bMidL[5];
			bMidL[5] = bMidL[4]; bMidL[4] = bMidL[3]; bMidL[3] = bMidL[2]; bMidL[2] = bMidL[1];
			bMidL[1] = bMidL[0]; bMidL[0] = accumulatorSampleL = (inputSampleL-bMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (bMidL[1] * fMid[1]);
			accumulatorSampleL += (bMidL[2] * fMid[2]);
			accumulatorSampleL += (bMidL[3] * fMid[3]);
			accumulatorSampleL += (bMidL[4] * fMid[4]);
			accumulatorSampleL += (bMidL[5] * fMid[5]);
			accumulatorSampleL += (bMidL[6] * fMid[6]);
			accumulatorSampleL += (bMidL[7] * fMid[7]);
			accumulatorSampleL += (bMidL[8] * fMid[8]);
			accumulatorSampleL += (bMidL[9] * fMid[9]);
			accumulatorSampleL += (bMidL[10] * fMid[10]);
			accumulatorSampleL += (bMidL[11] * fMid[11]);
			accumulatorSampleL += (bMidL[12] * fMid[12]);
			accumulatorSampleL += (bMidL[13] * fMid[13]);
			accumulatorSampleL += (bMidL[14] * fMid[14]);
			accumulatorSampleL += (bMidL[15] * fMid[15]);
			accumulatorSampleL += (bMidL[16] * fMid[16]);
			accumulatorSampleL += (bMidL[17] * fMid[17]);
			accumulatorSampleL += (bMidL[18] * fMid[18]);
			accumulatorSampleL += (bMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-bMidPrevL) - accumulatorSampleL;
			bMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * bWet) + (drySampleL * bDry);
			drySampleL = inputSampleL;		
			
			bMidR[19] = bMidR[18]; bMidR[18] = bMidR[17]; bMidR[17] = bMidR[16]; bMidR[16] = bMidR[15];
			bMidR[15] = bMidR[14]; bMidR[14] = bMidR[13]; bMidR[13] = bMidR[12]; bMidR[12] = bMidR[11];
			bMidR[11] = bMidR[10]; bMidR[10] = bMidR[9];
			bMidR[9] = bMidR[8]; bMidR[8] = bMidR[7]; bMidR[7] = bMidR[6]; bMidR[6] = bMidR[5];
			bMidR[5] = bMidR[4]; bMidR[4] = bMidR[3]; bMidR[3] = bMidR[2]; bMidR[2] = bMidR[1];
			bMidR[1] = bMidR[0]; bMidR[0] = accumulatorSampleR = (inputSampleR-bMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (bMidR[1] * fMid[1]);
			accumulatorSampleR += (bMidR[2] * fMid[2]);
			accumulatorSampleR += (bMidR[3] * fMid[3]);
			accumulatorSampleR += (bMidR[4] * fMid[4]);
			accumulatorSampleR += (bMidR[5] * fMid[5]);
			accumulatorSampleR += (bMidR[6] * fMid[6]);
			accumulatorSampleR += (bMidR[7] * fMid[7]);
			accumulatorSampleR += (bMidR[8] * fMid[8]);
			accumulatorSampleR += (bMidR[9] * fMid[9]);
			accumulatorSampleR += (bMidR[10] * fMid[10]);
			accumulatorSampleR += (bMidR[11] * fMid[11]);
			accumulatorSampleR += (bMidR[12] * fMid[12]);
			accumulatorSampleR += (bMidR[13] * fMid[13]);
			accumulatorSampleR += (bMidR[14] * fMid[14]);
			accumulatorSampleR += (bMidR[15] * fMid[15]);
			accumulatorSampleR += (bMidR[16] * fMid[16]);
			accumulatorSampleR += (bMidR[17] * fMid[17]);
			accumulatorSampleR += (bMidR[18] * fMid[18]);
			accumulatorSampleR += (bMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-bMidPrevR) - accumulatorSampleR;
			bMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * bWet) + (drySampleR * bDry);
			drySampleR = inputSampleR;
		}
		
		if (cWet > 0.0) {
			cMidL[19] = cMidL[18]; cMidL[18] = cMidL[17]; cMidL[17] = cMidL[16]; cMidL[16] = cMidL[15];
			cMidL[15] = cMidL[14]; cMidL[14] = cMidL[13]; cMidL[13] = cMidL[12]; cMidL[12] = cMidL[11];
			cMidL[11] = cMidL[10]; cMidL[10] = cMidL[9];
			cMidL[9] = cMidL[8]; cMidL[8] = cMidL[7]; cMidL[7] = cMidL[6]; cMidL[6] = cMidL[5];
			cMidL[5] = cMidL[4]; cMidL[4] = cMidL[3]; cMidL[3] = cMidL[2]; cMidL[2] = cMidL[1];
			cMidL[1] = cMidL[0]; cMidL[0] = accumulatorSampleL = (inputSampleL-cMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (cMidL[1] * fMid[1]);
			accumulatorSampleL += (cMidL[2] * fMid[2]);
			accumulatorSampleL += (cMidL[3] * fMid[3]);
			accumulatorSampleL += (cMidL[4] * fMid[4]);
			accumulatorSampleL += (cMidL[5] * fMid[5]);
			accumulatorSampleL += (cMidL[6] * fMid[6]);
			accumulatorSampleL += (cMidL[7] * fMid[7]);
			accumulatorSampleL += (cMidL[8] * fMid[8]);
			accumulatorSampleL += (cMidL[9] * fMid[9]);
			accumulatorSampleL += (cMidL[10] * fMid[10]);
			accumulatorSampleL += (cMidL[11] * fMid[11]);
			accumulatorSampleL += (cMidL[12] * fMid[12]);
			accumulatorSampleL += (cMidL[13] * fMid[13]);
			accumulatorSampleL += (cMidL[14] * fMid[14]);
			accumulatorSampleL += (cMidL[15] * fMid[15]);
			accumulatorSampleL += (cMidL[16] * fMid[16]);
			accumulatorSampleL += (cMidL[17] * fMid[17]);
			accumulatorSampleL += (cMidL[18] * fMid[18]);
			accumulatorSampleL += (cMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-cMidPrevL) - accumulatorSampleL;
			cMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * cWet) + (drySampleL * cDry);
			drySampleL = inputSampleL;		
			
			cMidR[19] = cMidR[18]; cMidR[18] = cMidR[17]; cMidR[17] = cMidR[16]; cMidR[16] = cMidR[15];
			cMidR[15] = cMidR[14]; cMidR[14] = cMidR[13]; cMidR[13] = cMidR[12]; cMidR[12] = cMidR[11];
			cMidR[11] = cMidR[10]; cMidR[10] = cMidR[9];
			cMidR[9] = cMidR[8]; cMidR[8] = cMidR[7]; cMidR[7] = cMidR[6]; cMidR[6] = cMidR[5];
			cMidR[5] = cMidR[4]; cMidR[4] = cMidR[3]; cMidR[3] = cMidR[2]; cMidR[2] = cMidR[1];
			cMidR[1] = cMidR[0]; cMidR[0] = accumulatorSampleR = (inputSampleR-cMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (cMidR[1] * fMid[1]);
			accumulatorSampleR += (cMidR[2] * fMid[2]);
			accumulatorSampleR += (cMidR[3] * fMid[3]);
			accumulatorSampleR += (cMidR[4] * fMid[4]);
			accumulatorSampleR += (cMidR[5] * fMid[5]);
			accumulatorSampleR += (cMidR[6] * fMid[6]);
			accumulatorSampleR += (cMidR[7] * fMid[7]);
			accumulatorSampleR += (cMidR[8] * fMid[8]);
			accumulatorSampleR += (cMidR[9] * fMid[9]);
			accumulatorSampleR += (cMidR[10] * fMid[10]);
			accumulatorSampleR += (cMidR[11] * fMid[11]);
			accumulatorSampleR += (cMidR[12] * fMid[12]);
			accumulatorSampleR += (cMidR[13] * fMid[13]);
			accumulatorSampleR += (cMidR[14] * fMid[14]);
			accumulatorSampleR += (cMidR[15] * fMid[15]);
			accumulatorSampleR += (cMidR[16] * fMid[16]);
			accumulatorSampleR += (cMidR[17] * fMid[17]);
			accumulatorSampleR += (cMidR[18] * fMid[18]);
			accumulatorSampleR += (cMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-cMidPrevR) - accumulatorSampleR;
			cMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * cWet) + (drySampleR * cDry);
			drySampleR = inputSampleR;
		}
		
		if (dWet > 0.0) {
			dMidL[19] = dMidL[18]; dMidL[18] = dMidL[17]; dMidL[17] = dMidL[16]; dMidL[16] = dMidL[15];
			dMidL[15] = dMidL[14]; dMidL[14] = dMidL[13]; dMidL[13] = dMidL[12]; dMidL[12] = dMidL[11];
			dMidL[11] = dMidL[10]; dMidL[10] = dMidL[9];
			dMidL[9] = dMidL[8]; dMidL[8] = dMidL[7]; dMidL[7] = dMidL[6]; dMidL[6] = dMidL[5];
			dMidL[5] = dMidL[4]; dMidL[4] = dMidL[3]; dMidL[3] = dMidL[2]; dMidL[2] = dMidL[1];
			dMidL[1] = dMidL[0]; dMidL[0] = accumulatorSampleL = (inputSampleL-dMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (dMidL[1] * fMid[1]);
			accumulatorSampleL += (dMidL[2] * fMid[2]);
			accumulatorSampleL += (dMidL[3] * fMid[3]);
			accumulatorSampleL += (dMidL[4] * fMid[4]);
			accumulatorSampleL += (dMidL[5] * fMid[5]);
			accumulatorSampleL += (dMidL[6] * fMid[6]);
			accumulatorSampleL += (dMidL[7] * fMid[7]);
			accumulatorSampleL += (dMidL[8] * fMid[8]);
			accumulatorSampleL += (dMidL[9] * fMid[9]);
			accumulatorSampleL += (dMidL[10] * fMid[10]);
			accumulatorSampleL += (dMidL[11] * fMid[11]);
			accumulatorSampleL += (dMidL[12] * fMid[12]);
			accumulatorSampleL += (dMidL[13] * fMid[13]);
			accumulatorSampleL += (dMidL[14] * fMid[14]);
			accumulatorSampleL += (dMidL[15] * fMid[15]);
			accumulatorSampleL += (dMidL[16] * fMid[16]);
			accumulatorSampleL += (dMidL[17] * fMid[17]);
			accumulatorSampleL += (dMidL[18] * fMid[18]);
			accumulatorSampleL += (dMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-dMidPrevL) - accumulatorSampleL;
			dMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * dWet) + (drySampleL * dDry);
			drySampleL = inputSampleL;		
			
			dMidR[19] = dMidR[18]; dMidR[18] = dMidR[17]; dMidR[17] = dMidR[16]; dMidR[16] = dMidR[15];
			dMidR[15] = dMidR[14]; dMidR[14] = dMidR[13]; dMidR[13] = dMidR[12]; dMidR[12] = dMidR[11];
			dMidR[11] = dMidR[10]; dMidR[10] = dMidR[9];
			dMidR[9] = dMidR[8]; dMidR[8] = dMidR[7]; dMidR[7] = dMidR[6]; dMidR[6] = dMidR[5];
			dMidR[5] = dMidR[4]; dMidR[4] = dMidR[3]; dMidR[3] = dMidR[2]; dMidR[2] = dMidR[1];
			dMidR[1] = dMidR[0]; dMidR[0] = accumulatorSampleR = (inputSampleR-dMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (dMidR[1] * fMid[1]);
			accumulatorSampleR += (dMidR[2] * fMid[2]);
			accumulatorSampleR += (dMidR[3] * fMid[3]);
			accumulatorSampleR += (dMidR[4] * fMid[4]);
			accumulatorSampleR += (dMidR[5] * fMid[5]);
			accumulatorSampleR += (dMidR[6] * fMid[6]);
			accumulatorSampleR += (dMidR[7] * fMid[7]);
			accumulatorSampleR += (dMidR[8] * fMid[8]);
			accumulatorSampleR += (dMidR[9] * fMid[9]);
			accumulatorSampleR += (dMidR[10] * fMid[10]);
			accumulatorSampleR += (dMidR[11] * fMid[11]);
			accumulatorSampleR += (dMidR[12] * fMid[12]);
			accumulatorSampleR += (dMidR[13] * fMid[13]);
			accumulatorSampleR += (dMidR[14] * fMid[14]);
			accumulatorSampleR += (dMidR[15] * fMid[15]);
			accumulatorSampleR += (dMidR[16] * fMid[16]);
			accumulatorSampleR += (dMidR[17] * fMid[17]);
			accumulatorSampleR += (dMidR[18] * fMid[18]);
			accumulatorSampleR += (dMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-dMidPrevR) - accumulatorSampleR;
			dMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * dWet) + (drySampleR * dDry);
			drySampleR = inputSampleR;
		}
		
		//stereo 32 bit dither, made small and tidy.
		int expon; frexpf((float)inputSampleL, &expon);
		long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
		frexpf((float)inputSampleR, &expon);
		dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
		inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
		//end 32 bit dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;
		
		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void CrunchyGrooveWear::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	
	double overallscale = (pow(A,2)*19.0)+1.0;
	double gain = overallscale;
	//mid groove wear
	if (gain > 1.0) {fMid[0] = 1.0; gain -= 1.0;} else {fMid[0] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[1] = 1.0; gain -= 1.0;} else {fMid[1] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[2] = 1.0; gain -= 1.0;} else {fMid[2] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[3] = 1.0; gain -= 1.0;} else {fMid[3] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[4] = 1.0; gain -= 1.0;} else {fMid[4] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[5] = 1.0; gain -= 1.0;} else {fMid[5] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[6] = 1.0; gain -= 1.0;} else {fMid[6] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[7] = 1.0; gain -= 1.0;} else {fMid[7] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[8] = 1.0; gain -= 1.0;} else {fMid[8] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[9] = 1.0; gain -= 1.0;} else {fMid[9] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[10] = 1.0; gain -= 1.0;} else {fMid[10] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[11] = 1.0; gain -= 1.0;} else {fMid[11] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[12] = 1.0; gain -= 1.0;} else {fMid[12] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[13] = 1.0; gain -= 1.0;} else {fMid[13] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[14] = 1.0; gain -= 1.0;} else {fMid[14] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[15] = 1.0; gain -= 1.0;} else {fMid[15] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[16] = 1.0; gain -= 1.0;} else {fMid[16] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[17] = 1.0; gain -= 1.0;} else {fMid[17] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[18] = 1.0; gain -= 1.0;} else {fMid[18] = gain; gain = 0.0;}
	if (gain > 1.0) {fMid[19] = 1.0; gain -= 1.0;} else {fMid[19] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders, in stereo	
	
	if (overallscale < 1.0) overallscale = 1.0;
	fMid[0] /= overallscale;
	fMid[1] /= overallscale;
	fMid[2] /= overallscale;
	fMid[3] /= overallscale;
	fMid[4] /= overallscale;
	fMid[5] /= overallscale;
	fMid[6] /= overallscale;
	fMid[7] /= overallscale;
	fMid[8] /= overallscale;
	fMid[9] /= overallscale;
	fMid[10] /= overallscale;
	fMid[11] /= overallscale;
	fMid[12] /= overallscale;
	fMid[13] /= overallscale;
	fMid[14] /= overallscale;
	fMid[15] /= overallscale;
	fMid[16] /= overallscale;
	fMid[17] /= overallscale;
	fMid[18] /= overallscale;
	fMid[19] /= overallscale;
	//and now it's neatly scaled, too
	
	double accumulatorSampleL;
	double correctionL;
	double accumulatorSampleR;
	double correctionR;
	
	
	double aWet = 1.0;
	double bWet = 1.0;
	double cWet = 1.0;
	double dWet = B*4.0;
	//four-stage wet/dry control using progressive stages that bypass when not engaged
	if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
	else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
	else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
	else {dWet -= 3.0;}
	//this is one way to make a little set of dry/wet stages that are successively added to the
	//output as the control is turned up. Each one independently goes from 0-1 and stays at 1
	//beyond that point: this is a way to progressively add a 'black box' sound processing
	//which lets you fall through to simpler processing at lower settings.
	
	//now we set them up so each full intensity one is blended evenly with dry for each stage.
	//That's because the GrooveWear algorithm works best combined with dry.
	//aWet *= 0.5;
	//bWet *= 0.5; This was the tweak which caused GrooveWear to be dark instead of distorty
	//cWet *= 0.5; Disabling this causes engaged stages to take on an edge, but 0.5 settings
	//dWet *= 0.5; for any stage will still produce a darker tone.
	// This will make the behavior of the plugin more complex
	//if you are using a more typical algorithm (like a sin() or something) you won't use this part
	
	double aDry = 1.0 - aWet;
	double bDry = 1.0 - bWet;
	double cDry = 1.0 - cWet;
	double dDry = 1.0 - dWet;
	
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
		
		if (aWet > 0.0) {
			aMidL[19] = aMidL[18]; aMidL[18] = aMidL[17]; aMidL[17] = aMidL[16]; aMidL[16] = aMidL[15];
			aMidL[15] = aMidL[14]; aMidL[14] = aMidL[13]; aMidL[13] = aMidL[12]; aMidL[12] = aMidL[11];
			aMidL[11] = aMidL[10]; aMidL[10] = aMidL[9];
			aMidL[9] = aMidL[8]; aMidL[8] = aMidL[7]; aMidL[7] = aMidL[6]; aMidL[6] = aMidL[5];
			aMidL[5] = aMidL[4]; aMidL[4] = aMidL[3]; aMidL[3] = aMidL[2]; aMidL[2] = aMidL[1];
			aMidL[1] = aMidL[0]; aMidL[0] = accumulatorSampleL = (inputSampleL-aMidPrevL);
			//this is different from Aura because that is accumulating rates of change OF the rate of change
			//this is just averaging slews directly, and we have two stages of it.
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (aMidL[1] * fMid[1]);
			accumulatorSampleL += (aMidL[2] * fMid[2]);
			accumulatorSampleL += (aMidL[3] * fMid[3]);
			accumulatorSampleL += (aMidL[4] * fMid[4]);
			accumulatorSampleL += (aMidL[5] * fMid[5]);
			accumulatorSampleL += (aMidL[6] * fMid[6]);
			accumulatorSampleL += (aMidL[7] * fMid[7]);
			accumulatorSampleL += (aMidL[8] * fMid[8]);
			accumulatorSampleL += (aMidL[9] * fMid[9]);
			accumulatorSampleL += (aMidL[10] * fMid[10]);
			accumulatorSampleL += (aMidL[11] * fMid[11]);
			accumulatorSampleL += (aMidL[12] * fMid[12]);
			accumulatorSampleL += (aMidL[13] * fMid[13]);
			accumulatorSampleL += (aMidL[14] * fMid[14]);
			accumulatorSampleL += (aMidL[15] * fMid[15]);
			accumulatorSampleL += (aMidL[16] * fMid[16]);
			accumulatorSampleL += (aMidL[17] * fMid[17]);
			accumulatorSampleL += (aMidL[18] * fMid[18]);
			accumulatorSampleL += (aMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-aMidPrevL) - accumulatorSampleL;
			aMidPrevL = inputSampleL;		
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * aWet) + (drySampleL * aDry);
			drySampleL = inputSampleL;		
			
			aMidR[19] = aMidR[18]; aMidR[18] = aMidR[17]; aMidR[17] = aMidR[16]; aMidR[16] = aMidR[15];
			aMidR[15] = aMidR[14]; aMidR[14] = aMidR[13]; aMidR[13] = aMidR[12]; aMidR[12] = aMidR[11];
			aMidR[11] = aMidR[10]; aMidR[10] = aMidR[9];
			aMidR[9] = aMidR[8]; aMidR[8] = aMidR[7]; aMidR[7] = aMidR[6]; aMidR[6] = aMidR[5];
			aMidR[5] = aMidR[4]; aMidR[4] = aMidR[3]; aMidR[3] = aMidR[2]; aMidR[2] = aMidR[1];
			aMidR[1] = aMidR[0]; aMidR[0] = accumulatorSampleR = (inputSampleR-aMidPrevR);
			//this is different from Aura because that is accumulating rates of change OF the rate of change
			//this is just averaging slews directly, and we have two stages of it.
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (aMidR[1] * fMid[1]);
			accumulatorSampleR += (aMidR[2] * fMid[2]);
			accumulatorSampleR += (aMidR[3] * fMid[3]);
			accumulatorSampleR += (aMidR[4] * fMid[4]);
			accumulatorSampleR += (aMidR[5] * fMid[5]);
			accumulatorSampleR += (aMidR[6] * fMid[6]);
			accumulatorSampleR += (aMidR[7] * fMid[7]);
			accumulatorSampleR += (aMidR[8] * fMid[8]);
			accumulatorSampleR += (aMidR[9] * fMid[9]);
			accumulatorSampleR += (aMidR[10] * fMid[10]);
			accumulatorSampleR += (aMidR[11] * fMid[11]);
			accumulatorSampleR += (aMidR[12] * fMid[12]);
			accumulatorSampleR += (aMidR[13] * fMid[13]);
			accumulatorSampleR += (aMidR[14] * fMid[14]);
			accumulatorSampleR += (aMidR[15] * fMid[15]);
			accumulatorSampleR += (aMidR[16] * fMid[16]);
			accumulatorSampleR += (aMidR[17] * fMid[17]);
			accumulatorSampleR += (aMidR[18] * fMid[18]);
			accumulatorSampleR += (aMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-aMidPrevR) - accumulatorSampleR;
			aMidPrevR = inputSampleR;		
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * aWet) + (drySampleR * aDry);
			drySampleR = inputSampleR;
		}		
		
		if (bWet > 0.0) {
			bMidL[19] = bMidL[18]; bMidL[18] = bMidL[17]; bMidL[17] = bMidL[16]; bMidL[16] = bMidL[15];
			bMidL[15] = bMidL[14]; bMidL[14] = bMidL[13]; bMidL[13] = bMidL[12]; bMidL[12] = bMidL[11];
			bMidL[11] = bMidL[10]; bMidL[10] = bMidL[9];
			bMidL[9] = bMidL[8]; bMidL[8] = bMidL[7]; bMidL[7] = bMidL[6]; bMidL[6] = bMidL[5];
			bMidL[5] = bMidL[4]; bMidL[4] = bMidL[3]; bMidL[3] = bMidL[2]; bMidL[2] = bMidL[1];
			bMidL[1] = bMidL[0]; bMidL[0] = accumulatorSampleL = (inputSampleL-bMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (bMidL[1] * fMid[1]);
			accumulatorSampleL += (bMidL[2] * fMid[2]);
			accumulatorSampleL += (bMidL[3] * fMid[3]);
			accumulatorSampleL += (bMidL[4] * fMid[4]);
			accumulatorSampleL += (bMidL[5] * fMid[5]);
			accumulatorSampleL += (bMidL[6] * fMid[6]);
			accumulatorSampleL += (bMidL[7] * fMid[7]);
			accumulatorSampleL += (bMidL[8] * fMid[8]);
			accumulatorSampleL += (bMidL[9] * fMid[9]);
			accumulatorSampleL += (bMidL[10] * fMid[10]);
			accumulatorSampleL += (bMidL[11] * fMid[11]);
			accumulatorSampleL += (bMidL[12] * fMid[12]);
			accumulatorSampleL += (bMidL[13] * fMid[13]);
			accumulatorSampleL += (bMidL[14] * fMid[14]);
			accumulatorSampleL += (bMidL[15] * fMid[15]);
			accumulatorSampleL += (bMidL[16] * fMid[16]);
			accumulatorSampleL += (bMidL[17] * fMid[17]);
			accumulatorSampleL += (bMidL[18] * fMid[18]);
			accumulatorSampleL += (bMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-bMidPrevL) - accumulatorSampleL;
			bMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * bWet) + (drySampleL * bDry);
			drySampleL = inputSampleL;		
			
			bMidR[19] = bMidR[18]; bMidR[18] = bMidR[17]; bMidR[17] = bMidR[16]; bMidR[16] = bMidR[15];
			bMidR[15] = bMidR[14]; bMidR[14] = bMidR[13]; bMidR[13] = bMidR[12]; bMidR[12] = bMidR[11];
			bMidR[11] = bMidR[10]; bMidR[10] = bMidR[9];
			bMidR[9] = bMidR[8]; bMidR[8] = bMidR[7]; bMidR[7] = bMidR[6]; bMidR[6] = bMidR[5];
			bMidR[5] = bMidR[4]; bMidR[4] = bMidR[3]; bMidR[3] = bMidR[2]; bMidR[2] = bMidR[1];
			bMidR[1] = bMidR[0]; bMidR[0] = accumulatorSampleR = (inputSampleR-bMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (bMidR[1] * fMid[1]);
			accumulatorSampleR += (bMidR[2] * fMid[2]);
			accumulatorSampleR += (bMidR[3] * fMid[3]);
			accumulatorSampleR += (bMidR[4] * fMid[4]);
			accumulatorSampleR += (bMidR[5] * fMid[5]);
			accumulatorSampleR += (bMidR[6] * fMid[6]);
			accumulatorSampleR += (bMidR[7] * fMid[7]);
			accumulatorSampleR += (bMidR[8] * fMid[8]);
			accumulatorSampleR += (bMidR[9] * fMid[9]);
			accumulatorSampleR += (bMidR[10] * fMid[10]);
			accumulatorSampleR += (bMidR[11] * fMid[11]);
			accumulatorSampleR += (bMidR[12] * fMid[12]);
			accumulatorSampleR += (bMidR[13] * fMid[13]);
			accumulatorSampleR += (bMidR[14] * fMid[14]);
			accumulatorSampleR += (bMidR[15] * fMid[15]);
			accumulatorSampleR += (bMidR[16] * fMid[16]);
			accumulatorSampleR += (bMidR[17] * fMid[17]);
			accumulatorSampleR += (bMidR[18] * fMid[18]);
			accumulatorSampleR += (bMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-bMidPrevR) - accumulatorSampleR;
			bMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * bWet) + (drySampleR * bDry);
			drySampleR = inputSampleR;
		}
		
		if (cWet > 0.0) {
			cMidL[19] = cMidL[18]; cMidL[18] = cMidL[17]; cMidL[17] = cMidL[16]; cMidL[16] = cMidL[15];
			cMidL[15] = cMidL[14]; cMidL[14] = cMidL[13]; cMidL[13] = cMidL[12]; cMidL[12] = cMidL[11];
			cMidL[11] = cMidL[10]; cMidL[10] = cMidL[9];
			cMidL[9] = cMidL[8]; cMidL[8] = cMidL[7]; cMidL[7] = cMidL[6]; cMidL[6] = cMidL[5];
			cMidL[5] = cMidL[4]; cMidL[4] = cMidL[3]; cMidL[3] = cMidL[2]; cMidL[2] = cMidL[1];
			cMidL[1] = cMidL[0]; cMidL[0] = accumulatorSampleL = (inputSampleL-cMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (cMidL[1] * fMid[1]);
			accumulatorSampleL += (cMidL[2] * fMid[2]);
			accumulatorSampleL += (cMidL[3] * fMid[3]);
			accumulatorSampleL += (cMidL[4] * fMid[4]);
			accumulatorSampleL += (cMidL[5] * fMid[5]);
			accumulatorSampleL += (cMidL[6] * fMid[6]);
			accumulatorSampleL += (cMidL[7] * fMid[7]);
			accumulatorSampleL += (cMidL[8] * fMid[8]);
			accumulatorSampleL += (cMidL[9] * fMid[9]);
			accumulatorSampleL += (cMidL[10] * fMid[10]);
			accumulatorSampleL += (cMidL[11] * fMid[11]);
			accumulatorSampleL += (cMidL[12] * fMid[12]);
			accumulatorSampleL += (cMidL[13] * fMid[13]);
			accumulatorSampleL += (cMidL[14] * fMid[14]);
			accumulatorSampleL += (cMidL[15] * fMid[15]);
			accumulatorSampleL += (cMidL[16] * fMid[16]);
			accumulatorSampleL += (cMidL[17] * fMid[17]);
			accumulatorSampleL += (cMidL[18] * fMid[18]);
			accumulatorSampleL += (cMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-cMidPrevL) - accumulatorSampleL;
			cMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * cWet) + (drySampleL * cDry);
			drySampleL = inputSampleL;		
			
			cMidR[19] = cMidR[18]; cMidR[18] = cMidR[17]; cMidR[17] = cMidR[16]; cMidR[16] = cMidR[15];
			cMidR[15] = cMidR[14]; cMidR[14] = cMidR[13]; cMidR[13] = cMidR[12]; cMidR[12] = cMidR[11];
			cMidR[11] = cMidR[10]; cMidR[10] = cMidR[9];
			cMidR[9] = cMidR[8]; cMidR[8] = cMidR[7]; cMidR[7] = cMidR[6]; cMidR[6] = cMidR[5];
			cMidR[5] = cMidR[4]; cMidR[4] = cMidR[3]; cMidR[3] = cMidR[2]; cMidR[2] = cMidR[1];
			cMidR[1] = cMidR[0]; cMidR[0] = accumulatorSampleR = (inputSampleR-cMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (cMidR[1] * fMid[1]);
			accumulatorSampleR += (cMidR[2] * fMid[2]);
			accumulatorSampleR += (cMidR[3] * fMid[3]);
			accumulatorSampleR += (cMidR[4] * fMid[4]);
			accumulatorSampleR += (cMidR[5] * fMid[5]);
			accumulatorSampleR += (cMidR[6] * fMid[6]);
			accumulatorSampleR += (cMidR[7] * fMid[7]);
			accumulatorSampleR += (cMidR[8] * fMid[8]);
			accumulatorSampleR += (cMidR[9] * fMid[9]);
			accumulatorSampleR += (cMidR[10] * fMid[10]);
			accumulatorSampleR += (cMidR[11] * fMid[11]);
			accumulatorSampleR += (cMidR[12] * fMid[12]);
			accumulatorSampleR += (cMidR[13] * fMid[13]);
			accumulatorSampleR += (cMidR[14] * fMid[14]);
			accumulatorSampleR += (cMidR[15] * fMid[15]);
			accumulatorSampleR += (cMidR[16] * fMid[16]);
			accumulatorSampleR += (cMidR[17] * fMid[17]);
			accumulatorSampleR += (cMidR[18] * fMid[18]);
			accumulatorSampleR += (cMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-cMidPrevR) - accumulatorSampleR;
			cMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * cWet) + (drySampleR * cDry);
			drySampleR = inputSampleR;
		}
		
		if (dWet > 0.0) {
			dMidL[19] = dMidL[18]; dMidL[18] = dMidL[17]; dMidL[17] = dMidL[16]; dMidL[16] = dMidL[15];
			dMidL[15] = dMidL[14]; dMidL[14] = dMidL[13]; dMidL[13] = dMidL[12]; dMidL[12] = dMidL[11];
			dMidL[11] = dMidL[10]; dMidL[10] = dMidL[9];
			dMidL[9] = dMidL[8]; dMidL[8] = dMidL[7]; dMidL[7] = dMidL[6]; dMidL[6] = dMidL[5];
			dMidL[5] = dMidL[4]; dMidL[4] = dMidL[3]; dMidL[3] = dMidL[2]; dMidL[2] = dMidL[1];
			dMidL[1] = dMidL[0]; dMidL[0] = accumulatorSampleL = (inputSampleL-dMidPrevL);
			
			accumulatorSampleL *= fMid[0];
			accumulatorSampleL += (dMidL[1] * fMid[1]);
			accumulatorSampleL += (dMidL[2] * fMid[2]);
			accumulatorSampleL += (dMidL[3] * fMid[3]);
			accumulatorSampleL += (dMidL[4] * fMid[4]);
			accumulatorSampleL += (dMidL[5] * fMid[5]);
			accumulatorSampleL += (dMidL[6] * fMid[6]);
			accumulatorSampleL += (dMidL[7] * fMid[7]);
			accumulatorSampleL += (dMidL[8] * fMid[8]);
			accumulatorSampleL += (dMidL[9] * fMid[9]);
			accumulatorSampleL += (dMidL[10] * fMid[10]);
			accumulatorSampleL += (dMidL[11] * fMid[11]);
			accumulatorSampleL += (dMidL[12] * fMid[12]);
			accumulatorSampleL += (dMidL[13] * fMid[13]);
			accumulatorSampleL += (dMidL[14] * fMid[14]);
			accumulatorSampleL += (dMidL[15] * fMid[15]);
			accumulatorSampleL += (dMidL[16] * fMid[16]);
			accumulatorSampleL += (dMidL[17] * fMid[17]);
			accumulatorSampleL += (dMidL[18] * fMid[18]);
			accumulatorSampleL += (dMidL[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionL = (inputSampleL-dMidPrevL) - accumulatorSampleL;
			dMidPrevL = inputSampleL;
			inputSampleL -= correctionL;
			inputSampleL = (inputSampleL * dWet) + (drySampleL * dDry);
			drySampleL = inputSampleL;		
			
			dMidR[19] = dMidR[18]; dMidR[18] = dMidR[17]; dMidR[17] = dMidR[16]; dMidR[16] = dMidR[15];
			dMidR[15] = dMidR[14]; dMidR[14] = dMidR[13]; dMidR[13] = dMidR[12]; dMidR[12] = dMidR[11];
			dMidR[11] = dMidR[10]; dMidR[10] = dMidR[9];
			dMidR[9] = dMidR[8]; dMidR[8] = dMidR[7]; dMidR[7] = dMidR[6]; dMidR[6] = dMidR[5];
			dMidR[5] = dMidR[4]; dMidR[4] = dMidR[3]; dMidR[3] = dMidR[2]; dMidR[2] = dMidR[1];
			dMidR[1] = dMidR[0]; dMidR[0] = accumulatorSampleR = (inputSampleR-dMidPrevR);
			
			accumulatorSampleR *= fMid[0];
			accumulatorSampleR += (dMidR[1] * fMid[1]);
			accumulatorSampleR += (dMidR[2] * fMid[2]);
			accumulatorSampleR += (dMidR[3] * fMid[3]);
			accumulatorSampleR += (dMidR[4] * fMid[4]);
			accumulatorSampleR += (dMidR[5] * fMid[5]);
			accumulatorSampleR += (dMidR[6] * fMid[6]);
			accumulatorSampleR += (dMidR[7] * fMid[7]);
			accumulatorSampleR += (dMidR[8] * fMid[8]);
			accumulatorSampleR += (dMidR[9] * fMid[9]);
			accumulatorSampleR += (dMidR[10] * fMid[10]);
			accumulatorSampleR += (dMidR[11] * fMid[11]);
			accumulatorSampleR += (dMidR[12] * fMid[12]);
			accumulatorSampleR += (dMidR[13] * fMid[13]);
			accumulatorSampleR += (dMidR[14] * fMid[14]);
			accumulatorSampleR += (dMidR[15] * fMid[15]);
			accumulatorSampleR += (dMidR[16] * fMid[16]);
			accumulatorSampleR += (dMidR[17] * fMid[17]);
			accumulatorSampleR += (dMidR[18] * fMid[18]);
			accumulatorSampleR += (dMidR[19] * fMid[19]);
			//we are doing our repetitive calculations on a separate value
			correctionR = (inputSampleR-dMidPrevR) - accumulatorSampleR;
			dMidPrevR = inputSampleR;
			inputSampleR -= correctionR;
			inputSampleR = (inputSampleR * dWet) + (drySampleR * dDry);
			drySampleR = inputSampleR;
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
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;
		
		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace CrunchyGrooveWear

