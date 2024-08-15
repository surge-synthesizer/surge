/* ========================================
 *  ToTape6 - ToTape6.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ToTape6_H
#include "ToTape6.h"
#endif

namespace ToTape6 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new ToTape6(audioMaster);}

ToTape6::ToTape6(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.5;
	E = 0.5;
	F = 1.0;
	iirMidRollerAL = 0.0;
	iirMidRollerBL = 0.0;
	iirHeadBumpAL = 0.0;
	iirHeadBumpBL = 0.0;
	
	iirMidRollerAR = 0.0;
	iirMidRollerBR = 0.0;
	iirHeadBumpAR = 0.0;
	iirHeadBumpBR = 0.0;
		
	for (int x = 0; x < 9; x++) {
		biquadAL[x] = 0.0;biquadBL[x] = 0.0;biquadCL[x] = 0.0;biquadDL[x] = 0.0;
		biquadAR[x] = 0.0;biquadBR[x] = 0.0;biquadCR[x] = 0.0;biquadDR[x] = 0.0;
	}
	flip = false;
	for (int temp = 0; temp < 501; temp++) {dL[temp] = 0.0;dR[temp] = 0.0;}
	
	gcount = 0;	
	rateof = 0.5;
	sweep = M_PI;
	nextmax = 0.5;
	lastSampleL = 0.0;
	lastSampleR = 0.0;
	flip = 0;
	
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

ToTape6::~ToTape6() {}
VstInt32 ToTape6::getVendorVersion () {return 1000;}
void ToTape6::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void ToTape6::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 ToTape6::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	chunkData[4] = E;
	chunkData[5] = F;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 ToTape6::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	E = pinParameter(chunkData[4]);
	F = pinParameter(chunkData[5]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void ToTape6::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float ToTape6::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        case kParamF: return F; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void ToTape6::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input Gain", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Soften", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Head Bump", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Flutter", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Output Gain", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void ToTape6::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string ((EXTV(A) - 0.5) * 24.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string ((EXTV(E) - 0.5) * 24.0, text, kVstMaxParamStrLen); break;
        case kParamF: float2string (EXTV(F) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool ToTape6::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   switch (index)
   {
   case kParamA:
   case kParamE:
      f = (v / 24.0) + 0.5;
      break;
   default:
      f = v / 100.0;
      break;
   }
   return true;
}

bool ToTape6::isParameterBipolar(VstInt32 index)
{
   return ( index == kParamA || index == kParamE );
}

void ToTape6::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamA || index == kParamE)
    {
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
}

VstInt32 ToTape6::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool ToTape6::getEffectName(char* name) {
    vst_strncpy(name, "ToTape6", kVstMaxProductStrLen); return true;
}

VstPlugCategory ToTape6::getPlugCategory() {return kPlugCategEffect;}

bool ToTape6::getProductString(char* text) {
  	vst_strncpy (text, "airwindows ToTape6", kVstMaxProductStrLen); return true;
}

bool ToTape6::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace ToTape6

