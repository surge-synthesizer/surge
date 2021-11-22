/* ========================================
 *  Compresaturator - Compresaturator.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Compresaturator_H
#include "Compresaturator.h"
#endif

namespace Compresaturator {


void Compresaturator::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double inputgain = pow(10.0,((A*24.0)-12.0)/20.0);
	double satComp = B*2.0;
	int widestRange = C*C*C*5000;
	if (widestRange < 50) widestRange = 50;
	satComp += (((double)widestRange/3000.0)*satComp);
	//set the max wideness of comp zone, minimum range boosted (too much?)
	double output = D;
	double wet = E;
	    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		if (dCount < 1 || dCount > 5000) {dCount = 5000;}
		
		//begin drive L
		long double temp = inputSampleL;
		double variSpeed = 1.0 + ((padFactorL/lastWidthL)*satComp);
		if (variSpeed < 1.0) variSpeed = 1.0;
		double totalgain = inputgain / variSpeed;
		if (totalgain != 1.0) {
			inputSampleL *= totalgain;
			if (totalgain < 1.0) {
				temp *= totalgain;
				//no boosting beyond unity please
			}
		}
		
		long double bridgerectifier = fabs(inputSampleL);
		double overspill = 0;
		int targetWidth = widestRange;
		//we now have defaults and an absolute input value to work with
		if (bridgerectifier < 0.01) padFactorL *= 0.9999;
		//in silences we bring back padFactor if it got out of hand		
		if (bridgerectifier > 1.57079633) {
			bridgerectifier = 1.57079633;
			targetWidth = 8;
		}
		//if our output's gone beyond saturating to distorting, we begin chasing the
		//buffer size smaller. Anytime we don't have that, we expand (smoothest sound, only adding to an increasingly subdivided buffer)
		
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) {
			inputSampleL = bridgerectifier;
			overspill = temp - bridgerectifier;
		}
		
		if (inputSampleL < 0) {
			inputSampleL = -bridgerectifier;
			overspill = (-temp) - bridgerectifier;
		} 
		//drive section L
		
		//begin drive R
		temp = inputSampleR;
		variSpeed = 1.0 + ((padFactorR/lastWidthR)*satComp);
		if (variSpeed < 1.0) variSpeed = 1.0;
		totalgain = inputgain / variSpeed;
		if (totalgain != 1.0) {
			inputSampleR *= totalgain;
			if (totalgain < 1.0) {
				temp *= totalgain;
				//no boosting beyond unity please
			}
		}
		
		bridgerectifier = fabs(inputSampleR);
		overspill = 0;
		targetWidth = widestRange;
		//we now have defaults and an absolute input value to work with
		if (bridgerectifier < 0.01) padFactorR *= 0.9999;
		//in silences we bring back padFactor if it got out of hand		
		if (bridgerectifier > 1.57079633) {
			bridgerectifier = 1.57079633;
			targetWidth = 8;
		}
		//if our output's gone beyond saturating to distorting, we begin chasing the
		//buffer size smaller. Anytime we don't have that, we expand (smoothest sound, only adding to an increasingly subdivided buffer)
		
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) {
			inputSampleR = bridgerectifier;
			overspill = temp - bridgerectifier;
		}
		
		if (inputSampleR < 0) {
			inputSampleR = -bridgerectifier;
			overspill = (-temp) - bridgerectifier;
		} 
		//drive section R
		
		
		dL[dCount + 5000] = dL[dCount] = overspill * satComp;
		dR[dCount + 5000] = dR[dCount] = overspill * satComp;
		dCount--;
		//we now have a big buffer to draw from, which is always positive amount of overspill
		
		//begin pad L
		padFactorL += dL[dCount];
		double randy = (rand()/(double)RAND_MAX);
		if ((targetWidth*randy) > lastWidthL) {
			//we are expanding the buffer so we don't remove this trailing sample
			lastWidthL += 1;
		} else {
			padFactorL -= dL[dCount+lastWidthL];
			//zero change, or target is smaller and we are shrinking
			if (targetWidth < lastWidthL) {
				lastWidthL -= 1;
				if (lastWidthL < 2) lastWidthL = 2;
				//sanity check as randy can give us target zero
				padFactorL -= dL[dCount+lastWidthL];
			}
		}
		//variable attack/release speed more rapid as comp intensity increases
		//implemented in a way where we're repeatedly not altering the buffer as it expands, which makes the comp artifacts smoother
		if (padFactorL < 0) padFactorL = 0;
		//end pad L
		
		//begin pad R
		padFactorR += dR[dCount];
		randy = (rand()/(double)RAND_MAX);
		if ((targetWidth*randy) > lastWidthR) {
			//we are expanding the buffer so we don't remove this trailing sample
			lastWidthR += 1;
		} else {
			padFactorR -= dR[dCount+lastWidthR];
			//zero change, or target is smaller and we are shrinking
			if (targetWidth < lastWidthR) {
				lastWidthR -= 1;
				if (lastWidthR < 2) lastWidthR = 2;
				//sanity check as randy can give us target zero
				padFactorR -= dR[dCount+lastWidthR];
			}
		}
		//variable attack/release speed more rapid as comp intensity increases
		//implemented in a way where we're repeatedly not altering the buffer as it expands, which makes the comp artifacts smoother
		if (padFactorR < 0) padFactorR = 0;
		//end pad R
				
		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		
		if (wet < 1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace Compresaturator

