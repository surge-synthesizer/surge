/* ========================================
 *  ADClip7 - ADClip7.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __ADClip7_H
#define __ADClip7_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace ADClip7 {

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
const unsigned long kUniqueId = 'adcr';    //Change this to what the AU identity is!

class ADClip7 : 
    public AudioEffectX 
{
public:
    ADClip7(audioMasterCallback audioMaster);
    ~ADClip7();
    virtual bool getEffectName(char* name);                       // The plug-in name
    virtual VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    virtual bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
    virtual bool getVendorString(char* text);                     // Vendor info
    virtual VstInt32 getVendorVersion();                          // Version number
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
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

    // Surge Extensions
    virtual bool parseParameterValueFromString( VstInt32 index, const char* txt, float &f );
    virtual bool isParameterIntegral( VstInt32 index );
    virtual int parameterIntegralUpperBound( VstInt32 index);
    virtual void getIntegralDisplayForValue( VstInt32 index, float value, char *txt );


private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff
	long double lastSampleL;
	long double lastSampleR;
	float bL[22200];
	float bR[22200];
	int gcount;
	double lowsL;
	double lowsR;
	double iirLowsAL;
	double iirLowsAR;
	double iirLowsBL;
	double iirLowsBR;
	long double refclipL;
	long double refclipR;

    float A;
    float B;
    float C;
    float D;

};

} // end namespace ADClip7

#endif
