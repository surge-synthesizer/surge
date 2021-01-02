/* ========================================
 *  Deckwrecka - Deckwrecka.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Deckwrecka_H
#define __Deckwrecka_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace DeckWrecka {

enum {
	kParamA = 0,
  kNumParameters = 1
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'dkwr';    //Change this to what the AU identity is!

class Deckwrecka : 
    public AudioEffectX 
{
public:
    Deckwrecka(audioMasterCallback audioMaster);
    ~Deckwrecka();
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
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	uint32_t fpd;
	//default stuff
	
	int bflip;
	
	double iirHeadBumpAL;
	double iirHeadBumpBL;
	double iirHeadBumpCL;
	
	double iirSampleAL;
	double iirSampleBL;
	double iirSampleCL;
	double iirSampleDL;
	double iirSampleEL;
	double iirSampleFL;
	double iirSampleGL;
	double iirSampleHL;
	double iirSampleIL;
	double iirSampleJL;
	double iirSampleKL;
	double iirSampleLL;
	double iirSampleML;
	double iirSampleNL;
	double iirSampleOL;
	double iirSamplePL;
	double iirSampleQL;
	double iirSampleRL;
	double iirSampleSL;
	double iirSampleTL;
	double iirSampleUL;
	double iirSampleVL;
	double iirSampleWL;
	double iirSampleXL;
	double iirSampleYL;
	double iirSampleZL;		
	
	double iirHeadBumpAR;
	double iirHeadBumpBR;
	double iirHeadBumpCR;
	
	double iirSampleAR;
	double iirSampleBR;
	double iirSampleCR;
	double iirSampleDR;
	double iirSampleER;
	double iirSampleFR;
	double iirSampleGR;
	double iirSampleHR;
	double iirSampleIR;
	double iirSampleJR;
	double iirSampleKR;
	double iirSampleLR;
	double iirSampleMR;
	double iirSampleNR;
	double iirSampleOR;
	double iirSamplePR;
	double iirSampleQR;
	double iirSampleRR;
	double iirSampleSR;
	double iirSampleTR;
	double iirSampleUR;
	double iirSampleVR;
	double iirSampleWR;
	double iirSampleXR;
	double iirSampleYR;
	double iirSampleZR;		

    float A;
};

} // end namespace DeckWrecka

#endif
