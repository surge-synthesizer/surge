/* ========================================
 *  Logical4 - Logical4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Logical4_H
#include "Logical4.h"
#endif

namespace Logical4 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Logical4(audioMaster);}

Logical4::Logical4(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.2;
	C = 0.19202020202020202;
	D = 0.5;
	E = 1.0;
	
	//begin ButterComps
	controlAposL = 1.0;
	controlAnegL = 1.0;
	controlBposL = 1.0;
	controlBnegL = 1.0;
	targetposL = 1.0;
	targetnegL = 1.0;
	
	controlAposBL = 1.0;
	controlAnegBL = 1.0;
	controlBposBL = 1.0;
	controlBnegBL = 1.0;
	targetposBL = 1.0;
	targetnegBL = 1.0;
	
	controlAposCL = 1.0;
	controlAnegCL = 1.0;
	controlBposCL = 1.0;
	controlBnegCL = 1.0;
	targetposCL = 1.0;
	targetnegCL = 1.0;
	
	avgAL = avgBL = avgCL = avgDL = avgEL = avgFL = 0.0;
	nvgAL = nvgBL = nvgCL = nvgDL = nvgEL = nvgFL = 0.0;
	//end ButterComps
	
	//begin ButterComps
	controlAposR = 1.0;
	controlAnegR = 1.0;
	controlBposR = 1.0;
	controlBnegR = 1.0;
	targetposR = 1.0;
	targetnegR = 1.0;
	
	controlAposBR = 1.0;
	controlAnegBR = 1.0;
	controlBposBR = 1.0;
	controlBnegBR = 1.0;
	targetposBR = 1.0;
	targetnegBR = 1.0;
	
	controlAposCR = 1.0;
	controlAnegCR = 1.0;
	controlBposCR = 1.0;
	controlBnegCR = 1.0;
	targetposCR = 1.0;
	targetnegCR = 1.0;
	
	avgAR = avgBR = avgCR = avgDR = avgER = avgFR = 0.0;
	nvgAR = nvgBR = nvgCR = nvgDR = nvgER = nvgFR = 0.0;
	//end ButterComps
	
	//begin Power Sags
	for(int count = 0; count < 999; count++) {dL[count] = 0; bL[count] = 0; cL[count] = 0; dR[count] = 0; bR[count] = 0; cR[count] = 0;}
	controlL = 0; controlBL = 0; controlCL = 0;
	controlR = 0; controlBR = 0; controlCR = 0;
	
	gcount = 0;
	//end Power Sags
	fpFlip = true;
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

Logical4::~Logical4() {}
VstInt32 Logical4::getVendorVersion () {return 1000;}
void Logical4::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Logical4::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Logical4::getChunk (void** data, bool isPreset)
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

VstInt32 Logical4::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Logical4::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Logical4::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Logical4::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Threshold", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Ratio", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Attack", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Makeup Gain", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Logical4::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string ((EXTV(A) * 40.0) - 20.0, text, kVstMaxParamStrLen); break;
        case kParamB:
        {
            float val = EXTV(B);
            float2string (((val * val) * 15.0) + 1.0, text, kVstMaxParamStrLen);
            break;
        }
        case kParamC:
        {
			float val = EXTV(C);
			float2string (((val * val) * 99.0) + 1.0, text, kVstMaxParamStrLen);
			break;
        }
        case kParamD: float2string ((EXTV(D) * 40.0) - 20.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool Logical4::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof( str );
   switch( index )
   {
   case kParamA: f = ( v + 20 ) / 40.0; break;
   case kParamB: f = v < 0 ? 0 : sqrt( ( v - 1 ) / 15.0 ); break;
   case kParamC: f = v < 0 ? 0 : sqrt( ( v - 1 ) / 99.0 ); break;
   case kParamD: f = ( v + 20 ) / 40.0; break;
   case kParamE: f = v / 100.0; break;
   }
   return true;
}

bool Logical4::isParameterBipolar(VstInt32 index)
{
   return( index == kParamA || index == kParamD );
}

void Logical4::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, ": 1", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "ms", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Logical4::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Logical4::getEffectName(char* name) {
    vst_strncpy(name, "Logical4", kVstMaxProductStrLen); return true;
}

VstPlugCategory Logical4::getPlugCategory() {return kPlugCategEffect;}

bool Logical4::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Logical4", kVstMaxProductStrLen); return true;
}

bool Logical4::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Logical4

