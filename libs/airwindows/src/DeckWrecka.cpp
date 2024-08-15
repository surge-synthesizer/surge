/* ========================================
 *  Deckwrecka - Deckwrecka.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Deckwrecka_H
#include "DeckWrecka.h"
#endif

namespace DeckWrecka {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Deckwrecka(audioMaster);}

Deckwrecka::Deckwrecka(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	fpd = 17;
	bflip = 1;
	
	iirHeadBumpAL = 0.0;
	iirHeadBumpBL = 0.0;
	iirHeadBumpCL = 0.0;
	
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleCL = 0.0;
	iirSampleDL = 0.0;
	iirSampleEL = 0.0;
	iirSampleFL = 0.0;
	iirSampleGL = 0.0;
	iirSampleHL = 0.0;
	iirSampleIL = 0.0;
	iirSampleJL = 0.0;
	iirSampleKL = 0.0;
	iirSampleLL = 0.0;
	iirSampleML = 0.0;
	iirSampleNL = 0.0;
	iirSampleOL = 0.0;
	iirSamplePL = 0.0;
	iirSampleQL = 0.0;
	iirSampleRL = 0.0;
	iirSampleSL = 0.0;
	iirSampleTL = 0.0;
	iirSampleUL = 0.0;
	iirSampleVL = 0.0;
	iirSampleWL = 0.0;
	iirSampleXL = 0.0;
	iirSampleYL = 0.0;
	iirSampleZL = 0.0;	

	iirHeadBumpAR = 0.0;
	iirHeadBumpBR = 0.0;
	iirHeadBumpCR = 0.0;
	
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	iirSampleCR = 0.0;
	iirSampleDR = 0.0;
	iirSampleER = 0.0;
	iirSampleFR = 0.0;
	iirSampleGR = 0.0;
	iirSampleHR = 0.0;
	iirSampleIR = 0.0;
	iirSampleJR = 0.0;
	iirSampleKR = 0.0;
	iirSampleLR = 0.0;
	iirSampleMR = 0.0;
	iirSampleNR = 0.0;
	iirSampleOR = 0.0;
	iirSamplePR = 0.0;
	iirSampleQR = 0.0;
	iirSampleRR = 0.0;
	iirSampleSR = 0.0;
	iirSampleTR = 0.0;
	iirSampleUR = 0.0;
	iirSampleVR = 0.0;
	iirSampleWR = 0.0;
	iirSampleXR = 0.0;
	iirSampleYR = 0.0;
	iirSampleZR = 0.0;	
	
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

Deckwrecka::~Deckwrecka() {}
VstInt32 Deckwrecka::getVendorVersion () {return 1000;}
void Deckwrecka::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Deckwrecka::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Deckwrecka::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Deckwrecka::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void Deckwrecka::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Deckwrecka::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Deckwrecka::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Amount", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Deckwrecka::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Deckwrecka::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}

bool Deckwrecka::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   f = v / 100.0;

   return true;
}

VstInt32 Deckwrecka::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Deckwrecka::getEffectName(char* name) {
    vst_strncpy(name, "Deckwrecka", kVstMaxProductStrLen); return true;
}

VstPlugCategory Deckwrecka::getPlugCategory() {return kPlugCategEffect;}

bool Deckwrecka::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Deckwrecka", kVstMaxProductStrLen); return true;
}

bool Deckwrecka::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace DeckWrecka

