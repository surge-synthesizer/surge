/* ========================================
 *  MackEQ - MackEQ.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __MackEQ_H
#include "MackEQ.h"
#endif

namespace MackEQ {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new MackEQ(audioMaster);}

MackEQ::MackEQ(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.1;
	B = 0.5;
	C = 0.5;
	D = 1.0;
	E = 1.0;
	
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleCL = 0.0;
	iirSampleDL = 0.0;
	iirSampleEL = 0.0;
	iirSampleFL = 0.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	iirSampleCR = 0.0;
	iirSampleDR = 0.0;
	iirSampleER = 0.0;
	iirSampleFR = 0.0;
	for (int x = 0; x < 15; x++) {biquadA[x] = 0.0; biquadB[x] = 0.0; biquadC[x] = 0.0; biquadD[x] = 0.0;}
	
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

MackEQ::~MackEQ() {}
VstInt32 MackEQ::getVendorVersion () {return 1000;}
void MackEQ::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void MackEQ::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 MackEQ::getChunk (void** data, bool isPreset)
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

VstInt32 MackEQ::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void MackEQ::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float MackEQ::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void MackEQ::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Trim", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Hi", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Lo", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Gain", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void MackEQ::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void MackEQ::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}


bool MackEQ::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
}

VstInt32 MackEQ::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool MackEQ::getEffectName(char* name) {
    vst_strncpy(name, "MackEQ", kVstMaxProductStrLen); return true;
}

VstPlugCategory MackEQ::getPlugCategory() {return kPlugCategEffect;}

bool MackEQ::getProductString(char* text) {
  	vst_strncpy (text, "airwindows MackEQ", kVstMaxProductStrLen); return true;
}

bool MackEQ::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace MackEQ

