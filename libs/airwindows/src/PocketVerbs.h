/* ========================================
 *  PocketVerbs - PocketVerbs.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __PocketVerbs_H
#define __PocketVerbs_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace PocketVerbs {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
	kParamD = 3,
  kNumParameters = 4
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'pkvb';    //Change this to what the AU identity is!

class PocketVerbs : 
    public AudioEffectX 
{
public:
    PocketVerbs(audioMasterCallback audioMaster);
    ~PocketVerbs();
    virtual bool getEffectName(char* name);                       // The plug-in name
    virtual VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    virtual bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
    virtual bool getVendorString(char* text);                     // Vendor info
    virtual VstInt32 getVendorVersion();                          // Version number
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames);
    virtual void getProgramName(char *name);                      // read the name from the host
    virtual void setProgramName(char *name);                      // changes the name of the preset displayed in the host
	virtual VstInt32 getChunk (void** data, bool isPreset);
	virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);
    virtual float getParameter(VstInt32 index);                   // get the parameter value at the specified index
    virtual void setParameter(VstInt32 index, float value);       // set the parameter at index to value
    virtual void getParameterLabel(VstInt32 index, char *text);  // label for the parameter (eg dB)
    virtual void getParameterName(VstInt32 index, char *text);    // name of the parameter
    virtual void getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal); // text description of the current value    
    virtual VstInt32 canDo(char *text);
    bool isParameterIntegral(VstInt32 index);
    int parameterIntegralUpperBound(VstInt32 index);
    void getIntegralDisplayForValue(VstInt32 index, float value, char* txt);
    bool parseParameterValueFromString(VstInt32 index, const char* str, float& f);
 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	int count;
	
	
	
	double dAR[7];
	double dBR[7];
	double dCR[7];
	double dDR[7];		
	double dER[7];
	double dFR[7];
	double dGR[7];
	double dHR[7];
	double dIR[7];
	double dJR[7];
	double dKR[7];
	double dLR[7];
	double dMR[7];
	double dNR[7];
	double dOR[7];
	double dPR[7];
	double dQR[7];
	double dRR[7];
	double dSR[7];
	double dTR[7];
	double dUR[7];
	double dVR[7];
	double dWR[7];
	double dXR[7];
	double dYR[7];
	double dZR[7];
	
	double aAR[15150];
	double aBR[14618];
	double aCR[14358];
	double aDR[13818];		
	double aER[13562];
	double aFR[13046];
	double aGR[11966];
	double aHR[11130];
	double aIR[10598];
	double aJR[9810];
	double aKR[9522];
	double aLR[8982];
	double aMR[8786];
	double aNR[8462];
	double aOR[8310];
	double aPR[7982];
	double aQR[7322];
	double aRR[6818];
	double aSR[6506];
	double aTR[6002];
	double aUR[5838];
	double aVR[5502];
	double aWR[5010];
	double aXR[4850];
	double aYR[4296];
	double aZR[4180];
	
	double oAR[15150];
	double oBR[14618];
	double oCR[14358];
	double oDR[13818];		
	double oER[13562];
	double oFR[13046];
	double oGR[11966];
	double oHR[11130];
	double oIR[10598];
	double oJR[9810];
	double oKR[9522];
	double oLR[8982];
	double oMR[8786];
	double oNR[8462];
	double oOR[8310];
	double oPR[7982];
	double oQR[7322];
	double oRR[6818];
	double oSR[6506];
	double oTR[6002];
	double oUR[5838];
	double oVR[5502];
	double oWR[5010];
	double oXR[4850];
	double oYR[4296];
	double oZR[4180];
	
	double dAL[7];
	double dBL[7];
	double dCL[7];
	double dDL[7];		
	double dEL[7];
	double dFL[7];
	double dGL[7];
	double dHL[7];
	double dIL[7];
	double dJL[7];
	double dKL[7];
	double dLL[7];
	double dML[7];
	double dNL[7];
	double dOL[7];
	double dPL[7];
	double dQL[7];
	double dRL[7];
	double dSL[7];
	double dTL[7];
	double dUL[7];
	double dVL[7];
	double dWL[7];
	double dXL[7];
	double dYL[7];
	double dZL[7];
	
	double aAL[15150];
	double aBL[14618];
	double aCL[14358];
	double aDL[13818];		
	double aEL[13562];
	double aFL[13046];
	double aGL[11966];
	double aHL[11130];
	double aIL[10598];
	double aJL[9810];
	double aKL[9522];
	double aLL[8982];
	double aML[8786];
	double aNL[8462];
	double aOL[8310];
	double aPL[7982];
	double aQL[7322];
	double aRL[6818];
	double aSL[6506];
	double aTL[6002];
	double aUL[5838];
	double aVL[5502];
	double aWL[5010];
	double aXL[4850];
	double aYL[4296];
	double aZL[4180];
	
	double oAL[15150];
	double oBL[14618];
	double oCL[14358];
	double oDL[13818];		
	double oEL[13562];
	double oFL[13046];
	double oGL[11966];
	double oHL[11130];
	double oIL[10598];
	double oJL[9810];
	double oKL[9522];
	double oLL[8982];
	double oML[8786];
	double oNL[8462];
	double oOL[8310];
	double oPL[7982];
	double oQL[7322];
	double oRL[6818];
	double oSL[6506];
	double oTL[6002];
	double oUL[5838];
	double oVL[5502];
	double oWL[5010];
	double oXL[4850];
	double oYL[4296];
	double oZL[4180];
	
	
	
	int outAL, alpAL;
	int outBL, alpBL;
	int outCL, alpCL;
	int outDL, alpDL;
	int outEL, alpEL;
	int outFL, alpFL;
	int outGL, alpGL;
	int outHL, alpHL;
	int outIL, alpIL;
	int outJL, alpJL;
	int outKL, alpKL;
	int outLL, alpLL;
	int outML, alpML;
	int outNL, alpNL;
	int outOL, alpOL;
	int outPL, alpPL;
	int outQL, alpQL;
	int outRL, alpRL;
	int outSL, alpSL;
	int outTL, alpTL;
	int outUL, alpUL;
	int outVL, alpVL;
	int outWL, alpWL;
	int outXL, alpXL;
	int outYL, alpYL;
	int outZL, alpZL;
	
	int outAR, alpAR, maxdelayA, delayA;
	int outBR, alpBR, maxdelayB, delayB;
	int outCR, alpCR, maxdelayC, delayC;
	int outDR, alpDR, maxdelayD, delayD;
	int outER, alpER, maxdelayE, delayE;
	int outFR, alpFR, maxdelayF, delayF;
	int outGR, alpGR, maxdelayG, delayG;
	int outHR, alpHR, maxdelayH, delayH;
	int outIR, alpIR, maxdelayI, delayI;
	int outJR, alpJR, maxdelayJ, delayJ;
	int outKR, alpKR, maxdelayK, delayK;
	int outLR, alpLR, maxdelayL, delayL;
	int outMR, alpMR, maxdelayM, delayM;
	int outNR, alpNR, maxdelayN, delayN;
	int outOR, alpOR, maxdelayO, delayO;
	int outPR, alpPR, maxdelayP, delayP;
	int outQR, alpQR, maxdelayQ, delayQ;
	int outRR, alpRR, maxdelayR, delayR;
	int outSR, alpSR, maxdelayS, delayS;
	int outTR, alpTR, maxdelayT, delayT;
	int outUR, alpUR, maxdelayU, delayU;
	int outVR, alpVR, maxdelayV, delayV;
	int outWR, alpWR, maxdelayW, delayW;
	int outXR, alpXR, maxdelayX, delayX;
	int outYR, alpYR, maxdelayY, delayY;
	int outZR, alpZR, maxdelayZ, delayZ;
	
	double savedRoomsize;
	int countdown;
	double peakL;
	double peakR;
	
	uint32_t fpd;
	//default stuff

    float A;
    float B;
    float C;
    float D;

};

} // end namespace PocketVerbs

#endif
