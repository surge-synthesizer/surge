/* ========================================
 *  Pop - Pop.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Pop_H
#define __Pop_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace Pop {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
  kNumParameters = 3
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'popc';    //Change this to what the AU identity is!

class Pop : 
    public AudioEffectX 
{
public:
    Pop(audioMasterCallback audioMaster);
    ~Pop();
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
    virtual void getParameterDisplay(VstInt32 index, char *text); // text description of the current value    
    virtual VstInt32 canDo(char *text);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff
	
	double muVaryL;
	double muAttackL;
	double muNewSpeedL;
	double muSpeedAL;
	double muSpeedBL;
	double muCoefficientAL;
	double muCoefficientBL;
	long double thickenL;
	long double previousL;
	long double previous2L;
	long double previous3L;
	long double previous4L;
	long double previous5L;
	double dL[10001];
	
	double muVaryR;
	double muAttackR;
	double muNewSpeedR;
	double muSpeedAR;
	double muSpeedBR;
	double muCoefficientAR;
	double muCoefficientBR;
	long double thickenR;
	long double previousR;
	long double previous2R;
	long double previous3R;
	long double previous4R;
	long double previous5R;
	double dR[10001];
	
	int delay;
	bool flip;	

    float A;
    float B;
    float C;
};

} // end namespace Pop

#endif
