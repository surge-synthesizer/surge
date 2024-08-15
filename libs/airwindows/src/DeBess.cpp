/* ========================================
 *  DeBess - DeBess.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeBess_H
#include "DeBess.h"
#endif

namespace DeBess {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DeBess(audioMaster);}

DeBess::DeBess(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.5;
	C = 0.5;
	D = 0.5;
	E = 0.0;
	for (int x = 0; x < 41; x++) {
		sL[x] = 0.0; mL[x] = 0.0; cL[x] = 0.0;
		sR[x] = 0.0; mR[x] = 0.0; cR[x] = 0.0;
	}
	ratioAL = ratioBL = 1.0;
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	ratioAR = ratioBR = 1.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	
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

DeBess::~DeBess() {}
VstInt32 DeBess::getVendorVersion () {return 1000;}
void DeBess::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DeBess::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 DeBess::getChunk (void** data, bool isPreset)
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

VstInt32 DeBess::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void DeBess::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float DeBess::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DeBess::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Intensity", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Sharpness", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Depth", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Filter", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Monitor", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void DeBess::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: switch((VstInt32)(EXTV(E) * 1.999 )) //0 to almost edge of # of params
		{case 0: vst_strncpy (text, "Normal", kVstMaxParamStrLen); break;
		 case 1: vst_strncpy (text, "Esses Only", kVstMaxParamStrLen); break;
		 default: break; // unknown parameter, shouldn't happen!
		} break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void DeBess::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy (text, "%", kVstMaxParamStrLen);
}

void DeBess::getIntegralDisplayForValue(VstInt32 index, float value, char* text)
{
   int iv = (int)(value * 1.999);

   switch ((VstInt32)(value * 1.999)) // 0 to almost edge of # of params
   {
   case 0:
      vst_strncpy(text, "Normal", kVstMaxParamStrLen);
      break;
   case 1:
      vst_strncpy(text, "Esses Only", kVstMaxParamStrLen);
      break;
   default:
      break; // unknown parameter, shouldn't happen!
   }
}

bool DeBess::isParameterIntegral(VstInt32 index)
{
   return (index == 4);
}

int DeBess::parameterIntegralUpperBound(VstInt32 index)
{
   return 1;
}

bool DeBess::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   f = v / 100.0;

   return true;
}

VstInt32 DeBess::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DeBess::getEffectName(char* name) {
    vst_strncpy(name, "DeBess", kVstMaxProductStrLen); return true;
}

VstPlugCategory DeBess::getPlugCategory() {return kPlugCategEffect;}

bool DeBess::getProductString(char* text) {
  	vst_strncpy (text, "airwindows DeBess", kVstMaxProductStrLen); return true;
}

bool DeBess::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace DeBess

