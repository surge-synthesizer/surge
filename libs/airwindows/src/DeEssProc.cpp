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
		
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace DeEss

