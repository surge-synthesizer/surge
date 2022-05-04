/* ========================================
 *  ToVinyl4 - ToVinyl4.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __ToVinyl4_H
#define __ToVinyl4_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace ToVinyl4
{

enum
{
    kParamA = 0,
    kParamB = 1,
    kParamC = 2,
    kParamD = 3,
    kNumParameters = 4
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'tovb'; // Change this to what the AU identity is!

class ToVinyl4 : public AudioEffectX
{
  public:
    ToVinyl4(audioMasterCallback audioMaster);
    ~ToVinyl4();
    virtual bool getEffectName(char *name);    // The plug-in name
    virtual VstPlugCategory getPlugCategory(); // The general category for the plug-in
    virtual bool
    getProductString(char *text); // This is a unique plug-in string provided by Steinberg
    virtual bool getVendorString(char *text); // Vendor info
    virtual VstInt32 getVendorVersion();      // Version number
    virtual void processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames);
    virtual void processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames);
    virtual void getProgramName(char *name); // read the name from the host
    virtual void setProgramName(char *name); // changes the name of the preset displayed in the host
    virtual VstInt32 getChunk(void **data, bool isPreset);
    virtual VstInt32 setChunk(void *data, VstInt32 byteSize, bool isPreset);
    virtual float getParameter(VstInt32 index); // get the parameter value at the specified index
    virtual void setParameter(VstInt32 index, float value); // set the parameter at index to value
    virtual void getParameterLabel(VstInt32 index, char *text); // label for the parameter (eg dB)
    virtual void getParameterName(VstInt32 index, char *text);  // name of the parameter
    virtual void getParameterDisplay(VstInt32 index, char *text, float,
                                     bool); // text description of the current value
    virtual VstInt32 canDo(char *text);

  private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set<std::string> _canDo;

    double ataLastOutL;
    double ataLastOutR;

    double s1L;
    double s2L;
    double s3L;
    double o1L;
    double o2L;
    double o3L;
    double m1L;
    double m2L;
    double s1R;
    double s2R;
    double s3R;
    double o1R;
    double o2R;
    double o3R;
    double m1R;
    double m2R;
    double desL;
    double desR;

    double midSampleA;
    double midSampleB;
    double midSampleC;
    double midSampleD;
    double midSampleE;
    double midSampleF;
    double midSampleG;
    double midSampleH;
    double midSampleI;
    double midSampleJ;
    double midSampleK;
    double midSampleL;
    double midSampleM;
    double midSampleN;
    double midSampleO;
    double midSampleP;
    double midSampleQ;
    double midSampleR;
    double midSampleS;
    double midSampleT;
    double midSampleU;
    double midSampleV;
    double midSampleW;
    double midSampleX;
    double midSampleY;
    double midSampleZ;

    double sideSampleA;
    double sideSampleB;
    double sideSampleC;
    double sideSampleD;
    double sideSampleE;
    double sideSampleF;
    double sideSampleG;
    double sideSampleH;
    double sideSampleI;
    double sideSampleJ;
    double sideSampleK;
    double sideSampleL;
    double sideSampleM;
    double sideSampleN;
    double sideSampleO;
    double sideSampleP;
    double sideSampleQ;
    double sideSampleR;
    double sideSampleS;
    double sideSampleT;
    double sideSampleU;
    double sideSampleV;
    double sideSampleW;
    double sideSampleX;
    double sideSampleY;
    double sideSampleZ;

    double aMid[11];
    double bMid[11];
    double fMid[11];
    double aSide[11];
    double bSide[11];
    double fSide[11];
    double aMidPrev;
    double aSidePrev;
    double bMidPrev;
    double bSidePrev;

    uint32_t fpdL;
    uint32_t fpdR;
    // default stuff

    float A;
    float B;
    float C;
    float D;
};

} // end namespace ToVinyl4

#endif
