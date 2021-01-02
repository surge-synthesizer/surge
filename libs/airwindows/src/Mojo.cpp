/* ========================================
 *  Mojo - Mojo.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Mojo_H
#include "Mojo.h"
#endif

namespace Mojo {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Mojo(audioMaster);}

Mojo::Mojo(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
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

Mojo::~Mojo() {}
VstInt32 Mojo::getVendorVersion () {return 1000;}
void Mojo::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Mojo::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Mojo::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Mojo::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void Mojo::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Mojo::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Mojo::getParameterName(VstInt32 index, char *text)
{
    vst_strncpy(text, "Input Gain", kVstMaxParamStrLen);
}

void Mojo::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal)
{
    float2string((EXTV(A) * 24.0) - 12.0, text, kVstMaxParamStrLen);
}

bool Mojo::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof( str );

   f = (v + 12.0 ) / 24.0;

   return true;
}

bool Mojo::isParameterBipolar(VstInt32 index)
{
   return ( index == kParamA );
}

void Mojo::getParameterLabel(VstInt32 index, char *text)
{
    vst_strncpy(text, "dB", kVstMaxParamStrLen);
}

VstInt32 Mojo::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Mojo::getEffectName(char* name) {
    vst_strncpy(name, "Mojo", kVstMaxProductStrLen); return true;
}

VstPlugCategory Mojo::getPlugCategory() {return kPlugCategEffect;}

bool Mojo::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Mojo", kVstMaxProductStrLen); return true;
}

bool Mojo::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Mojo

