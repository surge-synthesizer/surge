/* ========================================
 *  Verbity - Verbity.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Verbity_H
#include "Verbity.h"
#endif

namespace Verbity {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Verbity(audioMaster);}

Verbity::Verbity(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.25;
	B = 0.0;
	C = 0.25;
	D = 0.25;
	
	iirAL = 0.0; iirAR = 0.0;
	iirBL = 0.0; iirBR = 0.0;
	
	for(int count = 0; count < 6479; count++) {aIL[count] = 0.0;aIR[count] = 0.0;}
	for(int count = 0; count < 3659; count++) {aJL[count] = 0.0;aJR[count] = 0.0;}
	for(int count = 0; count < 1719; count++) {aKL[count] = 0.0;aKR[count] = 0.0;}
	for(int count = 0; count < 679; count++) {aLL[count] = 0.0;aLR[count] = 0.0;}
	
	for(int count = 0; count < 9699; count++) {aAL[count] = 0.0;aAR[count] = 0.0;}
	for(int count = 0; count < 5999; count++) {aBL[count] = 0.0;aBR[count] = 0.0;}
	for(int count = 0; count < 2319; count++) {aCL[count] = 0.0;aCR[count] = 0.0;}
	for(int count = 0; count < 939; count++) {aDL[count] = 0.0;aDR[count] = 0.0;}
	
	for(int count = 0; count < 15219; count++) {aEL[count] = 0.0;aER[count] = 0.0;}
	for(int count = 0; count < 8459; count++) {aFL[count] = 0.0;aFR[count] = 0.0;}
	for(int count = 0; count < 4539; count++) {aGL[count] = 0.0;aGR[count] = 0.0;}
	for(int count = 0; count < 3199; count++) {aHL[count] = 0.0;aHR[count] = 0.0;}
	
	feedbackAL = 0.0; feedbackAR = 0.0;
	feedbackBL = 0.0; feedbackBR = 0.0;
	feedbackCL = 0.0; feedbackCR = 0.0;
	feedbackDL = 0.0; feedbackDR = 0.0;
	previousAL = 0.0; previousAR = 0.0;
	previousBL = 0.0; previousBR = 0.0;
	previousCL = 0.0; previousCR = 0.0;
	previousDL = 0.0; previousDR = 0.0;
	
	for(int count = 0; count < 6; count++) {lastRefL[count] = 0.0;lastRefR[count] = 0.0;}
	
	thunderL = 0; thunderR = 0;
	
	countI = 1;
	countJ = 1;
	countK = 1;
	countL = 1;
	
	countA = 1;
	countB = 1;
	countC = 1;
	countD = 1;	

	countE = 1;
	countF = 1;
	countG = 1;
	countH = 1;
	cycle = 0;
	
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

Verbity::~Verbity() {}
VstInt32 Verbity::getVendorVersion () {return 1000;}
void Verbity::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Verbity::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Verbity::getChunk (void** data, bool isPreset)
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

VstInt32 Verbity::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Verbity::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Verbity::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Verbity::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Size", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Decay", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Brightness", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Verbity::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Verbity::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}

VstInt32 Verbity::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Verbity::getEffectName(char* name) {
    vst_strncpy(name, "Verbity", kVstMaxProductStrLen); return true;
}

VstPlugCategory Verbity::getPlugCategory() {return kPlugCategEffect;}

bool Verbity::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Verbity", kVstMaxProductStrLen); return true;
}

bool Verbity::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}

bool Verbity::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
}


} // end namespace Verbity

