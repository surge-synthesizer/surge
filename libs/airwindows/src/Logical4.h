/* ========================================
 *  Logical4 - Logical4.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Logical4_H
#define __Logical4_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace Logical4 {

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
const unsigned long kUniqueId = 'logv';    //Change this to what the AU identity is!

class Logical4 : 
    public AudioEffectX 
{
public:
    Logical4(audioMasterCallback audioMaster);
    ~Logical4();
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
    bool isParameterBipolar(VstInt32 index);

 private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
	
	//begin ButterComp
	double controlAposL;
	double controlAnegL;
	double controlBposL;
	double controlBnegL;
	double targetposL;
	double targetnegL;
	double controlAposBL;
	double controlAnegBL;
	double controlBposBL;
	double controlBnegBL;
	double targetposBL;
	double targetnegBL;
	double controlAposCL;
	double controlAnegCL;
	double controlBposCL;
	double controlBnegCL;
	double targetposCL;
	double targetnegCL;
	double avgAL;
	double avgBL;
	double avgCL;
	double avgDL;
	double avgEL;
	double avgFL;
	double nvgAL;
	double nvgBL;
	double nvgCL;
	double nvgDL;
	double nvgEL;
	double nvgFL;
	//end ButterComp
	
	//begin Power Sag
	double dL[1000];
	double controlL;
	double bL[1000];
	double controlBL;
	double cL[1000];
	double controlCL;
	//end Power Sag
	
	//begin ButterComp
	double controlAposR;
	double controlAnegR;
	double controlBposR;
	double controlBnegR;
	double targetposR;
	double targetnegR;
	double controlAposBR;
	double controlAnegBR;
	double controlBposBR;
	double controlBnegBR;
	double targetposBR;
	double targetnegBR;
	double controlAposCR;
	double controlAnegCR;
	double controlBposCR;
	double controlBnegCR;
	double targetposCR;
	double targetnegCR;
	double avgAR;
	double avgBR;
	double avgCR;
	double avgDR;
	double avgER;
	double avgFR;
	double nvgAR;
	double nvgBR;
	double nvgCR;
	double nvgDR;
	double nvgER;
	double nvgFR;
	//end ButterComp
	
	//begin Power Sag
	double dR[1000];
	double controlR;
	double bR[1000];
	double controlBR;
	double cR[1000];
	double controlCR;
	//end Power Sag
	
	int gcount;
	
    
	double fpNShapeL;
	double fpNShapeR;
	bool fpFlip;
	//default stuff

    float A;
    float B;
    float C;
    float D;
	float E;
};

} // end namespace Logical4

#endif
