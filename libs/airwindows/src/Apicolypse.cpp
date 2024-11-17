/* ========================================
 *  Apicolypse - Apicolypse.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Apicolypse_H
#include "Apicolypse.h"
#endif

namespace Apicolypse {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Apicolypse(audioMaster);}

Apicolypse::Apicolypse(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.70;
	B = 0.3333333;
	C = 0.3333333;
	D = 1.0;
	for(int count = 0; count < 34; count++) {bR[count] = 0;bL[count] = 0;}
	lastSampleR = 0.0;lastSampleL = 0.0;
	
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

Apicolypse::~Apicolypse() {}
VstInt32 Apicolypse::getVendorVersion () {return 1000;}
void Apicolypse::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Apicolypse::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Apicolypse::getChunk (void** data, bool isPreset)
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

VstInt32 Apicolypse::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Apicolypse::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Apicolypse::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Apicolypse::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Hardness", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Personality", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Apicolypse::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: dB2string (EXTV(D), text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Apicolypse::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamD)
    {
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
}

VstInt32 Apicolypse::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Apicolypse::getEffectName(char* name) {
    vst_strncpy(name, "Apicolypse", kVstMaxProductStrLen); return true;
}

VstPlugCategory Apicolypse::getPlugCategory() {return kPlugCategEffect;}

bool Apicolypse::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Apicolypse", kVstMaxProductStrLen); return true;
}

bool Apicolypse::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
bool Apicolypse::parseParameterValueFromString(VstInt32 index, const char* txt, float& f)
{
   float v = std::atof(txt);

   if (index == kParamD)
   {
      f = string2dB(txt, v);
   }
   else
   {
      f = v / 100.0;
   }

   return true;
}

} // end namespace Apicolypse

