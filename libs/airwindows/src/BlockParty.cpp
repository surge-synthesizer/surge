/* ========================================
 *  BlockParty - BlockParty.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BlockParty_H
#include "BlockParty.h"
#endif

namespace BlockParty {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new BlockParty(audioMaster);}

BlockParty::BlockParty(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 1.0;
		fpd = 17;
	
	muSpeedAL = 10000;
	muSpeedBL = 10000;
	muSpeedCL = 10000;
	muSpeedDL = 10000;
	muSpeedEL = 10000;
	muCoefficientAL = 1;
	muCoefficientBL = 1;
	muCoefficientCL = 1;
	muCoefficientDL = 1;
	muCoefficientEL = 1;
	lastCoefficientAL = 1;
	lastCoefficientBL = 1;
	lastCoefficientCL = 1;
	lastCoefficientDL = 1;
	mergedCoefficientsL = 1;
	thresholdL = 1.0;
	thresholdBL = 1.0;
	muVaryL = 1;

	muSpeedAR = 10000;
	muSpeedBR = 10000;
	muSpeedCR = 10000;
	muSpeedDR = 10000;
	muSpeedER = 10000;
	muCoefficientAR = 1;
	muCoefficientBR = 1;
	muCoefficientCR = 1;
	muCoefficientDR = 1;
	muCoefficientER = 1;
	lastCoefficientAR = 1;
	lastCoefficientBR = 1;
	lastCoefficientCR = 1;
	lastCoefficientDR = 1;
	mergedCoefficientsR = 1;
	thresholdR = 1.0;
	thresholdBR = 1.0;
	muVaryR = 1;
	
	count = 1;
	fpFlip = true;
	
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

BlockParty::~BlockParty() {}
VstInt32 BlockParty::getVendorVersion () {return 1000;}
void BlockParty::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void BlockParty::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 BlockParty::getChunk (void** data, bool isPreset)
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

VstInt32 BlockParty::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void BlockParty::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float BlockParty::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void BlockParty::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Pound", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void BlockParty::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void BlockParty::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}
bool BlockParty::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   float v = std::atof(str);

   f = v / 100.0;

   return true;
}


VstInt32 BlockParty::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool BlockParty::getEffectName(char* name) {
    vst_strncpy(name, "BlockParty", kVstMaxProductStrLen); return true;
}

VstPlugCategory BlockParty::getPlugCategory() {return kPlugCategEffect;}

bool BlockParty::getProductString(char* text) {
  	vst_strncpy (text, "airwindows BlockParty", kVstMaxProductStrLen); return true;
}

bool BlockParty::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace BlockParty

