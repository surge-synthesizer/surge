/* ========================================
 *  DustBunny - DustBunny.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __DustBunny_H
#define __DustBunny_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace DustBunny {

enum {
	kParamA = 0,
  kNumParameters = 1
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'dbny';    //Change this to what the AU identity is!

class DustBunny : 
    public AudioEffectX 
{
public:
    DustBunny(audioMasterCallback audioMaster);
    ~DustBunny();
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
    bool parseParameterValueFromString(VstInt32 index, const char* str, float& f);
 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	float LataLast3Sample;
	float LataLast2Sample;
	float LataLast1Sample;
	float LataHalfwaySample;
	float LataHalfDrySample;
	float LataHalfDiffSample;
	float LataA;
	float LataB;
	float LataC;
	float LataDecay;
	float LataUpsampleHighTweak;
	float LataDrySample;
	float LataDiffSample;
	float LataPrevDiffSample;
	
	float RataLast3Sample;
	float RataLast2Sample;
	float RataLast1Sample;
	float RataHalfwaySample;
	float RataHalfDrySample;
	float RataHalfDiffSample;
	float RataA;
	float RataB;
	float RataC;
	float RataDecay;
	float RataUpsampleHighTweak;
	float RataDrySample;
	float RataDiffSample;
	float RataPrevDiffSample;
	
	bool LataFlip; //end defining of antialiasing variables
	bool RataFlip; //end defining of antialiasing variables
	
    float A;
};

} // end namespace DustBunny

#endif
