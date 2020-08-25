/* ========================================
 *  BrassRider - BrassRider.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BrassRider_H
#include "BrassRider.h"
#endif

namespace BrassRider {


void BrassRider::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double limitOut = A*16;
	int offsetA = 13500;
	int offsetB = 16700;
	double wet = B;
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;
		
		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}
		
		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is 
		//effectively digital black, we'll subtract it again. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		inputSampleL *= limitOut;
		highIIRL = (highIIRL*0.5);
		highIIRL += (inputSampleL*0.5);
		inputSampleL -= highIIRL;
		highIIR2L = (highIIR2L*0.5);
		highIIR2L += (inputSampleL*0.5);
		inputSampleL -= highIIR2L;
		long double slewSampleL = fabs(inputSampleL - lastSampleL);
		lastSampleL = inputSampleL;
		slewSampleL /= fabs(inputSampleL * lastSampleL)+0.2;
		slewIIRL = (slewIIRL*0.5);
		slewIIRL += (slewSampleL*0.5);
		slewSampleL = fabs(slewSampleL - slewIIRL);
		slewIIR2L = (slewIIR2L*0.5);
		slewIIR2L += (slewSampleL*0.5);
		slewSampleL = fabs(slewSampleL - slewIIR2L);
		long double bridgerectifier = slewSampleL;
		//there's the left channel, now to feed it to overall clamp
		
		if (bridgerectifier > 3.1415) bridgerectifier = 0.0;
		bridgerectifier = sin(bridgerectifier);
		if (gcount < 0 || gcount > 40000) {gcount = 40000;}
		d[gcount+40000] = d[gcount] = bridgerectifier;
		control += (d[gcount] / (offsetA+1));
		control -= (d[gcount+offsetA] / offsetA);
		double ramp = (control*control) * 16.0;
		e[gcount+40000] = e[gcount] = ramp;
		clamp += (e[gcount] / (offsetB+1));
		clamp -= (e[gcount+offsetB] / offsetB);
		if (clamp > wet*8) clamp = wet*8;
		gcount--;
		
		inputSampleR *= limitOut;
		highIIRR = (highIIRR*0.5);
		highIIRR += (inputSampleR*0.5);
		inputSampleR -= highIIRR;
		highIIR2R = (highIIR2R*0.5);
		highIIR2R += (inputSampleR*0.5);
		inputSampleR -= highIIR2R;
		long double slewSampleR = fabs(inputSampleR - lastSampleR);
		lastSampleR = inputSampleR;
		slewSampleR /= fabs(inputSampleR * lastSampleR)+0.2;
		slewIIRR = (slewIIRR*0.5);
		slewIIRR += (slewSampleR*0.5);
		slewSampleR = fabs(slewSampleR - slewIIRR);
		slewIIR2R = (slewIIR2R*0.5);
		slewIIR2R += (slewSampleR*0.5);
		slewSampleR = fabs(slewSampleR - slewIIR2R);
		bridgerectifier = slewSampleR;
		//there's the right channel, now to feed it to overall clamp
		
		if (bridgerectifier > 3.1415) bridgerectifier = 0.0;
		bridgerectifier = sin(bridgerectifier);
		if (gcount < 0 || gcount > 40000) {gcount = 40000;}
		d[gcount+40000] = d[gcount] = bridgerectifier;
		control += (d[gcount] / (offsetA+1));
		control -= (d[gcount+offsetA] / offsetA);
		ramp = (control*control) * 16.0;
		e[gcount+40000] = e[gcount] = ramp;
		clamp += (e[gcount] / (offsetB+1));
		clamp -= (e[gcount+offsetB] / offsetB);
		if (clamp > wet*8) clamp = wet*8;
		gcount--;
		
		inputSampleL = (drySampleL * (1.0-wet)) + (drySampleL * clamp * wet * 16.0);
		inputSampleR = (drySampleR * (1.0-wet)) + (drySampleR * clamp * wet * 16.0);
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void BrassRider::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double limitOut = A*16;
	int offsetA = 13500;
	int offsetB = 16700;
	double wet = B;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;
		
		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}
		
		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is 
		//effectively digital black, we'll subtract it again. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		inputSampleL *= limitOut;
		highIIRL = (highIIRL*0.5);
		highIIRL += (inputSampleL*0.5);
		inputSampleL -= highIIRL;
		highIIR2L = (highIIR2L*0.5);
		highIIR2L += (inputSampleL*0.5);
		inputSampleL -= highIIR2L;
		long double slewSampleL = fabs(inputSampleL - lastSampleL);
		lastSampleL = inputSampleL;
		slewSampleL /= fabs(inputSampleL * lastSampleL)+0.2;
		slewIIRL = (slewIIRL*0.5);
		slewIIRL += (slewSampleL*0.5);
		slewSampleL = fabs(slewSampleL - slewIIRL);
		slewIIR2L = (slewIIR2L*0.5);
		slewIIR2L += (slewSampleL*0.5);
		slewSampleL = fabs(slewSampleL - slewIIR2L);
		long double bridgerectifier = slewSampleL;
		//there's the left channel, now to feed it to overall clamp
		
		if (bridgerectifier > 3.1415) bridgerectifier = 0.0;
		bridgerectifier = sin(bridgerectifier);
		if (gcount < 0 || gcount > 40000) {gcount = 40000;}
		d[gcount+40000] = d[gcount] = bridgerectifier;
		control += (d[gcount] / (offsetA+1));
		control -= (d[gcount+offsetA] / offsetA);
		double ramp = (control*control) * 16.0;
		e[gcount+40000] = e[gcount] = ramp;
		clamp += (e[gcount] / (offsetB+1));
		clamp -= (e[gcount+offsetB] / offsetB);
		if (clamp > wet*8) clamp = wet*8;
		gcount--;
		
		inputSampleR *= limitOut;
		highIIRR = (highIIRR*0.5);
		highIIRR += (inputSampleR*0.5);
		inputSampleR -= highIIRR;
		highIIR2R = (highIIR2R*0.5);
		highIIR2R += (inputSampleR*0.5);
		inputSampleR -= highIIR2R;
		long double slewSampleR = fabs(inputSampleR - lastSampleR);
		lastSampleR = inputSampleR;
		slewSampleR /= fabs(inputSampleR * lastSampleR)+0.2;
		slewIIRR = (slewIIRR*0.5);
		slewIIRR += (slewSampleR*0.5);
		slewSampleR = fabs(slewSampleR - slewIIRR);
		slewIIR2R = (slewIIR2R*0.5);
		slewIIR2R += (slewSampleR*0.5);
		slewSampleR = fabs(slewSampleR - slewIIR2R);
		bridgerectifier = slewSampleR;
		//there's the right channel, now to feed it to overall clamp
		
		if (bridgerectifier > 3.1415) bridgerectifier = 0.0;
		bridgerectifier = sin(bridgerectifier);
		if (gcount < 0 || gcount > 40000) {gcount = 40000;}
		d[gcount+40000] = d[gcount] = bridgerectifier;
		control += (d[gcount] / (offsetA+1));
		control -= (d[gcount+offsetA] / offsetA);
		ramp = (control*control) * 16.0;
		e[gcount+40000] = e[gcount] = ramp;
		clamp += (e[gcount] / (offsetB+1));
		clamp -= (e[gcount+offsetB] / offsetB);
		if (clamp > wet*8) clamp = wet*8;
		gcount--;
		
		inputSampleL = (drySampleL * (1.0-wet)) + (drySampleL * clamp * wet * 16.0);
		inputSampleR = (drySampleR * (1.0-wet)) + (drySampleR * clamp * wet * 16.0);
		
		//begin 64 bit stereo floating point dither
		int expon; frexp((double)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		frexp((double)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}


} // end namespace BrassRider

