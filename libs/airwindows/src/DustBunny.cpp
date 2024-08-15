/* ========================================
 *  DustBunny - DustBunny.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DustBunny_H
#include "DustBunny.h"
#endif

namespace DustBunny {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DustBunny(audioMaster);}

DustBunny::DustBunny(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	
	LataLast3Sample = LataLast2Sample = LataLast1Sample = 0.0;
	LataHalfwaySample = LataHalfDrySample = LataHalfDiffSample = 0.0;
	LataA = LataB = LataC = LataDrySample = LataDiffSample = LataPrevDiffSample = 0.0;
	LataUpsampleHighTweak = 0.0414213562373095048801688; //more adds treble to upsampling
	LataDecay = 0.915965594177219015; //Catalan's constant, more adds focus and clarity
	
	RataLast3Sample = RataLast2Sample = RataLast1Sample = 0.0;
	RataHalfwaySample = RataHalfDrySample = RataHalfDiffSample = 0.0;
	RataA = RataB = RataC = RataDrySample = RataDiffSample = RataPrevDiffSample = 0.0;
	RataUpsampleHighTweak = 0.0414213562373095048801688; //more adds treble to upsampling
	RataDecay = 0.915965594177219015; //CRatalan's constant, more adds focus and clarity
	LataFlip = false; //end reset of antialias parameters
	RataFlip = false; //end reset of antialias parameters
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

DustBunny::~DustBunny() {}
VstInt32 DustBunny::getVendorVersion () {return 1000;}
void DustBunny::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DustBunny::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 DustBunny::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 DustBunny::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void DustBunny::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float DustBunny::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DustBunny::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Amount", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void DustBunny::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void DustBunny::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}

bool DustBunny::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   f = v / 100.0;

   return true;
}

VstInt32 DustBunny::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DustBunny::getEffectName(char* name) {
    vst_strncpy(name, "DustBunny", kVstMaxProductStrLen); return true;
}

VstPlugCategory DustBunny::getPlugCategory() {return kPlugCategEffect;}

bool DustBunny::getProductString(char* text) {
  	vst_strncpy (text, "airwindows DustBunny", kVstMaxProductStrLen); return true;
}

bool DustBunny::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace DustBunny

