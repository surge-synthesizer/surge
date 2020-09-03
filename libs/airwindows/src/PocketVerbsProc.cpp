/* ========================================
 *  PocketVerbs - PocketVerbs.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PocketVerbs_H
#include "PocketVerbs.h"
#endif

namespace PocketVerbs {


void PocketVerbs::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	int verbtype = (VstInt32)( A * 5.999 )+1;
	
	double roomsize = (pow(B,2)*1.9)+0.1;
	
	double release = 0.00008 * pow(C,3);
	if (release == 0.0) {peakL = 1.0; peakR = 1.0;}
	double wetnesstarget = D;
	double dryness = (1.0 - wetnesstarget);
	//verbs use base wetness value internally
	double wetness = wetnesstarget;
	double constallpass = 0.618033988749894848204586; //golden ratio!
	int allpasstemp;
	int count;
	int max = 70; //biggest divisor to test primes against
	double bridgerectifier;
	double gain = 0.5+(wetnesstarget*0.5); //dryer for less verb drive
	//used as an aux, saturates when fed high levels
	
	//remap values to primes input number in question is 'i'
	//max is the largest prime we care about- HF interactions more interesting than the big numbers
	//pushing values larger and larger until we have a result
	//for (primetest=2; primetest <= max; primetest++) {if ( i!=primetest && i % primetest == 0 ) {i += 1; primetest=2;}}
	
	if (savedRoomsize != roomsize) {savedRoomsize = roomsize; countdown = 26;} //kick off the adjustment which will take 26 zippernoise refreshes to complete
    
	if (countdown > 0) {switch (countdown)
		{
			case 1:
				delayA = (int(maxdelayA * roomsize));
				for (count=2; count <= max; count++) {if ( delayA != count && delayA % count == 0 ) {delayA += 1; count=2;}} //try for primeish As
				if (delayA > maxdelayA) delayA = maxdelayA; //insanitycheck
				for(count = alpAL; count < 15149; count++) {aAL[count] = 0.0;}
				for(count = outAL; count < 15149; count++) {oAL[count] = 0.0;}
				break;
				
			case 2:
				delayB = (int(maxdelayB * roomsize));
				for (count=2; count <= max; count++) {if ( delayB != count && delayB % count == 0 ) {delayB += 1; count=2;}} //try for primeish Bs
				if (delayB > maxdelayB) delayB = maxdelayB; //insanitycheck
				for(count = alpBL; count < 14617; count++) {aBL[count] = 0.0;}
				for(count = outBL; count < 14617; count++) {oBL[count] = 0.0;}
				break;
				
			case 3:
				delayC = (int(maxdelayC * roomsize));
				for (count=2; count <= max; count++) {if ( delayC != count && delayC % count == 0 ) {delayC += 1; count=2;}} //try for primeish Cs
				if (delayC > maxdelayC) delayC = maxdelayC; //insanitycheck
				for(count = alpCL; count < 14357; count++) {aCL[count] = 0.0;}
				for(count = outCL; count < 14357; count++) {oCL[count] = 0.0;}
				break;
				
			case 4:
				delayD = (int(maxdelayD * roomsize));
				for (count=2; count <= max; count++) {if ( delayD != count && delayD % count == 0 ) {delayD += 1; count=2;}} //try for primeish Ds
				if (delayD > maxdelayD) delayD = maxdelayD; //insanitycheck
				for(count = alpDL; count < 13817; count++) {aDL[count] = 0.0;}
				for(count = outDL; count < 13817; count++) {oDL[count] = 0.0;}
				break;
				
			case 5:
				delayE = (int(maxdelayE * roomsize));
				for (count=2; count <= max; count++) {if ( delayE != count && delayE % count == 0 ) {delayE += 1; count=2;}} //try for primeish Es
				if (delayE > maxdelayE) delayE = maxdelayE; //insanitycheck
				for(count = alpEL; count < 13561; count++) {aEL[count] = 0.0;}
				for(count = outEL; count < 13561; count++) {oEL[count] = 0.0;}
				break;
				
			case 6:
				delayF = (int(maxdelayF * roomsize));
				for (count=2; count <= max; count++) {if ( delayF != count && delayF % count == 0 ) {delayF += 1; count=2;}} //try for primeish Fs
				if (delayF > maxdelayF) delayF = maxdelayF; //insanitycheck
				for(count = alpFL; count < 13045; count++) {aFL[count] = 0.0;}
				for(count = outFL; count < 13045; count++) {oFL[count] = 0.0;}
				break;
				
			case 7:
				delayG = (int(maxdelayG * roomsize));
				for (count=2; count <= max; count++) {if ( delayG != count && delayG % count == 0 ) {delayG += 1; count=2;}} //try for primeish Gs
				if (delayG > maxdelayG) delayG = maxdelayG; //insanitycheck
				for(count = alpGL; count < 11965; count++) {aGL[count] = 0.0;}
				for(count = outGL; count < 11965; count++) {oGL[count] = 0.0;}
				break;
				
			case 8:
				delayH = (int(maxdelayH * roomsize));
				for (count=2; count <= max; count++) {if ( delayH != count && delayH % count == 0 ) {delayH += 1; count=2;}} //try for primeish Hs
				if (delayH > maxdelayH) delayH = maxdelayH; //insanitycheck
				for(count = alpHL; count < 11129; count++) {aHL[count] = 0.0;}
				for(count = outHL; count < 11129; count++) {oHL[count] = 0.0;}
				break;
				
			case 9:
				delayI = (int(maxdelayI * roomsize));
				for (count=2; count <= max; count++) {if ( delayI != count && delayI % count == 0 ) {delayI += 1; count=2;}} //try for primeish Is
				if (delayI > maxdelayI) delayI = maxdelayI; //insanitycheck
				for(count = alpIL; count < 10597; count++) {aIL[count] = 0.0;}
				for(count = outIL; count < 10597; count++) {oIL[count] = 0.0;}
				break;
				
			case 10:
				delayJ = (int(maxdelayJ * roomsize));
				for (count=2; count <= max; count++) {if ( delayJ != count && delayJ % count == 0 ) {delayJ += 1; count=2;}} //try for primeish Js
				if (delayJ > maxdelayJ) delayJ = maxdelayJ; //insanitycheck
				for(count = alpJL; count < 9809; count++) {aJL[count] = 0.0;}
				for(count = outJL; count < 9809; count++) {oJL[count] = 0.0;}
				break;
				
			case 11:
				delayK = (int(maxdelayK * roomsize));
				for (count=2; count <= max; count++) {if ( delayK != count && delayK % count == 0 ) {delayK += 1; count=2;}} //try for primeish Ks
				if (delayK > maxdelayK) delayK = maxdelayK; //insanitycheck
				for(count = alpKL; count < 9521; count++) {aKL[count] = 0.0;}
				for(count = outKL; count < 9521; count++) {oKL[count] = 0.0;}
				break;
				
			case 12:
				delayL = (int(maxdelayL * roomsize));
				for (count=2; count <= max; count++) {if ( delayL != count && delayL % count == 0 ) {delayL += 1; count=2;}} //try for primeish Ls
				if (delayL > maxdelayL) delayL = maxdelayL; //insanitycheck
				for(count = alpLL; count < 8981; count++) {aLL[count] = 0.0;}
				for(count = outLL; count < 8981; count++) {oLL[count] = 0.0;}
				break;
				
			case 13:
				delayM = (int(maxdelayM * roomsize));
				for (count=2; count <= max; count++) {if ( delayM != count && delayM % count == 0 ) {delayM += 1; count=2;}} //try for primeish Ms
				if (delayM > maxdelayM) delayM = maxdelayM; //insanitycheck
				for(count = alpML; count < 8785; count++) {aML[count] = 0.0;}
				for(count = outML; count < 8785; count++) {oML[count] = 0.0;}
				break;
				
			case 14:
				delayN = (int(maxdelayN * roomsize));
				for (count=2; count <= max; count++) {if ( delayN != count && delayN % count == 0 ) {delayN += 1; count=2;}} //try for primeish Ns
				if (delayN > maxdelayN) delayN = maxdelayN; //insanitycheck
				for(count = alpNL; count < 8461; count++) {aNL[count] = 0.0;}
				for(count = outNL; count < 8461; count++) {oNL[count] = 0.0;}
				break;
				
			case 15:
				delayO = (int(maxdelayO * roomsize));
				for (count=2; count <= max; count++) {if ( delayO != count && delayO % count == 0 ) {delayO += 1; count=2;}} //try for primeish Os
				if (delayO > maxdelayO) delayO = maxdelayO; //insanitycheck
				for(count = alpOL; count < 8309; count++) {aOL[count] = 0.0;}
				for(count = outOL; count < 8309; count++) {oOL[count] = 0.0;}
				break;
				
			case 16:
				delayP = (int(maxdelayP * roomsize));
				for (count=2; count <= max; count++) {if ( delayP != count && delayP % count == 0 ) {delayP += 1; count=2;}} //try for primeish Ps
				if (delayP > maxdelayP) delayP = maxdelayP; //insanitycheck
				for(count = alpPL; count < 7981; count++) {aPL[count] = 0.0;}
				for(count = outPL; count < 7981; count++) {oPL[count] = 0.0;}
				break;
				
			case 17:
				delayQ = (int(maxdelayQ * roomsize));
				for (count=2; count <= max; count++) {if ( delayQ != count && delayQ % count == 0 ) {delayQ += 1; count=2;}} //try for primeish Qs
				if (delayQ > maxdelayQ) delayQ = maxdelayQ; //insanitycheck
				for(count = alpQL; count < 7321; count++) {aQL[count] = 0.0;}
				for(count = outQL; count < 7321; count++) {oQL[count] = 0.0;}
				break;
				
			case 18:
				delayR = (int(maxdelayR * roomsize));
				for (count=2; count <= max; count++) {if ( delayR != count && delayR % count == 0 ) {delayR += 1; count=2;}} //try for primeish Rs
				if (delayR > maxdelayR) delayR = maxdelayR; //insanitycheck
				for(count = alpRL; count < 6817; count++) {aRL[count] = 0.0;}
				for(count = outRL; count < 6817; count++) {oRL[count] = 0.0;}
				break;
				
			case 19:
				delayS = (int(maxdelayS * roomsize));
				for (count=2; count <= max; count++) {if ( delayS != count && delayS % count == 0 ) {delayS += 1; count=2;}} //try for primeish Ss
				if (delayS > maxdelayS) delayS = maxdelayS; //insanitycheck
				for(count = alpSL; count < 6505; count++) {aSL[count] = 0.0;}
				for(count = outSL; count < 6505; count++) {oSL[count] = 0.0;}
				break;
				
			case 20:
				delayT = (int(maxdelayT * roomsize));
				for (count=2; count <= max; count++) {if ( delayT != count && delayT % count == 0 ) {delayT += 1; count=2;}} //try for primeish Ts
				if (delayT > maxdelayT) delayT = maxdelayT; //insanitycheck
				for(count = alpTL; count < 6001; count++) {aTL[count] = 0.0;}
				for(count = outTL; count < 6001; count++) {oTL[count] = 0.0;}
				break;
				
			case 21:
				delayU = (int(maxdelayU * roomsize));
				for (count=2; count <= max; count++) {if ( delayU != count && delayU % count == 0 ) {delayU += 1; count=2;}} //try for primeish Us
				if (delayU > maxdelayU) delayU = maxdelayU; //insanitycheck
				for(count = alpUL; count < 5837; count++) {aUL[count] = 0.0;}
				for(count = outUL; count < 5837; count++) {oUL[count] = 0.0;}
				break;
				
			case 22:
				delayV = (int(maxdelayV * roomsize));
				for (count=2; count <= max; count++) {if ( delayV != count && delayV % count == 0 ) {delayV += 1; count=2;}} //try for primeish Vs
				if (delayV > maxdelayV) delayV = maxdelayV; //insanitycheck
				for(count = alpVL; count < 5501; count++) {aVL[count] = 0.0;}
				for(count = outVL; count < 5501; count++) {oVL[count] = 0.0;}
				break;
				
			case 23:
				delayW = (int(maxdelayW * roomsize));
				for (count=2; count <= max; count++) {if ( delayW != count && delayW % count == 0 ) {delayW += 1; count=2;}} //try for primeish Ws
				if (delayW > maxdelayW) delayW = maxdelayW; //insanitycheck
				for(count = alpWL; count < 5009; count++) {aWL[count] = 0.0;}
				for(count = outWL; count < 5009; count++) {oWL[count] = 0.0;}
				break;
				
			case 24:
				delayX = (int(maxdelayX * roomsize));
				for (count=2; count <= max; count++) {if ( delayX != count && delayX % count == 0 ) {delayX += 1; count=2;}} //try for primeish Xs
				if (delayX > maxdelayX) delayX = maxdelayX; //insanitycheck
				for(count = alpXL; count < 4849; count++) {aXL[count] = 0.0;}
				for(count = outXL; count < 4849; count++) {oXL[count] = 0.0;}
				break;
				
			case 25:
				delayY = (int(maxdelayY * roomsize));
				for (count=2; count <= max; count++) {if ( delayY != count && delayY % count == 0 ) {delayY += 1; count=2;}} //try for primeish Ys
				if (delayY > maxdelayY) delayY = maxdelayY; //insanitycheck
				for(count = alpYL; count < 4295; count++) {aYL[count] = 0.0;}
				for(count = outYL; count < 4295; count++) {oYL[count] = 0.0;}
				break;
				
			case 26:
				delayZ = (int(maxdelayZ * roomsize));
				for (count=2; count <= max; count++) {if ( delayZ != count && delayZ % count == 0 ) {delayZ += 1; count=2;}} //try for primeish Zs
				if (delayZ > maxdelayZ) delayZ = maxdelayZ; //insanitycheck
				for(count = alpZL; count < 4179; count++) {aZL[count] = 0.0;}	
				for(count = outZL; count < 4179; count++) {oZL[count] = 0.0;}
				break;
		} //end of switch statement
		//countdown--; we are doing this after the second one
	}
	
	if (countdown > 0) {switch (countdown)
		{
			case 1:
				delayA = (int(maxdelayA * roomsize));
				for (count=2; count <= max; count++) {if ( delayA != count && delayA % count == 0 ) {delayA += 1; count=2;}} //try for primeish As
				if (delayA > maxdelayA) delayA = maxdelayA; //insanitycheck
				for(count = alpAR; count < 15149; count++) {aAR[count] = 0.0;}
				for(count = outAR; count < 15149; count++) {oAR[count] = 0.0;}
				break;
				
			case 2:
				delayB = (int(maxdelayB * roomsize));
				for (count=2; count <= max; count++) {if ( delayB != count && delayB % count == 0 ) {delayB += 1; count=2;}} //try for primeish Bs
				if (delayB > maxdelayB) delayB = maxdelayB; //insanitycheck
				for(count = alpBR; count < 14617; count++) {aBR[count] = 0.0;}
				for(count = outBR; count < 14617; count++) {oBR[count] = 0.0;}
				break;
				
			case 3:
				delayC = (int(maxdelayC * roomsize));
				for (count=2; count <= max; count++) {if ( delayC != count && delayC % count == 0 ) {delayC += 1; count=2;}} //try for primeish Cs
				if (delayC > maxdelayC) delayC = maxdelayC; //insanitycheck
				for(count = alpCR; count < 14357; count++) {aCR[count] = 0.0;}
				for(count = outCR; count < 14357; count++) {oCR[count] = 0.0;}
				break;
				
			case 4:
				delayD = (int(maxdelayD * roomsize));
				for (count=2; count <= max; count++) {if ( delayD != count && delayD % count == 0 ) {delayD += 1; count=2;}} //try for primeish Ds
				if (delayD > maxdelayD) delayD = maxdelayD; //insanitycheck
				for(count = alpDR; count < 13817; count++) {aDR[count] = 0.0;}
				for(count = outDR; count < 13817; count++) {oDR[count] = 0.0;}
				break;
				
			case 5:
				delayE = (int(maxdelayE * roomsize));
				for (count=2; count <= max; count++) {if ( delayE != count && delayE % count == 0 ) {delayE += 1; count=2;}} //try for primeish Es
				if (delayE > maxdelayE) delayE = maxdelayE; //insanitycheck
				for(count = alpER; count < 13561; count++) {aER[count] = 0.0;}
				for(count = outER; count < 13561; count++) {oER[count] = 0.0;}
				break;
				
			case 6:
				delayF = (int(maxdelayF * roomsize));
				for (count=2; count <= max; count++) {if ( delayF != count && delayF % count == 0 ) {delayF += 1; count=2;}} //try for primeish Fs
				if (delayF > maxdelayF) delayF = maxdelayF; //insanitycheck
				for(count = alpFR; count < 13045; count++) {aFR[count] = 0.0;}
				for(count = outFR; count < 13045; count++) {oFR[count] = 0.0;}
				break;
				
			case 7:
				delayG = (int(maxdelayG * roomsize));
				for (count=2; count <= max; count++) {if ( delayG != count && delayG % count == 0 ) {delayG += 1; count=2;}} //try for primeish Gs
				if (delayG > maxdelayG) delayG = maxdelayG; //insanitycheck
				for(count = alpGR; count < 11965; count++) {aGR[count] = 0.0;}
				for(count = outGR; count < 11965; count++) {oGR[count] = 0.0;}
				break;
				
			case 8:
				delayH = (int(maxdelayH * roomsize));
				for (count=2; count <= max; count++) {if ( delayH != count && delayH % count == 0 ) {delayH += 1; count=2;}} //try for primeish Hs
				if (delayH > maxdelayH) delayH = maxdelayH; //insanitycheck
				for(count = alpHR; count < 11129; count++) {aHR[count] = 0.0;}
				for(count = outHR; count < 11129; count++) {oHR[count] = 0.0;}
				break;
				
			case 9:
				delayI = (int(maxdelayI * roomsize));
				for (count=2; count <= max; count++) {if ( delayI != count && delayI % count == 0 ) {delayI += 1; count=2;}} //try for primeish Is
				if (delayI > maxdelayI) delayI = maxdelayI; //insanitycheck
				for(count = alpIR; count < 10597; count++) {aIR[count] = 0.0;}
				for(count = outIR; count < 10597; count++) {oIR[count] = 0.0;}
				break;
				
			case 10:
				delayJ = (int(maxdelayJ * roomsize));
				for (count=2; count <= max; count++) {if ( delayJ != count && delayJ % count == 0 ) {delayJ += 1; count=2;}} //try for primeish Js
				if (delayJ > maxdelayJ) delayJ = maxdelayJ; //insanitycheck
				for(count = alpJR; count < 9809; count++) {aJR[count] = 0.0;}
				for(count = outJR; count < 9809; count++) {oJR[count] = 0.0;}
				break;
				
			case 11:
				delayK = (int(maxdelayK * roomsize));
				for (count=2; count <= max; count++) {if ( delayK != count && delayK % count == 0 ) {delayK += 1; count=2;}} //try for primeish Ks
				if (delayK > maxdelayK) delayK = maxdelayK; //insanitycheck
				for(count = alpKR; count < 9521; count++) {aKR[count] = 0.0;}
				for(count = outKR; count < 9521; count++) {oKR[count] = 0.0;}
				break;
				
			case 12:
				delayL = (int(maxdelayL * roomsize));
				for (count=2; count <= max; count++) {if ( delayL != count && delayL % count == 0 ) {delayL += 1; count=2;}} //try for primeish Ls
				if (delayL > maxdelayL) delayL = maxdelayL; //insanitycheck
				for(count = alpLR; count < 8981; count++) {aLR[count] = 0.0;}
				for(count = outLR; count < 8981; count++) {oLR[count] = 0.0;}
				break;
				
			case 13:
				delayM = (int(maxdelayM * roomsize));
				for (count=2; count <= max; count++) {if ( delayM != count && delayM % count == 0 ) {delayM += 1; count=2;}} //try for primeish Ms
				if (delayM > maxdelayM) delayM = maxdelayM; //insanitycheck
				for(count = alpMR; count < 8785; count++) {aMR[count] = 0.0;}
				for(count = outMR; count < 8785; count++) {oMR[count] = 0.0;}
				break;
				
			case 14:
				delayN = (int(maxdelayN * roomsize));
				for (count=2; count <= max; count++) {if ( delayN != count && delayN % count == 0 ) {delayN += 1; count=2;}} //try for primeish Ns
				if (delayN > maxdelayN) delayN = maxdelayN; //insanitycheck
				for(count = alpNR; count < 8461; count++) {aNR[count] = 0.0;}
				for(count = outNR; count < 8461; count++) {oNR[count] = 0.0;}
				break;
				
			case 15:
				delayO = (int(maxdelayO * roomsize));
				for (count=2; count <= max; count++) {if ( delayO != count && delayO % count == 0 ) {delayO += 1; count=2;}} //try for primeish Os
				if (delayO > maxdelayO) delayO = maxdelayO; //insanitycheck
				for(count = alpOR; count < 8309; count++) {aOR[count] = 0.0;}
				for(count = outOR; count < 8309; count++) {oOR[count] = 0.0;}
				break;
				
			case 16:
				delayP = (int(maxdelayP * roomsize));
				for (count=2; count <= max; count++) {if ( delayP != count && delayP % count == 0 ) {delayP += 1; count=2;}} //try for primeish Ps
				if (delayP > maxdelayP) delayP = maxdelayP; //insanitycheck
				for(count = alpPR; count < 7981; count++) {aPR[count] = 0.0;}
				for(count = outPR; count < 7981; count++) {oPR[count] = 0.0;}
				break;
				
			case 17:
				delayQ = (int(maxdelayQ * roomsize));
				for (count=2; count <= max; count++) {if ( delayQ != count && delayQ % count == 0 ) {delayQ += 1; count=2;}} //try for primeish Qs
				if (delayQ > maxdelayQ) delayQ = maxdelayQ; //insanitycheck
				for(count = alpQR; count < 7321; count++) {aQR[count] = 0.0;}
				for(count = outQR; count < 7321; count++) {oQR[count] = 0.0;}
				break;
				
			case 18:
				delayR = (int(maxdelayR * roomsize));
				for (count=2; count <= max; count++) {if ( delayR != count && delayR % count == 0 ) {delayR += 1; count=2;}} //try for primeish Rs
				if (delayR > maxdelayR) delayR = maxdelayR; //insanitycheck
				for(count = alpRR; count < 6817; count++) {aRR[count] = 0.0;}
				for(count = outRR; count < 6817; count++) {oRR[count] = 0.0;}
				break;
				
			case 19:
				delayS = (int(maxdelayS * roomsize));
				for (count=2; count <= max; count++) {if ( delayS != count && delayS % count == 0 ) {delayS += 1; count=2;}} //try for primeish Ss
				if (delayS > maxdelayS) delayS = maxdelayS; //insanitycheck
				for(count = alpSR; count < 6505; count++) {aSR[count] = 0.0;}
				for(count = outSR; count < 6505; count++) {oSR[count] = 0.0;}
				break;
				
			case 20:
				delayT = (int(maxdelayT * roomsize));
				for (count=2; count <= max; count++) {if ( delayT != count && delayT % count == 0 ) {delayT += 1; count=2;}} //try for primeish Ts
				if (delayT > maxdelayT) delayT = maxdelayT; //insanitycheck
				for(count = alpTR; count < 6001; count++) {aTR[count] = 0.0;}
				for(count = outTR; count < 6001; count++) {oTR[count] = 0.0;}
				break;
				
			case 21:
				delayU = (int(maxdelayU * roomsize));
				for (count=2; count <= max; count++) {if ( delayU != count && delayU % count == 0 ) {delayU += 1; count=2;}} //try for primeish Us
				if (delayU > maxdelayU) delayU = maxdelayU; //insanitycheck
				for(count = alpUR; count < 5837; count++) {aUR[count] = 0.0;}
				for(count = outUR; count < 5837; count++) {oUR[count] = 0.0;}
				break;
				
			case 22:
				delayV = (int(maxdelayV * roomsize));
				for (count=2; count <= max; count++) {if ( delayV != count && delayV % count == 0 ) {delayV += 1; count=2;}} //try for primeish Vs
				if (delayV > maxdelayV) delayV = maxdelayV; //insanitycheck
				for(count = alpVR; count < 5501; count++) {aVR[count] = 0.0;}
				for(count = outVR; count < 5501; count++) {oVR[count] = 0.0;}
				break;
				
			case 23:
				delayW = (int(maxdelayW * roomsize));
				for (count=2; count <= max; count++) {if ( delayW != count && delayW % count == 0 ) {delayW += 1; count=2;}} //try for primeish Ws
				if (delayW > maxdelayW) delayW = maxdelayW; //insanitycheck
				for(count = alpWR; count < 5009; count++) {aWR[count] = 0.0;}
				for(count = outWR; count < 5009; count++) {oWR[count] = 0.0;}
				break;
				
			case 24:
				delayX = (int(maxdelayX * roomsize));
				for (count=2; count <= max; count++) {if ( delayX != count && delayX % count == 0 ) {delayX += 1; count=2;}} //try for primeish Xs
				if (delayX > maxdelayX) delayX = maxdelayX; //insanitycheck
				for(count = alpXR; count < 4849; count++) {aXR[count] = 0.0;}
				for(count = outXR; count < 4849; count++) {oXR[count] = 0.0;}
				break;
				
			case 25:
				delayY = (int(maxdelayY * roomsize));
				for (count=2; count <= max; count++) {if ( delayY != count && delayY % count == 0 ) {delayY += 1; count=2;}} //try for primeish Ys
				if (delayY > maxdelayY) delayY = maxdelayY; //insanitycheck
				for(count = alpYR; count < 4295; count++) {aYR[count] = 0.0;}
				for(count = outYR; count < 4295; count++) {oYR[count] = 0.0;}
				break;
				
			case 26:
				delayZ = (int(maxdelayZ * roomsize));
				for (count=2; count <= max; count++) {if ( delayZ != count && delayZ % count == 0 ) {delayZ += 1; count=2;}} //try for primeish Zs
				if (delayZ > maxdelayZ) delayZ = maxdelayZ; //insanitycheck
				for(count = alpZR; count < 4179; count++) {aZR[count] = 0.0;}	
				for(count = outZR; count < 4179; count++) {oZR[count] = 0.0;}
				break;
		} //end of switch statement
		countdown--; //every buffer we'll do one of the recalculations for prime buffer sizes
	}
	
	
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		
		if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
		if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
		
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		peakL -= release;
		if (peakL < fabs(inputSampleL*2.0)) peakL = fabs(inputSampleL*2.0);
		if (peakL > 1.0) peakL = 1.0;
		peakR -= release;
		if (peakR < fabs(inputSampleR*2.0)) peakR = fabs(inputSampleR*2.0);
		if (peakR > 1.0) peakR = 1.0;
		//chase the maximum signal to incorporate the wetter/louder behavior
		//boost for more extreme effect when in use, cap it
		
		inputSampleL *= gain;
		bridgerectifier = fabs(inputSampleL);
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		inputSampleR *= gain;
		bridgerectifier = fabs(inputSampleR);
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//here we apply the ADT2 console-on-steroids trick		
		
		
		
		
		
		switch (verbtype)
		{
				
				
			case 1://Chamber
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dAL[3])/3.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dAL[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5] + dAL[6])/3.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5] + dZL[6]);		
				//output Chamber
				break;
				
				
				
				
				
			case 2:	//Spring
				
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dAL[3])/3.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dYL[1] + dAL[5] + dAL[6])/3.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dXL[1] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dWL[1] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dVL[1] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dUL[1] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dTL[1] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dSL[1] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dRL[1] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dQL[1] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dPL[1] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dOL[1] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dNL[1] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[1] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dLL[1] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dKL[1] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dJL[1] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dIL[1] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dHL[1] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dGL[1] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dFL[1] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dEL[1] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dDL[1] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dCL[1] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dBL[1] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[5] + dZL[6]);
				//output Spring
				break;
				
				
			case 3:	//Tiled
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2])/2.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2])/2.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[2])/2.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2])/2.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[2])/2.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2])/2.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dGL[2])/2.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2])/2.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2])/2.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2])/2.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[2])/2.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2])/2.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dAL[1] + dML[2])/2.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2])/2.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2])/2.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2])/2.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[2])/2.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2])/2.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dSL[2])/2.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2])/2.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2])/2.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2])/2.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dWL[2])/2.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2])/2.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2])/2.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[5])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[5])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[5])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dOL[5])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[5])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dUL[5])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5]);		
				//output Tiled
				break;
				
				
			case 4://Room
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[1]+dAL[2])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[1]+dBL[2])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[1]+dCL[2])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[1]+dDL[2])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[1]+dEL[2])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[1]+dFL[2])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[1]+dGL[2])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[1]+dHL[2])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[1]+dIL[2])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[1]+dJL[2])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[1]+dKL[2])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[1]+dLL[2])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[1]+dML[2])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[1]+dNL[2])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[1]+dOL[2])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[1]+dPL[2])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[1]+dQL[2])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[1]+dRL[2])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[1]+dSL[2])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[1]+dTL[2])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[1]+dUL[2])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[1]+dVL[2])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[1]+dWL[2])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[1]+dXL[2])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[1]+dYL[2])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dBL[4] * dryness);		
				inputSampleL += (dCL[4] * dryness);		
				inputSampleL += dDL[4];		
				inputSampleL += dEL[4];		
				inputSampleL += dFL[4];		
				inputSampleL += dGL[4];		
				inputSampleL += dHL[4];		
				inputSampleL += dIL[4];		
				inputSampleL += dJL[4];		
				inputSampleL += dKL[4];		
				inputSampleL += dLL[4];		
				inputSampleL += dML[4];		
				inputSampleL += dNL[4];		
				inputSampleL += dOL[4];		
				inputSampleL += dPL[4];		
				inputSampleL += dQL[4];		
				inputSampleL += dRL[4];		
				inputSampleL += dSL[4];		
				inputSampleL += dTL[4];		
				inputSampleL += dUL[4];		
				inputSampleL += dVL[4];		
				inputSampleL += dWL[4];		
				inputSampleL += dXL[4];		
				inputSampleL += dYL[4];		
				inputSampleL += (dZL[4] * wetness);
				
				inputSampleL += (dBL[5] * dryness);		
				inputSampleL += (dCL[5] * dryness);		
				inputSampleL += dDL[5];		
				inputSampleL += dEL[5];		
				inputSampleL += dFL[5];		
				inputSampleL += dGL[5];		
				inputSampleL += dHL[5];		
				inputSampleL += dIL[5];		
				inputSampleL += dJL[5];		
				inputSampleL += dKL[5];		
				inputSampleL += dLL[5];		
				inputSampleL += dML[5];		
				inputSampleL += dNL[5];		
				inputSampleL += dOL[5];		
				inputSampleL += dPL[5];		
				inputSampleL += dQL[5];		
				inputSampleL += dRL[5];		
				inputSampleL += dSL[5];		
				inputSampleL += dTL[5];		
				inputSampleL += dUL[5];		
				inputSampleL += dVL[5];		
				inputSampleL += dWL[5];		
				inputSampleL += dXL[5];		
				inputSampleL += dYL[5];		
				inputSampleL += (dZL[5] * wetness);
				
				inputSampleL /= (26.0 + (wetness * 4.0));
				//output Room effect
				break;
				
				
				
				
				
				
			case 5:	//Stretch	
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2])/2.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2])/2.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2])/2.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2])/2.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2])/2.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2])/2.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2])/2.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2])/2.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2])/2.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2])/2.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2])/2.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2])/2.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2])/2.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2])/2.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2])/2.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2])/2.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2])/2.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2])/2.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2])/2.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2])/2.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2])/2.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2])/2.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2])/2.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2])/2.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2])/2.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[4] + dCL[5])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[4] + dEL[5])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[4] + dKL[5])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[4] + dOL[5])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[4] + dQL[5])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[4] + dUL[5])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5])/2.0;		
				//output Stretch unrealistic but smooth fake Paulstretch
				break;				
				
				
			case 6:	//Zarathustra	
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dZL[3])/3.0; //add feedback
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dCL[1] + dAL[5] + dAL[6])/3.0;  //note, feeding in dry again for a little more clarity!
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[4] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[4] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[4] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[4] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[4] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[4] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5] + dZL[6]);		
				//output Zarathustra infinite space verb
				break;
				
		}
		//end big switch for verb type
		
		switch (verbtype)
		{
				
				
			case 1://Chamber
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dAR[3])/3.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dAR[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5] + dAR[6])/3.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dAR[1] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5] + dZR[6]);		
				//output Chamber
				break;
				
				
				
				
				
			case 2:	//Spring
				
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dAR[3])/3.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dYR[1] + dAR[5] + dAR[6])/3.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dXR[1] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dWR[1] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dVR[1] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dUR[1] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dTR[1] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dSR[1] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dRR[1] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dQR[1] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dPR[1] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dOR[1] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dNR[1] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dLR[1] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dKR[1] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dJR[1] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dIR[1] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dHR[1] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dGR[1] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dFR[1] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dER[1] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dDR[1] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dCR[1] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dBR[1] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[5] + dZR[6]);
				//output Spring
				break;
				
				
			case 3:	//Tiled
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2])/2.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2])/2.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[2])/2.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2])/2.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dAR[1] + dER[2])/2.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2])/2.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dGR[2])/2.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2])/2.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2])/2.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2])/2.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[2])/2.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2])/2.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dMR[2])/2.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2])/2.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2])/2.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2])/2.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[2])/2.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2])/2.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dSR[2])/2.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2])/2.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2])/2.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2])/2.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dWR[2])/2.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2])/2.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2])/2.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[5])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dAR[1] + dER[5])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[5])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dOR[5])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[5])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dUR[5])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5]);		
				//output Tiled
				break;
				
				
			case 4://Room
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[1]+dAR[2])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[1]+dBR[2])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[1]+dCR[2])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[1]+dDR[2])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[1]+dER[2])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[1]+dFR[2])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[1]+dGR[2])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[1]+dHR[2])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[1]+dIR[2])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[1]+dJR[2])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[1]+dKR[2])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[1]+dLR[2])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[1]+dMR[2])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[1]+dNR[2])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[1]+dOR[2])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[1]+dPR[2])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[1]+dQR[2])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[1]+dRR[2])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[1]+dSR[2])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[1]+dTR[2])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[1]+dUR[2])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[1]+dVR[2])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[1]+dWR[2])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[1]+dXR[2])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[1]+dYR[2])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dBR[4] * dryness);		
				inputSampleR += (dCR[4] * dryness);		
				inputSampleR += dDR[4];		
				inputSampleR += dER[4];		
				inputSampleR += dFR[4];		
				inputSampleR += dGR[4];		
				inputSampleR += dHR[4];		
				inputSampleR += dIR[4];		
				inputSampleR += dJR[4];		
				inputSampleR += dKR[4];		
				inputSampleR += dLR[4];		
				inputSampleR += dMR[4];		
				inputSampleR += dNR[4];		
				inputSampleR += dOR[4];		
				inputSampleR += dPR[4];		
				inputSampleR += dQR[4];		
				inputSampleR += dRR[4];		
				inputSampleR += dSR[4];		
				inputSampleR += dTR[4];		
				inputSampleR += dUR[4];		
				inputSampleR += dVR[4];		
				inputSampleR += dWR[4];		
				inputSampleR += dXR[4];		
				inputSampleR += dYR[4];		
				inputSampleR += (dZR[4] * wetness);
				
				inputSampleR += (dBR[5] * dryness);		
				inputSampleR += (dCR[5] * dryness);		
				inputSampleR += dDR[5];		
				inputSampleR += dER[5];		
				inputSampleR += dFR[5];		
				inputSampleR += dGR[5];		
				inputSampleR += dHR[5];		
				inputSampleR += dIR[5];		
				inputSampleR += dJR[5];		
				inputSampleR += dKR[5];		
				inputSampleR += dLR[5];		
				inputSampleR += dMR[5];		
				inputSampleR += dNR[5];		
				inputSampleR += dOR[5];		
				inputSampleR += dPR[5];		
				inputSampleR += dQR[5];		
				inputSampleR += dRR[5];		
				inputSampleR += dSR[5];		
				inputSampleR += dTR[5];		
				inputSampleR += dUR[5];		
				inputSampleR += dVR[5];		
				inputSampleR += dWR[5];		
				inputSampleR += dXR[5];		
				inputSampleR += dYR[5];		
				inputSampleR += (dZR[5] * wetness);
				
				inputSampleR /= (26.0 + (wetness * 4.0));
				//output Room effect
				break;
				
				
				
				
				
				
			case 5:	//Stretch	
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2])/2.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2])/2.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2])/2.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2])/2.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2])/2.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2])/2.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2])/2.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2])/2.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2])/2.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2])/2.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2])/2.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2])/2.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2])/2.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2])/2.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2])/2.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2])/2.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2])/2.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2])/2.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2])/2.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2])/2.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2])/2.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2])/2.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2])/2.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2])/2.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2])/2.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[4] + dCR[5])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[4] + dER[5])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[4] + dKR[5])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[4] + dOR[5])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[4] + dQR[5])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[4] + dUR[5])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5])/2.0;		
				//output Stretch unrealistic but smooth fake Paulstretch
				break;				
				
				
			case 6:	//Zarathustra	
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dZR[3])/3.0; //add feedback
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dCR[1] + dAR[5] + dAR[6])/3.0;  //note, feeding in dry again for a little more clarity!
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[4] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[4] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[4] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[4] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[4] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[4] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5] + dZR[6]);		
				//output Zarathustra infinite space verb
				break;
				
		}
		//end big switch for verb type		
		
		
		bridgerectifier = fabs(inputSampleL);
		bridgerectifier = 1.0-cos(bridgerectifier);
		if (inputSampleL > 0) inputSampleL -= bridgerectifier;
		else inputSampleL += bridgerectifier;
		inputSampleL /= gain;		
		bridgerectifier = fabs(inputSampleR);
		bridgerectifier = 1.0-cos(bridgerectifier);
		if (inputSampleR > 0) inputSampleR -= bridgerectifier;
		else inputSampleR += bridgerectifier;
		inputSampleR /= gain;		
		//here we apply the ADT2 'console on steroids' trick
		
		wetness = wetnesstarget;
		//setting up verb wetness to be manipulated by gate and peak
		
		wetness *= peakL;
		//but we only use peak (indirect) to deal with dry/wet, so that it'll manipulate the dry/wet like we want
		
		drySampleL *= (1.0-wetness);
		inputSampleL *= wetness;
		inputSampleL += drySampleL;
		//here we combine the tanks with the dry signal
		
		wetness = wetnesstarget;
		//setting up verb wetness to be manipulated by gate and peak
		
		wetness *= peakR;
		//but we only use peak (indirect) to deal with dry/wet, so that it'll manipulate the dry/wet like we want
		
		drySampleR *= (1.0-wetness);
		inputSampleR *= wetness;
		inputSampleR += drySampleR;
		//here we combine the tanks with the dry signal
		
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

void PocketVerbs::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	int verbtype = (VstInt32)( A * 5.999 )+1;
	
	double roomsize = (pow(B,2)*1.9)+0.1;
	
	double release = 0.00008 * pow(C,3);
	if (release == 0.0) {peakL = 1.0; peakR = 1.0;}
	double wetnesstarget = D;
	double dryness = (1.0 - wetnesstarget);
	//verbs use base wetness value internally
	double wetness = wetnesstarget;
	double constallpass = 0.618033988749894848204586; //golden ratio!
	int allpasstemp;
	int count;
	int max = 70; //biggest divisor to test primes against
	double bridgerectifier;
	double gain = 0.5+(wetnesstarget*0.5); //dryer for less verb drive
	//used as an aux, saturates when fed high levels
	
	//remap values to primes input number in question is 'i'
	//max is the largest prime we care about- HF interactions more interesting than the big numbers
	//pushing values larger and larger until we have a result
	//for (primetest=2; primetest <= max; primetest++) {if ( i!=primetest && i % primetest == 0 ) {i += 1; primetest=2;}}
	
	if (savedRoomsize != roomsize) {savedRoomsize = roomsize; countdown = 26;} //kick off the adjustment which will take 26 zippernoise refreshes to complete
	
	if (countdown > 0) {switch (countdown)
		{
			case 1:
				delayA = (int(maxdelayA * roomsize));
				for (count=2; count <= max; count++) {if ( delayA != count && delayA % count == 0 ) {delayA += 1; count=2;}} //try for primeish As
				if (delayA > maxdelayA) delayA = maxdelayA; //insanitycheck
				for(count = alpAL; count < 15149; count++) {aAL[count] = 0.0;}
				for(count = outAL; count < 15149; count++) {oAL[count] = 0.0;}
				break;
				
			case 2:
				delayB = (int(maxdelayB * roomsize));
				for (count=2; count <= max; count++) {if ( delayB != count && delayB % count == 0 ) {delayB += 1; count=2;}} //try for primeish Bs
				if (delayB > maxdelayB) delayB = maxdelayB; //insanitycheck
				for(count = alpBL; count < 14617; count++) {aBL[count] = 0.0;}
				for(count = outBL; count < 14617; count++) {oBL[count] = 0.0;}
				break;
				
			case 3:
				delayC = (int(maxdelayC * roomsize));
				for (count=2; count <= max; count++) {if ( delayC != count && delayC % count == 0 ) {delayC += 1; count=2;}} //try for primeish Cs
				if (delayC > maxdelayC) delayC = maxdelayC; //insanitycheck
				for(count = alpCL; count < 14357; count++) {aCL[count] = 0.0;}
				for(count = outCL; count < 14357; count++) {oCL[count] = 0.0;}
				break;
				
			case 4:
				delayD = (int(maxdelayD * roomsize));
				for (count=2; count <= max; count++) {if ( delayD != count && delayD % count == 0 ) {delayD += 1; count=2;}} //try for primeish Ds
				if (delayD > maxdelayD) delayD = maxdelayD; //insanitycheck
				for(count = alpDL; count < 13817; count++) {aDL[count] = 0.0;}
				for(count = outDL; count < 13817; count++) {oDL[count] = 0.0;}
				break;
				
			case 5:
				delayE = (int(maxdelayE * roomsize));
				for (count=2; count <= max; count++) {if ( delayE != count && delayE % count == 0 ) {delayE += 1; count=2;}} //try for primeish Es
				if (delayE > maxdelayE) delayE = maxdelayE; //insanitycheck
				for(count = alpEL; count < 13561; count++) {aEL[count] = 0.0;}
				for(count = outEL; count < 13561; count++) {oEL[count] = 0.0;}
				break;
				
			case 6:
				delayF = (int(maxdelayF * roomsize));
				for (count=2; count <= max; count++) {if ( delayF != count && delayF % count == 0 ) {delayF += 1; count=2;}} //try for primeish Fs
				if (delayF > maxdelayF) delayF = maxdelayF; //insanitycheck
				for(count = alpFL; count < 13045; count++) {aFL[count] = 0.0;}
				for(count = outFL; count < 13045; count++) {oFL[count] = 0.0;}
				break;
				
			case 7:
				delayG = (int(maxdelayG * roomsize));
				for (count=2; count <= max; count++) {if ( delayG != count && delayG % count == 0 ) {delayG += 1; count=2;}} //try for primeish Gs
				if (delayG > maxdelayG) delayG = maxdelayG; //insanitycheck
				for(count = alpGL; count < 11965; count++) {aGL[count] = 0.0;}
				for(count = outGL; count < 11965; count++) {oGL[count] = 0.0;}
				break;
				
			case 8:
				delayH = (int(maxdelayH * roomsize));
				for (count=2; count <= max; count++) {if ( delayH != count && delayH % count == 0 ) {delayH += 1; count=2;}} //try for primeish Hs
				if (delayH > maxdelayH) delayH = maxdelayH; //insanitycheck
				for(count = alpHL; count < 11129; count++) {aHL[count] = 0.0;}
				for(count = outHL; count < 11129; count++) {oHL[count] = 0.0;}
				break;
				
			case 9:
				delayI = (int(maxdelayI * roomsize));
				for (count=2; count <= max; count++) {if ( delayI != count && delayI % count == 0 ) {delayI += 1; count=2;}} //try for primeish Is
				if (delayI > maxdelayI) delayI = maxdelayI; //insanitycheck
				for(count = alpIL; count < 10597; count++) {aIL[count] = 0.0;}
				for(count = outIL; count < 10597; count++) {oIL[count] = 0.0;}
				break;
				
			case 10:
				delayJ = (int(maxdelayJ * roomsize));
				for (count=2; count <= max; count++) {if ( delayJ != count && delayJ % count == 0 ) {delayJ += 1; count=2;}} //try for primeish Js
				if (delayJ > maxdelayJ) delayJ = maxdelayJ; //insanitycheck
				for(count = alpJL; count < 9809; count++) {aJL[count] = 0.0;}
				for(count = outJL; count < 9809; count++) {oJL[count] = 0.0;}
				break;
				
			case 11:
				delayK = (int(maxdelayK * roomsize));
				for (count=2; count <= max; count++) {if ( delayK != count && delayK % count == 0 ) {delayK += 1; count=2;}} //try for primeish Ks
				if (delayK > maxdelayK) delayK = maxdelayK; //insanitycheck
				for(count = alpKL; count < 9521; count++) {aKL[count] = 0.0;}
				for(count = outKL; count < 9521; count++) {oKL[count] = 0.0;}
				break;
				
			case 12:
				delayL = (int(maxdelayL * roomsize));
				for (count=2; count <= max; count++) {if ( delayL != count && delayL % count == 0 ) {delayL += 1; count=2;}} //try for primeish Ls
				if (delayL > maxdelayL) delayL = maxdelayL; //insanitycheck
				for(count = alpLL; count < 8981; count++) {aLL[count] = 0.0;}
				for(count = outLL; count < 8981; count++) {oLL[count] = 0.0;}
				break;
				
			case 13:
				delayM = (int(maxdelayM * roomsize));
				for (count=2; count <= max; count++) {if ( delayM != count && delayM % count == 0 ) {delayM += 1; count=2;}} //try for primeish Ms
				if (delayM > maxdelayM) delayM = maxdelayM; //insanitycheck
				for(count = alpML; count < 8785; count++) {aML[count] = 0.0;}
				for(count = outML; count < 8785; count++) {oML[count] = 0.0;}
				break;
				
			case 14:
				delayN = (int(maxdelayN * roomsize));
				for (count=2; count <= max; count++) {if ( delayN != count && delayN % count == 0 ) {delayN += 1; count=2;}} //try for primeish Ns
				if (delayN > maxdelayN) delayN = maxdelayN; //insanitycheck
				for(count = alpNL; count < 8461; count++) {aNL[count] = 0.0;}
				for(count = outNL; count < 8461; count++) {oNL[count] = 0.0;}
				break;
				
			case 15:
				delayO = (int(maxdelayO * roomsize));
				for (count=2; count <= max; count++) {if ( delayO != count && delayO % count == 0 ) {delayO += 1; count=2;}} //try for primeish Os
				if (delayO > maxdelayO) delayO = maxdelayO; //insanitycheck
				for(count = alpOL; count < 8309; count++) {aOL[count] = 0.0;}
				for(count = outOL; count < 8309; count++) {oOL[count] = 0.0;}
				break;
				
			case 16:
				delayP = (int(maxdelayP * roomsize));
				for (count=2; count <= max; count++) {if ( delayP != count && delayP % count == 0 ) {delayP += 1; count=2;}} //try for primeish Ps
				if (delayP > maxdelayP) delayP = maxdelayP; //insanitycheck
				for(count = alpPL; count < 7981; count++) {aPL[count] = 0.0;}
				for(count = outPL; count < 7981; count++) {oPL[count] = 0.0;}
				break;
				
			case 17:
				delayQ = (int(maxdelayQ * roomsize));
				for (count=2; count <= max; count++) {if ( delayQ != count && delayQ % count == 0 ) {delayQ += 1; count=2;}} //try for primeish Qs
				if (delayQ > maxdelayQ) delayQ = maxdelayQ; //insanitycheck
				for(count = alpQL; count < 7321; count++) {aQL[count] = 0.0;}
				for(count = outQL; count < 7321; count++) {oQL[count] = 0.0;}
				break;
				
			case 18:
				delayR = (int(maxdelayR * roomsize));
				for (count=2; count <= max; count++) {if ( delayR != count && delayR % count == 0 ) {delayR += 1; count=2;}} //try for primeish Rs
				if (delayR > maxdelayR) delayR = maxdelayR; //insanitycheck
				for(count = alpRL; count < 6817; count++) {aRL[count] = 0.0;}
				for(count = outRL; count < 6817; count++) {oRL[count] = 0.0;}
				break;
				
			case 19:
				delayS = (int(maxdelayS * roomsize));
				for (count=2; count <= max; count++) {if ( delayS != count && delayS % count == 0 ) {delayS += 1; count=2;}} //try for primeish Ss
				if (delayS > maxdelayS) delayS = maxdelayS; //insanitycheck
				for(count = alpSL; count < 6505; count++) {aSL[count] = 0.0;}
				for(count = outSL; count < 6505; count++) {oSL[count] = 0.0;}
				break;
				
			case 20:
				delayT = (int(maxdelayT * roomsize));
				for (count=2; count <= max; count++) {if ( delayT != count && delayT % count == 0 ) {delayT += 1; count=2;}} //try for primeish Ts
				if (delayT > maxdelayT) delayT = maxdelayT; //insanitycheck
				for(count = alpTL; count < 6001; count++) {aTL[count] = 0.0;}
				for(count = outTL; count < 6001; count++) {oTL[count] = 0.0;}
				break;
				
			case 21:
				delayU = (int(maxdelayU * roomsize));
				for (count=2; count <= max; count++) {if ( delayU != count && delayU % count == 0 ) {delayU += 1; count=2;}} //try for primeish Us
				if (delayU > maxdelayU) delayU = maxdelayU; //insanitycheck
				for(count = alpUL; count < 5837; count++) {aUL[count] = 0.0;}
				for(count = outUL; count < 5837; count++) {oUL[count] = 0.0;}
				break;
				
			case 22:
				delayV = (int(maxdelayV * roomsize));
				for (count=2; count <= max; count++) {if ( delayV != count && delayV % count == 0 ) {delayV += 1; count=2;}} //try for primeish Vs
				if (delayV > maxdelayV) delayV = maxdelayV; //insanitycheck
				for(count = alpVL; count < 5501; count++) {aVL[count] = 0.0;}
				for(count = outVL; count < 5501; count++) {oVL[count] = 0.0;}
				break;
				
			case 23:
				delayW = (int(maxdelayW * roomsize));
				for (count=2; count <= max; count++) {if ( delayW != count && delayW % count == 0 ) {delayW += 1; count=2;}} //try for primeish Ws
				if (delayW > maxdelayW) delayW = maxdelayW; //insanitycheck
				for(count = alpWL; count < 5009; count++) {aWL[count] = 0.0;}
				for(count = outWL; count < 5009; count++) {oWL[count] = 0.0;}
				break;
				
			case 24:
				delayX = (int(maxdelayX * roomsize));
				for (count=2; count <= max; count++) {if ( delayX != count && delayX % count == 0 ) {delayX += 1; count=2;}} //try for primeish Xs
				if (delayX > maxdelayX) delayX = maxdelayX; //insanitycheck
				for(count = alpXL; count < 4849; count++) {aXL[count] = 0.0;}
				for(count = outXL; count < 4849; count++) {oXL[count] = 0.0;}
				break;
				
			case 25:
				delayY = (int(maxdelayY * roomsize));
				for (count=2; count <= max; count++) {if ( delayY != count && delayY % count == 0 ) {delayY += 1; count=2;}} //try for primeish Ys
				if (delayY > maxdelayY) delayY = maxdelayY; //insanitycheck
				for(count = alpYL; count < 4295; count++) {aYL[count] = 0.0;}
				for(count = outYL; count < 4295; count++) {oYL[count] = 0.0;}
				break;
				
			case 26:
				delayZ = (int(maxdelayZ * roomsize));
				for (count=2; count <= max; count++) {if ( delayZ != count && delayZ % count == 0 ) {delayZ += 1; count=2;}} //try for primeish Zs
				if (delayZ > maxdelayZ) delayZ = maxdelayZ; //insanitycheck
				for(count = alpZL; count < 4179; count++) {aZL[count] = 0.0;}	
				for(count = outZL; count < 4179; count++) {oZL[count] = 0.0;}
				break;
		} //end of switch statement
		//countdown--; we are doing this after the second one
	}
	
	if (countdown > 0) {switch (countdown)
		{
			case 1:
				delayA = (int(maxdelayA * roomsize));
				for (count=2; count <= max; count++) {if ( delayA != count && delayA % count == 0 ) {delayA += 1; count=2;}} //try for primeish As
				if (delayA > maxdelayA) delayA = maxdelayA; //insanitycheck
				for(count = alpAR; count < 15149; count++) {aAR[count] = 0.0;}
				for(count = outAR; count < 15149; count++) {oAR[count] = 0.0;}
				break;
				
			case 2:
				delayB = (int(maxdelayB * roomsize));
				for (count=2; count <= max; count++) {if ( delayB != count && delayB % count == 0 ) {delayB += 1; count=2;}} //try for primeish Bs
				if (delayB > maxdelayB) delayB = maxdelayB; //insanitycheck
				for(count = alpBR; count < 14617; count++) {aBR[count] = 0.0;}
				for(count = outBR; count < 14617; count++) {oBR[count] = 0.0;}
				break;
				
			case 3:
				delayC = (int(maxdelayC * roomsize));
				for (count=2; count <= max; count++) {if ( delayC != count && delayC % count == 0 ) {delayC += 1; count=2;}} //try for primeish Cs
				if (delayC > maxdelayC) delayC = maxdelayC; //insanitycheck
				for(count = alpCR; count < 14357; count++) {aCR[count] = 0.0;}
				for(count = outCR; count < 14357; count++) {oCR[count] = 0.0;}
				break;
				
			case 4:
				delayD = (int(maxdelayD * roomsize));
				for (count=2; count <= max; count++) {if ( delayD != count && delayD % count == 0 ) {delayD += 1; count=2;}} //try for primeish Ds
				if (delayD > maxdelayD) delayD = maxdelayD; //insanitycheck
				for(count = alpDR; count < 13817; count++) {aDR[count] = 0.0;}
				for(count = outDR; count < 13817; count++) {oDR[count] = 0.0;}
				break;
				
			case 5:
				delayE = (int(maxdelayE * roomsize));
				for (count=2; count <= max; count++) {if ( delayE != count && delayE % count == 0 ) {delayE += 1; count=2;}} //try for primeish Es
				if (delayE > maxdelayE) delayE = maxdelayE; //insanitycheck
				for(count = alpER; count < 13561; count++) {aER[count] = 0.0;}
				for(count = outER; count < 13561; count++) {oER[count] = 0.0;}
				break;
				
			case 6:
				delayF = (int(maxdelayF * roomsize));
				for (count=2; count <= max; count++) {if ( delayF != count && delayF % count == 0 ) {delayF += 1; count=2;}} //try for primeish Fs
				if (delayF > maxdelayF) delayF = maxdelayF; //insanitycheck
				for(count = alpFR; count < 13045; count++) {aFR[count] = 0.0;}
				for(count = outFR; count < 13045; count++) {oFR[count] = 0.0;}
				break;
				
			case 7:
				delayG = (int(maxdelayG * roomsize));
				for (count=2; count <= max; count++) {if ( delayG != count && delayG % count == 0 ) {delayG += 1; count=2;}} //try for primeish Gs
				if (delayG > maxdelayG) delayG = maxdelayG; //insanitycheck
				for(count = alpGR; count < 11965; count++) {aGR[count] = 0.0;}
				for(count = outGR; count < 11965; count++) {oGR[count] = 0.0;}
				break;
				
			case 8:
				delayH = (int(maxdelayH * roomsize));
				for (count=2; count <= max; count++) {if ( delayH != count && delayH % count == 0 ) {delayH += 1; count=2;}} //try for primeish Hs
				if (delayH > maxdelayH) delayH = maxdelayH; //insanitycheck
				for(count = alpHR; count < 11129; count++) {aHR[count] = 0.0;}
				for(count = outHR; count < 11129; count++) {oHR[count] = 0.0;}
				break;
				
			case 9:
				delayI = (int(maxdelayI * roomsize));
				for (count=2; count <= max; count++) {if ( delayI != count && delayI % count == 0 ) {delayI += 1; count=2;}} //try for primeish Is
				if (delayI > maxdelayI) delayI = maxdelayI; //insanitycheck
				for(count = alpIR; count < 10597; count++) {aIR[count] = 0.0;}
				for(count = outIR; count < 10597; count++) {oIR[count] = 0.0;}
				break;
				
			case 10:
				delayJ = (int(maxdelayJ * roomsize));
				for (count=2; count <= max; count++) {if ( delayJ != count && delayJ % count == 0 ) {delayJ += 1; count=2;}} //try for primeish Js
				if (delayJ > maxdelayJ) delayJ = maxdelayJ; //insanitycheck
				for(count = alpJR; count < 9809; count++) {aJR[count] = 0.0;}
				for(count = outJR; count < 9809; count++) {oJR[count] = 0.0;}
				break;
				
			case 11:
				delayK = (int(maxdelayK * roomsize));
				for (count=2; count <= max; count++) {if ( delayK != count && delayK % count == 0 ) {delayK += 1; count=2;}} //try for primeish Ks
				if (delayK > maxdelayK) delayK = maxdelayK; //insanitycheck
				for(count = alpKR; count < 9521; count++) {aKR[count] = 0.0;}
				for(count = outKR; count < 9521; count++) {oKR[count] = 0.0;}
				break;
				
			case 12:
				delayL = (int(maxdelayL * roomsize));
				for (count=2; count <= max; count++) {if ( delayL != count && delayL % count == 0 ) {delayL += 1; count=2;}} //try for primeish Ls
				if (delayL > maxdelayL) delayL = maxdelayL; //insanitycheck
				for(count = alpLR; count < 8981; count++) {aLR[count] = 0.0;}
				for(count = outLR; count < 8981; count++) {oLR[count] = 0.0;}
				break;
				
			case 13:
				delayM = (int(maxdelayM * roomsize));
				for (count=2; count <= max; count++) {if ( delayM != count && delayM % count == 0 ) {delayM += 1; count=2;}} //try for primeish Ms
				if (delayM > maxdelayM) delayM = maxdelayM; //insanitycheck
				for(count = alpMR; count < 8785; count++) {aMR[count] = 0.0;}
				for(count = outMR; count < 8785; count++) {oMR[count] = 0.0;}
				break;
				
			case 14:
				delayN = (int(maxdelayN * roomsize));
				for (count=2; count <= max; count++) {if ( delayN != count && delayN % count == 0 ) {delayN += 1; count=2;}} //try for primeish Ns
				if (delayN > maxdelayN) delayN = maxdelayN; //insanitycheck
				for(count = alpNR; count < 8461; count++) {aNR[count] = 0.0;}
				for(count = outNR; count < 8461; count++) {oNR[count] = 0.0;}
				break;
				
			case 15:
				delayO = (int(maxdelayO * roomsize));
				for (count=2; count <= max; count++) {if ( delayO != count && delayO % count == 0 ) {delayO += 1; count=2;}} //try for primeish Os
				if (delayO > maxdelayO) delayO = maxdelayO; //insanitycheck
				for(count = alpOR; count < 8309; count++) {aOR[count] = 0.0;}
				for(count = outOR; count < 8309; count++) {oOR[count] = 0.0;}
				break;
				
			case 16:
				delayP = (int(maxdelayP * roomsize));
				for (count=2; count <= max; count++) {if ( delayP != count && delayP % count == 0 ) {delayP += 1; count=2;}} //try for primeish Ps
				if (delayP > maxdelayP) delayP = maxdelayP; //insanitycheck
				for(count = alpPR; count < 7981; count++) {aPR[count] = 0.0;}
				for(count = outPR; count < 7981; count++) {oPR[count] = 0.0;}
				break;
				
			case 17:
				delayQ = (int(maxdelayQ * roomsize));
				for (count=2; count <= max; count++) {if ( delayQ != count && delayQ % count == 0 ) {delayQ += 1; count=2;}} //try for primeish Qs
				if (delayQ > maxdelayQ) delayQ = maxdelayQ; //insanitycheck
				for(count = alpQR; count < 7321; count++) {aQR[count] = 0.0;}
				for(count = outQR; count < 7321; count++) {oQR[count] = 0.0;}
				break;
				
			case 18:
				delayR = (int(maxdelayR * roomsize));
				for (count=2; count <= max; count++) {if ( delayR != count && delayR % count == 0 ) {delayR += 1; count=2;}} //try for primeish Rs
				if (delayR > maxdelayR) delayR = maxdelayR; //insanitycheck
				for(count = alpRR; count < 6817; count++) {aRR[count] = 0.0;}
				for(count = outRR; count < 6817; count++) {oRR[count] = 0.0;}
				break;
				
			case 19:
				delayS = (int(maxdelayS * roomsize));
				for (count=2; count <= max; count++) {if ( delayS != count && delayS % count == 0 ) {delayS += 1; count=2;}} //try for primeish Ss
				if (delayS > maxdelayS) delayS = maxdelayS; //insanitycheck
				for(count = alpSR; count < 6505; count++) {aSR[count] = 0.0;}
				for(count = outSR; count < 6505; count++) {oSR[count] = 0.0;}
				break;
				
			case 20:
				delayT = (int(maxdelayT * roomsize));
				for (count=2; count <= max; count++) {if ( delayT != count && delayT % count == 0 ) {delayT += 1; count=2;}} //try for primeish Ts
				if (delayT > maxdelayT) delayT = maxdelayT; //insanitycheck
				for(count = alpTR; count < 6001; count++) {aTR[count] = 0.0;}
				for(count = outTR; count < 6001; count++) {oTR[count] = 0.0;}
				break;
				
			case 21:
				delayU = (int(maxdelayU * roomsize));
				for (count=2; count <= max; count++) {if ( delayU != count && delayU % count == 0 ) {delayU += 1; count=2;}} //try for primeish Us
				if (delayU > maxdelayU) delayU = maxdelayU; //insanitycheck
				for(count = alpUR; count < 5837; count++) {aUR[count] = 0.0;}
				for(count = outUR; count < 5837; count++) {oUR[count] = 0.0;}
				break;
				
			case 22:
				delayV = (int(maxdelayV * roomsize));
				for (count=2; count <= max; count++) {if ( delayV != count && delayV % count == 0 ) {delayV += 1; count=2;}} //try for primeish Vs
				if (delayV > maxdelayV) delayV = maxdelayV; //insanitycheck
				for(count = alpVR; count < 5501; count++) {aVR[count] = 0.0;}
				for(count = outVR; count < 5501; count++) {oVR[count] = 0.0;}
				break;
				
			case 23:
				delayW = (int(maxdelayW * roomsize));
				for (count=2; count <= max; count++) {if ( delayW != count && delayW % count == 0 ) {delayW += 1; count=2;}} //try for primeish Ws
				if (delayW > maxdelayW) delayW = maxdelayW; //insanitycheck
				for(count = alpWR; count < 5009; count++) {aWR[count] = 0.0;}
				for(count = outWR; count < 5009; count++) {oWR[count] = 0.0;}
				break;
				
			case 24:
				delayX = (int(maxdelayX * roomsize));
				for (count=2; count <= max; count++) {if ( delayX != count && delayX % count == 0 ) {delayX += 1; count=2;}} //try for primeish Xs
				if (delayX > maxdelayX) delayX = maxdelayX; //insanitycheck
				for(count = alpXR; count < 4849; count++) {aXR[count] = 0.0;}
				for(count = outXR; count < 4849; count++) {oXR[count] = 0.0;}
				break;
				
			case 25:
				delayY = (int(maxdelayY * roomsize));
				for (count=2; count <= max; count++) {if ( delayY != count && delayY % count == 0 ) {delayY += 1; count=2;}} //try for primeish Ys
				if (delayY > maxdelayY) delayY = maxdelayY; //insanitycheck
				for(count = alpYR; count < 4295; count++) {aYR[count] = 0.0;}
				for(count = outYR; count < 4295; count++) {oYR[count] = 0.0;}
				break;
				
			case 26:
				delayZ = (int(maxdelayZ * roomsize));
				for (count=2; count <= max; count++) {if ( delayZ != count && delayZ % count == 0 ) {delayZ += 1; count=2;}} //try for primeish Zs
				if (delayZ > maxdelayZ) delayZ = maxdelayZ; //insanitycheck
				for(count = alpZR; count < 4179; count++) {aZR[count] = 0.0;}	
				for(count = outZR; count < 4179; count++) {oZR[count] = 0.0;}
				break;
		} //end of switch statement
		countdown--; //every buffer we'll do one of the recalculations for prime buffer sizes
	}
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		
		if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
		if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
		
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;
		
		peakL -= release;
		if (peakL < fabs(inputSampleL*2.0)) peakL = fabs(inputSampleL*2.0);
		if (peakL > 1.0) peakL = 1.0;
		peakR -= release;
		if (peakR < fabs(inputSampleR*2.0)) peakR = fabs(inputSampleR*2.0);
		if (peakR > 1.0) peakR = 1.0;
		//chase the maximum signal to incorporate the wetter/louder behavior
		//boost for more extreme effect when in use, cap it
		
		inputSampleL *= gain;
		bridgerectifier = fabs(inputSampleL);
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		inputSampleR *= gain;
		bridgerectifier = fabs(inputSampleR);
		bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//here we apply the ADT2 console-on-steroids trick		
		
		
		switch (verbtype)
		{
				
				
			case 1://Chamber
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dAL[3])/3.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dAL[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5] + dAL[6])/3.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5] + dZL[6]);		
				//output Chamber
				break;
				
				
				
				
				
			case 2:	//Spring
				
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dAL[3])/3.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dYL[1] + dAL[5] + dAL[6])/3.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dXL[1] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dWL[1] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dVL[1] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dUL[1] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dTL[1] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dSL[1] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dRL[1] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dQL[1] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dPL[1] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dOL[1] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dNL[1] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[1] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dLL[1] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dKL[1] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dJL[1] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dIL[1] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dHL[1] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dGL[1] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dFL[1] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dEL[1] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dDL[1] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dCL[1] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dBL[1] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[5] + dZL[6]);
				//output Spring
				break;
				
				
			case 3:	//Tiled
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2])/2.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2])/2.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[2])/2.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2])/2.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[2])/2.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2])/2.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dGL[2])/2.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2])/2.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2])/2.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2])/2.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[2])/2.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2])/2.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dAL[1] + dML[2])/2.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2])/2.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2])/2.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2])/2.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[2])/2.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2])/2.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dSL[2])/2.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2])/2.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2])/2.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2])/2.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dWL[2])/2.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2])/2.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2])/2.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dCL[5])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dEL[5])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dKL[5])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dOL[5])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dQL[5])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dAL[1] + dUL[5])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5]);		
				//output Tiled
				break;
				
				
			case 4://Room
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = drySampleL;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[1]+dAL[2])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[1]+dBL[2])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[1]+dCL[2])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[1]+dDL[2])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[1]+dEL[2])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[1]+dFL[2])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[1]+dGL[2])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[1]+dHL[2])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[1]+dIL[2])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[1]+dJL[2])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[1]+dKL[2])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[1]+dLL[2])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[1]+dML[2])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[1]+dNL[2])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[1]+dOL[2])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[1]+dPL[2])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[1]+dQL[2])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[1]+dRL[2])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[1]+dSL[2])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[1]+dTL[2])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[1]+dUL[2])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[1]+dVL[2])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[1]+dWL[2])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[1]+dXL[2])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[1]+dYL[2])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dBL[4] * dryness);		
				inputSampleL += (dCL[4] * dryness);		
				inputSampleL += dDL[4];		
				inputSampleL += dEL[4];		
				inputSampleL += dFL[4];		
				inputSampleL += dGL[4];		
				inputSampleL += dHL[4];		
				inputSampleL += dIL[4];		
				inputSampleL += dJL[4];		
				inputSampleL += dKL[4];		
				inputSampleL += dLL[4];		
				inputSampleL += dML[4];		
				inputSampleL += dNL[4];		
				inputSampleL += dOL[4];		
				inputSampleL += dPL[4];		
				inputSampleL += dQL[4];		
				inputSampleL += dRL[4];		
				inputSampleL += dSL[4];		
				inputSampleL += dTL[4];		
				inputSampleL += dUL[4];		
				inputSampleL += dVL[4];		
				inputSampleL += dWL[4];		
				inputSampleL += dXL[4];		
				inputSampleL += dYL[4];		
				inputSampleL += (dZL[4] * wetness);
				
				inputSampleL += (dBL[5] * dryness);		
				inputSampleL += (dCL[5] * dryness);		
				inputSampleL += dDL[5];		
				inputSampleL += dEL[5];		
				inputSampleL += dFL[5];		
				inputSampleL += dGL[5];		
				inputSampleL += dHL[5];		
				inputSampleL += dIL[5];		
				inputSampleL += dJL[5];		
				inputSampleL += dKL[5];		
				inputSampleL += dLL[5];		
				inputSampleL += dML[5];		
				inputSampleL += dNL[5];		
				inputSampleL += dOL[5];		
				inputSampleL += dPL[5];		
				inputSampleL += dQL[5];		
				inputSampleL += dRL[5];		
				inputSampleL += dSL[5];		
				inputSampleL += dTL[5];		
				inputSampleL += dUL[5];		
				inputSampleL += dVL[5];		
				inputSampleL += dWL[5];		
				inputSampleL += dXL[5];		
				inputSampleL += dYL[5];		
				inputSampleL += (dZL[5] * wetness);
				
				inputSampleL /= (26.0 + (wetness * 4.0));
				//output Room effect
				break;
				
				
				
				
				
				
			case 5:	//Stretch	
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2])/2.0;
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2])/2.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2])/2.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2])/2.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2])/2.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2])/2.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2])/2.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2])/2.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2])/2.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2])/2.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2])/2.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2])/2.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2])/2.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2])/2.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2])/2.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2])/2.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2])/2.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2])/2.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2])/2.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2])/2.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2])/2.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2])/2.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2])/2.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2])/2.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2])/2.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dAL[4] + dAL[5])/2.0;
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5])/2.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[4] + dCL[5])/2.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5])/2.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[4] + dEL[5])/2.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5])/2.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5])/2.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5])/2.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5])/2.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5])/2.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[4] + dKL[5])/2.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5])/2.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5])/2.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5])/2.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[4] + dOL[5])/2.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5])/2.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[4] + dQL[5])/2.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5])/2.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5])/2.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5])/2.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[4] + dUL[5])/2.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5])/2.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5])/2.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5])/2.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5])/2.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5])/2.0;		
				//output Stretch unrealistic but smooth fake Paulstretch
				break;				
				
				
			case 6:	//Zarathustra	
				allpasstemp = alpAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= aAL[allpasstemp]*constallpass;
				aAL[alpAL] = inputSampleL;
				inputSampleL *= constallpass;
				alpAL--; if (alpAL < 0 || alpAL > delayA) {alpAL = delayA;}
				inputSampleL += (aAL[alpAL]);
				//allpass filter A		
				
				dAL[3] = dAL[2];
				dAL[2] = dAL[1];
				dAL[1] = inputSampleL;
				inputSampleL = (dAL[1] + dAL[2] + dZL[3])/3.0; //add feedback
				
				allpasstemp = alpBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= aBL[allpasstemp]*constallpass;
				aBL[alpBL] = inputSampleL;
				inputSampleL *= constallpass;
				alpBL--; if (alpBL < 0 || alpBL > delayB) {alpBL = delayB;}
				inputSampleL += (aBL[alpBL]);
				//allpass filter B
				
				dBL[3] = dBL[2];
				dBL[2] = dBL[1];
				dBL[1] = inputSampleL;
				inputSampleL = (dBL[1] + dBL[2] + dBL[3])/3.0;
				
				allpasstemp = alpCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= aCL[allpasstemp]*constallpass;
				aCL[alpCL] = inputSampleL;
				inputSampleL *= constallpass;
				alpCL--; if (alpCL < 0 || alpCL > delayC) {alpCL = delayC;}
				inputSampleL += (aCL[alpCL]);
				//allpass filter C
				
				dCL[3] = dCL[2];
				dCL[2] = dCL[1];
				dCL[1] = inputSampleL;
				inputSampleL = (dCL[1] + dCL[2] + dCL[3])/3.0;
				
				allpasstemp = alpDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= aDL[allpasstemp]*constallpass;
				aDL[alpDL] = inputSampleL;
				inputSampleL *= constallpass;
				alpDL--; if (alpDL < 0 || alpDL > delayD) {alpDL = delayD;}
				inputSampleL += (aDL[alpDL]);
				//allpass filter D
				
				dDL[3] = dDL[2];
				dDL[2] = dDL[1];
				dDL[1] = inputSampleL;
				inputSampleL = (dDL[1] + dDL[2] + dDL[3])/3.0;
				
				allpasstemp = alpEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= aEL[allpasstemp]*constallpass;
				aEL[alpEL] = inputSampleL;
				inputSampleL *= constallpass;
				alpEL--; if (alpEL < 0 || alpEL > delayE) {alpEL = delayE;}
				inputSampleL += (aEL[alpEL]);
				//allpass filter E
				
				dEL[3] = dEL[2];
				dEL[2] = dEL[1];
				dEL[1] = inputSampleL;
				inputSampleL = (dEL[1] + dEL[2] + dEL[3])/3.0;
				
				allpasstemp = alpFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= aFL[allpasstemp]*constallpass;
				aFL[alpFL] = inputSampleL;
				inputSampleL *= constallpass;
				alpFL--; if (alpFL < 0 || alpFL > delayF) {alpFL = delayF;}
				inputSampleL += (aFL[alpFL]);
				//allpass filter F
				
				dFL[3] = dFL[2];
				dFL[2] = dFL[1];
				dFL[1] = inputSampleL;
				inputSampleL = (dFL[1] + dFL[2] + dFL[3])/3.0;
				
				allpasstemp = alpGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= aGL[allpasstemp]*constallpass;
				aGL[alpGL] = inputSampleL;
				inputSampleL *= constallpass;
				alpGL--; if (alpGL < 0 || alpGL > delayG) {alpGL = delayG;}
				inputSampleL += (aGL[alpGL]);
				//allpass filter G
				
				dGL[3] = dGL[2];
				dGL[2] = dGL[1];
				dGL[1] = inputSampleL;
				inputSampleL = (dGL[1] + dGL[2] + dGL[3])/3.0;
				
				allpasstemp = alpHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= aHL[allpasstemp]*constallpass;
				aHL[alpHL] = inputSampleL;
				inputSampleL *= constallpass;
				alpHL--; if (alpHL < 0 || alpHL > delayH) {alpHL = delayH;}
				inputSampleL += (aHL[alpHL]);
				//allpass filter H
				
				dHL[3] = dHL[2];
				dHL[2] = dHL[1];
				dHL[1] = inputSampleL;
				inputSampleL = (dHL[1] + dHL[2] + dHL[3])/3.0;
				
				allpasstemp = alpIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= aIL[allpasstemp]*constallpass;
				aIL[alpIL] = inputSampleL;
				inputSampleL *= constallpass;
				alpIL--; if (alpIL < 0 || alpIL > delayI) {alpIL = delayI;}
				inputSampleL += (aIL[alpIL]);
				//allpass filter I
				
				dIL[3] = dIL[2];
				dIL[2] = dIL[1];
				dIL[1] = inputSampleL;
				inputSampleL = (dIL[1] + dIL[2] + dIL[3])/3.0;
				
				allpasstemp = alpJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= aJL[allpasstemp]*constallpass;
				aJL[alpJL] = inputSampleL;
				inputSampleL *= constallpass;
				alpJL--; if (alpJL < 0 || alpJL > delayJ) {alpJL = delayJ;}
				inputSampleL += (aJL[alpJL]);
				//allpass filter J
				
				dJL[3] = dJL[2];
				dJL[2] = dJL[1];
				dJL[1] = inputSampleL;
				inputSampleL = (dJL[1] + dJL[2] + dJL[3])/3.0;
				
				allpasstemp = alpKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= aKL[allpasstemp]*constallpass;
				aKL[alpKL] = inputSampleL;
				inputSampleL *= constallpass;
				alpKL--; if (alpKL < 0 || alpKL > delayK) {alpKL = delayK;}
				inputSampleL += (aKL[alpKL]);
				//allpass filter K
				
				dKL[3] = dKL[2];
				dKL[2] = dKL[1];
				dKL[1] = inputSampleL;
				inputSampleL = (dKL[1] + dKL[2] + dKL[3])/3.0;
				
				allpasstemp = alpLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= aLL[allpasstemp]*constallpass;
				aLL[alpLL] = inputSampleL;
				inputSampleL *= constallpass;
				alpLL--; if (alpLL < 0 || alpLL > delayL) {alpLL = delayL;}
				inputSampleL += (aLL[alpLL]);
				//allpass filter L
				
				dLL[3] = dLL[2];
				dLL[2] = dLL[1];
				dLL[1] = inputSampleL;
				inputSampleL = (dLL[1] + dLL[2] + dLL[3])/3.0;
				
				allpasstemp = alpML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= aML[allpasstemp]*constallpass;
				aML[alpML] = inputSampleL;
				inputSampleL *= constallpass;
				alpML--; if (alpML < 0 || alpML > delayM) {alpML = delayM;}
				inputSampleL += (aML[alpML]);
				//allpass filter M
				
				dML[3] = dML[2];
				dML[2] = dML[1];
				dML[1] = inputSampleL;
				inputSampleL = (dML[1] + dML[2] + dML[3])/3.0;
				
				allpasstemp = alpNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= aNL[allpasstemp]*constallpass;
				aNL[alpNL] = inputSampleL;
				inputSampleL *= constallpass;
				alpNL--; if (alpNL < 0 || alpNL > delayN) {alpNL = delayN;}
				inputSampleL += (aNL[alpNL]);
				//allpass filter N
				
				dNL[3] = dNL[2];
				dNL[2] = dNL[1];
				dNL[1] = inputSampleL;
				inputSampleL = (dNL[1] + dNL[2] + dNL[3])/3.0;
				
				allpasstemp = alpOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= aOL[allpasstemp]*constallpass;
				aOL[alpOL] = inputSampleL;
				inputSampleL *= constallpass;
				alpOL--; if (alpOL < 0 || alpOL > delayO) {alpOL = delayO;}
				inputSampleL += (aOL[alpOL]);
				//allpass filter O
				
				dOL[3] = dOL[2];
				dOL[2] = dOL[1];
				dOL[1] = inputSampleL;
				inputSampleL = (dOL[1] + dOL[2] + dOL[3])/3.0;
				
				allpasstemp = alpPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= aPL[allpasstemp]*constallpass;
				aPL[alpPL] = inputSampleL;
				inputSampleL *= constallpass;
				alpPL--; if (alpPL < 0 || alpPL > delayP) {alpPL = delayP;}
				inputSampleL += (aPL[alpPL]);
				//allpass filter P
				
				dPL[3] = dPL[2];
				dPL[2] = dPL[1];
				dPL[1] = inputSampleL;
				inputSampleL = (dPL[1] + dPL[2] + dPL[3])/3.0;
				
				allpasstemp = alpQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= aQL[allpasstemp]*constallpass;
				aQL[alpQL] = inputSampleL;
				inputSampleL *= constallpass;
				alpQL--; if (alpQL < 0 || alpQL > delayQ) {alpQL = delayQ;}
				inputSampleL += (aQL[alpQL]);
				//allpass filter Q
				
				dQL[3] = dQL[2];
				dQL[2] = dQL[1];
				dQL[1] = inputSampleL;
				inputSampleL = (dQL[1] + dQL[2] + dQL[3])/3.0;
				
				allpasstemp = alpRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= aRL[allpasstemp]*constallpass;
				aRL[alpRL] = inputSampleL;
				inputSampleL *= constallpass;
				alpRL--; if (alpRL < 0 || alpRL > delayR) {alpRL = delayR;}
				inputSampleL += (aRL[alpRL]);
				//allpass filter R
				
				dRL[3] = dRL[2];
				dRL[2] = dRL[1];
				dRL[1] = inputSampleL;
				inputSampleL = (dRL[1] + dRL[2] + dRL[3])/3.0;
				
				allpasstemp = alpSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= aSL[allpasstemp]*constallpass;
				aSL[alpSL] = inputSampleL;
				inputSampleL *= constallpass;
				alpSL--; if (alpSL < 0 || alpSL > delayS) {alpSL = delayS;}
				inputSampleL += (aSL[alpSL]);
				//allpass filter S
				
				dSL[3] = dSL[2];
				dSL[2] = dSL[1];
				dSL[1] = inputSampleL;
				inputSampleL = (dSL[1] + dSL[2] + dSL[3])/3.0;
				
				allpasstemp = alpTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= aTL[allpasstemp]*constallpass;
				aTL[alpTL] = inputSampleL;
				inputSampleL *= constallpass;
				alpTL--; if (alpTL < 0 || alpTL > delayT) {alpTL = delayT;}
				inputSampleL += (aTL[alpTL]);
				//allpass filter T
				
				dTL[3] = dTL[2];
				dTL[2] = dTL[1];
				dTL[1] = inputSampleL;
				inputSampleL = (dTL[1] + dTL[2] + dTL[3])/3.0;
				
				allpasstemp = alpUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= aUL[allpasstemp]*constallpass;
				aUL[alpUL] = inputSampleL;
				inputSampleL *= constallpass;
				alpUL--; if (alpUL < 0 || alpUL > delayU) {alpUL = delayU;}
				inputSampleL += (aUL[alpUL]);
				//allpass filter U
				
				dUL[3] = dUL[2];
				dUL[2] = dUL[1];
				dUL[1] = inputSampleL;
				inputSampleL = (dUL[1] + dUL[2] + dUL[3])/3.0;
				
				allpasstemp = alpVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= aVL[allpasstemp]*constallpass;
				aVL[alpVL] = inputSampleL;
				inputSampleL *= constallpass;
				alpVL--; if (alpVL < 0 || alpVL > delayV) {alpVL = delayV;}
				inputSampleL += (aVL[alpVL]);
				//allpass filter V
				
				dVL[3] = dVL[2];
				dVL[2] = dVL[1];
				dVL[1] = inputSampleL;
				inputSampleL = (dVL[1] + dVL[2] + dVL[3])/3.0;
				
				allpasstemp = alpWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= aWL[allpasstemp]*constallpass;
				aWL[alpWL] = inputSampleL;
				inputSampleL *= constallpass;
				alpWL--; if (alpWL < 0 || alpWL > delayW) {alpWL = delayW;}
				inputSampleL += (aWL[alpWL]);
				//allpass filter W
				
				dWL[3] = dWL[2];
				dWL[2] = dWL[1];
				dWL[1] = inputSampleL;
				inputSampleL = (dWL[1] + dWL[2] + dWL[3])/3.0;
				
				allpasstemp = alpXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= aXL[allpasstemp]*constallpass;
				aXL[alpXL] = inputSampleL;
				inputSampleL *= constallpass;
				alpXL--; if (alpXL < 0 || alpXL > delayX) {alpXL = delayX;}
				inputSampleL += (aXL[alpXL]);
				//allpass filter X
				
				dXL[3] = dXL[2];
				dXL[2] = dXL[1];
				dXL[1] = inputSampleL;
				inputSampleL = (dXL[1] + dXL[2] + dXL[3])/3.0;
				
				allpasstemp = alpYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= aYL[allpasstemp]*constallpass;
				aYL[alpYL] = inputSampleL;
				inputSampleL *= constallpass;
				alpYL--; if (alpYL < 0 || alpYL > delayY) {alpYL = delayY;}
				inputSampleL += (aYL[alpYL]);
				//allpass filter Y
				
				dYL[3] = dYL[2];
				dYL[2] = dYL[1];
				dYL[1] = inputSampleL;
				inputSampleL = (dYL[1] + dYL[2] + dYL[3])/3.0;
				
				allpasstemp = alpZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= aZL[allpasstemp]*constallpass;
				aZL[alpZL] = inputSampleL;
				inputSampleL *= constallpass;
				alpZL--; if (alpZL < 0 || alpZL > delayZ) {alpZL = delayZ;}
				inputSampleL += (aZL[alpZL]);
				//allpass filter Z
				
				dZL[3] = dZL[2];
				dZL[2] = dZL[1];
				dZL[1] = inputSampleL;
				inputSampleL = (dZL[1] + dZL[2] + dZL[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAL - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleL -= oAL[allpasstemp]*constallpass;
				oAL[outAL] = inputSampleL;
				inputSampleL *= constallpass;
				outAL--; if (outAL < 0 || outAL > delayA) {outAL = delayA;}
				inputSampleL += (oAL[outAL]);
				//allpass filter A		
				
				dAL[6] = dAL[5];
				dAL[5] = dAL[4];
				dAL[4] = inputSampleL;
				inputSampleL = (dCL[1] + dAL[5] + dAL[6])/3.0;  //note, feeding in dry again for a little more clarity!
				
				allpasstemp = outBL - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleL -= oBL[allpasstemp]*constallpass;
				oBL[outBL] = inputSampleL;
				inputSampleL *= constallpass;
				outBL--; if (outBL < 0 || outBL > delayB) {outBL = delayB;}
				inputSampleL += (oBL[outBL]);
				//allpass filter B
				
				dBL[6] = dBL[5];
				dBL[5] = dBL[4];
				dBL[4] = inputSampleL;
				inputSampleL = (dBL[4] + dBL[5] + dBL[6])/3.0;
				
				allpasstemp = outCL - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleL -= oCL[allpasstemp]*constallpass;
				oCL[outCL] = inputSampleL;
				inputSampleL *= constallpass;
				outCL--; if (outCL < 0 || outCL > delayC) {outCL = delayC;}
				inputSampleL += (oCL[outCL]);
				//allpass filter C
				
				dCL[6] = dCL[5];
				dCL[5] = dCL[4];
				dCL[4] = inputSampleL;
				inputSampleL = (dCL[4] + dCL[5] + dCL[6])/3.0;
				
				allpasstemp = outDL - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleL -= oDL[allpasstemp]*constallpass;
				oDL[outDL] = inputSampleL;
				inputSampleL *= constallpass;
				outDL--; if (outDL < 0 || outDL > delayD) {outDL = delayD;}
				inputSampleL += (oDL[outDL]);
				//allpass filter D
				
				dDL[6] = dDL[5];
				dDL[5] = dDL[4];
				dDL[4] = inputSampleL;
				inputSampleL = (dDL[4] + dDL[5] + dDL[6])/3.0;
				
				allpasstemp = outEL - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleL -= oEL[allpasstemp]*constallpass;
				oEL[outEL] = inputSampleL;
				inputSampleL *= constallpass;
				outEL--; if (outEL < 0 || outEL > delayE) {outEL = delayE;}
				inputSampleL += (oEL[outEL]);
				//allpass filter E
				
				dEL[6] = dEL[5];
				dEL[5] = dEL[4];
				dEL[4] = inputSampleL;
				inputSampleL = (dEL[4] + dEL[5] + dEL[6])/3.0;
				
				allpasstemp = outFL - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleL -= oFL[allpasstemp]*constallpass;
				oFL[outFL] = inputSampleL;
				inputSampleL *= constallpass;
				outFL--; if (outFL < 0 || outFL > delayF) {outFL = delayF;}
				inputSampleL += (oFL[outFL]);
				//allpass filter F
				
				dFL[6] = dFL[5];
				dFL[5] = dFL[4];
				dFL[4] = inputSampleL;
				inputSampleL = (dFL[4] + dFL[5] + dFL[6])/3.0;
				
				allpasstemp = outGL - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleL -= oGL[allpasstemp]*constallpass;
				oGL[outGL] = inputSampleL;
				inputSampleL *= constallpass;
				outGL--; if (outGL < 0 || outGL > delayG) {outGL = delayG;}
				inputSampleL += (oGL[outGL]);
				//allpass filter G
				
				dGL[6] = dGL[5];
				dGL[5] = dGL[4];
				dGL[4] = inputSampleL;
				inputSampleL = (dGL[4] + dGL[5] + dGL[6])/3.0;
				
				allpasstemp = outHL - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleL -= oHL[allpasstemp]*constallpass;
				oHL[outHL] = inputSampleL;
				inputSampleL *= constallpass;
				outHL--; if (outHL < 0 || outHL > delayH) {outHL = delayH;}
				inputSampleL += (oHL[outHL]);
				//allpass filter H
				
				dHL[6] = dHL[5];
				dHL[5] = dHL[4];
				dHL[4] = inputSampleL;
				inputSampleL = (dHL[4] + dHL[5] + dHL[6])/3.0;
				
				allpasstemp = outIL - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleL -= oIL[allpasstemp]*constallpass;
				oIL[outIL] = inputSampleL;
				inputSampleL *= constallpass;
				outIL--; if (outIL < 0 || outIL > delayI) {outIL = delayI;}
				inputSampleL += (oIL[outIL]);
				//allpass filter I
				
				dIL[6] = dIL[5];
				dIL[5] = dIL[4];
				dIL[4] = inputSampleL;
				inputSampleL = (dIL[4] + dIL[5] + dIL[6])/3.0;
				
				allpasstemp = outJL - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleL -= oJL[allpasstemp]*constallpass;
				oJL[outJL] = inputSampleL;
				inputSampleL *= constallpass;
				outJL--; if (outJL < 0 || outJL > delayJ) {outJL = delayJ;}
				inputSampleL += (oJL[outJL]);
				//allpass filter J
				
				dJL[6] = dJL[5];
				dJL[5] = dJL[4];
				dJL[4] = inputSampleL;
				inputSampleL = (dJL[4] + dJL[5] + dJL[6])/3.0;
				
				allpasstemp = outKL - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleL -= oKL[allpasstemp]*constallpass;
				oKL[outKL] = inputSampleL;
				inputSampleL *= constallpass;
				outKL--; if (outKL < 0 || outKL > delayK) {outKL = delayK;}
				inputSampleL += (oKL[outKL]);
				//allpass filter K
				
				dKL[6] = dKL[5];
				dKL[5] = dKL[4];
				dKL[4] = inputSampleL;
				inputSampleL = (dKL[4] + dKL[5] + dKL[6])/3.0;
				
				allpasstemp = outLL - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleL -= oLL[allpasstemp]*constallpass;
				oLL[outLL] = inputSampleL;
				inputSampleL *= constallpass;
				outLL--; if (outLL < 0 || outLL > delayL) {outLL = delayL;}
				inputSampleL += (oLL[outLL]);
				//allpass filter L
				
				dLL[6] = dLL[5];
				dLL[5] = dLL[4];
				dLL[4] = inputSampleL;
				inputSampleL = (dLL[4] + dLL[5] + dLL[6])/3.0;
				
				allpasstemp = outML - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleL -= oML[allpasstemp]*constallpass;
				oML[outML] = inputSampleL;
				inputSampleL *= constallpass;
				outML--; if (outML < 0 || outML > delayM) {outML = delayM;}
				inputSampleL += (oML[outML]);
				//allpass filter M
				
				dML[6] = dML[5];
				dML[5] = dML[4];
				dML[4] = inputSampleL;
				inputSampleL = (dML[4] + dML[5] + dML[6])/3.0;
				
				allpasstemp = outNL - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleL -= oNL[allpasstemp]*constallpass;
				oNL[outNL] = inputSampleL;
				inputSampleL *= constallpass;
				outNL--; if (outNL < 0 || outNL > delayN) {outNL = delayN;}
				inputSampleL += (oNL[outNL]);
				//allpass filter N
				
				dNL[6] = dNL[5];
				dNL[5] = dNL[4];
				dNL[4] = inputSampleL;
				inputSampleL = (dNL[4] + dNL[5] + dNL[6])/3.0;
				
				allpasstemp = outOL - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleL -= oOL[allpasstemp]*constallpass;
				oOL[outOL] = inputSampleL;
				inputSampleL *= constallpass;
				outOL--; if (outOL < 0 || outOL > delayO) {outOL = delayO;}
				inputSampleL += (oOL[outOL]);
				//allpass filter O
				
				dOL[6] = dOL[5];
				dOL[5] = dOL[4];
				dOL[4] = inputSampleL;
				inputSampleL = (dOL[4] + dOL[5] + dOL[6])/3.0;
				
				allpasstemp = outPL - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleL -= oPL[allpasstemp]*constallpass;
				oPL[outPL] = inputSampleL;
				inputSampleL *= constallpass;
				outPL--; if (outPL < 0 || outPL > delayP) {outPL = delayP;}
				inputSampleL += (oPL[outPL]);
				//allpass filter P
				
				dPL[6] = dPL[5];
				dPL[5] = dPL[4];
				dPL[4] = inputSampleL;
				inputSampleL = (dPL[4] + dPL[5] + dPL[6])/3.0;
				
				allpasstemp = outQL - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleL -= oQL[allpasstemp]*constallpass;
				oQL[outQL] = inputSampleL;
				inputSampleL *= constallpass;
				outQL--; if (outQL < 0 || outQL > delayQ) {outQL = delayQ;}
				inputSampleL += (oQL[outQL]);
				//allpass filter Q
				
				dQL[6] = dQL[5];
				dQL[5] = dQL[4];
				dQL[4] = inputSampleL;
				inputSampleL = (dQL[4] + dQL[5] + dQL[6])/3.0;
				
				allpasstemp = outRL - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleL -= oRL[allpasstemp]*constallpass;
				oRL[outRL] = inputSampleL;
				inputSampleL *= constallpass;
				outRL--; if (outRL < 0 || outRL > delayR) {outRL = delayR;}
				inputSampleL += (oRL[outRL]);
				//allpass filter R
				
				dRL[6] = dRL[5];
				dRL[5] = dRL[4];
				dRL[4] = inputSampleL;
				inputSampleL = (dRL[4] + dRL[5] + dRL[6])/3.0;
				
				allpasstemp = outSL - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleL -= oSL[allpasstemp]*constallpass;
				oSL[outSL] = inputSampleL;
				inputSampleL *= constallpass;
				outSL--; if (outSL < 0 || outSL > delayS) {outSL = delayS;}
				inputSampleL += (oSL[outSL]);
				//allpass filter S
				
				dSL[6] = dSL[5];
				dSL[5] = dSL[4];
				dSL[4] = inputSampleL;
				inputSampleL = (dSL[4] + dSL[5] + dSL[6])/3.0;
				
				allpasstemp = outTL - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleL -= oTL[allpasstemp]*constallpass;
				oTL[outTL] = inputSampleL;
				inputSampleL *= constallpass;
				outTL--; if (outTL < 0 || outTL > delayT) {outTL = delayT;}
				inputSampleL += (oTL[outTL]);
				//allpass filter T
				
				dTL[6] = dTL[5];
				dTL[5] = dTL[4];
				dTL[4] = inputSampleL;
				inputSampleL = (dTL[4] + dTL[5] + dTL[6])/3.0;
				
				allpasstemp = outUL - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleL -= oUL[allpasstemp]*constallpass;
				oUL[outUL] = inputSampleL;
				inputSampleL *= constallpass;
				outUL--; if (outUL < 0 || outUL > delayU) {outUL = delayU;}
				inputSampleL += (oUL[outUL]);
				//allpass filter U
				
				dUL[6] = dUL[5];
				dUL[5] = dUL[4];
				dUL[4] = inputSampleL;
				inputSampleL = (dUL[4] + dUL[5] + dUL[6])/3.0;
				
				allpasstemp = outVL - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleL -= oVL[allpasstemp]*constallpass;
				oVL[outVL] = inputSampleL;
				inputSampleL *= constallpass;
				outVL--; if (outVL < 0 || outVL > delayV) {outVL = delayV;}
				inputSampleL += (oVL[outVL]);
				//allpass filter V
				
				dVL[6] = dVL[5];
				dVL[5] = dVL[4];
				dVL[4] = inputSampleL;
				inputSampleL = (dVL[4] + dVL[5] + dVL[6])/3.0;
				
				allpasstemp = outWL - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleL -= oWL[allpasstemp]*constallpass;
				oWL[outWL] = inputSampleL;
				inputSampleL *= constallpass;
				outWL--; if (outWL < 0 || outWL > delayW) {outWL = delayW;}
				inputSampleL += (oWL[outWL]);
				//allpass filter W
				
				dWL[6] = dWL[5];
				dWL[5] = dWL[4];
				dWL[4] = inputSampleL;
				inputSampleL = (dWL[4] + dWL[5] + dWL[6])/3.0;
				
				allpasstemp = outXL - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleL -= oXL[allpasstemp]*constallpass;
				oXL[outXL] = inputSampleL;
				inputSampleL *= constallpass;
				outXL--; if (outXL < 0 || outXL > delayX) {outXL = delayX;}
				inputSampleL += (oXL[outXL]);
				//allpass filter X
				
				dXL[6] = dXL[5];
				dXL[5] = dXL[4];
				dXL[4] = inputSampleL;
				inputSampleL = (dXL[4] + dXL[5] + dXL[6])/3.0;
				
				allpasstemp = outYL - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleL -= oYL[allpasstemp]*constallpass;
				oYL[outYL] = inputSampleL;
				inputSampleL *= constallpass;
				outYL--; if (outYL < 0 || outYL > delayY) {outYL = delayY;}
				inputSampleL += (oYL[outYL]);
				//allpass filter Y
				
				dYL[6] = dYL[5];
				dYL[5] = dYL[4];
				dYL[4] = inputSampleL;
				inputSampleL = (dYL[4] + dYL[5] + dYL[6])/3.0;
				
				allpasstemp = outZL - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleL -= oZL[allpasstemp]*constallpass;
				oZL[outZL] = inputSampleL;
				inputSampleL *= constallpass;
				outZL--; if (outZL < 0 || outZL > delayZ) {outZL = delayZ;}
				inputSampleL += (oZL[outZL]);
				//allpass filter Z
				
				dZL[6] = dZL[5];
				dZL[5] = dZL[4];
				dZL[4] = inputSampleL;
				inputSampleL = (dZL[4] + dZL[5] + dZL[6]);		
				//output Zarathustra infinite space verb
				break;
				
		}
		//end big switch for verb type
		
		switch (verbtype)
		{
				
				
			case 1://Chamber
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dAR[3])/3.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dAR[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5] + dAR[6])/3.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dAR[1] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5] + dZR[6]);		
				//output Chamber
				break;
				
				
				
				
				
			case 2:	//Spring
				
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dAR[3])/3.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dYR[1] + dAR[5] + dAR[6])/3.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dXR[1] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dWR[1] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dVR[1] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dUR[1] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dTR[1] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dSR[1] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dRR[1] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dQR[1] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dPR[1] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dOR[1] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dNR[1] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dLR[1] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dKR[1] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dJR[1] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dIR[1] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dHR[1] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dGR[1] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dFR[1] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dER[1] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dDR[1] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dCR[1] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dBR[1] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[5] + dZR[6]);
				//output Spring
				break;
				
				
			case 3:	//Tiled
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2])/2.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2])/2.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[2])/2.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2])/2.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dAR[1] + dER[2])/2.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2])/2.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dGR[2])/2.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2])/2.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2])/2.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2])/2.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[2])/2.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2])/2.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dMR[2])/2.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2])/2.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2])/2.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2])/2.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[2])/2.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2])/2.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dSR[2])/2.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2])/2.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2])/2.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2])/2.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dWR[2])/2.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2])/2.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2])/2.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dCR[5])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dAR[1] + dER[5])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dKR[5])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dOR[5])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dQR[5])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dAR[1] + dUR[5])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5]);		
				//output Tiled
				break;
				
				
			case 4://Room
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = drySampleR;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[1]+dAR[2])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[1]+dBR[2])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[1]+dCR[2])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[1]+dDR[2])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[1]+dER[2])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[1]+dFR[2])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[1]+dGR[2])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[1]+dHR[2])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[1]+dIR[2])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[1]+dJR[2])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[1]+dKR[2])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[1]+dLR[2])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[1]+dMR[2])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[1]+dNR[2])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[1]+dOR[2])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[1]+dPR[2])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[1]+dQR[2])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[1]+dRR[2])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[1]+dSR[2])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[1]+dTR[2])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[1]+dUR[2])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[1]+dVR[2])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[1]+dWR[2])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[1]+dXR[2])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[1]+dYR[2])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dBR[4] * dryness);		
				inputSampleR += (dCR[4] * dryness);		
				inputSampleR += dDR[4];		
				inputSampleR += dER[4];		
				inputSampleR += dFR[4];		
				inputSampleR += dGR[4];		
				inputSampleR += dHR[4];		
				inputSampleR += dIR[4];		
				inputSampleR += dJR[4];		
				inputSampleR += dKR[4];		
				inputSampleR += dLR[4];		
				inputSampleR += dMR[4];		
				inputSampleR += dNR[4];		
				inputSampleR += dOR[4];		
				inputSampleR += dPR[4];		
				inputSampleR += dQR[4];		
				inputSampleR += dRR[4];		
				inputSampleR += dSR[4];		
				inputSampleR += dTR[4];		
				inputSampleR += dUR[4];		
				inputSampleR += dVR[4];		
				inputSampleR += dWR[4];		
				inputSampleR += dXR[4];		
				inputSampleR += dYR[4];		
				inputSampleR += (dZR[4] * wetness);
				
				inputSampleR += (dBR[5] * dryness);		
				inputSampleR += (dCR[5] * dryness);		
				inputSampleR += dDR[5];		
				inputSampleR += dER[5];		
				inputSampleR += dFR[5];		
				inputSampleR += dGR[5];		
				inputSampleR += dHR[5];		
				inputSampleR += dIR[5];		
				inputSampleR += dJR[5];		
				inputSampleR += dKR[5];		
				inputSampleR += dLR[5];		
				inputSampleR += dMR[5];		
				inputSampleR += dNR[5];		
				inputSampleR += dOR[5];		
				inputSampleR += dPR[5];		
				inputSampleR += dQR[5];		
				inputSampleR += dRR[5];		
				inputSampleR += dSR[5];		
				inputSampleR += dTR[5];		
				inputSampleR += dUR[5];		
				inputSampleR += dVR[5];		
				inputSampleR += dWR[5];		
				inputSampleR += dXR[5];		
				inputSampleR += dYR[5];		
				inputSampleR += (dZR[5] * wetness);
				
				inputSampleR /= (26.0 + (wetness * 4.0));
				//output Room effect
				break;
				
				
				
				
				
				
			case 5:	//Stretch	
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2])/2.0;
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2])/2.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2])/2.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2])/2.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2])/2.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2])/2.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2])/2.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2])/2.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2])/2.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2])/2.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2])/2.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2])/2.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2])/2.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2])/2.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2])/2.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2])/2.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2])/2.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2])/2.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2])/2.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2])/2.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2])/2.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2])/2.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2])/2.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2])/2.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2])/2.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2])/2.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dAR[4] + dAR[5])/2.0;
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5])/2.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[4] + dCR[5])/2.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5])/2.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[4] + dER[5])/2.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5])/2.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5])/2.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5])/2.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5])/2.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5])/2.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[4] + dKR[5])/2.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5])/2.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5])/2.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5])/2.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[4] + dOR[5])/2.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5])/2.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[4] + dQR[5])/2.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5])/2.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5])/2.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5])/2.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[4] + dUR[5])/2.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5])/2.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5])/2.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5])/2.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5])/2.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5])/2.0;		
				//output Stretch unrealistic but smooth fake Paulstretch
				break;				
				
				
			case 6:	//Zarathustra	
				allpasstemp = alpAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= aAR[allpasstemp]*constallpass;
				aAR[alpAR] = inputSampleR;
				inputSampleR *= constallpass;
				alpAR--; if (alpAR < 0 || alpAR > delayA) {alpAR = delayA;}
				inputSampleR += (aAR[alpAR]);
				//allpass filter A		
				
				dAR[3] = dAR[2];
				dAR[2] = dAR[1];
				dAR[1] = inputSampleR;
				inputSampleR = (dAR[1] + dAR[2] + dZR[3])/3.0; //add feedback
				
				allpasstemp = alpBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= aBR[allpasstemp]*constallpass;
				aBR[alpBR] = inputSampleR;
				inputSampleR *= constallpass;
				alpBR--; if (alpBR < 0 || alpBR > delayB) {alpBR = delayB;}
				inputSampleR += (aBR[alpBR]);
				//allpass filter B
				
				dBR[3] = dBR[2];
				dBR[2] = dBR[1];
				dBR[1] = inputSampleR;
				inputSampleR = (dBR[1] + dBR[2] + dBR[3])/3.0;
				
				allpasstemp = alpCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= aCR[allpasstemp]*constallpass;
				aCR[alpCR] = inputSampleR;
				inputSampleR *= constallpass;
				alpCR--; if (alpCR < 0 || alpCR > delayC) {alpCR = delayC;}
				inputSampleR += (aCR[alpCR]);
				//allpass filter C
				
				dCR[3] = dCR[2];
				dCR[2] = dCR[1];
				dCR[1] = inputSampleR;
				inputSampleR = (dCR[1] + dCR[2] + dCR[3])/3.0;
				
				allpasstemp = alpDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= aDR[allpasstemp]*constallpass;
				aDR[alpDR] = inputSampleR;
				inputSampleR *= constallpass;
				alpDR--; if (alpDR < 0 || alpDR > delayD) {alpDR = delayD;}
				inputSampleR += (aDR[alpDR]);
				//allpass filter D
				
				dDR[3] = dDR[2];
				dDR[2] = dDR[1];
				dDR[1] = inputSampleR;
				inputSampleR = (dDR[1] + dDR[2] + dDR[3])/3.0;
				
				allpasstemp = alpER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= aER[allpasstemp]*constallpass;
				aER[alpER] = inputSampleR;
				inputSampleR *= constallpass;
				alpER--; if (alpER < 0 || alpER > delayE) {alpER = delayE;}
				inputSampleR += (aER[alpER]);
				//allpass filter E
				
				dER[3] = dER[2];
				dER[2] = dER[1];
				dER[1] = inputSampleR;
				inputSampleR = (dER[1] + dER[2] + dER[3])/3.0;
				
				allpasstemp = alpFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= aFR[allpasstemp]*constallpass;
				aFR[alpFR] = inputSampleR;
				inputSampleR *= constallpass;
				alpFR--; if (alpFR < 0 || alpFR > delayF) {alpFR = delayF;}
				inputSampleR += (aFR[alpFR]);
				//allpass filter F
				
				dFR[3] = dFR[2];
				dFR[2] = dFR[1];
				dFR[1] = inputSampleR;
				inputSampleR = (dFR[1] + dFR[2] + dFR[3])/3.0;
				
				allpasstemp = alpGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= aGR[allpasstemp]*constallpass;
				aGR[alpGR] = inputSampleR;
				inputSampleR *= constallpass;
				alpGR--; if (alpGR < 0 || alpGR > delayG) {alpGR = delayG;}
				inputSampleR += (aGR[alpGR]);
				//allpass filter G
				
				dGR[3] = dGR[2];
				dGR[2] = dGR[1];
				dGR[1] = inputSampleR;
				inputSampleR = (dGR[1] + dGR[2] + dGR[3])/3.0;
				
				allpasstemp = alpHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= aHR[allpasstemp]*constallpass;
				aHR[alpHR] = inputSampleR;
				inputSampleR *= constallpass;
				alpHR--; if (alpHR < 0 || alpHR > delayH) {alpHR = delayH;}
				inputSampleR += (aHR[alpHR]);
				//allpass filter H
				
				dHR[3] = dHR[2];
				dHR[2] = dHR[1];
				dHR[1] = inputSampleR;
				inputSampleR = (dHR[1] + dHR[2] + dHR[3])/3.0;
				
				allpasstemp = alpIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= aIR[allpasstemp]*constallpass;
				aIR[alpIR] = inputSampleR;
				inputSampleR *= constallpass;
				alpIR--; if (alpIR < 0 || alpIR > delayI) {alpIR = delayI;}
				inputSampleR += (aIR[alpIR]);
				//allpass filter I
				
				dIR[3] = dIR[2];
				dIR[2] = dIR[1];
				dIR[1] = inputSampleR;
				inputSampleR = (dIR[1] + dIR[2] + dIR[3])/3.0;
				
				allpasstemp = alpJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= aJR[allpasstemp]*constallpass;
				aJR[alpJR] = inputSampleR;
				inputSampleR *= constallpass;
				alpJR--; if (alpJR < 0 || alpJR > delayJ) {alpJR = delayJ;}
				inputSampleR += (aJR[alpJR]);
				//allpass filter J
				
				dJR[3] = dJR[2];
				dJR[2] = dJR[1];
				dJR[1] = inputSampleR;
				inputSampleR = (dJR[1] + dJR[2] + dJR[3])/3.0;
				
				allpasstemp = alpKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= aKR[allpasstemp]*constallpass;
				aKR[alpKR] = inputSampleR;
				inputSampleR *= constallpass;
				alpKR--; if (alpKR < 0 || alpKR > delayK) {alpKR = delayK;}
				inputSampleR += (aKR[alpKR]);
				//allpass filter K
				
				dKR[3] = dKR[2];
				dKR[2] = dKR[1];
				dKR[1] = inputSampleR;
				inputSampleR = (dKR[1] + dKR[2] + dKR[3])/3.0;
				
				allpasstemp = alpLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= aLR[allpasstemp]*constallpass;
				aLR[alpLR] = inputSampleR;
				inputSampleR *= constallpass;
				alpLR--; if (alpLR < 0 || alpLR > delayL) {alpLR = delayL;}
				inputSampleR += (aLR[alpLR]);
				//allpass filter L
				
				dLR[3] = dLR[2];
				dLR[2] = dLR[1];
				dLR[1] = inputSampleR;
				inputSampleR = (dLR[1] + dLR[2] + dLR[3])/3.0;
				
				allpasstemp = alpMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= aMR[allpasstemp]*constallpass;
				aMR[alpMR] = inputSampleR;
				inputSampleR *= constallpass;
				alpMR--; if (alpMR < 0 || alpMR > delayM) {alpMR = delayM;}
				inputSampleR += (aMR[alpMR]);
				//allpass filter M
				
				dMR[3] = dMR[2];
				dMR[2] = dMR[1];
				dMR[1] = inputSampleR;
				inputSampleR = (dMR[1] + dMR[2] + dMR[3])/3.0;
				
				allpasstemp = alpNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= aNR[allpasstemp]*constallpass;
				aNR[alpNR] = inputSampleR;
				inputSampleR *= constallpass;
				alpNR--; if (alpNR < 0 || alpNR > delayN) {alpNR = delayN;}
				inputSampleR += (aNR[alpNR]);
				//allpass filter N
				
				dNR[3] = dNR[2];
				dNR[2] = dNR[1];
				dNR[1] = inputSampleR;
				inputSampleR = (dNR[1] + dNR[2] + dNR[3])/3.0;
				
				allpasstemp = alpOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= aOR[allpasstemp]*constallpass;
				aOR[alpOR] = inputSampleR;
				inputSampleR *= constallpass;
				alpOR--; if (alpOR < 0 || alpOR > delayO) {alpOR = delayO;}
				inputSampleR += (aOR[alpOR]);
				//allpass filter O
				
				dOR[3] = dOR[2];
				dOR[2] = dOR[1];
				dOR[1] = inputSampleR;
				inputSampleR = (dOR[1] + dOR[2] + dOR[3])/3.0;
				
				allpasstemp = alpPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= aPR[allpasstemp]*constallpass;
				aPR[alpPR] = inputSampleR;
				inputSampleR *= constallpass;
				alpPR--; if (alpPR < 0 || alpPR > delayP) {alpPR = delayP;}
				inputSampleR += (aPR[alpPR]);
				//allpass filter P
				
				dPR[3] = dPR[2];
				dPR[2] = dPR[1];
				dPR[1] = inputSampleR;
				inputSampleR = (dPR[1] + dPR[2] + dPR[3])/3.0;
				
				allpasstemp = alpQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= aQR[allpasstemp]*constallpass;
				aQR[alpQR] = inputSampleR;
				inputSampleR *= constallpass;
				alpQR--; if (alpQR < 0 || alpQR > delayQ) {alpQR = delayQ;}
				inputSampleR += (aQR[alpQR]);
				//allpass filter Q
				
				dQR[3] = dQR[2];
				dQR[2] = dQR[1];
				dQR[1] = inputSampleR;
				inputSampleR = (dQR[1] + dQR[2] + dQR[3])/3.0;
				
				allpasstemp = alpRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= aRR[allpasstemp]*constallpass;
				aRR[alpRR] = inputSampleR;
				inputSampleR *= constallpass;
				alpRR--; if (alpRR < 0 || alpRR > delayR) {alpRR = delayR;}
				inputSampleR += (aRR[alpRR]);
				//allpass filter R
				
				dRR[3] = dRR[2];
				dRR[2] = dRR[1];
				dRR[1] = inputSampleR;
				inputSampleR = (dRR[1] + dRR[2] + dRR[3])/3.0;
				
				allpasstemp = alpSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= aSR[allpasstemp]*constallpass;
				aSR[alpSR] = inputSampleR;
				inputSampleR *= constallpass;
				alpSR--; if (alpSR < 0 || alpSR > delayS) {alpSR = delayS;}
				inputSampleR += (aSR[alpSR]);
				//allpass filter S
				
				dSR[3] = dSR[2];
				dSR[2] = dSR[1];
				dSR[1] = inputSampleR;
				inputSampleR = (dSR[1] + dSR[2] + dSR[3])/3.0;
				
				allpasstemp = alpTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= aTR[allpasstemp]*constallpass;
				aTR[alpTR] = inputSampleR;
				inputSampleR *= constallpass;
				alpTR--; if (alpTR < 0 || alpTR > delayT) {alpTR = delayT;}
				inputSampleR += (aTR[alpTR]);
				//allpass filter T
				
				dTR[3] = dTR[2];
				dTR[2] = dTR[1];
				dTR[1] = inputSampleR;
				inputSampleR = (dTR[1] + dTR[2] + dTR[3])/3.0;
				
				allpasstemp = alpUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= aUR[allpasstemp]*constallpass;
				aUR[alpUR] = inputSampleR;
				inputSampleR *= constallpass;
				alpUR--; if (alpUR < 0 || alpUR > delayU) {alpUR = delayU;}
				inputSampleR += (aUR[alpUR]);
				//allpass filter U
				
				dUR[3] = dUR[2];
				dUR[2] = dUR[1];
				dUR[1] = inputSampleR;
				inputSampleR = (dUR[1] + dUR[2] + dUR[3])/3.0;
				
				allpasstemp = alpVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= aVR[allpasstemp]*constallpass;
				aVR[alpVR] = inputSampleR;
				inputSampleR *= constallpass;
				alpVR--; if (alpVR < 0 || alpVR > delayV) {alpVR = delayV;}
				inputSampleR += (aVR[alpVR]);
				//allpass filter V
				
				dVR[3] = dVR[2];
				dVR[2] = dVR[1];
				dVR[1] = inputSampleR;
				inputSampleR = (dVR[1] + dVR[2] + dVR[3])/3.0;
				
				allpasstemp = alpWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= aWR[allpasstemp]*constallpass;
				aWR[alpWR] = inputSampleR;
				inputSampleR *= constallpass;
				alpWR--; if (alpWR < 0 || alpWR > delayW) {alpWR = delayW;}
				inputSampleR += (aWR[alpWR]);
				//allpass filter W
				
				dWR[3] = dWR[2];
				dWR[2] = dWR[1];
				dWR[1] = inputSampleR;
				inputSampleR = (dWR[1] + dWR[2] + dWR[3])/3.0;
				
				allpasstemp = alpXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= aXR[allpasstemp]*constallpass;
				aXR[alpXR] = inputSampleR;
				inputSampleR *= constallpass;
				alpXR--; if (alpXR < 0 || alpXR > delayX) {alpXR = delayX;}
				inputSampleR += (aXR[alpXR]);
				//allpass filter X
				
				dXR[3] = dXR[2];
				dXR[2] = dXR[1];
				dXR[1] = inputSampleR;
				inputSampleR = (dXR[1] + dXR[2] + dXR[3])/3.0;
				
				allpasstemp = alpYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= aYR[allpasstemp]*constallpass;
				aYR[alpYR] = inputSampleR;
				inputSampleR *= constallpass;
				alpYR--; if (alpYR < 0 || alpYR > delayY) {alpYR = delayY;}
				inputSampleR += (aYR[alpYR]);
				//allpass filter Y
				
				dYR[3] = dYR[2];
				dYR[2] = dYR[1];
				dYR[1] = inputSampleR;
				inputSampleR = (dYR[1] + dYR[2] + dYR[3])/3.0;
				
				allpasstemp = alpZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= aZR[allpasstemp]*constallpass;
				aZR[alpZR] = inputSampleR;
				inputSampleR *= constallpass;
				alpZR--; if (alpZR < 0 || alpZR > delayZ) {alpZR = delayZ;}
				inputSampleR += (aZR[alpZR]);
				//allpass filter Z
				
				dZR[3] = dZR[2];
				dZR[2] = dZR[1];
				dZR[1] = inputSampleR;
				inputSampleR = (dZR[1] + dZR[2] + dZR[3])/3.0;
				
				// now the second stage using the 'out' bank of allpasses		
				
				allpasstemp = outAR - 1;
				if (allpasstemp < 0 || allpasstemp > delayA) {allpasstemp = delayA;}
				inputSampleR -= oAR[allpasstemp]*constallpass;
				oAR[outAR] = inputSampleR;
				inputSampleR *= constallpass;
				outAR--; if (outAR < 0 || outAR > delayA) {outAR = delayA;}
				inputSampleR += (oAR[outAR]);
				//allpass filter A		
				
				dAR[6] = dAR[5];
				dAR[5] = dAR[4];
				dAR[4] = inputSampleR;
				inputSampleR = (dCR[1] + dAR[5] + dAR[6])/3.0;  //note, feeding in dry again for a little more clarity!
				
				allpasstemp = outBR - 1;
				if (allpasstemp < 0 || allpasstemp > delayB) {allpasstemp = delayB;}
				inputSampleR -= oBR[allpasstemp]*constallpass;
				oBR[outBR] = inputSampleR;
				inputSampleR *= constallpass;
				outBR--; if (outBR < 0 || outBR > delayB) {outBR = delayB;}
				inputSampleR += (oBR[outBR]);
				//allpass filter B
				
				dBR[6] = dBR[5];
				dBR[5] = dBR[4];
				dBR[4] = inputSampleR;
				inputSampleR = (dBR[4] + dBR[5] + dBR[6])/3.0;
				
				allpasstemp = outCR - 1;
				if (allpasstemp < 0 || allpasstemp > delayC) {allpasstemp = delayC;}
				inputSampleR -= oCR[allpasstemp]*constallpass;
				oCR[outCR] = inputSampleR;
				inputSampleR *= constallpass;
				outCR--; if (outCR < 0 || outCR > delayC) {outCR = delayC;}
				inputSampleR += (oCR[outCR]);
				//allpass filter C
				
				dCR[6] = dCR[5];
				dCR[5] = dCR[4];
				dCR[4] = inputSampleR;
				inputSampleR = (dCR[4] + dCR[5] + dCR[6])/3.0;
				
				allpasstemp = outDR - 1;
				if (allpasstemp < 0 || allpasstemp > delayD) {allpasstemp = delayD;}
				inputSampleR -= oDR[allpasstemp]*constallpass;
				oDR[outDR] = inputSampleR;
				inputSampleR *= constallpass;
				outDR--; if (outDR < 0 || outDR > delayD) {outDR = delayD;}
				inputSampleR += (oDR[outDR]);
				//allpass filter D
				
				dDR[6] = dDR[5];
				dDR[5] = dDR[4];
				dDR[4] = inputSampleR;
				inputSampleR = (dDR[4] + dDR[5] + dDR[6])/3.0;
				
				allpasstemp = outER - 1;
				if (allpasstemp < 0 || allpasstemp > delayE) {allpasstemp = delayE;}
				inputSampleR -= oER[allpasstemp]*constallpass;
				oER[outER] = inputSampleR;
				inputSampleR *= constallpass;
				outER--; if (outER < 0 || outER > delayE) {outER = delayE;}
				inputSampleR += (oER[outER]);
				//allpass filter E
				
				dER[6] = dER[5];
				dER[5] = dER[4];
				dER[4] = inputSampleR;
				inputSampleR = (dER[4] + dER[5] + dER[6])/3.0;
				
				allpasstemp = outFR - 1;
				if (allpasstemp < 0 || allpasstemp > delayF) {allpasstemp = delayF;}
				inputSampleR -= oFR[allpasstemp]*constallpass;
				oFR[outFR] = inputSampleR;
				inputSampleR *= constallpass;
				outFR--; if (outFR < 0 || outFR > delayF) {outFR = delayF;}
				inputSampleR += (oFR[outFR]);
				//allpass filter F
				
				dFR[6] = dFR[5];
				dFR[5] = dFR[4];
				dFR[4] = inputSampleR;
				inputSampleR = (dFR[4] + dFR[5] + dFR[6])/3.0;
				
				allpasstemp = outGR - 1;
				if (allpasstemp < 0 || allpasstemp > delayG) {allpasstemp = delayG;}
				inputSampleR -= oGR[allpasstemp]*constallpass;
				oGR[outGR] = inputSampleR;
				inputSampleR *= constallpass;
				outGR--; if (outGR < 0 || outGR > delayG) {outGR = delayG;}
				inputSampleR += (oGR[outGR]);
				//allpass filter G
				
				dGR[6] = dGR[5];
				dGR[5] = dGR[4];
				dGR[4] = inputSampleR;
				inputSampleR = (dGR[4] + dGR[5] + dGR[6])/3.0;
				
				allpasstemp = outHR - 1;
				if (allpasstemp < 0 || allpasstemp > delayH) {allpasstemp = delayH;}
				inputSampleR -= oHR[allpasstemp]*constallpass;
				oHR[outHR] = inputSampleR;
				inputSampleR *= constallpass;
				outHR--; if (outHR < 0 || outHR > delayH) {outHR = delayH;}
				inputSampleR += (oHR[outHR]);
				//allpass filter H
				
				dHR[6] = dHR[5];
				dHR[5] = dHR[4];
				dHR[4] = inputSampleR;
				inputSampleR = (dHR[4] + dHR[5] + dHR[6])/3.0;
				
				allpasstemp = outIR - 1;
				if (allpasstemp < 0 || allpasstemp > delayI) {allpasstemp = delayI;}
				inputSampleR -= oIR[allpasstemp]*constallpass;
				oIR[outIR] = inputSampleR;
				inputSampleR *= constallpass;
				outIR--; if (outIR < 0 || outIR > delayI) {outIR = delayI;}
				inputSampleR += (oIR[outIR]);
				//allpass filter I
				
				dIR[6] = dIR[5];
				dIR[5] = dIR[4];
				dIR[4] = inputSampleR;
				inputSampleR = (dIR[4] + dIR[5] + dIR[6])/3.0;
				
				allpasstemp = outJR - 1;
				if (allpasstemp < 0 || allpasstemp > delayJ) {allpasstemp = delayJ;}
				inputSampleR -= oJR[allpasstemp]*constallpass;
				oJR[outJR] = inputSampleR;
				inputSampleR *= constallpass;
				outJR--; if (outJR < 0 || outJR > delayJ) {outJR = delayJ;}
				inputSampleR += (oJR[outJR]);
				//allpass filter J
				
				dJR[6] = dJR[5];
				dJR[5] = dJR[4];
				dJR[4] = inputSampleR;
				inputSampleR = (dJR[4] + dJR[5] + dJR[6])/3.0;
				
				allpasstemp = outKR - 1;
				if (allpasstemp < 0 || allpasstemp > delayK) {allpasstemp = delayK;}
				inputSampleR -= oKR[allpasstemp]*constallpass;
				oKR[outKR] = inputSampleR;
				inputSampleR *= constallpass;
				outKR--; if (outKR < 0 || outKR > delayK) {outKR = delayK;}
				inputSampleR += (oKR[outKR]);
				//allpass filter K
				
				dKR[6] = dKR[5];
				dKR[5] = dKR[4];
				dKR[4] = inputSampleR;
				inputSampleR = (dKR[4] + dKR[5] + dKR[6])/3.0;
				
				allpasstemp = outLR - 1;
				if (allpasstemp < 0 || allpasstemp > delayL) {allpasstemp = delayL;}
				inputSampleR -= oLR[allpasstemp]*constallpass;
				oLR[outLR] = inputSampleR;
				inputSampleR *= constallpass;
				outLR--; if (outLR < 0 || outLR > delayL) {outLR = delayL;}
				inputSampleR += (oLR[outLR]);
				//allpass filter L
				
				dLR[6] = dLR[5];
				dLR[5] = dLR[4];
				dLR[4] = inputSampleR;
				inputSampleR = (dLR[4] + dLR[5] + dLR[6])/3.0;
				
				allpasstemp = outMR - 1;
				if (allpasstemp < 0 || allpasstemp > delayM) {allpasstemp = delayM;}
				inputSampleR -= oMR[allpasstemp]*constallpass;
				oMR[outMR] = inputSampleR;
				inputSampleR *= constallpass;
				outMR--; if (outMR < 0 || outMR > delayM) {outMR = delayM;}
				inputSampleR += (oMR[outMR]);
				//allpass filter M
				
				dMR[6] = dMR[5];
				dMR[5] = dMR[4];
				dMR[4] = inputSampleR;
				inputSampleR = (dMR[4] + dMR[5] + dMR[6])/3.0;
				
				allpasstemp = outNR - 1;
				if (allpasstemp < 0 || allpasstemp > delayN) {allpasstemp = delayN;}
				inputSampleR -= oNR[allpasstemp]*constallpass;
				oNR[outNR] = inputSampleR;
				inputSampleR *= constallpass;
				outNR--; if (outNR < 0 || outNR > delayN) {outNR = delayN;}
				inputSampleR += (oNR[outNR]);
				//allpass filter N
				
				dNR[6] = dNR[5];
				dNR[5] = dNR[4];
				dNR[4] = inputSampleR;
				inputSampleR = (dNR[4] + dNR[5] + dNR[6])/3.0;
				
				allpasstemp = outOR - 1;
				if (allpasstemp < 0 || allpasstemp > delayO) {allpasstemp = delayO;}
				inputSampleR -= oOR[allpasstemp]*constallpass;
				oOR[outOR] = inputSampleR;
				inputSampleR *= constallpass;
				outOR--; if (outOR < 0 || outOR > delayO) {outOR = delayO;}
				inputSampleR += (oOR[outOR]);
				//allpass filter O
				
				dOR[6] = dOR[5];
				dOR[5] = dOR[4];
				dOR[4] = inputSampleR;
				inputSampleR = (dOR[4] + dOR[5] + dOR[6])/3.0;
				
				allpasstemp = outPR - 1;
				if (allpasstemp < 0 || allpasstemp > delayP) {allpasstemp = delayP;}
				inputSampleR -= oPR[allpasstemp]*constallpass;
				oPR[outPR] = inputSampleR;
				inputSampleR *= constallpass;
				outPR--; if (outPR < 0 || outPR > delayP) {outPR = delayP;}
				inputSampleR += (oPR[outPR]);
				//allpass filter P
				
				dPR[6] = dPR[5];
				dPR[5] = dPR[4];
				dPR[4] = inputSampleR;
				inputSampleR = (dPR[4] + dPR[5] + dPR[6])/3.0;
				
				allpasstemp = outQR - 1;
				if (allpasstemp < 0 || allpasstemp > delayQ) {allpasstemp = delayQ;}
				inputSampleR -= oQR[allpasstemp]*constallpass;
				oQR[outQR] = inputSampleR;
				inputSampleR *= constallpass;
				outQR--; if (outQR < 0 || outQR > delayQ) {outQR = delayQ;}
				inputSampleR += (oQR[outQR]);
				//allpass filter Q
				
				dQR[6] = dQR[5];
				dQR[5] = dQR[4];
				dQR[4] = inputSampleR;
				inputSampleR = (dQR[4] + dQR[5] + dQR[6])/3.0;
				
				allpasstemp = outRR - 1;
				if (allpasstemp < 0 || allpasstemp > delayR) {allpasstemp = delayR;}
				inputSampleR -= oRR[allpasstemp]*constallpass;
				oRR[outRR] = inputSampleR;
				inputSampleR *= constallpass;
				outRR--; if (outRR < 0 || outRR > delayR) {outRR = delayR;}
				inputSampleR += (oRR[outRR]);
				//allpass filter R
				
				dRR[6] = dRR[5];
				dRR[5] = dRR[4];
				dRR[4] = inputSampleR;
				inputSampleR = (dRR[4] + dRR[5] + dRR[6])/3.0;
				
				allpasstemp = outSR - 1;
				if (allpasstemp < 0 || allpasstemp > delayS) {allpasstemp = delayS;}
				inputSampleR -= oSR[allpasstemp]*constallpass;
				oSR[outSR] = inputSampleR;
				inputSampleR *= constallpass;
				outSR--; if (outSR < 0 || outSR > delayS) {outSR = delayS;}
				inputSampleR += (oSR[outSR]);
				//allpass filter S
				
				dSR[6] = dSR[5];
				dSR[5] = dSR[4];
				dSR[4] = inputSampleR;
				inputSampleR = (dSR[4] + dSR[5] + dSR[6])/3.0;
				
				allpasstemp = outTR - 1;
				if (allpasstemp < 0 || allpasstemp > delayT) {allpasstemp = delayT;}
				inputSampleR -= oTR[allpasstemp]*constallpass;
				oTR[outTR] = inputSampleR;
				inputSampleR *= constallpass;
				outTR--; if (outTR < 0 || outTR > delayT) {outTR = delayT;}
				inputSampleR += (oTR[outTR]);
				//allpass filter T
				
				dTR[6] = dTR[5];
				dTR[5] = dTR[4];
				dTR[4] = inputSampleR;
				inputSampleR = (dTR[4] + dTR[5] + dTR[6])/3.0;
				
				allpasstemp = outUR - 1;
				if (allpasstemp < 0 || allpasstemp > delayU) {allpasstemp = delayU;}
				inputSampleR -= oUR[allpasstemp]*constallpass;
				oUR[outUR] = inputSampleR;
				inputSampleR *= constallpass;
				outUR--; if (outUR < 0 || outUR > delayU) {outUR = delayU;}
				inputSampleR += (oUR[outUR]);
				//allpass filter U
				
				dUR[6] = dUR[5];
				dUR[5] = dUR[4];
				dUR[4] = inputSampleR;
				inputSampleR = (dUR[4] + dUR[5] + dUR[6])/3.0;
				
				allpasstemp = outVR - 1;
				if (allpasstemp < 0 || allpasstemp > delayV) {allpasstemp = delayV;}
				inputSampleR -= oVR[allpasstemp]*constallpass;
				oVR[outVR] = inputSampleR;
				inputSampleR *= constallpass;
				outVR--; if (outVR < 0 || outVR > delayV) {outVR = delayV;}
				inputSampleR += (oVR[outVR]);
				//allpass filter V
				
				dVR[6] = dVR[5];
				dVR[5] = dVR[4];
				dVR[4] = inputSampleR;
				inputSampleR = (dVR[4] + dVR[5] + dVR[6])/3.0;
				
				allpasstemp = outWR - 1;
				if (allpasstemp < 0 || allpasstemp > delayW) {allpasstemp = delayW;}
				inputSampleR -= oWR[allpasstemp]*constallpass;
				oWR[outWR] = inputSampleR;
				inputSampleR *= constallpass;
				outWR--; if (outWR < 0 || outWR > delayW) {outWR = delayW;}
				inputSampleR += (oWR[outWR]);
				//allpass filter W
				
				dWR[6] = dWR[5];
				dWR[5] = dWR[4];
				dWR[4] = inputSampleR;
				inputSampleR = (dWR[4] + dWR[5] + dWR[6])/3.0;
				
				allpasstemp = outXR - 1;
				if (allpasstemp < 0 || allpasstemp > delayX) {allpasstemp = delayX;}
				inputSampleR -= oXR[allpasstemp]*constallpass;
				oXR[outXR] = inputSampleR;
				inputSampleR *= constallpass;
				outXR--; if (outXR < 0 || outXR > delayX) {outXR = delayX;}
				inputSampleR += (oXR[outXR]);
				//allpass filter X
				
				dXR[6] = dXR[5];
				dXR[5] = dXR[4];
				dXR[4] = inputSampleR;
				inputSampleR = (dXR[4] + dXR[5] + dXR[6])/3.0;
				
				allpasstemp = outYR - 1;
				if (allpasstemp < 0 || allpasstemp > delayY) {allpasstemp = delayY;}
				inputSampleR -= oYR[allpasstemp]*constallpass;
				oYR[outYR] = inputSampleR;
				inputSampleR *= constallpass;
				outYR--; if (outYR < 0 || outYR > delayY) {outYR = delayY;}
				inputSampleR += (oYR[outYR]);
				//allpass filter Y
				
				dYR[6] = dYR[5];
				dYR[5] = dYR[4];
				dYR[4] = inputSampleR;
				inputSampleR = (dYR[4] + dYR[5] + dYR[6])/3.0;
				
				allpasstemp = outZR - 1;
				if (allpasstemp < 0 || allpasstemp > delayZ) {allpasstemp = delayZ;}
				inputSampleR -= oZR[allpasstemp]*constallpass;
				oZR[outZR] = inputSampleR;
				inputSampleR *= constallpass;
				outZR--; if (outZR < 0 || outZR > delayZ) {outZR = delayZ;}
				inputSampleR += (oZR[outZR]);
				//allpass filter Z
				
				dZR[6] = dZR[5];
				dZR[5] = dZR[4];
				dZR[4] = inputSampleR;
				inputSampleR = (dZR[4] + dZR[5] + dZR[6]);		
				//output Zarathustra infinite space verb
				break;
				
		}
		//end big switch for verb type
		
		bridgerectifier = fabs(inputSampleL);
		bridgerectifier = 1.0-cos(bridgerectifier);
		if (inputSampleL > 0) inputSampleL -= bridgerectifier;
		else inputSampleL += bridgerectifier;
		inputSampleL /= gain;		
		bridgerectifier = fabs(inputSampleR);
		bridgerectifier = 1.0-cos(bridgerectifier);
		if (inputSampleR > 0) inputSampleR -= bridgerectifier;
		else inputSampleR += bridgerectifier;
		inputSampleR /= gain;		
		//here we apply the ADT2 'console on steroids' trick
		
		wetness = wetnesstarget;
		//setting up verb wetness to be manipulated by gate and peak
		
		wetness *= peakL;
		//but we only use peak (indirect) to deal with dry/wet, so that it'll manipulate the dry/wet like we want
		
		drySampleL *= (1.0-wetness);
		inputSampleL *= wetness;
		inputSampleL += drySampleL;
		//here we combine the tanks with the dry signal
		
		wetness = wetnesstarget;
		//setting up verb wetness to be manipulated by gate and peak
		
		wetness *= peakR;
		//but we only use peak (indirect) to deal with dry/wet, so that it'll manipulate the dry/wet like we want
		
		drySampleR *= (1.0-wetness);
		inputSampleR *= wetness;
		inputSampleR += drySampleR;
		//here we combine the tanks with the dry signal
		
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


} // end namespace PocketVerbs

