/* ========================================
 *  ADClip7 - ADClip7.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ADClip7_H
#include "ADClip7.h"
#endif

namespace ADClip7 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new ADClip7(audioMaster);}

ADClip7::ADClip7(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.5;
	C = 0.5;
	D = 0.0;

	lastSampleL = 0.0;
	lastSampleR = 0.0;
	for(int count = 0; count < 22199; count++) {bL[count] = 0; bR[count] = 0;}
	gcount = 0;
	lowsL = 0;
	lowsR = 0;
	refclipL = 0.99;
	refclipR = 0.99;
	iirLowsAL = 0.0;
	iirLowsAR = 0.0;
	iirLowsBL = 0.0;	
	iirLowsBR = 0.0;	
	
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

ADClip7::~ADClip7() {}
VstInt32 ADClip7::getVendorVersion () {return 1000;}
void ADClip7::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void ADClip7::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 ADClip7::getChunk (void** data, bool isPreset)
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

VstInt32 ADClip7::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void ADClip7::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float ADClip7::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void ADClip7::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Boost", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Soften", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Enhance", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mode", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void ADClip7::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 18.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: switch((VstInt32)(EXTV(D) * 2.999)) //0 to almost edge of # of params
		{case 0: vst_strncpy (text, "Normal", kVstMaxParamStrLen); break;
		 case 1: vst_strncpy (text, "Gain Matched", kVstMaxParamStrLen); break;
		 case 2: vst_strncpy (text, "Clipped Only", kVstMaxParamStrLen); break;
		 default: break; // unknown parameter, shouldn't happen!
		} break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void ADClip7::getIntegralDisplayForValue( VstInt32 index, float value, char *text )
{
   int iv = (int)(value * 2.999 );
   switch((VstInt32)( value * 2.999 )) //0 to almost edge of # of params
   {
   case 0: vst_strncpy (text, "Normal", kVstMaxParamStrLen); break;
   case 1: vst_strncpy (text, "Gain Matched", kVstMaxParamStrLen); break;
   case 2: vst_strncpy (text, "Clipped Only", kVstMaxParamStrLen); break;
   default: break; // unknown parameter, shouldn't happen!
   }
}
bool ADClip7::parseParameterValueFromString(VstInt32 index, const char* txt, float& f)
{
   float v = std::atof(txt);

   switch (index)
   {
   case kParamA:
   {
	  f = v / 18.0;
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
bool ADClip7::isParameterIntegral(VstInt32 index)
{
   return( index == 3 );
}
int ADClip7::parameterIntegralUpperBound(VstInt32 index)
{
   return 2;
}

void ADClip7::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 ADClip7::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool ADClip7::getEffectName(char* name) {
    vst_strncpy(name, "ADClip7", kVstMaxProductStrLen); return true;
}

VstPlugCategory ADClip7::getPlugCategory() {return kPlugCategEffect;}

bool ADClip7::getProductString(char* text) {
  	vst_strncpy (text, "airwindows ADClip7", kVstMaxProductStrLen); return true;
}

bool ADClip7::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace ADClip7

