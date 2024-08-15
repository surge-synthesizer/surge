/* ========================================
 *  Air - Air.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Air_H
#include "Air.h"
#endif

namespace Air {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Air(audioMaster);}

Air::Air(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	airPrevAL = 0.0;
	airEvenAL = 0.0;
	airOddAL = 0.0;
	airFactorAL = 0.0;
	airPrevBL = 0.0;
	airEvenBL = 0.0;
	airOddBL = 0.0;
	airFactorBL = 0.0;
	airPrevCL = 0.0;
	airEvenCL = 0.0;
	airOddCL = 0.0;
	airFactorCL = 0.0;
	tripletPrevL = 0.0;
	tripletMidL = 0.0;
	tripletAL = 0.0;
	tripletBL = 0.0;
	tripletCL = 0.0;
	tripletFactorL = 0.0;

	airPrevAR = 0.0;
	airEvenAR = 0.0;
	airOddAR = 0.0;
	airFactorAR = 0.0;
	airPrevBR = 0.0;
	airEvenBR = 0.0;
	airOddBR = 0.0;
	airFactorBR = 0.0;
	airPrevCR = 0.0;
	airEvenCR = 0.0;
	airOddCR = 0.0;
	airFactorCR = 0.0;
	tripletPrevR = 0.0;
	tripletMidR = 0.0;
	tripletAR = 0.0;
	tripletBR = 0.0;
	tripletCR = 0.0;
	tripletFactorR = 0.0;
	
	flipA = false;
	flipB = false;
	flop = false;
	count = 1;

	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.0;
	E = 1.0;
	F = 1.0;
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

Air::~Air() {}
VstInt32 Air::getVendorVersion () {return 1000;}
void Air::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Air::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Air::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	chunkData[4] = E;
	chunkData[5] = F;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Air::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	E = pinParameter(chunkData[4]);
	F = pinParameter(chunkData[5]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void Air::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Air::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        case kParamF: return F; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Air::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "22 kHz", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "15 kHz", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "11 kHz", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Resonance", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Air::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (((EXTV(A) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (((EXTV(B) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (((EXTV(C) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: dB2string (EXTV(E), text, kVstMaxParamStrLen); break;
        case kParamF: float2string (EXTV(F) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Air::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamF: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

bool Air::parseParameterValueFromString(VstInt32 index, const char* txt, float& f)
{
   float v = std::atof(txt);

   switch (index)
   {
   case kParamA:
   case kParamB:
   case kParamC:
   {
      f = (v + 100.0) / 200.0;
      break;
   }
   case kParamE:
   {
      f = string2dB(txt, v);

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

VstInt32 Air::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Air::getEffectName(char* name) {
    vst_strncpy(name, "Air", kVstMaxProductStrLen); return true;
}

VstPlugCategory Air::getPlugCategory() {return kPlugCategEffect;}

bool Air::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Air", kVstMaxProductStrLen); return true;
}

bool Air::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Air

