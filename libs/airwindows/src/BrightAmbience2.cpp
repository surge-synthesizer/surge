/* ========================================
 *  BrightAmbience2 - BrightAmbience2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BrightAmbience2_H
#include "BrightAmbience2.h"
#endif

namespace BrightAmbience2 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new BrightAmbience2(audioMaster);}

BrightAmbience2::BrightAmbience2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	for(int count = 0; count < 32767; count++) {pL[count] = 0.0; pR[count] = 0.0;}
	feedbackA = feedbackB = feedbackC = 0.0;
	gcount = 0;
	A = 0.2;
	B = 0.2;
	C = 0.0;
	D = 0.5;
	fpd = 17;
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

BrightAmbience2::~BrightAmbience2() {}
VstInt32 BrightAmbience2::getVendorVersion () {return 1000;}
void BrightAmbience2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void BrightAmbience2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 BrightAmbience2::getChunk (void** data, bool isPreset)
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

VstInt32 BrightAmbience2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void BrightAmbience2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float BrightAmbience2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
		default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void BrightAmbience2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Start", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Length", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Feedback", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void BrightAmbience2::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void BrightAmbience2::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}

bool BrightAmbience2::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   float v = std::atof(str);

   f = v / 100.0;

   return true;
}

VstInt32 BrightAmbience2::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool BrightAmbience2::getEffectName(char* name) {
    vst_strncpy(name, "BrightAmbience2", kVstMaxProductStrLen); return true;
}

VstPlugCategory BrightAmbience2::getPlugCategory() {return kPlugCategEffect;}

bool BrightAmbience2::getProductString(char* text) {
  	vst_strncpy (text, "airwindows BrightAmbience2", kVstMaxProductStrLen); return true;
}

bool BrightAmbience2::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace BrightAmbience2

