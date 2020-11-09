/* ========================================
 *  Pressure4 - Pressure4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Pressure4_H
#include "Pressure4.h"
#endif

namespace Pressure4 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Pressure4(audioMaster);}

Pressure4::Pressure4(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.2;
	C = 1.0;
	D = 1.0;
	fpNShapeL = 0.0;
	fpNShapeR = 0.0;
	muSpeedA = 10000;
	muSpeedB = 10000;
	muCoefficientA = 1;
	muCoefficientB = 1;
	muVary = 1;
	flip = false;	
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

Pressure4::~Pressure4() {}
VstInt32 Pressure4::getVendorVersion () {return 1000;}
void Pressure4::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Pressure4::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Pressure4::getChunk (void** data, bool isPreset)
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

VstInt32 Pressure4::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Pressure4::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        case kParamC: C = value; break; //this is the popup, stored as a float
        case kParamD: D = value; break; //this is the popup, stored as a float
        default: throw; // unknown parameter, shouldn't happen!
    }
	//we can also set other defaults here, and do calculations that only have to happen
	//once when parameters actually change. Here is the 'popup' setting its (global) values.
	//variables can also be set in the processreplacing loop, and there they'll be set every buffersize
	//here they're set when a parameter's actually changed, which should be less frequent, but
	//you must use global variables in the Pressure4.h file to do it.
}

float Pressure4::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
		case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Pressure4::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Pressure", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Speed", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mewiness", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Output Gain", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Pressure4::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break; //also display 0-1 as percent
        case kParamC: float2string ((C*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}
bool Pressure4::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof( str );
   switch( index )
   {
   case kParamC: f = ( v + 1.0 ) / 2.0; break;
   default: f = v;
   }
   return true;
}

bool Pressure4::isParameterBipolar(VstInt32 index)
{
   return ( index == kParamC );
}
void Pressure4::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, " ", kVstMaxParamStrLen); break; //the percent
        case kParamC: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Pressure4::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Pressure4::getEffectName(char* name) {
    vst_strncpy(name, "Pressure4", kVstMaxProductStrLen); return true;
}

VstPlugCategory Pressure4::getPlugCategory() {return kPlugCategEffect;}

bool Pressure4::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Pressure4", kVstMaxProductStrLen); return true;
}

bool Pressure4::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Pressure4

