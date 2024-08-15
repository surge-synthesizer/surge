/* ========================================
 *  IronOxide5 - IronOxide5.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __IronOxide5_H
#include "IronOxide5.h"
#endif

namespace IronOxide5 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new IronOxide5(audioMaster);}

IronOxide5::IronOxide5(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5; //0.0 input trim in dB -18 to +18, default 0 ((A*36.0)-18.0)
	B = 0.562341325190349; //15.0 ips 1.5 to 150.0 logarithmic. B*B
	C = 0.562341325190349; // (C*C) * (C*C) * 150 gives 15 ips (clamp to 1.5 minimum)
	//15.0  (0.316227766016838)squared * 150 gives 15 ips
	D = 0.5; //0.5 flutter 0 to 1
	E = 0.5; //0.5 noise 0 to 1
	F = 0.5; //0.0 output trim in dB -18 to +18, default 0 ((E*36.0)-18.0)
	G = 1.0; //1.0 inv/dry/wet -1 0 1 ((F*2.0)-1.0)
	//needs very fussy defaults to comply with unusual defaults
	
	for (int temp = 0; temp < 263; temp++) {dL[temp] = 0.0; dR[temp] = 0.0;}
	gcount = 0;
	
	fastIIRAL = fastIIRBL = slowIIRAL = slowIIRBL = 0.0;
	fastIIHAL = fastIIHBL = slowIIHAL = slowIIHBL = 0.0;
	iirSamplehAL = iirSamplehBL = 0.0;
	iirSampleAL = iirSampleBL = 0.0;
	prevInputSampleL = 0.0;
	
	fastIIRAR = fastIIRBR = slowIIRAR = slowIIRBR = 0.0;
	fastIIHAR = fastIIHBR = slowIIHAR = slowIIHBR = 0.0;
	iirSamplehAR = iirSamplehBR = 0.0;
	iirSampleAR = iirSampleBR = 0.0;
	prevInputSampleR = 0.0;
		
	flip = false;
	for (int temp = 0; temp < 99; temp++) {flL[temp] = 0.0; flR[temp] = 0.0;}
	
	fstoredcount = 0;	
	sweep = 0.0;
	rateof = 0.5;
	nextmax = 0.5;
	
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

IronOxide5::~IronOxide5() {}
VstInt32 IronOxide5::getVendorVersion () {return 1000;}
void IronOxide5::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void IronOxide5::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 IronOxide5::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	chunkData[4] = E;
	chunkData[5] = F;
	chunkData[6] = G;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 IronOxide5::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	E = pinParameter(chunkData[4]);
	F = pinParameter(chunkData[5]);
	G = pinParameter(chunkData[6]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void IronOxide5::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        case kParamG: G = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float IronOxide5::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        case kParamF: return F; break;
        case kParamG: return G; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void IronOxide5::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input Gain", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Tape High", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Tape Low", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Flutter", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Noise", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "Output Gain", kVstMaxParamStrLen); break;
		case kParamG: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void IronOxide5::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (((EXTV(A) * 36.0) - 18.0), text, kVstMaxParamStrLen); break;
        case kParamB:
        {
            float val = EXTV(B);
            float2string (((val * val) * (val * val) * 148.5) + 1.5, text, kVstMaxParamStrLen);
            break;
        }
        case kParamC:
        {
            float val = EXTV(C);
            float2string (((val * val) * (val * val) * 148.5) + 1.5, text, kVstMaxParamStrLen);
            break;
        }
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamF: float2string (((EXTV(F) * 36.0) - 18.0), text, kVstMaxParamStrLen); break;
        case kParamG: float2string (((EXTV(G) * 2.0) - 1.0) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}
bool IronOxide5::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   float v = std::atof(str);

   switch (index)
   {
   case kParamA:
   {
	   f = (v + 18.0) / 36.0;
       break;
   }
   case kParamB:
   {
	   f = pow((v - 1.5) / 148.5, 0.25);
       break;
   }
   case kParamC:
   {
	   f = pow((v - 1.5) / 148.5, 0.25);
       break;
   }
   case kParamD:
   case kParamE:
   {
	   f = v / 100.0;
	   break;
   }
   case kParamF:
   {
	   f = (v + 18.0) / 36.0;
	   break;
   }
   case kParamG:
   {
	   f = (v + 100.0) / 200.0;
	   break;
   }
   }

   return true;
}

bool IronOxide5::isParameterBipolar(VstInt32 index)
{
   return (index == kParamA || index == kParamF || index == kParamG );
}
void IronOxide5::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "ips", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "ips", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamF: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamG: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 IronOxide5::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool IronOxide5::getEffectName(char* name) {
    vst_strncpy(name, "IronOxide5", kVstMaxProductStrLen); return true;
}

VstPlugCategory IronOxide5::getPlugCategory() {return kPlugCategEffect;}

bool IronOxide5::getProductString(char* text) {
  	vst_strncpy (text, "airwindows IronOxide5", kVstMaxProductStrLen); return true;
}

bool IronOxide5::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace IronOxide5

