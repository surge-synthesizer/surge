/* ========================================
 *  BassDrive - BassDrive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BassDrive_H
#include "BassDrive.h"
#endif

namespace BassDrive {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new BassDrive(audioMaster);}

BassDrive::BassDrive(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.5;
	E = 0.5;
	for (int fcount = 0; fcount < 7; fcount++)
	{
		presenceInAL[fcount] = 0.0;
		presenceOutAL[fcount] = 0.0;
		highInAL[fcount] = 0.0;
		highOutAL[fcount] = 0.0;
		midInAL[fcount] = 0.0;
		midOutAL[fcount] = 0.0;
		lowInAL[fcount] = 0.0;
		lowOutAL[fcount] = 0.0;
		presenceInBL[fcount] = 0.0;
		presenceOutBL[fcount] = 0.0;
		highInBL[fcount] = 0.0;
		highOutBL[fcount] = 0.0;
		midInBL[fcount] = 0.0;
		midOutBL[fcount] = 0.0;
		lowInBL[fcount] = 0.0;
		lowOutBL[fcount] = 0.0;
		
		presenceInAR[fcount] = 0.0;
		presenceOutAR[fcount] = 0.0;
		highInAR[fcount] = 0.0;
		highOutAR[fcount] = 0.0;
		midInAR[fcount] = 0.0;
		midOutAR[fcount] = 0.0;
		lowInAR[fcount] = 0.0;
		lowOutAR[fcount] = 0.0;
		presenceInBR[fcount] = 0.0;
		presenceOutBR[fcount] = 0.0;
		highInBR[fcount] = 0.0;
		highOutBR[fcount] = 0.0;
		midInBR[fcount] = 0.0;
		midOutBR[fcount] = 0.0;
		lowInBR[fcount] = 0.0;
		lowOutBR[fcount] = 0.0;
	}
	flip = false;
	
	
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

BassDrive::~BassDrive() {}
VstInt32 BassDrive::getVendorVersion () {return 1000;}
void BassDrive::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void BassDrive::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 BassDrive::getChunk (void** data, bool isPreset)
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

VstInt32 BassDrive::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void BassDrive::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float BassDrive::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void BassDrive::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Presence", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "High", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mid", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Low", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void BassDrive::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void BassDrive::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy (text, "%", kVstMaxParamStrLen);
}

bool BassDrive::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   float v = std::atof(str);

   f = v / 100.0;

   return true;
}

VstInt32 BassDrive::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool BassDrive::getEffectName(char* name) {
    vst_strncpy(name, "BassDrive", kVstMaxProductStrLen); return true;
}

VstPlugCategory BassDrive::getPlugCategory() {return kPlugCategEffect;}

bool BassDrive::getProductString(char* text) {
  	vst_strncpy (text, "airwindows BassDrive", kVstMaxProductStrLen); return true;
}

bool BassDrive::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace BassDrive

