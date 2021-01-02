/* ========================================
 *  UnBox - UnBox.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __UnBox_H
#define __UnBox_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace UnBox {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
  kNumParameters = 3
};

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'unbx';    //Change this to what the AU identity is!

class UnBox : 
    public AudioEffectX 
{
public:
    UnBox(audioMasterCallback audioMaster);
    ~UnBox();
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
    bool parseParameterValueFromString(VstInt32 index, const char* str, float& f);

 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff
	long double aL[5];
	long double bL[5];
	long double cL[11];
	long double aR[5];
	long double bR[5];
	long double cR[11];
	long double e[5];
	long double f[11];
	long double iirSampleAL;
	long double iirSampleBL;
	long double iirSampleAR;
	long double iirSampleBR;
	

    float A;
    float B;
    float C;
};

} // end namespace UnBox

#endif
