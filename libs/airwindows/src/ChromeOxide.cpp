/* ========================================
 *  ChromeOxide - ChromeOxide.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ChromeOxide_H
#include "ChromeOxide.h"
#endif

namespace ChromeOxide
{

// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new
// ChromeOxide(audioMaster);}

ChromeOxide::ChromeOxide(audioMasterCallback audioMaster)
    : AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    secondSampleL = 0.0;
    thirdSampleL = 0.0;
    fourthSampleL = 0.0;
    fifthSampleL = 0.0;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    secondSampleR = 0.0;
    thirdSampleR = 0.0;
    fourthSampleR = 0.0;
    fifthSampleR = 0.0;
    flip = false;
    A = 0.5;
    B = 0.5;
    fpd = 17;
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

ChromeOxide::~ChromeOxide() {}
VstInt32 ChromeOxide::getVendorVersion() { return 1000; }
void ChromeOxide::setProgramName(char *name)
{
    vst_strncpy(_programName, name, kVstMaxProgNameLen);
}
void ChromeOxide::getProgramName(char *name)
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

VstInt32 ChromeOxide::getChunk(void **data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 ChromeOxide::setChunk(void *data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void ChromeOxide::setParameter(VstInt32 index, float value)
{
    switch (index)
    {
    case kParamA:
        A = value;
        break;
    case kParamB:
        B = value;
        break;
    default:
        throw; // unknown parameter, shouldn't happen!
    }
}

float ChromeOxide::getParameter(VstInt32 index)
{
    switch (index)
    {
    case kParamA:
        return A;
        break;
    case kParamB:
        return B;
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
    return 0.0; // we only need to update the relevant name, this is simple to manage
}

void ChromeOxide::getParameterName(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "Intensity", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "Bias", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this is our labels for displaying in the VST host
}

void ChromeOxide::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    switch (index)
    {
    case kParamA:
        float2string(EXTV(A) * 100, text, kVstMaxParamStrLen);
        break;
    case kParamB:
        float2string(EXTV(B) * 100, text, kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }          // this displays the values and handles 'popups' where it's discrete choices
}

void ChromeOxide::getParameterLabel(VstInt32 index, char *text)
{
    switch (index)
    {
    case kParamA:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    case kParamB:
        vst_strncpy(text, "%", kVstMaxParamStrLen);
        break;
    default:
        break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 ChromeOxide::canDo(char *text)
{
    return (_canDo.find(text) == _canDo.end()) ? -1 : 1;
} // 1 = yes, -1 = no, 0 = don't know

bool ChromeOxide::getEffectName(char *name)
{
    vst_strncpy(name, "ChromeOxide", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory ChromeOxide::getPlugCategory() { return kPlugCategEffect; }

bool ChromeOxide::getProductString(char *text)
{
    vst_strncpy(text, "airwindows ChromeOxide", kVstMaxProductStrLen);
    return true;
}

bool ChromeOxide::getVendorString(char *text)
{
    vst_strncpy(text, "airwindows", kVstMaxVendorStrLen);
    return true;
}

} // end namespace ChromeOxide
