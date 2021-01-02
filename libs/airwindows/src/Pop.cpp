/* ========================================
 *  Pop - Pop.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Pop_H
#include "Pop.h"
#endif

namespace Pop {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Pop(audioMaster);}

Pop::Pop(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.3;
	B = 1.0;
	C = 1.0;
	fpNShapeL = 0.0;
	fpNShapeR = 0.0;
	
	for(int count = 0; count < 10000; count++) {dL[count] = 0; dR[count] = 0;}
	delay = 0;
	flip = false;
	
	muSpeedAL = 10000;
	muSpeedBL = 10000;
	muCoefficientAL = 1;
	muCoefficientBL = 1;
	thickenL = 1;
	muVaryL = 1;
	previousL = 0.0;
	previous2L = 0.0;
	previous3L = 0.0;
	previous4L = 0.0;
	previous5L = 0.0;

	muSpeedAR = 10000;
	muSpeedBR = 10000;
	muCoefficientAR = 1;
	muCoefficientBR = 1;
	thickenR = 1;
	muVaryR = 1;
	previousR = 0.0;
	previous2R = 0.0;
	previous3R = 0.0;
	previous4R = 0.0;
	previous5R = 0.0;
	//this is reset: values being initialized only once. Startup values, whatever they are.
	
    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
    _canDo.insert("x2in2out"); 
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
	programsAreChunks(true);
    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

Pop::~Pop() {}
VstInt32 Pop::getVendorVersion () {return 1000;}
void Pop::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Pop::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Pop::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Pop::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void Pop::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Pop::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Pop::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Amount", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Pop::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: dB2string (EXTV(B), text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Pop::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamB)
    {
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
}

bool Pop::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
    auto v = std::atof(str);

    if (index == kParamB)
    {
        f = string2dB(str, v);
    }
    else
    {
        f = v / 100.0;
    }

	return true;
}

VstInt32 Pop::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Pop::getEffectName(char* name) {
    vst_strncpy(name, "Pop", kVstMaxProductStrLen); return true;
}

VstPlugCategory Pop::getPlugCategory() {return kPlugCategEffect;}

bool Pop::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Pop", kVstMaxProductStrLen); return true;
}

bool Pop::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Pop

