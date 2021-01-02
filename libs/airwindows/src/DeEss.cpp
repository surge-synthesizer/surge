/* ========================================
 *  DeEss - DeEss.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeEss_H
#include "DeEss.h"
#endif

namespace DeEss {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DeEss(audioMaster);}

DeEss::DeEss(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.5; //-48.0 to 0.0
	C = 0.5;
	
	s1L = s2L = s3L = s4L = s5L = s6L= s7L = 0.0;
	m1L = m2L = m3L = m4L = m5L = m6L = 0.0;
	c1L = c2L = c3L = c4L = c5L = 0.0;
	ratioAL = ratioBL = 1.0;
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	
	s1R = s2R = s3R = s4R = s5R = s6R = s7R = 0.0;
	m1R = m2R = m3R = m4R = m5R = m6R = 0.0;
	c1R = c2R = c3R = c4R = c5R = 0.0;
	ratioAR = ratioBR = 1.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	
	flip = false;	
	
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

DeEss::~DeEss() {}
VstInt32 DeEss::getVendorVersion () {return 1000;}
void DeEss::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DeEss::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 DeEss::getChunk (void** data, bool isPreset)
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

VstInt32 DeEss::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void DeEss::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float DeEss::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DeEss::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Intensity", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Ducking", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Frequency", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void DeEss::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string ((EXTV(B) - 1.0) * 48.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}


bool DeEss::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof( str );
   switch( index )
   {
   case kParamB:
   {
      f = ( v / 48.f ) + 1.0;
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

void DeEss::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 DeEss::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DeEss::getEffectName(char* name) {
    vst_strncpy(name, "DeEss", kVstMaxProductStrLen); return true;
}

VstPlugCategory DeEss::getPlugCategory() {return kPlugCategEffect;}

bool DeEss::getProductString(char* text) {
  	vst_strncpy (text, "airwindows DeEss", kVstMaxProductStrLen); return true;
}

bool DeEss::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace DeEss

