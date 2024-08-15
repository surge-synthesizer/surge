/* ========================================
 *  FireAmp - FireAmp.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Gain_H
#include "FireAmp.h"
#endif

namespace FireAmp
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// FireAmp(audioMaster);}

FireAmp::FireAmp(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.5;
    B = 0.5;
    C = 0.5;
    D = 1.0;

    lastSampleL = 0.0;
    storeSampleL = 0.0;
    smoothAL = 0.0;
    smoothBL = 0.0;
    smoothCL = 0.0;
    smoothDL = 0.0;
    smoothEL = 0.0;
    smoothFL = 0.0;
    smoothGL = 0.0;
    smoothHL = 0.0;
    smoothIL = 0.0;
    smoothJL = 0.0;
    smoothKL = 0.0;
    smoothLL = 0.0;
    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    iirSampleEL = 0.0;
    iirSampleFL = 0.0;
    iirSampleGL = 0.0;
    iirSampleHL = 0.0;
    iirSampleIL = 0.0;
    iirSampleJL = 0.0;
    iirSampleKL = 0.0;
    iirSampleLL = 0.0;
    iirLowpassL = 0.0;
    iirSpkAL = 0.0;
    iirSpkBL = 0.0;
    iirSubL = 0.0;

    lastSampleR = 0.0;
    storeSampleR = 0.0;
    smoothAR = 0.0;
    smoothBR = 0.0;
    smoothCR = 0.0;
    smoothDR = 0.0;
    smoothER = 0.0;
    smoothFR = 0.0;
    smoothGR = 0.0;
    smoothHR = 0.0;
    smoothIR = 0.0;
    smoothJR = 0.0;
    smoothKR = 0.0;
    smoothLR = 0.0;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    iirSampleER = 0.0;
    iirSampleFR = 0.0;
    iirSampleGR = 0.0;
    iirSampleHR = 0.0;
    iirSampleIR = 0.0;
    iirSampleJR = 0.0;
    iirSampleKR = 0.0;
    iirSampleLR = 0.0;
    iirLowpassR = 0.0;
    iirSpkAR = 0.0;
    iirSpkBR = 0.0;
    iirSubR = 0.0;

    for (int fcount = 0; fcount < 257; fcount++)
    {
        OddL[fcount] = 0.0;
        EvenL[fcount] = 0.0;
        OddR[fcount] = 0.0;
        EvenR[fcount] = 0.0;
    }

    count = 0;
    flip = false; // amp

    for (int fcount = 0; fcount < 90; fcount++)
    {
        bL[fcount] = 0;
        bR[fcount] = 0;
    }
    smoothCabAL = 0.0;
    smoothCabBL = 0.0;
    lastCabSampleL = 0.0; // cab
    smoothCabAR = 0.0;
    smoothCabBR = 0.0;
    lastCabSampleR = 0.0; // cab

    for (int fcount = 0; fcount < 9; fcount++)
    {
        lastRefL[fcount] = 0.0;
        lastRefR[fcount] = 0.0;
    }
    cycle = 0; // undersampling

    for (int x = 0; x < fix_total; x++)
    {
        fixA[x] = 0.0;
        fixB[x] = 0.0;
        fixC[x] = 0.0;
        fixD[x] = 0.0;
        fixE[x] = 0.0;
        fixF[x] = 0.0;
    } // filtering

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

FireAmp::~FireAmp() {}
VstInt32 FireAmp::getVendorVersion() { return 1000; }
void FireAmp::setProgramName(char *name) { vst_strncpy(_programName, name, kVstMaxProgNameLen); }
void FireAmp::getProgramName(char *name) { vst_strncpy(name, _programName, kVstMaxProgNameLen); }
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

VstInt32 FireAmp::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 FireAmp::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void FireAmp::setParameter(VstInt32 index, float value)
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
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float FireAmp::getParameter(VstInt32 index)
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
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void FireAmp::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "Gain", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "Tone", kVstMaxParamStrLen);
        break;
    case kParamC:
        vst_strncpy(text, "Output", kVstMaxParamStrLen);
        break;
    case kParamD:
        vst_strncpy(text, "Mix", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void FireAmp::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        float2string(EXTV(A) * 100, text, kVstMaxParamStrLen);
        break;
    case kParamB:
        float2string(EXTV(B) * 100, text, kVstMaxParamStrLen);
        break;
    case kParamC:
        float2string(EXTV(C) * 100, text, kVstMaxParamStrLen);
        break;
    case kParamD:
        float2string(EXTV(D) * 100, text, kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void FireAmp::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
    case kParamB:
    case kParamC:
    case kParamD:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 FireAmp::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool FireAmp::getEffectName(char *name)
{
    vst_strncpy(name, "FireAmp", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory FireAmp::getPlugCategory() { return kPlugCategEffect; }

bool FireAmp::getProductString(char *text)
{
    vst_strncpy(text, "airwindows FireAmp", kVstMaxProductStrLen);
    return true;
}

bool FireAmp::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace FireAmp
