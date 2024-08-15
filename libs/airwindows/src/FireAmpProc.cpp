/* ========================================
 *  FireAmp - FireAmp.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Gain_H
#include "FireAmp.h"
#endif

namespace FireAmp {


void FireAmp::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	double bassfill = A;
	double outputlevel = C;
	double wet = D;
	
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	int cycleEnd = floor(overallscale);
	if (cycleEnd < 1) cycleEnd = 1;
	if (cycleEnd > 4) cycleEnd = 4;
	//this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
	if (cycle > cycleEnd-1) cycle = cycleEnd-1; //sanity check		
	
	double startlevel = bassfill;
	double samplerate = getSampleRate();
	double basstrim = bassfill / 16.0;
	double toneEQ = (B / samplerate)*22050.0;
	double EQ = (basstrim / samplerate)*22050.0;
	double bleed = outputlevel/16.0;
	double bassfactor = 1.0-(basstrim*basstrim);
	double BEQ = (bleed / samplerate)*22050.0;
	int diagonal = (int)(0.000861678*samplerate);
	if (diagonal > 127) diagonal = 127;
	int side = (int)(diagonal/1.4142135623730951);
	int down = (side + diagonal)/2;
	//now we've got down, side and diagonal as offsets and we also use three successive samples upfront
	
	double cutoff = (15000.0+(B*10000.0)) / getSampleRate();
	if (cutoff > 0.49) cutoff = 0.49; //don't crash if run at 44.1k
	if (cutoff < 0.001) cutoff = 0.001; //or if cutoff's too low
	
	fixF[fix_freq] = fixE[fix_freq] = fixD[fix_freq] = fixC[fix_freq] = fixB[fix_freq] = fixA[fix_freq] = cutoff;
	
    fixA[fix_reso] = 4.46570214;
	fixB[fix_reso] = 1.51387132;
	fixC[fix_reso] = 0.93979296;
	fixD[fix_reso] = 0.70710678;
	fixE[fix_reso] = 0.52972649;
	fixF[fix_reso] = 0.50316379;
	
	double K = tan(M_PI * fixA[fix_freq]); //lowpass
	double norm = 1.0 / (1.0 + K / fixA[fix_reso] + K * K);
	fixA[fix_a0] = K * K * norm;
	fixA[fix_a1] = 2.0 * fixA[fix_a0];
	fixA[fix_a2] = fixA[fix_a0];
	fixA[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixA[fix_b2] = (1.0 - K / fixA[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixB[fix_freq]);
	norm = 1.0 / (1.0 + K / fixB[fix_reso] + K * K);
	fixB[fix_a0] = K * K * norm;
	fixB[fix_a1] = 2.0 * fixB[fix_a0];
	fixB[fix_a2] = fixB[fix_a0];
	fixB[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixB[fix_b2] = (1.0 - K / fixB[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixC[fix_freq]);
	norm = 1.0 / (1.0 + K / fixC[fix_reso] + K * K);
	fixC[fix_a0] = K * K * norm;
	fixC[fix_a1] = 2.0 * fixC[fix_a0];
	fixC[fix_a2] = fixC[fix_a0];
	fixC[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixC[fix_b2] = (1.0 - K / fixC[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixD[fix_freq]);
	norm = 1.0 / (1.0 + K / fixD[fix_reso] + K * K);
	fixD[fix_a0] = K * K * norm;
	fixD[fix_a1] = 2.0 * fixD[fix_a0];
	fixD[fix_a2] = fixD[fix_a0];
	fixD[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixD[fix_b2] = (1.0 - K / fixD[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixE[fix_freq]);
	norm = 1.0 / (1.0 + K / fixE[fix_reso] + K * K);
	fixE[fix_a0] = K * K * norm;
	fixE[fix_a1] = 2.0 * fixE[fix_a0];
	fixE[fix_a2] = fixE[fix_a0];
	fixE[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixE[fix_b2] = (1.0 - K / fixE[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixF[fix_freq]);
	norm = 1.0 / (1.0 + K / fixF[fix_reso] + K * K);
	fixF[fix_a0] = K * K * norm;
	fixF[fix_a1] = 2.0 * fixF[fix_a0];
	fixF[fix_a2] = fixF[fix_a0];
	fixF[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixF[fix_b2] = (1.0 - K / fixF[fix_reso] + K * K) * norm;
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		
		double outSample = (inputSampleL * fixA[fix_a0]) + fixA[fix_sL1];
		fixA[fix_sL1] = (inputSampleL * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sL2];
		fixA[fix_sL2] = (inputSampleL * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixA[fix_a0]) + fixA[fix_sR1];
		fixA[fix_sR1] = (inputSampleR * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sR2];
		fixA[fix_sR2] = (inputSampleR * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		double basscutL = 0.98;
		//we're going to be shifting this as the stages progress
		double inputlevelL = startlevel;
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleAL = (iirSampleAL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleAL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		double bridgerectifier = (smoothAL + inputSampleL);
		smoothAL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleBL = (iirSampleBL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleBL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothBL + inputSampleL);
		smoothBL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		double basscutR = 0.98;
		//we're going to be shifting this as the stages progress
		double inputlevelR = startlevel;
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleAR = (iirSampleAR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleAR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothAR + inputSampleR);
		smoothAR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleBR = (iirSampleBR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleBR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothBR + inputSampleR);
		smoothBR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		
		outSample = (inputSampleL * fixB[fix_a0]) + fixB[fix_sL1];
		fixB[fix_sL1] = (inputSampleL * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sL2];
		fixB[fix_sL2] = (inputSampleL * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixB[fix_a0]) + fixB[fix_sR1];
		fixB[fix_sR1] = (inputSampleR * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sR2];
		fixB[fix_sR2] = (inputSampleR * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleCL = (iirSampleCL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleCL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothCL + inputSampleL);
		smoothCL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleDL = (iirSampleDL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleDL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654) );
		//overdrive
		bridgerectifier = (smoothDL + inputSampleL);
		smoothDL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleCR = (iirSampleCR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleCR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothCR + inputSampleR);
		smoothCR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleDR = (iirSampleDR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleDR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothDR + inputSampleR);
		smoothDR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixC[fix_a0]) + fixC[fix_sL1];
		fixC[fix_sL1] = (inputSampleL * fixC[fix_a1]) - (outSample * fixC[fix_b1]) + fixC[fix_sL2];
		fixC[fix_sL2] = (inputSampleL * fixC[fix_a2]) - (outSample * fixC[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixC[fix_a0]) + fixC[fix_sR1];
		fixC[fix_sR1] = (inputSampleR * fixC[fix_a1]) - (outSample * fixC[fix_b1]) + fixC[fix_sR2];
		fixC[fix_sR2] = (inputSampleR * fixC[fix_a2]) - (outSample * fixC[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleEL = (iirSampleEL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleEL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothEL + inputSampleL);
		smoothEL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleFL = (iirSampleFL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleFL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothFL + inputSampleL);
		smoothFL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleER = (iirSampleER * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleER*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothER + inputSampleR);
		smoothER = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleFR = (iirSampleFR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleFR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothFR + inputSampleR);
		smoothFR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixD[fix_a0]) + fixD[fix_sL1];
		fixD[fix_sL1] = (inputSampleL * fixD[fix_a1]) - (outSample * fixD[fix_b1]) + fixD[fix_sL2];
		fixD[fix_sL2] = (inputSampleL * fixD[fix_a2]) - (outSample * fixD[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixD[fix_a0]) + fixD[fix_sR1];
		fixD[fix_sR1] = (inputSampleR * fixD[fix_a1]) - (outSample * fixD[fix_b1]) + fixD[fix_sR2];
		fixD[fix_sR2] = (inputSampleR * fixD[fix_a2]) - (outSample * fixD[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleGL = (iirSampleGL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleGL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothGL + inputSampleL);
		smoothGL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleHL = (iirSampleHL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleHL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothHL + inputSampleL);
		smoothHL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleGR = (iirSampleGR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleGR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothGR + inputSampleR);
		smoothGR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleHR = (iirSampleHR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleHR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothHR + inputSampleR);
		smoothHR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixE[fix_a0]) + fixE[fix_sL1];
		fixE[fix_sL1] = (inputSampleL * fixE[fix_a1]) - (outSample * fixE[fix_b1]) + fixE[fix_sL2];
		fixE[fix_sL2] = (inputSampleL * fixE[fix_a2]) - (outSample * fixE[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixE[fix_a0]) + fixE[fix_sR1];
		fixE[fix_sR1] = (inputSampleR * fixE[fix_a1]) - (outSample * fixE[fix_b1]) + fixE[fix_sR2];
		fixE[fix_sR2] = (inputSampleR * fixE[fix_a2]) - (outSample * fixE[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleIL = (iirSampleIL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleIL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothIL + inputSampleL);
		smoothIL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleJL = (iirSampleJL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleJL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothJL + inputSampleL);
		smoothJL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleIR = (iirSampleIR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleIR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothIR + inputSampleR);
		smoothIR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleJR = (iirSampleJR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleJR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothJR + inputSampleR);
		smoothJR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixF[fix_a0]) + fixF[fix_sL1];
		fixF[fix_sL1] = (inputSampleL * fixF[fix_a1]) - (outSample * fixF[fix_b1]) + fixF[fix_sL2];
		fixF[fix_sL2] = (inputSampleL * fixF[fix_a2]) - (outSample * fixF[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixF[fix_a0]) + fixF[fix_sR1];
		fixF[fix_sR1] = (inputSampleR * fixF[fix_a1]) - (outSample * fixF[fix_b1]) + fixF[fix_sR2];
		fixF[fix_sR2] = (inputSampleR * fixF[fix_a2]) - (outSample * fixF[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleKL = (iirSampleKL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleKL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothKL + inputSampleL);
		smoothKL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleLL = (iirSampleLL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleLL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothLL + inputSampleL);
		smoothLL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleKR = (iirSampleKR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleKR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothKR + inputSampleR);
		smoothKR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleLR = (iirSampleLR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleLR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothLR + inputSampleR);
		smoothLR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		iirLowpassL = (iirLowpassL * (1.0 - toneEQ)) + (inputSampleL * toneEQ);
		inputSampleL = iirLowpassL;
		//lowpass. The only one of this type.
		iirLowpassR = (iirLowpassR * (1.0 - toneEQ)) + (inputSampleR * toneEQ);
		inputSampleR = iirLowpassR;
		//lowpass. The only one of this type.
		
		iirSpkAL = (iirSpkAL * (1.0 -  BEQ)) + (inputSampleL * BEQ);
		//extra lowpass for 4*12" speakers
		iirSpkAR = (iirSpkAR * (1.0 -  BEQ)) + (inputSampleR * BEQ);
		//extra lowpass for 4*12" speakers
		
		if (count < 0 || count > 128) {count = 128;}
		double resultBL = 0.0;
		double resultBR = 0.0;
		if (flip)
		{
			OddL[count+128] = OddL[count] = iirSpkAL;
			resultBL = (OddL[count+down] + OddL[count+side] + OddL[count+diagonal]);
			OddR[count+128] = OddR[count] = iirSpkAR;
			resultBR = (OddR[count+down] + OddR[count+side] + OddR[count+diagonal]);
		}
		else
		{
			EvenL[count+128] = EvenL[count] = iirSpkAL;
			resultBL = (EvenL[count+down] + EvenL[count+side] + EvenL[count+diagonal]);
			EvenR[count+128] = EvenR[count] = iirSpkAR;
			resultBR = (EvenR[count+down] + EvenR[count+side] + EvenR[count+diagonal]);
		}
		count--;
		iirSpkBL = (iirSpkBL * (1.0 - BEQ)) + (resultBL * BEQ);
		inputSampleL += (iirSpkBL * bleed);
		//extra lowpass for 4*12" speakers
		iirSpkBR = (iirSpkBR * (1.0 - BEQ)) + (resultBR * BEQ);
		inputSampleR += (iirSpkBR * bleed);
		//extra lowpass for 4*12" speakers
		
		bridgerectifier = fabs(inputSampleL*outputlevel);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		
		bridgerectifier = fabs(inputSampleR*outputlevel);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		
		iirSubL = (iirSubL * (1.0 - BEQ)) + (inputSampleL * BEQ);
		inputSampleL += (iirSubL * bassfill * outputlevel);
		
		iirSubR = (iirSubR * (1.0 - BEQ)) + (inputSampleR * BEQ);
		inputSampleR += (iirSubR * bassfill * outputlevel);
		
		double randy = ((double(fpdL)/UINT32_MAX)*0.053);
		inputSampleL = ((inputSampleL*(1.0-randy))+(storeSampleL*randy))*outputlevel;
		storeSampleL = inputSampleL;
		
		randy = ((double(fpdR)/UINT32_MAX)*0.053);
		inputSampleR = ((inputSampleR*(1.0-randy))+(storeSampleR*randy))*outputlevel;
		storeSampleR = inputSampleR;
		
		flip = !flip;
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		//amp
		
		cycle++;
		if (cycle == cycleEnd) {
			double temp = (inputSampleL + smoothCabAL)/3.0;
			smoothCabAL = inputSampleL;
			inputSampleL = temp;
			
			bL[84] = bL[83]; bL[83] = bL[82]; bL[82] = bL[81]; bL[81] = bL[80]; bL[80] = bL[79]; 
			bL[79] = bL[78]; bL[78] = bL[77]; bL[77] = bL[76]; bL[76] = bL[75]; bL[75] = bL[74]; bL[74] = bL[73]; bL[73] = bL[72]; bL[72] = bL[71]; 
			bL[71] = bL[70]; bL[70] = bL[69]; bL[69] = bL[68]; bL[68] = bL[67]; bL[67] = bL[66]; bL[66] = bL[65]; bL[65] = bL[64]; bL[64] = bL[63]; 
			bL[63] = bL[62]; bL[62] = bL[61]; bL[61] = bL[60]; bL[60] = bL[59]; bL[59] = bL[58]; bL[58] = bL[57]; bL[57] = bL[56]; bL[56] = bL[55]; 
			bL[55] = bL[54]; bL[54] = bL[53]; bL[53] = bL[52]; bL[52] = bL[51]; bL[51] = bL[50]; bL[50] = bL[49]; bL[49] = bL[48]; bL[48] = bL[47]; 
			bL[47] = bL[46]; bL[46] = bL[45]; bL[45] = bL[44]; bL[44] = bL[43]; bL[43] = bL[42]; bL[42] = bL[41]; bL[41] = bL[40]; bL[40] = bL[39]; 
			bL[39] = bL[38]; bL[38] = bL[37]; bL[37] = bL[36]; bL[36] = bL[35]; bL[35] = bL[34]; bL[34] = bL[33]; bL[33] = bL[32]; bL[32] = bL[31]; 
			bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28]; bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; 
			bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; 
			bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7]; 
			bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL;
			inputSampleL += (bL[1] * (1.31698250313308396  - (0.08140616497621633*fabs(bL[1]))));
			inputSampleL += (bL[2] * (1.47229016949915326  - (0.27680278993637253*fabs(bL[2]))));
			inputSampleL += (bL[3] * (1.30410109086044956  - (0.35629113432046489*fabs(bL[3]))));
			inputSampleL += (bL[4] * (0.81766210474551260  - (0.26808782337659753*fabs(bL[4]))));
			inputSampleL += (bL[5] * (0.19868872545506663  - (0.11105517193919669*fabs(bL[5]))));
			inputSampleL -= (bL[6] * (0.39115909132567039  - (0.12630622002682679*fabs(bL[6]))));
			inputSampleL -= (bL[7] * (0.76881891559343574  - (0.40879849500403143*fabs(bL[7]))));
			inputSampleL -= (bL[8] * (0.87146861782680340  - (0.59529560488000599*fabs(bL[8]))));
			inputSampleL -= (bL[9] * (0.79504575932563670  - (0.60877047551611796*fabs(bL[9]))));
			inputSampleL -= (bL[10] * (0.61653017622406314  - (0.47662851438557335*fabs(bL[10]))));
			inputSampleL -= (bL[11] * (0.40718195794382067  - (0.24955839378539713*fabs(bL[11]))));
			inputSampleL -= (bL[12] * (0.31794900040616203  - (0.04169792259600613*fabs(bL[12]))));
			inputSampleL -= (bL[13] * (0.41075032540217843  + (0.00368483996076280*fabs(bL[13]))));
			inputSampleL -= (bL[14] * (0.56901352922170667  - (0.11027360805893105*fabs(bL[14]))));
			inputSampleL -= (bL[15] * (0.62443222391889264  - (0.22198075154245228*fabs(bL[15]))));
			inputSampleL -= (bL[16] * (0.53462856723129204  - (0.22933544545324852*fabs(bL[16]))));
			inputSampleL -= (bL[17] * (0.34441703361995046  - (0.12956809502269492*fabs(bL[17]))));
			inputSampleL -= (bL[18] * (0.13947052337867882  + (0.00339775055962799*fabs(bL[18]))));
			inputSampleL += (bL[19] * (0.03771252648928484  - (0.10863931549251718*fabs(bL[19]))));
			inputSampleL += (bL[20] * (0.18280210770271693  - (0.17413646599296417*fabs(bL[20]))));
			inputSampleL += (bL[21] * (0.24621986701761467  - (0.14547053270435095*fabs(bL[21]))));
			inputSampleL += (bL[22] * (0.22347075142737360  - (0.02493869490104031*fabs(bL[22]))));
			inputSampleL += (bL[23] * (0.14346348482123716  + (0.11284054747963246*fabs(bL[23]))));
			inputSampleL += (bL[24] * (0.00834364862916028  + (0.24284684053733926*fabs(bL[24]))));
			inputSampleL -= (bL[25] * (0.11559740296078347  - (0.32623054435304538*fabs(bL[25]))));
			inputSampleL -= (bL[26] * (0.18067604561283060  - (0.32311481551122478*fabs(bL[26]))));
			inputSampleL -= (bL[27] * (0.22927997789035612  - (0.26991539052832925*fabs(bL[27]))));
			inputSampleL -= (bL[28] * (0.28487666578669446  - (0.22437227250279349*fabs(bL[28]))));
			inputSampleL -= (bL[29] * (0.31992973037153838  - (0.15289876100963865*fabs(bL[29]))));
			inputSampleL -= (bL[30] * (0.35174606303520733  - (0.05656293023086628*fabs(bL[30]))));
			inputSampleL -= (bL[31] * (0.36894898011375254  + (0.04333925421463558*fabs(bL[31]))));
			inputSampleL -= (bL[32] * (0.32567576055307507  + (0.14594589410921388*fabs(bL[32]))));
			inputSampleL -= (bL[33] * (0.27440135050585784  + (0.15529667398122521*fabs(bL[33]))));
			inputSampleL -= (bL[34] * (0.21998973785078091  + (0.05083553737157104*fabs(bL[34]))));
			inputSampleL -= (bL[35] * (0.10323624876862457  - (0.04651829594199963*fabs(bL[35]))));
			inputSampleL += (bL[36] * (0.02091603687851074  + (0.12000046818439322*fabs(bL[36]))));
			inputSampleL += (bL[37] * (0.11344930914138468  + (0.17697142512225839*fabs(bL[37]))));
			inputSampleL += (bL[38] * (0.22766779627643968  + (0.13645102964003858*fabs(bL[38]))));
			inputSampleL += (bL[39] * (0.38378309953638229  - (0.01997653307333791*fabs(bL[39]))));
			inputSampleL += (bL[40] * (0.52789400804568076  - (0.21409137428422448*fabs(bL[40]))));
			inputSampleL += (bL[41] * (0.55444630296938280  - (0.32331980931576626*fabs(bL[41]))));
			inputSampleL += (bL[42] * (0.42333237669264601  - (0.26855847463044280*fabs(bL[42]))));
			inputSampleL += (bL[43] * (0.21942831522035078  - (0.12051365248820624*fabs(bL[43]))));
			inputSampleL -= (bL[44] * (0.00584169427830633  - (0.03706970171280329*fabs(bL[44]))));
			inputSampleL -= (bL[45] * (0.24279799124660351  - (0.17296440491477982*fabs(bL[45]))));
			inputSampleL -= (bL[46] * (0.40173760787507085  - (0.21717989835163351*fabs(bL[46]))));
			inputSampleL -= (bL[47] * (0.43930035724188155  - (0.16425928481378199*fabs(bL[47]))));
			inputSampleL -= (bL[48] * (0.41067765934041811  - (0.10390115786636855*fabs(bL[48]))));
			inputSampleL -= (bL[49] * (0.34409235547165967  - (0.07268159377411920*fabs(bL[49]))));
			inputSampleL -= (bL[50] * (0.26542883122568151  - (0.05483457497365785*fabs(bL[50]))));
			inputSampleL -= (bL[51] * (0.22024754776138800  - (0.06484897950087598*fabs(bL[51]))));
			inputSampleL -= (bL[52] * (0.20394367993632415  - (0.08746309731952180*fabs(bL[52]))));
			inputSampleL -= (bL[53] * (0.17565242431124092  - (0.07611309538078760*fabs(bL[53]))));
			inputSampleL -= (bL[54] * (0.10116623231246825  - (0.00642818706295112*fabs(bL[54]))));
			inputSampleL -= (bL[55] * (0.00782648272053632  + (0.08004141267685004*fabs(bL[55]))));
			inputSampleL += (bL[56] * (0.05059046006747323  - (0.12436676387548490*fabs(bL[56]))));
			inputSampleL += (bL[57] * (0.06241531553254467  - (0.11530779547021434*fabs(bL[57]))));
			inputSampleL += (bL[58] * (0.04952694587101836  - (0.08340945324333944*fabs(bL[58]))));
			inputSampleL += (bL[59] * (0.00843873294401687  - (0.03279659052562903*fabs(bL[59]))));
			inputSampleL -= (bL[60] * (0.05161338949440241  - (0.03428181149163798*fabs(bL[60]))));
			inputSampleL -= (bL[61] * (0.08165520146902012  - (0.08196746092283110*fabs(bL[61]))));
			inputSampleL -= (bL[62] * (0.06639532849935320  - (0.09797462781896329*fabs(bL[62]))));
			inputSampleL -= (bL[63] * (0.02953430910661621  - (0.09175612938515763*fabs(bL[63]))));
			inputSampleL += (bL[64] * (0.00741058547442938  + (0.05442091048731967*fabs(bL[64]))));
			inputSampleL += (bL[65] * (0.01832866125391727  + (0.00306243693643687*fabs(bL[65]))));
			inputSampleL += (bL[66] * (0.00526964230373573  - (0.04364102661136410*fabs(bL[66]))));
			inputSampleL -= (bL[67] * (0.00300984373848200  + (0.09742737841278880*fabs(bL[67]))));
			inputSampleL -= (bL[68] * (0.00413616769576694  + (0.14380661694523073*fabs(bL[68]))));
			inputSampleL -= (bL[69] * (0.00588769034931419  + (0.16012843578892538*fabs(bL[69]))));
			inputSampleL -= (bL[70] * (0.00688588239450581  + (0.14074464279305798*fabs(bL[70]))));
			inputSampleL -= (bL[71] * (0.02277307992926315  + (0.07914752191801366*fabs(bL[71]))));
			inputSampleL -= (bL[72] * (0.04627166091180877  - (0.00192787268067208*fabs(bL[72]))));
			inputSampleL -= (bL[73] * (0.05562045897455786  - (0.05932868727665747*fabs(bL[73]))));
			inputSampleL -= (bL[74] * (0.05134243784922165  - (0.08245334798868090*fabs(bL[74]))));
			inputSampleL -= (bL[75] * (0.04719409472239919  - (0.07498680629253825*fabs(bL[75]))));
			inputSampleL -= (bL[76] * (0.05889738914266415  - (0.06116127018043697*fabs(bL[76]))));
			inputSampleL -= (bL[77] * (0.09428363535111127  - (0.06535868867863834*fabs(bL[77]))));
			inputSampleL -= (bL[78] * (0.15181756953225126  - (0.08982979655234427*fabs(bL[78]))));
			inputSampleL -= (bL[79] * (0.20878969456036670  - (0.10761070891499538*fabs(bL[79]))));
			inputSampleL -= (bL[80] * (0.22647885581813790  - (0.08462542510349125*fabs(bL[80]))));
			inputSampleL -= (bL[81] * (0.19723482443646323  - (0.02665160920736287*fabs(bL[81]))));
			inputSampleL -= (bL[82] * (0.16441643451155163  + (0.02314691954338197*fabs(bL[82]))));
			inputSampleL -= (bL[83] * (0.15201914054931515  + (0.04424903493886839*fabs(bL[83]))));
			inputSampleL -= (bL[84] * (0.15454370641307855  + (0.04223203797913008*fabs(bL[84]))));
						
			temp = (inputSampleL + smoothCabBL)/3.0;
			smoothCabBL = inputSampleL;
			inputSampleL = temp/4.0;
			
			randy = ((double(fpdL)/UINT32_MAX)*0.057);
			drySampleL = ((((inputSampleL*(1-randy))+(lastCabSampleL*randy))*wet)+(drySampleL*(1.0-wet)))*outputlevel;
			lastCabSampleL = inputSampleL;
			inputSampleL = drySampleL; //cab L
			
			temp = (inputSampleR + smoothCabAR)/3.0;
			smoothCabAR = inputSampleR;
			inputSampleR = temp;
			
			bR[84] = bR[83]; bR[83] = bR[82]; bR[82] = bR[81]; bR[81] = bR[80]; bR[80] = bR[79]; 
			bR[79] = bR[78]; bR[78] = bR[77]; bR[77] = bR[76]; bR[76] = bR[75]; bR[75] = bR[74]; bR[74] = bR[73]; bR[73] = bR[72]; bR[72] = bR[71]; 
			bR[71] = bR[70]; bR[70] = bR[69]; bR[69] = bR[68]; bR[68] = bR[67]; bR[67] = bR[66]; bR[66] = bR[65]; bR[65] = bR[64]; bR[64] = bR[63]; 
			bR[63] = bR[62]; bR[62] = bR[61]; bR[61] = bR[60]; bR[60] = bR[59]; bR[59] = bR[58]; bR[58] = bR[57]; bR[57] = bR[56]; bR[56] = bR[55]; 
			bR[55] = bR[54]; bR[54] = bR[53]; bR[53] = bR[52]; bR[52] = bR[51]; bR[51] = bR[50]; bR[50] = bR[49]; bR[49] = bR[48]; bR[48] = bR[47]; 
			bR[47] = bR[46]; bR[46] = bR[45]; bR[45] = bR[44]; bR[44] = bR[43]; bR[43] = bR[42]; bR[42] = bR[41]; bR[41] = bR[40]; bR[40] = bR[39]; 
			bR[39] = bR[38]; bR[38] = bR[37]; bR[37] = bR[36]; bR[36] = bR[35]; bR[35] = bR[34]; bR[34] = bR[33]; bR[33] = bR[32]; bR[32] = bR[31]; 
			bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28]; bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; 
			bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; 
			bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7]; 
			bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR;
			inputSampleR += (bR[1] * (1.31698250313308396  - (0.08140616497621633*fabs(bR[1]))));
			inputSampleR += (bR[2] * (1.47229016949915326  - (0.27680278993637253*fabs(bR[2]))));
			inputSampleR += (bR[3] * (1.30410109086044956  - (0.35629113432046489*fabs(bR[3]))));
			inputSampleR += (bR[4] * (0.81766210474551260  - (0.26808782337659753*fabs(bR[4]))));
			inputSampleR += (bR[5] * (0.19868872545506663  - (0.11105517193919669*fabs(bR[5]))));
			inputSampleR -= (bR[6] * (0.39115909132567039  - (0.12630622002682679*fabs(bR[6]))));
			inputSampleR -= (bR[7] * (0.76881891559343574  - (0.40879849500403143*fabs(bR[7]))));
			inputSampleR -= (bR[8] * (0.87146861782680340  - (0.59529560488000599*fabs(bR[8]))));
			inputSampleR -= (bR[9] * (0.79504575932563670  - (0.60877047551611796*fabs(bR[9]))));
			inputSampleR -= (bR[10] * (0.61653017622406314  - (0.47662851438557335*fabs(bR[10]))));
			inputSampleR -= (bR[11] * (0.40718195794382067  - (0.24955839378539713*fabs(bR[11]))));
			inputSampleR -= (bR[12] * (0.31794900040616203  - (0.04169792259600613*fabs(bR[12]))));
			inputSampleR -= (bR[13] * (0.41075032540217843  + (0.00368483996076280*fabs(bR[13]))));
			inputSampleR -= (bR[14] * (0.56901352922170667  - (0.11027360805893105*fabs(bR[14]))));
			inputSampleR -= (bR[15] * (0.62443222391889264  - (0.22198075154245228*fabs(bR[15]))));
			inputSampleR -= (bR[16] * (0.53462856723129204  - (0.22933544545324852*fabs(bR[16]))));
			inputSampleR -= (bR[17] * (0.34441703361995046  - (0.12956809502269492*fabs(bR[17]))));
			inputSampleR -= (bR[18] * (0.13947052337867882  + (0.00339775055962799*fabs(bR[18]))));
			inputSampleR += (bR[19] * (0.03771252648928484  - (0.10863931549251718*fabs(bR[19]))));
			inputSampleR += (bR[20] * (0.18280210770271693  - (0.17413646599296417*fabs(bR[20]))));
			inputSampleR += (bR[21] * (0.24621986701761467  - (0.14547053270435095*fabs(bR[21]))));
			inputSampleR += (bR[22] * (0.22347075142737360  - (0.02493869490104031*fabs(bR[22]))));
			inputSampleR += (bR[23] * (0.14346348482123716  + (0.11284054747963246*fabs(bR[23]))));
			inputSampleR += (bR[24] * (0.00834364862916028  + (0.24284684053733926*fabs(bR[24]))));
			inputSampleR -= (bR[25] * (0.11559740296078347  - (0.32623054435304538*fabs(bR[25]))));
			inputSampleR -= (bR[26] * (0.18067604561283060  - (0.32311481551122478*fabs(bR[26]))));
			inputSampleR -= (bR[27] * (0.22927997789035612  - (0.26991539052832925*fabs(bR[27]))));
			inputSampleR -= (bR[28] * (0.28487666578669446  - (0.22437227250279349*fabs(bR[28]))));
			inputSampleR -= (bR[29] * (0.31992973037153838  - (0.15289876100963865*fabs(bR[29]))));
			inputSampleR -= (bR[30] * (0.35174606303520733  - (0.05656293023086628*fabs(bR[30]))));
			inputSampleR -= (bR[31] * (0.36894898011375254  + (0.04333925421463558*fabs(bR[31]))));
			inputSampleR -= (bR[32] * (0.32567576055307507  + (0.14594589410921388*fabs(bR[32]))));
			inputSampleR -= (bR[33] * (0.27440135050585784  + (0.15529667398122521*fabs(bR[33]))));
			inputSampleR -= (bR[34] * (0.21998973785078091  + (0.05083553737157104*fabs(bR[34]))));
			inputSampleR -= (bR[35] * (0.10323624876862457  - (0.04651829594199963*fabs(bR[35]))));
			inputSampleR += (bR[36] * (0.02091603687851074  + (0.12000046818439322*fabs(bR[36]))));
			inputSampleR += (bR[37] * (0.11344930914138468  + (0.17697142512225839*fabs(bR[37]))));
			inputSampleR += (bR[38] * (0.22766779627643968  + (0.13645102964003858*fabs(bR[38]))));
			inputSampleR += (bR[39] * (0.38378309953638229  - (0.01997653307333791*fabs(bR[39]))));
			inputSampleR += (bR[40] * (0.52789400804568076  - (0.21409137428422448*fabs(bR[40]))));
			inputSampleR += (bR[41] * (0.55444630296938280  - (0.32331980931576626*fabs(bR[41]))));
			inputSampleR += (bR[42] * (0.42333237669264601  - (0.26855847463044280*fabs(bR[42]))));
			inputSampleR += (bR[43] * (0.21942831522035078  - (0.12051365248820624*fabs(bR[43]))));
			inputSampleR -= (bR[44] * (0.00584169427830633  - (0.03706970171280329*fabs(bR[44]))));
			inputSampleR -= (bR[45] * (0.24279799124660351  - (0.17296440491477982*fabs(bR[45]))));
			inputSampleR -= (bR[46] * (0.40173760787507085  - (0.21717989835163351*fabs(bR[46]))));
			inputSampleR -= (bR[47] * (0.43930035724188155  - (0.16425928481378199*fabs(bR[47]))));
			inputSampleR -= (bR[48] * (0.41067765934041811  - (0.10390115786636855*fabs(bR[48]))));
			inputSampleR -= (bR[49] * (0.34409235547165967  - (0.07268159377411920*fabs(bR[49]))));
			inputSampleR -= (bR[50] * (0.26542883122568151  - (0.05483457497365785*fabs(bR[50]))));
			inputSampleR -= (bR[51] * (0.22024754776138800  - (0.06484897950087598*fabs(bR[51]))));
			inputSampleR -= (bR[52] * (0.20394367993632415  - (0.08746309731952180*fabs(bR[52]))));
			inputSampleR -= (bR[53] * (0.17565242431124092  - (0.07611309538078760*fabs(bR[53]))));
			inputSampleR -= (bR[54] * (0.10116623231246825  - (0.00642818706295112*fabs(bR[54]))));
			inputSampleR -= (bR[55] * (0.00782648272053632  + (0.08004141267685004*fabs(bR[55]))));
			inputSampleR += (bR[56] * (0.05059046006747323  - (0.12436676387548490*fabs(bR[56]))));
			inputSampleR += (bR[57] * (0.06241531553254467  - (0.11530779547021434*fabs(bR[57]))));
			inputSampleR += (bR[58] * (0.04952694587101836  - (0.08340945324333944*fabs(bR[58]))));
			inputSampleR += (bR[59] * (0.00843873294401687  - (0.03279659052562903*fabs(bR[59]))));
			inputSampleR -= (bR[60] * (0.05161338949440241  - (0.03428181149163798*fabs(bR[60]))));
			inputSampleR -= (bR[61] * (0.08165520146902012  - (0.08196746092283110*fabs(bR[61]))));
			inputSampleR -= (bR[62] * (0.06639532849935320  - (0.09797462781896329*fabs(bR[62]))));
			inputSampleR -= (bR[63] * (0.02953430910661621  - (0.09175612938515763*fabs(bR[63]))));
			inputSampleR += (bR[64] * (0.00741058547442938  + (0.05442091048731967*fabs(bR[64]))));
			inputSampleR += (bR[65] * (0.01832866125391727  + (0.00306243693643687*fabs(bR[65]))));
			inputSampleR += (bR[66] * (0.00526964230373573  - (0.04364102661136410*fabs(bR[66]))));
			inputSampleR -= (bR[67] * (0.00300984373848200  + (0.09742737841278880*fabs(bR[67]))));
			inputSampleR -= (bR[68] * (0.00413616769576694  + (0.14380661694523073*fabs(bR[68]))));
			inputSampleR -= (bR[69] * (0.00588769034931419  + (0.16012843578892538*fabs(bR[69]))));
			inputSampleR -= (bR[70] * (0.00688588239450581  + (0.14074464279305798*fabs(bR[70]))));
			inputSampleR -= (bR[71] * (0.02277307992926315  + (0.07914752191801366*fabs(bR[71]))));
			inputSampleR -= (bR[72] * (0.04627166091180877  - (0.00192787268067208*fabs(bR[72]))));
			inputSampleR -= (bR[73] * (0.05562045897455786  - (0.05932868727665747*fabs(bR[73]))));
			inputSampleR -= (bR[74] * (0.05134243784922165  - (0.08245334798868090*fabs(bR[74]))));
			inputSampleR -= (bR[75] * (0.04719409472239919  - (0.07498680629253825*fabs(bR[75]))));
			inputSampleR -= (bR[76] * (0.05889738914266415  - (0.06116127018043697*fabs(bR[76]))));
			inputSampleR -= (bR[77] * (0.09428363535111127  - (0.06535868867863834*fabs(bR[77]))));
			inputSampleR -= (bR[78] * (0.15181756953225126  - (0.08982979655234427*fabs(bR[78]))));
			inputSampleR -= (bR[79] * (0.20878969456036670  - (0.10761070891499538*fabs(bR[79]))));
			inputSampleR -= (bR[80] * (0.22647885581813790  - (0.08462542510349125*fabs(bR[80]))));
			inputSampleR -= (bR[81] * (0.19723482443646323  - (0.02665160920736287*fabs(bR[81]))));
			inputSampleR -= (bR[82] * (0.16441643451155163  + (0.02314691954338197*fabs(bR[82]))));
			inputSampleR -= (bR[83] * (0.15201914054931515  + (0.04424903493886839*fabs(bR[83]))));
			inputSampleR -= (bR[84] * (0.15454370641307855  + (0.04223203797913008*fabs(bR[84]))));
			
			temp = (inputSampleR + smoothCabBR)/3.0;
			smoothCabBR = inputSampleR;
			inputSampleR = temp/4.0;
			
			randy = ((double(fpdR)/UINT32_MAX)*0.057);
			drySampleR = ((((inputSampleR*(1.0-randy))+(lastCabSampleR*randy))*wet)+(drySampleR*(1.0-wet)))*outputlevel;
			lastCabSampleR = inputSampleR;
			inputSampleR = drySampleR; //cab
			
			if (cycleEnd == 4) {
				lastRefL[0] = lastRefL[4]; //start from previous last
				lastRefL[2] = (lastRefL[0] + inputSampleL)/2; //half
				lastRefL[1] = (lastRefL[0] + lastRefL[2])/2; //one quarter
				lastRefL[3] = (lastRefL[2] + inputSampleL)/2; //three quarters
				lastRefL[4] = inputSampleL; //full
				lastRefR[0] = lastRefR[4]; //start from previous last
				lastRefR[2] = (lastRefR[0] + inputSampleR)/2; //half
				lastRefR[1] = (lastRefR[0] + lastRefR[2])/2; //one quarter
				lastRefR[3] = (lastRefR[2] + inputSampleR)/2; //three quarters
				lastRefR[4] = inputSampleR; //full
			}
			if (cycleEnd == 3) {
				lastRefL[0] = lastRefL[3]; //start from previous last
				lastRefL[2] = (lastRefL[0]+lastRefL[0]+inputSampleL)/3; //third
				lastRefL[1] = (lastRefL[0]+inputSampleL+inputSampleL)/3; //two thirds
				lastRefL[3] = inputSampleL; //full
				lastRefR[0] = lastRefR[3]; //start from previous last
				lastRefR[2] = (lastRefR[0]+lastRefR[0]+inputSampleR)/3; //third
				lastRefR[1] = (lastRefR[0]+inputSampleR+inputSampleR)/3; //two thirds
				lastRefR[3] = inputSampleR; //full
			}
			if (cycleEnd == 2) {
				lastRefL[0] = lastRefL[2]; //start from previous last
				lastRefL[1] = (lastRefL[0] + inputSampleL)/2; //half
				lastRefL[2] = inputSampleL; //full
				lastRefR[0] = lastRefR[2]; //start from previous last
				lastRefR[1] = (lastRefR[0] + inputSampleR)/2; //half
				lastRefR[2] = inputSampleR; //full
			}
			if (cycleEnd == 1) {
				lastRefL[0] = inputSampleL;
				lastRefR[0] = inputSampleR;
			}
			cycle = 0; //reset
			inputSampleL = lastRefL[cycle];
			inputSampleR = lastRefR[cycle];
		} else {
			inputSampleL = lastRefL[cycle];
			inputSampleR = lastRefR[cycle];
			//we are going through our references now
		}
		switch (cycleEnd) //multi-pole average using lastRef[] variables
		{
			case 4:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[7])*0.5;
				lastRefL[7] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[7])*0.5;
				lastRefR[7] = lastRefR[8]; //continue, do not break
			case 3:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[6])*0.5;
				lastRefL[6] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[6])*0.5;
				lastRefR[6] = lastRefR[8]; //continue, do not break
			case 2:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[5])*0.5;
				lastRefL[5] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[5])*0.5;
				lastRefR[5] = lastRefR[8]; //continue, do not break
			case 1:
				break; //no further averaging
		}
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void FireAmp::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double bassfill = A;
	double outputlevel = C;
	double wet = D;
	
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	int cycleEnd = floor(overallscale);
	if (cycleEnd < 1) cycleEnd = 1;
	if (cycleEnd > 4) cycleEnd = 4;
	//this is going to be 2 for 88.1 or 96k, 3 for silly people, 4 for 176 or 192k
	if (cycle > cycleEnd-1) cycle = cycleEnd-1; //sanity check		
	
	double startlevel = bassfill;
	double samplerate = getSampleRate();
	double basstrim = bassfill / 16.0;
	double toneEQ = (B / samplerate)*22050.0;
	double EQ = (basstrim / samplerate)*22050.0;
	double bleed = outputlevel/16.0;
	double bassfactor = 1.0-(basstrim*basstrim);
	double BEQ = (bleed / samplerate)*22050.0;
	int diagonal = (int)(0.000861678*samplerate);
	if (diagonal > 127) diagonal = 127;
	int side = (int)(diagonal/1.4142135623730951);
	int down = (side + diagonal)/2;
	//now we've got down, side and diagonal as offsets and we also use three successive samples upfront
	
	double cutoff = (15000.0+(B*10000.0)) / getSampleRate();
	if (cutoff > 0.49) cutoff = 0.49; //don't crash if run at 44.1k
	if (cutoff < 0.001) cutoff = 0.001; //or if cutoff's too low
	
	fixF[fix_freq] = fixE[fix_freq] = fixD[fix_freq] = fixC[fix_freq] = fixB[fix_freq] = fixA[fix_freq] = cutoff;
	
    fixA[fix_reso] = 4.46570214;
	fixB[fix_reso] = 1.51387132;
	fixC[fix_reso] = 0.93979296;
	fixD[fix_reso] = 0.70710678;
	fixE[fix_reso] = 0.52972649;
	fixF[fix_reso] = 0.50316379;
	
	double K = tan(M_PI * fixA[fix_freq]); //lowpass
	double norm = 1.0 / (1.0 + K / fixA[fix_reso] + K * K);
	fixA[fix_a0] = K * K * norm;
	fixA[fix_a1] = 2.0 * fixA[fix_a0];
	fixA[fix_a2] = fixA[fix_a0];
	fixA[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixA[fix_b2] = (1.0 - K / fixA[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixB[fix_freq]);
	norm = 1.0 / (1.0 + K / fixB[fix_reso] + K * K);
	fixB[fix_a0] = K * K * norm;
	fixB[fix_a1] = 2.0 * fixB[fix_a0];
	fixB[fix_a2] = fixB[fix_a0];
	fixB[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixB[fix_b2] = (1.0 - K / fixB[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixC[fix_freq]);
	norm = 1.0 / (1.0 + K / fixC[fix_reso] + K * K);
	fixC[fix_a0] = K * K * norm;
	fixC[fix_a1] = 2.0 * fixC[fix_a0];
	fixC[fix_a2] = fixC[fix_a0];
	fixC[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixC[fix_b2] = (1.0 - K / fixC[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixD[fix_freq]);
	norm = 1.0 / (1.0 + K / fixD[fix_reso] + K * K);
	fixD[fix_a0] = K * K * norm;
	fixD[fix_a1] = 2.0 * fixD[fix_a0];
	fixD[fix_a2] = fixD[fix_a0];
	fixD[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixD[fix_b2] = (1.0 - K / fixD[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixE[fix_freq]);
	norm = 1.0 / (1.0 + K / fixE[fix_reso] + K * K);
	fixE[fix_a0] = K * K * norm;
	fixE[fix_a1] = 2.0 * fixE[fix_a0];
	fixE[fix_a2] = fixE[fix_a0];
	fixE[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixE[fix_b2] = (1.0 - K / fixE[fix_reso] + K * K) * norm;
	
	K = tan(M_PI * fixF[fix_freq]);
	norm = 1.0 / (1.0 + K / fixF[fix_reso] + K * K);
	fixF[fix_a0] = K * K * norm;
	fixF[fix_a1] = 2.0 * fixF[fix_a0];
	fixF[fix_a2] = fixF[fix_a0];
	fixF[fix_b1] = 2.0 * (K * K - 1.0) * norm;
	fixF[fix_b2] = (1.0 - K / fixF[fix_reso] + K * K) * norm;
	
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		
		double outSample = (inputSampleL * fixA[fix_a0]) + fixA[fix_sL1];
		fixA[fix_sL1] = (inputSampleL * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sL2];
		fixA[fix_sL2] = (inputSampleL * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixA[fix_a0]) + fixA[fix_sR1];
		fixA[fix_sR1] = (inputSampleR * fixA[fix_a1]) - (outSample * fixA[fix_b1]) + fixA[fix_sR2];
		fixA[fix_sR2] = (inputSampleR * fixA[fix_a2]) - (outSample * fixA[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		double basscutL = 0.98;
		//we're going to be shifting this as the stages progress
		double inputlevelL = startlevel;
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleAL = (iirSampleAL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleAL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		double bridgerectifier = (smoothAL + inputSampleL);
		smoothAL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleBL = (iirSampleBL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleBL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothBL + inputSampleL);
		smoothBL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		double basscutR = 0.98;
		//we're going to be shifting this as the stages progress
		double inputlevelR = startlevel;
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleAR = (iirSampleAR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleAR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothAR + inputSampleR);
		smoothAR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleBR = (iirSampleBR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleBR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothBR + inputSampleR);
		smoothBR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		
		outSample = (inputSampleL * fixB[fix_a0]) + fixB[fix_sL1];
		fixB[fix_sL1] = (inputSampleL * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sL2];
		fixB[fix_sL2] = (inputSampleL * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixB[fix_a0]) + fixB[fix_sR1];
		fixB[fix_sR1] = (inputSampleR * fixB[fix_a1]) - (outSample * fixB[fix_b1]) + fixB[fix_sR2];
		fixB[fix_sR2] = (inputSampleR * fixB[fix_a2]) - (outSample * fixB[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleCL = (iirSampleCL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleCL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothCL + inputSampleL);
		smoothCL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleDL = (iirSampleDL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleDL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654) );
		//overdrive
		bridgerectifier = (smoothDL + inputSampleL);
		smoothDL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleCR = (iirSampleCR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleCR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothCR + inputSampleR);
		smoothCR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleDR = (iirSampleDR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleDR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654) );
		//overdrive
		bridgerectifier = (smoothDR + inputSampleR);
		smoothDR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixC[fix_a0]) + fixC[fix_sL1];
		fixC[fix_sL1] = (inputSampleL * fixC[fix_a1]) - (outSample * fixC[fix_b1]) + fixC[fix_sL2];
		fixC[fix_sL2] = (inputSampleL * fixC[fix_a2]) - (outSample * fixC[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixC[fix_a0]) + fixC[fix_sR1];
		fixC[fix_sR1] = (inputSampleR * fixC[fix_a1]) - (outSample * fixC[fix_b1]) + fixC[fix_sR2];
		fixC[fix_sR2] = (inputSampleR * fixC[fix_a2]) - (outSample * fixC[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleEL = (iirSampleEL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleEL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothEL + inputSampleL);
		smoothEL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleFL = (iirSampleFL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleFL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothFL + inputSampleL);
		smoothFL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleER = (iirSampleER * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleER*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothER + inputSampleR);
		smoothER = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleFR = (iirSampleFR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleFR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothFR + inputSampleR);
		smoothFR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixD[fix_a0]) + fixD[fix_sL1];
		fixD[fix_sL1] = (inputSampleL * fixD[fix_a1]) - (outSample * fixD[fix_b1]) + fixD[fix_sL2];
		fixD[fix_sL2] = (inputSampleL * fixD[fix_a2]) - (outSample * fixD[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixD[fix_a0]) + fixD[fix_sR1];
		fixD[fix_sR1] = (inputSampleR * fixD[fix_a1]) - (outSample * fixD[fix_b1]) + fixD[fix_sR2];
		fixD[fix_sR2] = (inputSampleR * fixD[fix_a2]) - (outSample * fixD[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleGL = (iirSampleGL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleGL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothGL + inputSampleL);
		smoothGL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleHL = (iirSampleHL * (1.0 - EQ)) + (inputSampleL * EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleHL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothHL + inputSampleL);
		smoothHL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleGR = (iirSampleGR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleGR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothGR + inputSampleR);
		smoothGR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleHR = (iirSampleHR * (1.0 - EQ)) + (inputSampleR * EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleHR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothHR + inputSampleR);
		smoothHR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixE[fix_a0]) + fixE[fix_sL1];
		fixE[fix_sL1] = (inputSampleL * fixE[fix_a1]) - (outSample * fixE[fix_b1]) + fixE[fix_sL2];
		fixE[fix_sL2] = (inputSampleL * fixE[fix_a2]) - (outSample * fixE[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixE[fix_a0]) + fixE[fix_sR1];
		fixE[fix_sR1] = (inputSampleR * fixE[fix_a1]) - (outSample * fixE[fix_b1]) + fixE[fix_sR2];
		fixE[fix_sR2] = (inputSampleR * fixE[fix_a2]) - (outSample * fixE[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleIL = (iirSampleIL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleIL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothIL + inputSampleL);
		smoothIL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleJL = (iirSampleJL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleJL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothJL + inputSampleL);
		smoothJL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleIR = (iirSampleIR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleIR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothIR + inputSampleR);
		smoothIR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleJR = (iirSampleJR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleJR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothJR + inputSampleR);
		smoothJR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		outSample = (inputSampleL * fixF[fix_a0]) + fixF[fix_sL1];
		fixF[fix_sL1] = (inputSampleL * fixF[fix_a1]) - (outSample * fixF[fix_b1]) + fixF[fix_sL2];
		fixF[fix_sL2] = (inputSampleL * fixF[fix_a2]) - (outSample * fixF[fix_b2]);
		inputSampleL = outSample; //fixed biquad filtering ultrasonics
		outSample = (inputSampleR * fixF[fix_a0]) + fixF[fix_sR1];
		fixF[fix_sR1] = (inputSampleR * fixF[fix_a1]) - (outSample * fixF[fix_b1]) + fixF[fix_sR2];
		fixF[fix_sR2] = (inputSampleR * fixF[fix_a2]) - (outSample * fixF[fix_b2]);
		inputSampleR = outSample; //fixed biquad filtering ultrasonics
		
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleKL = (iirSampleKL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleKL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothKL + inputSampleL);
		smoothKL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleL *= inputlevelL;
		inputlevelL = ((inputlevelL * 7.0)+1.0)/8.0;
		iirSampleLL = (iirSampleLL * (1.0 - EQ)) + (inputSampleL *  EQ);
		basscutL *= bassfactor;
		inputSampleL = inputSampleL - (iirSampleLL*basscutL);
		//highpass
		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * 0.654) * (fabs(inputSampleL) * 0.654));
		//overdrive
		bridgerectifier = (smoothLL + inputSampleL);
		smoothLL = inputSampleL;
		inputSampleL = bridgerectifier;
		//two-sample averaging lowpass
		
		
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleKR = (iirSampleKR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleKR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothKR + inputSampleR);
		smoothKR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		inputSampleR *= inputlevelR;
		inputlevelR = ((inputlevelR * 7.0)+1.0)/8.0;
		iirSampleLR = (iirSampleLR * (1.0 - EQ)) + (inputSampleR *  EQ);
		basscutR *= bassfactor;
		inputSampleR = inputSampleR - (iirSampleLR*basscutR);
		//highpass
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * 0.654) * (fabs(inputSampleR) * 0.654));
		//overdrive
		bridgerectifier = (smoothLR + inputSampleR);
		smoothLR = inputSampleR;
		inputSampleR = bridgerectifier;
		//two-sample averaging lowpass
		
		iirLowpassL = (iirLowpassL * (1.0 - toneEQ)) + (inputSampleL * toneEQ);
		inputSampleL = iirLowpassL;
		//lowpass. The only one of this type.
		iirLowpassR = (iirLowpassR * (1.0 - toneEQ)) + (inputSampleR * toneEQ);
		inputSampleR = iirLowpassR;
		//lowpass. The only one of this type.
		
		iirSpkAL = (iirSpkAL * (1.0 -  BEQ)) + (inputSampleL * BEQ);
		//extra lowpass for 4*12" speakers
		iirSpkAR = (iirSpkAR * (1.0 -  BEQ)) + (inputSampleR * BEQ);
		//extra lowpass for 4*12" speakers
		
		if (count < 0 || count > 128) {count = 128;}
		double resultBL = 0.0;
		double resultBR = 0.0;
		if (flip)
		{
			OddL[count+128] = OddL[count] = iirSpkAL;
			resultBL = (OddL[count+down] + OddL[count+side] + OddL[count+diagonal]);
			OddR[count+128] = OddR[count] = iirSpkAR;
			resultBR = (OddR[count+down] + OddR[count+side] + OddR[count+diagonal]);
		}
		else
		{
			EvenL[count+128] = EvenL[count] = iirSpkAL;
			resultBL = (EvenL[count+down] + EvenL[count+side] + EvenL[count+diagonal]);
			EvenR[count+128] = EvenR[count] = iirSpkAR;
			resultBR = (EvenR[count+down] + EvenR[count+side] + EvenR[count+diagonal]);
		}
		count--;
		iirSpkBL = (iirSpkBL * (1.0 - BEQ)) + (resultBL * BEQ);
		inputSampleL += (iirSpkBL * bleed);
		//extra lowpass for 4*12" speakers
		iirSpkBR = (iirSpkBR * (1.0 - BEQ)) + (resultBR * BEQ);
		inputSampleR += (iirSpkBR * bleed);
		//extra lowpass for 4*12" speakers
		
		bridgerectifier = fabs(inputSampleL*outputlevel);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		
		bridgerectifier = fabs(inputSampleR*outputlevel);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		
		iirSubL = (iirSubL * (1.0 - BEQ)) + (inputSampleL * BEQ);
		inputSampleL += (iirSubL * bassfill * outputlevel);
		
		iirSubR = (iirSubR * (1.0 - BEQ)) + (inputSampleR * BEQ);
		inputSampleR += (iirSubR * bassfill * outputlevel);
		
		double randy = ((double(fpdL)/UINT32_MAX)*0.053);
		inputSampleL = ((inputSampleL*(1.0-randy))+(storeSampleL*randy))*outputlevel;
		storeSampleL = inputSampleL;
		
		randy = ((double(fpdR)/UINT32_MAX)*0.053);
		inputSampleR = ((inputSampleR*(1.0-randy))+(storeSampleR*randy))*outputlevel;
		storeSampleR = inputSampleR;
		
		flip = !flip;
		
		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
			inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
		}
		//Dry/Wet control, defaults to the last slider
		//amp
		
		cycle++;
		if (cycle == cycleEnd) {
			double temp = (inputSampleL + smoothCabAL)/3.0;
			smoothCabAL = inputSampleL;
			inputSampleL = temp;
			
			bL[84] = bL[83]; bL[83] = bL[82]; bL[82] = bL[81]; bL[81] = bL[80]; bL[80] = bL[79]; 
			bL[79] = bL[78]; bL[78] = bL[77]; bL[77] = bL[76]; bL[76] = bL[75]; bL[75] = bL[74]; bL[74] = bL[73]; bL[73] = bL[72]; bL[72] = bL[71]; 
			bL[71] = bL[70]; bL[70] = bL[69]; bL[69] = bL[68]; bL[68] = bL[67]; bL[67] = bL[66]; bL[66] = bL[65]; bL[65] = bL[64]; bL[64] = bL[63]; 
			bL[63] = bL[62]; bL[62] = bL[61]; bL[61] = bL[60]; bL[60] = bL[59]; bL[59] = bL[58]; bL[58] = bL[57]; bL[57] = bL[56]; bL[56] = bL[55]; 
			bL[55] = bL[54]; bL[54] = bL[53]; bL[53] = bL[52]; bL[52] = bL[51]; bL[51] = bL[50]; bL[50] = bL[49]; bL[49] = bL[48]; bL[48] = bL[47]; 
			bL[47] = bL[46]; bL[46] = bL[45]; bL[45] = bL[44]; bL[44] = bL[43]; bL[43] = bL[42]; bL[42] = bL[41]; bL[41] = bL[40]; bL[40] = bL[39]; 
			bL[39] = bL[38]; bL[38] = bL[37]; bL[37] = bL[36]; bL[36] = bL[35]; bL[35] = bL[34]; bL[34] = bL[33]; bL[33] = bL[32]; bL[32] = bL[31]; 
			bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28]; bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; 
			bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; 
			bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7]; 
			bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL;
			inputSampleL += (bL[1] * (1.31698250313308396  - (0.08140616497621633*fabs(bL[1]))));
			inputSampleL += (bL[2] * (1.47229016949915326  - (0.27680278993637253*fabs(bL[2]))));
			inputSampleL += (bL[3] * (1.30410109086044956  - (0.35629113432046489*fabs(bL[3]))));
			inputSampleL += (bL[4] * (0.81766210474551260  - (0.26808782337659753*fabs(bL[4]))));
			inputSampleL += (bL[5] * (0.19868872545506663  - (0.11105517193919669*fabs(bL[5]))));
			inputSampleL -= (bL[6] * (0.39115909132567039  - (0.12630622002682679*fabs(bL[6]))));
			inputSampleL -= (bL[7] * (0.76881891559343574  - (0.40879849500403143*fabs(bL[7]))));
			inputSampleL -= (bL[8] * (0.87146861782680340  - (0.59529560488000599*fabs(bL[8]))));
			inputSampleL -= (bL[9] * (0.79504575932563670  - (0.60877047551611796*fabs(bL[9]))));
			inputSampleL -= (bL[10] * (0.61653017622406314  - (0.47662851438557335*fabs(bL[10]))));
			inputSampleL -= (bL[11] * (0.40718195794382067  - (0.24955839378539713*fabs(bL[11]))));
			inputSampleL -= (bL[12] * (0.31794900040616203  - (0.04169792259600613*fabs(bL[12]))));
			inputSampleL -= (bL[13] * (0.41075032540217843  + (0.00368483996076280*fabs(bL[13]))));
			inputSampleL -= (bL[14] * (0.56901352922170667  - (0.11027360805893105*fabs(bL[14]))));
			inputSampleL -= (bL[15] * (0.62443222391889264  - (0.22198075154245228*fabs(bL[15]))));
			inputSampleL -= (bL[16] * (0.53462856723129204  - (0.22933544545324852*fabs(bL[16]))));
			inputSampleL -= (bL[17] * (0.34441703361995046  - (0.12956809502269492*fabs(bL[17]))));
			inputSampleL -= (bL[18] * (0.13947052337867882  + (0.00339775055962799*fabs(bL[18]))));
			inputSampleL += (bL[19] * (0.03771252648928484  - (0.10863931549251718*fabs(bL[19]))));
			inputSampleL += (bL[20] * (0.18280210770271693  - (0.17413646599296417*fabs(bL[20]))));
			inputSampleL += (bL[21] * (0.24621986701761467  - (0.14547053270435095*fabs(bL[21]))));
			inputSampleL += (bL[22] * (0.22347075142737360  - (0.02493869490104031*fabs(bL[22]))));
			inputSampleL += (bL[23] * (0.14346348482123716  + (0.11284054747963246*fabs(bL[23]))));
			inputSampleL += (bL[24] * (0.00834364862916028  + (0.24284684053733926*fabs(bL[24]))));
			inputSampleL -= (bL[25] * (0.11559740296078347  - (0.32623054435304538*fabs(bL[25]))));
			inputSampleL -= (bL[26] * (0.18067604561283060  - (0.32311481551122478*fabs(bL[26]))));
			inputSampleL -= (bL[27] * (0.22927997789035612  - (0.26991539052832925*fabs(bL[27]))));
			inputSampleL -= (bL[28] * (0.28487666578669446  - (0.22437227250279349*fabs(bL[28]))));
			inputSampleL -= (bL[29] * (0.31992973037153838  - (0.15289876100963865*fabs(bL[29]))));
			inputSampleL -= (bL[30] * (0.35174606303520733  - (0.05656293023086628*fabs(bL[30]))));
			inputSampleL -= (bL[31] * (0.36894898011375254  + (0.04333925421463558*fabs(bL[31]))));
			inputSampleL -= (bL[32] * (0.32567576055307507  + (0.14594589410921388*fabs(bL[32]))));
			inputSampleL -= (bL[33] * (0.27440135050585784  + (0.15529667398122521*fabs(bL[33]))));
			inputSampleL -= (bL[34] * (0.21998973785078091  + (0.05083553737157104*fabs(bL[34]))));
			inputSampleL -= (bL[35] * (0.10323624876862457  - (0.04651829594199963*fabs(bL[35]))));
			inputSampleL += (bL[36] * (0.02091603687851074  + (0.12000046818439322*fabs(bL[36]))));
			inputSampleL += (bL[37] * (0.11344930914138468  + (0.17697142512225839*fabs(bL[37]))));
			inputSampleL += (bL[38] * (0.22766779627643968  + (0.13645102964003858*fabs(bL[38]))));
			inputSampleL += (bL[39] * (0.38378309953638229  - (0.01997653307333791*fabs(bL[39]))));
			inputSampleL += (bL[40] * (0.52789400804568076  - (0.21409137428422448*fabs(bL[40]))));
			inputSampleL += (bL[41] * (0.55444630296938280  - (0.32331980931576626*fabs(bL[41]))));
			inputSampleL += (bL[42] * (0.42333237669264601  - (0.26855847463044280*fabs(bL[42]))));
			inputSampleL += (bL[43] * (0.21942831522035078  - (0.12051365248820624*fabs(bL[43]))));
			inputSampleL -= (bL[44] * (0.00584169427830633  - (0.03706970171280329*fabs(bL[44]))));
			inputSampleL -= (bL[45] * (0.24279799124660351  - (0.17296440491477982*fabs(bL[45]))));
			inputSampleL -= (bL[46] * (0.40173760787507085  - (0.21717989835163351*fabs(bL[46]))));
			inputSampleL -= (bL[47] * (0.43930035724188155  - (0.16425928481378199*fabs(bL[47]))));
			inputSampleL -= (bL[48] * (0.41067765934041811  - (0.10390115786636855*fabs(bL[48]))));
			inputSampleL -= (bL[49] * (0.34409235547165967  - (0.07268159377411920*fabs(bL[49]))));
			inputSampleL -= (bL[50] * (0.26542883122568151  - (0.05483457497365785*fabs(bL[50]))));
			inputSampleL -= (bL[51] * (0.22024754776138800  - (0.06484897950087598*fabs(bL[51]))));
			inputSampleL -= (bL[52] * (0.20394367993632415  - (0.08746309731952180*fabs(bL[52]))));
			inputSampleL -= (bL[53] * (0.17565242431124092  - (0.07611309538078760*fabs(bL[53]))));
			inputSampleL -= (bL[54] * (0.10116623231246825  - (0.00642818706295112*fabs(bL[54]))));
			inputSampleL -= (bL[55] * (0.00782648272053632  + (0.08004141267685004*fabs(bL[55]))));
			inputSampleL += (bL[56] * (0.05059046006747323  - (0.12436676387548490*fabs(bL[56]))));
			inputSampleL += (bL[57] * (0.06241531553254467  - (0.11530779547021434*fabs(bL[57]))));
			inputSampleL += (bL[58] * (0.04952694587101836  - (0.08340945324333944*fabs(bL[58]))));
			inputSampleL += (bL[59] * (0.00843873294401687  - (0.03279659052562903*fabs(bL[59]))));
			inputSampleL -= (bL[60] * (0.05161338949440241  - (0.03428181149163798*fabs(bL[60]))));
			inputSampleL -= (bL[61] * (0.08165520146902012  - (0.08196746092283110*fabs(bL[61]))));
			inputSampleL -= (bL[62] * (0.06639532849935320  - (0.09797462781896329*fabs(bL[62]))));
			inputSampleL -= (bL[63] * (0.02953430910661621  - (0.09175612938515763*fabs(bL[63]))));
			inputSampleL += (bL[64] * (0.00741058547442938  + (0.05442091048731967*fabs(bL[64]))));
			inputSampleL += (bL[65] * (0.01832866125391727  + (0.00306243693643687*fabs(bL[65]))));
			inputSampleL += (bL[66] * (0.00526964230373573  - (0.04364102661136410*fabs(bL[66]))));
			inputSampleL -= (bL[67] * (0.00300984373848200  + (0.09742737841278880*fabs(bL[67]))));
			inputSampleL -= (bL[68] * (0.00413616769576694  + (0.14380661694523073*fabs(bL[68]))));
			inputSampleL -= (bL[69] * (0.00588769034931419  + (0.16012843578892538*fabs(bL[69]))));
			inputSampleL -= (bL[70] * (0.00688588239450581  + (0.14074464279305798*fabs(bL[70]))));
			inputSampleL -= (bL[71] * (0.02277307992926315  + (0.07914752191801366*fabs(bL[71]))));
			inputSampleL -= (bL[72] * (0.04627166091180877  - (0.00192787268067208*fabs(bL[72]))));
			inputSampleL -= (bL[73] * (0.05562045897455786  - (0.05932868727665747*fabs(bL[73]))));
			inputSampleL -= (bL[74] * (0.05134243784922165  - (0.08245334798868090*fabs(bL[74]))));
			inputSampleL -= (bL[75] * (0.04719409472239919  - (0.07498680629253825*fabs(bL[75]))));
			inputSampleL -= (bL[76] * (0.05889738914266415  - (0.06116127018043697*fabs(bL[76]))));
			inputSampleL -= (bL[77] * (0.09428363535111127  - (0.06535868867863834*fabs(bL[77]))));
			inputSampleL -= (bL[78] * (0.15181756953225126  - (0.08982979655234427*fabs(bL[78]))));
			inputSampleL -= (bL[79] * (0.20878969456036670  - (0.10761070891499538*fabs(bL[79]))));
			inputSampleL -= (bL[80] * (0.22647885581813790  - (0.08462542510349125*fabs(bL[80]))));
			inputSampleL -= (bL[81] * (0.19723482443646323  - (0.02665160920736287*fabs(bL[81]))));
			inputSampleL -= (bL[82] * (0.16441643451155163  + (0.02314691954338197*fabs(bL[82]))));
			inputSampleL -= (bL[83] * (0.15201914054931515  + (0.04424903493886839*fabs(bL[83]))));
			inputSampleL -= (bL[84] * (0.15454370641307855  + (0.04223203797913008*fabs(bL[84]))));
			
			temp = (inputSampleL + smoothCabBL)/3.0;
			smoothCabBL = inputSampleL;
			inputSampleL = temp/4.0;
			
			randy = ((double(fpdL)/UINT32_MAX)*0.057);
			drySampleL = ((((inputSampleL*(1-randy))+(lastCabSampleL*randy))*wet)+(drySampleL*(1.0-wet)))*outputlevel;
			lastCabSampleL = inputSampleL;
			inputSampleL = drySampleL; //cab L
			
			temp = (inputSampleR + smoothCabAR)/3.0;
			smoothCabAR = inputSampleR;
			inputSampleR = temp;
			
			bR[84] = bR[83]; bR[83] = bR[82]; bR[82] = bR[81]; bR[81] = bR[80]; bR[80] = bR[79]; 
			bR[79] = bR[78]; bR[78] = bR[77]; bR[77] = bR[76]; bR[76] = bR[75]; bR[75] = bR[74]; bR[74] = bR[73]; bR[73] = bR[72]; bR[72] = bR[71]; 
			bR[71] = bR[70]; bR[70] = bR[69]; bR[69] = bR[68]; bR[68] = bR[67]; bR[67] = bR[66]; bR[66] = bR[65]; bR[65] = bR[64]; bR[64] = bR[63]; 
			bR[63] = bR[62]; bR[62] = bR[61]; bR[61] = bR[60]; bR[60] = bR[59]; bR[59] = bR[58]; bR[58] = bR[57]; bR[57] = bR[56]; bR[56] = bR[55]; 
			bR[55] = bR[54]; bR[54] = bR[53]; bR[53] = bR[52]; bR[52] = bR[51]; bR[51] = bR[50]; bR[50] = bR[49]; bR[49] = bR[48]; bR[48] = bR[47]; 
			bR[47] = bR[46]; bR[46] = bR[45]; bR[45] = bR[44]; bR[44] = bR[43]; bR[43] = bR[42]; bR[42] = bR[41]; bR[41] = bR[40]; bR[40] = bR[39]; 
			bR[39] = bR[38]; bR[38] = bR[37]; bR[37] = bR[36]; bR[36] = bR[35]; bR[35] = bR[34]; bR[34] = bR[33]; bR[33] = bR[32]; bR[32] = bR[31]; 
			bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28]; bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; 
			bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; 
			bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7]; 
			bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR;
			inputSampleR += (bR[1] * (1.31698250313308396  - (0.08140616497621633*fabs(bR[1]))));
			inputSampleR += (bR[2] * (1.47229016949915326  - (0.27680278993637253*fabs(bR[2]))));
			inputSampleR += (bR[3] * (1.30410109086044956  - (0.35629113432046489*fabs(bR[3]))));
			inputSampleR += (bR[4] * (0.81766210474551260  - (0.26808782337659753*fabs(bR[4]))));
			inputSampleR += (bR[5] * (0.19868872545506663  - (0.11105517193919669*fabs(bR[5]))));
			inputSampleR -= (bR[6] * (0.39115909132567039  - (0.12630622002682679*fabs(bR[6]))));
			inputSampleR -= (bR[7] * (0.76881891559343574  - (0.40879849500403143*fabs(bR[7]))));
			inputSampleR -= (bR[8] * (0.87146861782680340  - (0.59529560488000599*fabs(bR[8]))));
			inputSampleR -= (bR[9] * (0.79504575932563670  - (0.60877047551611796*fabs(bR[9]))));
			inputSampleR -= (bR[10] * (0.61653017622406314  - (0.47662851438557335*fabs(bR[10]))));
			inputSampleR -= (bR[11] * (0.40718195794382067  - (0.24955839378539713*fabs(bR[11]))));
			inputSampleR -= (bR[12] * (0.31794900040616203  - (0.04169792259600613*fabs(bR[12]))));
			inputSampleR -= (bR[13] * (0.41075032540217843  + (0.00368483996076280*fabs(bR[13]))));
			inputSampleR -= (bR[14] * (0.56901352922170667  - (0.11027360805893105*fabs(bR[14]))));
			inputSampleR -= (bR[15] * (0.62443222391889264  - (0.22198075154245228*fabs(bR[15]))));
			inputSampleR -= (bR[16] * (0.53462856723129204  - (0.22933544545324852*fabs(bR[16]))));
			inputSampleR -= (bR[17] * (0.34441703361995046  - (0.12956809502269492*fabs(bR[17]))));
			inputSampleR -= (bR[18] * (0.13947052337867882  + (0.00339775055962799*fabs(bR[18]))));
			inputSampleR += (bR[19] * (0.03771252648928484  - (0.10863931549251718*fabs(bR[19]))));
			inputSampleR += (bR[20] * (0.18280210770271693  - (0.17413646599296417*fabs(bR[20]))));
			inputSampleR += (bR[21] * (0.24621986701761467  - (0.14547053270435095*fabs(bR[21]))));
			inputSampleR += (bR[22] * (0.22347075142737360  - (0.02493869490104031*fabs(bR[22]))));
			inputSampleR += (bR[23] * (0.14346348482123716  + (0.11284054747963246*fabs(bR[23]))));
			inputSampleR += (bR[24] * (0.00834364862916028  + (0.24284684053733926*fabs(bR[24]))));
			inputSampleR -= (bR[25] * (0.11559740296078347  - (0.32623054435304538*fabs(bR[25]))));
			inputSampleR -= (bR[26] * (0.18067604561283060  - (0.32311481551122478*fabs(bR[26]))));
			inputSampleR -= (bR[27] * (0.22927997789035612  - (0.26991539052832925*fabs(bR[27]))));
			inputSampleR -= (bR[28] * (0.28487666578669446  - (0.22437227250279349*fabs(bR[28]))));
			inputSampleR -= (bR[29] * (0.31992973037153838  - (0.15289876100963865*fabs(bR[29]))));
			inputSampleR -= (bR[30] * (0.35174606303520733  - (0.05656293023086628*fabs(bR[30]))));
			inputSampleR -= (bR[31] * (0.36894898011375254  + (0.04333925421463558*fabs(bR[31]))));
			inputSampleR -= (bR[32] * (0.32567576055307507  + (0.14594589410921388*fabs(bR[32]))));
			inputSampleR -= (bR[33] * (0.27440135050585784  + (0.15529667398122521*fabs(bR[33]))));
			inputSampleR -= (bR[34] * (0.21998973785078091  + (0.05083553737157104*fabs(bR[34]))));
			inputSampleR -= (bR[35] * (0.10323624876862457  - (0.04651829594199963*fabs(bR[35]))));
			inputSampleR += (bR[36] * (0.02091603687851074  + (0.12000046818439322*fabs(bR[36]))));
			inputSampleR += (bR[37] * (0.11344930914138468  + (0.17697142512225839*fabs(bR[37]))));
			inputSampleR += (bR[38] * (0.22766779627643968  + (0.13645102964003858*fabs(bR[38]))));
			inputSampleR += (bR[39] * (0.38378309953638229  - (0.01997653307333791*fabs(bR[39]))));
			inputSampleR += (bR[40] * (0.52789400804568076  - (0.21409137428422448*fabs(bR[40]))));
			inputSampleR += (bR[41] * (0.55444630296938280  - (0.32331980931576626*fabs(bR[41]))));
			inputSampleR += (bR[42] * (0.42333237669264601  - (0.26855847463044280*fabs(bR[42]))));
			inputSampleR += (bR[43] * (0.21942831522035078  - (0.12051365248820624*fabs(bR[43]))));
			inputSampleR -= (bR[44] * (0.00584169427830633  - (0.03706970171280329*fabs(bR[44]))));
			inputSampleR -= (bR[45] * (0.24279799124660351  - (0.17296440491477982*fabs(bR[45]))));
			inputSampleR -= (bR[46] * (0.40173760787507085  - (0.21717989835163351*fabs(bR[46]))));
			inputSampleR -= (bR[47] * (0.43930035724188155  - (0.16425928481378199*fabs(bR[47]))));
			inputSampleR -= (bR[48] * (0.41067765934041811  - (0.10390115786636855*fabs(bR[48]))));
			inputSampleR -= (bR[49] * (0.34409235547165967  - (0.07268159377411920*fabs(bR[49]))));
			inputSampleR -= (bR[50] * (0.26542883122568151  - (0.05483457497365785*fabs(bR[50]))));
			inputSampleR -= (bR[51] * (0.22024754776138800  - (0.06484897950087598*fabs(bR[51]))));
			inputSampleR -= (bR[52] * (0.20394367993632415  - (0.08746309731952180*fabs(bR[52]))));
			inputSampleR -= (bR[53] * (0.17565242431124092  - (0.07611309538078760*fabs(bR[53]))));
			inputSampleR -= (bR[54] * (0.10116623231246825  - (0.00642818706295112*fabs(bR[54]))));
			inputSampleR -= (bR[55] * (0.00782648272053632  + (0.08004141267685004*fabs(bR[55]))));
			inputSampleR += (bR[56] * (0.05059046006747323  - (0.12436676387548490*fabs(bR[56]))));
			inputSampleR += (bR[57] * (0.06241531553254467  - (0.11530779547021434*fabs(bR[57]))));
			inputSampleR += (bR[58] * (0.04952694587101836  - (0.08340945324333944*fabs(bR[58]))));
			inputSampleR += (bR[59] * (0.00843873294401687  - (0.03279659052562903*fabs(bR[59]))));
			inputSampleR -= (bR[60] * (0.05161338949440241  - (0.03428181149163798*fabs(bR[60]))));
			inputSampleR -= (bR[61] * (0.08165520146902012  - (0.08196746092283110*fabs(bR[61]))));
			inputSampleR -= (bR[62] * (0.06639532849935320  - (0.09797462781896329*fabs(bR[62]))));
			inputSampleR -= (bR[63] * (0.02953430910661621  - (0.09175612938515763*fabs(bR[63]))));
			inputSampleR += (bR[64] * (0.00741058547442938  + (0.05442091048731967*fabs(bR[64]))));
			inputSampleR += (bR[65] * (0.01832866125391727  + (0.00306243693643687*fabs(bR[65]))));
			inputSampleR += (bR[66] * (0.00526964230373573  - (0.04364102661136410*fabs(bR[66]))));
			inputSampleR -= (bR[67] * (0.00300984373848200  + (0.09742737841278880*fabs(bR[67]))));
			inputSampleR -= (bR[68] * (0.00413616769576694  + (0.14380661694523073*fabs(bR[68]))));
			inputSampleR -= (bR[69] * (0.00588769034931419  + (0.16012843578892538*fabs(bR[69]))));
			inputSampleR -= (bR[70] * (0.00688588239450581  + (0.14074464279305798*fabs(bR[70]))));
			inputSampleR -= (bR[71] * (0.02277307992926315  + (0.07914752191801366*fabs(bR[71]))));
			inputSampleR -= (bR[72] * (0.04627166091180877  - (0.00192787268067208*fabs(bR[72]))));
			inputSampleR -= (bR[73] * (0.05562045897455786  - (0.05932868727665747*fabs(bR[73]))));
			inputSampleR -= (bR[74] * (0.05134243784922165  - (0.08245334798868090*fabs(bR[74]))));
			inputSampleR -= (bR[75] * (0.04719409472239919  - (0.07498680629253825*fabs(bR[75]))));
			inputSampleR -= (bR[76] * (0.05889738914266415  - (0.06116127018043697*fabs(bR[76]))));
			inputSampleR -= (bR[77] * (0.09428363535111127  - (0.06535868867863834*fabs(bR[77]))));
			inputSampleR -= (bR[78] * (0.15181756953225126  - (0.08982979655234427*fabs(bR[78]))));
			inputSampleR -= (bR[79] * (0.20878969456036670  - (0.10761070891499538*fabs(bR[79]))));
			inputSampleR -= (bR[80] * (0.22647885581813790  - (0.08462542510349125*fabs(bR[80]))));
			inputSampleR -= (bR[81] * (0.19723482443646323  - (0.02665160920736287*fabs(bR[81]))));
			inputSampleR -= (bR[82] * (0.16441643451155163  + (0.02314691954338197*fabs(bR[82]))));
			inputSampleR -= (bR[83] * (0.15201914054931515  + (0.04424903493886839*fabs(bR[83]))));
			inputSampleR -= (bR[84] * (0.15454370641307855  + (0.04223203797913008*fabs(bR[84]))));
			
			temp = (inputSampleR + smoothCabBR)/3.0;
			smoothCabBR = inputSampleR;
			inputSampleR = temp/4.0;
			
			randy = ((double(fpdR)/UINT32_MAX)*0.057);
			drySampleR = ((((inputSampleR*(1.0-randy))+(lastCabSampleR*randy))*wet)+(drySampleR*(1.0-wet)))*outputlevel;
			lastCabSampleR = inputSampleR;
			inputSampleR = drySampleR; //cab
			
			if (cycleEnd == 4) {
				lastRefL[0] = lastRefL[4]; //start from previous last
				lastRefL[2] = (lastRefL[0] + inputSampleL)/2; //half
				lastRefL[1] = (lastRefL[0] + lastRefL[2])/2; //one quarter
				lastRefL[3] = (lastRefL[2] + inputSampleL)/2; //three quarters
				lastRefL[4] = inputSampleL; //full
				lastRefR[0] = lastRefR[4]; //start from previous last
				lastRefR[2] = (lastRefR[0] + inputSampleR)/2; //half
				lastRefR[1] = (lastRefR[0] + lastRefR[2])/2; //one quarter
				lastRefR[3] = (lastRefR[2] + inputSampleR)/2; //three quarters
				lastRefR[4] = inputSampleR; //full
			}
			if (cycleEnd == 3) {
				lastRefL[0] = lastRefL[3]; //start from previous last
				lastRefL[2] = (lastRefL[0]+lastRefL[0]+inputSampleL)/3; //third
				lastRefL[1] = (lastRefL[0]+inputSampleL+inputSampleL)/3; //two thirds
				lastRefL[3] = inputSampleL; //full
				lastRefR[0] = lastRefR[3]; //start from previous last
				lastRefR[2] = (lastRefR[0]+lastRefR[0]+inputSampleR)/3; //third
				lastRefR[1] = (lastRefR[0]+inputSampleR+inputSampleR)/3; //two thirds
				lastRefR[3] = inputSampleR; //full
			}
			if (cycleEnd == 2) {
				lastRefL[0] = lastRefL[2]; //start from previous last
				lastRefL[1] = (lastRefL[0] + inputSampleL)/2; //half
				lastRefL[2] = inputSampleL; //full
				lastRefR[0] = lastRefR[2]; //start from previous last
				lastRefR[1] = (lastRefR[0] + inputSampleR)/2; //half
				lastRefR[2] = inputSampleR; //full
			}
			if (cycleEnd == 1) {
				lastRefL[0] = inputSampleL;
				lastRefR[0] = inputSampleR;
			}
			cycle = 0; //reset
			inputSampleL = lastRefL[cycle];
			inputSampleR = lastRefR[cycle];
		} else {
			inputSampleL = lastRefL[cycle];
			inputSampleR = lastRefR[cycle];
			//we are going through our references now
		}
		switch (cycleEnd) //multi-pole average using lastRef[] variables
		{
			case 4:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[7])*0.5;
				lastRefL[7] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[7])*0.5;
				lastRefR[7] = lastRefR[8]; //continue, do not break
			case 3:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[6])*0.5;
				lastRefL[6] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[6])*0.5;
				lastRefR[6] = lastRefR[8]; //continue, do not break
			case 2:
				lastRefL[8] = inputSampleL; inputSampleL = (inputSampleL+lastRefL[5])*0.5;
				lastRefL[5] = lastRefL[8]; //continue, do not break
				lastRefR[8] = inputSampleR; inputSampleR = (inputSampleR+lastRefR[5])*0.5;
				lastRefR[5] = lastRefR[8]; //continue, do not break
			case 1:
				break; //no further averaging
		}
		
		//begin 64 bit stereo floating point dither
		//int expon; frexp((double)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		//inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//frexp((double)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		//inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


} // end namespace FireAmp

