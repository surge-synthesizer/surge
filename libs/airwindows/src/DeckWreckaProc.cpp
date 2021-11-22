/* ========================================
 *  Deckwrecka - Deckwrecka.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Deckwrecka_H
#include "DeckWrecka.h"
#endif

namespace DeckWrecka {


void Deckwrecka::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double wreck = pow(A,2);
	double randyL;
	double randyR;
	double HeadBumpL = 0.0;
	double HeadBumpR = 0.0;
	double HeadBumpFreq = 0.01/overallscale;
	double iirHPAmount = 0.0000014/overallscale;
	double altHPAmount = 1.0 - iirHPAmount;
	double tempSample;
	double bridgerectifier;
	double correction;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		
		bflip++;
		if (bflip < 1 || bflip > 3) bflip = 1;

		randyL = (rand()/(double)RAND_MAX);
		randyL += (rand()/(double)RAND_MAX);
		randyL += (rand()/(double)RAND_MAX);
		randyL += (rand()/(double)RAND_MAX);
		randyL += (rand()/(double)RAND_MAX);
		randyL += (rand()/(double)RAND_MAX);
		randyL /= 6.0;
		randyL *= wreck; //0 to 1 the noise, may not be needed
		//set up the noise

		randyR = (rand()/(double)RAND_MAX);
		randyR += (rand()/(double)RAND_MAX);
		randyR += (rand()/(double)RAND_MAX);
		randyR += (rand()/(double)RAND_MAX);
		randyR += (rand()/(double)RAND_MAX);
		randyR += (rand()/(double)RAND_MAX);
		randyR /= 6.0;
		randyR *= wreck; //0 to 1 the noise, may not be needed
		//set up the noise
		
		switch (bflip)
		{
			case 1:				
				iirHeadBumpAL += (inputSampleL * randyL);
				iirHeadBumpAL -= (iirHeadBumpAL * iirHeadBumpAL * iirHeadBumpAL * HeadBumpFreq);
				iirHeadBumpAR += (inputSampleR * randyR);
				iirHeadBumpAR -= (iirHeadBumpAR * iirHeadBumpAR * iirHeadBumpAR * HeadBumpFreq);
				break;
			case 2:
				iirHeadBumpBL += (inputSampleL * randyL);
				iirHeadBumpBL -= (iirHeadBumpBL * iirHeadBumpBL * iirHeadBumpBL * HeadBumpFreq);
				iirHeadBumpBR += (inputSampleR * randyR);
				iirHeadBumpBR -= (iirHeadBumpBR * iirHeadBumpBR * iirHeadBumpBR * HeadBumpFreq);
				break;
			case 3:
				iirHeadBumpCL += (inputSampleL * randyL);
				iirHeadBumpCL -= (iirHeadBumpCL * iirHeadBumpCL * iirHeadBumpCL * HeadBumpFreq);
				iirHeadBumpCR += (inputSampleR * randyR);
				iirHeadBumpCR -= (iirHeadBumpCR * iirHeadBumpCR * iirHeadBumpCR * HeadBumpFreq);
				break;
		}
		HeadBumpL = iirHeadBumpAL + iirHeadBumpBL + iirHeadBumpCL;		
		HeadBumpR = iirHeadBumpAR + iirHeadBumpBR + iirHeadBumpCR;
		
		tempSample = HeadBumpL;
		iirSampleAL = (iirSampleAL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleAL; correction = iirSampleAL;
		iirSampleBL = (iirSampleBL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleBL; correction += iirSampleBL;
		iirSampleCL = (iirSampleCL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleCL; correction += iirSampleCL;
		iirSampleDL = (iirSampleDL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleDL; correction += iirSampleDL;
		iirSampleEL = (iirSampleEL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleEL; correction += iirSampleEL;
		iirSampleFL = (iirSampleFL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleFL; correction += iirSampleFL;
		iirSampleGL = (iirSampleGL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleGL; correction += iirSampleGL;
		iirSampleHL = (iirSampleHL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleHL; correction += iirSampleHL;
		iirSampleIL = (iirSampleIL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleIL; correction += iirSampleIL;
		iirSampleJL = (iirSampleJL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleJL; correction += iirSampleJL;
		iirSampleKL = (iirSampleKL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleKL; correction += iirSampleKL;
		iirSampleLL = (iirSampleLL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleLL; correction += iirSampleLL;
		iirSampleML = (iirSampleML * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleML; correction += iirSampleML;
		iirSampleNL = (iirSampleNL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleNL; correction += iirSampleNL;
		iirSampleOL = (iirSampleOL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleOL; correction += iirSampleOL;
		iirSamplePL = (iirSamplePL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSamplePL; correction += iirSamplePL;
		iirSampleQL = (iirSampleQL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleQL; correction += iirSampleQL;
		iirSampleRL = (iirSampleRL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleRL; correction += iirSampleRL;
		iirSampleSL = (iirSampleSL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleSL; correction += iirSampleSL;
		iirSampleTL = (iirSampleTL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleTL; correction += iirSampleTL;
		iirSampleUL = (iirSampleUL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleUL; correction += iirSampleUL;
		iirSampleVL = (iirSampleVL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleVL; correction += iirSampleVL;
		iirSampleWL = (iirSampleWL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleWL; correction += iirSampleWL;
		iirSampleXL = (iirSampleXL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleXL; correction += iirSampleXL;
		iirSampleYL = (iirSampleYL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleYL; correction += iirSampleYL;
		iirSampleZL = (iirSampleZL * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleZL; correction += iirSampleZL;
		//do the IIR on a dummy sample, and store up the correction in a variable at the same scale as the very low level
		//numbers being used. Don't keep doing it against the possibly high level signal number.
		HeadBumpL -= correction;
		//apply stored up tiny corrections
		HeadBumpL *= (1.0 - (wreck/2));

		tempSample = HeadBumpR;
		iirSampleAR = (iirSampleAR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleAR; correction = iirSampleAR;
		iirSampleBR = (iirSampleBR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleBR; correction += iirSampleBR;
		iirSampleCR = (iirSampleCR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleCR; correction += iirSampleCR;
		iirSampleDR = (iirSampleDR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleDR; correction += iirSampleDR;
		iirSampleER = (iirSampleER * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleER; correction += iirSampleER;
		iirSampleFR = (iirSampleFR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleFR; correction += iirSampleFR;
		iirSampleGR = (iirSampleGR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleGR; correction += iirSampleGR;
		iirSampleHR = (iirSampleHR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleHR; correction += iirSampleHR;
		iirSampleIR = (iirSampleIR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleIR; correction += iirSampleIR;
		iirSampleJR = (iirSampleJR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleJR; correction += iirSampleJR;
		iirSampleKR = (iirSampleKR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleKR; correction += iirSampleKR;
		iirSampleLR = (iirSampleLR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleLR; correction += iirSampleLR;
		iirSampleMR = (iirSampleMR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleMR; correction += iirSampleMR;
		iirSampleNR = (iirSampleNR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleNR; correction += iirSampleNR;
		iirSampleOR = (iirSampleOR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleOR; correction += iirSampleOR;
		iirSamplePR = (iirSamplePR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSamplePR; correction += iirSamplePR;
		iirSampleQR = (iirSampleQR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleQR; correction += iirSampleQR;
		iirSampleRR = (iirSampleRR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleRR; correction += iirSampleRR;
		iirSampleSR = (iirSampleSR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleSR; correction += iirSampleSR;
		iirSampleTR = (iirSampleTR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleTR; correction += iirSampleTR;
		iirSampleUR = (iirSampleUR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleUR; correction += iirSampleUR;
		iirSampleVR = (iirSampleVR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleVR; correction += iirSampleVR;
		iirSampleWR = (iirSampleWR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleWR; correction += iirSampleWR;
		iirSampleXR = (iirSampleXR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleXR; correction += iirSampleXR;
		iirSampleYR = (iirSampleYR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleYR; correction += iirSampleYR;
		iirSampleZR = (iirSampleZR * altHPAmount) + (tempSample * iirHPAmount); tempSample -= iirSampleZR; correction += iirSampleZR;
		//do the IIR on a dummy sample, and store up the correction in a variable at the same scale as the very low level
		//numbers being used. Don't keep doing it against the possibly high level signal number.
		HeadBumpR -= correction;
		//apply stored up tiny corrections
		HeadBumpR *= (1.0 - (wreck/2));
		
		bridgerectifier = fabs(HeadBumpL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (HeadBumpL > 0.0) HeadBumpL = bridgerectifier;
		else HeadBumpL = -bridgerectifier;
		//clamp HeadBump to -1 to 1 with distortion		

		bridgerectifier = fabs(HeadBumpR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (HeadBumpR > 0.0) HeadBumpR = bridgerectifier;
		else HeadBumpR = -bridgerectifier;
		//clamp HeadBump to -1 to 1 with distortion		
		
		inputSampleL *= (1.0 - wreck);
		inputSampleL += (HeadBumpL * wreck);

		inputSampleR *= (1.0 - wreck);
		inputSampleR += (HeadBumpR * wreck);

		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace DeckWrecka

