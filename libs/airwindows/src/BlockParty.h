/* ========================================
 *  BlockParty - BlockParty.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __BlockParty_H
#define __BlockParty_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace BlockParty {

enum {
	kParamA = 0,
	kParamB = 1,
  kNumParameters = 2
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'blok';    //Change this to what the AU identity is!

class BlockParty : 
    public AudioEffectX 
{
public:
    BlockParty(audioMasterCallback audioMaster);
    ~BlockParty();
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

	// Surge extensions
	bool parseParameterValueFromString(VstInt32 index, const char* str, float& f);
 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
		uint32_t fpd;
	//default stuff

    float A;
    float B;
	
	double muVaryL;
	double muAttackL;
	double muNewSpeedL;
	double muSpeedAL;
	double muSpeedBL;
	double muSpeedCL;
	double muSpeedDL;
	double muSpeedEL;
	double muCoefficientAL;
	double muCoefficientBL;
	double muCoefficientCL;
	double muCoefficientDL;
	double muCoefficientEL;
	double lastCoefficientAL;
	double lastCoefficientBL;
	double lastCoefficientCL;
	double lastCoefficientDL;
	double mergedCoefficientsL;
	double thresholdL;
	double thresholdBL;

	double muVaryR;
	double muAttackR;
	double muNewSpeedR;
	double muSpeedAR;
	double muSpeedBR;
	double muSpeedCR;
	double muSpeedDR;
	double muSpeedER;
	double muCoefficientAR;
	double muCoefficientBR;
	double muCoefficientCR;
	double muCoefficientDR;
	double muCoefficientER;
	double lastCoefficientAR;
	double lastCoefficientBR;
	double lastCoefficientCR;
	double lastCoefficientDR;
	double mergedCoefficientsR;
	double thresholdR;
	double thresholdBR;

	int count;
	bool fpFlip;
};

} // end namespace BlockParty

#endif
