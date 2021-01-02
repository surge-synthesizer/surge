/* ========================================
 *  NCSeventeen - NCSeventeen.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NCSeventeen_H
#include "NCSeventeen.h"
#endif

namespace NCSeventeen {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new NCSeventeen(audioMaster);}

NCSeventeen::NCSeventeen(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 1.0;
	
	lastSampleL = 0.0;
	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	basslevL = 0.0;
	treblevL = 0.0;
	cheblevL = 0.0;
	
	lastSampleR = 0.0;
	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	basslevR = 0.0;
	treblevR = 0.0;
	cheblevR = 0.0;
	
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

NCSeventeen::~NCSeventeen() {}
VstInt32 NCSeventeen::getVendorVersion () {return 1000;}
void NCSeventeen::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void NCSeventeen::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 NCSeventeen::getChunk (void** data, bool isPreset)
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

VstInt32 NCSeventeen::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void NCSeventeen::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float NCSeventeen::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void NCSeventeen::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Louder", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void NCSeventeen::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 24.0, text, kVstMaxParamStrLen); break;
        case kParamB: dB2string (EXTV(B), text, kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool NCSeventeen::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   if (index == kParamA)
   {
      f = v / 24.0;
   }
   else
   {
      f = string2dB(str, v);
   }

   return true;
}
void NCSeventeen::getParameterLabel(VstInt32 index, char *text)
{
   vst_strncpy(text, "dB", kVstMaxParamStrLen);
}

VstInt32 NCSeventeen::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool NCSeventeen::getEffectName(char* name) {
    vst_strncpy(name, "NC-17", kVstMaxProductStrLen); return true;
}

VstPlugCategory NCSeventeen::getPlugCategory() {return kPlugCategEffect;}

bool NCSeventeen::getProductString(char* text) {
  	vst_strncpy (text, "airwindows NC-17", kVstMaxProductStrLen); return true;
}

bool NCSeventeen::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace NCSeventeen

