/* ========================================
 *  NonlinearSpace - NonlinearSpace.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __NonlinearSpace_H
#define __NonlinearSpace_H

#ifndef __audioeffect__
#include "airwindows/AirWinBaseClass.h"
#endif

#include <set>
#include <string>
#include <math.h>

namespace NonlinearSpace
{

enum
{
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
const unsigned long kUniqueId = 'nspc'; // Change this to what the AU identity is!

class NonlinearSpace : public AudioEffectX
{
  public:
    NonlinearSpace(audioMasterCallback audioMaster);
    ~NonlinearSpace();
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

    double avgInputL;
    double avgOutputL;
    double avg2InputL;
    double avg2OutputL;
    double avgInputR;
    double avgOutputR;
    double avg2InputR;
    double avg2OutputR;
    double a2vgInputL;
    double a2vgOutputL;
    double a2vg2InputL;
    double a2vg2OutputL;
    double a2vgInputR;
    double a2vgOutputR;
    double a2vg2InputR;
    double a2vg2OutputR;

    double verboutL;
    double verboutR;
    double iirCCSampleL;
    double iirCCSampleR;
    double iirSampleL;
    double iirSampleR;

    double dMid[2348];
    double dSide[1334];
    double dLeft[5924];
    double dRight[5926];

    double dpreR[7575];
    double dpreL[7575];

    double dA[7575];
    double dB[7309];
    double dC[7179];
    double dD[6909];
    double dE[6781];
    double dF[6523];
    double dG[5983];
    double dH[5565];
    double dI[5299];
    double dJ[4905];
    double dK[4761];
    double dL[4491];
    double dM[4393];
    double dN[4231];
    double dO[4155];
    double dP[3991];
    double dQ[3661];
    double dR[3409];
    double dS[3253];
    double dT[3001];
    double dU[2919];
    double dV[2751];
    double dW[2505];
    double dX[2425];
    double dY[2148];
    double dZ[2090];

    double interpolA, pitchshiftA; // 7575
    double interpolB, pitchshiftB; // 7309
    double interpolC, pitchshiftC; // 7179
    double interpolD, pitchshiftD; // 6909
    double interpolE, pitchshiftE; // 6781
    double interpolF, pitchshiftF; // 6523
    double interpolG, pitchshiftG; // 5983
    double interpolH, pitchshiftH; // 5565
    double interpolI, pitchshiftI; // 5299
    double interpolJ, pitchshiftJ; // 4905
    double interpolK, pitchshiftK; // 4761
    double interpolL, pitchshiftL; // 4491
    double interpolM, pitchshiftM; // 4393
    double interpolN, pitchshiftN; // 4231
    double interpolO, pitchshiftO; // 4155
    double interpolP, pitchshiftP; // 3991
    double interpolQ, pitchshiftQ; // 3661
    double interpolR, pitchshiftR; // 3409
    double interpolS, pitchshiftS; // 3253
    double interpolT, pitchshiftT; // 3001
    double interpolU, pitchshiftU; // 2919
    double interpolV, pitchshiftV; // 2751
    double interpolW, pitchshiftW; // 2505
    double interpolX, pitchshiftX; // 2425
    double interpolY, pitchshiftY; // 2148
    double interpolZ, pitchshiftZ; // 2090

    int oneMid, delayMid, maxdelayMid;
    int oneSide, delaySide, maxdelaySide;
    int oneLeft, delayLeft, maxdelayLeft;
    int oneRight, delayRight, maxdelayRight;

    int onepre, delaypre, maxdelaypre;

    int oneA, twoA, treA, delayA, maxdelayA;
    int oneB, twoB, treB, delayB, maxdelayB;
    int oneC, twoC, treC, delayC, maxdelayC;
    int oneD, twoD, treD, delayD, maxdelayD;
    int oneE, twoE, treE, delayE, maxdelayE;
    int oneF, twoF, treF, delayF, maxdelayF;
    int oneG, twoG, treG, delayG, maxdelayG;
    int oneH, twoH, treH, delayH, maxdelayH;
    int oneI, twoI, treI, delayI, maxdelayI;
    int oneJ, twoJ, treJ, delayJ, maxdelayJ;
    int oneK, twoK, treK, delayK, maxdelayK;
    int oneL, twoL, treL, delayL, maxdelayL;
    int oneM, twoM, treM, delayM, maxdelayM;
    int oneN, twoN, treN, delayN, maxdelayN;
    int oneO, twoO, treO, delayO, maxdelayO;
    int oneP, twoP, treP, delayP, maxdelayP;
    int oneQ, twoQ, treQ, delayQ, maxdelayQ;
    int oneR, twoR, treR, delayR, maxdelayR;
    int oneS, twoS, treS, delayS, maxdelayS;
    int oneT, twoT, treT, delayT, maxdelayT;
    int oneU, twoU, treU, delayU, maxdelayU;
    int oneV, twoV, treV, delayV, maxdelayV;
    int oneW, twoW, treW, delayW, maxdelayW;
    int oneX, twoX, treX, delayX, maxdelayX;
    int oneY, twoY, treY, delayY, maxdelayY;
    int oneZ, twoZ, treZ, delayZ, maxdelayZ;
    double savedPredelay;
    double savedRoomsize;
    int countdown;

    double lowpassSampleAA;
    double lowpassSampleAB;
    double lowpassSampleBA;
    double lowpassSampleBB;
    double lowpassSampleCA;
    double lowpassSampleCB;
    double lowpassSampleDA;
    double lowpassSampleDB;
    double lowpassSampleE;
    double lowpassSampleF;
    double lowpassSampleG;

    double rowpassSampleAA;
    double rowpassSampleAB;
    double rowpassSampleBA;
    double rowpassSampleBB;
    double rowpassSampleCA;
    double rowpassSampleCB;
    double rowpassSampleDA;
    double rowpassSampleDB;
    double rowpassSampleE;
    double rowpassSampleF;
    double rowpassSampleG;

    bool flip;

    double nonlin;

    uint32_t fpdL;
    uint32_t fpdR;
    // default stuff

    float A;
    float B;
    float C;
    float D;
    float E;
    float F; // parameters. Always 0-1, and we scale/alter them elsewhere.
};

} // end namespace NonlinearSpace

#endif
