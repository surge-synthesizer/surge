/* ========================================
 *  MackEQ - MackEQ.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __MackEQ_H
#include "MackEQ.h"
#endif

namespace MackEQ {


void MackEQ::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double inTrim = A*10.0;
	inTrim *= inTrim;
	
	double gainHigh = pow(B,2)*4.0;
	double outHigh = sqrt(B);
	double gainBass = pow(C,2)*4.0;
	double outBass = sqrt(C);
	double outPad = D;
	double wet = E;
	
	double iirAmountA = 0.001860867/overallscale;
	double iirAmountB = 0.000287496/overallscale;
	double iirBassMid = 0.159/overallscale;
	double iirMidHigh = 0.236/overallscale;
	
	biquadD[0] = biquadC[0] = biquadB[0] = biquadA[0] = 19160.0 / getSampleRate();
    biquadA[1] = 0.431684981684982;
	biquadB[1] = 1.1582298;
	biquadC[1] = 0.657027382751269;
	biquadD[1] = 1.076210852946577;
	
	double K = tan(M_PI * biquadA[0]); //lowpass
	double norm = 1.0 / (1.0 + K / biquadA[1] + K * K);
	biquadA[2] = K * K * norm;
	biquadA[3] = 2.0 * biquadA[2];
	biquadA[4] = biquadA[2];
	biquadA[5] = 2.0 * (K * K - 1.0) * norm;
	biquadA[6] = (1.0 - K / biquadA[1] + K * K) * norm;
	
	K = tan(M_PI * biquadB[0]);
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
	
	K = tan(M_PI * biquadD[0]);
	norm = 1.0 / (1.0 + K / biquadD[1] + K * K);
	biquadD[2] = K * K * norm;
	biquadD[3] = 2.0 * biquadD[2];
	biquadD[4] = biquadD[2];
	biquadD[5] = 2.0 * (K * K - 1.0) * norm;
	biquadD[6] = (1.0 - K / biquadD[1] + K * K) * norm;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		if (fabs(iirSampleAL)<1.18e-37) iirSampleAL = 0.0;
		iirSampleAL = (iirSampleAL * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
		inputSampleL -= iirSampleAL;
		if (fabs(iirSampleAR)<1.18e-37) iirSampleAR = 0.0;
		iirSampleAR = (iirSampleAR * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
		inputSampleR -= iirSampleAR;
		
		if (inTrim != 1.0) {inputSampleL *= inTrim; inputSampleR *= inTrim;}
		
		//begin Mackity input stage
		long double outSampleL = biquadA[2]*inputSampleL+biquadA[3]*biquadA[7]+biquadA[4]*biquadA[8]-biquadA[5]*biquadA[9]-biquadA[6]*biquadA[10];
		biquadA[8] = biquadA[7]; biquadA[7] = inputSampleL; inputSampleL = outSampleL; biquadA[10] = biquadA[9]; biquadA[9] = inputSampleL; //DF1 left
		
		long double outSampleR = biquadA[2]*inputSampleR+biquadA[3]*biquadA[11]+biquadA[4]*biquadA[12]-biquadA[5]*biquadA[13]-biquadA[6]*biquadA[14];
		biquadA[12] = biquadA[11]; biquadA[11] = inputSampleR; inputSampleR = outSampleR; biquadA[14] = biquadA[13]; biquadA[13] = inputSampleR; //DF1 right
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		inputSampleL -= pow(inputSampleL,5)*0.1768;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleR -= pow(inputSampleR,5)*0.1768;
		
		outSampleL = biquadB[2]*inputSampleL+biquadB[3]*biquadB[7]+biquadB[4]*biquadB[8]-biquadB[5]*biquadB[9]-biquadB[6]*biquadB[10];
		biquadB[8] = biquadB[7]; biquadB[7] = inputSampleL; inputSampleL = outSampleL; biquadB[10] = biquadB[9]; biquadB[9] = inputSampleL; //DF1 left
		
		outSampleR = biquadB[2]*inputSampleR+biquadB[3]*biquadB[11]+biquadB[4]*biquadB[12]-biquadB[5]*biquadB[13]-biquadB[6]*biquadB[14];
		biquadB[12] = biquadB[11]; biquadB[11] = inputSampleR; inputSampleR = outSampleR; biquadB[14] = biquadB[13]; biquadB[13] = inputSampleR; //DF1 right
		
		if (fabs(iirSampleBL)<1.18e-37) iirSampleBL = 0.0;
		iirSampleBL = (iirSampleBL * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
		inputSampleL -= iirSampleBL;
		if (fabs(iirSampleBR)<1.18e-37) iirSampleBR = 0.0;
		iirSampleBR = (iirSampleBR * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
		inputSampleR -= iirSampleBR;
		//end Mackity input stage
		
		//begin EQ section
		if (fabs(iirSampleCL)<1.18e-37) iirSampleCL = 0.0;
		iirSampleCL = (iirSampleCL * (1.0 - iirBassMid)) + (inputSampleL * iirBassMid);
		if (fabs(iirSampleCR)<1.18e-37) iirSampleCR = 0.0;
		iirSampleCR = (iirSampleCR * (1.0 - iirBassMid)) + (inputSampleR * iirBassMid);
		
		long double bassSampleL = iirSampleCL;
		long double midSampleL = inputSampleL - bassSampleL;
		long double bassSampleR = iirSampleCR;
		long double midSampleR = inputSampleR - bassSampleR;
		
		if (gainBass != 1.0) {bassSampleL *= gainBass; bassSampleR *= gainBass;}
		
		if (bassSampleL > 1.0) bassSampleL = 1.0;
		if (bassSampleL < -1.0) bassSampleL = -1.0;
		bassSampleL -= pow(bassSampleL,5)*0.1768;
		if (bassSampleR > 1.0) bassSampleR = 1.0;
		if (bassSampleR < -1.0) bassSampleR = -1.0;
		bassSampleR -= pow(bassSampleR,5)*0.1768;
		
		if (fabs(iirSampleDL)<1.18e-37) iirSampleDL = 0.0;
		iirSampleDL = (iirSampleDL * (1.0 - iirBassMid)) + (bassSampleL * iirBassMid);
		bassSampleL = iirSampleDL;
		if (fabs(iirSampleDR)<1.18e-37) iirSampleDR = 0.0;
		iirSampleDR = (iirSampleDR * (1.0 - iirBassMid)) + (bassSampleR * iirBassMid);
		bassSampleR = iirSampleDR;
		//we've taken the bass sample, made the mids from it, distorted it
		//and hit it with another pole of darkening.
		//mid sample is still normal from undistorted bass
		
		if (fabs(iirSampleEL)<1.18e-37) iirSampleEL = 0.0;
		iirSampleEL = (iirSampleEL * (1.0 - iirMidHigh)) + (midSampleL * iirMidHigh);
		long double highSampleL = midSampleL - iirSampleEL;
		midSampleL = iirSampleEL;
		if (fabs(iirSampleER)<1.18e-37) iirSampleER = 0.0;
		iirSampleER = (iirSampleER * (1.0 - iirMidHigh)) + (midSampleR * iirMidHigh);
		long double highSampleR = midSampleR - iirSampleER;
		midSampleR = iirSampleER;
		//here is where we make the high sample out of the mid, and take highs
		//away from the mid.
		
		if (fabs(iirSampleFL)<1.18e-37) iirSampleFL = 0.0;
		iirSampleFL = (iirSampleFL * (1.0 - iirMidHigh)) + (highSampleL * iirMidHigh);
		highSampleL -= iirSampleFL;
		if (fabs(iirSampleFR)<1.18e-37) iirSampleFR = 0.0;
		iirSampleFR = (iirSampleFR * (1.0 - iirMidHigh)) + (highSampleR * iirMidHigh);
		highSampleR -= iirSampleFR;
		
		if (gainHigh != 1.0) {highSampleL *= gainHigh; highSampleR *= gainHigh;}
		
		if (highSampleL > 1.0) highSampleL = 1.0;
		if (highSampleL < -1.0) highSampleL = -1.0;
		highSampleL -= pow(highSampleL,5)*0.1768;
		if (highSampleR > 1.0) highSampleR = 1.0;
		if (highSampleR < -1.0) highSampleR = -1.0;
		highSampleR -= pow(highSampleR,5)*0.1768;
		//highpassing HighSample another stage, before distorting it
		
		inputSampleL = ((bassSampleL*outBass) + midSampleL + (highSampleL*outHigh))*4.0;
		inputSampleR = ((bassSampleR*outBass) + midSampleR + (highSampleR*outHigh))*4.0;
		//end EQ section
		
		outSampleL = biquadC[2]*inputSampleL+biquadC[3]*biquadC[7]+biquadC[4]*biquadC[8]-biquadC[5]*biquadC[9]-biquadC[6]*biquadC[10];
		biquadC[8] = biquadC[7]; biquadC[7] = inputSampleL; inputSampleL = outSampleL; biquadC[10] = biquadC[9]; biquadC[9] = inputSampleL; //DF1 left
		
		outSampleR = biquadC[2]*inputSampleR+biquadC[3]*biquadC[11]+biquadC[4]*biquadC[12]-biquadC[5]*biquadC[13]-biquadC[6]*biquadC[14];
		biquadC[12] = biquadC[11]; biquadC[11] = inputSampleR; inputSampleR = outSampleR; biquadC[14] = biquadC[13]; biquadC[13] = inputSampleR; //DF1 right
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		inputSampleL -= pow(inputSampleL,5)*0.1768;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		inputSampleR -= pow(inputSampleR,5)*0.1768;
		
		outSampleL = biquadD[2]*inputSampleL+biquadD[3]*biquadD[7]+biquadD[4]*biquadD[8]-biquadD[5]*biquadD[9]-biquadD[6]*biquadD[10];
		biquadD[8] = biquadD[7]; biquadD[7] = inputSampleL; inputSampleL = outSampleL; biquadD[10] = biquadD[9]; biquadD[9] = inputSampleL; //DF1 left
		
		outSampleR = biquadD[2]*inputSampleR+biquadD[3]*biquadD[11]+biquadD[4]*biquadD[12]-biquadD[5]*biquadD[13]-biquadD[6]*biquadD[14];
		biquadD[12] = biquadD[11]; biquadD[11] = inputSampleR; inputSampleR = outSampleR; biquadD[14] = biquadD[13]; biquadD[13] = inputSampleR; //DF1 right
		
		if (outPad != 1.0) {
			inputSampleL *= outPad;
			inputSampleR *= outPad;
		}
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace MackEQ

