/* ========================================
 *  TripleSpread - TripleSpread.h
 *  Created 8/12/11 by SPIAdmin 
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __TripleSpread_H
#define __TripleSpread_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace TripleSpread {

enum {
	kParamA = 0,
	kParamB = 1,
	kParamC = 2,
  kNumParameters = 3
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'trsp';    //Change this to what the AU identity is!

class TripleSpread : 
    public AudioEffectX 
{
public:
    TripleSpread(audioMasterCallback audioMaster);
    ~TripleSpread();
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
    virtual void getParameterDisplay(VstInt32 index, char *text, float, bool); // text description of the current value
    virtual VstInt32 canDo(char *text);
    virtual bool isParameterBipolar(VstInt32 index);
    virtual bool parseParameterValueFromString(VstInt32 index, const char *str, float &f);

  private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;
    
	VstInt32 pL[131076];
	VstInt32 offsetL[258];
	VstInt32 pastzeroL[258];
	VstInt32 previousL[258];
	VstInt32 thirdL[258];
	VstInt32 fourthL[258];
	VstInt32 tempL;
	VstInt32 lasttempL;
	VstInt32 thirdtempL;
	VstInt32 fourthtempL;
	VstInt32 sincezerocrossL;
	int crossesL;
	int realzeroesL;
	double positionL;
	bool splicingL;
	
	double airPrevL;
	double airEvenL;
	double airOddL;
	double airFactorL;
	
	VstInt32 pR[131076];
	VstInt32 offsetR[258];
	VstInt32 pastzeroR[258];
	VstInt32 previousR[258];
	VstInt32 thirdR[258];
	VstInt32 fourthR[258];
	VstInt32 tempR;
	VstInt32 lasttempR;
	VstInt32 thirdtempR;
	VstInt32 fourthtempR;
	VstInt32 sincezerocrossR;
	int crossesR;
	int realzeroesR;
	double positionR;
	bool splicingR;
	
	double airPrevR;
	double airEvenR;
	double airOddR;
	double airFactorR;
	
	int gcount;
	VstInt32 lastwidth;
	bool flip;
	uint32_t fpd;
	//default stuff

    float A;
    float B;
    float C;

};

} // end namespace TripleSpread

#endif
