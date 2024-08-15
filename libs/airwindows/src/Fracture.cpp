/* ========================================
 *  Fracture - Fracture.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Fracture_H
#include "Fracture.h"
#endif

namespace Fracture {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Fracture(audioMaster);}

Fracture::Fracture(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.25; //1 from 0 to 4: A*4
	B = 0.0;
	C = 1.0;
	D = 1.0;
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

Fracture::~Fracture() {}
VstInt32 Fracture::getVendorVersion () {return 1000;}
void Fracture::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Fracture::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Fracture::getChunk (void** data, bool isPreset)
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

VstInt32 Fracture::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Fracture::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Fracture::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Fracture::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Fracture", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Fracture::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA:
        {
            // let's not show 12.04 dB but clamp it at 12.00 dB just for display
            float v = EXTV(A) * 4.0;
            
            if (v > 3.983)
            {
               v = 3.983;
            }

            dB2string (v, text, kVstMaxParamStrLen);
            break;
        }
        case kParamB:
        {
            std::string txt = std::to_string((int)floor(1.0 + (B * 3.999))).append("x");

            vst_strncpy (text, txt.c_str(), kVstMaxParamStrLen);

            break;
        }
        case kParamC: dB2string (EXTV(C), text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D), text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Fracture::getIntegralDisplayForValue(VstInt32 index, float value, char* text)
{
   std::string txt = std::to_string((int)floor(1.0 + (value * 3.999))).append("x");
   vst_strncpy(text, txt.c_str(), kVstMaxParamStrLen);
}

bool Fracture::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);
   
   switch (index)
   {
   case kParamA:
   {
       f = string2dB(str, v) * 0.25;
       break;
   }
   case kParamC:
   {
       f = string2dB(str, v);
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

bool Fracture::isParameterIntegral(VstInt32 index)
{
   return index==kParamB;
}

int Fracture::parameterIntegralUpperBound(VstInt32 index)
{
   return 3;
}

void Fracture::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Fracture::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Fracture::getEffectName(char* name) {
    vst_strncpy(name, "Fracture", kVstMaxProductStrLen); return true;
}

VstPlugCategory Fracture::getPlugCategory() {return kPlugCategEffect;}

bool Fracture::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Fracture", kVstMaxProductStrLen); return true;
}

bool Fracture::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace Fracture

