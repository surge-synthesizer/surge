/* ========================================
 *  NCSeventeen - NCSeventeen.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NCSeventeen_H
#include "NCSeventeen.h"
#endif

namespace NCSeventeen {


void NCSeventeen::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double inP2;
	double chebyshev;
	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double IIRscaleback = 0.0004716;
	double bassScaleback = 0.0002364;
	double trebleScaleback = 0.0005484;
	double addBassBuss = 0.000243;
	double addTrebBuss = 0.000407;
	double addShortBuss = 0.000326;
	IIRscaleback /= overallscale;
	bassScaleback /= overallscale;
	trebleScaleback /= overallscale;
	addBassBuss /= overallscale;
	addTrebBuss /= overallscale;
	addShortBuss /= overallscale;
	double limitingBass = 0.39;
	double limitingTreb = 0.6;
	double limiting = 0.36;
	double maxfeedBass = 0.972;
	double maxfeedTreb = 0.972;
	double maxfeed = 0.975;
	double bridgerectifier;
	long double inputSampleL;
	double lowSampleL = 0.0;
	double highSampleL;
	double distSampleL;
	double minusSampleL;
	double plusSampleL;
	long double inputSampleR;
	double lowSampleR = 0.0;
	double highSampleR;
	double distSampleR;
	double minusSampleR;
	double plusSampleR;
	double gain = pow(10.0,(A*24.0)/20);
	double outlevel = B;
	
    
    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		inputSampleL *= gain;
		inputSampleR *= gain;
		
		if (flip)
		{
			iirSampleAL = (iirSampleAL * 0.9) + (inputSampleL * 0.1);
			lowSampleL = iirSampleAL;
			iirSampleAR = (iirSampleAR * 0.9) + (inputSampleR * 0.1);
			lowSampleR = iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * 0.9) + (inputSampleL * 0.1);
			lowSampleL = iirSampleBL;
			iirSampleBR = (iirSampleBR * 0.9) + (inputSampleR * 0.1);
			lowSampleR = iirSampleBR;
		}
		highSampleL = inputSampleL - lowSampleL;
		highSampleR = inputSampleR - lowSampleR;
		flip = !flip;
		//we now have two bands and the original source
		
		inP2 = lowSampleL * lowSampleL;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= basslevL;
		//second harmonic max +1
		if (basslevL > 0) basslevL -= bassScaleback;
		if (basslevL < 0) basslevL += bassScaleback;
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(lowSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (lowSampleL > 0.0) distSampleL = bridgerectifier;
		else distSampleL = -bridgerectifier;
		minusSampleL = lowSampleL - distSampleL;
		plusSampleL = lowSampleL + distSampleL;
		if (minusSampleL > maxfeedBass) minusSampleL = maxfeedBass;
		if (plusSampleL > maxfeedBass) plusSampleL = maxfeedBass;
		if (plusSampleL < -maxfeedBass) plusSampleL = -maxfeedBass;
		if (minusSampleL < -maxfeedBass) minusSampleL = -maxfeedBass;
		if (lowSampleL > distSampleL) basslevL += (minusSampleL*addBassBuss);
		if (lowSampleL < -distSampleL) basslevL -= (plusSampleL*addBassBuss);
		if (basslevL > 1.0)  basslevL = 1.0;
		if (basslevL < -1.0) basslevL = -1.0;
		bridgerectifier = fabs(lowSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (lowSampleL > 0.0) lowSampleL = bridgerectifier;
		else lowSampleL = -bridgerectifier;
		//apply the distortion transform for reals
		lowSampleL /= (1.0+fabs(basslevL*limitingBass));
		lowSampleL += chebyshev;
		//apply the correction measuresL
		
		inP2 = lowSampleR * lowSampleR;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= basslevR;
		//second harmonic max +1
		if (basslevR > 0) basslevR -= bassScaleback;
		if (basslevR < 0) basslevR += bassScaleback;
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(lowSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (lowSampleR > 0.0) distSampleR = bridgerectifier;
		else distSampleR = -bridgerectifier;
		minusSampleR = lowSampleR - distSampleR;
		plusSampleR = lowSampleR + distSampleR;
		if (minusSampleR > maxfeedBass) minusSampleR = maxfeedBass;
		if (plusSampleR > maxfeedBass) plusSampleR = maxfeedBass;
		if (plusSampleR < -maxfeedBass) plusSampleR = -maxfeedBass;
		if (minusSampleR < -maxfeedBass) minusSampleR = -maxfeedBass;
		if (lowSampleR > distSampleR) basslevR += (minusSampleR*addBassBuss);
		if (lowSampleR < -distSampleR) basslevR -= (plusSampleR*addBassBuss);
		if (basslevR > 1.0)  basslevR = 1.0;
		if (basslevR < -1.0) basslevR = -1.0;
		bridgerectifier = fabs(lowSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (lowSampleR > 0.0) lowSampleR = bridgerectifier;
		else lowSampleR = -bridgerectifier;
		//apply the distortion transform for reals
		lowSampleR /= (1.0+fabs(basslevR*limitingBass));
		lowSampleR += chebyshev;
		//apply the correction measuresR
		
		inP2 = highSampleL * highSampleL;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= treblevL;
		//second harmonic max +1
		if (treblevL > 0) treblevL -= trebleScaleback;
		if (treblevL < 0) treblevL += trebleScaleback;
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(highSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (highSampleL > 0.0) distSampleL = bridgerectifier;
		else distSampleL = -bridgerectifier;
		minusSampleL = highSampleL - distSampleL;
		plusSampleL = highSampleL + distSampleL;
		if (minusSampleL > maxfeedTreb) minusSampleL = maxfeedTreb;
		if (plusSampleL > maxfeedTreb) plusSampleL = maxfeedTreb;
		if (plusSampleL < -maxfeedTreb) plusSampleL = -maxfeedTreb;
		if (minusSampleL < -maxfeedTreb) minusSampleL = -maxfeedTreb;
		if (highSampleL > distSampleL) treblevL += (minusSampleL*addTrebBuss);
		if (highSampleL < -distSampleL) treblevL -= (plusSampleL*addTrebBuss);
		if (treblevL > 1.0)  treblevL = 1.0;
		if (treblevL < -1.0) treblevL = -1.0;
		bridgerectifier = fabs(highSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (highSampleL > 0.0) highSampleL = bridgerectifier;
		else highSampleL = -bridgerectifier;
		//apply the distortion transform for reals
		highSampleL /= (1.0+fabs(treblevL*limitingTreb));
		highSampleL += chebyshev;
		//apply the correction measuresL
		
		inP2 = highSampleR * highSampleR;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= treblevR;
		//second harmonic max +1
		if (treblevR > 0) treblevR -= trebleScaleback;
		if (treblevR < 0) treblevR += trebleScaleback;
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(highSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (highSampleR > 0.0) distSampleR = bridgerectifier;
		else distSampleR = -bridgerectifier;
		minusSampleR = highSampleR - distSampleR;
		plusSampleR = highSampleR + distSampleR;
		if (minusSampleR > maxfeedTreb) minusSampleR = maxfeedTreb;
		if (plusSampleR > maxfeedTreb) plusSampleR = maxfeedTreb;
		if (plusSampleR < -maxfeedTreb) plusSampleR = -maxfeedTreb;
		if (minusSampleR < -maxfeedTreb) minusSampleR = -maxfeedTreb;
		if (highSampleR > distSampleR) treblevR += (minusSampleR*addTrebBuss);
		if (highSampleR < -distSampleR) treblevR -= (plusSampleR*addTrebBuss);
		if (treblevR > 1.0)  treblevR = 1.0;
		if (treblevR < -1.0) treblevR = -1.0;
		bridgerectifier = fabs(highSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (highSampleR > 0.0) highSampleR = bridgerectifier;
		else highSampleR = -bridgerectifier;
		//apply the distortion transform for reals
		highSampleR /= (1.0+fabs(treblevR*limitingTreb));
		highSampleR += chebyshev;
		//apply the correction measuresR
		
		inputSampleL = lowSampleL + highSampleL;
		inputSampleR = lowSampleR + highSampleR;
		
		inP2 = inputSampleL * inputSampleL;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= cheblevL;
		//third harmonic max -1
		if (cheblevL > 0) cheblevL -= (IIRscaleback);
		if (cheblevL < 0) cheblevL += (IIRscaleback);
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0.0) distSampleL = bridgerectifier;
		else distSampleL = -bridgerectifier;
		minusSampleL = inputSampleL - distSampleL;
		plusSampleL = inputSampleL + distSampleL;
		if (minusSampleL > maxfeed) minusSampleL = maxfeed;
		if (plusSampleL > maxfeed) plusSampleL = maxfeed;
		if (plusSampleL < -maxfeed) plusSampleL = -maxfeed;
		if (minusSampleL < -maxfeed) minusSampleL = -maxfeed;
		if (inputSampleL > distSampleL) cheblevL += (minusSampleL*addShortBuss);
		if (inputSampleL < -distSampleL) cheblevL -= (plusSampleL*addShortBuss);
		if (cheblevL > 1.0)  cheblevL = 1.0;
		if (cheblevL < -1.0) cheblevL = -1.0;
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		//apply the distortion transform for reals
		inputSampleL /= (1.0+fabs(cheblevL*limiting));
		inputSampleL += chebyshev;
		//apply the correction measuresL

		inP2 = inputSampleR * inputSampleR;
		if (inP2 > 1.0) inP2 = 1.0; if (inP2 < -1.0) inP2 = -1.0;
		chebyshev = (2 * inP2);
		chebyshev *= cheblevR;
		//third harmonic max -1
		if (cheblevR > 0) cheblevR -= IIRscaleback;
		if (cheblevR < 0) cheblevR += IIRscaleback;
		//this is ShortBuss, IIRscaleback is the decay speed. *2 for second harmonic, and so on
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0.0) distSampleR = bridgerectifier;
		else distSampleR = -bridgerectifier;
		minusSampleR = inputSampleR - distSampleR;
		plusSampleR = inputSampleR + distSampleR;
		if (minusSampleR > maxfeed) minusSampleR = maxfeed;
		if (plusSampleR > maxfeed) plusSampleR = maxfeed;
		if (plusSampleR < -maxfeed) plusSampleR = -maxfeed;
		if (minusSampleR < -maxfeed) minusSampleR = -maxfeed;
		if (inputSampleR > distSampleR) cheblevR += (minusSampleR*addShortBuss);
		if (inputSampleR < -distSampleR) cheblevR -= (plusSampleR*addShortBuss);
		if (cheblevR > 1.0)  cheblevR = 1.0;
		if (cheblevR < -1.0) cheblevR = -1.0;
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//apply the distortion transform for reals
		inputSampleR /= (1.0+fabs(cheblevR*limiting));
		inputSampleR += chebyshev;
		//apply the correction measuresR
		
		if (outlevel < 1.0) {
			inputSampleL *= outlevel;
			inputSampleR *= outlevel;
		}
		
		if (inputSampleL > 0.95) inputSampleL = 0.95;
		if (inputSampleL < -0.95) inputSampleL = -0.95;
		if (inputSampleR > 0.95) inputSampleR = 0.95;
		if (inputSampleR < -0.95) inputSampleR = -0.95;
		//iron bar

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

} // end namespace NCSeventeen

