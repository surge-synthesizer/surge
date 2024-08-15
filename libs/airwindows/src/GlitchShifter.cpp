/* ========================================
 *  GlitchShifter - GlitchShifter.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __GlitchShifter_H
#include "GlitchShifter.h"
#endif

namespace GlitchShifter
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// GlitchShifter(audioMaster);}

GlitchShifter::GlitchShifter(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    for (int count = 0; count < 131074; count++)
    {
        pL[count] = 0;
        pR[count] = 0;
    }
    for (int count = 0; count < 257; count++)
    {
        offsetL[count] = 0;
        pastzeroL[count] = 0;
        previousL[count] = 0;
        thirdL[count] = 0;
        fourthL[count] = 0;
        offsetR[count] = 0;
        pastzeroR[count] = 0;
        previousR[count] = 0;
        thirdR[count] = 0;
        fourthR[count] = 0;
    }
    crossesL = 0;
    realzeroesL = 0;
    tempL = 0;
    lasttempL = 0;
    thirdtempL = 0;
    fourthtempL = 0;
    sincezerocrossL = 0;
    airPrevL = 0.0;
    airEvenL = 0.0;
    airOddL = 0.0;
    airFactorL = 0.0;
    positionL = 0.0;
    splicingL = false;

    crossesR = 0;
    realzeroesR = 0;
    tempR = 0;
    lasttempR = 0;
    thirdtempR = 0;
    fourthtempR = 0;
    sincezerocrossR = 0;
    airPrevR = 0.0;
    airEvenR = 0.0;
    airOddR = 0.0;
    airFactorR = 0.0;
    positionR = 0.0;
    splicingR = false;

    gcount = 0;
    lastwidth = 16386;
    flip = false;
    A = 0.5;
    B = 0.5;
    C = 0.5;
    D = 0.0;
    E = 0.5;
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

GlitchShifter::~GlitchShifter() {}
VstInt32 GlitchShifter::getVendorVersion() { return 1000; }
void GlitchShifter::setProgramName(char *name)
{
    vst_strncpy(_programName, name, kVstMaxProgNameLen);
}
void GlitchShifter::getProgramName(char *name)
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

VstInt32 GlitchShifter::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    chunkData[4] = E;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 GlitchShifter::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    E = pinParameter(chunkData[4]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void GlitchShifter::setParameter(VstInt32 index, float value)
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
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float GlitchShifter::getParameter(VstInt32 index)
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
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void GlitchShifter::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "Pitch", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "Trim", kVstMaxParamStrLen);
        break;
    case kParamC:
        vst_strncpy(text, "Tighten", kVstMaxParamStrLen);
        break;
    case kParamD:
        vst_strncpy(text, "Feedback", kVstMaxParamStrLen);
        break;
    case kParamE:
        vst_strncpy(text, "Mix", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void GlitchShifter::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        int2string(((int)(EXTV(A) * 24.999) - 12), text, kVstMaxParamStrLen);
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
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void GlitchShifter::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "semitones", kVstMaxParamStrLen);
        break;
    case kParamB:
    case kParamC:
    case kParamD:
    case kParamE:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 GlitchShifter::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool GlitchShifter::getEffectName(char *name)
{
    vst_strncpy(name, "GlitchShifter", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory GlitchShifter::getPlugCategory() { return kPlugCategEffect; }

bool GlitchShifter::getProductString(char *text)
{
    vst_strncpy(text, "airwindows GlitchShifter", kVstMaxProductStrLen);
    return true;
}

bool GlitchShifter::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace GlitchShifter
