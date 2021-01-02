/* ========================================
 *  Air - Air.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Air_H
#define __Air_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace Air {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
	kParamD = 3,
	kParamE = 4,
	kParamF = 5,
  kNumParameters = 6
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'aiyr';    //Change this to what the AU identity is!

class Air : 
    public AudioEffectX 
{
public:
    Air(audioMasterCallback audioMaster);
    ~Air();
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

    // Surge Extensions
    virtual bool parseParameterValueFromString(VstInt32 index, const char* txt, float& f);

 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
	
	double airPrevAL;
	double airEvenAL;
	double airOddAL;
	double airFactorAL;
	double airPrevBL;
	double airEvenBL;
	double airOddBL;
	double airFactorBL;
	double airPrevCL;
	double airEvenCL;
	double airOddCL;
	double airFactorCL;
	double tripletPrevL;
	double tripletMidL;
	double tripletAL;
	double tripletBL;
	double tripletCL;
	double tripletFactorL;

	double airPrevAR;
	double airEvenAR;
	double airOddAR;
	double airFactorAR;
	double airPrevBR;
	double airEvenBR;
	double airOddBR;
	double airFactorBR;
	double airPrevCR;
	double airEvenCR;
	double airOddCR;
	double airFactorCR;
	double tripletPrevR;
	double tripletMidR;
	double tripletAR;
	double tripletBR;
	double tripletCR;
	double tripletFactorR;
	
	bool flipA;
	bool flipB;
	bool flop;
	int count;
    
	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff

    float A;
    float B;
    float C;
    float D; //parameters. Always 0-1, and we scale/alter them elsewhere.
	float E;
	float F;

};

} // end namespace Air

#endif
