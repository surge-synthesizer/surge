/* ========================================
 *  BussColors4 - BussColors4.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __BussColors4_H
#define __BussColors4_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace BussColors4 {

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
const unsigned long kUniqueId = 'bsc4';    //Change this to what the AU identity is!

class BussColors4 : 
    public AudioEffectX 
{
public:
    BussColors4(audioMasterCallback audioMaster);
    ~BussColors4();
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
    void getIntegralDisplayForValue(VstInt32 index, float value, char* txt);
    virtual bool isParameterIntegral(VstInt32 index);
   virtual int parameterIntegralUpperBound( VstInt32 index );
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;

	double bL[175]; //full buffer for high sample rates. Scales to 192K
	double bR[175]; //full buffer for high sample rates. Scales to 192K
	double dL[100]; //buffer for calculating sag as it relates to the dynamic impulse synthesis. To 192K.
	double dR[100]; //buffer for calculating sag as it relates to the dynamic impulse synthesis. To 192K.
	int c[35]; //just the number of taps we use, doesn't have to scale
	double g[9]; //console model
	double outg[9]; //console model
	double controlL;
	double controlR;
	double slowdynL;
	double slowdynR;
	int gcount;
	
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff

    float A;
    float B;
    float C;
    float D; //parameters. Always 0-1, and we scale/alter them elsewhere.

};

} // end namespace BussColors4

#endif
