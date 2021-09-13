/* ========================================
 *  Tube - Tube.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Tube_H
#include "Tube.h"
#endif

namespace Tube
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// Tube(audioMaster);}

Tube::Tube(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.0;
    previousSampleA = 0.0;
    previousSampleB = 0.0;
    previousSampleC = 0.0;
    previousSampleD = 0.0;
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

Tube::~Tube() {}
VstInt32 Tube::getVendorVersion() { return 1000; }
void Tube::setProgramName(char *name) { vst_strncpy(_programName, name, kVstMaxProgNameLen); }
void Tube::getProgramName(char *name) { vst_strncpy(name, _programName, kVstMaxProgNameLen); }
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

VstInt32 Tube::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Tube::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Tube::setParameter(VstInt32 index, float value)
{
    switch (index)
    {
    case kParamA:
        A = value;
        break;
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float Tube::getParameter(VstInt32 index)
{
    switch (index)
    {
    case kParamA:
        return A;
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void Tube::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "TUBE", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void Tube::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        float2string(EXTV(A) * 100, text, kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void Tube::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

bool Tube::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
}

VstInt32 Tube::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool Tube::getEffectName(char *name)
{
    vst_strncpy(name, "Tube", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory Tube::getPlugCategory() { return kPlugCategEffect; }

bool Tube::getProductString(char *text)
{
    vst_strncpy(text, "airwindows Tube", kVstMaxProductStrLen);
    return true;
}

bool Tube::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace Tube
