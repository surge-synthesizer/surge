/* ========================================
 *  NonlinearSpace - NonlinearSpace.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NonlinearSpace_H
#include "NonlinearSpace.h"
#endif

namespace NonlinearSpace
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// NonlinearSpace(audioMaster);}

NonlinearSpace::NonlinearSpace(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.5; // this is the sample rate so it will become a 'popup' with fixed values
    B = 0.5;
    C = 0.5;
    D = 0.5;
    E = 0.5; // this is nonlin, so it produces -1 to 1: 0.5 will become 0
    F = 0.5;

    int count;
    for (count = 0; count < 2347; count++)
    {
        dMid[count] = 0.0;
    }
    for (count = 0; count < 1333; count++)
    {
        dSide[count] = 0.0;
    }
    for (count = 0; count < 5923; count++)
    {
        dLeft[count] = 0.0;
    }
    for (count = 0; count < 5925; count++)
    {
        dRight[count] = 0.0;
    }

    for (count = 0; count < 7574; count++)
    {
        dpreR[count] = 0.0;
    }
    for (count = 0; count < 7574; count++)
    {
        dpreL[count] = 0.0;
    }

    for (count = 0; count < 7574; count++)
    {
        dA[count] = 0.0;
    }
    for (count = 0; count < 7308; count++)
    {
        dB[count] = 0.0;
    }
    for (count = 0; count < 7178; count++)
    {
        dC[count] = 0.0;
    }
    for (count = 0; count < 6908; count++)
    {
        dD[count] = 0.0;
    }
    for (count = 0; count < 6780; count++)
    {
        dE[count] = 0.0;
    }
    for (count = 0; count < 6522; count++)
    {
        dF[count] = 0.0;
    }
    for (count = 0; count < 5982; count++)
    {
        dG[count] = 0.0;
    }
    for (count = 0; count < 5564; count++)
    {
        dH[count] = 0.0;
    }
    for (count = 0; count < 5298; count++)
    {
        dI[count] = 0.0;
    }
    for (count = 0; count < 4904; count++)
    {
        dJ[count] = 0.0;
    }
    for (count = 0; count < 4760; count++)
    {
        dK[count] = 0.0;
    }
    for (count = 0; count < 4490; count++)
    {
        dL[count] = 0.0;
    }
    for (count = 0; count < 4392; count++)
    {
        dM[count] = 0.0;
    }
    for (count = 0; count < 4230; count++)
    {
        dN[count] = 0.0;
    }
    for (count = 0; count < 4154; count++)
    {
        dO[count] = 0.0;
    }
    for (count = 0; count < 3990; count++)
    {
        dP[count] = 0.0;
    }
    for (count = 0; count < 3660; count++)
    {
        dQ[count] = 0.0;
    }
    for (count = 0; count < 3408; count++)
    {
        dR[count] = 0.0;
    }
    for (count = 0; count < 3252; count++)
    {
        dS[count] = 0.0;
    }
    for (count = 0; count < 3000; count++)
    {
        dT[count] = 0.0;
    }
    for (count = 0; count < 2918; count++)
    {
        dU[count] = 0.0;
    }
    for (count = 0; count < 2750; count++)
    {
        dV[count] = 0.0;
    }
    for (count = 0; count < 2504; count++)
    {
        dW[count] = 0.0;
    }
    for (count = 0; count < 2424; count++)
    {
        dX[count] = 0.0;
    }
    for (count = 0; count < 2147; count++)
    {
        dY[count] = 0.0;
    }
    for (count = 0; count < 2089; count++)
    {
        dZ[count] = 0.0;
    }

    oneMid = 1;
    delayMid = 2346;
    maxdelayMid = 2346;
    oneSide = 1;
    delaySide = 1332;
    maxdelaySide = 1332;
    oneLeft = 1;
    delayLeft = 5922;
    maxdelayLeft = 5922;
    oneRight = 1;
    delayRight = 5924;
    maxdelayRight = 5924;
    onepre = 1;
    delaypre = 7573;
    maxdelaypre = 7573;

    oneA = 1;
    twoA = 2;
    treA = 3;
    delayA = 7573;
    maxdelayA = 7573;
    oneB = 1;
    twoB = 2;
    treB = 3;
    delayB = 7307;
    maxdelayB = 7307;
    oneC = 1;
    twoC = 2;
    treC = 3;
    delayC = 7177;
    maxdelayC = 7177;
    oneD = 1;
    twoD = 2;
    treD = 3;
    delayD = 6907;
    maxdelayD = 6907;
    oneE = 1;
    twoE = 2;
    treE = 3;
    delayE = 6779;
    maxdelayE = 6779;
    oneF = 1;
    twoF = 2;
    treF = 3;
    delayF = 6521;
    maxdelayF = 6521;
    oneG = 1;
    twoG = 2;
    treG = 3;
    delayG = 5981;
    maxdelayG = 5981;
    oneH = 1;
    twoH = 2;
    treH = 3;
    delayH = 5563;
    maxdelayH = 5563;
    oneI = 1;
    twoI = 2;
    treI = 3;
    delayI = 5297;
    maxdelayI = 5297;
    oneJ = 1;
    twoJ = 2;
    treJ = 3;
    delayJ = 4903;
    maxdelayJ = 4903;
    oneK = 1;
    twoK = 2;
    treK = 3;
    delayK = 4759;
    maxdelayK = 4759;
    oneL = 1;
    twoL = 2;
    treL = 3;
    delayL = 4489;
    maxdelayL = 4489;
    oneM = 1;
    twoM = 2;
    treM = 3;
    delayM = 4391;
    maxdelayM = 4391;
    oneN = 1;
    twoN = 2;
    treN = 3;
    delayN = 4229;
    maxdelayN = 4229;
    oneO = 1;
    twoO = 2;
    treO = 3;
    delayO = 4153;
    maxdelayO = 4153;
    oneP = 1;
    twoP = 2;
    treP = 3;
    delayP = 3989;
    maxdelayP = 3989;
    oneQ = 1;
    twoQ = 2;
    treQ = 3;
    delayQ = 3659;
    maxdelayQ = 3659;
    oneR = 1;
    twoR = 2;
    treR = 3;
    delayR = 3407;
    maxdelayR = 3407;
    oneS = 1;
    twoS = 2;
    treS = 3;
    delayS = 3251;
    maxdelayS = 3251;
    oneT = 1;
    twoT = 2;
    treT = 3;
    delayT = 2999;
    maxdelayT = 2999;
    oneU = 1;
    twoU = 2;
    treU = 3;
    delayU = 2917;
    maxdelayU = 2917;
    oneV = 1;
    twoV = 2;
    treV = 3;
    delayV = 2749;
    maxdelayV = 2749;
    oneW = 1;
    twoW = 2;
    treW = 3;
    delayW = 2503;
    maxdelayW = 2503;
    oneX = 1;
    twoX = 2;
    treX = 3;
    delayX = 2423;
    maxdelayX = 2423;
    oneY = 1;
    twoY = 2;
    treY = 3;
    delayY = 2146;
    maxdelayY = 2146;
    oneZ = 1;
    twoZ = 2;
    treZ = 3;
    delayZ = 2088;
    maxdelayZ = 2088;

    avgInputL = 0.0;
    avgInputR = 0.0;
    avgOutputL = 0.0;
    avgOutputR = 0.0;
    avg2InputL = 0.0;
    avg2InputR = 0.0;
    avg2OutputL = 0.0;
    avg2OutputR = 0.0;
    a2vgInputL = 0.0;
    a2vgInputR = 0.0;
    a2vgOutputL = 0.0;
    a2vgOutputR = 0.0;
    a2vg2InputL = 0.0;
    a2vg2InputR = 0.0;
    a2vg2OutputL = 0.0;
    a2vg2OutputR = 0.0;

    lowpassSampleAA = 0.0;
    lowpassSampleAB = 0.0;
    lowpassSampleBA = 0.0;
    lowpassSampleBB = 0.0;
    lowpassSampleCA = 0.0;
    lowpassSampleCB = 0.0;
    lowpassSampleDA = 0.0;
    lowpassSampleDB = 0.0;
    lowpassSampleE = 0.0;
    lowpassSampleF = 0.0;
    lowpassSampleG = 0.0;

    rowpassSampleAA = 0.0;
    rowpassSampleAB = 0.0;
    rowpassSampleBA = 0.0;
    rowpassSampleBB = 0.0;
    rowpassSampleCA = 0.0;
    rowpassSampleCB = 0.0;
    rowpassSampleDA = 0.0;
    rowpassSampleDB = 0.0;
    rowpassSampleE = 0.0;
    rowpassSampleF = 0.0;
    rowpassSampleG = 0.0;

    interpolA = 0.0;
    interpolB = 0.0;
    interpolC = 0.0;
    interpolD = 0.0;
    interpolE = 0.0;
    interpolF = 0.0;
    interpolG = 0.0;
    interpolH = 0.0;
    interpolI = 0.0;
    interpolJ = 0.0;
    interpolK = 0.0;
    interpolL = 0.0;
    interpolM = 0.0;
    interpolN = 0.0;
    interpolO = 0.0;
    interpolP = 0.0;
    interpolQ = 0.0;
    interpolR = 0.0;
    interpolS = 0.0;
    interpolT = 0.0;
    interpolU = 0.0;
    interpolV = 0.0;
    interpolW = 0.0;
    interpolX = 0.0;
    interpolY = 0.0;
    interpolZ = 0.0;

    pitchshiftA = 1.0 / maxdelayA;
    pitchshiftB = 1.0 / maxdelayB;
    pitchshiftC = 1.0 / maxdelayC;
    pitchshiftD = 1.0 / maxdelayD;
    pitchshiftE = 1.0 / maxdelayE;
    pitchshiftF = 1.0 / maxdelayF;
    pitchshiftG = 1.0 / maxdelayG;
    pitchshiftH = 1.0 / maxdelayH;
    pitchshiftI = 1.0 / maxdelayI;
    pitchshiftJ = 1.0 / maxdelayJ;
    pitchshiftK = 1.0 / maxdelayK;
    pitchshiftL = 1.0 / maxdelayL;
    pitchshiftM = 1.0 / maxdelayM;
    pitchshiftN = 1.0 / maxdelayN;
    pitchshiftO = 1.0 / maxdelayO;
    pitchshiftP = 1.0 / maxdelayP;
    pitchshiftQ = 1.0 / maxdelayQ;
    pitchshiftR = 1.0 / maxdelayR;
    pitchshiftS = 1.0 / maxdelayS;
    pitchshiftT = 1.0 / maxdelayT;
    pitchshiftU = 1.0 / maxdelayU;
    pitchshiftV = 1.0 / maxdelayV;
    pitchshiftW = 1.0 / maxdelayW;
    pitchshiftX = 1.0 / maxdelayX;
    pitchshiftY = 1.0 / maxdelayY;
    pitchshiftZ = 1.0 / maxdelayZ;

    nonlin = 0.0;

    verboutL = 0.0;
    verboutR = 0.0;
    iirCCSampleL = 0.0;
    iirCCSampleR = 0.0;
    iirSampleL = 0.0;
    iirSampleR = 0.0;
    savedRoomsize = -1.0; // force update to begin
    countdown = -1;
    flip = true;

    fpdL = 1.0;
    while (fpdL < 16386)
        fpdL = rand() * UINT32_MAX;
    fpdR = 1.0;
    while (fpdR < 16386)
        fpdR = rand() * UINT32_MAX;
    // this is reset: values being initialized only once. Startup values, whatever they are.

    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend");          // plug-in can be used as a send effect.
    _canDo.insert("x2in2out");
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing(); // supports output replacing
    canDoubleReplacing();  // supports double precision processing
    programsAreChunks(true);
    vst_strncpy(_programName, "Default", kVstMaxProgNameLen); // default program name
}

NonlinearSpace::~NonlinearSpace() {}
VstInt32 NonlinearSpace::getVendorVersion() { return 1000; }
void NonlinearSpace::setProgramName(char *name)
{
    vst_strncpy(_programName, name, kVstMaxProgNameLen);
}
void NonlinearSpace::getProgramName(char *name)
{
    vst_strncpy(name, _programName, kVstMaxProgNameLen);
}
// airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather
// than trying to do versioning and preventing people from using older versions. Maybe they like the
// old one!

static float pinParameter(float data)
{
    if (data < 0.0f)
        return 0.0f;
    if (data > 1.0f)
        return 1.0f;
    return data;
}

VstInt32 NonlinearSpace::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    chunkData[4] = E;
    chunkData[5] = F;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 NonlinearSpace::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    E = pinParameter(chunkData[4]);
    F = pinParameter(chunkData[5]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void NonlinearSpace::setParameter(VstInt32 index, float value)
{
    switch (index)
    {
    case kParamA:
        A = value;
        break;
    case kParamB:
        B = value;
        break;
    case kParamC:
        C = value;
        break;
    case kParamD:
        D = value;
        break;
    case kParamE:
        E = value;
        break;
    case kParamF:
        F = value;
        break;
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float NonlinearSpace::getParameter(VstInt32 index)
{
    switch (index)
    {
    case kParamA:
        return A;
        break;
    case kParamB:
        return B;
        break;
    case kParamC:
        return C;
        break;
    case kParamD:
        return D;
        break;
    case kParamE:
        return E;
        break;
    case kParamF:
        return F;
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void NonlinearSpace::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "Sample Rate", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "Liveness", kVstMaxParamStrLen);
        break;
    case kParamC:
        vst_strncpy(text, "Treble", kVstMaxParamStrLen);
        break;
    case kParamD:
        vst_strncpy(text, "Bass", kVstMaxParamStrLen);
        break;
    case kParamE:
        vst_strncpy(text, "Nonlinear", kVstMaxParamStrLen);
        break;
    case kParamF:
        vst_strncpy(text, "Mix", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void NonlinearSpace::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        switch ((VstInt32)(EXTV(A) * 6.999)) // 0 to almost edge of # of params
        {
        case 0:
            vst_strncpy(text, "16k", kVstMaxParamStrLen);
            break;
        case 1:
            vst_strncpy(text, "32k", kVstMaxParamStrLen);
            break;
        case 2:
            vst_strncpy(text, "44.1k", kVstMaxParamStrLen);
            break;
        case 3:
            vst_strncpy(text, "48k", kVstMaxParamStrLen);
            break;
        case 4:
            vst_strncpy(text, "64k", kVstMaxParamStrLen);
            break;
        case 5:
            vst_strncpy(text, "88.2k", kVstMaxParamStrLen);
            break;
        case 6:
            vst_strncpy(text, "96k", kVstMaxParamStrLen);
            break;
        default:
            break; // unknown parameter, shouldn't happen!
        }
        break; // E as example 'popup' parameter with four values  */
    case kParamB:
        float2string(EXTV(B) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamC:
        float2string(EXTV(C) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamD:
        float2string(EXTV(D) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamE:
        float2string(((EXTV(E) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamF:
        float2string(EXTV(F) * 100.0, text, kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void NonlinearSpace::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "", kVstMaxParamStrLen);
        break;
    case kParamB:
    case kParamC:
    case kParamD:
    case kParamE:
    case kParamF:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 NonlinearSpace::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool NonlinearSpace::getEffectName(char *name)
{
    vst_strncpy(name, "NonlinearSpace", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory NonlinearSpace::getPlugCategory() { return kPlugCategEffect; }

bool NonlinearSpace::getProductString(char *text)
{
    vst_strncpy(text, "airwindows NonlinearSpace", kVstMaxProductStrLen);
    return true;
}

bool NonlinearSpace::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace NonlinearSpace
