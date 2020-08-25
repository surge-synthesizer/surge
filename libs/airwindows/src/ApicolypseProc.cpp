/* ========================================
 *  Apicolypse - Apicolypse.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Apicolypse_H
#include "Apicolypse.h"
#endif

namespace Apicolypse {


void Apicolypse::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	double threshold = A;
	double hardness;
	double breakup = (1.0-(threshold/2.0))*3.14159265358979;
	double bridgerectifier;
	double sqdrive = (B*3.0);
	if (sqdrive > 1.0) sqdrive *= sqdrive;
	sqdrive = sqrt(sqdrive);
	double indrive = C*3.0;
	if (indrive > 1.0) indrive *= indrive;
	indrive *= (1.0-(0.008*sqdrive));
	//correct for gain loss of convolution
	//calibrate this to match noise level with character at 1.0
	//you get for instance 0.819 and 1.0-0.819 is 0.181
	double randy;
	double outlevel = D;
	
	if (threshold < 1) hardness = 1.0 / (1.0-threshold);
	else hardness = 999999999999999999999.0;
	//set up hardness to exactly fill gap between threshold and 0db
	//if threshold is literally 1 then hardness is infinite, so we make it very big
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
		if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
		
		inputSampleL *= indrive;
		inputSampleR *= indrive;
		//calibrated to match gain through convolution and -0.3 correction
				
		if (sqdrive > 0.0){
			bL[33] = bL[32]; bL[32] = bL[31]; 
			bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28]; bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; 
			bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; 
			bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7]; 
			bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL * sqdrive;

			inputSampleL += (bL[1] * (0.09299870608542582  - (0.00009582362368873*fabs(bL[1]))));
			inputSampleL -= (bL[2] * (0.11947847710741009  - (0.00004500891602770*fabs(bL[2]))));
			inputSampleL += (bL[3] * (0.09071606264761795  + (0.00005639498984741*fabs(bL[3]))));
			inputSampleL -= (bL[4] * (0.08561982770836980  - (0.00004964855606916*fabs(bL[4]))));
			inputSampleL += (bL[5] * (0.06440549220820363  + (0.00002428052139507*fabs(bL[5]))));
			inputSampleL -= (bL[6] * (0.05987991812840746  + (0.00000101867082290*fabs(bL[6]))));
			inputSampleL += (bL[7] * (0.03980233135839382  + (0.00003312430049041*fabs(bL[7]))));
			inputSampleL -= (bL[8] * (0.03648402630896925  - (0.00002116186381142*fabs(bL[8]))));
			inputSampleL += (bL[9] * (0.01826860869525248  + (0.00003115110025396*fabs(bL[9]))));
			inputSampleL -= (bL[10] * (0.01723968622495364  - (0.00002450634121718*fabs(bL[10]))));
			inputSampleL += (bL[11] * (0.00187588812316724  + (0.00002838206198968*fabs(bL[11]))));
			inputSampleL -= (bL[12] * (0.00381796423957237  - (0.00003155815499462*fabs(bL[12]))));
			inputSampleL -= (bL[13] * (0.00852092214496733  - (0.00001702651162392*fabs(bL[13]))));
			inputSampleL += (bL[14] * (0.00315560292270588  + (0.00002547861676047*fabs(bL[14]))));
			inputSampleL -= (bL[15] * (0.01258630914496868  - (0.00004555319243213*fabs(bL[15]))));
			inputSampleL += (bL[16] * (0.00536435648963575  + (0.00001812393657101*fabs(bL[16]))));
			inputSampleL -= (bL[17] * (0.01272975658159178  - (0.00004103775306121*fabs(bL[17]))));
			inputSampleL += (bL[18] * (0.00403818975172755  + (0.00003764615492871*fabs(bL[18]))));
			inputSampleL -= (bL[19] * (0.01042617366897483  - (0.00003605210426041*fabs(bL[19]))));
			inputSampleL += (bL[20] * (0.00126599583390057  + (0.00004305458668852*fabs(bL[20]))));
			inputSampleL -= (bL[21] * (0.00747876207688339  - (0.00003731207018977*fabs(bL[21]))));
			inputSampleL -= (bL[22] * (0.00149873689175324  - (0.00005086601800791*fabs(bL[22]))));
			inputSampleL -= (bL[23] * (0.00503221309488033  - (0.00003636086782783*fabs(bL[23]))));
			inputSampleL -= (bL[24] * (0.00342998224655821  - (0.00004103091180506*fabs(bL[24]))));
			inputSampleL -= (bL[25] * (0.00355585977903117  - (0.00003698982145400*fabs(bL[25]))));
			inputSampleL -= (bL[26] * (0.00437201792934817  - (0.00002720235666939*fabs(bL[26]))));
			inputSampleL -= (bL[27] * (0.00299217874451556  - (0.00004446954727956*fabs(bL[27]))));
			inputSampleL -= (bL[28] * (0.00457924652487249  - (0.00003859065778860*fabs(bL[28]))));
			inputSampleL -= (bL[29] * (0.00298182934892027  - (0.00002064710931733*fabs(bL[29]))));
			inputSampleL -= (bL[30] * (0.00438838441540584  - (0.00005223008424866*fabs(bL[30]))));
			inputSampleL -= (bL[31] * (0.00323984218794705  - (0.00003397987535887*fabs(bL[31]))));
			inputSampleL -= (bL[32] * (0.00407693981307314  - (0.00003935772436894*fabs(bL[32]))));
			inputSampleL -= (bL[33] * (0.00350435348467321  - (0.00005525463935338*fabs(bL[33]))));
			
			bR[33] = bR[32]; bR[32] = bR[31]; 
			bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28]; bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; 
			bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; 
			bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7]; 
			bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR * sqdrive;
		
		inputSampleR += (bR[1] * (0.09299870608542582  - (0.00009582362368873*fabs(bR[1]))));
		inputSampleR -= (bR[2] * (0.11947847710741009  - (0.00004500891602770*fabs(bR[2]))));
		inputSampleR += (bR[3] * (0.09071606264761795  + (0.00005639498984741*fabs(bR[3]))));
		inputSampleR -= (bR[4] * (0.08561982770836980  - (0.00004964855606916*fabs(bR[4]))));
		inputSampleR += (bR[5] * (0.06440549220820363  + (0.00002428052139507*fabs(bR[5]))));
		inputSampleR -= (bR[6] * (0.05987991812840746  + (0.00000101867082290*fabs(bR[6]))));
		inputSampleR += (bR[7] * (0.03980233135839382  + (0.00003312430049041*fabs(bR[7]))));
		inputSampleR -= (bR[8] * (0.03648402630896925  - (0.00002116186381142*fabs(bR[8]))));
		inputSampleR += (bR[9] * (0.01826860869525248  + (0.00003115110025396*fabs(bR[9]))));
		inputSampleR -= (bR[10] * (0.01723968622495364  - (0.00002450634121718*fabs(bR[10]))));
		inputSampleR += (bR[11] * (0.00187588812316724  + (0.00002838206198968*fabs(bR[11]))));
		inputSampleR -= (bR[12] * (0.00381796423957237  - (0.00003155815499462*fabs(bR[12]))));
		inputSampleR -= (bR[13] * (0.00852092214496733  - (0.00001702651162392*fabs(bR[13]))));
		inputSampleR += (bR[14] * (0.00315560292270588  + (0.00002547861676047*fabs(bR[14]))));
		inputSampleR -= (bR[15] * (0.01258630914496868  - (0.00004555319243213*fabs(bR[15]))));
		inputSampleR += (bR[16] * (0.00536435648963575  + (0.00001812393657101*fabs(bR[16]))));
		inputSampleR -= (bR[17] * (0.01272975658159178  - (0.00004103775306121*fabs(bR[17]))));
		inputSampleR += (bR[18] * (0.00403818975172755  + (0.00003764615492871*fabs(bR[18]))));
		inputSampleR -= (bR[19] * (0.01042617366897483  - (0.00003605210426041*fabs(bR[19]))));
		inputSampleR += (bR[20] * (0.00126599583390057  + (0.00004305458668852*fabs(bR[20]))));
		inputSampleR -= (bR[21] * (0.00747876207688339  - (0.00003731207018977*fabs(bR[21]))));
		inputSampleR -= (bR[22] * (0.00149873689175324  - (0.00005086601800791*fabs(bR[22]))));
		inputSampleR -= (bR[23] * (0.00503221309488033  - (0.00003636086782783*fabs(bR[23]))));
		inputSampleR -= (bR[24] * (0.00342998224655821  - (0.00004103091180506*fabs(bR[24]))));
		inputSampleR -= (bR[25] * (0.00355585977903117  - (0.00003698982145400*fabs(bR[25]))));
		inputSampleR -= (bR[26] * (0.00437201792934817  - (0.00002720235666939*fabs(bR[26]))));
		inputSampleR -= (bR[27] * (0.00299217874451556  - (0.00004446954727956*fabs(bR[27]))));
		inputSampleR -= (bR[28] * (0.00457924652487249  - (0.00003859065778860*fabs(bR[28]))));
		inputSampleR -= (bR[29] * (0.00298182934892027  - (0.00002064710931733*fabs(bR[29]))));
		inputSampleR -= (bR[30] * (0.00438838441540584  - (0.00005223008424866*fabs(bR[30]))));
		inputSampleR -= (bR[31] * (0.00323984218794705  - (0.00003397987535887*fabs(bR[31]))));
		inputSampleR -= (bR[32] * (0.00407693981307314  - (0.00003935772436894*fabs(bR[32]))));
		inputSampleR -= (bR[33] * (0.00350435348467321  - (0.00005525463935338*fabs(bR[33]))));	
		}
		
		if (fabs(inputSampleL) > threshold)
		{
			bridgerectifier = (fabs(inputSampleL)-threshold)*hardness;
			//skip flat area if any, scale to distortion limit
			if (bridgerectifier > breakup) bridgerectifier = breakup;
			//max value for sine function, 'breakup' modeling for trashed console tone
			//more hardness = more solidness behind breakup modeling. more softness, more 'grunge' and sag
			bridgerectifier = sin(bridgerectifier)/hardness;
			//do the sine factor, scale back to proper amount
			if (inputSampleL > 0) inputSampleL = bridgerectifier+threshold;
			else inputSampleL = -(bridgerectifier+threshold);
		}
		//otherwise we leave it untouched by the overdrive stuff
		if (fabs(inputSampleR) > threshold)
		{
			bridgerectifier = (fabs(inputSampleR)-threshold)*hardness;
			//skip flat area if any, scale to distortion limit
			if (bridgerectifier > breakup) bridgerectifier = breakup;
			//max value for sine function, 'breakup' modeling for trashed console tone
			//more hardness = more solidness behind breakup modeling. more softness, more 'grunge' and sag
			bridgerectifier = sin(bridgerectifier)/hardness;
			//do the sine factor, scale back to proper amount
			if (inputSampleR > 0) inputSampleR = bridgerectifier+threshold;
			else inputSampleR = -(bridgerectifier+threshold);
		}
		//otherwise we leave it untouched by the overdrive stuff
		
		randy = ((rand()/(double)RAND_MAX)*0.033);
		inputSampleL = ((inputSampleL*(1-randy))+(lastSampleL*randy)) * outlevel;
		lastSampleL = inputSampleL;
		
		randy = ((rand()/(double)RAND_MAX)*0.033);
		inputSampleR = ((inputSampleR*(1-randy))+(lastSampleR*randy)) * outlevel;
		lastSampleR = inputSampleR;
		
		
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

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void Apicolypse::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double threshold = A;
	double hardness;
	double breakup = (1.0-(threshold/2.0))*3.14159265358979;
	double bridgerectifier;
	double sqdrive = (B*3.0);
	if (sqdrive > 1.0) sqdrive *= sqdrive;
	sqdrive = sqrt(sqdrive);
	double indrive = C*3.0;
	if (indrive > 1.0) indrive *= indrive;
	indrive *= (1.0-(0.008*sqdrive));
	//correct for gain loss of convolution
	//calibrate this to match noise level with character at 1.0
	//you get for instance 0.819 and 1.0-0.819 is 0.181
	double randy;
	double outlevel = D;
	
	if (threshold < 1) hardness = 1.0 / (1.0-threshold);
	else hardness = 999999999999999999999.0;
	//set up hardness to exactly fill gap between threshold and 0db
	//if threshold is literally 1 then hardness is infinite, so we make it very big
    
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
		if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
		
		inputSampleL *= indrive;
		inputSampleR *= indrive;
		//calibrated to match gain through convolution and -0.3 correction
		
		if (sqdrive > 0.0){
			bL[33] = bL[32]; bL[32] = bL[31]; 
			bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28]; bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; 
			bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; 
			bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7]; 
			bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL * sqdrive;
			
			inputSampleL += (bL[1] * (0.09299870608542582  - (0.00009582362368873*fabs(bL[1]))));
			inputSampleL -= (bL[2] * (0.11947847710741009  - (0.00004500891602770*fabs(bL[2]))));
			inputSampleL += (bL[3] * (0.09071606264761795  + (0.00005639498984741*fabs(bL[3]))));
			inputSampleL -= (bL[4] * (0.08561982770836980  - (0.00004964855606916*fabs(bL[4]))));
			inputSampleL += (bL[5] * (0.06440549220820363  + (0.00002428052139507*fabs(bL[5]))));
			inputSampleL -= (bL[6] * (0.05987991812840746  + (0.00000101867082290*fabs(bL[6]))));
			inputSampleL += (bL[7] * (0.03980233135839382  + (0.00003312430049041*fabs(bL[7]))));
			inputSampleL -= (bL[8] * (0.03648402630896925  - (0.00002116186381142*fabs(bL[8]))));
			inputSampleL += (bL[9] * (0.01826860869525248  + (0.00003115110025396*fabs(bL[9]))));
			inputSampleL -= (bL[10] * (0.01723968622495364  - (0.00002450634121718*fabs(bL[10]))));
			inputSampleL += (bL[11] * (0.00187588812316724  + (0.00002838206198968*fabs(bL[11]))));
			inputSampleL -= (bL[12] * (0.00381796423957237  - (0.00003155815499462*fabs(bL[12]))));
			inputSampleL -= (bL[13] * (0.00852092214496733  - (0.00001702651162392*fabs(bL[13]))));
			inputSampleL += (bL[14] * (0.00315560292270588  + (0.00002547861676047*fabs(bL[14]))));
			inputSampleL -= (bL[15] * (0.01258630914496868  - (0.00004555319243213*fabs(bL[15]))));
			inputSampleL += (bL[16] * (0.00536435648963575  + (0.00001812393657101*fabs(bL[16]))));
			inputSampleL -= (bL[17] * (0.01272975658159178  - (0.00004103775306121*fabs(bL[17]))));
			inputSampleL += (bL[18] * (0.00403818975172755  + (0.00003764615492871*fabs(bL[18]))));
			inputSampleL -= (bL[19] * (0.01042617366897483  - (0.00003605210426041*fabs(bL[19]))));
			inputSampleL += (bL[20] * (0.00126599583390057  + (0.00004305458668852*fabs(bL[20]))));
			inputSampleL -= (bL[21] * (0.00747876207688339  - (0.00003731207018977*fabs(bL[21]))));
			inputSampleL -= (bL[22] * (0.00149873689175324  - (0.00005086601800791*fabs(bL[22]))));
			inputSampleL -= (bL[23] * (0.00503221309488033  - (0.00003636086782783*fabs(bL[23]))));
			inputSampleL -= (bL[24] * (0.00342998224655821  - (0.00004103091180506*fabs(bL[24]))));
			inputSampleL -= (bL[25] * (0.00355585977903117  - (0.00003698982145400*fabs(bL[25]))));
			inputSampleL -= (bL[26] * (0.00437201792934817  - (0.00002720235666939*fabs(bL[26]))));
			inputSampleL -= (bL[27] * (0.00299217874451556  - (0.00004446954727956*fabs(bL[27]))));
			inputSampleL -= (bL[28] * (0.00457924652487249  - (0.00003859065778860*fabs(bL[28]))));
			inputSampleL -= (bL[29] * (0.00298182934892027  - (0.00002064710931733*fabs(bL[29]))));
			inputSampleL -= (bL[30] * (0.00438838441540584  - (0.00005223008424866*fabs(bL[30]))));
			inputSampleL -= (bL[31] * (0.00323984218794705  - (0.00003397987535887*fabs(bL[31]))));
			inputSampleL -= (bL[32] * (0.00407693981307314  - (0.00003935772436894*fabs(bL[32]))));
			inputSampleL -= (bL[33] * (0.00350435348467321  - (0.00005525463935338*fabs(bL[33]))));
			
			bR[33] = bR[32]; bR[32] = bR[31]; 
			bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28]; bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; 
			bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; 
			bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7]; 
			bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR * sqdrive;
			
			inputSampleR += (bR[1] * (0.09299870608542582  - (0.00009582362368873*fabs(bR[1]))));
			inputSampleR -= (bR[2] * (0.11947847710741009  - (0.00004500891602770*fabs(bR[2]))));
			inputSampleR += (bR[3] * (0.09071606264761795  + (0.00005639498984741*fabs(bR[3]))));
			inputSampleR -= (bR[4] * (0.08561982770836980  - (0.00004964855606916*fabs(bR[4]))));
			inputSampleR += (bR[5] * (0.06440549220820363  + (0.00002428052139507*fabs(bR[5]))));
			inputSampleR -= (bR[6] * (0.05987991812840746  + (0.00000101867082290*fabs(bR[6]))));
			inputSampleR += (bR[7] * (0.03980233135839382  + (0.00003312430049041*fabs(bR[7]))));
			inputSampleR -= (bR[8] * (0.03648402630896925  - (0.00002116186381142*fabs(bR[8]))));
			inputSampleR += (bR[9] * (0.01826860869525248  + (0.00003115110025396*fabs(bR[9]))));
			inputSampleR -= (bR[10] * (0.01723968622495364  - (0.00002450634121718*fabs(bR[10]))));
			inputSampleR += (bR[11] * (0.00187588812316724  + (0.00002838206198968*fabs(bR[11]))));
			inputSampleR -= (bR[12] * (0.00381796423957237  - (0.00003155815499462*fabs(bR[12]))));
			inputSampleR -= (bR[13] * (0.00852092214496733  - (0.00001702651162392*fabs(bR[13]))));
			inputSampleR += (bR[14] * (0.00315560292270588  + (0.00002547861676047*fabs(bR[14]))));
			inputSampleR -= (bR[15] * (0.01258630914496868  - (0.00004555319243213*fabs(bR[15]))));
			inputSampleR += (bR[16] * (0.00536435648963575  + (0.00001812393657101*fabs(bR[16]))));
			inputSampleR -= (bR[17] * (0.01272975658159178  - (0.00004103775306121*fabs(bR[17]))));
			inputSampleR += (bR[18] * (0.00403818975172755  + (0.00003764615492871*fabs(bR[18]))));
			inputSampleR -= (bR[19] * (0.01042617366897483  - (0.00003605210426041*fabs(bR[19]))));
			inputSampleR += (bR[20] * (0.00126599583390057  + (0.00004305458668852*fabs(bR[20]))));
			inputSampleR -= (bR[21] * (0.00747876207688339  - (0.00003731207018977*fabs(bR[21]))));
			inputSampleR -= (bR[22] * (0.00149873689175324  - (0.00005086601800791*fabs(bR[22]))));
			inputSampleR -= (bR[23] * (0.00503221309488033  - (0.00003636086782783*fabs(bR[23]))));
			inputSampleR -= (bR[24] * (0.00342998224655821  - (0.00004103091180506*fabs(bR[24]))));
			inputSampleR -= (bR[25] * (0.00355585977903117  - (0.00003698982145400*fabs(bR[25]))));
			inputSampleR -= (bR[26] * (0.00437201792934817  - (0.00002720235666939*fabs(bR[26]))));
			inputSampleR -= (bR[27] * (0.00299217874451556  - (0.00004446954727956*fabs(bR[27]))));
			inputSampleR -= (bR[28] * (0.00457924652487249  - (0.00003859065778860*fabs(bR[28]))));
			inputSampleR -= (bR[29] * (0.00298182934892027  - (0.00002064710931733*fabs(bR[29]))));
			inputSampleR -= (bR[30] * (0.00438838441540584  - (0.00005223008424866*fabs(bR[30]))));
			inputSampleR -= (bR[31] * (0.00323984218794705  - (0.00003397987535887*fabs(bR[31]))));
			inputSampleR -= (bR[32] * (0.00407693981307314  - (0.00003935772436894*fabs(bR[32]))));
			inputSampleR -= (bR[33] * (0.00350435348467321  - (0.00005525463935338*fabs(bR[33]))));	
		}
		
		if (fabs(inputSampleL) > threshold)
		{
			bridgerectifier = (fabs(inputSampleL)-threshold)*hardness;
			//skip flat area if any, scale to distortion limit
			if (bridgerectifier > breakup) bridgerectifier = breakup;
			//max value for sine function, 'breakup' modeling for trashed console tone
			//more hardness = more solidness behind breakup modeling. more softness, more 'grunge' and sag
			bridgerectifier = sin(bridgerectifier)/hardness;
			//do the sine factor, scale back to proper amount
			if (inputSampleL > 0) inputSampleL = bridgerectifier+threshold;
			else inputSampleL = -(bridgerectifier+threshold);
		}
		//otherwise we leave it untouched by the overdrive stuff
		if (fabs(inputSampleR) > threshold)
		{
			bridgerectifier = (fabs(inputSampleR)-threshold)*hardness;
			//skip flat area if any, scale to distortion limit
			if (bridgerectifier > breakup) bridgerectifier = breakup;
			//max value for sine function, 'breakup' modeling for trashed console tone
			//more hardness = more solidness behind breakup modeling. more softness, more 'grunge' and sag
			bridgerectifier = sin(bridgerectifier)/hardness;
			//do the sine factor, scale back to proper amount
			if (inputSampleR > 0) inputSampleR = bridgerectifier+threshold;
			else inputSampleR = -(bridgerectifier+threshold);
		}
		//otherwise we leave it untouched by the overdrive stuff
		
		randy = ((rand()/(double)RAND_MAX)*0.033);
		inputSampleL = ((inputSampleL*(1-randy))+(lastSampleL*randy)) * outlevel;
		lastSampleL = inputSampleL;
		
		randy = ((rand()/(double)RAND_MAX)*0.033);
		inputSampleR = ((inputSampleR*(1-randy))+(lastSampleR*randy)) * outlevel;
		lastSampleR = inputSampleR;
		
		
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

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


} // end namespace Apicolypse

