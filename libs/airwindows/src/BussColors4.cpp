/* ========================================
 *  BussColors4 - BussColors4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BussColors4_H
#include "BussColors4.h"
#endif

namespace BussColors4 {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new BussColors4(audioMaster);}

BussColors4::BussColors4(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.5;
	C = 0.5;
	D = 1.0;

	for (int count = 0; count < 174; count++) {bL[count] = 0; bR[count] = 0;}
	for (int count = 0; count < 99; count++) {dL[count] = 0; dR[count] = 0;}
	for (int count = 0; count < 34; count++) c[count] = count; //initial setup for 44.1K
	g[1] = pow(10.0, -5.2 / 14.0); //dark
	g[2] = pow(10.0, -6.2 / 14.0); //rock
	g[3] = pow(10.0, -2.9 / 14.0); //lush
	g[4] = pow(10.0, -1.1 / 14.0); //vibe
	g[5] = pow(10.0, -5.1 / 14.0); //holo
	g[6] = pow(10.0, -3.6 / 14.0); //punch
	g[7] = pow(10.0, -2.3 / 14.0); //steel
	g[8] = pow(10.0, -2.9 / 14.0); //tube
	//preset gains for models
	outg[1] = pow(10.0, -0.3 / 14.0); //dark
	outg[2] = pow(10.0, 0.5 / 14.0); //rock
	outg[3] = pow(10.0, -0.7 / 14.0); //lush
	outg[4] = pow(10.0, -0.6 / 14.0); //vibe
	outg[5] = pow(10.0, -0.2 / 14.0); //holo
	outg[6] = pow(10.0, 0.3 / 14.0); //punch
	outg[7] = pow(10.0, 0.1 / 14.0); //steel
	outg[8] = pow(10.0, 0.9 / 14.0); //tube
	//preset gains for models
	controlL = 0;
	controlR = 0;
	slowdynL = 0;
	slowdynR = 0;
	gcount = 0;
	
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

BussColors4::~BussColors4() {}
VstInt32 BussColors4::getVendorVersion () {return 1000;}
void BussColors4::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void BussColors4::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 BussColors4::getChunk (void** data, bool isPreset)
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

VstInt32 BussColors4::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void BussColors4::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        case kParamC: C = value; break;
        case kParamD: D = value; break; //this is the popup, stored as a float
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float BussColors4::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void BussColors4::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Color", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Input Gain", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Makeup Gain", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void BussColors4::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: switch((VstInt32)(EXTV(A) * 7.999 )) //0 to almost edge of # of params
		{case 0: vst_strncpy (text, "Dark", kVstMaxParamStrLen); break;
			case 1: vst_strncpy (text, "Rock", kVstMaxParamStrLen); break;
			case 2: vst_strncpy (text, "Lush", kVstMaxParamStrLen); break;
			case 3: vst_strncpy (text, "Vibe", kVstMaxParamStrLen); break;
			case 4: vst_strncpy (text, "Holo", kVstMaxParamStrLen); break;
			case 5: vst_strncpy (text, "Punch", kVstMaxParamStrLen); break;
			case 6: vst_strncpy (text, "Steel", kVstMaxParamStrLen); break;
			case 7: vst_strncpy (text, "Tube", kVstMaxParamStrLen); break;
			default: break; // unknown parameter, shouldn't happen!
		} break; //completed A 'popup' parameter, exit
        case kParamB: float2string ((EXTV(B) * 36.0) - 18.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string ((EXTV(C) * 36.0) - 18.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void BussColors4::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

bool BussColors4::isParameterIntegral(VstInt32 index)
{
   if( index == kParamA ) return true;
   return false;
}

int BussColors4::parameterIntegralUpperBound( VstInt32 index )
{
   if( index == kParamA ) return 7;
   return -1;
}

bool BussColors4::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   float v = std::atof(str);

   switch (index)
   {
   case kParamB:
   case kParamC:
   {
      f = (v + 18.0) / 36.0;
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

VstInt32 BussColors4::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool BussColors4::getEffectName(char* name) {
    vst_strncpy(name, "BussColors4", kVstMaxProductStrLen); return true;
}

VstPlugCategory BussColors4::getPlugCategory() {return kPlugCategEffect;}

bool BussColors4::getProductString(char* text) {
  	vst_strncpy (text, "airwindows BussColors4", kVstMaxProductStrLen); return true;
}

bool BussColors4::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
void BussColors4::getIntegralDisplayForValue(VstInt32 index, float value, char* text)
{
   switch((VstInt32)( value * 7.999 )) //0 to almost edge of # of params
   {case 0: vst_strncpy (text, "Dark", kVstMaxParamStrLen); break;
   case 1: vst_strncpy (text, "Rock", kVstMaxParamStrLen); break;
   case 2: vst_strncpy (text, "Lush", kVstMaxParamStrLen); break;
   case 3: vst_strncpy (text, "Vibe", kVstMaxParamStrLen); break;
   case 4: vst_strncpy (text, "Holo", kVstMaxParamStrLen); break;
   case 5: vst_strncpy (text, "Punch", kVstMaxParamStrLen); break;
   case 6: vst_strncpy (text, "Steel", kVstMaxParamStrLen); break;
   case 7: vst_strncpy (text, "Tube", kVstMaxParamStrLen); break;
   default: break; // unknown parameter, shouldn't happen!
   }
}

} // end namespace BussColors4

