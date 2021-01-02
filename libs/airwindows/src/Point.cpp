/* ========================================
 *  Point - Point.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Point_H
#include "Point.h"
#endif

namespace Point {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Point(audioMaster);}

Point::Point(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	nibAL = 0.0;
	nobAL = 0.0;
	nibBL = 0.0;
	nobBL = 0.0;
	nibAR = 0.0;
	nobAR = 0.0;
	nibBR = 0.0;
	nobBR = 0.0;
	
	fpNShapeL = 0.0;
	fpNShapeR = 0.0;
	fpFlip = true;
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

Point::~Point() {}
VstInt32 Point::getVendorVersion () {return 1000;}
void Point::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Point::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Point::getChunk (void** data, bool isPreset)
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

VstInt32 Point::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Point::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Point::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Point::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input Gain", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Transient Shape", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Reaction Speed", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Point::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string ((EXTV(A) * 24.0) - 12.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string ((EXTV(B) * 200.0) - 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool Point::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   switch (index)
   {
   case kParamA: f = ( v + 12.0 ) / 24.0; break;
   case kParamB: f = ( v + 100.0 ) / 200.0; break;
   default: f = v / 100.0; break;
   }
   return true;
}

bool Point::isParameterBipolar(VstInt32 index)
{
   return (index != kParamC);
}

void Point::getParameterLabel(VstInt32 index, char *text)
{
    if (index == kParamA)
	{
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
}

VstInt32 Point::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Point::getEffectName(char* name) {
    vst_strncpy(name, "Point", kVstMaxProductStrLen); return true;
}

VstPlugCategory Point::getPlugCategory() {return kPlugCategEffect;}

bool Point::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Point", kVstMaxProductStrLen); return true;
}

bool Point::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Point

