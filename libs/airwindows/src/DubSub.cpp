/* ========================================
 *  DubSub - DubSub.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DubSub_H
#include "DubSub.h"
#endif

namespace DubSub
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// DubSub(audioMaster);}

DubSub::DubSub(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.75;
    B = 0.5;
    C = 0.5;
    D = 0.5;
    E = 0.5;
    F = 0.5;
    G = 0.25;
    H = 0.25;
    I = 0.75;
    J = 0.5;

    WasNegativeL = false;
    SubOctaveL = false;
    WasNegativeR = false;
    SubOctaveR = false;
    flip = false;
    bflip = 0;

    iirDriveSampleAL = 0.0;
    iirDriveSampleBL = 0.0;
    iirDriveSampleCL = 0.0;
    iirDriveSampleDL = 0.0;
    iirDriveSampleEL = 0.0;
    iirDriveSampleFL = 0.0;
    iirDriveSampleAR = 0.0;
    iirDriveSampleBR = 0.0;
    iirDriveSampleCR = 0.0;
    iirDriveSampleDR = 0.0;
    iirDriveSampleER = 0.0;
    iirDriveSampleFR = 0.0;

    iirHeadBumpAL = 0.0;
    iirHeadBumpBL = 0.0;
    iirHeadBumpCL = 0.0;
    iirHeadBumpAR = 0.0;
    iirHeadBumpBR = 0.0;
    iirHeadBumpCR = 0.0;

    iirSubBumpAL = 0.0;
    iirSubBumpBL = 0.0;
    iirSubBumpCL = 0.0;
    iirSubBumpAR = 0.0;
    iirSubBumpBR = 0.0;
    iirSubBumpCR = 0.0;

    lastHeadBumpL = 0.0;
    lastSubBumpL = 0.0;
    lastHeadBumpR = 0.0;
    lastSubBumpR = 0.0;

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
    iirSampleML = 0.0;
    iirSampleNL = 0.0;
    iirSampleOL = 0.0;
    iirSamplePL = 0.0;
    iirSampleQL = 0.0;
    iirSampleRL = 0.0;
    iirSampleSL = 0.0;
    iirSampleTL = 0.0;
    iirSampleUL = 0.0;
    iirSampleVL = 0.0;
    iirSampleWL = 0.0;
    iirSampleXL = 0.0;
    iirSampleYL = 0.0;
    iirSampleZL = 0.0;
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
    iirSampleMR = 0.0;
    iirSampleNR = 0.0;
    iirSampleOR = 0.0;
    iirSamplePR = 0.0;
    iirSampleQR = 0.0;
    iirSampleRR = 0.0;
    iirSampleSR = 0.0;
    iirSampleTR = 0.0;
    iirSampleUR = 0.0;
    iirSampleVR = 0.0;
    iirSampleWR = 0.0;
    iirSampleXR = 0.0;
    iirSampleYR = 0.0;
    iirSampleZR = 0.0;

    oscGateL = 1.0;
    oscGateR = 1.0;

    fpNShapeL = 0.0;
    fpNShapeR = 0.0;
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

DubSub::~DubSub() {}
VstInt32 DubSub::getVendorVersion() { return 1000; }
void DubSub::setProgramName(char *name) { vst_strncpy(_programName, name, kVstMaxProgNameLen); }
void DubSub::getProgramName(char *name) { vst_strncpy(name, _programName, kVstMaxProgNameLen); }
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

VstInt32 DubSub::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    chunkData[4] = E;
    chunkData[5] = F;
    chunkData[6] = G;
    chunkData[7] = H;
    chunkData[8] = I;
    chunkData[9] = J;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 DubSub::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    E = pinParameter(chunkData[4]);
    F = pinParameter(chunkData[5]);
    G = pinParameter(chunkData[6]);
    H = pinParameter(chunkData[7]);
    I = pinParameter(chunkData[8]);
    J = pinParameter(chunkData[9]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void DubSub::setParameter(VstInt32 index, float value)
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
    case kParamG:
        G = value;
        break;
    case kParamH:
        H = value;
        break;
    case kParamI:
        I = value;
        break;
    case kParamJ:
        J = value;
        break;
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float DubSub::getParameter(VstInt32 index)
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
    case kParamG:
        return G;
        break;
    case kParamH:
        return H;
        break;
    case kParamI:
        return I;
        break;
    case kParamJ:
        return J;
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void DubSub::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "Treble Grind", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "Grind Output", kVstMaxParamStrLen);
        break;
    case kParamC:
        vst_strncpy(text, "Crossover", kVstMaxParamStrLen);
        break;
    case kParamD:
        vst_strncpy(text, "Bass Drive", kVstMaxParamStrLen);
        break;
    case kParamE:
        vst_strncpy(text, "Bass Voicing", kVstMaxParamStrLen);
        break;
    case kParamF:
        vst_strncpy(text, "Bass Output", kVstMaxParamStrLen);
        break;
    case kParamG:
        vst_strncpy(text, "Sub Drive", kVstMaxParamStrLen);
        break;
    case kParamH:
        vst_strncpy(text, "Sub Voicing", kVstMaxParamStrLen);
        break;
    case kParamI:
        vst_strncpy(text, "Sub Output", kVstMaxParamStrLen);
        break;
    case kParamJ:
        vst_strncpy(text, "Mix", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void DubSub::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        float2string(EXTV(A) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamB:
        float2string(((EXTV(B) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamC:
        float2string(EXTV(C) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamD:
        float2string(EXTV(D) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamE:
        float2string(EXTV(E) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamF:
        float2string(((EXTV(F) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamG:
        float2string(EXTV(G) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamH:
        float2string(EXTV(H) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamI:
        float2string(((EXTV(I) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen);
        break;
    case kParamJ:
        float2string(EXTV(J) * 100.0, text, kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void DubSub::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
    case kParamB:
    case kParamC:
    case kParamD:
    case kParamE:
    case kParamF:
    case kParamG:
    case kParamH:
    case kParamI:
    case kParamJ:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 DubSub::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool DubSub::getEffectName(char *name)
{
    vst_strncpy(name, "DubSub", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory DubSub::getPlugCategory() { return kPlugCategEffect; }

bool DubSub::getProductString(char *text)
{
    vst_strncpy(text, "airwindows DubSub", kVstMaxProductStrLen);
    return true;
}

bool DubSub::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace DubSub
