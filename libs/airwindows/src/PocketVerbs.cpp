/* ========================================
 *  PocketVerbs - PocketVerbs.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PocketVerbs_H
#include "PocketVerbs.h"
#endif

namespace PocketVerbs {


// AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PocketVerbs(audioMaster);}

PocketVerbs::PocketVerbs(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 0.5;
	C = 0.0;
	D = 0.5;
	
	for(count = 0; count < 6; count++) {dAL[count] = 0.0; dBL[count] = 0.0; dCL[count] = 0.0; dDL[count] = 0.0; dEL[count] = 0.0;
		dFL[count] = 0.0; dGL[count] = 0.0; dHL[count] = 0.0; dIL[count] = 0.0; dJL[count] = 0.0; dKL[count] = 0.0; dLL[count] = 0.0;
		dML[count] = 0.0; dNL[count] = 0.0; dOL[count] = 0.0; dPL[count] = 0.0; dQL[count] = 0.0; dRL[count] = 0.0; dSL[count] = 0.0;
		dTL[count] = 0.0; dUL[count] = 0.0; dVL[count] = 0.0; dWL[count] = 0.0; dXL[count] = 0.0; dYL[count] = 0.0; dZL[count] = 0.0;}
	
	for(count = 0; count < 15149; count++) {aAL[count] = 0.0;}
	for(count = 0; count < 15149; count++) {oAL[count] = 0.0;}
	for(count = 0; count < 14617; count++) {aBL[count] = 0.0;}
	for(count = 0; count < 14617; count++) {oBL[count] = 0.0;}
	for(count = 0; count < 14357; count++) {aCL[count] = 0.0;}
	for(count = 0; count < 14357; count++) {oCL[count] = 0.0;}
	for(count = 0; count < 13817; count++) {aDL[count] = 0.0;}
	for(count = 0; count < 13817; count++) {oDL[count] = 0.0;}
	for(count = 0; count < 13561; count++) {aEL[count] = 0.0;}
	for(count = 0; count < 13561; count++) {oEL[count] = 0.0;}
	for(count = 0; count < 13045; count++) {aFL[count] = 0.0;}
	for(count = 0; count < 13045; count++) {oFL[count] = 0.0;}
	for(count = 0; count < 11965; count++) {aGL[count] = 0.0;}
	for(count = 0; count < 11965; count++) {oGL[count] = 0.0;}
	for(count = 0; count < 11129; count++) {aHL[count] = 0.0;}
	for(count = 0; count < 11129; count++) {oHL[count] = 0.0;}
	for(count = 0; count < 10597; count++) {aIL[count] = 0.0;}
	for(count = 0; count < 10597; count++) {oIL[count] = 0.0;}
	for(count = 0; count < 9809; count++) {aJL[count] = 0.0;}
	for(count = 0; count < 9809; count++) {oJL[count] = 0.0;}
	for(count = 0; count < 9521; count++) {aKL[count] = 0.0;}
	for(count = 0; count < 9521; count++) {oKL[count] = 0.0;}
	for(count = 0; count < 8981; count++) {aLL[count] = 0.0;}
	for(count = 0; count < 8981; count++) {oLL[count] = 0.0;}
	for(count = 0; count < 8785; count++) {aML[count] = 0.0;}
	for(count = 0; count < 8785; count++) {oML[count] = 0.0;}
	for(count = 0; count < 8461; count++) {aNL[count] = 0.0;}
	for(count = 0; count < 8461; count++) {oNL[count] = 0.0;}
	for(count = 0; count < 8309; count++) {aOL[count] = 0.0;}
	for(count = 0; count < 8309; count++) {oOL[count] = 0.0;}
	for(count = 0; count < 7981; count++) {aPL[count] = 0.0;}
	for(count = 0; count < 7981; count++) {oPL[count] = 0.0;}
	for(count = 0; count < 7321; count++) {aQL[count] = 0.0;}
	for(count = 0; count < 7321; count++) {oQL[count] = 0.0;}
	for(count = 0; count < 6817; count++) {aRL[count] = 0.0;}
	for(count = 0; count < 6817; count++) {oRL[count] = 0.0;}
	for(count = 0; count < 6505; count++) {aSL[count] = 0.0;}
	for(count = 0; count < 6505; count++) {oSL[count] = 0.0;}
	for(count = 0; count < 6001; count++) {aTL[count] = 0.0;}
	for(count = 0; count < 6001; count++) {oTL[count] = 0.0;}
	for(count = 0; count < 5837; count++) {aUL[count] = 0.0;}
	for(count = 0; count < 5837; count++) {oUL[count] = 0.0;}
	for(count = 0; count < 5501; count++) {aVL[count] = 0.0;}
	for(count = 0; count < 5501; count++) {oVL[count] = 0.0;}
	for(count = 0; count < 5009; count++) {aWL[count] = 0.0;}
	for(count = 0; count < 5009; count++) {oWL[count] = 0.0;}
	for(count = 0; count < 4849; count++) {aXL[count] = 0.0;}
	for(count = 0; count < 4849; count++) {oXL[count] = 0.0;}
	for(count = 0; count < 4295; count++) {aYL[count] = 0.0;}
	for(count = 0; count < 4295; count++) {oYL[count] = 0.0;}
	for(count = 0; count < 4179; count++) {aZL[count] = 0.0;}	
	for(count = 0; count < 4179; count++) {oZL[count] = 0.0;}
	
	for(count = 0; count < 6; count++) {dAR[count] = 0.0; dBR[count] = 0.0; dCR[count] = 0.0; dDR[count] = 0.0; dER[count] = 0.0;
		dFR[count] = 0.0; dGR[count] = 0.0; dHR[count] = 0.0; dIR[count] = 0.0; dJR[count] = 0.0; dKR[count] = 0.0; dLR[count] = 0.0;
		dMR[count] = 0.0; dNR[count] = 0.0; dOR[count] = 0.0; dPR[count] = 0.0; dQR[count] = 0.0; dRR[count] = 0.0; dSR[count] = 0.0;
		dTR[count] = 0.0; dUR[count] = 0.0; dVR[count] = 0.0; dWR[count] = 0.0; dXR[count] = 0.0; dYR[count] = 0.0; dZR[count] = 0.0;}
	
	for(count = 0; count < 15149; count++) {aAR[count] = 0.0;}
	for(count = 0; count < 15149; count++) {oAR[count] = 0.0;}
	for(count = 0; count < 14617; count++) {aBR[count] = 0.0;}
	for(count = 0; count < 14617; count++) {oBR[count] = 0.0;}
	for(count = 0; count < 14357; count++) {aCR[count] = 0.0;}
	for(count = 0; count < 14357; count++) {oCR[count] = 0.0;}
	for(count = 0; count < 13817; count++) {aDR[count] = 0.0;}
	for(count = 0; count < 13817; count++) {oDR[count] = 0.0;}
	for(count = 0; count < 13561; count++) {aER[count] = 0.0;}
	for(count = 0; count < 13561; count++) {oER[count] = 0.0;}
	for(count = 0; count < 13045; count++) {aFR[count] = 0.0;}
	for(count = 0; count < 13045; count++) {oFR[count] = 0.0;}
	for(count = 0; count < 11965; count++) {aGR[count] = 0.0;}
	for(count = 0; count < 11965; count++) {oGR[count] = 0.0;}
	for(count = 0; count < 11129; count++) {aHR[count] = 0.0;}
	for(count = 0; count < 11129; count++) {oHR[count] = 0.0;}
	for(count = 0; count < 10597; count++) {aIR[count] = 0.0;}
	for(count = 0; count < 10597; count++) {oIR[count] = 0.0;}
	for(count = 0; count < 9809; count++) {aJR[count] = 0.0;}
	for(count = 0; count < 9809; count++) {oJR[count] = 0.0;}
	for(count = 0; count < 9521; count++) {aKR[count] = 0.0;}
	for(count = 0; count < 9521; count++) {oKR[count] = 0.0;}
	for(count = 0; count < 8981; count++) {aLR[count] = 0.0;}
	for(count = 0; count < 8981; count++) {oLR[count] = 0.0;}
	for(count = 0; count < 8785; count++) {aMR[count] = 0.0;}
	for(count = 0; count < 8785; count++) {oMR[count] = 0.0;}
	for(count = 0; count < 8461; count++) {aNR[count] = 0.0;}
	for(count = 0; count < 8461; count++) {oNR[count] = 0.0;}
	for(count = 0; count < 8309; count++) {aOR[count] = 0.0;}
	for(count = 0; count < 8309; count++) {oOR[count] = 0.0;}
	for(count = 0; count < 7981; count++) {aPR[count] = 0.0;}
	for(count = 0; count < 7981; count++) {oPR[count] = 0.0;}
	for(count = 0; count < 7321; count++) {aQR[count] = 0.0;}
	for(count = 0; count < 7321; count++) {oQR[count] = 0.0;}
	for(count = 0; count < 6817; count++) {aRR[count] = 0.0;}
	for(count = 0; count < 6817; count++) {oRR[count] = 0.0;}
	for(count = 0; count < 6505; count++) {aSR[count] = 0.0;}
	for(count = 0; count < 6505; count++) {oSR[count] = 0.0;}
	for(count = 0; count < 6001; count++) {aTR[count] = 0.0;}
	for(count = 0; count < 6001; count++) {oTR[count] = 0.0;}
	for(count = 0; count < 5837; count++) {aUR[count] = 0.0;}
	for(count = 0; count < 5837; count++) {oUR[count] = 0.0;}
	for(count = 0; count < 5501; count++) {aVR[count] = 0.0;}
	for(count = 0; count < 5501; count++) {oVR[count] = 0.0;}
	for(count = 0; count < 5009; count++) {aWR[count] = 0.0;}
	for(count = 0; count < 5009; count++) {oWR[count] = 0.0;}
	for(count = 0; count < 4849; count++) {aXR[count] = 0.0;}
	for(count = 0; count < 4849; count++) {oXR[count] = 0.0;}
	for(count = 0; count < 4295; count++) {aYR[count] = 0.0;}
	for(count = 0; count < 4295; count++) {oYR[count] = 0.0;}
	for(count = 0; count < 4179; count++) {aZR[count] = 0.0;}	
	for(count = 0; count < 4179; count++) {oZR[count] = 0.0;}
	
	outAL = 1; alpAL = 1;
	outBL = 1; alpBL = 1;
	outCL = 1; alpCL = 1;
	outDL = 1; alpDL = 1;
	outEL = 1; alpEL = 1;
	outFL = 1; alpFL = 1;
	outGL = 1; alpGL = 1;
	outHL = 1; alpHL = 1;
	outIL = 1; alpIL = 1;
	outJL = 1; alpJL = 1;
	outKL = 1; alpKL = 1;
	outLL = 1; alpLL = 1;
	outML = 1; alpML = 1;
	outNL = 1; alpNL = 1;
	outOL = 1; alpOL = 1;
	outPL = 1; alpPL = 1;
	outQL = 1; alpQL = 1;
	outRL = 1; alpRL = 1;
	outSL = 1; alpSL = 1;
	outTL = 1; alpTL = 1;
	outUL = 1; alpUL = 1;
	outVL = 1; alpVL = 1;
	outWL = 1; alpWL = 1;
	outXL = 1; alpXL = 1;
	outYL = 1; alpYL = 1;
	outZL = 1; alpZL = 1;
	
	outAR = 1; alpAR = 1; delayA = 4; maxdelayA = 7573;
	outBR = 1; alpBR = 1; delayB = 4; maxdelayB = 7307;
	outCR = 1; alpCR = 1; delayC = 4; maxdelayC = 7177;
	outDR = 1; alpDR = 1; delayD = 4; maxdelayD = 6907;
	outER = 1; alpER = 1; delayE = 4; maxdelayE = 6779;
	outFR = 1; alpFR = 1; delayF = 4; maxdelayF = 6521;
	outGR = 1; alpGR = 1; delayG = 4; maxdelayG = 5981;
	outHR = 1; alpHR = 1; delayH = 4; maxdelayH = 5563;
	outIR = 1; alpIR = 1; delayI = 4; maxdelayI = 5297;
	outJR = 1; alpJR = 1; delayJ = 4; maxdelayJ = 4903;
	outKR = 1; alpKR = 1; delayK = 4; maxdelayK = 4759;
	outLR = 1; alpLR = 1; delayL = 4; maxdelayL = 4489;
	outMR = 1; alpMR = 1; delayM = 4; maxdelayM = 4391;
	outNR = 1; alpNR = 1; delayN = 4; maxdelayN = 4229;
	outOR = 1; alpOR = 1; delayO = 4; maxdelayO = 4153;
	outPR = 1; alpPR = 1; delayP = 4; maxdelayP = 3989;
	outQR = 1; alpQR = 1; delayQ = 4; maxdelayQ = 3659;
	outRR = 1; alpRR = 1; delayR = 4; maxdelayR = 3407;
	outSR = 1; alpSR = 1; delayS = 4; maxdelayS = 3251;
	outTR = 1; alpTR = 1; delayT = 4; maxdelayT = 2999;
	outUR = 1; alpUR = 1; delayU = 4; maxdelayU = 2917;
	outVR = 1; alpVR = 1; delayV = 4; maxdelayV = 2749;
	outWR = 1; alpWR = 1; delayW = 4; maxdelayW = 2503;
	outXR = 1; alpXR = 1; delayX = 4; maxdelayX = 2423;
	outYR = 1; alpYR = 1; delayY = 4; maxdelayY = 2146;
	outZR = 1; alpZR = 1; delayZ = 4; maxdelayZ = 2088;
	
	savedRoomsize = -1.0; //force update to begin
	countdown = -1;
	peakL = 1.0;
	peakR = 1.0;
	fpd = 17;
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

PocketVerbs::~PocketVerbs() {}
VstInt32 PocketVerbs::getVendorVersion () {return 1000;}
void PocketVerbs::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PocketVerbs::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 PocketVerbs::getChunk (void** data, bool isPreset)
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

VstInt32 PocketVerbs::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void PocketVerbs::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float PocketVerbs::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PocketVerbs::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Type", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Size", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Gate", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Mix", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void PocketVerbs::getParameterDisplay(VstInt32 index, char *text, float extVal, bool isExternal) {
    switch (index) {
        case kParamA: switch((VstInt32)(EXTV(A) * 5.999 )) //0 to almost edge of # of params
		{	case 0: vst_strncpy (text, "Chamber", kVstMaxParamStrLen); break;
			case 1: vst_strncpy (text, "Spring", kVstMaxParamStrLen); break;
			case 2: vst_strncpy (text, "Tiled", kVstMaxParamStrLen); break;
			case 3: vst_strncpy (text, "Room", kVstMaxParamStrLen); break;
			case 4: vst_strncpy (text, "Stretch", kVstMaxParamStrLen); break;
			case 5: vst_strncpy (text, "Zarathustra", kVstMaxParamStrLen); break;
			default: break; // unknown parameter, shouldn't happen!
		} break;			
        case kParamB: float2string (EXTV(B) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (EXTV(C) * 100.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (EXTV(D) * 100.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

bool PocketVerbs::isParameterIntegral(VstInt32 index)
{
   return index == kParamA;
}

int PocketVerbs::parameterIntegralUpperBound(VstInt32 index)
{
   return 5;
}

void PocketVerbs::getIntegralDisplayForValue(VstInt32 index, float value, char* text)
{
   switch((VstInt32)( value * 5.999 )) //0 to almost edge of # of params
   {
   case 0: vst_strncpy (text, "Chamber", kVstMaxParamStrLen); break;
   case 1: vst_strncpy (text, "Spring", kVstMaxParamStrLen); break;
   case 2: vst_strncpy (text, "Tiled", kVstMaxParamStrLen); break;
   case 3: vst_strncpy (text, "Room", kVstMaxParamStrLen); break;
   case 4: vst_strncpy (text, "Stretch", kVstMaxParamStrLen); break;
   case 5: vst_strncpy (text, "Zarathustra", kVstMaxParamStrLen); break;
   default: break; // unknown parameter, shouldn't happen!
   }
}

void PocketVerbs::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "%", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

bool PocketVerbs::parseParameterValueFromString(VstInt32 index, const char* str, float& f)
{
    auto v = std::atof(str);

    f = v / 100.0;

    return true;
}

VstInt32 PocketVerbs::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PocketVerbs::getEffectName(char* name) {
    vst_strncpy(name, "PocketVerbs", kVstMaxProductStrLen); return true;
}

VstPlugCategory PocketVerbs::getPlugCategory() {return kPlugCategEffect;}

bool PocketVerbs::getProductString(char* text) {
  	vst_strncpy (text, "airwindows PocketVerbs", kVstMaxProductStrLen); return true;
}

bool PocketVerbs::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}


} // end namespace PocketVerbs

