/* ========================================
 *  StarChild - StarChild.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __StarChild_H
#include "StarChild.h"
#endif

namespace StarChild {


void StarChild::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];


	double drySampleL;
	double drySampleR;
	double inputSampleL;
	double inputSampleR;
	
	int bufferL = 0;
	int bufferR = 0;
	//these are to build up the reverb tank outs
	
	int rangeDirect = (pow(B,2) * 156.0) + 7.0;
	//maximum safe delay is 259 * the prime tap, not including room for the pitch shift offset
	
	float scaleDirect = (pow(A,2) * (3280.0/rangeDirect)) + 2.0;
	//let's try making it always be the max delay: smaller range forces scale to be longer
	
	float outputPad = 4 * rangeDirect * sqrt(rangeDirect);
	
	float overallscale = ((1.0-B)*9.0)+1.0;
	//apply the singlestage groove wear strongest when bits are heavily crushed
	float gain = overallscale;
	if (gain > 1.0) {factor[0] = 1.0; gain -= 1.0;} else {factor[0] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[1] = 1.0; gain -= 1.0;} else {factor[1] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[2] = 1.0; gain -= 1.0;} else {factor[2] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[3] = 1.0; gain -= 1.0;} else {factor[3] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[4] = 1.0; gain -= 1.0;} else {factor[4] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[5] = 1.0; gain -= 1.0;} else {factor[5] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[6] = 1.0; gain -= 1.0;} else {factor[6] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[7] = 1.0; gain -= 1.0;} else {factor[7] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[8] = 1.0; gain -= 1.0;} else {factor[8] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[9] = 1.0; gain -= 1.0;} else {factor[9] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders
	
	if (overallscale < 1.0) overallscale = 1.0;
	factor[0] /= overallscale;
	factor[1] /= overallscale;
	factor[2] /= overallscale;
	factor[3] /= overallscale;
	factor[4] /= overallscale;
	factor[5] /= overallscale;
	factor[6] /= overallscale;
	factor[7] /= overallscale;
	factor[8] /= overallscale;
	factor[9] /= overallscale;
	//and now it's neatly scaled, too
	float accumulatorSample;
	float correction;
	float wetness = C;
	float dryness = 1.0 - wetness;	//reverb setup
	
	int count;
	for(count = 1; count < 165; count++)
	{
		t[count] = p[count] * scaleDirect;
		//this is the scaled tap for direct out, in number of samples delay
	}
	
    
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
		
		//assign working variables like the dry
		
		if (dCount < 0 || dCount > 22050) {dCount = 22050;}
		d[dCount + 22050] = d[dCount] = inputSampleL + inputSampleR;
		dCount--; 
		//feed the delay line with summed channels. The stuff we're reading back
		//will always be plus dCount, because we're counting back to 0.
		
		//now we're going to start pitch shifting.
		dutyCycle += 1;
		if (dutyCycle > scaleDirect) {
			dutyCycle = 1;
			//this whole routine doesn't run every sample, it's making a wacky hypervibrato
			t[pitchCounter] += increment; pitchCounter += 1;
			//pitchCounter always goes up, t[] goes up and down
			//possibly do that not every sample? Let's see what we get
			if (pitchCounter > rangeDirect) {
				if (increment == 1) {
					pitchCounter = 1;
					if (t[1] > ((11 * scaleDirect) + 1000)) increment = -1;
					//let's try hardcoding a big 1000 sample buffer
				}
				else {
					//increment is -1 so we have been counting down!
					pitchCounter = 1;
					
					if (t[1] < (11 * scaleDirect)) {
						//we've scaled everything back so we're going up again
						increment = 1;
						//and we're gonna reset the lot in case of screw-ups (control manipulations)
						for(count = 1; count < 165; count++)
						{
							t[count] = p[count] * scaleDirect;
						}
						//which means we're back to normal and counting up again.
					}
				}
				//wrap around to begin again, and if our first tap is greater than
				//its base value plus scaleDirect, start going down.
			}
		}
		//always wrap around to the first tap		
		
		//Now we do a select case where we jump into the middle of some repetitive math, unrolled.
		bufferL = 0; bufferR = 0;
		//clear before making our delay sound
		switch (rangeDirect) {
			case 164: bufferL += (int)(d[dCount+t[164]]*outL[164]); bufferR += (int)(d[dCount+t[164]]*outR[164]); 
			case 163: bufferL += (int)(d[dCount+t[163]]*outL[163]); bufferR += (int)(d[dCount+t[163]]*outR[163]); 
			case 162: bufferL += (int)(d[dCount+t[162]]*outL[162]); bufferR += (int)(d[dCount+t[162]]*outR[162]); 
			case 161: bufferL += (int)(d[dCount+t[161]]*outL[161]); bufferR += (int)(d[dCount+t[161]]*outR[161]); 
			case 160: bufferL += (int)(d[dCount+t[160]]*outL[160]); bufferR += (int)(d[dCount+t[160]]*outR[160]); 
			case 159: bufferL += (int)(d[dCount+t[159]]*outL[159]); bufferR += (int)(d[dCount+t[159]]*outR[159]); 
			case 158: bufferL += (int)(d[dCount+t[158]]*outL[158]); bufferR += (int)(d[dCount+t[158]]*outR[158]); 
			case 157: bufferL += (int)(d[dCount+t[157]]*outL[157]); bufferR += (int)(d[dCount+t[157]]*outR[157]); 
			case 156: bufferL += (int)(d[dCount+t[156]]*outL[156]); bufferR += (int)(d[dCount+t[156]]*outR[156]); 
			case 155: bufferL += (int)(d[dCount+t[155]]*outL[155]); bufferR += (int)(d[dCount+t[155]]*outR[155]); 
			case 154: bufferL += (int)(d[dCount+t[154]]*outL[154]); bufferR += (int)(d[dCount+t[154]]*outR[154]); 
			case 153: bufferL += (int)(d[dCount+t[153]]*outL[153]); bufferR += (int)(d[dCount+t[153]]*outR[153]); 
			case 152: bufferL += (int)(d[dCount+t[152]]*outL[152]); bufferR += (int)(d[dCount+t[152]]*outR[152]); 
			case 151: bufferL += (int)(d[dCount+t[151]]*outL[151]); bufferR += (int)(d[dCount+t[151]]*outR[151]); 
			case 150: bufferL += (int)(d[dCount+t[150]]*outL[150]); bufferR += (int)(d[dCount+t[150]]*outR[150]);
			case 149: bufferL += (int)(d[dCount+t[149]]*outL[149]); bufferR += (int)(d[dCount+t[149]]*outR[149]); 
			case 148: bufferL += (int)(d[dCount+t[148]]*outL[148]); bufferR += (int)(d[dCount+t[148]]*outR[148]); 
			case 147: bufferL += (int)(d[dCount+t[147]]*outL[147]); bufferR += (int)(d[dCount+t[147]]*outR[147]); 
			case 146: bufferL += (int)(d[dCount+t[146]]*outL[146]); bufferR += (int)(d[dCount+t[146]]*outR[146]); 
			case 145: bufferL += (int)(d[dCount+t[145]]*outL[145]); bufferR += (int)(d[dCount+t[145]]*outR[145]); 
			case 144: bufferL += (int)(d[dCount+t[144]]*outL[144]); bufferR += (int)(d[dCount+t[144]]*outR[144]); 
			case 143: bufferL += (int)(d[dCount+t[143]]*outL[143]); bufferR += (int)(d[dCount+t[143]]*outR[143]); 
			case 142: bufferL += (int)(d[dCount+t[142]]*outL[142]); bufferR += (int)(d[dCount+t[142]]*outR[142]); 
			case 141: bufferL += (int)(d[dCount+t[141]]*outL[141]); bufferR += (int)(d[dCount+t[141]]*outR[141]); 
			case 140: bufferL += (int)(d[dCount+t[140]]*outL[140]); bufferR += (int)(d[dCount+t[140]]*outR[140]); 
			case 139: bufferL += (int)(d[dCount+t[139]]*outL[139]); bufferR += (int)(d[dCount+t[139]]*outR[139]); 
			case 138: bufferL += (int)(d[dCount+t[138]]*outL[138]); bufferR += (int)(d[dCount+t[138]]*outR[138]); 
			case 137: bufferL += (int)(d[dCount+t[137]]*outL[137]); bufferR += (int)(d[dCount+t[137]]*outR[137]); 
			case 136: bufferL += (int)(d[dCount+t[136]]*outL[136]); bufferR += (int)(d[dCount+t[136]]*outR[136]); 
			case 135: bufferL += (int)(d[dCount+t[135]]*outL[135]); bufferR += (int)(d[dCount+t[135]]*outR[135]); 
			case 134: bufferL += (int)(d[dCount+t[134]]*outL[134]); bufferR += (int)(d[dCount+t[134]]*outR[134]); 
			case 133: bufferL += (int)(d[dCount+t[133]]*outL[133]); bufferR += (int)(d[dCount+t[133]]*outR[133]); 
			case 132: bufferL += (int)(d[dCount+t[132]]*outL[132]); bufferR += (int)(d[dCount+t[132]]*outR[132]); 
			case 131: bufferL += (int)(d[dCount+t[131]]*outL[131]); bufferR += (int)(d[dCount+t[131]]*outR[131]); 
			case 130: bufferL += (int)(d[dCount+t[130]]*outL[130]); bufferR += (int)(d[dCount+t[130]]*outR[130]); 
			case 129: bufferL += (int)(d[dCount+t[129]]*outL[129]); bufferR += (int)(d[dCount+t[129]]*outR[129]); 
			case 128: bufferL += (int)(d[dCount+t[128]]*outL[128]); bufferR += (int)(d[dCount+t[128]]*outR[128]); 
			case 127: bufferL += (int)(d[dCount+t[127]]*outL[127]); bufferR += (int)(d[dCount+t[127]]*outR[127]); 
			case 126: bufferL += (int)(d[dCount+t[126]]*outL[126]); bufferR += (int)(d[dCount+t[126]]*outR[126]); 
			case 125: bufferL += (int)(d[dCount+t[125]]*outL[125]); bufferR += (int)(d[dCount+t[125]]*outR[125]); 
			case 124: bufferL += (int)(d[dCount+t[124]]*outL[124]); bufferR += (int)(d[dCount+t[124]]*outR[124]); 
			case 123: bufferL += (int)(d[dCount+t[123]]*outL[123]); bufferR += (int)(d[dCount+t[123]]*outR[123]); 
			case 122: bufferL += (int)(d[dCount+t[122]]*outL[122]); bufferR += (int)(d[dCount+t[122]]*outR[122]); 
			case 121: bufferL += (int)(d[dCount+t[121]]*outL[121]); bufferR += (int)(d[dCount+t[121]]*outR[121]); 
			case 120: bufferL += (int)(d[dCount+t[120]]*outL[120]); bufferR += (int)(d[dCount+t[120]]*outR[120]); 
			case 119: bufferL += (int)(d[dCount+t[119]]*outL[119]); bufferR += (int)(d[dCount+t[119]]*outR[119]); 
			case 118: bufferL += (int)(d[dCount+t[118]]*outL[118]); bufferR += (int)(d[dCount+t[118]]*outR[118]); 
			case 117: bufferL += (int)(d[dCount+t[117]]*outL[117]); bufferR += (int)(d[dCount+t[117]]*outR[117]); 
			case 116: bufferL += (int)(d[dCount+t[116]]*outL[116]); bufferR += (int)(d[dCount+t[116]]*outR[116]); 
			case 115: bufferL += (int)(d[dCount+t[115]]*outL[115]); bufferR += (int)(d[dCount+t[115]]*outR[115]); 
			case 114: bufferL += (int)(d[dCount+t[114]]*outL[114]); bufferR += (int)(d[dCount+t[114]]*outR[114]); 
			case 113: bufferL += (int)(d[dCount+t[113]]*outL[113]); bufferR += (int)(d[dCount+t[113]]*outR[113]); 
			case 112: bufferL += (int)(d[dCount+t[112]]*outL[112]); bufferR += (int)(d[dCount+t[112]]*outR[112]); 
			case 111: bufferL += (int)(d[dCount+t[111]]*outL[111]); bufferR += (int)(d[dCount+t[111]]*outR[111]); 
			case 110: bufferL += (int)(d[dCount+t[110]]*outL[110]); bufferR += (int)(d[dCount+t[110]]*outR[110]); 
			case 109: bufferL += (int)(d[dCount+t[109]]*outL[109]); bufferR += (int)(d[dCount+t[109]]*outR[109]); 
			case 108: bufferL += (int)(d[dCount+t[108]]*outL[108]); bufferR += (int)(d[dCount+t[108]]*outR[108]); 
			case 107: bufferL += (int)(d[dCount+t[107]]*outL[107]); bufferR += (int)(d[dCount+t[107]]*outR[107]); 
			case 106: bufferL += (int)(d[dCount+t[106]]*outL[106]); bufferR += (int)(d[dCount+t[106]]*outR[106]); 
			case 105: bufferL += (int)(d[dCount+t[105]]*outL[105]); bufferR += (int)(d[dCount+t[105]]*outR[105]); 
			case 104: bufferL += (int)(d[dCount+t[104]]*outL[104]); bufferR += (int)(d[dCount+t[104]]*outR[104]); 
			case 103: bufferL += (int)(d[dCount+t[103]]*outL[103]); bufferR += (int)(d[dCount+t[103]]*outR[103]); 
			case 102: bufferL += (int)(d[dCount+t[102]]*outL[102]); bufferR += (int)(d[dCount+t[102]]*outR[102]); 
			case 101: bufferL += (int)(d[dCount+t[101]]*outL[101]); bufferR += (int)(d[dCount+t[101]]*outR[101]); 
			case 100: bufferL += (int)(d[dCount+t[100]]*outL[100]); bufferR += (int)(d[dCount+t[100]]*outR[100]); 
			case  99: bufferL += (int)(d[dCount+t[ 99]]*outL[ 99]); bufferR += (int)(d[dCount+t[ 99]]*outR[ 99]); 
			case  98: bufferL += (int)(d[dCount+t[ 98]]*outL[ 98]); bufferR += (int)(d[dCount+t[ 98]]*outR[ 98]); 
			case  97: bufferL += (int)(d[dCount+t[ 97]]*outL[ 97]); bufferR += (int)(d[dCount+t[ 97]]*outR[ 97]); 
			case  96: bufferL += (int)(d[dCount+t[ 96]]*outL[ 96]); bufferR += (int)(d[dCount+t[ 96]]*outR[ 96]); 
			case  95: bufferL += (int)(d[dCount+t[ 95]]*outL[ 95]); bufferR += (int)(d[dCount+t[ 95]]*outR[ 95]); 
			case  94: bufferL += (int)(d[dCount+t[ 94]]*outL[ 94]); bufferR += (int)(d[dCount+t[ 94]]*outR[ 94]); 
			case  93: bufferL += (int)(d[dCount+t[ 93]]*outL[ 93]); bufferR += (int)(d[dCount+t[ 93]]*outR[ 93]); 
			case  92: bufferL += (int)(d[dCount+t[ 92]]*outL[ 92]); bufferR += (int)(d[dCount+t[ 92]]*outR[ 92]); 
			case  91: bufferL += (int)(d[dCount+t[ 91]]*outL[ 91]); bufferR += (int)(d[dCount+t[ 91]]*outR[ 91]); 
			case  90: bufferL += (int)(d[dCount+t[ 90]]*outL[ 90]); bufferR += (int)(d[dCount+t[ 90]]*outR[ 90]); 
			case  89: bufferL += (int)(d[dCount+t[ 89]]*outL[ 89]); bufferR += (int)(d[dCount+t[ 89]]*outR[ 89]); 
			case  88: bufferL += (int)(d[dCount+t[ 88]]*outL[ 88]); bufferR += (int)(d[dCount+t[ 88]]*outR[ 88]); 
			case  87: bufferL += (int)(d[dCount+t[ 87]]*outL[ 87]); bufferR += (int)(d[dCount+t[ 87]]*outR[ 87]); 
			case  86: bufferL += (int)(d[dCount+t[ 86]]*outL[ 86]); bufferR += (int)(d[dCount+t[ 86]]*outR[ 86]); 
			case  85: bufferL += (int)(d[dCount+t[ 85]]*outL[ 85]); bufferR += (int)(d[dCount+t[ 85]]*outR[ 85]); 
			case  84: bufferL += (int)(d[dCount+t[ 84]]*outL[ 84]); bufferR += (int)(d[dCount+t[ 84]]*outR[ 84]); 
			case  83: bufferL += (int)(d[dCount+t[ 83]]*outL[ 83]); bufferR += (int)(d[dCount+t[ 83]]*outR[ 83]); 
			case  82: bufferL += (int)(d[dCount+t[ 82]]*outL[ 82]); bufferR += (int)(d[dCount+t[ 82]]*outR[ 82]); 
			case  81: bufferL += (int)(d[dCount+t[ 81]]*outL[ 81]); bufferR += (int)(d[dCount+t[ 81]]*outR[ 81]); 
			case  80: bufferL += (int)(d[dCount+t[ 80]]*outL[ 80]); bufferR += (int)(d[dCount+t[ 80]]*outR[ 80]); 
			case  79: bufferL += (int)(d[dCount+t[ 79]]*outL[ 79]); bufferR += (int)(d[dCount+t[ 79]]*outR[ 79]); 
			case  78: bufferL += (int)(d[dCount+t[ 78]]*outL[ 78]); bufferR += (int)(d[dCount+t[ 78]]*outR[ 78]); 
			case  77: bufferL += (int)(d[dCount+t[ 77]]*outL[ 77]); bufferR += (int)(d[dCount+t[ 77]]*outR[ 77]); 
			case  76: bufferL += (int)(d[dCount+t[ 76]]*outL[ 76]); bufferR += (int)(d[dCount+t[ 76]]*outR[ 76]); 
			case  75: bufferL += (int)(d[dCount+t[ 75]]*outL[ 75]); bufferR += (int)(d[dCount+t[ 75]]*outR[ 75]); 
			case  74: bufferL += (int)(d[dCount+t[ 74]]*outL[ 74]); bufferR += (int)(d[dCount+t[ 74]]*outR[ 74]); 
			case  73: bufferL += (int)(d[dCount+t[ 73]]*outL[ 73]); bufferR += (int)(d[dCount+t[ 73]]*outR[ 73]); 
			case  72: bufferL += (int)(d[dCount+t[ 72]]*outL[ 72]); bufferR += (int)(d[dCount+t[ 72]]*outR[ 72]); 
			case  71: bufferL += (int)(d[dCount+t[ 71]]*outL[ 71]); bufferR += (int)(d[dCount+t[ 71]]*outR[ 71]); 
			case  70: bufferL += (int)(d[dCount+t[ 70]]*outL[ 70]); bufferR += (int)(d[dCount+t[ 70]]*outR[ 70]); 
			case  69: bufferL += (int)(d[dCount+t[ 69]]*outL[ 69]); bufferR += (int)(d[dCount+t[ 69]]*outR[ 69]); 
			case  68: bufferL += (int)(d[dCount+t[ 68]]*outL[ 68]); bufferR += (int)(d[dCount+t[ 68]]*outR[ 68]); 
			case  67: bufferL += (int)(d[dCount+t[ 67]]*outL[ 67]); bufferR += (int)(d[dCount+t[ 67]]*outR[ 67]); 
			case  66: bufferL += (int)(d[dCount+t[ 66]]*outL[ 66]); bufferR += (int)(d[dCount+t[ 66]]*outR[ 66]); 
			case  65: bufferL += (int)(d[dCount+t[ 65]]*outL[ 65]); bufferR += (int)(d[dCount+t[ 65]]*outR[ 65]); 
			case  64: bufferL += (int)(d[dCount+t[ 64]]*outL[ 64]); bufferR += (int)(d[dCount+t[ 64]]*outR[ 64]); 
			case  63: bufferL += (int)(d[dCount+t[ 63]]*outL[ 63]); bufferR += (int)(d[dCount+t[ 63]]*outR[ 63]); 
			case  62: bufferL += (int)(d[dCount+t[ 62]]*outL[ 62]); bufferR += (int)(d[dCount+t[ 62]]*outR[ 62]); 
			case  61: bufferL += (int)(d[dCount+t[ 61]]*outL[ 61]); bufferR += (int)(d[dCount+t[ 61]]*outR[ 61]); 
			case  60: bufferL += (int)(d[dCount+t[ 60]]*outL[ 60]); bufferR += (int)(d[dCount+t[ 60]]*outR[ 60]); 
			case  59: bufferL += (int)(d[dCount+t[ 59]]*outL[ 59]); bufferR += (int)(d[dCount+t[ 59]]*outR[ 59]); 
			case  58: bufferL += (int)(d[dCount+t[ 58]]*outL[ 58]); bufferR += (int)(d[dCount+t[ 58]]*outR[ 58]); 
			case  57: bufferL += (int)(d[dCount+t[ 57]]*outL[ 57]); bufferR += (int)(d[dCount+t[ 57]]*outR[ 57]); 
			case  56: bufferL += (int)(d[dCount+t[ 56]]*outL[ 56]); bufferR += (int)(d[dCount+t[ 56]]*outR[ 56]); 
			case  55: bufferL += (int)(d[dCount+t[ 55]]*outL[ 55]); bufferR += (int)(d[dCount+t[ 55]]*outR[ 55]); 
			case  54: bufferL += (int)(d[dCount+t[ 54]]*outL[ 54]); bufferR += (int)(d[dCount+t[ 54]]*outR[ 54]); 
			case  53: bufferL += (int)(d[dCount+t[ 53]]*outL[ 53]); bufferR += (int)(d[dCount+t[ 53]]*outR[ 53]); 
			case  52: bufferL += (int)(d[dCount+t[ 52]]*outL[ 52]); bufferR += (int)(d[dCount+t[ 52]]*outR[ 52]); 
			case  51: bufferL += (int)(d[dCount+t[ 51]]*outL[ 51]); bufferR += (int)(d[dCount+t[ 51]]*outR[ 51]); 
			case  50: bufferL += (int)(d[dCount+t[ 50]]*outL[ 50]); bufferR += (int)(d[dCount+t[ 50]]*outR[ 50]); 
			case  49: bufferL += (int)(d[dCount+t[ 49]]*outL[ 49]); bufferR += (int)(d[dCount+t[ 49]]*outR[ 49]); 
			case  48: bufferL += (int)(d[dCount+t[ 48]]*outL[ 48]); bufferR += (int)(d[dCount+t[ 48]]*outR[ 48]); 
			case  47: bufferL += (int)(d[dCount+t[ 47]]*outL[ 47]); bufferR += (int)(d[dCount+t[ 47]]*outR[ 47]); 
			case  46: bufferL += (int)(d[dCount+t[ 46]]*outL[ 46]); bufferR += (int)(d[dCount+t[ 46]]*outR[ 46]); 
			case  45: bufferL += (int)(d[dCount+t[ 45]]*outL[ 45]); bufferR += (int)(d[dCount+t[ 45]]*outR[ 45]); 
			case  44: bufferL += (int)(d[dCount+t[ 44]]*outL[ 44]); bufferR += (int)(d[dCount+t[ 44]]*outR[ 44]); 
			case  43: bufferL += (int)(d[dCount+t[ 43]]*outL[ 43]); bufferR += (int)(d[dCount+t[ 43]]*outR[ 43]); 
			case  42: bufferL += (int)(d[dCount+t[ 42]]*outL[ 42]); bufferR += (int)(d[dCount+t[ 42]]*outR[ 42]); 
			case  41: bufferL += (int)(d[dCount+t[ 41]]*outL[ 41]); bufferR += (int)(d[dCount+t[ 41]]*outR[ 41]); 
			case  40: bufferL += (int)(d[dCount+t[ 40]]*outL[ 40]); bufferR += (int)(d[dCount+t[ 40]]*outR[ 40]); 
			case  39: bufferL += (int)(d[dCount+t[ 39]]*outL[ 39]); bufferR += (int)(d[dCount+t[ 39]]*outR[ 39]); 
			case  38: bufferL += (int)(d[dCount+t[ 38]]*outL[ 38]); bufferR += (int)(d[dCount+t[ 38]]*outR[ 38]); 
			case  37: bufferL += (int)(d[dCount+t[ 37]]*outL[ 37]); bufferR += (int)(d[dCount+t[ 37]]*outR[ 37]); 
			case  36: bufferL += (int)(d[dCount+t[ 36]]*outL[ 36]); bufferR += (int)(d[dCount+t[ 36]]*outR[ 36]); 
			case  35: bufferL += (int)(d[dCount+t[ 35]]*outL[ 35]); bufferR += (int)(d[dCount+t[ 35]]*outR[ 35]); 
			case  34: bufferL += (int)(d[dCount+t[ 34]]*outL[ 34]); bufferR += (int)(d[dCount+t[ 34]]*outR[ 34]); 
			case  33: bufferL += (int)(d[dCount+t[ 33]]*outL[ 33]); bufferR += (int)(d[dCount+t[ 33]]*outR[ 33]); 
			case  32: bufferL += (int)(d[dCount+t[ 32]]*outL[ 32]); bufferR += (int)(d[dCount+t[ 32]]*outR[ 32]); 
			case  31: bufferL += (int)(d[dCount+t[ 31]]*outL[ 31]); bufferR += (int)(d[dCount+t[ 31]]*outR[ 31]); 
			case  30: bufferL += (int)(d[dCount+t[ 30]]*outL[ 30]); bufferR += (int)(d[dCount+t[ 30]]*outR[ 30]);
			case  29: bufferL += (int)(d[dCount+t[ 29]]*outL[ 29]); bufferR += (int)(d[dCount+t[ 29]]*outR[ 29]); 
			case  28: bufferL += (int)(d[dCount+t[ 28]]*outL[ 28]); bufferR += (int)(d[dCount+t[ 28]]*outR[ 28]); 
			case  27: bufferL += (int)(d[dCount+t[ 27]]*outL[ 27]); bufferR += (int)(d[dCount+t[ 27]]*outR[ 27]); 
			case  26: bufferL += (int)(d[dCount+t[ 26]]*outL[ 26]); bufferR += (int)(d[dCount+t[ 26]]*outR[ 26]); 
			case  25: bufferL += (int)(d[dCount+t[ 25]]*outL[ 25]); bufferR += (int)(d[dCount+t[ 25]]*outR[ 25]); 
			case  24: bufferL += (int)(d[dCount+t[ 24]]*outL[ 24]); bufferR += (int)(d[dCount+t[ 24]]*outR[ 24]); 
			case  23: bufferL += (int)(d[dCount+t[ 23]]*outL[ 23]); bufferR += (int)(d[dCount+t[ 23]]*outR[ 23]); 
			case  22: bufferL += (int)(d[dCount+t[ 22]]*outL[ 22]); bufferR += (int)(d[dCount+t[ 22]]*outR[ 22]); 
			case  21: bufferL += (int)(d[dCount+t[ 21]]*outL[ 21]); bufferR += (int)(d[dCount+t[ 21]]*outR[ 21]); 
			case  20: bufferL += (int)(d[dCount+t[ 20]]*outL[ 20]); bufferR += (int)(d[dCount+t[ 20]]*outR[ 20]); 
			case  19: bufferL += (int)(d[dCount+t[ 19]]*outL[ 19]); bufferR += (int)(d[dCount+t[ 19]]*outR[ 19]); 
			case  18: bufferL += (int)(d[dCount+t[ 18]]*outL[ 18]); bufferR += (int)(d[dCount+t[ 18]]*outR[ 18]); 
			case  17: bufferL += (int)(d[dCount+t[ 17]]*outL[ 17]); bufferR += (int)(d[dCount+t[ 17]]*outR[ 17]); 
			case  16: bufferL += (int)(d[dCount+t[ 16]]*outL[ 16]); bufferR += (int)(d[dCount+t[ 16]]*outR[ 16]); 
			case  15: bufferL += (int)(d[dCount+t[ 15]]*outL[ 15]); bufferR += (int)(d[dCount+t[ 15]]*outR[ 15]); 
			case  14: bufferL += (int)(d[dCount+t[ 14]]*outL[ 14]); bufferR += (int)(d[dCount+t[ 14]]*outR[ 14]); 
			case  13: bufferL += (int)(d[dCount+t[ 13]]*outL[ 13]); bufferR += (int)(d[dCount+t[ 13]]*outR[ 13]); 
			case  12: bufferL += (int)(d[dCount+t[ 12]]*outL[ 12]); bufferR += (int)(d[dCount+t[ 12]]*outR[ 12]); 
			case  11: bufferL += (int)(d[dCount+t[ 11]]*outL[ 11]); bufferR += (int)(d[dCount+t[ 11]]*outR[ 11]); 
			case  10: bufferL += (int)(d[dCount+t[ 10]]*outL[ 10]); bufferR += (int)(d[dCount+t[ 10]]*outR[ 10]); 
			case   9: bufferL += (int)(d[dCount+t[  9]]*outL[  9]); bufferR += (int)(d[dCount+t[  9]]*outR[  9]); 
			case   8: bufferL += (int)(d[dCount+t[  8]]*outL[  8]); bufferR += (int)(d[dCount+t[  8]]*outR[  8]); 
			case   7: bufferL += (int)(d[dCount+t[  7]]*outL[  7]); bufferR += (int)(d[dCount+t[  7]]*outR[  7]); 
			case   6: bufferL += (int)(d[dCount+t[  6]]*outL[  6]); bufferR += (int)(d[dCount+t[  6]]*outR[  6]); 
			case   5: bufferL += (int)(d[dCount+t[  5]]*outL[  5]); bufferR += (int)(d[dCount+t[  5]]*outR[  5]); 
			case   4: bufferL += (int)(d[dCount+t[  4]]*outL[  4]); bufferR += (int)(d[dCount+t[  4]]*outR[  4]);
			case   3: bufferL += (int)(d[dCount+t[  3]]*outL[  3]); bufferR += (int)(d[dCount+t[  3]]*outR[  3]);
			case   2: bufferL += (int)(d[dCount+t[  2]]*outL[  2]); bufferR += (int)(d[dCount+t[  2]]*outR[  2]);
			case   1: bufferL += (int)(d[dCount+t[  1]]*outL[  1]); bufferR += (int)(d[dCount+t[  1]]*outR[  1]);
		}
		//test to see that delay is working at all: it will be a big stack of case with no break
		
		inputSampleL = bufferL;
		inputSampleR = bufferR;
		//scale back the reverb buffers based on how big of a range we used
		
		
		wearR[9] = wearR[8]; wearR[8] = wearR[7]; wearR[7] = wearR[6]; wearR[6] = wearR[5];
		wearR[5] = wearR[4]; wearR[4] = wearR[3]; wearR[3] = wearR[2]; wearR[2] = wearR[1];
		wearR[1] = wearR[0]; wearR[0] = accumulatorSample = (inputSampleR-wearRPrev);
		
		accumulatorSample *= factor[0];
		accumulatorSample += (wearR[1] * factor[1]);
		accumulatorSample += (wearR[2] * factor[2]);
		accumulatorSample += (wearR[3] * factor[3]);
		accumulatorSample += (wearR[4] * factor[4]);
		accumulatorSample += (wearR[5] * factor[5]);
		accumulatorSample += (wearR[6] * factor[6]);
		accumulatorSample += (wearR[7] * factor[7]);
		accumulatorSample += (wearR[8] * factor[8]);
		accumulatorSample += (wearR[9] * factor[9]);
		//we are doing our repetitive calculations on a separate value
		correction = (inputSampleR-wearRPrev) + accumulatorSample;
		wearRPrev = inputSampleR;		
		inputSampleR += correction;
		
		wearL[9] = wearL[8]; wearL[8] = wearL[7]; wearL[7] = wearL[6]; wearL[6] = wearL[5];
		wearL[5] = wearL[4]; wearL[4] = wearL[3]; wearL[3] = wearL[2]; wearL[2] = wearL[1];
		wearL[1] = wearL[0]; wearL[0] = accumulatorSample = (inputSampleL-wearLPrev);
		
		accumulatorSample *= factor[0];
		accumulatorSample += (wearL[1] * factor[1]);
		accumulatorSample += (wearL[2] * factor[2]);
		accumulatorSample += (wearL[3] * factor[3]);
		accumulatorSample += (wearL[4] * factor[4]);
		accumulatorSample += (wearL[5] * factor[5]);
		accumulatorSample += (wearL[6] * factor[6]);
		accumulatorSample += (wearL[7] * factor[7]);
		accumulatorSample += (wearL[8] * factor[8]);
		accumulatorSample += (wearL[9] * factor[9]);
		//we are doing our repetitive calculations on a separate value		
		correction = (inputSampleL-wearLPrev) + accumulatorSample;
		wearLPrev = inputSampleL;		
		inputSampleL += correction;
		//completed Groove Wear section
		
		inputSampleL /= outputPad;
		inputSampleR /= outputPad;
		
		//back to previous plugin
		drySampleL *= dryness;
		drySampleR *= dryness;
		
		inputSampleL *= wetness;
		inputSampleR *= wetness;
		
		inputSampleL += drySampleL;
		inputSampleR += drySampleR;
		//here we combine the tanks with the dry signal
		
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

void StarChild::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];


	double drySampleL;
	double drySampleR;
	double inputSampleL;
	double inputSampleR;
	
	int bufferL = 0;
	int bufferR = 0;
	//these are to build up the reverb tank outs
	
	int rangeDirect = (pow(B,2) * 156.0) + 7.0;
	//maximum safe delay is 259 * the prime tap, not including room for the pitch shift offset
	
	float scaleDirect = (pow(A,2) * (3280.0/rangeDirect)) + 2.0;
	//let's try making it always be the max delay: smaller range forces scale to be longer
	
	float outputPad = 4 * rangeDirect * sqrt(rangeDirect);
	
	float overallscale = ((1.0-B)*9.0)+1.0;
	//apply the singlestage groove wear strongest when bits are heavily crushed
	float gain = overallscale;
	if (gain > 1.0) {factor[0] = 1.0; gain -= 1.0;} else {factor[0] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[1] = 1.0; gain -= 1.0;} else {factor[1] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[2] = 1.0; gain -= 1.0;} else {factor[2] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[3] = 1.0; gain -= 1.0;} else {factor[3] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[4] = 1.0; gain -= 1.0;} else {factor[4] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[5] = 1.0; gain -= 1.0;} else {factor[5] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[6] = 1.0; gain -= 1.0;} else {factor[6] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[7] = 1.0; gain -= 1.0;} else {factor[7] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[8] = 1.0; gain -= 1.0;} else {factor[8] = gain; gain = 0.0;}
	if (gain > 1.0) {factor[9] = 1.0; gain -= 1.0;} else {factor[9] = gain; gain = 0.0;}
	//there, now we have a neat little moving average with remainders
	
	if (overallscale < 1.0) overallscale = 1.0;
	factor[0] /= overallscale;
	factor[1] /= overallscale;
	factor[2] /= overallscale;
	factor[3] /= overallscale;
	factor[4] /= overallscale;
	factor[5] /= overallscale;
	factor[6] /= overallscale;
	factor[7] /= overallscale;
	factor[8] /= overallscale;
	factor[9] /= overallscale;
	//and now it's neatly scaled, too
	float accumulatorSample;
	float correction;
	float wetness = C;
	float dryness = 1.0 - wetness;	//reverb setup
	
	int count;
	for(count = 1; count < 165; count++)
	{
		t[count] = p[count] * scaleDirect;
		//this is the scaled tap for direct out, in number of samples delay
	}
	
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
		
		
		//assign working variables like the dry
		
		if (dCount < 0 || dCount > 22050) {dCount = 22050;}
		d[dCount + 22050] = d[dCount] = inputSampleL + inputSampleR;
		dCount--; 
		//feed the delay line with summed channels. The stuff we're reading back
		//will always be plus dCount, because we're counting back to 0.
		
		//now we're going to start pitch shifting.
		dutyCycle += 1;
		if (dutyCycle > scaleDirect) {
			dutyCycle = 1;
			//this whole routine doesn't run every sample, it's making a wacky hypervibrato
			t[pitchCounter] += increment; pitchCounter += 1;
			//pitchCounter always goes up, t[] goes up and down
			//possibly do that not every sample? Let's see what we get
			if (pitchCounter > rangeDirect) {
				if (increment == 1) {
					pitchCounter = 1;
					if (t[1] > ((11 * scaleDirect) + 1000)) increment = -1;
					//let's try hardcoding a big 1000 sample buffer
				}
				else {
					//increment is -1 so we have been counting down!
					pitchCounter = 1;
					
					if (t[1] < (11 * scaleDirect)) {
						//we've scaled everything back so we're going up again
						increment = 1;
						//and we're gonna reset the lot in case of screw-ups (control manipulations)
						for(count = 1; count < 165; count++)
						{
							t[count] = p[count] * scaleDirect;
						}
						//which means we're back to normal and counting up again.
					}
				}
				//wrap around to begin again, and if our first tap is greater than
				//its base value plus scaleDirect, start going down.
			}
		}
		//always wrap around to the first tap		
		
		//Now we do a select case where we jump into the middle of some repetitive math, unrolled.
		bufferL = 0; bufferR = 0;
		//clear before making our delay sound
		switch (rangeDirect) {
			case 164: bufferL += (int)(d[dCount+t[164]]*outL[164]); bufferR += (int)(d[dCount+t[164]]*outR[164]); 
			case 163: bufferL += (int)(d[dCount+t[163]]*outL[163]); bufferR += (int)(d[dCount+t[163]]*outR[163]); 
			case 162: bufferL += (int)(d[dCount+t[162]]*outL[162]); bufferR += (int)(d[dCount+t[162]]*outR[162]); 
			case 161: bufferL += (int)(d[dCount+t[161]]*outL[161]); bufferR += (int)(d[dCount+t[161]]*outR[161]); 
			case 160: bufferL += (int)(d[dCount+t[160]]*outL[160]); bufferR += (int)(d[dCount+t[160]]*outR[160]); 
			case 159: bufferL += (int)(d[dCount+t[159]]*outL[159]); bufferR += (int)(d[dCount+t[159]]*outR[159]); 
			case 158: bufferL += (int)(d[dCount+t[158]]*outL[158]); bufferR += (int)(d[dCount+t[158]]*outR[158]); 
			case 157: bufferL += (int)(d[dCount+t[157]]*outL[157]); bufferR += (int)(d[dCount+t[157]]*outR[157]); 
			case 156: bufferL += (int)(d[dCount+t[156]]*outL[156]); bufferR += (int)(d[dCount+t[156]]*outR[156]); 
			case 155: bufferL += (int)(d[dCount+t[155]]*outL[155]); bufferR += (int)(d[dCount+t[155]]*outR[155]); 
			case 154: bufferL += (int)(d[dCount+t[154]]*outL[154]); bufferR += (int)(d[dCount+t[154]]*outR[154]); 
			case 153: bufferL += (int)(d[dCount+t[153]]*outL[153]); bufferR += (int)(d[dCount+t[153]]*outR[153]); 
			case 152: bufferL += (int)(d[dCount+t[152]]*outL[152]); bufferR += (int)(d[dCount+t[152]]*outR[152]); 
			case 151: bufferL += (int)(d[dCount+t[151]]*outL[151]); bufferR += (int)(d[dCount+t[151]]*outR[151]); 
			case 150: bufferL += (int)(d[dCount+t[150]]*outL[150]); bufferR += (int)(d[dCount+t[150]]*outR[150]);
			case 149: bufferL += (int)(d[dCount+t[149]]*outL[149]); bufferR += (int)(d[dCount+t[149]]*outR[149]); 
			case 148: bufferL += (int)(d[dCount+t[148]]*outL[148]); bufferR += (int)(d[dCount+t[148]]*outR[148]); 
			case 147: bufferL += (int)(d[dCount+t[147]]*outL[147]); bufferR += (int)(d[dCount+t[147]]*outR[147]); 
			case 146: bufferL += (int)(d[dCount+t[146]]*outL[146]); bufferR += (int)(d[dCount+t[146]]*outR[146]); 
			case 145: bufferL += (int)(d[dCount+t[145]]*outL[145]); bufferR += (int)(d[dCount+t[145]]*outR[145]); 
			case 144: bufferL += (int)(d[dCount+t[144]]*outL[144]); bufferR += (int)(d[dCount+t[144]]*outR[144]); 
			case 143: bufferL += (int)(d[dCount+t[143]]*outL[143]); bufferR += (int)(d[dCount+t[143]]*outR[143]); 
			case 142: bufferL += (int)(d[dCount+t[142]]*outL[142]); bufferR += (int)(d[dCount+t[142]]*outR[142]); 
			case 141: bufferL += (int)(d[dCount+t[141]]*outL[141]); bufferR += (int)(d[dCount+t[141]]*outR[141]); 
			case 140: bufferL += (int)(d[dCount+t[140]]*outL[140]); bufferR += (int)(d[dCount+t[140]]*outR[140]); 
			case 139: bufferL += (int)(d[dCount+t[139]]*outL[139]); bufferR += (int)(d[dCount+t[139]]*outR[139]); 
			case 138: bufferL += (int)(d[dCount+t[138]]*outL[138]); bufferR += (int)(d[dCount+t[138]]*outR[138]); 
			case 137: bufferL += (int)(d[dCount+t[137]]*outL[137]); bufferR += (int)(d[dCount+t[137]]*outR[137]); 
			case 136: bufferL += (int)(d[dCount+t[136]]*outL[136]); bufferR += (int)(d[dCount+t[136]]*outR[136]); 
			case 135: bufferL += (int)(d[dCount+t[135]]*outL[135]); bufferR += (int)(d[dCount+t[135]]*outR[135]); 
			case 134: bufferL += (int)(d[dCount+t[134]]*outL[134]); bufferR += (int)(d[dCount+t[134]]*outR[134]); 
			case 133: bufferL += (int)(d[dCount+t[133]]*outL[133]); bufferR += (int)(d[dCount+t[133]]*outR[133]); 
			case 132: bufferL += (int)(d[dCount+t[132]]*outL[132]); bufferR += (int)(d[dCount+t[132]]*outR[132]); 
			case 131: bufferL += (int)(d[dCount+t[131]]*outL[131]); bufferR += (int)(d[dCount+t[131]]*outR[131]); 
			case 130: bufferL += (int)(d[dCount+t[130]]*outL[130]); bufferR += (int)(d[dCount+t[130]]*outR[130]); 
			case 129: bufferL += (int)(d[dCount+t[129]]*outL[129]); bufferR += (int)(d[dCount+t[129]]*outR[129]); 
			case 128: bufferL += (int)(d[dCount+t[128]]*outL[128]); bufferR += (int)(d[dCount+t[128]]*outR[128]); 
			case 127: bufferL += (int)(d[dCount+t[127]]*outL[127]); bufferR += (int)(d[dCount+t[127]]*outR[127]); 
			case 126: bufferL += (int)(d[dCount+t[126]]*outL[126]); bufferR += (int)(d[dCount+t[126]]*outR[126]); 
			case 125: bufferL += (int)(d[dCount+t[125]]*outL[125]); bufferR += (int)(d[dCount+t[125]]*outR[125]); 
			case 124: bufferL += (int)(d[dCount+t[124]]*outL[124]); bufferR += (int)(d[dCount+t[124]]*outR[124]); 
			case 123: bufferL += (int)(d[dCount+t[123]]*outL[123]); bufferR += (int)(d[dCount+t[123]]*outR[123]); 
			case 122: bufferL += (int)(d[dCount+t[122]]*outL[122]); bufferR += (int)(d[dCount+t[122]]*outR[122]); 
			case 121: bufferL += (int)(d[dCount+t[121]]*outL[121]); bufferR += (int)(d[dCount+t[121]]*outR[121]); 
			case 120: bufferL += (int)(d[dCount+t[120]]*outL[120]); bufferR += (int)(d[dCount+t[120]]*outR[120]); 
			case 119: bufferL += (int)(d[dCount+t[119]]*outL[119]); bufferR += (int)(d[dCount+t[119]]*outR[119]); 
			case 118: bufferL += (int)(d[dCount+t[118]]*outL[118]); bufferR += (int)(d[dCount+t[118]]*outR[118]); 
			case 117: bufferL += (int)(d[dCount+t[117]]*outL[117]); bufferR += (int)(d[dCount+t[117]]*outR[117]); 
			case 116: bufferL += (int)(d[dCount+t[116]]*outL[116]); bufferR += (int)(d[dCount+t[116]]*outR[116]); 
			case 115: bufferL += (int)(d[dCount+t[115]]*outL[115]); bufferR += (int)(d[dCount+t[115]]*outR[115]); 
			case 114: bufferL += (int)(d[dCount+t[114]]*outL[114]); bufferR += (int)(d[dCount+t[114]]*outR[114]); 
			case 113: bufferL += (int)(d[dCount+t[113]]*outL[113]); bufferR += (int)(d[dCount+t[113]]*outR[113]); 
			case 112: bufferL += (int)(d[dCount+t[112]]*outL[112]); bufferR += (int)(d[dCount+t[112]]*outR[112]); 
			case 111: bufferL += (int)(d[dCount+t[111]]*outL[111]); bufferR += (int)(d[dCount+t[111]]*outR[111]); 
			case 110: bufferL += (int)(d[dCount+t[110]]*outL[110]); bufferR += (int)(d[dCount+t[110]]*outR[110]); 
			case 109: bufferL += (int)(d[dCount+t[109]]*outL[109]); bufferR += (int)(d[dCount+t[109]]*outR[109]); 
			case 108: bufferL += (int)(d[dCount+t[108]]*outL[108]); bufferR += (int)(d[dCount+t[108]]*outR[108]); 
			case 107: bufferL += (int)(d[dCount+t[107]]*outL[107]); bufferR += (int)(d[dCount+t[107]]*outR[107]); 
			case 106: bufferL += (int)(d[dCount+t[106]]*outL[106]); bufferR += (int)(d[dCount+t[106]]*outR[106]); 
			case 105: bufferL += (int)(d[dCount+t[105]]*outL[105]); bufferR += (int)(d[dCount+t[105]]*outR[105]); 
			case 104: bufferL += (int)(d[dCount+t[104]]*outL[104]); bufferR += (int)(d[dCount+t[104]]*outR[104]); 
			case 103: bufferL += (int)(d[dCount+t[103]]*outL[103]); bufferR += (int)(d[dCount+t[103]]*outR[103]); 
			case 102: bufferL += (int)(d[dCount+t[102]]*outL[102]); bufferR += (int)(d[dCount+t[102]]*outR[102]); 
			case 101: bufferL += (int)(d[dCount+t[101]]*outL[101]); bufferR += (int)(d[dCount+t[101]]*outR[101]); 
			case 100: bufferL += (int)(d[dCount+t[100]]*outL[100]); bufferR += (int)(d[dCount+t[100]]*outR[100]); 
			case  99: bufferL += (int)(d[dCount+t[ 99]]*outL[ 99]); bufferR += (int)(d[dCount+t[ 99]]*outR[ 99]); 
			case  98: bufferL += (int)(d[dCount+t[ 98]]*outL[ 98]); bufferR += (int)(d[dCount+t[ 98]]*outR[ 98]); 
			case  97: bufferL += (int)(d[dCount+t[ 97]]*outL[ 97]); bufferR += (int)(d[dCount+t[ 97]]*outR[ 97]); 
			case  96: bufferL += (int)(d[dCount+t[ 96]]*outL[ 96]); bufferR += (int)(d[dCount+t[ 96]]*outR[ 96]); 
			case  95: bufferL += (int)(d[dCount+t[ 95]]*outL[ 95]); bufferR += (int)(d[dCount+t[ 95]]*outR[ 95]); 
			case  94: bufferL += (int)(d[dCount+t[ 94]]*outL[ 94]); bufferR += (int)(d[dCount+t[ 94]]*outR[ 94]); 
			case  93: bufferL += (int)(d[dCount+t[ 93]]*outL[ 93]); bufferR += (int)(d[dCount+t[ 93]]*outR[ 93]); 
			case  92: bufferL += (int)(d[dCount+t[ 92]]*outL[ 92]); bufferR += (int)(d[dCount+t[ 92]]*outR[ 92]); 
			case  91: bufferL += (int)(d[dCount+t[ 91]]*outL[ 91]); bufferR += (int)(d[dCount+t[ 91]]*outR[ 91]); 
			case  90: bufferL += (int)(d[dCount+t[ 90]]*outL[ 90]); bufferR += (int)(d[dCount+t[ 90]]*outR[ 90]); 
			case  89: bufferL += (int)(d[dCount+t[ 89]]*outL[ 89]); bufferR += (int)(d[dCount+t[ 89]]*outR[ 89]); 
			case  88: bufferL += (int)(d[dCount+t[ 88]]*outL[ 88]); bufferR += (int)(d[dCount+t[ 88]]*outR[ 88]); 
			case  87: bufferL += (int)(d[dCount+t[ 87]]*outL[ 87]); bufferR += (int)(d[dCount+t[ 87]]*outR[ 87]); 
			case  86: bufferL += (int)(d[dCount+t[ 86]]*outL[ 86]); bufferR += (int)(d[dCount+t[ 86]]*outR[ 86]); 
			case  85: bufferL += (int)(d[dCount+t[ 85]]*outL[ 85]); bufferR += (int)(d[dCount+t[ 85]]*outR[ 85]); 
			case  84: bufferL += (int)(d[dCount+t[ 84]]*outL[ 84]); bufferR += (int)(d[dCount+t[ 84]]*outR[ 84]); 
			case  83: bufferL += (int)(d[dCount+t[ 83]]*outL[ 83]); bufferR += (int)(d[dCount+t[ 83]]*outR[ 83]); 
			case  82: bufferL += (int)(d[dCount+t[ 82]]*outL[ 82]); bufferR += (int)(d[dCount+t[ 82]]*outR[ 82]); 
			case  81: bufferL += (int)(d[dCount+t[ 81]]*outL[ 81]); bufferR += (int)(d[dCount+t[ 81]]*outR[ 81]); 
			case  80: bufferL += (int)(d[dCount+t[ 80]]*outL[ 80]); bufferR += (int)(d[dCount+t[ 80]]*outR[ 80]); 
			case  79: bufferL += (int)(d[dCount+t[ 79]]*outL[ 79]); bufferR += (int)(d[dCount+t[ 79]]*outR[ 79]); 
			case  78: bufferL += (int)(d[dCount+t[ 78]]*outL[ 78]); bufferR += (int)(d[dCount+t[ 78]]*outR[ 78]); 
			case  77: bufferL += (int)(d[dCount+t[ 77]]*outL[ 77]); bufferR += (int)(d[dCount+t[ 77]]*outR[ 77]); 
			case  76: bufferL += (int)(d[dCount+t[ 76]]*outL[ 76]); bufferR += (int)(d[dCount+t[ 76]]*outR[ 76]); 
			case  75: bufferL += (int)(d[dCount+t[ 75]]*outL[ 75]); bufferR += (int)(d[dCount+t[ 75]]*outR[ 75]); 
			case  74: bufferL += (int)(d[dCount+t[ 74]]*outL[ 74]); bufferR += (int)(d[dCount+t[ 74]]*outR[ 74]); 
			case  73: bufferL += (int)(d[dCount+t[ 73]]*outL[ 73]); bufferR += (int)(d[dCount+t[ 73]]*outR[ 73]); 
			case  72: bufferL += (int)(d[dCount+t[ 72]]*outL[ 72]); bufferR += (int)(d[dCount+t[ 72]]*outR[ 72]); 
			case  71: bufferL += (int)(d[dCount+t[ 71]]*outL[ 71]); bufferR += (int)(d[dCount+t[ 71]]*outR[ 71]); 
			case  70: bufferL += (int)(d[dCount+t[ 70]]*outL[ 70]); bufferR += (int)(d[dCount+t[ 70]]*outR[ 70]); 
			case  69: bufferL += (int)(d[dCount+t[ 69]]*outL[ 69]); bufferR += (int)(d[dCount+t[ 69]]*outR[ 69]); 
			case  68: bufferL += (int)(d[dCount+t[ 68]]*outL[ 68]); bufferR += (int)(d[dCount+t[ 68]]*outR[ 68]); 
			case  67: bufferL += (int)(d[dCount+t[ 67]]*outL[ 67]); bufferR += (int)(d[dCount+t[ 67]]*outR[ 67]); 
			case  66: bufferL += (int)(d[dCount+t[ 66]]*outL[ 66]); bufferR += (int)(d[dCount+t[ 66]]*outR[ 66]); 
			case  65: bufferL += (int)(d[dCount+t[ 65]]*outL[ 65]); bufferR += (int)(d[dCount+t[ 65]]*outR[ 65]); 
			case  64: bufferL += (int)(d[dCount+t[ 64]]*outL[ 64]); bufferR += (int)(d[dCount+t[ 64]]*outR[ 64]); 
			case  63: bufferL += (int)(d[dCount+t[ 63]]*outL[ 63]); bufferR += (int)(d[dCount+t[ 63]]*outR[ 63]); 
			case  62: bufferL += (int)(d[dCount+t[ 62]]*outL[ 62]); bufferR += (int)(d[dCount+t[ 62]]*outR[ 62]); 
			case  61: bufferL += (int)(d[dCount+t[ 61]]*outL[ 61]); bufferR += (int)(d[dCount+t[ 61]]*outR[ 61]); 
			case  60: bufferL += (int)(d[dCount+t[ 60]]*outL[ 60]); bufferR += (int)(d[dCount+t[ 60]]*outR[ 60]); 
			case  59: bufferL += (int)(d[dCount+t[ 59]]*outL[ 59]); bufferR += (int)(d[dCount+t[ 59]]*outR[ 59]); 
			case  58: bufferL += (int)(d[dCount+t[ 58]]*outL[ 58]); bufferR += (int)(d[dCount+t[ 58]]*outR[ 58]); 
			case  57: bufferL += (int)(d[dCount+t[ 57]]*outL[ 57]); bufferR += (int)(d[dCount+t[ 57]]*outR[ 57]); 
			case  56: bufferL += (int)(d[dCount+t[ 56]]*outL[ 56]); bufferR += (int)(d[dCount+t[ 56]]*outR[ 56]); 
			case  55: bufferL += (int)(d[dCount+t[ 55]]*outL[ 55]); bufferR += (int)(d[dCount+t[ 55]]*outR[ 55]); 
			case  54: bufferL += (int)(d[dCount+t[ 54]]*outL[ 54]); bufferR += (int)(d[dCount+t[ 54]]*outR[ 54]); 
			case  53: bufferL += (int)(d[dCount+t[ 53]]*outL[ 53]); bufferR += (int)(d[dCount+t[ 53]]*outR[ 53]); 
			case  52: bufferL += (int)(d[dCount+t[ 52]]*outL[ 52]); bufferR += (int)(d[dCount+t[ 52]]*outR[ 52]); 
			case  51: bufferL += (int)(d[dCount+t[ 51]]*outL[ 51]); bufferR += (int)(d[dCount+t[ 51]]*outR[ 51]); 
			case  50: bufferL += (int)(d[dCount+t[ 50]]*outL[ 50]); bufferR += (int)(d[dCount+t[ 50]]*outR[ 50]); 
			case  49: bufferL += (int)(d[dCount+t[ 49]]*outL[ 49]); bufferR += (int)(d[dCount+t[ 49]]*outR[ 49]); 
			case  48: bufferL += (int)(d[dCount+t[ 48]]*outL[ 48]); bufferR += (int)(d[dCount+t[ 48]]*outR[ 48]); 
			case  47: bufferL += (int)(d[dCount+t[ 47]]*outL[ 47]); bufferR += (int)(d[dCount+t[ 47]]*outR[ 47]); 
			case  46: bufferL += (int)(d[dCount+t[ 46]]*outL[ 46]); bufferR += (int)(d[dCount+t[ 46]]*outR[ 46]); 
			case  45: bufferL += (int)(d[dCount+t[ 45]]*outL[ 45]); bufferR += (int)(d[dCount+t[ 45]]*outR[ 45]); 
			case  44: bufferL += (int)(d[dCount+t[ 44]]*outL[ 44]); bufferR += (int)(d[dCount+t[ 44]]*outR[ 44]); 
			case  43: bufferL += (int)(d[dCount+t[ 43]]*outL[ 43]); bufferR += (int)(d[dCount+t[ 43]]*outR[ 43]); 
			case  42: bufferL += (int)(d[dCount+t[ 42]]*outL[ 42]); bufferR += (int)(d[dCount+t[ 42]]*outR[ 42]); 
			case  41: bufferL += (int)(d[dCount+t[ 41]]*outL[ 41]); bufferR += (int)(d[dCount+t[ 41]]*outR[ 41]); 
			case  40: bufferL += (int)(d[dCount+t[ 40]]*outL[ 40]); bufferR += (int)(d[dCount+t[ 40]]*outR[ 40]); 
			case  39: bufferL += (int)(d[dCount+t[ 39]]*outL[ 39]); bufferR += (int)(d[dCount+t[ 39]]*outR[ 39]); 
			case  38: bufferL += (int)(d[dCount+t[ 38]]*outL[ 38]); bufferR += (int)(d[dCount+t[ 38]]*outR[ 38]); 
			case  37: bufferL += (int)(d[dCount+t[ 37]]*outL[ 37]); bufferR += (int)(d[dCount+t[ 37]]*outR[ 37]); 
			case  36: bufferL += (int)(d[dCount+t[ 36]]*outL[ 36]); bufferR += (int)(d[dCount+t[ 36]]*outR[ 36]); 
			case  35: bufferL += (int)(d[dCount+t[ 35]]*outL[ 35]); bufferR += (int)(d[dCount+t[ 35]]*outR[ 35]); 
			case  34: bufferL += (int)(d[dCount+t[ 34]]*outL[ 34]); bufferR += (int)(d[dCount+t[ 34]]*outR[ 34]); 
			case  33: bufferL += (int)(d[dCount+t[ 33]]*outL[ 33]); bufferR += (int)(d[dCount+t[ 33]]*outR[ 33]); 
			case  32: bufferL += (int)(d[dCount+t[ 32]]*outL[ 32]); bufferR += (int)(d[dCount+t[ 32]]*outR[ 32]); 
			case  31: bufferL += (int)(d[dCount+t[ 31]]*outL[ 31]); bufferR += (int)(d[dCount+t[ 31]]*outR[ 31]); 
			case  30: bufferL += (int)(d[dCount+t[ 30]]*outL[ 30]); bufferR += (int)(d[dCount+t[ 30]]*outR[ 30]);
			case  29: bufferL += (int)(d[dCount+t[ 29]]*outL[ 29]); bufferR += (int)(d[dCount+t[ 29]]*outR[ 29]); 
			case  28: bufferL += (int)(d[dCount+t[ 28]]*outL[ 28]); bufferR += (int)(d[dCount+t[ 28]]*outR[ 28]); 
			case  27: bufferL += (int)(d[dCount+t[ 27]]*outL[ 27]); bufferR += (int)(d[dCount+t[ 27]]*outR[ 27]); 
			case  26: bufferL += (int)(d[dCount+t[ 26]]*outL[ 26]); bufferR += (int)(d[dCount+t[ 26]]*outR[ 26]); 
			case  25: bufferL += (int)(d[dCount+t[ 25]]*outL[ 25]); bufferR += (int)(d[dCount+t[ 25]]*outR[ 25]); 
			case  24: bufferL += (int)(d[dCount+t[ 24]]*outL[ 24]); bufferR += (int)(d[dCount+t[ 24]]*outR[ 24]); 
			case  23: bufferL += (int)(d[dCount+t[ 23]]*outL[ 23]); bufferR += (int)(d[dCount+t[ 23]]*outR[ 23]); 
			case  22: bufferL += (int)(d[dCount+t[ 22]]*outL[ 22]); bufferR += (int)(d[dCount+t[ 22]]*outR[ 22]); 
			case  21: bufferL += (int)(d[dCount+t[ 21]]*outL[ 21]); bufferR += (int)(d[dCount+t[ 21]]*outR[ 21]); 
			case  20: bufferL += (int)(d[dCount+t[ 20]]*outL[ 20]); bufferR += (int)(d[dCount+t[ 20]]*outR[ 20]); 
			case  19: bufferL += (int)(d[dCount+t[ 19]]*outL[ 19]); bufferR += (int)(d[dCount+t[ 19]]*outR[ 19]); 
			case  18: bufferL += (int)(d[dCount+t[ 18]]*outL[ 18]); bufferR += (int)(d[dCount+t[ 18]]*outR[ 18]); 
			case  17: bufferL += (int)(d[dCount+t[ 17]]*outL[ 17]); bufferR += (int)(d[dCount+t[ 17]]*outR[ 17]); 
			case  16: bufferL += (int)(d[dCount+t[ 16]]*outL[ 16]); bufferR += (int)(d[dCount+t[ 16]]*outR[ 16]); 
			case  15: bufferL += (int)(d[dCount+t[ 15]]*outL[ 15]); bufferR += (int)(d[dCount+t[ 15]]*outR[ 15]); 
			case  14: bufferL += (int)(d[dCount+t[ 14]]*outL[ 14]); bufferR += (int)(d[dCount+t[ 14]]*outR[ 14]); 
			case  13: bufferL += (int)(d[dCount+t[ 13]]*outL[ 13]); bufferR += (int)(d[dCount+t[ 13]]*outR[ 13]); 
			case  12: bufferL += (int)(d[dCount+t[ 12]]*outL[ 12]); bufferR += (int)(d[dCount+t[ 12]]*outR[ 12]); 
			case  11: bufferL += (int)(d[dCount+t[ 11]]*outL[ 11]); bufferR += (int)(d[dCount+t[ 11]]*outR[ 11]); 
			case  10: bufferL += (int)(d[dCount+t[ 10]]*outL[ 10]); bufferR += (int)(d[dCount+t[ 10]]*outR[ 10]); 
			case   9: bufferL += (int)(d[dCount+t[  9]]*outL[  9]); bufferR += (int)(d[dCount+t[  9]]*outR[  9]); 
			case   8: bufferL += (int)(d[dCount+t[  8]]*outL[  8]); bufferR += (int)(d[dCount+t[  8]]*outR[  8]); 
			case   7: bufferL += (int)(d[dCount+t[  7]]*outL[  7]); bufferR += (int)(d[dCount+t[  7]]*outR[  7]); 
			case   6: bufferL += (int)(d[dCount+t[  6]]*outL[  6]); bufferR += (int)(d[dCount+t[  6]]*outR[  6]); 
			case   5: bufferL += (int)(d[dCount+t[  5]]*outL[  5]); bufferR += (int)(d[dCount+t[  5]]*outR[  5]); 
			case   4: bufferL += (int)(d[dCount+t[  4]]*outL[  4]); bufferR += (int)(d[dCount+t[  4]]*outR[  4]);
			case   3: bufferL += (int)(d[dCount+t[  3]]*outL[  3]); bufferR += (int)(d[dCount+t[  3]]*outR[  3]);
			case   2: bufferL += (int)(d[dCount+t[  2]]*outL[  2]); bufferR += (int)(d[dCount+t[  2]]*outR[  2]);
			case   1: bufferL += (int)(d[dCount+t[  1]]*outL[  1]); bufferR += (int)(d[dCount+t[  1]]*outR[  1]);
		}
		//test to see that delay is working at all: it will be a big stack of case with no break
		
		inputSampleL = bufferL;
		inputSampleR = bufferR;
		//scale back the reverb buffers based on how big of a range we used
		
		
		wearR[9] = wearR[8]; wearR[8] = wearR[7]; wearR[7] = wearR[6]; wearR[6] = wearR[5];
		wearR[5] = wearR[4]; wearR[4] = wearR[3]; wearR[3] = wearR[2]; wearR[2] = wearR[1];
		wearR[1] = wearR[0]; wearR[0] = accumulatorSample = (inputSampleR-wearRPrev);
		
		accumulatorSample *= factor[0];
		accumulatorSample += (wearR[1] * factor[1]);
		accumulatorSample += (wearR[2] * factor[2]);
		accumulatorSample += (wearR[3] * factor[3]);
		accumulatorSample += (wearR[4] * factor[4]);
		accumulatorSample += (wearR[5] * factor[5]);
		accumulatorSample += (wearR[6] * factor[6]);
		accumulatorSample += (wearR[7] * factor[7]);
		accumulatorSample += (wearR[8] * factor[8]);
		accumulatorSample += (wearR[9] * factor[9]);
		//we are doing our repetitive calculations on a separate value
		correction = (inputSampleR-wearRPrev) + accumulatorSample;
		wearRPrev = inputSampleR;		
		inputSampleR += correction;
		
		wearL[9] = wearL[8]; wearL[8] = wearL[7]; wearL[7] = wearL[6]; wearL[6] = wearL[5];
		wearL[5] = wearL[4]; wearL[4] = wearL[3]; wearL[3] = wearL[2]; wearL[2] = wearL[1];
		wearL[1] = wearL[0]; wearL[0] = accumulatorSample = (inputSampleL-wearLPrev);
		
		accumulatorSample *= factor[0];
		accumulatorSample += (wearL[1] * factor[1]);
		accumulatorSample += (wearL[2] * factor[2]);
		accumulatorSample += (wearL[3] * factor[3]);
		accumulatorSample += (wearL[4] * factor[4]);
		accumulatorSample += (wearL[5] * factor[5]);
		accumulatorSample += (wearL[6] * factor[6]);
		accumulatorSample += (wearL[7] * factor[7]);
		accumulatorSample += (wearL[8] * factor[8]);
		accumulatorSample += (wearL[9] * factor[9]);
		//we are doing our repetitive calculations on a separate value		
		correction = (inputSampleL-wearLPrev) + accumulatorSample;
		wearLPrev = inputSampleL;		
		inputSampleL += correction;
		//completed Groove Wear section
		
		inputSampleL /= outputPad;
		inputSampleR /= outputPad;
		
		//back to previous plugin
		drySampleL *= dryness;
		drySampleR *= dryness;
		
		inputSampleL *= wetness;
		inputSampleR *= wetness;
		
		inputSampleL += drySampleL;
		inputSampleR += drySampleR;
		//here we combine the tanks with the dry signal
				
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

} // end namespace StarChild

