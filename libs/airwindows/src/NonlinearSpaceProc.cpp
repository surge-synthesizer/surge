/* ========================================
 *  NonlinearSpace - NonlinearSpace.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NonlinearSpace_H
#include "NonlinearSpace.h"
#endif

namespace NonlinearSpace
{

void NonlinearSpace::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float *in1 = inputs[0];
    float *in2 = inputs[1];
    float *out1 = outputs[0];
    float *out2 = outputs[1];

    double drySampleL;
    double drySampleR;
    double inputSampleL;
    double inputSampleR;
    double mid;
    double side;
    double overallscale = 1.0;
    int samplerate = (int)(A * 6.999) + 1;
    switch (samplerate)
    {
    case 1:
        overallscale *= (16.0 / 44.1);
        break; // 16
    case 2:
        overallscale *= (32.0 / 44.1);
        break; // 32
    case 3:
        overallscale *= 1.0;
        break; // 44.1
    case 4:
        overallscale *= (48.0 / 44.1);
        break; // 48
    case 5:
        overallscale *= (64.0 / 44.1);
        break; // 64
    case 6:
        overallscale *= 2.0;
        break; // 88.2
    case 7:
        overallscale *= (96.0 / 44.1);
        break; // 96
    }
    nonlin *= 0.001; // scale suitably to apply to our liveness value
    double basefeedback =
        0.45 + (nonlin * pow(((E * 2.0) - 1.0),
                             3)); // nonlin from previous sample, positive adds liveness when loud
    nonlin = 0.0;                 // reset it here for setting up again next time
    double tankfeedback = basefeedback + (pow(B, 2) * 0.05);
    // liveness
    if (tankfeedback > 0.5)
        tankfeedback = 0.5;
    if (tankfeedback < 0.4)
        tankfeedback = 0.4;
    double iirAmountC = 1.0 - pow(1.0 - C, 2);
    // most of the range is up at the top end
    iirAmountC += (iirAmountC / overallscale);
    iirAmountC /= 2.0;
    if (iirAmountC > 1.1)
        iirAmountC = 1.1;
    // lowpass, check to see if it's working reasonably at 96K
    double iirAmount = (((1.0 - pow(D, 2)) * 0.09) / overallscale) + 0.001;
    if (iirAmount > 1.0)
        iirAmount = 1.0;
    if (iirAmount < 0.001)
        iirAmount = 0.001;
    double wetness = F;
    double dryness = 1.0 - wetness;
    double roomsize = overallscale * 0.203;
    double lean = 0.125;
    double invlean = 1.0 - lean;
    double pspeed = 0.145;
    double outcouple = 0.5 - tankfeedback;
    double constallpass = 0.618033988749894848204586; // golden ratio!
    double temp;
    int allpasstemp;
    double predelay = 0.222 * overallscale;

    // reverb setup

    delayA = (int(maxdelayA * roomsize));
    delayB = (int(maxdelayB * roomsize));
    delayC = (int(maxdelayC * roomsize));
    delayD = (int(maxdelayD * roomsize));
    delayE = (int(maxdelayE * roomsize));
    delayF = (int(maxdelayF * roomsize));
    delayG = (int(maxdelayG * roomsize));
    delayH = (int(maxdelayH * roomsize));
    delayI = (int(maxdelayI * roomsize));
    delayJ = (int(maxdelayJ * roomsize));
    delayK = (int(maxdelayK * roomsize));
    delayL = (int(maxdelayL * roomsize));
    delayM = (int(maxdelayM * roomsize));
    delayN = (int(maxdelayN * roomsize));
    delayO = (int(maxdelayO * roomsize));
    delayP = (int(maxdelayP * roomsize));
    delayQ = (int(maxdelayQ * roomsize));
    delayR = (int(maxdelayR * roomsize));
    delayS = (int(maxdelayS * roomsize));
    delayT = (int(maxdelayT * roomsize));
    delayU = (int(maxdelayU * roomsize));
    delayV = (int(maxdelayV * roomsize));
    delayW = (int(maxdelayW * roomsize));
    delayX = (int(maxdelayX * roomsize));
    delayY = (int(maxdelayY * roomsize));
    delayZ = (int(maxdelayZ * roomsize));
    delayMid = (int(maxdelayMid * roomsize));
    delaySide = (int(maxdelaySide * roomsize));
    delayLeft = (int(maxdelayLeft * roomsize));
    delayRight = (int(maxdelayRight * roomsize));
    delaypre = (int(maxdelaypre * predelay));

    while (--sampleFrames >= 0)
    {
        inputSampleL = *in1;
        inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-23)
            inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR) < 1.18e-23)
            inputSampleR = fpdR * 1.18e-17;
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        dpreL[onepre] = inputSampleL;
        dpreR[onepre] = inputSampleR;
        onepre--;
        if (onepre < 0 || onepre > delaypre)
        {
            onepre = delaypre;
        }
        inputSampleL = (dpreL[onepre]);
        inputSampleR = (dpreR[onepre]);
        // predelay

        interpolA += pitchshiftA * pspeed;
        interpolB += pitchshiftB * pspeed;
        interpolC += pitchshiftC * pspeed;
        interpolD += pitchshiftD * pspeed;
        interpolE += pitchshiftE * pspeed;
        interpolF += pitchshiftF * pspeed;
        interpolG += pitchshiftG * pspeed;
        interpolH += pitchshiftH * pspeed;
        interpolI += pitchshiftI * pspeed;
        interpolJ += pitchshiftJ * pspeed;
        interpolK += pitchshiftK * pspeed;
        interpolL += pitchshiftL * pspeed;
        interpolM += pitchshiftM * pspeed;
        interpolN += pitchshiftN * pspeed;
        interpolO += pitchshiftO * pspeed;
        interpolP += pitchshiftP * pspeed;
        interpolQ += pitchshiftQ * pspeed;
        interpolR += pitchshiftR * pspeed;
        interpolS += pitchshiftS * pspeed;
        interpolT += pitchshiftT * pspeed;
        interpolU += pitchshiftU * pspeed;
        interpolV += pitchshiftV * pspeed;
        interpolW += pitchshiftW * pspeed;
        interpolX += pitchshiftX * pspeed;
        interpolY += pitchshiftY * pspeed;
        interpolZ += pitchshiftZ * pspeed;
        // increment all the sub-sample offsets for the pitch shifting of combs

        if (interpolA > 1.0)
        {
            pitchshiftA = -fabs(pitchshiftA);
            interpolA += pitchshiftA * pspeed;
        }
        if (interpolB > 1.0)
        {
            pitchshiftB = -fabs(pitchshiftB);
            interpolB += pitchshiftB * pspeed;
        }
        if (interpolC > 1.0)
        {
            pitchshiftC = -fabs(pitchshiftC);
            interpolC += pitchshiftC * pspeed;
        }
        if (interpolD > 1.0)
        {
            pitchshiftD = -fabs(pitchshiftD);
            interpolD += pitchshiftD * pspeed;
        }
        if (interpolE > 1.0)
        {
            pitchshiftE = -fabs(pitchshiftE);
            interpolE += pitchshiftE * pspeed;
        }
        if (interpolF > 1.0)
        {
            pitchshiftF = -fabs(pitchshiftF);
            interpolF += pitchshiftF * pspeed;
        }
        if (interpolG > 1.0)
        {
            pitchshiftG = -fabs(pitchshiftG);
            interpolG += pitchshiftG * pspeed;
        }
        if (interpolH > 1.0)
        {
            pitchshiftH = -fabs(pitchshiftH);
            interpolH += pitchshiftH * pspeed;
        }
        if (interpolI > 1.0)
        {
            pitchshiftI = -fabs(pitchshiftI);
            interpolI += pitchshiftI * pspeed;
        }
        if (interpolJ > 1.0)
        {
            pitchshiftJ = -fabs(pitchshiftJ);
            interpolJ += pitchshiftJ * pspeed;
        }
        if (interpolK > 1.0)
        {
            pitchshiftK = -fabs(pitchshiftK);
            interpolK += pitchshiftK * pspeed;
        }
        if (interpolL > 1.0)
        {
            pitchshiftL = -fabs(pitchshiftL);
            interpolL += pitchshiftL * pspeed;
        }
        if (interpolM > 1.0)
        {
            pitchshiftM = -fabs(pitchshiftM);
            interpolM += pitchshiftM * pspeed;
        }
        if (interpolN > 1.0)
        {
            pitchshiftN = -fabs(pitchshiftN);
            interpolN += pitchshiftN * pspeed;
        }
        if (interpolO > 1.0)
        {
            pitchshiftO = -fabs(pitchshiftO);
            interpolO += pitchshiftO * pspeed;
        }
        if (interpolP > 1.0)
        {
            pitchshiftP = -fabs(pitchshiftP);
            interpolP += pitchshiftP * pspeed;
        }
        if (interpolQ > 1.0)
        {
            pitchshiftQ = -fabs(pitchshiftQ);
            interpolQ += pitchshiftQ * pspeed;
        }
        if (interpolR > 1.0)
        {
            pitchshiftR = -fabs(pitchshiftR);
            interpolR += pitchshiftR * pspeed;
        }
        if (interpolS > 1.0)
        {
            pitchshiftS = -fabs(pitchshiftS);
            interpolS += pitchshiftS * pspeed;
        }
        if (interpolT > 1.0)
        {
            pitchshiftT = -fabs(pitchshiftT);
            interpolT += pitchshiftT * pspeed;
        }
        if (interpolU > 1.0)
        {
            pitchshiftU = -fabs(pitchshiftU);
            interpolU += pitchshiftU * pspeed;
        }
        if (interpolV > 1.0)
        {
            pitchshiftV = -fabs(pitchshiftV);
            interpolV += pitchshiftV * pspeed;
        }
        if (interpolW > 1.0)
        {
            pitchshiftW = -fabs(pitchshiftW);
            interpolW += pitchshiftW * pspeed;
        }
        if (interpolX > 1.0)
        {
            pitchshiftX = -fabs(pitchshiftX);
            interpolX += pitchshiftX * pspeed;
        }
        if (interpolY > 1.0)
        {
            pitchshiftY = -fabs(pitchshiftY);
            interpolY += pitchshiftY * pspeed;
        }
        if (interpolZ > 1.0)
        {
            pitchshiftZ = -fabs(pitchshiftZ);
            interpolZ += pitchshiftZ * pspeed;
        }

        if (interpolA < 0.0)
        {
            pitchshiftA = fabs(pitchshiftA);
            interpolA += pitchshiftA * pspeed;
        }
        if (interpolB < 0.0)
        {
            pitchshiftB = fabs(pitchshiftB);
            interpolB += pitchshiftB * pspeed;
        }
        if (interpolC < 0.0)
        {
            pitchshiftC = fabs(pitchshiftC);
            interpolC += pitchshiftC * pspeed;
        }
        if (interpolD < 0.0)
        {
            pitchshiftD = fabs(pitchshiftD);
            interpolD += pitchshiftD * pspeed;
        }
        if (interpolE < 0.0)
        {
            pitchshiftE = fabs(pitchshiftE);
            interpolE += pitchshiftE * pspeed;
        }
        if (interpolF < 0.0)
        {
            pitchshiftF = fabs(pitchshiftF);
            interpolF += pitchshiftF * pspeed;
        }
        if (interpolG < 0.0)
        {
            pitchshiftG = fabs(pitchshiftG);
            interpolG += pitchshiftG * pspeed;
        }
        if (interpolH < 0.0)
        {
            pitchshiftH = fabs(pitchshiftH);
            interpolH += pitchshiftH * pspeed;
        }
        if (interpolI < 0.0)
        {
            pitchshiftI = fabs(pitchshiftI);
            interpolI += pitchshiftI * pspeed;
        }
        if (interpolJ < 0.0)
        {
            pitchshiftJ = fabs(pitchshiftJ);
            interpolJ += pitchshiftJ * pspeed;
        }
        if (interpolK < 0.0)
        {
            pitchshiftK = fabs(pitchshiftK);
            interpolK += pitchshiftK * pspeed;
        }
        if (interpolL < 0.0)
        {
            pitchshiftL = fabs(pitchshiftL);
            interpolL += pitchshiftL * pspeed;
        }
        if (interpolM < 0.0)
        {
            pitchshiftM = fabs(pitchshiftM);
            interpolM += pitchshiftM * pspeed;
        }
        if (interpolN < 0.0)
        {
            pitchshiftN = fabs(pitchshiftN);
            interpolN += pitchshiftN * pspeed;
        }
        if (interpolO < 0.0)
        {
            pitchshiftO = fabs(pitchshiftO);
            interpolO += pitchshiftO * pspeed;
        }
        if (interpolP < 0.0)
        {
            pitchshiftP = fabs(pitchshiftP);
            interpolP += pitchshiftP * pspeed;
        }
        if (interpolQ < 0.0)
        {
            pitchshiftQ = fabs(pitchshiftQ);
            interpolQ += pitchshiftQ * pspeed;
        }
        if (interpolR < 0.0)
        {
            pitchshiftR = fabs(pitchshiftR);
            interpolR += pitchshiftR * pspeed;
        }
        if (interpolS < 0.0)
        {
            pitchshiftS = fabs(pitchshiftS);
            interpolS += pitchshiftS * pspeed;
        }
        if (interpolT < 0.0)
        {
            pitchshiftT = fabs(pitchshiftT);
            interpolT += pitchshiftT * pspeed;
        }
        if (interpolU < 0.0)
        {
            pitchshiftU = fabs(pitchshiftU);
            interpolU += pitchshiftU * pspeed;
        }
        if (interpolV < 0.0)
        {
            pitchshiftV = fabs(pitchshiftV);
            interpolV += pitchshiftV * pspeed;
        }
        if (interpolW < 0.0)
        {
            pitchshiftW = fabs(pitchshiftW);
            interpolW += pitchshiftW * pspeed;
        }
        if (interpolX < 0.0)
        {
            pitchshiftX = fabs(pitchshiftX);
            interpolX += pitchshiftX * pspeed;
        }
        if (interpolY < 0.0)
        {
            pitchshiftY = fabs(pitchshiftY);
            interpolY += pitchshiftY * pspeed;
        }
        if (interpolZ < 0.0)
        {
            pitchshiftZ = fabs(pitchshiftZ);
            interpolZ += pitchshiftZ * pspeed;
        }
        // all of the sanity checks for interpol for all combs

        if (verboutR > 1.0)
            verboutR = 1.0;
        if (verboutR < -1.0)
            verboutR = -1.0;
        if (verboutL > 1.0)
            verboutL = 1.0;
        if (verboutL < -1.0)
            verboutL = -1.0;

        inputSampleL += verboutR;
        inputSampleR += verboutL;
        verboutL = 0.0;
        verboutR = 0.0;
        // here we add in the cross-coupling- output of L tank to R, output of R tank to L

        mid = inputSampleL + inputSampleR;
        side = inputSampleL - inputSampleR;
        // assign mid and side.

        allpasstemp = oneMid - 1;
        if (allpasstemp < 0 || allpasstemp > delayMid)
        {
            allpasstemp = delayMid;
        }
        mid -= dMid[allpasstemp] * constallpass;
        dMid[oneMid] = mid;
        mid *= constallpass;
        oneMid--;
        if (oneMid < 0 || oneMid > delayMid)
        {
            oneMid = delayMid;
        }
        mid += (dMid[oneMid]);
        nonlin += fabs(dMid[oneMid]);
        // allpass filter mid

        allpasstemp = oneSide - 1;
        if (allpasstemp < 0 || allpasstemp > delaySide)
        {
            allpasstemp = delaySide;
        }
        side -= dSide[allpasstemp] * constallpass;
        dSide[oneSide] = side;
        side *= constallpass;
        oneSide--;
        if (oneSide < 0 || oneSide > delaySide)
        {
            oneSide = delaySide;
        }
        side += (dSide[oneSide]);
        nonlin += fabs(dSide[oneSide]);
        // allpass filter side

        // here we do allpasses on the mid and side

        allpasstemp = oneLeft - 1;
        if (allpasstemp < 0 || allpasstemp > delayLeft)
        {
            allpasstemp = delayLeft;
        }
        inputSampleL -= dLeft[allpasstemp] * constallpass;
        dLeft[oneLeft] = verboutL;
        inputSampleL *= constallpass;
        oneLeft--;
        if (oneLeft < 0 || oneLeft > delayLeft)
        {
            oneLeft = delayLeft;
        }
        inputSampleL += (dLeft[oneLeft]);
        nonlin += fabs(dLeft[oneLeft]);
        // allpass filter left

        allpasstemp = oneRight - 1;
        if (allpasstemp < 0 || allpasstemp > delayRight)
        {
            allpasstemp = delayRight;
        }
        inputSampleR -= dRight[allpasstemp] * constallpass;
        dRight[oneRight] = verboutR;
        inputSampleR *= constallpass;
        oneRight--;
        if (oneRight < 0 || oneRight > delayRight)
        {
            oneRight = delayRight;
        }
        inputSampleR += (dRight[oneRight]);
        nonlin += fabs(dRight[oneRight]);
        // allpass filter right

        inputSampleL += (mid + side) / 2.0;
        inputSampleR += (mid - side) / 2.0;
        // here we get back to a L/R topology by adding the mid/side in parallel with L/R

        temp = (dA[oneA] * interpolA);
        temp += (dA[treA] * (1.0 - interpolA));
        temp += ((dA[twoA]));
        dA[treA] = (temp * tankfeedback);
        dA[treA] += inputSampleL;
        oneA--;
        if (oneA < 0 || oneA > delayA)
        {
            oneA = delayA;
        }
        twoA--;
        if (twoA < 0 || twoA > delayA)
        {
            twoA = delayA;
        }
        treA--;
        if (treA < 0 || treA > delayA)
        {
            treA = delayA;
        }
        temp = (dA[oneA] * interpolA);
        temp += (dA[treA] * (1.0 - interpolA));
        temp *= (invlean + (lean * fabs(dA[twoA])));
        verboutL += temp;
        // comb filter A
        temp = (dC[oneC] * interpolC);
        temp += (dC[treC] * (1.0 - interpolC));
        temp += ((dC[twoC]));
        dC[treC] = (temp * tankfeedback);
        dC[treC] += inputSampleL;
        oneC--;
        if (oneC < 0 || oneC > delayC)
        {
            oneC = delayC;
        }
        twoC--;
        if (twoC < 0 || twoC > delayC)
        {
            twoC = delayC;
        }
        treC--;
        if (treC < 0 || treC > delayC)
        {
            treC = delayC;
        }
        temp = (dC[oneC] * interpolC);
        temp += (dC[treC] * (1.0 - interpolC));
        temp *= (invlean + (lean * fabs(dC[twoC])));
        verboutL += temp;
        // comb filter C
        temp = (dE[oneE] * interpolE);
        temp += (dE[treE] * (1.0 - interpolE));
        temp += ((dE[twoE]));
        dE[treE] = (temp * tankfeedback);
        dE[treE] += inputSampleL;
        oneE--;
        if (oneE < 0 || oneE > delayE)
        {
            oneE = delayE;
        }
        twoE--;
        if (twoE < 0 || twoE > delayE)
        {
            twoE = delayE;
        }
        treE--;
        if (treE < 0 || treE > delayE)
        {
            treE = delayE;
        }
        temp = (dE[oneE] * interpolE);
        temp += (dE[treE] * (1.0 - interpolE));
        temp *= (invlean + (lean * fabs(dE[twoE])));
        verboutL += temp;
        // comb filter E
        temp = (dG[oneG] * interpolG);
        temp += (dG[treG] * (1.0 - interpolG));
        temp += ((dG[twoG]));
        dG[treG] = (temp * tankfeedback);
        dG[treG] += inputSampleL;
        oneG--;
        if (oneG < 0 || oneG > delayG)
        {
            oneG = delayG;
        }
        twoG--;
        if (twoG < 0 || twoG > delayG)
        {
            twoG = delayG;
        }
        treG--;
        if (treG < 0 || treG > delayG)
        {
            treG = delayG;
        }
        temp = (dG[oneG] * interpolG);
        temp += (dG[treG] * (1.0 - interpolG));
        temp *= (invlean + (lean * fabs(dG[twoG])));
        verboutL += temp;
        // comb filter G
        temp = (dI[oneI] * interpolI);
        temp += (dI[treI] * (1.0 - interpolI));
        temp += ((dI[twoI]));
        dI[treI] = (temp * tankfeedback);
        dI[treI] += inputSampleL;
        oneI--;
        if (oneI < 0 || oneI > delayI)
        {
            oneI = delayI;
        }
        twoI--;
        if (twoI < 0 || twoI > delayI)
        {
            twoI = delayI;
        }
        treI--;
        if (treI < 0 || treI > delayI)
        {
            treI = delayI;
        }
        temp = (dI[oneI] * interpolI);
        temp += (dI[treI] * (1.0 - interpolI));
        temp *= (invlean + (lean * fabs(dI[twoI])));
        verboutL += temp;
        // comb filter I
        temp = (dK[oneK] * interpolK);
        temp += (dK[treK] * (1.0 - interpolK));
        temp += ((dK[twoK]));
        dK[treK] = (temp * tankfeedback);
        dK[treK] += inputSampleL;
        oneK--;
        if (oneK < 0 || oneK > delayK)
        {
            oneK = delayK;
        }
        twoK--;
        if (twoK < 0 || twoK > delayK)
        {
            twoK = delayK;
        }
        treK--;
        if (treK < 0 || treK > delayK)
        {
            treK = delayK;
        }
        temp = (dK[oneK] * interpolK);
        temp += (dK[treK] * (1.0 - interpolK));
        temp *= (invlean + (lean * fabs(dK[twoK])));
        verboutL += temp;
        // comb filter K
        temp = (dM[oneM] * interpolM);
        temp += (dM[treM] * (1.0 - interpolM));
        temp += ((dM[twoM]));
        dM[treM] = (temp * tankfeedback);
        dM[treM] += inputSampleL;
        oneM--;
        if (oneM < 0 || oneM > delayM)
        {
            oneM = delayM;
        }
        twoM--;
        if (twoM < 0 || twoM > delayM)
        {
            twoM = delayM;
        }
        treM--;
        if (treM < 0 || treM > delayM)
        {
            treM = delayM;
        }
        temp = (dM[oneM] * interpolM);
        temp += (dM[treM] * (1.0 - interpolM));
        temp *= (invlean + (lean * fabs(dM[twoM])));
        verboutL += temp;
        // comb filter M
        temp = (dO[oneO] * interpolO);
        temp += (dO[treO] * (1.0 - interpolO));
        temp += ((dO[twoO]));
        dO[treO] = (temp * tankfeedback);
        dO[treO] += inputSampleL;
        oneO--;
        if (oneO < 0 || oneO > delayO)
        {
            oneO = delayO;
        }
        twoO--;
        if (twoO < 0 || twoO > delayO)
        {
            twoO = delayO;
        }
        treO--;
        if (treO < 0 || treO > delayO)
        {
            treO = delayO;
        }
        temp = (dO[oneO] * interpolO);
        temp += (dO[treO] * (1.0 - interpolO));
        temp *= (invlean + (lean * fabs(dO[twoO])));
        verboutL += temp;
        // comb filter O
        temp = (dQ[oneQ] * interpolQ);
        temp += (dQ[treQ] * (1.0 - interpolQ));
        temp += ((dQ[twoQ]));
        dQ[treQ] = (temp * tankfeedback);
        dQ[treQ] += inputSampleL;
        oneQ--;
        if (oneQ < 0 || oneQ > delayQ)
        {
            oneQ = delayQ;
        }
        twoQ--;
        if (twoQ < 0 || twoQ > delayQ)
        {
            twoQ = delayQ;
        }
        treQ--;
        if (treQ < 0 || treQ > delayQ)
        {
            treQ = delayQ;
        }
        temp = (dQ[oneQ] * interpolQ);
        temp += (dQ[treQ] * (1.0 - interpolQ));
        temp *= (invlean + (lean * fabs(dQ[twoQ])));
        verboutL += temp;
        // comb filter Q
        temp = (dS[oneS] * interpolS);
        temp += (dS[treS] * (1.0 - interpolS));
        temp += ((dS[twoS]));
        dS[treS] = (temp * tankfeedback);
        dS[treS] += inputSampleL;
        oneS--;
        if (oneS < 0 || oneS > delayS)
        {
            oneS = delayS;
        }
        twoS--;
        if (twoS < 0 || twoS > delayS)
        {
            twoS = delayS;
        }
        treS--;
        if (treS < 0 || treS > delayS)
        {
            treS = delayS;
        }
        temp = (dS[oneS] * interpolS);
        temp += (dS[treS] * (1.0 - interpolS));
        temp *= (invlean + (lean * fabs(dS[twoS])));
        verboutL += temp;
        // comb filter S
        temp = (dU[oneU] * interpolU);
        temp += (dU[treU] * (1.0 - interpolU));
        temp += ((dU[twoU]));
        dU[treU] = (temp * tankfeedback);
        dU[treU] += inputSampleL;
        oneU--;
        if (oneU < 0 || oneU > delayU)
        {
            oneU = delayU;
        }
        twoU--;
        if (twoU < 0 || twoU > delayU)
        {
            twoU = delayU;
        }
        treU--;
        if (treU < 0 || treU > delayU)
        {
            treU = delayU;
        }
        temp = (dU[oneU] * interpolU);
        temp += (dU[treU] * (1.0 - interpolU));
        temp *= (invlean + (lean * fabs(dU[twoU])));
        verboutL += temp;
        // comb filter U
        temp = (dW[oneW] * interpolW);
        temp += (dW[treW] * (1.0 - interpolW));
        temp += ((dW[twoW]));
        dW[treW] = (temp * tankfeedback);
        dW[treW] += inputSampleL;
        oneW--;
        if (oneW < 0 || oneW > delayW)
        {
            oneW = delayW;
        }
        twoW--;
        if (twoW < 0 || twoW > delayW)
        {
            twoW = delayW;
        }
        treW--;
        if (treW < 0 || treW > delayW)
        {
            treW = delayW;
        }
        temp = (dW[oneW] * interpolW);
        temp += (dW[treW] * (1.0 - interpolW));
        temp *= (invlean + (lean * fabs(dW[twoW])));
        verboutL += temp;
        // comb filter W
        temp = (dY[oneY] * interpolY);
        temp += (dY[treY] * (1.0 - interpolY));
        temp += ((dY[twoY]));
        dY[treY] = (temp * tankfeedback);
        dY[treY] += inputSampleL;
        oneY--;
        if (oneY < 0 || oneY > delayY)
        {
            oneY = delayY;
        }
        twoY--;
        if (twoY < 0 || twoY > delayY)
        {
            twoY = delayY;
        }
        treY--;
        if (treY < 0 || treY > delayY)
        {
            treY = delayY;
        }
        temp = (dY[oneY] * interpolY);
        temp += (dY[treY] * (1.0 - interpolY));
        temp *= (invlean + (lean * fabs(dY[twoY])));
        verboutL += temp;
        // comb filter Y
        // here we do the L delay tank, every other letter A C E G I

        temp = (dB[oneB] * interpolB);
        temp += (dB[treB] * (1.0 - interpolB));
        temp += ((dB[twoB]));
        dB[treB] = (temp * tankfeedback);
        dB[treB] += inputSampleR;
        oneB--;
        if (oneB < 0 || oneB > delayB)
        {
            oneB = delayB;
        }
        twoB--;
        if (twoB < 0 || twoB > delayB)
        {
            twoB = delayB;
        }
        treB--;
        if (treB < 0 || treB > delayB)
        {
            treB = delayB;
        }
        temp = (dB[oneB] * interpolB);
        temp += (dB[treB] * (1.0 - interpolB));
        temp *= (invlean + (lean * fabs(dB[twoB])));
        verboutR += temp;
        // comb filter B
        temp = (dD[oneD] * interpolD);
        temp += (dD[treD] * (1.0 - interpolD));
        temp += ((dD[twoD]));
        dD[treD] = (temp * tankfeedback);
        dD[treD] += inputSampleR;
        oneD--;
        if (oneD < 0 || oneD > delayD)
        {
            oneD = delayD;
        }
        twoD--;
        if (twoD < 0 || twoD > delayD)
        {
            twoD = delayD;
        }
        treD--;
        if (treD < 0 || treD > delayD)
        {
            treD = delayD;
        }
        temp = (dD[oneD] * interpolD);
        temp += (dD[treD] * (1.0 - interpolD));
        temp *= (invlean + (lean * fabs(dD[twoD])));
        verboutR += temp;
        // comb filter D
        temp = (dF[oneF] * interpolF);
        temp += (dF[treF] * (1.0 - interpolF));
        temp += ((dF[twoF]));
        dF[treF] = (temp * tankfeedback);
        dF[treF] += inputSampleR;
        oneF--;
        if (oneF < 0 || oneF > delayF)
        {
            oneF = delayF;
        }
        twoF--;
        if (twoF < 0 || twoF > delayF)
        {
            twoF = delayF;
        }
        treF--;
        if (treF < 0 || treF > delayF)
        {
            treF = delayF;
        }
        temp = (dF[oneF] * interpolF);
        temp += (dF[treF] * (1.0 - interpolF));
        temp *= (invlean + (lean * fabs(dF[twoF])));
        verboutR += temp;
        // comb filter F
        temp = (dH[oneH] * interpolH);
        temp += (dH[treH] * (1.0 - interpolH));
        temp += ((dH[twoH]));
        dH[treH] = (temp * tankfeedback);
        dH[treH] += inputSampleR;
        oneH--;
        if (oneH < 0 || oneH > delayH)
        {
            oneH = delayH;
        }
        twoH--;
        if (twoH < 0 || twoH > delayH)
        {
            twoH = delayH;
        }
        treH--;
        if (treH < 0 || treH > delayH)
        {
            treH = delayH;
        }
        temp = (dH[oneH] * interpolH);
        temp += (dH[treH] * (1.0 - interpolH));
        temp *= (invlean + (lean * fabs(dH[twoH])));
        verboutR += temp;
        // comb filter H
        temp = (dJ[oneJ] * interpolJ);
        temp += (dJ[treJ] * (1.0 - interpolJ));
        temp += ((dJ[twoJ]));
        dJ[treJ] = (temp * tankfeedback);
        dJ[treJ] += inputSampleR;
        oneJ--;
        if (oneJ < 0 || oneJ > delayJ)
        {
            oneJ = delayJ;
        }
        twoJ--;
        if (twoJ < 0 || twoJ > delayJ)
        {
            twoJ = delayJ;
        }
        treJ--;
        if (treJ < 0 || treJ > delayJ)
        {
            treJ = delayJ;
        }
        temp = (dJ[oneJ] * interpolJ);
        temp += (dJ[treJ] * (1.0 - interpolJ));
        temp *= (invlean + (lean * fabs(dJ[twoJ])));
        verboutR += temp;
        // comb filter J
        temp = (dL[oneL] * interpolL);
        temp += (dL[treL] * (1.0 - interpolL));
        temp += ((dL[twoL]));
        dL[treL] = (temp * tankfeedback);
        dL[treL] += inputSampleR;
        oneL--;
        if (oneL < 0 || oneL > delayL)
        {
            oneL = delayL;
        }
        twoL--;
        if (twoL < 0 || twoL > delayL)
        {
            twoL = delayL;
        }
        treL--;
        if (treL < 0 || treL > delayL)
        {
            treL = delayL;
        }
        temp = (dL[oneL] * interpolL);
        temp += (dL[treL] * (1.0 - interpolL));
        temp *= (invlean + (lean * fabs(dL[twoL])));
        verboutR += temp;
        // comb filter L
        temp = (dN[oneN] * interpolN);
        temp += (dN[treN] * (1.0 - interpolN));
        temp += ((dN[twoN]));
        dN[treN] = (temp * tankfeedback);
        dN[treN] += inputSampleR;
        oneN--;
        if (oneN < 0 || oneN > delayN)
        {
            oneN = delayN;
        }
        twoN--;
        if (twoN < 0 || twoN > delayN)
        {
            twoN = delayN;
        }
        treN--;
        if (treN < 0 || treN > delayN)
        {
            treN = delayN;
        }
        temp = (dN[oneN] * interpolN);
        temp += (dN[treN] * (1.0 - interpolN));
        temp *= (invlean + (lean * fabs(dN[twoN])));
        verboutR += temp;
        // comb filter N
        temp = (dP[oneP] * interpolP);
        temp += (dP[treP] * (1.0 - interpolP));
        temp += ((dP[twoP]));
        dP[treP] = (temp * tankfeedback);
        dP[treP] += inputSampleR;
        oneP--;
        if (oneP < 0 || oneP > delayP)
        {
            oneP = delayP;
        }
        twoP--;
        if (twoP < 0 || twoP > delayP)
        {
            twoP = delayP;
        }
        treP--;
        if (treP < 0 || treP > delayP)
        {
            treP = delayP;
        }
        temp = (dP[oneP] * interpolP);
        temp += (dP[treP] * (1.0 - interpolP));
        temp *= (invlean + (lean * fabs(dP[twoP])));
        verboutR += temp;
        // comb filter P
        temp = (dR[oneR] * interpolR);
        temp += (dR[treR] * (1.0 - interpolR));
        temp += ((dR[twoR]));
        dR[treR] = (temp * tankfeedback);
        dR[treR] += inputSampleR;
        oneR--;
        if (oneR < 0 || oneR > delayR)
        {
            oneR = delayR;
        }
        twoR--;
        if (twoR < 0 || twoR > delayR)
        {
            twoR = delayR;
        }
        treR--;
        if (treR < 0 || treR > delayR)
        {
            treR = delayR;
        }
        temp = (dR[oneR] * interpolR);
        temp += (dR[treR] * (1.0 - interpolR));
        temp *= (invlean + (lean * fabs(dR[twoR])));
        verboutR += temp;
        // comb filter R
        temp = (dT[oneT] * interpolT);
        temp += (dT[treT] * (1.0 - interpolT));
        temp += ((dT[twoT]));
        dT[treT] = (temp * tankfeedback);
        dT[treT] += inputSampleR;
        oneT--;
        if (oneT < 0 || oneT > delayT)
        {
            oneT = delayT;
        }
        twoT--;
        if (twoT < 0 || twoT > delayT)
        {
            twoT = delayT;
        }
        treT--;
        if (treT < 0 || treT > delayT)
        {
            treT = delayT;
        }
        temp = (dT[oneT] * interpolT);
        temp += (dT[treT] * (1.0 - interpolT));
        temp *= (invlean + (lean * fabs(dT[twoT])));
        verboutR += temp;
        // comb filter T
        temp = (dV[oneV] * interpolV);
        temp += (dV[treV] * (1.0 - interpolV));
        temp += ((dV[twoV]));
        dV[treV] = (temp * tankfeedback);
        dV[treV] += inputSampleR;
        oneV--;
        if (oneV < 0 || oneV > delayV)
        {
            oneV = delayV;
        }
        twoV--;
        if (twoV < 0 || twoV > delayV)
        {
            twoV = delayV;
        }
        treV--;
        if (treV < 0 || treV > delayV)
        {
            treV = delayV;
        }
        temp = (dV[oneV] * interpolV);
        temp += (dV[treV] * (1.0 - interpolV));
        temp *= (invlean + (lean * fabs(dV[twoV])));
        verboutR += temp;
        // comb filter V
        temp = (dX[oneX] * interpolX);
        temp += (dX[treX] * (1.0 - interpolX));
        temp += ((dX[twoX]));
        dX[treX] = (temp * tankfeedback);
        dX[treX] += inputSampleR;
        oneX--;
        if (oneX < 0 || oneX > delayX)
        {
            oneX = delayX;
        }
        twoX--;
        if (twoX < 0 || twoX > delayX)
        {
            twoX = delayX;
        }
        treX--;
        if (treX < 0 || treX > delayX)
        {
            treX = delayX;
        }
        temp = (dX[oneX] * interpolX);
        temp += (dX[treX] * (1.0 - interpolX));
        temp *= (invlean + (lean * fabs(dX[twoX])));
        verboutR += temp;
        // comb filter X
        temp = (dZ[oneZ] * interpolZ);
        temp += (dZ[treZ] * (1.0 - interpolZ));
        temp += ((dZ[twoZ]));
        dZ[treZ] = (temp * tankfeedback);
        dZ[treZ] += inputSampleR;
        oneZ--;
        if (oneZ < 0 || oneZ > delayZ)
        {
            oneZ = delayZ;
        }
        twoZ--;
        if (twoZ < 0 || twoZ > delayZ)
        {
            twoZ = delayZ;
        }
        treZ--;
        if (treZ < 0 || treZ > delayZ)
        {
            treZ = delayZ;
        }
        temp = (dZ[oneZ] * interpolZ);
        temp += (dZ[treZ] * (1.0 - interpolZ));
        temp *= (invlean + (lean * fabs(dZ[twoZ])));
        verboutR += temp;
        // comb filter Z
        // here we do the R delay tank, every other letter B D F H J

        verboutL /= 8;
        verboutR /= 8;

        iirSampleL = (iirSampleL * (1 - iirAmount)) + (verboutL * iirAmount);
        verboutL = verboutL - iirSampleL;

        iirSampleR = (iirSampleR * (1 - iirAmount)) + (verboutR * iirAmount);
        verboutR = verboutR - iirSampleR;
        // we need to highpass the crosscoupling, it's making DC runaway

        verboutL *= (invlean + (lean * fabs(verboutL)));
        verboutR *= (invlean + (lean * fabs(verboutR)));
        // scale back the verb tank the same way we scaled the combs

        inputSampleL = verboutL;
        inputSampleR = verboutR;

        // EQ lowpass is after all processing like the compressor that might produce hash
        if (flip)
        {
            lowpassSampleAA = (lowpassSampleAA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleAA;
            lowpassSampleBA = (lowpassSampleBA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleBA;
            lowpassSampleCA = (lowpassSampleCA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleCA;
            lowpassSampleDA = (lowpassSampleDA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleDA;
            lowpassSampleE = (lowpassSampleE * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleE;
        }
        else
        {
            lowpassSampleAB = (lowpassSampleAB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleAB;
            lowpassSampleBB = (lowpassSampleBB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleBB;
            lowpassSampleCB = (lowpassSampleCB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleCB;
            lowpassSampleDB = (lowpassSampleDB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleDB;
            lowpassSampleF = (lowpassSampleF * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleF;
        }
        lowpassSampleG = (lowpassSampleG * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
        inputSampleL = (lowpassSampleG * (1 - iirAmountC)) + (inputSampleL * iirAmountC);

        if (flip)
        {
            rowpassSampleAA = (rowpassSampleAA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleAA;
            rowpassSampleBA = (rowpassSampleBA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleBA;
            rowpassSampleCA = (rowpassSampleCA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleCA;
            rowpassSampleDA = (rowpassSampleDA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleDA;
            rowpassSampleE = (rowpassSampleE * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleE;
        }
        else
        {
            rowpassSampleAB = (rowpassSampleAB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleAB;
            rowpassSampleBB = (rowpassSampleBB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleBB;
            rowpassSampleCB = (rowpassSampleCB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleCB;
            rowpassSampleDB = (rowpassSampleDB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleDB;
            rowpassSampleF = (rowpassSampleF * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleF;
        }
        rowpassSampleG = (rowpassSampleG * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
        inputSampleR = (rowpassSampleG * (1 - iirAmountC)) + (inputSampleR * iirAmountC);

        iirCCSampleL = (iirCCSampleL * (1 - iirAmount)) + (verboutL * iirAmount);
        verboutL = verboutL - iirCCSampleL;

        iirCCSampleR = (iirCCSampleR * (1 - iirAmount)) + (verboutR * iirAmount);
        verboutR = verboutR - iirCCSampleR;
        // we need to highpass the crosscoupling, it's making DC runaway

        verboutL *= (invlean + (lean * fabs(verboutL)));
        verboutR *= (invlean + (lean * fabs(verboutR)));
        // scale back the crosscouple the same way we scaled the combs
        verboutL = (inputSampleL)*outcouple;
        verboutR = (inputSampleR)*outcouple;
        // send it off to the input again

        nonlin += fabs(verboutL);
        nonlin += fabs(verboutR); // post highpassing and a lot of processing

        drySampleL *= dryness;
        drySampleR *= dryness;

        inputSampleL *= wetness;
        inputSampleR *= wetness;

        inputSampleL += drySampleL;
        inputSampleR += drySampleR;
        // here we combine the tanks with the dry signal

        // begin 32 bit stereo floating point dither
        int expon;
        frexpf((float)inputSampleL, &expon);
        fpdL ^= fpdL << 13;
        fpdL ^= fpdL >> 17;
        fpdL ^= fpdL << 5;
        inputSampleL += ((double(fpdL) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        frexpf((float)inputSampleR, &expon);
        fpdR ^= fpdR << 13;
        fpdR ^= fpdR >> 17;
        fpdR ^= fpdR << 5;
        inputSampleR += ((double(fpdR) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
        // end 32 bit stereo floating point dither
        flip = !flip;

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

void NonlinearSpace::processDoubleReplacing(double **inputs, double **outputs,
                                            VstInt32 sampleFrames)
{
    double *in1 = inputs[0];
    double *in2 = inputs[1];
    double *out1 = outputs[0];
    double *out2 = outputs[1];

    double drySampleL;
    double drySampleR;
    double inputSampleL;
    double inputSampleR;
    double mid;
    double side;
    double overallscale = 1.0;
    int samplerate = (int)(A * 6.999) + 1;
    switch (samplerate)
    {
    case 1:
        overallscale *= (16.0 / 44.1);
        break; // 16
    case 2:
        overallscale *= (32.0 / 44.1);
        break; // 32
    case 3:
        overallscale *= 1.0;
        break; // 44.1
    case 4:
        overallscale *= (48.0 / 44.1);
        break; // 48
    case 5:
        overallscale *= (64.0 / 44.1);
        break; // 64
    case 6:
        overallscale *= 2.0;
        break; // 88.2
    case 7:
        overallscale *= (96.0 / 44.1);
        break; // 96
    }
    nonlin *= 0.001; // scale suitably to apply to our liveness value
    double basefeedback =
        0.45 + (nonlin * pow(((E * 2.0) - 1.0),
                             3)); // nonlin from previous sample, positive adds liveness when loud
    nonlin = 0.0;                 // reset it here for setting up again next time
    double tankfeedback = basefeedback + (pow(B, 2) * 0.05);
    // liveness
    if (tankfeedback > 0.5)
        tankfeedback = 0.5;
    if (tankfeedback < 0.4)
        tankfeedback = 0.4;
    double iirAmountC = 1.0 - pow(1.0 - C, 2);
    // most of the range is up at the top end
    iirAmountC += (iirAmountC / overallscale);
    iirAmountC /= 2.0;
    if (iirAmountC > 1.1)
        iirAmountC = 1.1;
    // lowpass, check to see if it's working reasonably at 96K
    double iirAmount = (((1.0 - pow(D, 2)) * 0.09) / overallscale) + 0.001;
    if (iirAmount > 1.0)
        iirAmount = 1.0;
    if (iirAmount < 0.001)
        iirAmount = 0.001;
    double wetness = F;
    double dryness = 1.0 - wetness;
    double roomsize = overallscale * 0.203;
    double lean = 0.125;
    double invlean = 1.0 - lean;
    double pspeed = 0.145;
    double outcouple = 0.5 - tankfeedback;
    double constallpass = 0.618033988749894848204586; // golden ratio!
    double temp;
    int allpasstemp;
    double predelay = 0.222 * overallscale;

    // reverb setup

    delayA = (int(maxdelayA * roomsize));
    delayB = (int(maxdelayB * roomsize));
    delayC = (int(maxdelayC * roomsize));
    delayD = (int(maxdelayD * roomsize));
    delayE = (int(maxdelayE * roomsize));
    delayF = (int(maxdelayF * roomsize));
    delayG = (int(maxdelayG * roomsize));
    delayH = (int(maxdelayH * roomsize));
    delayI = (int(maxdelayI * roomsize));
    delayJ = (int(maxdelayJ * roomsize));
    delayK = (int(maxdelayK * roomsize));
    delayL = (int(maxdelayL * roomsize));
    delayM = (int(maxdelayM * roomsize));
    delayN = (int(maxdelayN * roomsize));
    delayO = (int(maxdelayO * roomsize));
    delayP = (int(maxdelayP * roomsize));
    delayQ = (int(maxdelayQ * roomsize));
    delayR = (int(maxdelayR * roomsize));
    delayS = (int(maxdelayS * roomsize));
    delayT = (int(maxdelayT * roomsize));
    delayU = (int(maxdelayU * roomsize));
    delayV = (int(maxdelayV * roomsize));
    delayW = (int(maxdelayW * roomsize));
    delayX = (int(maxdelayX * roomsize));
    delayY = (int(maxdelayY * roomsize));
    delayZ = (int(maxdelayZ * roomsize));
    delayMid = (int(maxdelayMid * roomsize));
    delaySide = (int(maxdelaySide * roomsize));
    delayLeft = (int(maxdelayLeft * roomsize));
    delayRight = (int(maxdelayRight * roomsize));
    delaypre = (int(maxdelaypre * predelay));

    while (--sampleFrames >= 0)
    {
        inputSampleL = *in1;
        inputSampleR = *in2;
        if (fabs(inputSampleL) < 1.18e-23)
            inputSampleL = fpdL * 1.18e-17;
        if (fabs(inputSampleR) < 1.18e-23)
            inputSampleR = fpdR * 1.18e-17;
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        dpreL[onepre] = inputSampleL;
        dpreR[onepre] = inputSampleR;
        onepre--;
        if (onepre < 0 || onepre > delaypre)
        {
            onepre = delaypre;
        }
        inputSampleL = (dpreL[onepre]);
        inputSampleR = (dpreR[onepre]);
        // predelay

        interpolA += pitchshiftA * pspeed;
        interpolB += pitchshiftB * pspeed;
        interpolC += pitchshiftC * pspeed;
        interpolD += pitchshiftD * pspeed;
        interpolE += pitchshiftE * pspeed;
        interpolF += pitchshiftF * pspeed;
        interpolG += pitchshiftG * pspeed;
        interpolH += pitchshiftH * pspeed;
        interpolI += pitchshiftI * pspeed;
        interpolJ += pitchshiftJ * pspeed;
        interpolK += pitchshiftK * pspeed;
        interpolL += pitchshiftL * pspeed;
        interpolM += pitchshiftM * pspeed;
        interpolN += pitchshiftN * pspeed;
        interpolO += pitchshiftO * pspeed;
        interpolP += pitchshiftP * pspeed;
        interpolQ += pitchshiftQ * pspeed;
        interpolR += pitchshiftR * pspeed;
        interpolS += pitchshiftS * pspeed;
        interpolT += pitchshiftT * pspeed;
        interpolU += pitchshiftU * pspeed;
        interpolV += pitchshiftV * pspeed;
        interpolW += pitchshiftW * pspeed;
        interpolX += pitchshiftX * pspeed;
        interpolY += pitchshiftY * pspeed;
        interpolZ += pitchshiftZ * pspeed;
        // increment all the sub-sample offsets for the pitch shifting of combs

        if (interpolA > 1.0)
        {
            pitchshiftA = -fabs(pitchshiftA);
            interpolA += pitchshiftA * pspeed;
        }
        if (interpolB > 1.0)
        {
            pitchshiftB = -fabs(pitchshiftB);
            interpolB += pitchshiftB * pspeed;
        }
        if (interpolC > 1.0)
        {
            pitchshiftC = -fabs(pitchshiftC);
            interpolC += pitchshiftC * pspeed;
        }
        if (interpolD > 1.0)
        {
            pitchshiftD = -fabs(pitchshiftD);
            interpolD += pitchshiftD * pspeed;
        }
        if (interpolE > 1.0)
        {
            pitchshiftE = -fabs(pitchshiftE);
            interpolE += pitchshiftE * pspeed;
        }
        if (interpolF > 1.0)
        {
            pitchshiftF = -fabs(pitchshiftF);
            interpolF += pitchshiftF * pspeed;
        }
        if (interpolG > 1.0)
        {
            pitchshiftG = -fabs(pitchshiftG);
            interpolG += pitchshiftG * pspeed;
        }
        if (interpolH > 1.0)
        {
            pitchshiftH = -fabs(pitchshiftH);
            interpolH += pitchshiftH * pspeed;
        }
        if (interpolI > 1.0)
        {
            pitchshiftI = -fabs(pitchshiftI);
            interpolI += pitchshiftI * pspeed;
        }
        if (interpolJ > 1.0)
        {
            pitchshiftJ = -fabs(pitchshiftJ);
            interpolJ += pitchshiftJ * pspeed;
        }
        if (interpolK > 1.0)
        {
            pitchshiftK = -fabs(pitchshiftK);
            interpolK += pitchshiftK * pspeed;
        }
        if (interpolL > 1.0)
        {
            pitchshiftL = -fabs(pitchshiftL);
            interpolL += pitchshiftL * pspeed;
        }
        if (interpolM > 1.0)
        {
            pitchshiftM = -fabs(pitchshiftM);
            interpolM += pitchshiftM * pspeed;
        }
        if (interpolN > 1.0)
        {
            pitchshiftN = -fabs(pitchshiftN);
            interpolN += pitchshiftN * pspeed;
        }
        if (interpolO > 1.0)
        {
            pitchshiftO = -fabs(pitchshiftO);
            interpolO += pitchshiftO * pspeed;
        }
        if (interpolP > 1.0)
        {
            pitchshiftP = -fabs(pitchshiftP);
            interpolP += pitchshiftP * pspeed;
        }
        if (interpolQ > 1.0)
        {
            pitchshiftQ = -fabs(pitchshiftQ);
            interpolQ += pitchshiftQ * pspeed;
        }
        if (interpolR > 1.0)
        {
            pitchshiftR = -fabs(pitchshiftR);
            interpolR += pitchshiftR * pspeed;
        }
        if (interpolS > 1.0)
        {
            pitchshiftS = -fabs(pitchshiftS);
            interpolS += pitchshiftS * pspeed;
        }
        if (interpolT > 1.0)
        {
            pitchshiftT = -fabs(pitchshiftT);
            interpolT += pitchshiftT * pspeed;
        }
        if (interpolU > 1.0)
        {
            pitchshiftU = -fabs(pitchshiftU);
            interpolU += pitchshiftU * pspeed;
        }
        if (interpolV > 1.0)
        {
            pitchshiftV = -fabs(pitchshiftV);
            interpolV += pitchshiftV * pspeed;
        }
        if (interpolW > 1.0)
        {
            pitchshiftW = -fabs(pitchshiftW);
            interpolW += pitchshiftW * pspeed;
        }
        if (interpolX > 1.0)
        {
            pitchshiftX = -fabs(pitchshiftX);
            interpolX += pitchshiftX * pspeed;
        }
        if (interpolY > 1.0)
        {
            pitchshiftY = -fabs(pitchshiftY);
            interpolY += pitchshiftY * pspeed;
        }
        if (interpolZ > 1.0)
        {
            pitchshiftZ = -fabs(pitchshiftZ);
            interpolZ += pitchshiftZ * pspeed;
        }

        if (interpolA < 0.0)
        {
            pitchshiftA = fabs(pitchshiftA);
            interpolA += pitchshiftA * pspeed;
        }
        if (interpolB < 0.0)
        {
            pitchshiftB = fabs(pitchshiftB);
            interpolB += pitchshiftB * pspeed;
        }
        if (interpolC < 0.0)
        {
            pitchshiftC = fabs(pitchshiftC);
            interpolC += pitchshiftC * pspeed;
        }
        if (interpolD < 0.0)
        {
            pitchshiftD = fabs(pitchshiftD);
            interpolD += pitchshiftD * pspeed;
        }
        if (interpolE < 0.0)
        {
            pitchshiftE = fabs(pitchshiftE);
            interpolE += pitchshiftE * pspeed;
        }
        if (interpolF < 0.0)
        {
            pitchshiftF = fabs(pitchshiftF);
            interpolF += pitchshiftF * pspeed;
        }
        if (interpolG < 0.0)
        {
            pitchshiftG = fabs(pitchshiftG);
            interpolG += pitchshiftG * pspeed;
        }
        if (interpolH < 0.0)
        {
            pitchshiftH = fabs(pitchshiftH);
            interpolH += pitchshiftH * pspeed;
        }
        if (interpolI < 0.0)
        {
            pitchshiftI = fabs(pitchshiftI);
            interpolI += pitchshiftI * pspeed;
        }
        if (interpolJ < 0.0)
        {
            pitchshiftJ = fabs(pitchshiftJ);
            interpolJ += pitchshiftJ * pspeed;
        }
        if (interpolK < 0.0)
        {
            pitchshiftK = fabs(pitchshiftK);
            interpolK += pitchshiftK * pspeed;
        }
        if (interpolL < 0.0)
        {
            pitchshiftL = fabs(pitchshiftL);
            interpolL += pitchshiftL * pspeed;
        }
        if (interpolM < 0.0)
        {
            pitchshiftM = fabs(pitchshiftM);
            interpolM += pitchshiftM * pspeed;
        }
        if (interpolN < 0.0)
        {
            pitchshiftN = fabs(pitchshiftN);
            interpolN += pitchshiftN * pspeed;
        }
        if (interpolO < 0.0)
        {
            pitchshiftO = fabs(pitchshiftO);
            interpolO += pitchshiftO * pspeed;
        }
        if (interpolP < 0.0)
        {
            pitchshiftP = fabs(pitchshiftP);
            interpolP += pitchshiftP * pspeed;
        }
        if (interpolQ < 0.0)
        {
            pitchshiftQ = fabs(pitchshiftQ);
            interpolQ += pitchshiftQ * pspeed;
        }
        if (interpolR < 0.0)
        {
            pitchshiftR = fabs(pitchshiftR);
            interpolR += pitchshiftR * pspeed;
        }
        if (interpolS < 0.0)
        {
            pitchshiftS = fabs(pitchshiftS);
            interpolS += pitchshiftS * pspeed;
        }
        if (interpolT < 0.0)
        {
            pitchshiftT = fabs(pitchshiftT);
            interpolT += pitchshiftT * pspeed;
        }
        if (interpolU < 0.0)
        {
            pitchshiftU = fabs(pitchshiftU);
            interpolU += pitchshiftU * pspeed;
        }
        if (interpolV < 0.0)
        {
            pitchshiftV = fabs(pitchshiftV);
            interpolV += pitchshiftV * pspeed;
        }
        if (interpolW < 0.0)
        {
            pitchshiftW = fabs(pitchshiftW);
            interpolW += pitchshiftW * pspeed;
        }
        if (interpolX < 0.0)
        {
            pitchshiftX = fabs(pitchshiftX);
            interpolX += pitchshiftX * pspeed;
        }
        if (interpolY < 0.0)
        {
            pitchshiftY = fabs(pitchshiftY);
            interpolY += pitchshiftY * pspeed;
        }
        if (interpolZ < 0.0)
        {
            pitchshiftZ = fabs(pitchshiftZ);
            interpolZ += pitchshiftZ * pspeed;
        }
        // all of the sanity checks for interpol for all combs

        if (verboutR > 1.0)
            verboutR = 1.0;
        if (verboutR < -1.0)
            verboutR = -1.0;
        if (verboutL > 1.0)
            verboutL = 1.0;
        if (verboutL < -1.0)
            verboutL = -1.0;

        inputSampleL += verboutR;
        inputSampleR += verboutL;
        verboutL = 0.0;
        verboutR = 0.0;
        // here we add in the cross-coupling- output of L tank to R, output of R tank to L

        mid = inputSampleL + inputSampleR;
        side = inputSampleL - inputSampleR;
        // assign mid and side.

        allpasstemp = oneMid - 1;
        if (allpasstemp < 0 || allpasstemp > delayMid)
        {
            allpasstemp = delayMid;
        }
        mid -= dMid[allpasstemp] * constallpass;
        dMid[oneMid] = mid;
        mid *= constallpass;
        oneMid--;
        if (oneMid < 0 || oneMid > delayMid)
        {
            oneMid = delayMid;
        }
        mid += (dMid[oneMid]);
        nonlin += fabs(dMid[oneMid]);
        // allpass filter mid

        allpasstemp = oneSide - 1;
        if (allpasstemp < 0 || allpasstemp > delaySide)
        {
            allpasstemp = delaySide;
        }
        side -= dSide[allpasstemp] * constallpass;
        dSide[oneSide] = side;
        side *= constallpass;
        oneSide--;
        if (oneSide < 0 || oneSide > delaySide)
        {
            oneSide = delaySide;
        }
        side += (dSide[oneSide]);
        nonlin += fabs(dSide[oneSide]);
        // allpass filter side

        // here we do allpasses on the mid and side

        allpasstemp = oneLeft - 1;
        if (allpasstemp < 0 || allpasstemp > delayLeft)
        {
            allpasstemp = delayLeft;
        }
        inputSampleL -= dLeft[allpasstemp] * constallpass;
        dLeft[oneLeft] = verboutL;
        inputSampleL *= constallpass;
        oneLeft--;
        if (oneLeft < 0 || oneLeft > delayLeft)
        {
            oneLeft = delayLeft;
        }
        inputSampleL += (dLeft[oneLeft]);
        nonlin += fabs(dLeft[oneLeft]);
        // allpass filter left

        allpasstemp = oneRight - 1;
        if (allpasstemp < 0 || allpasstemp > delayRight)
        {
            allpasstemp = delayRight;
        }
        inputSampleR -= dRight[allpasstemp] * constallpass;
        dRight[oneRight] = verboutR;
        inputSampleR *= constallpass;
        oneRight--;
        if (oneRight < 0 || oneRight > delayRight)
        {
            oneRight = delayRight;
        }
        inputSampleR += (dRight[oneRight]);
        nonlin += fabs(dRight[oneRight]);
        // allpass filter right

        inputSampleL += (mid + side) / 2.0;
        inputSampleR += (mid - side) / 2.0;
        // here we get back to a L/R topology by adding the mid/side in parallel with L/R

        temp = (dA[oneA] * interpolA);
        temp += (dA[treA] * (1.0 - interpolA));
        temp += ((dA[twoA]));
        dA[treA] = (temp * tankfeedback);
        dA[treA] += inputSampleL;
        oneA--;
        if (oneA < 0 || oneA > delayA)
        {
            oneA = delayA;
        }
        twoA--;
        if (twoA < 0 || twoA > delayA)
        {
            twoA = delayA;
        }
        treA--;
        if (treA < 0 || treA > delayA)
        {
            treA = delayA;
        }
        temp = (dA[oneA] * interpolA);
        temp += (dA[treA] * (1.0 - interpolA));
        temp *= (invlean + (lean * fabs(dA[twoA])));
        verboutL += temp;
        // comb filter A
        temp = (dC[oneC] * interpolC);
        temp += (dC[treC] * (1.0 - interpolC));
        temp += ((dC[twoC]));
        dC[treC] = (temp * tankfeedback);
        dC[treC] += inputSampleL;
        oneC--;
        if (oneC < 0 || oneC > delayC)
        {
            oneC = delayC;
        }
        twoC--;
        if (twoC < 0 || twoC > delayC)
        {
            twoC = delayC;
        }
        treC--;
        if (treC < 0 || treC > delayC)
        {
            treC = delayC;
        }
        temp = (dC[oneC] * interpolC);
        temp += (dC[treC] * (1.0 - interpolC));
        temp *= (invlean + (lean * fabs(dC[twoC])));
        verboutL += temp;
        // comb filter C
        temp = (dE[oneE] * interpolE);
        temp += (dE[treE] * (1.0 - interpolE));
        temp += ((dE[twoE]));
        dE[treE] = (temp * tankfeedback);
        dE[treE] += inputSampleL;
        oneE--;
        if (oneE < 0 || oneE > delayE)
        {
            oneE = delayE;
        }
        twoE--;
        if (twoE < 0 || twoE > delayE)
        {
            twoE = delayE;
        }
        treE--;
        if (treE < 0 || treE > delayE)
        {
            treE = delayE;
        }
        temp = (dE[oneE] * interpolE);
        temp += (dE[treE] * (1.0 - interpolE));
        temp *= (invlean + (lean * fabs(dE[twoE])));
        verboutL += temp;
        // comb filter E
        temp = (dG[oneG] * interpolG);
        temp += (dG[treG] * (1.0 - interpolG));
        temp += ((dG[twoG]));
        dG[treG] = (temp * tankfeedback);
        dG[treG] += inputSampleL;
        oneG--;
        if (oneG < 0 || oneG > delayG)
        {
            oneG = delayG;
        }
        twoG--;
        if (twoG < 0 || twoG > delayG)
        {
            twoG = delayG;
        }
        treG--;
        if (treG < 0 || treG > delayG)
        {
            treG = delayG;
        }
        temp = (dG[oneG] * interpolG);
        temp += (dG[treG] * (1.0 - interpolG));
        temp *= (invlean + (lean * fabs(dG[twoG])));
        verboutL += temp;
        // comb filter G
        temp = (dI[oneI] * interpolI);
        temp += (dI[treI] * (1.0 - interpolI));
        temp += ((dI[twoI]));
        dI[treI] = (temp * tankfeedback);
        dI[treI] += inputSampleL;
        oneI--;
        if (oneI < 0 || oneI > delayI)
        {
            oneI = delayI;
        }
        twoI--;
        if (twoI < 0 || twoI > delayI)
        {
            twoI = delayI;
        }
        treI--;
        if (treI < 0 || treI > delayI)
        {
            treI = delayI;
        }
        temp = (dI[oneI] * interpolI);
        temp += (dI[treI] * (1.0 - interpolI));
        temp *= (invlean + (lean * fabs(dI[twoI])));
        verboutL += temp;
        // comb filter I
        temp = (dK[oneK] * interpolK);
        temp += (dK[treK] * (1.0 - interpolK));
        temp += ((dK[twoK]));
        dK[treK] = (temp * tankfeedback);
        dK[treK] += inputSampleL;
        oneK--;
        if (oneK < 0 || oneK > delayK)
        {
            oneK = delayK;
        }
        twoK--;
        if (twoK < 0 || twoK > delayK)
        {
            twoK = delayK;
        }
        treK--;
        if (treK < 0 || treK > delayK)
        {
            treK = delayK;
        }
        temp = (dK[oneK] * interpolK);
        temp += (dK[treK] * (1.0 - interpolK));
        temp *= (invlean + (lean * fabs(dK[twoK])));
        verboutL += temp;
        // comb filter K
        temp = (dM[oneM] * interpolM);
        temp += (dM[treM] * (1.0 - interpolM));
        temp += ((dM[twoM]));
        dM[treM] = (temp * tankfeedback);
        dM[treM] += inputSampleL;
        oneM--;
        if (oneM < 0 || oneM > delayM)
        {
            oneM = delayM;
        }
        twoM--;
        if (twoM < 0 || twoM > delayM)
        {
            twoM = delayM;
        }
        treM--;
        if (treM < 0 || treM > delayM)
        {
            treM = delayM;
        }
        temp = (dM[oneM] * interpolM);
        temp += (dM[treM] * (1.0 - interpolM));
        temp *= (invlean + (lean * fabs(dM[twoM])));
        verboutL += temp;
        // comb filter M
        temp = (dO[oneO] * interpolO);
        temp += (dO[treO] * (1.0 - interpolO));
        temp += ((dO[twoO]));
        dO[treO] = (temp * tankfeedback);
        dO[treO] += inputSampleL;
        oneO--;
        if (oneO < 0 || oneO > delayO)
        {
            oneO = delayO;
        }
        twoO--;
        if (twoO < 0 || twoO > delayO)
        {
            twoO = delayO;
        }
        treO--;
        if (treO < 0 || treO > delayO)
        {
            treO = delayO;
        }
        temp = (dO[oneO] * interpolO);
        temp += (dO[treO] * (1.0 - interpolO));
        temp *= (invlean + (lean * fabs(dO[twoO])));
        verboutL += temp;
        // comb filter O
        temp = (dQ[oneQ] * interpolQ);
        temp += (dQ[treQ] * (1.0 - interpolQ));
        temp += ((dQ[twoQ]));
        dQ[treQ] = (temp * tankfeedback);
        dQ[treQ] += inputSampleL;
        oneQ--;
        if (oneQ < 0 || oneQ > delayQ)
        {
            oneQ = delayQ;
        }
        twoQ--;
        if (twoQ < 0 || twoQ > delayQ)
        {
            twoQ = delayQ;
        }
        treQ--;
        if (treQ < 0 || treQ > delayQ)
        {
            treQ = delayQ;
        }
        temp = (dQ[oneQ] * interpolQ);
        temp += (dQ[treQ] * (1.0 - interpolQ));
        temp *= (invlean + (lean * fabs(dQ[twoQ])));
        verboutL += temp;
        // comb filter Q
        temp = (dS[oneS] * interpolS);
        temp += (dS[treS] * (1.0 - interpolS));
        temp += ((dS[twoS]));
        dS[treS] = (temp * tankfeedback);
        dS[treS] += inputSampleL;
        oneS--;
        if (oneS < 0 || oneS > delayS)
        {
            oneS = delayS;
        }
        twoS--;
        if (twoS < 0 || twoS > delayS)
        {
            twoS = delayS;
        }
        treS--;
        if (treS < 0 || treS > delayS)
        {
            treS = delayS;
        }
        temp = (dS[oneS] * interpolS);
        temp += (dS[treS] * (1.0 - interpolS));
        temp *= (invlean + (lean * fabs(dS[twoS])));
        verboutL += temp;
        // comb filter S
        temp = (dU[oneU] * interpolU);
        temp += (dU[treU] * (1.0 - interpolU));
        temp += ((dU[twoU]));
        dU[treU] = (temp * tankfeedback);
        dU[treU] += inputSampleL;
        oneU--;
        if (oneU < 0 || oneU > delayU)
        {
            oneU = delayU;
        }
        twoU--;
        if (twoU < 0 || twoU > delayU)
        {
            twoU = delayU;
        }
        treU--;
        if (treU < 0 || treU > delayU)
        {
            treU = delayU;
        }
        temp = (dU[oneU] * interpolU);
        temp += (dU[treU] * (1.0 - interpolU));
        temp *= (invlean + (lean * fabs(dU[twoU])));
        verboutL += temp;
        // comb filter U
        temp = (dW[oneW] * interpolW);
        temp += (dW[treW] * (1.0 - interpolW));
        temp += ((dW[twoW]));
        dW[treW] = (temp * tankfeedback);
        dW[treW] += inputSampleL;
        oneW--;
        if (oneW < 0 || oneW > delayW)
        {
            oneW = delayW;
        }
        twoW--;
        if (twoW < 0 || twoW > delayW)
        {
            twoW = delayW;
        }
        treW--;
        if (treW < 0 || treW > delayW)
        {
            treW = delayW;
        }
        temp = (dW[oneW] * interpolW);
        temp += (dW[treW] * (1.0 - interpolW));
        temp *= (invlean + (lean * fabs(dW[twoW])));
        verboutL += temp;
        // comb filter W
        temp = (dY[oneY] * interpolY);
        temp += (dY[treY] * (1.0 - interpolY));
        temp += ((dY[twoY]));
        dY[treY] = (temp * tankfeedback);
        dY[treY] += inputSampleL;
        oneY--;
        if (oneY < 0 || oneY > delayY)
        {
            oneY = delayY;
        }
        twoY--;
        if (twoY < 0 || twoY > delayY)
        {
            twoY = delayY;
        }
        treY--;
        if (treY < 0 || treY > delayY)
        {
            treY = delayY;
        }
        temp = (dY[oneY] * interpolY);
        temp += (dY[treY] * (1.0 - interpolY));
        temp *= (invlean + (lean * fabs(dY[twoY])));
        verboutL += temp;
        // comb filter Y
        // here we do the L delay tank, every other letter A C E G I

        temp = (dB[oneB] * interpolB);
        temp += (dB[treB] * (1.0 - interpolB));
        temp += ((dB[twoB]));
        dB[treB] = (temp * tankfeedback);
        dB[treB] += inputSampleR;
        oneB--;
        if (oneB < 0 || oneB > delayB)
        {
            oneB = delayB;
        }
        twoB--;
        if (twoB < 0 || twoB > delayB)
        {
            twoB = delayB;
        }
        treB--;
        if (treB < 0 || treB > delayB)
        {
            treB = delayB;
        }
        temp = (dB[oneB] * interpolB);
        temp += (dB[treB] * (1.0 - interpolB));
        temp *= (invlean + (lean * fabs(dB[twoB])));
        verboutR += temp;
        // comb filter B
        temp = (dD[oneD] * interpolD);
        temp += (dD[treD] * (1.0 - interpolD));
        temp += ((dD[twoD]));
        dD[treD] = (temp * tankfeedback);
        dD[treD] += inputSampleR;
        oneD--;
        if (oneD < 0 || oneD > delayD)
        {
            oneD = delayD;
        }
        twoD--;
        if (twoD < 0 || twoD > delayD)
        {
            twoD = delayD;
        }
        treD--;
        if (treD < 0 || treD > delayD)
        {
            treD = delayD;
        }
        temp = (dD[oneD] * interpolD);
        temp += (dD[treD] * (1.0 - interpolD));
        temp *= (invlean + (lean * fabs(dD[twoD])));
        verboutR += temp;
        // comb filter D
        temp = (dF[oneF] * interpolF);
        temp += (dF[treF] * (1.0 - interpolF));
        temp += ((dF[twoF]));
        dF[treF] = (temp * tankfeedback);
        dF[treF] += inputSampleR;
        oneF--;
        if (oneF < 0 || oneF > delayF)
        {
            oneF = delayF;
        }
        twoF--;
        if (twoF < 0 || twoF > delayF)
        {
            twoF = delayF;
        }
        treF--;
        if (treF < 0 || treF > delayF)
        {
            treF = delayF;
        }
        temp = (dF[oneF] * interpolF);
        temp += (dF[treF] * (1.0 - interpolF));
        temp *= (invlean + (lean * fabs(dF[twoF])));
        verboutR += temp;
        // comb filter F
        temp = (dH[oneH] * interpolH);
        temp += (dH[treH] * (1.0 - interpolH));
        temp += ((dH[twoH]));
        dH[treH] = (temp * tankfeedback);
        dH[treH] += inputSampleR;
        oneH--;
        if (oneH < 0 || oneH > delayH)
        {
            oneH = delayH;
        }
        twoH--;
        if (twoH < 0 || twoH > delayH)
        {
            twoH = delayH;
        }
        treH--;
        if (treH < 0 || treH > delayH)
        {
            treH = delayH;
        }
        temp = (dH[oneH] * interpolH);
        temp += (dH[treH] * (1.0 - interpolH));
        temp *= (invlean + (lean * fabs(dH[twoH])));
        verboutR += temp;
        // comb filter H
        temp = (dJ[oneJ] * interpolJ);
        temp += (dJ[treJ] * (1.0 - interpolJ));
        temp += ((dJ[twoJ]));
        dJ[treJ] = (temp * tankfeedback);
        dJ[treJ] += inputSampleR;
        oneJ--;
        if (oneJ < 0 || oneJ > delayJ)
        {
            oneJ = delayJ;
        }
        twoJ--;
        if (twoJ < 0 || twoJ > delayJ)
        {
            twoJ = delayJ;
        }
        treJ--;
        if (treJ < 0 || treJ > delayJ)
        {
            treJ = delayJ;
        }
        temp = (dJ[oneJ] * interpolJ);
        temp += (dJ[treJ] * (1.0 - interpolJ));
        temp *= (invlean + (lean * fabs(dJ[twoJ])));
        verboutR += temp;
        // comb filter J
        temp = (dL[oneL] * interpolL);
        temp += (dL[treL] * (1.0 - interpolL));
        temp += ((dL[twoL]));
        dL[treL] = (temp * tankfeedback);
        dL[treL] += inputSampleR;
        oneL--;
        if (oneL < 0 || oneL > delayL)
        {
            oneL = delayL;
        }
        twoL--;
        if (twoL < 0 || twoL > delayL)
        {
            twoL = delayL;
        }
        treL--;
        if (treL < 0 || treL > delayL)
        {
            treL = delayL;
        }
        temp = (dL[oneL] * interpolL);
        temp += (dL[treL] * (1.0 - interpolL));
        temp *= (invlean + (lean * fabs(dL[twoL])));
        verboutR += temp;
        // comb filter L
        temp = (dN[oneN] * interpolN);
        temp += (dN[treN] * (1.0 - interpolN));
        temp += ((dN[twoN]));
        dN[treN] = (temp * tankfeedback);
        dN[treN] += inputSampleR;
        oneN--;
        if (oneN < 0 || oneN > delayN)
        {
            oneN = delayN;
        }
        twoN--;
        if (twoN < 0 || twoN > delayN)
        {
            twoN = delayN;
        }
        treN--;
        if (treN < 0 || treN > delayN)
        {
            treN = delayN;
        }
        temp = (dN[oneN] * interpolN);
        temp += (dN[treN] * (1.0 - interpolN));
        temp *= (invlean + (lean * fabs(dN[twoN])));
        verboutR += temp;
        // comb filter N
        temp = (dP[oneP] * interpolP);
        temp += (dP[treP] * (1.0 - interpolP));
        temp += ((dP[twoP]));
        dP[treP] = (temp * tankfeedback);
        dP[treP] += inputSampleR;
        oneP--;
        if (oneP < 0 || oneP > delayP)
        {
            oneP = delayP;
        }
        twoP--;
        if (twoP < 0 || twoP > delayP)
        {
            twoP = delayP;
        }
        treP--;
        if (treP < 0 || treP > delayP)
        {
            treP = delayP;
        }
        temp = (dP[oneP] * interpolP);
        temp += (dP[treP] * (1.0 - interpolP));
        temp *= (invlean + (lean * fabs(dP[twoP])));
        verboutR += temp;
        // comb filter P
        temp = (dR[oneR] * interpolR);
        temp += (dR[treR] * (1.0 - interpolR));
        temp += ((dR[twoR]));
        dR[treR] = (temp * tankfeedback);
        dR[treR] += inputSampleR;
        oneR--;
        if (oneR < 0 || oneR > delayR)
        {
            oneR = delayR;
        }
        twoR--;
        if (twoR < 0 || twoR > delayR)
        {
            twoR = delayR;
        }
        treR--;
        if (treR < 0 || treR > delayR)
        {
            treR = delayR;
        }
        temp = (dR[oneR] * interpolR);
        temp += (dR[treR] * (1.0 - interpolR));
        temp *= (invlean + (lean * fabs(dR[twoR])));
        verboutR += temp;
        // comb filter R
        temp = (dT[oneT] * interpolT);
        temp += (dT[treT] * (1.0 - interpolT));
        temp += ((dT[twoT]));
        dT[treT] = (temp * tankfeedback);
        dT[treT] += inputSampleR;
        oneT--;
        if (oneT < 0 || oneT > delayT)
        {
            oneT = delayT;
        }
        twoT--;
        if (twoT < 0 || twoT > delayT)
        {
            twoT = delayT;
        }
        treT--;
        if (treT < 0 || treT > delayT)
        {
            treT = delayT;
        }
        temp = (dT[oneT] * interpolT);
        temp += (dT[treT] * (1.0 - interpolT));
        temp *= (invlean + (lean * fabs(dT[twoT])));
        verboutR += temp;
        // comb filter T
        temp = (dV[oneV] * interpolV);
        temp += (dV[treV] * (1.0 - interpolV));
        temp += ((dV[twoV]));
        dV[treV] = (temp * tankfeedback);
        dV[treV] += inputSampleR;
        oneV--;
        if (oneV < 0 || oneV > delayV)
        {
            oneV = delayV;
        }
        twoV--;
        if (twoV < 0 || twoV > delayV)
        {
            twoV = delayV;
        }
        treV--;
        if (treV < 0 || treV > delayV)
        {
            treV = delayV;
        }
        temp = (dV[oneV] * interpolV);
        temp += (dV[treV] * (1.0 - interpolV));
        temp *= (invlean + (lean * fabs(dV[twoV])));
        verboutR += temp;
        // comb filter V
        temp = (dX[oneX] * interpolX);
        temp += (dX[treX] * (1.0 - interpolX));
        temp += ((dX[twoX]));
        dX[treX] = (temp * tankfeedback);
        dX[treX] += inputSampleR;
        oneX--;
        if (oneX < 0 || oneX > delayX)
        {
            oneX = delayX;
        }
        twoX--;
        if (twoX < 0 || twoX > delayX)
        {
            twoX = delayX;
        }
        treX--;
        if (treX < 0 || treX > delayX)
        {
            treX = delayX;
        }
        temp = (dX[oneX] * interpolX);
        temp += (dX[treX] * (1.0 - interpolX));
        temp *= (invlean + (lean * fabs(dX[twoX])));
        verboutR += temp;
        // comb filter X
        temp = (dZ[oneZ] * interpolZ);
        temp += (dZ[treZ] * (1.0 - interpolZ));
        temp += ((dZ[twoZ]));
        dZ[treZ] = (temp * tankfeedback);
        dZ[treZ] += inputSampleR;
        oneZ--;
        if (oneZ < 0 || oneZ > delayZ)
        {
            oneZ = delayZ;
        }
        twoZ--;
        if (twoZ < 0 || twoZ > delayZ)
        {
            twoZ = delayZ;
        }
        treZ--;
        if (treZ < 0 || treZ > delayZ)
        {
            treZ = delayZ;
        }
        temp = (dZ[oneZ] * interpolZ);
        temp += (dZ[treZ] * (1.0 - interpolZ));
        temp *= (invlean + (lean * fabs(dZ[twoZ])));
        verboutR += temp;
        // comb filter Z
        // here we do the R delay tank, every other letter B D F H J

        verboutL /= 8;
        verboutR /= 8;

        iirSampleL = (iirSampleL * (1 - iirAmount)) + (verboutL * iirAmount);
        verboutL = verboutL - iirSampleL;

        iirSampleR = (iirSampleR * (1 - iirAmount)) + (verboutR * iirAmount);
        verboutR = verboutR - iirSampleR;
        // we need to highpass the crosscoupling, it's making DC runaway

        verboutL *= (invlean + (lean * fabs(verboutL)));
        verboutR *= (invlean + (lean * fabs(verboutR)));
        // scale back the verb tank the same way we scaled the combs

        inputSampleL = verboutL;
        inputSampleR = verboutR;

        // EQ lowpass is after all processing like the compressor that might produce hash
        if (flip)
        {
            lowpassSampleAA = (lowpassSampleAA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleAA;
            lowpassSampleBA = (lowpassSampleBA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleBA;
            lowpassSampleCA = (lowpassSampleCA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleCA;
            lowpassSampleDA = (lowpassSampleDA * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleDA;
            lowpassSampleE = (lowpassSampleE * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleE;
        }
        else
        {
            lowpassSampleAB = (lowpassSampleAB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleAB;
            lowpassSampleBB = (lowpassSampleBB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleBB;
            lowpassSampleCB = (lowpassSampleCB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleCB;
            lowpassSampleDB = (lowpassSampleDB * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleDB;
            lowpassSampleF = (lowpassSampleF * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
            inputSampleL = lowpassSampleF;
        }
        lowpassSampleG = (lowpassSampleG * (1 - iirAmountC)) + (inputSampleL * iirAmountC);
        inputSampleL = (lowpassSampleG * (1 - iirAmountC)) + (inputSampleL * iirAmountC);

        if (flip)
        {
            rowpassSampleAA = (rowpassSampleAA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleAA;
            rowpassSampleBA = (rowpassSampleBA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleBA;
            rowpassSampleCA = (rowpassSampleCA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleCA;
            rowpassSampleDA = (rowpassSampleDA * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleDA;
            rowpassSampleE = (rowpassSampleE * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleE;
        }
        else
        {
            rowpassSampleAB = (rowpassSampleAB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleAB;
            rowpassSampleBB = (rowpassSampleBB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleBB;
            rowpassSampleCB = (rowpassSampleCB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleCB;
            rowpassSampleDB = (rowpassSampleDB * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleDB;
            rowpassSampleF = (rowpassSampleF * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
            inputSampleR = rowpassSampleF;
        }
        rowpassSampleG = (rowpassSampleG * (1 - iirAmountC)) + (inputSampleR * iirAmountC);
        inputSampleR = (rowpassSampleG * (1 - iirAmountC)) + (inputSampleR * iirAmountC);

        iirCCSampleL = (iirCCSampleL * (1 - iirAmount)) + (verboutL * iirAmount);
        verboutL = verboutL - iirCCSampleL;

        iirCCSampleR = (iirCCSampleR * (1 - iirAmount)) + (verboutR * iirAmount);
        verboutR = verboutR - iirCCSampleR;
        // we need to highpass the crosscoupling, it's making DC runaway

        verboutL *= (invlean + (lean * fabs(verboutL)));
        verboutR *= (invlean + (lean * fabs(verboutR)));
        // scale back the crosscouple the same way we scaled the combs
        verboutL = (inputSampleL)*outcouple;
        verboutR = (inputSampleR)*outcouple;
        // send it off to the input again

        nonlin += fabs(verboutL);
        nonlin += fabs(verboutR); // post highpassing and a lot of processing

        drySampleL *= dryness;
        drySampleR *= dryness;

        inputSampleL *= wetness;
        inputSampleR *= wetness;

        inputSampleL += drySampleL;
        inputSampleR += drySampleR;
        // here we combine the tanks with the dry signal

        // begin 64 bit stereo floating point dither
        // int expon; frexp((double)inputSampleL, &expon);
        fpdL ^= fpdL << 13;
        fpdL ^= fpdL >> 17;
        fpdL ^= fpdL << 5;
        // inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        // frexp((double)inputSampleR, &expon);
        fpdR ^= fpdR << 13;
        fpdR ^= fpdR >> 17;
        fpdR ^= fpdR << 5;
        // inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        // end 64 bit stereo floating point dither
        flip = !flip;

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        in1++;
        in2++;
        out1++;
        out2++;
    }
}

} // end namespace NonlinearSpace
