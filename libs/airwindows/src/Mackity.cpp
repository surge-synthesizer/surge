/* ========================================
 *  Mackity - Mackity.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Mackity_H
#include "Mackity.h"
#endif

namespace Mackity {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Mackity(audioMaster);}

Mackity::Mackity(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.1;
	B = 1.0;
	
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	for (int x = 0; x < 15; x++) {biquadA[x] = 0.0; biquadB[x] = 0.0;}

	fpdL = 1.0; while (fpdL < 16386) fpdL = rand()*UINT32_MAX;
	fpdR = 1.0; while (fpdR < 16386) fpdR = rand()*UINT32_MAX;
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

Mackity::~Mackity() {}
VstInt32 Mackity::getVendorVersion () {return 1000;}
void Mackity::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Mackity::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Mackity::getChunk (void** data, bool isPreset)
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

VstInt32 Mackity::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void Mackity::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Mackity::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Mackity::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "In Trim", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Out Pad", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Mackity::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Mackity::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Mackity::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Mackity::getEffectName(char* name) {
    vst_strncpy(name, "Mackity", kVstMaxProductStrLen); return true;
}

VstPlugCategory Mackity::getPlugCategory() {return kPlugCategEffect;}

bool Mackity::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Mackity", kVstMaxProductStrLen); return true;
}

bool Mackity::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}

bool Mackity::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
}

} // end namespace Mackity

