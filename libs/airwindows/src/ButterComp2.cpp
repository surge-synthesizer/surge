/* ========================================
 *  ButterComp2 - ButterComp2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ButterComp2_H
#include "ButterComp2.h"
#endif

namespace ButterComp2 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new ButterComp2(audioMaster);}

ButterComp2::ButterComp2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	controlAposL = 1.0;
	controlAnegL = 1.0;
	controlBposL = 1.0;
	controlBnegL = 1.0;
	targetposL = 1.0;
	targetnegL = 1.0;
	lastOutputL = 0.0;

	controlAposR = 1.0;
	controlAnegR = 1.0;
	controlBposR = 1.0;
	controlBnegR = 1.0;
	targetposR = 1.0;
	targetnegR = 1.0;
	lastOutputR = 0.0;

	flip = false;
	A = 0.0;
	B = 0.5;
	C = 1.0;
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

ButterComp2::~ButterComp2() {}
VstInt32 ButterComp2::getVendorVersion () {return 1000;}
void ButterComp2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void ButterComp2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 ButterComp2::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 ButterComp2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void ButterComp2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float ButterComp2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void ButterComp2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Compress", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void ButterComp2::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB:
		{
		   // let's not show 6.02 dB but clamp it at 6.00 dB just for display
           float v = EXTV(B) * 2.0;
           
		   if (v > 1.996)
           {
			  v = 1.996;
           }

           dB2string(v, text, kVstMaxParamStrLen);
           break;
        }
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool ButterComp2::parseParameterValueFromString(VstInt32 index, const char* txt, float& f)
{
   auto v = std::atof(txt);
   
   if (index == kParamB)
   {
      f = string2dB(txt, v) * 0.5;
   }
   else
   {
      f = v / 100.0;
   }

   return true;
}

void ButterComp2::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 ButterComp2::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool ButterComp2::getEffectName(char* name) {
    vst_strncpy(name, "ButterComp2", kVstMaxProductStrLen); return true;
}

VstPlugCategory ButterComp2::getPlugCategory() {return kPlugCategEffect;}

bool ButterComp2::getProductString(char* text) {
  	vst_strncpy (text, "airwindows ButterComp2", kVstMaxProductStrLen); return true;
}

bool ButterComp2::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace ButterComp2

