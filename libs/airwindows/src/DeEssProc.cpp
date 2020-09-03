/* ========================================
 *  DeEss - DeEss.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeEss_H
#include "DeEss.h"
#endif

namespace DeEss {


void DeEss::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double intensity = pow(A,5)*(8192/overallscale);
	double maxdess = 1.0 / pow(10.0,((B-1.0)*48.0)/20);
	double iirAmount = pow(C,2)/overallscale;
	double offset;
	double sense;
	double recovery;
	double attackspeed;
    
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
		//effectively digital black, we'll subtract it aDeEss. We want a 'air' hiss
		
		s3L = s2L;
		s2L = s1L;
		s1L = inputSampleL;
		m1L = (s1L-s2L)*((s1L-s2L)/1.3);
		m2L = (s2L-s3L)*((s1L-s2L)/1.3);
		sense = fabs((m1L-m2L)*((m1L-m2L)/1.3));
		//this will be 0 for smooth, high for SSS
		attackspeed = 7.0+(sense*1024);
		//this does not vary with intensity, but it does react to onset transients
		
		sense = 1.0+(intensity*intensity*sense);
		if (sense > intensity) {sense = intensity;}
		//this will be 1 for smooth, 'intensity' for SSS
		recovery = 1.0+(0.01/sense);
		//this will be 1.1 for smooth, 1.0000000...1 for SSS
		
		offset = 1.0-fabs(inputSampleL);
		
		if (flip) {
			iirSampleAL = (iirSampleAL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			if (ratioAL < sense)
			{ratioAL = ((ratioAL*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioAL = 1.0+((ratioAL-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioAL > maxdess){ratioAL = maxdess;}
			inputSampleL = iirSampleAL+((inputSampleL-iirSampleAL)/ratioAL);
		}
		else {
			iirSampleBL = (iirSampleBL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));	
			if (ratioBL < sense)
			{ratioBL = ((ratioBL*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioBL = 1.0+((ratioBL-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioBL > maxdess){ratioBL = maxdess;}
			inputSampleL = iirSampleBL+((inputSampleL-iirSampleBL)/ratioBL);
		} //have the ratio chase Sense
		
		s3R = s2R;
		s2R = s1R;
		s1R = inputSampleR;
		m1R = (s1R-s2R)*((s1R-s2R)/1.3);
		m2R = (s2R-s3R)*((s1R-s2R)/1.3);
		sense = fabs((m1R-m2R)*((m1R-m2R)/1.3));
		//this will be 0 for smooth, high for SSS
		attackspeed = 7.0+(sense*1024);
		//this does not vary with intensity, but it does react to onset transients
		
		sense = 1.0+(intensity*intensity*sense);
		if (sense > intensity) {sense = intensity;}
		//this will be 1 for smooth, 'intensity' for SSS
		recovery = 1.0+(0.01/sense);
		//this will be 1.1 for smooth, 1.0000000...1 for SSS
		
		offset = 1.0-fabs(inputSampleR);
		
		if (flip) {
			iirSampleAR = (iirSampleAR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			if (ratioAR < sense)
			{ratioAR = ((ratioAR*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioAR = 1.0+((ratioAR-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioAR > maxdess){ratioAR = maxdess;}
			inputSampleR = iirSampleAR+((inputSampleR-iirSampleAR)/ratioAR);
		}
		else {
			iirSampleBR = (iirSampleBR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));	
			if (ratioBR < sense)
			{ratioBR = ((ratioBR*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioBR = 1.0+((ratioBR-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioBR > maxdess){ratioBR = maxdess;}
			inputSampleR = iirSampleBR+((inputSampleR-iirSampleBR)/ratioBR);
		} //have the ratio chase Sense
		
		flip = !flip;
		
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

void DeEss::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double intensity = pow(A,5)*(8192/overallscale);
	double maxdess = 1.0 / pow(10.0,((B-1.0)*48.0)/20);
	double iirAmount = pow(C,2)/overallscale;
	double offset;
	double sense;
	double recovery;
	double attackspeed;
    
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
		//effectively digital black, we'll subtract it aDeEss. We want a 'air' hiss
		
		s3L = s2L;
		s2L = s1L;
		s1L = inputSampleL;
		m1L = (s1L-s2L)*((s1L-s2L)/1.3);
		m2L = (s2L-s3L)*((s1L-s2L)/1.3);
		sense = fabs((m1L-m2L)*((m1L-m2L)/1.3));
		//this will be 0 for smooth, high for SSS
		attackspeed = 7.0+(sense*1024);
		//this does not vary with intensity, but it does react to onset transients
		
		sense = 1.0+(intensity*intensity*sense);
		if (sense > intensity) {sense = intensity;}
		//this will be 1 for smooth, 'intensity' for SSS
		recovery = 1.0+(0.01/sense);
		//this will be 1.1 for smooth, 1.0000000...1 for SSS
		
		offset = 1.0-fabs(inputSampleL);
		
		if (flip) {
			iirSampleAL = (iirSampleAL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			if (ratioAL < sense)
			{ratioAL = ((ratioAL*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioAL = 1.0+((ratioAL-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioAL > maxdess){ratioAL = maxdess;}
			inputSampleL = iirSampleAL+((inputSampleL-iirSampleAL)/ratioAL);
		}
		else {
			iirSampleBL = (iirSampleBL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));	
			if (ratioBL < sense)
			{ratioBL = ((ratioBL*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioBL = 1.0+((ratioBL-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioBL > maxdess){ratioBL = maxdess;}
			inputSampleL = iirSampleBL+((inputSampleL-iirSampleBL)/ratioBL);
		} //have the ratio chase Sense
		
		s3R = s2R;
		s2R = s1R;
		s1R = inputSampleR;
		m1R = (s1R-s2R)*((s1R-s2R)/1.3);
		m2R = (s2R-s3R)*((s1R-s2R)/1.3);
		sense = fabs((m1R-m2R)*((m1R-m2R)/1.3));
		//this will be 0 for smooth, high for SSS
		attackspeed = 7.0+(sense*1024);
		//this does not vary with intensity, but it does react to onset transients
		
		sense = 1.0+(intensity*intensity*sense);
		if (sense > intensity) {sense = intensity;}
		//this will be 1 for smooth, 'intensity' for SSS
		recovery = 1.0+(0.01/sense);
		//this will be 1.1 for smooth, 1.0000000...1 for SSS
		
		offset = 1.0-fabs(inputSampleR);
		
		if (flip) {
			iirSampleAR = (iirSampleAR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			if (ratioAR < sense)
			{ratioAR = ((ratioAR*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioAR = 1.0+((ratioAR-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioAR > maxdess){ratioAR = maxdess;}
			inputSampleR = iirSampleAR+((inputSampleR-iirSampleAR)/ratioAR);
		}
		else {
			iirSampleBR = (iirSampleBR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));	
			if (ratioBR < sense)
			{ratioBR = ((ratioBR*attackspeed)+sense)/(attackspeed+1.0);}
			else
			{ratioBR = 1.0+((ratioBR-1.0)/recovery);}
			//returny to 1/1 code
			if (ratioBR > maxdess){ratioBR = maxdess;}
			inputSampleR = iirSampleBR+((inputSampleR-iirSampleBR)/ratioBR);
		} //have the ratio chase Sense
		
		flip = !flip;
		
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


} // end namespace DeEss

