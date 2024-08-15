/* ========================================
 *  Logical4 - Logical4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Logical4_H
#include "Logical4.h"
#endif

namespace Logical4 {


void Logical4::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	float drySampleL;
	float drySampleR;
	double inputSampleL;
	double inputSampleR;
	
	//begin ButterComp
	double inputpos;
	double inputneg;
	double calcpos;
	double calcneg;
	double outputpos;
	double outputneg;
	double totalmultiplier;
	double inputgain = pow(10.0,(-((A*40.0)-20.0))/20.0);
	//fussing with the controls to make it hit the right threshold values
	double compoutgain = inputgain; //let's try compensating for this
	
	double attackspeed = ((C*C)*99.0)+1.0;
	//is in ms
	attackspeed = 10.0 / sqrt(attackspeed);
	//convert to a remainder for use in comp
	double divisor = 0.000782*attackspeed;
	//First Speed control
	divisor /= overallscale;
	double remainder = divisor;
	divisor = 1.0 - divisor;
	
	double divisorB = 0.000819*attackspeed;
	//Second Speed control
	divisorB /= overallscale;
	double remainderB = divisorB;
	divisorB = 1.0 - divisorB;
	
	double divisorC = 0.000857;
	//Third Speed control
	divisorC /= overallscale;
	double remainderC = divisorC*attackspeed;
	divisorC = 1.0 - divisorC;
	//end ButterComp
	
	double dynamicDivisor;
	double dynamicRemainder;
	//used for variable release
	
	//begin Desk Power Sag
	double intensity = 0.0445556;
	double depthA = 2.42;
	int offsetA = (int)(depthA * overallscale);
	if (offsetA < 1) offsetA = 1;
	if (offsetA > 498) offsetA = 498;
	
	double depthB = 2.42; //was 3.38
	int offsetB = (int)(depthB * overallscale);
	if (offsetB < 1) offsetB = 1;
	if (offsetB > 498) offsetB = 498;
	
	double depthC = 2.42; //was 4.35
	int offsetC = (int)(depthC * overallscale);
	if (offsetC < 1) offsetC = 1;
	if (offsetC > 498) offsetC = 498;
	
	double clamp;
	double thickness;
	double out;
	double bridgerectifier;
	double powerSag = 0.003300223685324102874217; //was .00365 
	//the Power Sag constant is how much the sag is cut back in high compressions. Increasing it
	//increases the guts and gnarl of the music, decreasing it or making it negative causes the texture to go 
	//'ethereal' and unsolid under compression. Very subtle!
	//end Desk Power Sag	
	
	double ratio = sqrt(((B*B)*15.0)+1.0)-1.0;
	if (ratio > 2.99999) ratio = 2.99999;
	if (ratio < 0.0) ratio = 0.0;
	//anything we do must adapt to our dry/a/b/c output stages
	int ratioselector = floor( ratio );
	//zero to three, it'll become, always with 3 as the max
	ratio -= ratioselector;
	double invRatio = 1.0 - ratio;
	//for all processing we've settled on the 'stage' and are just interpolating between top and bottom
	//ratio is the more extreme case, invratio becomes our 'floor' case including drySample
	
	double outSampleAL = 0.0;
	double outSampleBL = 0.0;
	double outSampleCL = 0.0;
	double outSampleAR = 0.0;
	double outSampleBR = 0.0;
	double outSampleCR = 0.0;
	//what we interpolate between using ratio
	
	double outputgain = pow(10.0,((D*40.0)-20.0)/20.0);
	double wet = E;
	double dry = 1.0 - wet;
	
    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		gcount--;
		if (gcount < 0 || gcount > 499) {gcount = 499;}
		
		//begin first Power SagL
		dL[gcount+499] = dL[gcount] = fabs(inputSampleL)*(intensity-((controlAposL+controlBposL+controlAnegL+controlBnegL)*powerSag));
		controlL += (dL[gcount] / offsetA);
		controlL -= (dL[gcount+offsetA] / offsetA);
		controlL -= 0.000001;
		clamp = 1;
		if (controlL < 0) {controlL = 0;}
		if (controlL > 1) {clamp -= (controlL - 1); controlL = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
		thickness = ((1.0 - controlL) * 2.0) - 1.0;
		out = fabs(thickness);		
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
		//blend according to density control
		if (clamp != 1.0) inputSampleL *= clamp;
		//end first Power SagL
		
		//begin first Power SagR
		dR[gcount+499] = dR[gcount] = fabs(inputSampleR)*(intensity-((controlAposR+controlBposR+controlAnegR+controlBnegR)*powerSag));
		controlR += (dR[gcount] / offsetA);
		controlR -= (dR[gcount+offsetA] / offsetA);
		controlR -= 0.000001;
		clamp = 1;
		if (controlR < 0) {controlR = 0;}
		if (controlR > 1) {clamp -= (controlR - 1); controlR = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
		thickness = ((1.0 - controlR) * 2.0) - 1.0;
		out = fabs(thickness);		
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
		//blend according to density control
		if (clamp != 1.0) inputSampleR *= clamp;
		//end first Power SagR
		
		//begin first compressorL
		if (inputgain != 1.0) inputSampleL *= inputgain;
		inputpos = (inputSampleL * fpOld) + (avgAL * fpNew) + 1.0;
		avgAL = inputSampleL;
		//hovers around 1 when there's no signal
		
		if (inputpos < 0.001) inputpos = 0.001;
		outputpos = inputpos / 2.0;
		if (outputpos > 1.0) outputpos = 1.0;		
		inputpos *= inputpos;
		//will be very high for hot input, can be 0.00001-1 for other-polarity
		
		dynamicRemainder = remainder * (inputpos + 1.0);
		if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
		dynamicDivisor = 1.0 - dynamicRemainder;
		//calc chases much faster if input swing is high
		
		targetposL *= dynamicDivisor;
		targetposL += (inputpos * dynamicRemainder);
		calcpos = pow((1.0/targetposL),2);
		//extra tiny, quick, if the inputpos of this polarity is high
		
		inputneg = (-inputSampleL * fpOld) + (nvgAL * fpNew) + 1.0;
		nvgAL = -inputSampleL;
		
		if (inputneg < 0.001) inputneg = 0.001;
		outputneg = inputneg / 2.0;
		if (outputneg > 1.0) outputneg = 1.0;		
		inputneg *= inputneg;
		//will be very high for hot input, can be 0.00001-1 for other-polarity
		
		dynamicRemainder = remainder * (inputneg + 1.0);
		if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
		dynamicDivisor = 1.0 - dynamicRemainder;
		//calc chases much faster if input swing is high
		
		targetnegL *= dynamicDivisor;
		targetnegL += (inputneg * dynamicRemainder);
		calcneg = pow((1.0/targetnegL),2);
		//now we have mirrored targets for comp
		//each calc is a blowed up chased target from tiny (at high levels of that polarity) to 1 at no input
		//calc is the one we want to react differently: go tiny quick, 
		//outputpos and outputneg go from 0 to 1
		
		if (inputSampleL > 0)
		{ //working on pos
			if (true == fpFlip)
			{
				controlAposL *= divisor;
				controlAposL += (calcpos*remainder);
				if (controlAposR > controlAposL) controlAposR = (controlAposR + controlAposL) * 0.5;
				//this part makes the compressor linked: both channels will converge towards whichever
				//is the most compressed at the time.
			}
			else
			{
				controlBposL *= divisor;
				controlBposL += (calcpos*remainder);
				if (controlBposR > controlBposL) controlBposR = (controlBposR + controlBposL) * 0.5;
			}	
		}
		else
		{ //working on neg
			if (true == fpFlip)
			{
				controlAnegL *= divisor;
				controlAnegL += (calcneg*remainder);
				if (controlAnegR > controlAnegL) controlAnegR = (controlAnegR + controlAnegL) * 0.5;
			}
			else
			{
				controlBnegL *= divisor;
				controlBnegL += (calcneg*remainder);
				if (controlBnegR > controlBnegL) controlBnegR = (controlBnegR + controlBnegL) * 0.5;
			}
		}
		//this causes each of the four to update only when active and in the correct 'fpFlip'
		
		if (true == fpFlip)
		{totalmultiplier = (controlAposL * outputpos) + (controlAnegL * outputneg);}
		else
		{totalmultiplier = (controlBposL * outputpos) + (controlBnegL * outputneg);}
		//this combines the sides according to fpFlip, blending relative to the input value
		
		if (totalmultiplier != 1.0) inputSampleL *= totalmultiplier;
		//if (compoutgain != 1.0) inputSample /= compoutgain;
		if (inputSampleL > 36.0) inputSampleL = 36.0;
		if (inputSampleL < -36.0) inputSampleL = -36.0;
		//build in +18db hard clip on insano inputs
		outSampleAL = inputSampleL / compoutgain;
		//end first compressorL
		
		//begin first compressorR
		if (inputgain != 1.0) inputSampleR *= inputgain;
		inputpos = (inputSampleR * fpOld) + (avgAR * fpNew) + 1.0;
		avgAR = inputSampleR;
		//hovers around 1 when there's no signal
		
		if (inputpos < 0.001) inputpos = 0.001;
		outputpos = inputpos / 2.0;
		if (outputpos > 1.0) outputpos = 1.0;		
		inputpos *= inputpos;
		//will be very high for hot input, can be 0.00001-1 for other-polarity
		
		dynamicRemainder = remainder * (inputpos + 1.0);
		if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
		dynamicDivisor = 1.0 - dynamicRemainder;
		//calc chases much faster if input swing is high
		
		targetposR *= dynamicDivisor;
		targetposR += (inputpos * dynamicRemainder);
		calcpos = pow((1.0/targetposR),2);
		//extra tiny, quick, if the inputpos of this polarity is high
		
		inputneg = (-inputSampleR * fpOld) + (nvgAR * fpNew) + 1.0;
		nvgAR = -inputSampleR;
		
		if (inputneg < 0.001) inputneg = 0.001;
		outputneg = inputneg / 2.0;
		if (outputneg > 1.0) outputneg = 1.0;		
		inputneg *= inputneg;
		//will be very high for hot input, can be 0.00001-1 for other-polarity
		
		dynamicRemainder = remainder * (inputneg + 1.0);
		if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
		dynamicDivisor = 1.0 - dynamicRemainder;
		//calc chases much faster if input swing is high
		
		targetnegR *= dynamicDivisor;
		targetnegR += (inputneg * dynamicRemainder);
		calcneg = pow((1.0/targetnegR),2);
		//now we have mirrored targets for comp
		//each calc is a blowed up chased target from tiny (at high levels of that polarity) to 1 at no input
		//calc is the one we want to react differently: go tiny quick, 
		//outputpos and outputneg go from 0 to 1
		
		if (inputSampleR > 0)
		{ //working on pos
			if (true == fpFlip)
			{
				controlAposR *= divisor;
				controlAposR += (calcpos*remainder);
				if (controlAposL > controlAposR) controlAposL = (controlAposR + controlAposL) * 0.5;
				//this part makes the compressor linked: both channels will converge towards whichever
				//is the most compressed at the time.				
			}
			else
			{
				controlBposR *= divisor;
				controlBposR += (calcpos*remainder);
				if (controlBposL > controlBposR) controlBposL = (controlBposR + controlBposL) * 0.5;
			}	
		}
		else
		{ //working on neg
			if (true == fpFlip)
			{
				controlAnegR *= divisor;
				controlAnegR += (calcneg*remainder);
				if (controlAnegL > controlAnegR) controlAnegL = (controlAnegR + controlAnegL) * 0.5;
			}
			else
			{
				controlBnegR *= divisor;
				controlBnegR += (calcneg*remainder);
				if (controlBnegL > controlBnegR) controlBnegL = (controlBnegR + controlBnegL) * 0.5;
			}
		}
		//this causes each of the four to update only when active and in the correct 'fpFlip'
		
		if (true == fpFlip)
		{totalmultiplier = (controlAposR * outputpos) + (controlAnegR * outputneg);}
		else
		{totalmultiplier = (controlBposR * outputpos) + (controlBnegR * outputneg);}
		//this combines the sides according to fpFlip, blending relative to the input value
		
		if (totalmultiplier != 1.0) inputSampleR *= totalmultiplier;
		//if (compoutgain != 1.0) inputSample /= compoutgain;
		if (inputSampleR > 36.0) inputSampleR = 36.0;
		if (inputSampleR < -36.0) inputSampleR = -36.0;
		//build in +18db hard clip on insano inputs
		outSampleAR = inputSampleR / compoutgain;
		//end first compressorR
		
		if (ratioselector > 0) {
			
			//begin second Power SagL
			bL[gcount+499] = bL[gcount] = fabs(inputSampleL)*(intensity-((controlAposBL+controlBposBL+controlAnegBL+controlBnegBL)*powerSag));
			controlBL += (bL[gcount] / offsetB);
			controlBL -= (bL[gcount+offsetB] / offsetB);
			controlBL -= 0.000001;
			clamp = 1;
			if (controlBL < 0) {controlBL = 0;}
			if (controlBL > 1) {clamp -= (controlBL - 1); controlBL = 1;}
			if (clamp < 0.5) {clamp = 0.5;}
			//control = 0 to 1
			thickness = ((1.0 - controlBL) * 2.0) - 1.0;
			out = fabs(thickness);		
			bridgerectifier = fabs(inputSampleL);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (thickness > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
			else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
			//blend according to density control
			if (clamp != 1.0) inputSampleL *= clamp;
			//end second Power SagL
			
			//begin second Power SagR
			bR[gcount+499] = bR[gcount] = fabs(inputSampleR)*(intensity-((controlAposBR+controlBposBR+controlAnegBR+controlBnegBR)*powerSag));
			controlBR += (bR[gcount] / offsetB);
			controlBR -= (bR[gcount+offsetB] / offsetB);
			controlBR -= 0.000001;
			clamp = 1;
			if (controlBR < 0) {controlBR = 0;}
			if (controlBR > 1) {clamp -= (controlBR - 1); controlBR = 1;}
			if (clamp < 0.5) {clamp = 0.5;}
			//control = 0 to 1
			thickness = ((1.0 - controlBR) * 2.0) - 1.0;
			out = fabs(thickness);		
			bridgerectifier = fabs(inputSampleR);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (thickness > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
			else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
			//blend according to density control
			if (clamp != 1.0) inputSampleR *= clamp;
			//end second Power SagR
			
			
			//begin second compressorL
			inputpos = (inputSampleL * fpOld) + (avgBL * fpNew) + 1.0;
			avgBL = inputSampleL;
			
			if (inputpos < 0.001) inputpos = 0.001;
			outputpos = inputpos / 2.0;
			if (outputpos > 1.0) outputpos = 1.0;		
			inputpos *= inputpos;
			//will be very high for hot input, can be 0.00001-1 for other-polarity
			
			dynamicRemainder = remainderB * (inputpos + 1.0);
			if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
			dynamicDivisor = 1.0 - dynamicRemainder;
			//calc chases much faster if input swing is high
			
			targetposBL *= dynamicDivisor;
			targetposBL += (inputpos * dynamicRemainder);
			calcpos = pow((1.0/targetposBL),2);
			
			inputneg = (-inputSampleL * fpOld) + (nvgBL * fpNew) + 1.0;
			nvgBL = -inputSampleL;
			
			if (inputneg < 0.001) inputneg = 0.001;
			outputneg = inputneg / 2.0;
			if (outputneg > 1.0) outputneg = 1.0;		
			inputneg *= inputneg;
			//will be very high for hot input, can be 0.00001-1 for other-polarity
			
			dynamicRemainder = remainderB * (inputneg + 1.0);
			if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
			dynamicDivisor = 1.0 - dynamicRemainder;
			//calc chases much faster if input swing is high
			
			targetnegBL *= dynamicDivisor;
			targetnegBL += (inputneg * dynamicRemainder);
			calcneg = pow((1.0/targetnegBL),2);
			//now we have mirrored targets for comp
			//outputpos and outputneg go from 0 to 1
			if (inputSampleL > 0)
			{ //working on pos
				if (true == fpFlip)
				{
					controlAposBL *= divisorB;
					controlAposBL += (calcpos*remainderB);
					if (controlAposBR > controlAposBL) controlAposBR = (controlAposBR + controlAposBL) * 0.5;
					//this part makes the compressor linked: both channels will converge towards whichever
					//is the most compressed at the time.				
				}
				else
				{
					controlBposBL *= divisorB;
					controlBposBL += (calcpos*remainderB);
					if (controlBposBR > controlBposBL) controlBposBR = (controlBposBR + controlBposBL) * 0.5;
				}	
			}
			else
			{ //working on neg
				if (true == fpFlip)
				{
					controlAnegBL *= divisorB;
					controlAnegBL += (calcneg*remainderB);
					if (controlAnegBR > controlAnegBL) controlAnegBR = (controlAnegBR + controlAnegBL) * 0.5;
				}
				else
				{
					controlBnegBL *= divisorB;
					controlBnegBL += (calcneg*remainderB);
					if (controlBnegBR > controlBnegBL) controlBnegBR = (controlBnegBR + controlBnegBL) * 0.5;
				}
			}
			//this causes each of the four to update only when active and in the correct 'fpFlip'
			
			if (true == fpFlip)
			{totalmultiplier = (controlAposBL * outputpos) + (controlAnegBL * outputneg);}
			else
			{totalmultiplier = (controlBposBL * outputpos) + (controlBnegBL * outputneg);}
			//this combines the sides according to fpFlip, blending relative to the input value
			
			if (totalmultiplier != 1.0) inputSampleL *= totalmultiplier;
			//if (compoutgain != 1.0) inputSample /= compoutgain;
			if (inputSampleL > 36.0) inputSampleL = 36.0;
			if (inputSampleL < -36.0) inputSampleL = -36.0;
			//build in +18db hard clip on insano inputs
			outSampleBL = inputSampleL / compoutgain;
			//end second compressorL
			
			//begin second compressorR
			inputpos = (inputSampleR * fpOld) + (avgBR * fpNew) + 1.0;
			avgBR = inputSampleR;
			
			if (inputpos < 0.001) inputpos = 0.001;
			outputpos = inputpos / 2.0;
			if (outputpos > 1.0) outputpos = 1.0;		
			inputpos *= inputpos;
			//will be very high for hot input, can be 0.00001-1 for other-polarity
			
			dynamicRemainder = remainderB * (inputpos + 1.0);
			if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
			dynamicDivisor = 1.0 - dynamicRemainder;
			//calc chases much faster if input swing is high
			
			targetposBR *= dynamicDivisor;
			targetposBR += (inputpos * dynamicRemainder);
			calcpos = pow((1.0/targetposBR),2);
			
			inputneg = (-inputSampleR * fpOld) + (nvgBR * fpNew) + 1.0;
			nvgBR = -inputSampleR;
			
			if (inputneg < 0.001) inputneg = 0.001;
			outputneg = inputneg / 2.0;
			if (outputneg > 1.0) outputneg = 1.0;		
			inputneg *= inputneg;
			//will be very high for hot input, can be 0.00001-1 for other-polarity
			
			dynamicRemainder = remainderB * (inputneg + 1.0);
			if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
			dynamicDivisor = 1.0 - dynamicRemainder;
			//calc chases much faster if input swing is high
			
			targetnegBR *= dynamicDivisor;
			targetnegBR += (inputneg * dynamicRemainder);
			calcneg = pow((1.0/targetnegBR),2);
			//now we have mirrored targets for comp
			//outputpos and outputneg go from 0 to 1
			if (inputSampleR > 0)
			{ //working on pos
				if (true == fpFlip)
				{
					controlAposBR *= divisorB;
					controlAposBR += (calcpos*remainderB);
					if (controlAposBL > controlAposBR) controlAposBL = (controlAposBR + controlAposBL) * 0.5;
					//this part makes the compressor linked: both channels will converge towards whichever
					//is the most compressed at the time.				
				}
				else
				{
					controlBposBR *= divisorB;
					controlBposBR += (calcpos*remainderB);
					if (controlBposBL > controlBposBR) controlBposBL = (controlBposBR + controlBposBL) * 0.5;
				}	
			}
			else
			{ //working on neg
				if (true == fpFlip)
				{
					controlAnegBR *= divisorB;
					controlAnegBR += (calcneg*remainderB);
					if (controlAnegBL > controlAnegBR) controlAnegBL = (controlAnegBR + controlAnegBL) * 0.5;
				}
				else
				{
					controlBnegBR *= divisorB;
					controlBnegBR += (calcneg*remainderB);
					if (controlBnegBL > controlBnegBR) controlBnegBL = (controlBnegBR + controlBnegBL) * 0.5;
				}
			}
			//this causes each of the four to update only when active and in the correct 'fpFlip'
			
			if (true == fpFlip)
			{totalmultiplier = (controlAposBR * outputpos) + (controlAnegBR * outputneg);}
			else
			{totalmultiplier = (controlBposBR * outputpos) + (controlBnegBR * outputneg);}
			//this combines the sides according to fpFlip, blending relative to the input value
			
			if (totalmultiplier != 1.0) inputSampleR *= totalmultiplier;
			//if (compoutgain != 1.0) inputSample /= compoutgain;
			if (inputSampleR > 36.0) inputSampleR = 36.0;
			if (inputSampleR < -36.0) inputSampleR = -36.0;
			//build in +18db hard clip on insano inputs
			outSampleBR = inputSampleR / compoutgain;
			//end second compressorR
			
			if (ratioselector > 1) {
				
				//begin third Power SagL
				cL[gcount+499] = cL[gcount] = fabs(inputSampleL)*(intensity-((controlAposCL+controlBposCL+controlAnegCL+controlBnegCL)*powerSag));
				controlCL += (cL[gcount] / offsetC);
				controlCL -= (cL[gcount+offsetB] / offsetC);
				controlCL -= 0.000001;
				clamp = 1;
				if (controlCL < 0) {controlCL = 0;}
				if (controlCL > 1) {clamp -= (controlCL - 1); controlCL = 1;}
				if (clamp < 0.5) {clamp = 0.5;}
				//control = 0 to 1
				thickness = ((1.0 - controlCL) * 2.0) - 1.0;
				out = fabs(thickness);		
				bridgerectifier = fabs(inputSampleL);
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				//max value for sine function
				if (thickness > 0) bridgerectifier = sin(bridgerectifier);
				else bridgerectifier = 1-cos(bridgerectifier);
				//produce either boosted or starved version
				if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
				else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
				//blend according to density control
				if (clamp != 1.0) inputSampleL *= clamp;
				//end third Power SagL
				
				//begin third Power SagR
				cR[gcount+499] = cR[gcount] = fabs(inputSampleR)*(intensity-((controlAposCR+controlBposCR+controlAnegCR+controlBnegCR)*powerSag));
				controlCR += (cR[gcount] / offsetC);
				controlCR -= (cR[gcount+offsetB] / offsetC);
				controlCR -= 0.000001;
				clamp = 1;
				if (controlCR < 0) {controlCR = 0;}
				if (controlCR > 1) {clamp -= (controlCR - 1); controlCR = 1;}
				if (clamp < 0.5) {clamp = 0.5;}
				//control = 0 to 1
				thickness = ((1.0 - controlCR) * 2.0) - 1.0;
				out = fabs(thickness);		
				bridgerectifier = fabs(inputSampleR);
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				//max value for sine function
				if (thickness > 0) bridgerectifier = sin(bridgerectifier);
				else bridgerectifier = 1-cos(bridgerectifier);
				//produce either boosted or starved version
				if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
				else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
				//blend according to density control
				if (clamp != 1.0) inputSampleR *= clamp;
				//end third Power SagR
				
				//begin third compressorL
				inputpos = (inputSampleL * fpOld) + (avgCL * fpNew) + 1.0;
				avgCL = inputSampleL;
				
				if (inputpos < 0.001) inputpos = 0.001;
				outputpos = inputpos / 2.0;
				if (outputpos > 1.0) outputpos = 1.0;		
				inputpos *= inputpos;
				//will be very high for hot input, can be 0.00001-1 for other-polarity
				
				dynamicRemainder = remainderC * (inputpos + 1.0);
				if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
				dynamicDivisor = 1.0 - dynamicRemainder;
				//calc chases much faster if input swing is high
				
				targetposCL *= dynamicDivisor;
				targetposCL += (inputpos * dynamicRemainder);
				calcpos = pow((1.0/targetposCL),2);
				
				inputneg = (-inputSampleL * fpOld) + (nvgCL * fpNew) + 1.0;
				nvgCL = -inputSampleL;
				
				if (inputneg < 0.001) inputneg = 0.001;
				outputneg = inputneg / 2.0;
				if (outputneg > 1.0) outputneg = 1.0;		
				inputneg *= inputneg;
				//will be very high for hot input, can be 0.00001-1 for other-polarity
				
				dynamicRemainder = remainderC * (inputneg + 1.0);
				if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
				dynamicDivisor = 1.0 - dynamicRemainder;
				//calc chases much faster if input swing is high
				
				targetnegCL *= dynamicDivisor;
				targetnegCL += (inputneg * dynamicRemainder);
				calcneg = pow((1.0/targetnegCL),2);
				//now we have mirrored targets for comp
				//outputpos and outputneg go from 0 to 1
				if (inputSampleL > 0)
				{ //working on pos
					if (true == fpFlip)
					{
						controlAposCL *= divisorC;
						controlAposCL += (calcpos*remainderC);
						if (controlAposCR > controlAposCL) controlAposCR = (controlAposCR + controlAposCL) * 0.5;
						//this part makes the compressor linked: both channels will converge towards whichever
						//is the most compressed at the time.				
					}
					else
					{
						controlBposCL *= divisorC;
						controlBposCL += (calcpos*remainderC);
						if (controlBposCR > controlBposCL) controlBposCR = (controlBposCR + controlBposCL) * 0.5;
					}	
				}
				else
				{ //working on neg
					if (true == fpFlip)
					{
						controlAnegCL *= divisorC;
						controlAnegCL += (calcneg*remainderC);
						if (controlAnegCR > controlAnegCL) controlAnegCR = (controlAnegCR + controlAnegCL) * 0.5;
					}
					else
					{
						controlBnegCL *= divisorC;
						controlBnegCL += (calcneg*remainderC);
						if (controlBnegCR > controlBnegCL) controlBnegCR = (controlBnegCR + controlBnegCL) * 0.5;
					}
				}
				//this causes each of the four to update only when active and in the correct 'fpFlip'
				
				if (true == fpFlip)
				{totalmultiplier = (controlAposCL * outputpos) + (controlAnegCL * outputneg);}
				else
				{totalmultiplier = (controlBposCL * outputpos) + (controlBnegCL * outputneg);}
				//this combines the sides according to fpFlip, blending relative to the input value
				
				if (totalmultiplier != 1.0) inputSampleL *= totalmultiplier;
				if (inputSampleL > 36.0) inputSampleL = 36.0;
				if (inputSampleL < -36.0) inputSampleL = -36.0;
				//build in +18db hard clip on insano inputs
				outSampleCL = inputSampleL / compoutgain;
				//if (compoutgain != 1.0) inputSample /= compoutgain;
				//end third compressorL
				
				//begin third compressorR
				inputpos = (inputSampleR * fpOld) + (avgCR * fpNew) + 1.0;
				avgCR = inputSampleR;
				
				if (inputpos < 0.001) inputpos = 0.001;
				outputpos = inputpos / 2.0;
				if (outputpos > 1.0) outputpos = 1.0;		
				inputpos *= inputpos;
				//will be very high for hot input, can be 0.00001-1 for other-polarity
				
				dynamicRemainder = remainderC * (inputpos + 1.0);
				if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
				dynamicDivisor = 1.0 - dynamicRemainder;
				//calc chases much faster if input swing is high
				
				targetposCL *= dynamicDivisor;
				targetposCL += (inputpos * dynamicRemainder);
				calcpos = pow((1.0/targetposCR),2);
				
				inputneg = (-inputSampleR * fpOld) + (nvgCR * fpNew) + 1.0;
				nvgCR = -inputSampleR;
				
				if (inputneg < 0.001) inputneg = 0.001;
				outputneg = inputneg / 2.0;
				if (outputneg > 1.0) outputneg = 1.0;		
				inputneg *= inputneg;
				//will be very high for hot input, can be 0.00001-1 for other-polarity
				
				dynamicRemainder = remainderC * (inputneg + 1.0);
				if (dynamicRemainder > 1.0) dynamicRemainder = 1.0;
				dynamicDivisor = 1.0 - dynamicRemainder;
				//calc chases much faster if input swing is high
				
				targetnegCR *= dynamicDivisor;
				targetnegCR += (inputneg * dynamicRemainder);
				calcneg = pow((1.0/targetnegCR),2);
				//now we have mirrored targets for comp
				//outputpos and outputneg go from 0 to 1
				if (inputSampleR > 0)
				{ //working on pos
					if (true == fpFlip)
					{
						controlAposCR *= divisorC;
						controlAposCR += (calcpos*remainderC);
						if (controlAposCL > controlAposCR) controlAposCL = (controlAposCR + controlAposCL) * 0.5;
						//this part makes the compressor linked: both channels will converge towards whichever
						//is the most compressed at the time.				
					}
					else
					{
						controlBposCR *= divisorC;
						controlBposCR += (calcpos*remainderC);
						if (controlBposCL > controlBposCR) controlBposCL = (controlBposCR + controlBposCL) * 0.5;
					}	
				}
				else
				{ //working on neg
					if (true == fpFlip)
					{
						controlAnegCR *= divisorC;
						controlAnegCR += (calcneg*remainderC);
						if (controlAnegCL > controlAnegCR) controlAnegCL = (controlAnegCR + controlAnegCL) * 0.5;
					}
					else
					{
						controlBnegCR *= divisorC;
						controlBnegCR += (calcneg*remainderC);
						if (controlBnegCL > controlBnegCR) controlBnegCL = (controlBnegCR + controlBnegCL) * 0.5;
					}
				}
				//this causes each of the four to update only when active and in the correct 'fpFlip'
				
				if (true == fpFlip)
				{totalmultiplier = (controlAposCR * outputpos) + (controlAnegCR * outputneg);}
				else
				{totalmultiplier = (controlBposCR * outputpos) + (controlBnegCR * outputneg);}
				//this combines the sides according to fpFlip, blending relative to the input value
				
				if (totalmultiplier != 1.0) inputSampleR *= totalmultiplier;
				if (inputSampleR > 36.0) inputSampleR = 36.0;
				if (inputSampleR < -36.0) inputSampleR = -36.0;
				//build in +18db hard clip on insano inputs
				outSampleCR = inputSampleR / compoutgain;
				//if (compoutgain != 1.0) inputSample /= compoutgain;
				//end third compressorR
			}
		} //these nested if blocks are not indented because the old xCode doesn't support it
		
		//here we will interpolate between dry, and the three post-stages of processing
		switch (ratioselector) {
			case 0:
				inputSampleL = (drySampleL * invRatio) + (outSampleAL * ratio);
				inputSampleR = (drySampleR * invRatio) + (outSampleAR * ratio);
				break;
			case 1:
				inputSampleL = (outSampleAL * invRatio) + (outSampleBL * ratio);
				inputSampleR = (outSampleAR * invRatio) + (outSampleBR * ratio);
				break;
			default:
				inputSampleL = (outSampleBL * invRatio) + (outSampleCL * ratio);
				inputSampleR = (outSampleBR * invRatio) + (outSampleCR * ratio);
				break;
		}
		//only then do we reconstruct the output, but our ratio is built here
		
		if (outputgain != 1.0) {
			inputSampleL *= outputgain;
			inputSampleR *= outputgain;
		}
		
		if (wet != 1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}		
		fpFlip = !fpFlip;
		

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Logical4

