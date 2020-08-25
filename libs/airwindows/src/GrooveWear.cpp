/* ========================================
 *  GrooveWear - GrooveWear.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __GrooveWear_H
#include "GrooveWear.h"
#endif

namespace GrooveWear {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new GrooveWear(audioMaster);}

GrooveWear::GrooveWear(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.064;
	B = 1.0;

	for(int count = 0; count < 21; count++) {
		aMidL[count] = 0.0;
		bMidL[count] = 0.0;
		cMidL[count] = 0.0;
		dMidL[count] = 0.0;
		aMidR[count] = 0.0;
		bMidR[count] = 0.0;
		cMidR[count] = 0.0;
		dMidR[count] = 0.0;
		fMid[count] = 0.0;
	}
	aMidPrevL = 0.0;
	bMidPrevL = 0.0;
	cMidPrevL = 0.0;
	dMidPrevL = 0.0;

	aMidPrevR = 0.0;
	bMidPrevR = 0.0;
	cMidPrevR = 0.0;
	dMidPrevR = 0.0;

	fpNShapeL = 0.0;
	fpNShapeR = 0.0;
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

GrooveWear::~GrooveWear() {}
VstInt32 GrooveWear::getVendorVersion () {return 1000;}
void GrooveWear::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void GrooveWear::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 GrooveWear::getChunk (void** data, bool isPreset)
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

VstInt32 GrooveWear::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void GrooveWear::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float GrooveWear::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void GrooveWear::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Wear", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void GrooveWear::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void GrooveWear::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 GrooveWear::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool GrooveWear::getEffectName(char* name) {
    vst_strncpy(name, "GrooveWear", kVstMaxProductStrLen); return true;
}

VstPlugCategory GrooveWear::getPlugCategory() {return kPlugCategEffect;}

bool GrooveWear::getProductString(char* text) {
  	vst_strncpy (text, "airwindows GrooveWear", kVstMaxProductStrLen); return true;
}

bool GrooveWear::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace GrooveWear

