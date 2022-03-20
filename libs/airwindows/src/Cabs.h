/* ========================================
 *  Cabs - Cabs.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Cabs_H
#define __Cabs_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace Cabs {

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
const unsigned long kUniqueId = 'kabz';    //Change this to what the AU identity is!

class Cabs : 
    public AudioEffectX 
{
public:
    Cabs(audioMasterCallback audioMaster);
    ~Cabs();
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
    virtual bool parseParameterValueFromString(VstInt32 index, const char *str, float &f);
    virtual VstInt32 canDo(char *text);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	double ataLast3SampleL; //begin L
	double ataLast2SampleL;
	double ataLast1SampleL;
	double ataHalfwaySampleL;
	double ataHalfDrySampleL;
	double ataHalfDiffSampleL;
	double ataAL;
	double ataBL;
	double ataCL;
	double ataDrySampleL;
	double ataDiffSampleL;
	double ataPrevDiffSampleL;
	double bL[90];
	double lastSampleL;
	double lastHalfSampleL;
	double lastPostSampleL;
	double lastPostHalfSampleL;
	double postPostSampleL;
	double dL[21];
	double controlL;
	double iirHeadBumpAL;
	double iirHeadBumpBL;
	double iirHalfHeadBumpAL;
	double iirHalfHeadBumpBL;
	double lastRefL[7]; //end L
	
	double ataLast3SampleR; //begin R
	double ataLast2SampleR;
	double ataLast1SampleR;
	double ataHalfwaySampleR;
	double ataHalfDrySampleR;
	double ataHalfDiffSampleR;
	double ataAR;
	double ataBR;
	double ataCR;
	double ataDrySampleR;
	double ataDiffSampleR;
	double ataPrevDiffSampleR;
	double bR[90];
	double lastSampleR;
	double lastHalfSampleR;
	double lastPostSampleR;
	double lastPostHalfSampleR;
	double postPostSampleR;
	double dR[21];
	double controlR;
	double iirHeadBumpAR;
	double iirHeadBumpBR;
	double iirHalfHeadBumpAR;
	double iirHalfHeadBumpBR;
	double lastRefR[7]; //end R
	
	bool flip;
	bool ataFlip; //end defining of antialiasing variables
	int gcount;
	int cycle;
	
	uint32_t fpdL;
	uint32_t fpdR;
	//default stuff

    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
};

} // end namespace Cabs

#endif
