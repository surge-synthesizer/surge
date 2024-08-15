/* ========================================
 *  Spiral2 - Spiral2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Spiral2_H
#include "Spiral2.h"
#endif

namespace Spiral2 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Spiral2(audioMaster);}

Spiral2::Spiral2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.0;
	C = 0.5;
	D = 1.0;
	E = 1.0;
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	prevSampleL = 0.0;
	fpNShapeL = 0.0;

	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	prevSampleR = 0.0;
	fpNShapeR = 0.0;
	flip = true;
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

Spiral2::~Spiral2() {}
VstInt32 Spiral2::getVendorVersion () {return 1000;}
void Spiral2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Spiral2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Spiral2::getChunk (void** data, bool isPreset)
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

VstInt32 Spiral2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Spiral2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Spiral2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Spiral2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Highpass", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Presence", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Spiral2::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
    case kParamA:
     {
         // let's not show 6.02 dB but clamp it at 6.00 dB just for display
         float v = EXTV(A) * 2.0;

         if (v > 1.996)
         {
             v = 1.996;
         }

         dB2string(v, text, kVstMaxParamStrLen);
         break;
     }
     case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
     case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
     case kParamD: dB2string (EXTV(D), text, kVstMaxParamStrLen); break;
     case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;
     default: break; // unknown parameter, shouldn't happen!
     } //this displays the values and handles 'popups' where it's discrete choices
}

void Spiral2::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamA || index == kParamD)
    {
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
}

bool Spiral2::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

    if (index == kParamA)
    {
        f = string2dB(str, v) * 0.5;
    }
    else if (index == kParamD)
    {
        f = string2dB(str, v);
    }
    else
    {
        f = v / 100.0;
    }

   return true;
}

VstInt32 Spiral2::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Spiral2::getEffectName(char* name) {
    vst_strncpy(name, "Spiral2", kVstMaxProductStrLen); return true;
}

VstPlugCategory Spiral2::getPlugCategory() {return kPlugCategEffect;}

bool Spiral2::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Spiral2", kVstMaxProductStrLen); return true;
}

bool Spiral2::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Spiral2

