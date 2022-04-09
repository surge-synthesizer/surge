/* ========================================
 *  Cabs - Cabs.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Cabs_H
#include "Cabs.h"
#endif

namespace Cabs {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Cabs(audioMaster);}

Cabs::Cabs(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.66;
	C = 0.33;
	D = 0.66;
	E = 0.33;
	F = 0.5;
	
	ataLast3SampleL = ataLast2SampleL = ataLast1SampleL = 0.0; //begin L
	ataHalfwaySampleL = ataHalfDrySampleL = ataHalfDiffSampleL = 0.0;
	ataAL = ataBL = ataCL = ataDrySampleL = ataDiffSampleL = ataPrevDiffSampleL = 0.0;
	
	for(int count = 0; count < 90; count++) {bL[count] = 0;}
	lastSampleL = 0.0;
	lastHalfSampleL = 0.0;
	lastPostSampleL = 0.0;
	lastPostHalfSampleL = 0.0;
	postPostSampleL = 0.0;
	for(int count = 0; count < 20; count++) {dL[count] = 0;}
	controlL = 0;
	iirHeadBumpAL = 0.0;
	iirHeadBumpBL = 0.0;
	iirHalfHeadBumpAL = 0.0;
	iirHalfHeadBumpBL = 0.0;
	for(int count = 0; count < 6; count++) lastRefL[count] = 0.0; //end L
	
	ataLast3SampleR = ataLast2SampleR = ataLast1SampleR = 0.0; //begin R
	ataHalfwaySampleR = ataHalfDrySampleR = ataHalfDiffSampleR = 0.0;
	ataAR = ataBR = ataCR = ataDrySampleR = ataDiffSampleR = ataPrevDiffSampleR = 0.0;
	
	for(int count = 0; count < 90; count++) {bR[count] = 0;}
	lastSampleR = 0.0;
	lastHalfSampleR = 0.0;
	lastPostSampleR = 0.0;
	lastPostHalfSampleR = 0.0;
	postPostSampleR = 0.0;
	for(int count = 0; count < 20; count++) {dR[count] = 0;}
	controlR = 0;
	iirHeadBumpAR = 0.0;
	iirHeadBumpBR = 0.0;
	iirHalfHeadBumpAR = 0.0;
	iirHalfHeadBumpBR = 0.0;
	for(int count = 0; count < 6; count++) lastRefR[count] = 0.0; //end R
	
	flip = false;
	ataFlip = false;
	gcount = 0;
	cycle = 0;
	fpdL = 1.0; while (fpdL < 16386) fpdL = rand()*UINT32_MAX;
	fpdR = 1.0; while (fpdR < 16386) fpdR = rand()*UINT32_MAX;
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

Cabs::~Cabs() {}
VstInt32 Cabs::getVendorVersion () {return 1000;}
void Cabs::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Cabs::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Cabs::getChunk (void** data, bool isPreset)
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

VstInt32 Cabs::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Cabs::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Cabs::getParameter(VstInt32 index) {
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

void Cabs::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Type", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Tone", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Room", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Size", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Off-Axis", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Cabs::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: switch((VstInt32)(EXTV(A) * 5.999 )) //0 to almost edge of # of params
		{	case 0: vst_strncpy (text, "Stack", kVstMaxParamStrLen); break;
			case 1: vst_strncpy (text, "Vintage", kVstMaxParamStrLen); break;
			case 2: vst_strncpy (text, "Boutique", kVstMaxParamStrLen); break;
			case 3: vst_strncpy (text, "Large", kVstMaxParamStrLen); break;
			case 4: vst_strncpy (text, "Small", kVstMaxParamStrLen); break;
			case 5: vst_strncpy (text, "Bass Amp", kVstMaxParamStrLen); break;
			default: break; // unknown parameter, shouldn't happen!
		} break; //E as example 'popup' parameter with four values  */
        case kParamB: float2string (EXTV(B), text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C), text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D), text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E), text, kVstMaxParamStrLen); break;
        case kParamF: float2string (EXTV(F), text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Cabs::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamF: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Cabs::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Cabs::getEffectName(char* name) {
    vst_strncpy(name, "Cabs", kVstMaxProductStrLen); return true;
}

VstPlugCategory Cabs::getPlugCategory() {return kPlugCategEffect;}

bool Cabs::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Cabs", kVstMaxProductStrLen); return true;
}

bool Cabs::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}

bool Cabs::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
} 

} // end namespace Cabs

