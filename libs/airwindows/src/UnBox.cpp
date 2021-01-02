/* ========================================
 *  UnBox - UnBox.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __UnBox_H
#include "UnBox.h"
#endif

namespace UnBox {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new UnBox(audioMaster);}

UnBox::UnBox(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.0;
	C = 0.5;
	
	for(int count = 0; count < 5; count++) {aL[count] = 0.0; bL[count] = 0.0; aR[count] = 0.0; bR[count] = 0.0; e[count] = 0.0;}
	for(int count = 0; count < 11; count++) {cL[count] = 0.0; cR[count] = 0.0; f[count] = 0.0;}
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	
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

UnBox::~UnBox() {}
VstInt32 UnBox::getVendorVersion () {return 1000;}
void UnBox::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void UnBox::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 UnBox::getChunk (void** data, bool isPreset)
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

VstInt32 UnBox::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void UnBox::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float UnBox::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void UnBox::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Amount", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void UnBox::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    if (index == kParamB)
    {
        float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen);
    }
    else
    {
        // let's not show 6.02 dB but clamp it at 6.00 dB just for display
        float v = (index == kParamA ? EXTV(A) : EXTV(C)) * 2.0;
         
        if (v > 1.996)
        {
            v = 1.996;
        }
         
        dB2string(v, text, kVstMaxParamStrLen);
    }
}

bool UnBox::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   if (index == kParamB)
   {
      f = v / 100.0;
   }
   else
   {
      f = string2dB(str, v) * 0.5;
   }

   return true;
}

void UnBox::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamB)
    {
        vst_strncpy(text, "%", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy(text, "dB", kVstMaxParamStrLen);
    }
}

VstInt32 UnBox::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool UnBox::getEffectName(char* name) {
    vst_strncpy(name, "UnBox", kVstMaxProductStrLen); return true;
}

VstPlugCategory UnBox::getPlugCategory() {return kPlugCategEffect;}

bool UnBox::getProductString(char* text) {
  	vst_strncpy (text, "airwindows UnBox", kVstMaxProductStrLen); return true;
}

bool UnBox::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace UnBox

