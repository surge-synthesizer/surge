/* ========================================
 *  VoiceOfTheStarship - VoiceOfTheStarship.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __VoiceOfTheStarship_H
#include "VoiceOfTheStarship.h"
#endif

namespace VoiceOfTheStarship {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new VoiceOfTheStarship(audioMaster);}

VoiceOfTheStarship::VoiceOfTheStarship(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.0;
	position = 99999999;
	quadratic = 0;
	noiseAL = 0.0;
	noiseBL = 0.0;
	noiseCL = 0.0;
	flipL = false;
	noiseAR = 0.0;
	noiseBR = 0.0;
	noiseCR = 0.0;
	flipR = false;
	filterflip = false;
	for(int count = 0; count < 11; count++) {bL[count] = 0.0; bR[count] = 0.0; f[count] = 0.0;}
	lastAlgorithm = 0;
	
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

VoiceOfTheStarship::~VoiceOfTheStarship() {}
VstInt32 VoiceOfTheStarship::getVendorVersion () {return 1000;}
void VoiceOfTheStarship::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void VoiceOfTheStarship::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 VoiceOfTheStarship::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 VoiceOfTheStarship::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void VoiceOfTheStarship::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float VoiceOfTheStarship::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void VoiceOfTheStarship::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Filter", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Algorithm", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void VoiceOfTheStarship::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB:
        {
            auto type = std::to_string((int)floor(1.0 + (EXTV(B) * 16.0)));
            std::string txt = "Type " + type;

		    vst_strncpy (text, txt.c_str(), kVstMaxParamStrLen);

            break;
        }
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool VoiceOfTheStarship::isParameterIntegral(VstInt32 index)
{
   return (index == kParamB);
}

int VoiceOfTheStarship::parameterIntegralUpperBound(VstInt32 index)
{
   return 16;
}

void VoiceOfTheStarship::getIntegralDisplayForValue(VstInt32 index, float value, char* text)
{
   auto type = std::to_string((int)floor(1.0 + (value * 16.0)));
   std::string txt = "Type " + type;

   vst_strncpy(text, txt.c_str(), kVstMaxParamStrLen);
}

void VoiceOfTheStarship::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamA)
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "", kVstMaxParamStrLen);
    }
}

VstInt32 VoiceOfTheStarship::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool VoiceOfTheStarship::getEffectName(char* name) {
    vst_strncpy(name, "VoiceOfTheStarship", kVstMaxProductStrLen); return true;
}

VstPlugCategory VoiceOfTheStarship::getPlugCategory() {return kPlugCategEffect;}

bool VoiceOfTheStarship::getProductString(char* text) {
  	vst_strncpy (text, "airwindows VoiceOfTheStarship", kVstMaxProductStrLen); return true;
}

bool VoiceOfTheStarship::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace VoiceOfTheStarship

