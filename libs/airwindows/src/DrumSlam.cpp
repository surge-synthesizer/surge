/* ========================================
 *  DrumSlam - DrumSlam.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DrumSlam_H
#include "DrumSlam.h"
#endif

namespace DrumSlam {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DrumSlam(audioMaster);}

DrumSlam::DrumSlam(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 1.0;
	C = 1.0;

	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleCL = 0.0;
	iirSampleDL = 0.0;
	iirSampleEL = 0.0;
	iirSampleFL = 0.0;
	iirSampleGL = 0.0;
	iirSampleHL = 0.0;
	lastSampleL = 0.0;

	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	iirSampleCR = 0.0;
	iirSampleDR = 0.0;
	iirSampleER = 0.0;
	iirSampleFR = 0.0;
	iirSampleGR = 0.0;
	iirSampleHR = 0.0;
	lastSampleR = 0.0;	
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

DrumSlam::~DrumSlam() {}
VstInt32 DrumSlam::getVendorVersion () {return 1000;}
void DrumSlam::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DrumSlam::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 DrumSlam::getChunk (void** data, bool isPreset)
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

VstInt32 DrumSlam::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void DrumSlam::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float DrumSlam::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DrumSlam::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void DrumSlam::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA:
        {
            // let's not show 12.04 dB but clamp it at 12.00 dB just for display
            float v = (EXTV(A) * 3.0) + 1.0;
		 
            if (v > 3.983)
            {
               v = 3.983;
            }
		 
            dB2string (v, text, kVstMaxParamStrLen); break;
        }
        case kParamB: dB2string (EXTV(B), text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void DrumSlam::getParameterLabel(VstInt32 index, char *text) {
    if (index == kParamC)
	{
        vst_strncpy (text, "%", kVstMaxParamStrLen);
    }
    else
    {
        vst_strncpy (text, "dB", kVstMaxParamStrLen);
    }
}

bool DrumSlam::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
   auto v = std::atof(str);

   switch (index)
   {
   case kParamA:
   {
      f = (string2dB(str, v) - 1.0) / 3.0;
	  break;
   }
   case kParamB:
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

VstInt32 DrumSlam::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DrumSlam::getEffectName(char* name) {
    vst_strncpy(name, "DrumSlam", kVstMaxProductStrLen); return true;
}

VstPlugCategory DrumSlam::getPlugCategory() {return kPlugCategEffect;}

bool DrumSlam::getProductString(char* text) {
  	vst_strncpy (text, "airwindows DrumSlam", kVstMaxProductStrLen); return true;
}

bool DrumSlam::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace DrumSlam

