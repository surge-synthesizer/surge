/* ========================================
 *  MatrixVerb - MatrixVerb.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __MatrixVerb_H
#include "MatrixVerb.h"
#endif

namespace MatrixVerb {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new MatrixVerb(audioMaster);}

MatrixVerb::MatrixVerb(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	
	for (int x = 0; x < 11; x++) {biquadA[x] = 0.0;biquadB[x] = 0.0;biquadC[x] = 0.0;}
	
	feedbackAL = feedbackAR = 0.0;
	feedbackBL = feedbackBR = 0.0;
	feedbackCL = feedbackCR = 0.0;
	feedbackDL = feedbackDR = 0.0;
	feedbackEL = feedbackER = 0.0;
	feedbackFL = feedbackFR = 0.0;
	feedbackGL = feedbackGR = 0.0;
	feedbackHL = feedbackHR = 0.0;
	
	int count;
	for(count = 0; count < 8110; count++) {aAL[count] = aAR[count] = 0.0;}
	for(count = 0; count < 7510; count++) {aBL[count] = aBR[count] = 0.0;}
	for(count = 0; count < 7310; count++) {aCL[count] = aCR[count] = 0.0;}
	for(count = 0; count < 6910; count++) {aDL[count] = aDR[count] = 0.0;}
	for(count = 0; count < 6310; count++) {aEL[count] = aER[count] = 0.0;}
	for(count = 0; count < 6110; count++) {aFL[count] = aFR[count] = 0.0;}
	for(count = 0; count < 5510; count++) {aGL[count] = aGR[count] = 0.0;}
	for(count = 0; count < 4910; count++) {aHL[count] = aHR[count] = 0.0;}
	//maximum value needed will be delay * 100, plus 206 (absolute max vibrato depth)
	for(count = 0; count < 4510; count++) {aIL[count] = aIR[count] = 0.0;}
	for(count = 0; count < 4310; count++) {aJL[count] = aJR[count] = 0.0;}
	for(count = 0; count < 3910; count++) {aKL[count] = aKR[count] = 0.0;}
	for(count = 0; count < 3310; count++) {aLL[count] = aLR[count] = 0.0;}
	//maximum value will be delay * 100
	for(count = 0; count < 3110; count++) {aML[count] = aMR[count] = 0.0;}	
	//maximum value will be delay * 100
	countA = 1; delayA = 79;
	countB = 1; delayB = 73;
	countC = 1; delayC = 71;
	countD = 1; delayD = 67;	
	countE = 1; delayE = 61;
	countF = 1; delayF = 59;
	countG = 1; delayG = 53;
	countH = 1; delayH = 47;
	//the householder matrices
	countI = 1; delayI = 43;
	countJ = 1; delayJ = 41;
	countK = 1; delayK = 37;
	countL = 1; delayL = 31;
	//the allpasses
	countM = 1; delayM = 29;
	//the predelay
	depthA = 0.003251;
	depthB = 0.002999;
	depthC = 0.002917;
	depthD = 0.002749;
	depthE = 0.002503;
	depthF = 0.002423;
	depthG = 0.002146;
	depthH = 0.002088;
	//the individual vibrato rates for the delays
	vibAL = rand()*-2147483647;
	vibBL = rand()*-2147483647;
	vibCL = rand()*-2147483647;
	vibDL = rand()*-2147483647;
	vibEL = rand()*-2147483647;
	vibFL = rand()*-2147483647;
	vibGL = rand()*-2147483647;
	vibHL = rand()*-2147483647;
	
	vibAR = rand()*-2147483647;
	vibBR = rand()*-2147483647;
	vibCR = rand()*-2147483647;
	vibDR = rand()*-2147483647;
	vibER = rand()*-2147483647;
	vibFR = rand()*-2147483647;
	vibGR = rand()*-2147483647;
	vibHR = rand()*-2147483647;
	
	
	A = 1.0;
	B = 0.0;
	C = 0.0;
	D = 0.0;
	E = 0.5;
	F = 0.5;
	G = 1.0;
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

MatrixVerb::~MatrixVerb() {}
VstInt32 MatrixVerb::getVendorVersion () {return 1000;}
void MatrixVerb::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void MatrixVerb::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 MatrixVerb::getChunk (void** data, bool isPreset)
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

VstInt32 MatrixVerb::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void MatrixVerb::setParameter(VstInt32 index, float value) {
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

float MatrixVerb::getParameter(VstInt32 index) {
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

void MatrixVerb::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Filter", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Damping", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Speed", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Modulation", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Size", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "Flavor", kVstMaxParamStrLen); break;
		case kParamG: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void MatrixVerb::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: float2string (EXTV(A) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (EXTV(E) * 100.0, text, kVstMaxParamStrLen); break;
		case kParamF: float2string (EXTV(F) * 100.0, text, kVstMaxParamStrLen); break;
		case kParamG: float2string (EXTV(G) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void MatrixVerb::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy(text, "%", kVstMaxParamStrLen);
}

VstInt32 MatrixVerb::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool MatrixVerb::getEffectName(char* name) {
    vst_strncpy(name, "MatrixVerb", kVstMaxProductStrLen); return true;
}

VstPlugCategory MatrixVerb::getPlugCategory() {return kPlugCategEffect;}

bool MatrixVerb::getProductString(char* text) {
  	vst_strncpy (text, "airwindows MatrixVerb", kVstMaxProductStrLen); return true;
}

bool MatrixVerb::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}

bool MatrixVerb::parseParameterValueFromString(VstInt32 index, const char *str, float &f)
{
    auto v = std::atof(str);
    f = v / 100.0;
    return true;
}


} // end namespace MatrixVerb

