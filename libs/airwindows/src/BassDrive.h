/* ========================================
 *  BassDrive - BassDrive.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __BassDrive_H
#define __BassDrive_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace BassDrive {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
	kParamD = 3,
	kParamE = 4,
  kNumParameters = 5
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'bsdr';    //Change this to what the AU identity is!

class BassDrive : 
    public AudioEffectX 
{
public:
    BassDrive(audioMasterCallback audioMaster);
    ~BassDrive();
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
	double presenceInAL[7];
	double presenceOutAL[7];
	double highInAL[7];
	double highOutAL[7];
	double midInAL[7];
	double midOutAL[7];
	double lowInAL[7];
	double lowOutAL[7];
	double presenceInBL[7];
	double presenceOutBL[7];
	double highInBL[7];
	double highOutBL[7];
	double midInBL[7];
	double midOutBL[7];
	double lowInBL[7];
	double lowOutBL[7];
	
	double presenceInAR[7];
	double presenceOutAR[7];
	double highInAR[7];
	double highOutAR[7];
	double midInAR[7];
	double midOutAR[7];
	double lowInAR[7];
	double lowOutAR[7];
	double presenceInBR[7];
	double presenceOutBR[7];
	double highInBR[7];
	double highOutBR[7];
	double midInBR[7];
	double midOutBR[7];
	double lowInBR[7];
	double lowOutBR[7];
	
	bool flip;
	
    float A;
    float B;
    float C;
    float D;
    float E; //parameters. Always 0-1, and we scale/alter them elsewhere.

};

} // end namespace BassDrive

#endif
