/* ========================================
 *  Cojones - Cojones.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Cojones_H
#include "Cojones.h"
#endif

namespace Cojones {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Cojones(audioMaster);}

Cojones::Cojones(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 1.0;
	E = 1.0;
	
	storedL[0] = storedL[1] = 0.0;
	diffL[0] = diffL[1] = diffL[2] = diffL[3] = diffL[4] = diffL[5] = 0.0;

	storedR[0] = storedR[1] = 0.0;
	diffR[0] = diffR[1] = diffR[2] = diffR[3] = diffR[4] = diffR[5] = 0.0;

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

Cojones::~Cojones() {}
VstInt32 Cojones::getVendorVersion () {return 1000;}
void Cojones::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Cojones::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Cojones::getChunk (void** data, bool isPreset)
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

VstInt32 Cojones::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Cojones::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Cojones::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Cojones::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Breathy", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Cojones", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Body", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Cojones::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (((EXTV(A) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (((EXTV(B) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (((EXTV(C) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: dB2string (EXTV(D), text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;			
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool Cojones::parseParameterValueFromString(VstInt32 index, const char* txt, float& f)
{
   auto v = std::atof(txt);

   switch (index)
   {
   case kParamA:
   case kParamB:
   case kParamC:
      f = (v + 100.0) / 200.0;
      break;
   case kParamD:
   {
      f = string2dB(txt, v);
      break;
   }
   default:
   {
      f = v / 100.0;
      break;
   }
   }

   return true;
}

void Cojones::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Cojones::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Cojones::getEffectName(char* name) {
    vst_strncpy(name, "Cojones", kVstMaxProductStrLen); return true;
}

VstPlugCategory Cojones::getPlugCategory() {return kPlugCategEffect;}

bool Cojones::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Cojones", kVstMaxProductStrLen); return true;
}

bool Cojones::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Cojones

