/* ========================================
 *  OneCornerClip - OneCornerClip.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __OneCornerClip_H
#include "OneCornerClip.h"
#endif

namespace OneCornerClip {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new OneCornerClip(audioMaster);}

OneCornerClip::OneCornerClip(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.33333333333333333;
	B = 0.966;
	C = 0.966;
	D = 0.618;
	E = 1.0;
	
	lastSampleL = 0.0;
	limitPosL = 0.0;
	limitNegL = 0.0;
	lastSampleR = 0.0;
	limitPosR = 0.0;
	limitNegR = 0.0;
	
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

OneCornerClip::~OneCornerClip() {}
VstInt32 OneCornerClip::getVendorVersion () {return 1000;}
void OneCornerClip::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void OneCornerClip::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 OneCornerClip::getChunk (void** data, bool isPreset)
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

VstInt32 OneCornerClip::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void OneCornerClip::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float OneCornerClip::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void OneCornerClip::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Pos Thr", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Neg Thr", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Voicing", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void OneCornerClip::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A*36.0)-12.0, text, kVstMaxParamStrLen); break;
        case kParamB: dB2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: dB2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (E, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool OneCornerClip::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof( str );
   switch( index )
   {
   case kParamA: f = ( v + 12.0 ) / 36.0; break;
   case kParamB:
   case kParamC:
      f = pow( 10.0, ( v / 20.0 ) ); break;
   default:
      f = v;
   }
   return true;
}
void OneCornerClip::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 OneCornerClip::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool OneCornerClip::getEffectName(char* name) {
    vst_strncpy(name, "OneCornerClip", kVstMaxProductStrLen); return true;
}

VstPlugCategory OneCornerClip::getPlugCategory() {return kPlugCategEffect;}

bool OneCornerClip::getProductString(char* text) {
  	vst_strncpy (text, "airwindows OneCornerClip", kVstMaxProductStrLen); return true;
}

bool OneCornerClip::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace OneCornerClip

