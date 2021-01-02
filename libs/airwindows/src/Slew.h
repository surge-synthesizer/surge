/* ========================================
 *  Slew - Slew.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Slew_H
#define __Slew_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

// TODO: Change kFirstParameter to your first parameter and add any additional parameters.
namespace Slew {

enum {
  kSlewParam = 0,
  kNumParameters = 1
};

// TODO: Add other macros or preprocessor defines here

// TODO: Change to reflect your plugin
const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'slew';    //Change this to what the AU identity is!

class Slew : 
    public AudioEffectX 
{
public:
    Slew(audioMasterCallback audioMaster);
    ~Slew();
    
    // Configuration
    virtual bool getEffectName(char* name);                       // The plug-in name
    virtual VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    
    virtual bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
                                                                  // when the plug-in is registered with them.
    
    virtual bool getVendorString(char* text);                     // Vendor info
    virtual VstInt32 getVendorVersion();                          // Version number
    
    // Processing
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames);

    // Programs and parameters
 	virtual VstInt32 getChunk (void** data, bool isPreset);
	virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);
   
    virtual void getProgramName(char *name);                      // read the name from the host
    virtual void setProgramName(char *name);                      // changes the name of the preset displayed in the host
    
    // parameter values are all 0.0f to 1.0f
    virtual float getParameter(VstInt32 index);                   // get the parameter value at the specified index
    virtual void setParameter(VstInt32 index, float value);       // set the parameter at index to value

    virtual void getParameterLabel(VstInt32 index, char *text);  // label for the parameter (eg dB)
    virtual void getParameterName(VstInt32 index, char *text);    // name of the parameter
    virtual void getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal); // text description of the current value    

    // Capabilities
    virtual VstInt32 canDo(char *text);

    bool parseParameterValueFromString(VstInt32 index, const char* str, float& f);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	float A;
	double lastSampleL;
	double lastSampleR;
};

} // end namespace Slew

#endif
