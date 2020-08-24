/* ========================================
 *  BrassRider - BrassRider.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __BrassRider_H
#define __BrassRider_H

#ifndef __audioeffect__
#include "audioeffect_airwinstub.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace BrassRider {

enum {
	kParamA = 0,
	kParamB = 1,
	kNumParameters = 2
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'brsr';    //Change this to what the AU identity is!

class BrassRider : 
    public AudioEffectX 
{
public:
    BrassRider(audioMasterCallback audioMaster);
    ~BrassRider();
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
    
	double d[80002];
	double e[80002];
	double highIIRL;
	double slewIIRL;
	double highIIR2L;
	double slewIIR2L;
	double highIIRR;
	double slewIIRR;
	double highIIR2R;
	double slewIIR2R;
	double control;
	double clamp;
	double lastSampleL;
	double lastSlewL;
	double lastSampleR;
	double lastSlewR;
	int gcount;
	uint32_t fpd;
	//default stuff

    float A;
    float B;
};

} // end namespace BrassRider

#endif
