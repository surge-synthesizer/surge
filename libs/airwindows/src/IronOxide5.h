/* ========================================
 *  IronOxide5 - IronOxide5.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __IronOxide5_H
#define __IronOxide5_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace IronOxide5 {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
	kParamD = 3,
	kParamE = 4,
	kParamF = 5,
	kParamG = 6,
	kNumParameters = 7
	
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'feob';    //Change this to what the AU identity is!

class IronOxide5 : 
    public AudioEffectX 
{
public:
    IronOxide5(audioMasterCallback audioMaster);
    ~IronOxide5();
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
    bool isParameterBipolar(VstInt32 index);

 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
	
	double iirSamplehAL;
	double iirSamplehBL;
	double iirSampleAL;
	double iirSampleBL;
	double dL[264];
	double fastIIRAL;
	double fastIIRBL;
	double slowIIRAL;
	double slowIIRBL;
	double fastIIHAL;
	double fastIIHBL;
	double slowIIHAL;
	double slowIIHBL;
	double prevInputSampleL;

	double iirSamplehAR;
	double iirSamplehBR;
	double iirSampleAR;
	double iirSampleBR;
	double dR[264];
	double fastIIRAR;
	double fastIIRBR;
	double slowIIRAR;
	double slowIIRBR;
	double fastIIHAR;
	double fastIIHBR;
	double slowIIHAR;
	double slowIIHBR;
	double prevInputSampleR;
	
	int gcount;
	bool flip;
	
	double flL[100];
	double flR[100];
	
	int fstoredcount;
	double rateof;
	double sweep;
	double nextmax;
	
    
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff

    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
    float G;

};

} // end namespace IronOxide5

#endif
