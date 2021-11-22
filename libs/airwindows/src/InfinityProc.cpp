/* ========================================
 *  Infinity - Infinity.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Infinity_H
#include "Infinity.h"
#endif

namespace Infinity {

void Infinity::resetTail() {
    for (int x = 0; x < 11; x++)
    {
        biquadA[x] = 0.0;
        biquadB[x] = 0.0;
        biquadC[x] = 0.0;
    }

    feedbackAL = feedbackAR = 0.0;
    feedbackBL = feedbackBR = 0.0;
    feedbackCL = feedbackCR = 0.0;
    feedbackDL = feedbackDR = 0.0;
    feedbackEL = feedbackER = 0.0;
    feedbackFL = feedbackFR = 0.0;
    feedbackGL = feedbackGR = 0.0;
    feedbackHL = feedbackHR = 0.0;

    int count;
    for (count = 0; count < 8110; count++)
    {
        aAL[count] = aAR[count] = 0.0;
    }
    for (count = 0; count < 7510; count++)
    {
        aBL[count] = aBR[count] = 0.0;
    }
    for (count = 0; count < 7310; count++)
    {
        aCL[count] = aCR[count] = 0.0;
    }
    for (count = 0; count < 6910; count++)
    {
        aDL[count] = aDR[count] = 0.0;
    }
    for (count = 0; count < 6310; count++)
    {
        aEL[count] = aER[count] = 0.0;
    }
    for (count = 0; count < 6110; count++)
    {
        aFL[count] = aFR[count] = 0.0;
    }
    for (count = 0; count < 5510; count++)
    {
        aGL[count] = aGR[count] = 0.0;
    }
    for (count = 0; count < 4910; count++)
    {
        aHL[count] = aHR[count] = 0.0;
    }
    // maximum value needed will be delay * 100, plus 206 (absolute max vibrato depth)
    for (count = 0; count < 4510; count++)
    {
        aIL[count] = aIR[count] = 0.0;
    }
    for (count = 0; count < 4310; count++)
    {
        aJL[count] = aJR[count] = 0.0;
    }
    for (count = 0; count < 3910; count++)
    {
        aKL[count] = aKR[count] = 0.0;
    }
    for (count = 0; count < 3310; count++)
    {
        aLL[count] = aLR[count] = 0.0;
    }
    // maximum value will be delay * 100
    countA = 1;
    delayA = 79;
    countB = 1;
    delayB = 73;
    countC = 1;
    delayC = 71;
    countD = 1;
    delayD = 67;
    countE = 1;
    delayE = 61;
    countF = 1;
    delayF = 59;
    countG = 1;
    delayG = 53;
    countH = 1;
    delayH = 47;
    // the householder matrices
    countI = 1;
    delayI = 43;
    countJ = 1;
    delayJ = 41;
    countK = 1;
    delayK = 37;
    countL = 1;
    delayL = 31;
    // the allpasses

    A = 1.0;
    B = 0.0;
    C = 0.5;
    D = 1.0;
    E = 0.0f;
    fpdL = 1.0;
    while (fpdL < 16386)
        fpdL = rand() * UINT32_MAX;
    fpdR = 1.0;
    while (fpdR < 16386)
        fpdR = rand() * UINT32_MAX;
}

void Infinity::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    if (tailFadeOutCounter == -1) // don't change the clear state if tail fade out already in progress
    {
        if (clearState)
        {
            // HIGH to LOW
            if (E < 0.5f)
            {
                clearState = false;
            }
        }
        else
        {
            // LOW to HIGH
            if (E >= 0.5f)
            {
                clearState = true;
                tailFadeOutCounter = 0;
                tailFadeOutLen = sr * 0.02; // fade for 20 ms, ideally this would be configurable...
            }
        }
   
    }
	

	float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	biquadC[0] = biquadB[0] = biquadA[0] = ((pow(A,2)*9900.0)+100.0) / getSampleRate();
	biquadA[1] = 0.618033988749894848204586;
	biquadB[1] = (A*0.5)+0.118033988749894848204586;
    biquadC[1] = 0.5;
	
	double K = tan(M_PI * biquadA[0]); //lowpass
	double norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
	biquadA[2] = K * K * norm;
	biquadA[3] = 2.0 * biquadA[2];
	biquadA[4] = biquadA[2];
	biquadA[5] = 2.0 * (K * K - 1.0) * norm;
	biquadA[6] = (1.0 - K / biquadA[1] + K * K) * norm;
	
	K = tan(M_PI * biquadA[0]);
	norm = 1.0 / (1.0 + K / biquadB[1] + K * K);
	biquadB[2] = K * K * norm;
	biquadB[3] = 2.0 * biquadB[2];
	biquadB[4] = biquadB[2];
	biquadB[5] = 2.0 * (K * K - 1.0) * norm;
	biquadB[6] = (1.0 - K / biquadB[1] + K * K) * norm;
	
	K = tan(M_PI * biquadC[0]);
	norm = 1.0 / (1.0 + K / biquadC[1] + K * K);
	biquadC[2] = K * K * norm;
	biquadC[3] = 2.0 * biquadC[2];
	biquadC[4] = biquadC[2];
	biquadC[5] = 2.0 * (K * K - 1.0) * norm;
	biquadC[6] = (1.0 - K / biquadC[1] + K * K) * norm;
	
	double damping = pow(B,2)*0.5;
	
	double size = (pow(C,2)*90.0)+10.0;
	
	double wet = D;
	
	delayA = 79*size;
	delayB = 73*size;
	delayC = 71*size;
	delayD = 67*size;
	delayE = 61*size;
	delayF = 59*size;
	delayG = 53*size;
	delayH = 47*size;
	
	delayI = 43*size;
	delayJ = 41*size;
	delayK = 37*size;
	delayL = 31*size;
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		long double tempSampleL = (inputSampleL * biquadA[2]) + biquadA[7];
		biquadA[7] = (inputSampleL * biquadA[3]) - (tempSampleL * biquadA[5]) + biquadA[8];
		biquadA[8] = (inputSampleL * biquadA[4]) - (tempSampleL * biquadA[6]);
		inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel
		
		long double tempSampleR = (inputSampleR * biquadA[2]) + biquadA[9];
		biquadA[9] = (inputSampleR * biquadA[3]) - (tempSampleR * biquadA[5]) + biquadA[10];
		biquadA[10] = (inputSampleR * biquadA[4]) - (tempSampleR * biquadA[6]);
		inputSampleR = tempSampleR; //note: 9 and 10 store the R channel
		
		inputSampleL *= wet;
		inputSampleR *= wet;
		//we're going to use this as a kind of balance since the reverb buildup can be so large
		inputSampleL *= 0.5;
		inputSampleR *= 0.5;
				
		long double allpassIL = inputSampleL;
		long double allpassJL = inputSampleL;
		long double allpassKL = inputSampleL;
		long double allpassLL = inputSampleL;
		
		long double allpassIR = inputSampleR;
		long double allpassJR = inputSampleR;
		long double allpassKR = inputSampleR;
		long double allpassLR = inputSampleR;
		
		int allpasstemp = countI + 1;
		if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = 0;}
		allpassIL -= aIL[allpasstemp]*0.5;
		aIL[countI] = allpassIL;
		allpassIL *= 0.5;
		allpassIR -= aIR[allpasstemp]*0.5;
		aIR[countI] = allpassIR;
		allpassIR *= 0.5;
		countI++; if (countI < 0 || countI > delayI) {countI = 0;}		
		allpassIL += (aIL[countI]);
		allpassIR += (aIR[countI]);
		
		allpasstemp = countJ + 1;
		if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = 0;}
		allpassJL -= aJL[allpasstemp]*0.5;
		aJL[countJ] = allpassJL;
		allpassJL *= 0.5;
		allpassJR -= aJR[allpasstemp]*0.5;
		aJR[countJ] = allpassJR;
		allpassJR *= 0.5;
		countJ++; if (countJ < 0 || countJ > delayJ) {countJ = 0;}		
		allpassJL += (aJL[countJ]);
		allpassJR += (aJR[countJ]);
		
		allpasstemp = countK + 1;
		if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = 0;}
		allpassKL -= aKL[allpasstemp]*0.5;
		aKL[countK] = allpassKL;
		allpassKL *= 0.5;
		allpassKR -= aKR[allpasstemp]*0.5;
		aKR[countK] = allpassKR;
		allpassKR *= 0.5;
		countK++; if (countK < 0 || countK > delayK) {countK = 0;}		
		allpassKL += (aKL[countK]);
		allpassKR += (aKR[countK]);
		
		allpasstemp = countL + 1;
		if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = 0;}
		allpassLL -= aLL[allpasstemp]*0.5;
		aLL[countL] = allpassLL;
		allpassLL *= 0.5;
		allpassLR -= aLR[allpasstemp]*0.5;
		aLR[countL] = allpassLR;
		allpassLR *= 0.5;
		countL++; if (countL < 0 || countL > delayL) {countL = 0;}		
		allpassLL += (aLL[countL]);		
		allpassLR += (aLR[countL]);		
		//the big allpass in front of everything
		
		aAL[countA] = allpassLL + feedbackAL;
		aBL[countB] = allpassKL + feedbackBL;
		aCL[countC] = allpassJL + feedbackCL;
		aDL[countD] = allpassIL + feedbackDL;
		aEL[countE] = allpassIL + feedbackEL;
		aFL[countF] = allpassJL + feedbackFL;
		aGL[countG] = allpassKL + feedbackGL;
		aHL[countH] = allpassLL + feedbackHL; //L
		
		aAR[countA] = allpassLR + feedbackAR;
		aBR[countB] = allpassKR + feedbackBR;
		aCR[countC] = allpassJR + feedbackCR;
		aDR[countD] = allpassIR + feedbackDR;
		aER[countE] = allpassIR + feedbackER;
		aFR[countF] = allpassJR + feedbackFR;
		aGR[countG] = allpassKR + feedbackGR;
		aHR[countH] = allpassLR + feedbackHR; //R
		
		countA++; if (countA < 0 || countA > delayA) {countA = 0;}
		countB++; if (countB < 0 || countB > delayB) {countB = 0;}
		countC++; if (countC < 0 || countC > delayC) {countC = 0;}
		countD++; if (countD < 0 || countD > delayD) {countD = 0;}
		countE++; if (countE < 0 || countE > delayE) {countE = 0;}
		countF++; if (countF < 0 || countF > delayF) {countF = 0;}
		countG++; if (countG < 0 || countG > delayG) {countG = 0;}
		countH++; if (countH < 0 || countH > delayH) {countH = 0;}
		//the Householder matrices (shared between channels, offset is stereo)

		
		double infiniteAL = (aAL[countA-((countA > delayA)?delayA+1:0)] * (1-(damping-floor(damping))) );
		infiniteAL += (aAL[countA+1-((countA+1 > delayA)?delayA+1:0)] * ((damping-floor(damping))) );
		double infiniteBL = aBL[countB-((countB > delayB)?delayB+1:0)];
		double infiniteCL = aCL[countC-((countC > delayC)?delayC+1:0)];
		double infiniteDL = aDL[countD-((countD > delayD)?delayD+1:0)];
		
		double infiniteAR = (aAR[countA-((countA > delayA)?delayA+1:0)] * (1-(damping-floor(damping))) );
		infiniteAR += (aAR[countA+1-((countA+1 > delayA)?delayA+1:0)] * ((damping-floor(damping))) );
		double infiniteBR = aBR[countB-((countB > delayB)?delayB+1:0)];
		double infiniteCR = aCR[countC-((countC > delayC)?delayC+1:0)];
		double infiniteDR = aDR[countD-((countD > delayD)?delayD+1:0)];
		
		double infiniteEL = (aEL[countE-((countE > delayE)?delayE+1:0)] * (1-(damping-floor(damping))) );
		infiniteEL += (aEL[countE+1-((countE+1 > delayE)?delayE+1:0)] * ((damping-floor(damping))) );
		double infiniteFL = aFL[countF-((countF > delayF)?delayF+1:0)];
		double infiniteGL = aGL[countG-((countG > delayG)?delayG+1:0)];
		double infiniteHL = aHL[countH-((countH > delayH)?delayH+1:0)];
		
		double infiniteER = (aER[countE-((countE > delayE)?delayE+1:0)] * (1-(damping-floor(damping))) );
		infiniteER += (aER[countE+1-((countE+1 > delayE)?delayE+1:0)] * ((damping-floor(damping))) );
		double infiniteFR = aFR[countF-((countF > delayF)?delayF+1:0)];
		double infiniteGR = aGR[countG-((countG > delayG)?delayG+1:0)];
		double infiniteHR = aHR[countH-((countH > delayH)?delayH+1:0)];
		
		double dialBackAL = 0.5;
		double dialBackEL = 0.5;
		double dialBackDryL = 0.5;
		if (fabs(infiniteAL)>0.4) dialBackAL -= ((fabs(infiniteAL)-0.4)*0.2);
		if (fabs(infiniteEL)>0.4) dialBackEL -= ((fabs(infiniteEL)-0.4)*0.2);
		if (fabs(drySampleL)>0.4) dialBackDryL -= ((fabs(drySampleL)-0.4)*0.2);
		//we're compressing things subtly so we can feed energy in and not overload
		
		double dialBackAR = 0.5;
		double dialBackER = 0.5;
		double dialBackDryR = 0.5;
		if (fabs(infiniteAR)>0.4) dialBackAR -= ((fabs(infiniteAR)-0.4)*0.2);
		if (fabs(infiniteER)>0.4) dialBackER -= ((fabs(infiniteER)-0.4)*0.2);
		if (fabs(drySampleR)>0.4) dialBackDryR -= ((fabs(drySampleR)-0.4)*0.2);
		//we're compressing things subtly so we can feed energy in and not overload
		
		feedbackAL = (infiniteAL - (infiniteBL + infiniteCL + infiniteDL)) * dialBackAL;
		feedbackBL = (infiniteBL - (infiniteAL + infiniteCL + infiniteDL)) * dialBackDryL;
		feedbackCL = (infiniteCL - (infiniteAL + infiniteBL + infiniteDL)) * dialBackDryL;
		feedbackDL = (infiniteDL - (infiniteAL + infiniteBL + infiniteCL)) * dialBackDryL; //Householder feedback matrix, L
		
		feedbackEL = (infiniteEL - (infiniteFL + infiniteGL + infiniteHL)) * dialBackEL;
		feedbackFL = (infiniteFL - (infiniteEL + infiniteGL + infiniteHL)) * dialBackDryL;
		feedbackGL = (infiniteGL - (infiniteEL + infiniteFL + infiniteHL)) * dialBackDryL;
		feedbackHL = (infiniteHL - (infiniteEL + infiniteFL + infiniteGL)) * dialBackDryL; //Householder feedback matrix, L
		
		feedbackAR = (infiniteAR - (infiniteBR + infiniteCR + infiniteDR)) * dialBackAR;
		feedbackBR = (infiniteBR - (infiniteAR + infiniteCR + infiniteDR)) * dialBackDryR;
		feedbackCR = (infiniteCR - (infiniteAR + infiniteBR + infiniteDR)) * dialBackDryR;
		feedbackDR = (infiniteDR - (infiniteAR + infiniteBR + infiniteCR)) * dialBackDryR; //Householder feedback matrix, R
		
		feedbackER = (infiniteER - (infiniteFR + infiniteGR + infiniteHR)) * dialBackER;
		feedbackFR = (infiniteFR - (infiniteER + infiniteGR + infiniteHR)) * dialBackDryR;
		feedbackGR = (infiniteGR - (infiniteER + infiniteFR + infiniteHR)) * dialBackDryR;
		feedbackHR = (infiniteHR - (infiniteER + infiniteFR + infiniteGR)) * dialBackDryR; //Householder feedback matrix, R
		
		inputSampleL = (infiniteAL + infiniteBL + infiniteCL + infiniteDL + infiniteEL + infiniteFL + infiniteGL + infiniteHL)/8.0;
		inputSampleR = (infiniteAR + infiniteBR + infiniteCR + infiniteDR + infiniteER + infiniteFR + infiniteGR + infiniteHR)/8.0;
		
		tempSampleL = (inputSampleL * biquadB[2]) + biquadB[7];
		biquadB[7] = (inputSampleL * biquadB[3]) - (tempSampleL * biquadB[5]) + biquadB[8];
		biquadB[8] = (inputSampleL * biquadB[4]) - (tempSampleL * biquadB[6]);
		inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel
		
		tempSampleR = (inputSampleR * biquadB[2]) + biquadB[9];
		biquadB[9] = (inputSampleR * biquadB[3]) - (tempSampleR * biquadB[5]) + biquadB[10];
		biquadB[10] = (inputSampleR * biquadB[4]) - (tempSampleR * biquadB[6]);
		inputSampleR = tempSampleR; //note: 9 and 10 store the R channel
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		//without this, you can get a NaN condition where it spits out DC offset at full blast!
		
		inputSampleL = asin(inputSampleL);
		inputSampleR = asin(inputSampleR);
		
		tempSampleL = (inputSampleL * biquadC[2]) + biquadC[7];
		biquadC[7] = (inputSampleL * biquadC[3]) - (tempSampleL * biquadC[5]) + biquadC[8];
		biquadC[8] = (inputSampleL * biquadC[4]) - (tempSampleL * biquadC[6]);
		inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel
		
		tempSampleR = (inputSampleR * biquadC[2]) + biquadC[9];
		biquadC[9] = (inputSampleR * biquadC[3]) - (tempSampleR * biquadC[5]) + biquadC[10];
		biquadC[10] = (inputSampleR * biquadC[4]) - (tempSampleR * biquadC[6]);
		inputSampleR = tempSampleR; //note: 9 and 10 store the R channel
        
        if (tailFadeOutCounter >=0 )
        {
            float fadeGain = 1.0f - 1.0f / tailFadeOutLen * tailFadeOutCounter;
            inputSampleL = (inputSampleL * wet * fadeGain) + (drySampleL * (1.0 - wet));
            inputSampleR = (inputSampleR * wet * fadeGain) + (drySampleR * (1.0 - wet));
            ++tailFadeOutCounter;
            if (tailFadeOutCounter == tailFadeOutLen)
            {
                tailFadeOutCounter = -1;
                resetTail();
			}
		} else
        {
            if (wet !=1.0) {
                    inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
                    inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
            }
        }
        
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
	
}

} // end namespace Infinity

