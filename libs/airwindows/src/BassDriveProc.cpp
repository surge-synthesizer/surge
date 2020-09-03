/* ========================================
 *  BassDrive - BassDrive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BassDrive_H
#include "BassDrive.h"
#endif

namespace BassDrive {


void BassDrive::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double sumL;
	double sumR;
	double presence = pow(A,5) * 8.0;
	double high = pow(B,3) * 4.0;
	double mid = pow(C,2);
	double low = D / 4.0;
	double drive = E * 2.0;
	double bridgerectifier;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
		if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
		
		sumL = 0.0;
		sumR = 0.0;
		
		
		if (flip)
		{
			presenceInAL[0] = presenceInAL[1]; presenceInAL[1] = presenceInAL[2]; presenceInAL[2] = presenceInAL[3];
			presenceInAL[3] = presenceInAL[4]; presenceInAL[4] = presenceInAL[5]; presenceInAL[5] = presenceInAL[6]; 
			presenceInAL[6] = inputSampleL * presence; presenceOutAL[2] = presenceOutAL[3];
			presenceOutAL[3] = presenceOutAL[4]; presenceOutAL[4] = presenceOutAL[5]; presenceOutAL[5] = presenceOutAL[6]; 
			presenceOutAL[6] = (presenceInAL[0] + presenceInAL[6]) + 1.9152966321 * (presenceInAL[1] + presenceInAL[5]) 
			- (presenceInAL[2] + presenceInAL[4]) - 3.8305932641 * presenceInAL[3]
			+ ( -0.2828214615 * presenceOutAL[2]) + (  0.2613069963 * presenceOutAL[3])
			+ ( -0.8628193852 * presenceOutAL[4]) + (  0.5387164389 * presenceOutAL[5]);
			bridgerectifier = fabs(presenceOutAL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutAL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//presence section L
			presenceInAR[0] = presenceInAR[1]; presenceInAR[1] = presenceInAR[2]; presenceInAR[2] = presenceInAR[3];
			presenceInAR[3] = presenceInAR[4]; presenceInAR[4] = presenceInAR[5]; presenceInAR[5] = presenceInAR[6]; 
			presenceInAR[6] = inputSampleR * presence; presenceOutAR[2] = presenceOutAR[3];
			presenceOutAR[3] = presenceOutAR[4]; presenceOutAR[4] = presenceOutAR[5]; presenceOutAR[5] = presenceOutAR[6]; 
			presenceOutAR[6] = (presenceInAR[0] + presenceInAR[6]) + 1.9152966321 * (presenceInAR[1] + presenceInAR[5]) 
			- (presenceInAR[2] + presenceInAR[4]) - 3.8305932641 * presenceInAR[3]
			+ ( -0.2828214615 * presenceOutAR[2]) + (  0.2613069963 * presenceOutAR[3])
			+ ( -0.8628193852 * presenceOutAR[4]) + (  0.5387164389 * presenceOutAR[5]);
			bridgerectifier = fabs(presenceOutAR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutAR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//presence section R
			
			highInAL[0] = highInAL[1]; highInAL[1] = highInAL[2]; highInAL[2] = highInAL[3];
			highInAL[3] = highInAL[4]; highInAL[4] = highInAL[5]; highInAL[5] = highInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {highInAL[6] = bridgerectifier;}
			else {highInAL[6] = -bridgerectifier;}
			highInAL[6] *= high; highOutAL[2] = highOutAL[3];
			highOutAL[3] = highOutAL[4]; highOutAL[4] = highOutAL[5]; highOutAL[5] = highOutAL[6]; 
			highOutAL[6] = (highInAL[0] + highInAL[6]) -   0.5141967433 * (highInAL[1] + highInAL[5]) 
			- (highInAL[2] + highInAL[4]) +   1.0283934866 * highInAL[3]
			+ ( -0.2828214615 * highOutAL[2]) + (  1.0195930909 * highOutAL[3])
			+ ( -1.9633013869 * highOutAL[4]) + (  2.1020162751 * highOutAL[5]);
			bridgerectifier = fabs(highOutAL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutAL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//high section L
			highInAR[0] = highInAR[1]; highInAR[1] = highInAR[2]; highInAR[2] = highInAR[3];
			highInAR[3] = highInAR[4]; highInAR[4] = highInAR[5]; highInAR[5] = highInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {highInAR[6] = bridgerectifier;}
			else {highInAR[6] = -bridgerectifier;}
			highInAR[6] *= high; highOutAR[2] = highOutAR[3];
			highOutAR[3] = highOutAR[4]; highOutAR[4] = highOutAR[5]; highOutAR[5] = highOutAR[6]; 
			highOutAR[6] = (highInAR[0] + highInAR[6]) -   0.5141967433 * (highInAR[1] + highInAR[5]) 
			- (highInAR[2] + highInAR[4]) +   1.0283934866 * highInAR[3]
			+ ( -0.2828214615 * highOutAR[2]) + (  1.0195930909 * highOutAR[3])
			+ ( -1.9633013869 * highOutAR[4]) + (  2.1020162751 * highOutAR[5]);
			bridgerectifier = fabs(highOutAR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutAR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//high section R
			
			midInAL[0] = midInAL[1]; midInAL[1] = midInAL[2]; midInAL[2] = midInAL[3];
			midInAL[3] = midInAL[4]; midInAL[4] = midInAL[5]; midInAL[5] = midInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {midInAL[6] = bridgerectifier;}
			else {midInAL[6] = -bridgerectifier;}
			midInAL[6] *= mid; midOutAL[2] = midOutAL[3];
			midOutAL[3] = midOutAL[4]; midOutAL[4] = midOutAL[5]; midOutAL[5] = midOutAL[6]; 
			midOutAL[6] = (midInAL[0] + midInAL[6]) - 1.1790257790 * (midInAL[1] + midInAL[5]) 
			- (midInAL[2] + midInAL[4]) + 2.3580515580 * midInAL[3]
			+ ( -0.6292082828 * midOutAL[2]) + (  2.7785843605 * midOutAL[3])
			+ ( -4.6638295236 * midOutAL[4]) + (  3.5142515802 * midOutAL[5]);
			sumL += midOutAL[6];
			//mid section L
			midInAR[0] = midInAR[1]; midInAR[1] = midInAR[2]; midInAR[2] = midInAR[3];
			midInAR[3] = midInAR[4]; midInAR[4] = midInAR[5]; midInAR[5] = midInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {midInAR[6] = bridgerectifier;}
			else {midInAR[6] = -bridgerectifier;}
			midInAR[6] *= mid; midOutAR[2] = midOutAR[3];
			midOutAR[3] = midOutAR[4]; midOutAR[4] = midOutAR[5]; midOutAR[5] = midOutAR[6]; 
			midOutAR[6] = (midInAR[0] + midInAR[6]) - 1.1790257790 * (midInAR[1] + midInAR[5]) 
			- (midInAR[2] + midInAR[4]) + 2.3580515580 * midInAR[3]
			+ ( -0.6292082828 * midOutAR[2]) + (  2.7785843605 * midOutAR[3])
			+ ( -4.6638295236 * midOutAR[4]) + (  3.5142515802 * midOutAR[5]);
			sumR += midOutAR[6];
			//mid section R
			
			lowInAL[0] = lowInAL[1]; lowInAL[1] = lowInAL[2]; lowInAL[2] = lowInAL[3];
			lowInAL[3] = lowInAL[4]; lowInAL[4] = lowInAL[5]; lowInAL[5] = lowInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {lowInAL[6] = bridgerectifier;}
			else {lowInAL[6] = -bridgerectifier;}
			lowInAL[6] *= low; lowOutAL[2] = lowOutAL[3];
			lowOutAL[3] = lowOutAL[4]; lowOutAL[4] = lowOutAL[5]; lowOutAL[5] = lowOutAL[6]; 
			lowOutAL[6] = (lowInAL[0] + lowInAL[6]) - 1.9193504547 * (lowInAL[1] + lowInAL[5]) 
			- (lowInAL[2] + lowInAL[4]) + 3.8387009093 * lowInAL[3]
			+ ( -0.9195964462 * lowOutAL[2]) + (  3.7538173833 * lowOutAL[3])
			+ ( -5.7487775603 * lowOutAL[4]) + (  3.9145559258 * lowOutAL[5]);
			sumL += lowOutAL[6];
			//low section L
			lowInAR[0] = lowInAR[1]; lowInAR[1] = lowInAR[2]; lowInAR[2] = lowInAR[3];
			lowInAR[3] = lowInAR[4]; lowInAR[4] = lowInAR[5]; lowInAR[5] = lowInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {lowInAR[6] = bridgerectifier;}
			else {lowInAR[6] = -bridgerectifier;}
			lowInAR[6] *= low; lowOutAR[2] = lowOutAR[3];
			lowOutAR[3] = lowOutAR[4]; lowOutAR[4] = lowOutAR[5]; lowOutAR[5] = lowOutAR[6]; 
			lowOutAR[6] = (lowInAR[0] + lowInAR[6]) - 1.9193504547 * (lowInAR[1] + lowInAR[5]) 
			- (lowInAR[2] + lowInAR[4]) + 3.8387009093 * lowInAR[3]
			+ ( -0.9195964462 * lowOutAR[2]) + (  3.7538173833 * lowOutAR[3])
			+ ( -5.7487775603 * lowOutAR[4]) + (  3.9145559258 * lowOutAR[5]);
			sumR += lowOutAR[6];
			//low section R
		}
		else
		{
			presenceInBL[0] = presenceInBL[1]; presenceInBL[1] = presenceInBL[2]; presenceInBL[2] = presenceInBL[3];
			presenceInBL[3] = presenceInBL[4]; presenceInBL[4] = presenceInBL[5]; presenceInBL[5] = presenceInBL[6]; 
			presenceInBL[6] = inputSampleL * presence; presenceOutBL[2] = presenceOutBL[3];
			presenceOutBL[3] = presenceOutBL[4]; presenceOutBL[4] = presenceOutBL[5]; presenceOutBL[5] = presenceOutBL[6]; 
			presenceOutBL[6] = (presenceInBL[0] + presenceInBL[6]) + 1.9152966321 * (presenceInBL[1] + presenceInBL[5]) 
			- (presenceInBL[2] + presenceInBL[4]) - 3.8305932641 * presenceInBL[3]
			+ ( -0.2828214615 * presenceOutBL[2]) + (  0.2613069963 * presenceOutBL[3])
			+ ( -0.8628193852 * presenceOutBL[4]) + (  0.5387164389 * presenceOutBL[5]);
			bridgerectifier = fabs(presenceOutBL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutBL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//presence section L
			presenceInBR[0] = presenceInBR[1]; presenceInBR[1] = presenceInBR[2]; presenceInBR[2] = presenceInBR[3];
			presenceInBR[3] = presenceInBR[4]; presenceInBR[4] = presenceInBR[5]; presenceInBR[5] = presenceInBR[6]; 
			presenceInBR[6] = inputSampleR * presence; presenceOutBR[2] = presenceOutBR[3];
			presenceOutBR[3] = presenceOutBR[4]; presenceOutBR[4] = presenceOutBR[5]; presenceOutBR[5] = presenceOutBR[6]; 
			presenceOutBR[6] = (presenceInBR[0] + presenceInBR[6]) + 1.9152966321 * (presenceInBR[1] + presenceInBR[5]) 
			- (presenceInBR[2] + presenceInBR[4]) - 3.8305932641 * presenceInBR[3]
			+ ( -0.2828214615 * presenceOutBR[2]) + (  0.2613069963 * presenceOutBR[3])
			+ ( -0.8628193852 * presenceOutBR[4]) + (  0.5387164389 * presenceOutBR[5]);
			bridgerectifier = fabs(presenceOutBR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutBR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//presence section R
			
			highInBL[0] = highInBL[1]; highInBL[1] = highInBL[2]; highInBL[2] = highInBL[3];
			highInBL[3] = highInBL[4]; highInBL[4] = highInBL[5]; highInBL[5] = highInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {highInBL[6] = bridgerectifier;}
			else {highInBL[6] = -bridgerectifier;}
			highInBL[6] *= high; highOutBL[2] = highOutBL[3];
			highOutBL[3] = highOutBL[4]; highOutBL[4] = highOutBL[5]; highOutBL[5] = highOutBL[6]; 
			highOutBL[6] = (highInBL[0] + highInBL[6]) -   0.5141967433 * (highInBL[1] + highInBL[5]) 
			- (highInBL[2] + highInBL[4]) +   1.0283934866 * highInBL[3]
			+ ( -0.2828214615 * highOutBL[2]) + (  1.0195930909 * highOutBL[3])
			+ ( -1.9633013869 * highOutBL[4]) + (  2.1020162751 * highOutBL[5]);
			bridgerectifier = fabs(highOutBL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutBL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//high section L
			highInBR[0] = highInBR[1]; highInBR[1] = highInBR[2]; highInBR[2] = highInBR[3];
			highInBR[3] = highInBR[4]; highInBR[4] = highInBR[5]; highInBR[5] = highInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {highInBR[6] = bridgerectifier;}
			else {highInBR[6] = -bridgerectifier;}
			highInBR[6] *= high; highOutBR[2] = highOutBR[3];
			highOutBR[3] = highOutBR[4]; highOutBR[4] = highOutBR[5]; highOutBR[5] = highOutBR[6]; 
			highOutBR[6] = (highInBR[0] + highInBR[6]) -   0.5141967433 * (highInBR[1] + highInBR[5]) 
			- (highInBR[2] + highInBR[4]) +   1.0283934866 * highInBR[3]
			+ ( -0.2828214615 * highOutBR[2]) + (  1.0195930909 * highOutBR[3])
			+ ( -1.9633013869 * highOutBR[4]) + (  2.1020162751 * highOutBR[5]);
			bridgerectifier = fabs(highOutBR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutBR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//high section R
			
			midInBL[0] = midInBL[1]; midInBL[1] = midInBL[2]; midInBL[2] = midInBL[3];
			midInBL[3] = midInBL[4]; midInBL[4] = midInBL[5]; midInBL[5] = midInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {midInBL[6] = bridgerectifier;}
			else {midInBL[6] = -bridgerectifier;}
			midInBL[6] *= mid; midOutBL[2] = midOutBL[3];
			midOutBL[3] = midOutBL[4]; midOutBL[4] = midOutBL[5]; midOutBL[5] = midOutBL[6]; 
			midOutBL[6] = (midInBL[0] + midInBL[6]) - 1.1790257790 * (midInBL[1] + midInBL[5]) 
			- (midInBL[2] + midInBL[4]) + 2.3580515580 * midInBL[3]
			+ ( -0.6292082828 * midOutBL[2]) + (  2.7785843605 * midOutBL[3])
			+ ( -4.6638295236 * midOutBL[4]) + (  3.5142515802 * midOutBL[5]);
			sumL += midOutBL[6];
			//mid section L
			midInBR[0] = midInBR[1]; midInBR[1] = midInBR[2]; midInBR[2] = midInBR[3];
			midInBR[3] = midInBR[4]; midInBR[4] = midInBR[5]; midInBR[5] = midInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {midInBR[6] = bridgerectifier;}
			else {midInBR[6] = -bridgerectifier;}
			midInBR[6] *= mid; midOutBR[2] = midOutBR[3];
			midOutBR[3] = midOutBR[4]; midOutBR[4] = midOutBR[5]; midOutBR[5] = midOutBR[6]; 
			midOutBR[6] = (midInBR[0] + midInBR[6]) - 1.1790257790 * (midInBR[1] + midInBR[5]) 
			- (midInBR[2] + midInBR[4]) + 2.3580515580 * midInBR[3]
			+ ( -0.6292082828 * midOutBR[2]) + (  2.7785843605 * midOutBR[3])
			+ ( -4.6638295236 * midOutBR[4]) + (  3.5142515802 * midOutBR[5]);
			sumR += midOutBR[6];
			//mid section R
			
			lowInBL[0] = lowInBL[1]; lowInBL[1] = lowInBL[2]; lowInBL[2] = lowInBL[3];
			lowInBL[3] = lowInBL[4]; lowInBL[4] = lowInBL[5]; lowInBL[5] = lowInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {lowInBL[6] = bridgerectifier;}
			else {lowInBL[6] = -bridgerectifier;}
			lowInBL[6] *= low; lowOutBL[2] = lowOutBL[3];
			lowOutBL[3] = lowOutBL[4]; lowOutBL[4] = lowOutBL[5]; lowOutBL[5] = lowOutBL[6]; 
			lowOutBL[6] = (lowInBL[0] + lowInBL[6]) - 1.9193504547 * (lowInBL[1] + lowInBL[5]) 
			- (lowInBL[2] + lowInBL[4]) + 3.8387009093 * lowInBL[3]
			+ ( -0.9195964462 * lowOutBL[2]) + (  3.7538173833 * lowOutBL[3])
			+ ( -5.7487775603 * lowOutBL[4]) + (  3.9145559258 * lowOutBL[5]);
			sumL += lowOutBL[6];
			//low section L
			lowInBR[0] = lowInBR[1]; lowInBR[1] = lowInBR[2]; lowInBR[2] = lowInBR[3];
			lowInBR[3] = lowInBR[4]; lowInBR[4] = lowInBR[5]; lowInBR[5] = lowInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {lowInBR[6] = bridgerectifier;}
			else {lowInBR[6] = -bridgerectifier;}
			lowInBR[6] *= low; lowOutBR[2] = lowOutBR[3];
			lowOutBR[3] = lowOutBR[4]; lowOutBR[4] = lowOutBR[5]; lowOutBR[5] = lowOutBR[6]; 
			lowOutBR[6] = (lowInBR[0] + lowInBR[6]) - 1.9193504547 * (lowInBR[1] + lowInBR[5]) 
			- (lowInBR[2] + lowInBR[4]) + 3.8387009093 * lowInBR[3]
			+ ( -0.9195964462 * lowOutBR[2]) + (  3.7538173833 * lowOutBR[3])
			+ ( -5.7487775603 * lowOutBR[4]) + (  3.9145559258 * lowOutBR[5]);
			sumR += lowOutBR[6];
			//low section R
		}
		
		inputSampleL = fabs(sumL) * drive;
		if (inputSampleL > 1.57079633) {inputSampleL = 1.57079633;}
		inputSampleL = sin(inputSampleL);
		if (sumL < 0) inputSampleL = -inputSampleL;
		//output L
		inputSampleR = fabs(sumR) * drive;
		if (inputSampleR > 1.57079633) {inputSampleR = 1.57079633;}
		inputSampleR = sin(inputSampleR);
		if (sumR < 0) inputSampleR = -inputSampleR;
		//output R
		
		flip = !flip;
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void BassDrive::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double sumL;
	double sumR;
	double presence = pow(A,5) * 8.0;
	double high = pow(B,3) * 4.0;
	double mid = pow(C,2);
	double low = D / 4.0;
	double drive = E * 2.0;
	double bridgerectifier;
	
    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
		if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
		
		sumL = 0.0;
		sumR = 0.0;
		
		
		if (flip)
		{
			presenceInAL[0] = presenceInAL[1]; presenceInAL[1] = presenceInAL[2]; presenceInAL[2] = presenceInAL[3];
			presenceInAL[3] = presenceInAL[4]; presenceInAL[4] = presenceInAL[5]; presenceInAL[5] = presenceInAL[6]; 
			presenceInAL[6] = inputSampleL * presence; presenceOutAL[2] = presenceOutAL[3];
			presenceOutAL[3] = presenceOutAL[4]; presenceOutAL[4] = presenceOutAL[5]; presenceOutAL[5] = presenceOutAL[6]; 
			presenceOutAL[6] = (presenceInAL[0] + presenceInAL[6]) + 1.9152966321 * (presenceInAL[1] + presenceInAL[5]) 
			- (presenceInAL[2] + presenceInAL[4]) - 3.8305932641 * presenceInAL[3]
			+ ( -0.2828214615 * presenceOutAL[2]) + (  0.2613069963 * presenceOutAL[3])
			+ ( -0.8628193852 * presenceOutAL[4]) + (  0.5387164389 * presenceOutAL[5]);
			bridgerectifier = fabs(presenceOutAL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutAL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//presence section L
			presenceInAR[0] = presenceInAR[1]; presenceInAR[1] = presenceInAR[2]; presenceInAR[2] = presenceInAR[3];
			presenceInAR[3] = presenceInAR[4]; presenceInAR[4] = presenceInAR[5]; presenceInAR[5] = presenceInAR[6]; 
			presenceInAR[6] = inputSampleR * presence; presenceOutAR[2] = presenceOutAR[3];
			presenceOutAR[3] = presenceOutAR[4]; presenceOutAR[4] = presenceOutAR[5]; presenceOutAR[5] = presenceOutAR[6]; 
			presenceOutAR[6] = (presenceInAR[0] + presenceInAR[6]) + 1.9152966321 * (presenceInAR[1] + presenceInAR[5]) 
			- (presenceInAR[2] + presenceInAR[4]) - 3.8305932641 * presenceInAR[3]
			+ ( -0.2828214615 * presenceOutAR[2]) + (  0.2613069963 * presenceOutAR[3])
			+ ( -0.8628193852 * presenceOutAR[4]) + (  0.5387164389 * presenceOutAR[5]);
			bridgerectifier = fabs(presenceOutAR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutAR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//presence section R
			
			highInAL[0] = highInAL[1]; highInAL[1] = highInAL[2]; highInAL[2] = highInAL[3];
			highInAL[3] = highInAL[4]; highInAL[4] = highInAL[5]; highInAL[5] = highInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {highInAL[6] = bridgerectifier;}
			else {highInAL[6] = -bridgerectifier;}
			highInAL[6] *= high; highOutAL[2] = highOutAL[3];
			highOutAL[3] = highOutAL[4]; highOutAL[4] = highOutAL[5]; highOutAL[5] = highOutAL[6]; 
			highOutAL[6] = (highInAL[0] + highInAL[6]) -   0.5141967433 * (highInAL[1] + highInAL[5]) 
			- (highInAL[2] + highInAL[4]) +   1.0283934866 * highInAL[3]
			+ ( -0.2828214615 * highOutAL[2]) + (  1.0195930909 * highOutAL[3])
			+ ( -1.9633013869 * highOutAL[4]) + (  2.1020162751 * highOutAL[5]);
			bridgerectifier = fabs(highOutAL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutAL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//high section L
			highInAR[0] = highInAR[1]; highInAR[1] = highInAR[2]; highInAR[2] = highInAR[3];
			highInAR[3] = highInAR[4]; highInAR[4] = highInAR[5]; highInAR[5] = highInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {highInAR[6] = bridgerectifier;}
			else {highInAR[6] = -bridgerectifier;}
			highInAR[6] *= high; highOutAR[2] = highOutAR[3];
			highOutAR[3] = highOutAR[4]; highOutAR[4] = highOutAR[5]; highOutAR[5] = highOutAR[6]; 
			highOutAR[6] = (highInAR[0] + highInAR[6]) -   0.5141967433 * (highInAR[1] + highInAR[5]) 
			- (highInAR[2] + highInAR[4]) +   1.0283934866 * highInAR[3]
			+ ( -0.2828214615 * highOutAR[2]) + (  1.0195930909 * highOutAR[3])
			+ ( -1.9633013869 * highOutAR[4]) + (  2.1020162751 * highOutAR[5]);
			bridgerectifier = fabs(highOutAR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutAR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//high section R
			
			midInAL[0] = midInAL[1]; midInAL[1] = midInAL[2]; midInAL[2] = midInAL[3];
			midInAL[3] = midInAL[4]; midInAL[4] = midInAL[5]; midInAL[5] = midInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {midInAL[6] = bridgerectifier;}
			else {midInAL[6] = -bridgerectifier;}
			midInAL[6] *= mid; midOutAL[2] = midOutAL[3];
			midOutAL[3] = midOutAL[4]; midOutAL[4] = midOutAL[5]; midOutAL[5] = midOutAL[6]; 
			midOutAL[6] = (midInAL[0] + midInAL[6]) - 1.1790257790 * (midInAL[1] + midInAL[5]) 
			- (midInAL[2] + midInAL[4]) + 2.3580515580 * midInAL[3]
			+ ( -0.6292082828 * midOutAL[2]) + (  2.7785843605 * midOutAL[3])
			+ ( -4.6638295236 * midOutAL[4]) + (  3.5142515802 * midOutAL[5]);
			sumL += midOutAL[6];
			//mid section L
			midInAR[0] = midInAR[1]; midInAR[1] = midInAR[2]; midInAR[2] = midInAR[3];
			midInAR[3] = midInAR[4]; midInAR[4] = midInAR[5]; midInAR[5] = midInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {midInAR[6] = bridgerectifier;}
			else {midInAR[6] = -bridgerectifier;}
			midInAR[6] *= mid; midOutAR[2] = midOutAR[3];
			midOutAR[3] = midOutAR[4]; midOutAR[4] = midOutAR[5]; midOutAR[5] = midOutAR[6]; 
			midOutAR[6] = (midInAR[0] + midInAR[6]) - 1.1790257790 * (midInAR[1] + midInAR[5]) 
			- (midInAR[2] + midInAR[4]) + 2.3580515580 * midInAR[3]
			+ ( -0.6292082828 * midOutAR[2]) + (  2.7785843605 * midOutAR[3])
			+ ( -4.6638295236 * midOutAR[4]) + (  3.5142515802 * midOutAR[5]);
			sumR += midOutAR[6];
			//mid section R
			
			lowInAL[0] = lowInAL[1]; lowInAL[1] = lowInAL[2]; lowInAL[2] = lowInAL[3];
			lowInAL[3] = lowInAL[4]; lowInAL[4] = lowInAL[5]; lowInAL[5] = lowInAL[6]; 
			bridgerectifier = fabs(inputSampleL) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {lowInAL[6] = bridgerectifier;}
			else {lowInAL[6] = -bridgerectifier;}
			lowInAL[6] *= low; lowOutAL[2] = lowOutAL[3];
			lowOutAL[3] = lowOutAL[4]; lowOutAL[4] = lowOutAL[5]; lowOutAL[5] = lowOutAL[6]; 
			lowOutAL[6] = (lowInAL[0] + lowInAL[6]) - 1.9193504547 * (lowInAL[1] + lowInAL[5]) 
			- (lowInAL[2] + lowInAL[4]) + 3.8387009093 * lowInAL[3]
			+ ( -0.9195964462 * lowOutAL[2]) + (  3.7538173833 * lowOutAL[3])
			+ ( -5.7487775603 * lowOutAL[4]) + (  3.9145559258 * lowOutAL[5]);
			sumL += lowOutAL[6];
			//low section L
			lowInAR[0] = lowInAR[1]; lowInAR[1] = lowInAR[2]; lowInAR[2] = lowInAR[3];
			lowInAR[3] = lowInAR[4]; lowInAR[4] = lowInAR[5]; lowInAR[5] = lowInAR[6]; 
			bridgerectifier = fabs(inputSampleR) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {lowInAR[6] = bridgerectifier;}
			else {lowInAR[6] = -bridgerectifier;}
			lowInAR[6] *= low; lowOutAR[2] = lowOutAR[3];
			lowOutAR[3] = lowOutAR[4]; lowOutAR[4] = lowOutAR[5]; lowOutAR[5] = lowOutAR[6]; 
			lowOutAR[6] = (lowInAR[0] + lowInAR[6]) - 1.9193504547 * (lowInAR[1] + lowInAR[5]) 
			- (lowInAR[2] + lowInAR[4]) + 3.8387009093 * lowInAR[3]
			+ ( -0.9195964462 * lowOutAR[2]) + (  3.7538173833 * lowOutAR[3])
			+ ( -5.7487775603 * lowOutAR[4]) + (  3.9145559258 * lowOutAR[5]);
			sumR += lowOutAR[6];
			//low section R
		}
		else
		{
			presenceInBL[0] = presenceInBL[1]; presenceInBL[1] = presenceInBL[2]; presenceInBL[2] = presenceInBL[3];
			presenceInBL[3] = presenceInBL[4]; presenceInBL[4] = presenceInBL[5]; presenceInBL[5] = presenceInBL[6]; 
			presenceInBL[6] = inputSampleL * presence; presenceOutBL[2] = presenceOutBL[3];
			presenceOutBL[3] = presenceOutBL[4]; presenceOutBL[4] = presenceOutBL[5]; presenceOutBL[5] = presenceOutBL[6]; 
			presenceOutBL[6] = (presenceInBL[0] + presenceInBL[6]) + 1.9152966321 * (presenceInBL[1] + presenceInBL[5]) 
			- (presenceInBL[2] + presenceInBL[4]) - 3.8305932641 * presenceInBL[3]
			+ ( -0.2828214615 * presenceOutBL[2]) + (  0.2613069963 * presenceOutBL[3])
			+ ( -0.8628193852 * presenceOutBL[4]) + (  0.5387164389 * presenceOutBL[5]);
			bridgerectifier = fabs(presenceOutBL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutBL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//presence section L
			presenceInBR[0] = presenceInBR[1]; presenceInBR[1] = presenceInBR[2]; presenceInBR[2] = presenceInBR[3];
			presenceInBR[3] = presenceInBR[4]; presenceInBR[4] = presenceInBR[5]; presenceInBR[5] = presenceInBR[6]; 
			presenceInBR[6] = inputSampleR * presence; presenceOutBR[2] = presenceOutBR[3];
			presenceOutBR[3] = presenceOutBR[4]; presenceOutBR[4] = presenceOutBR[5]; presenceOutBR[5] = presenceOutBR[6]; 
			presenceOutBR[6] = (presenceInBR[0] + presenceInBR[6]) + 1.9152966321 * (presenceInBR[1] + presenceInBR[5]) 
			- (presenceInBR[2] + presenceInBR[4]) - 3.8305932641 * presenceInBR[3]
			+ ( -0.2828214615 * presenceOutBR[2]) + (  0.2613069963 * presenceOutBR[3])
			+ ( -0.8628193852 * presenceOutBR[4]) + (  0.5387164389 * presenceOutBR[5]);
			bridgerectifier = fabs(presenceOutBR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (presenceOutBR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//presence section R
			
			highInBL[0] = highInBL[1]; highInBL[1] = highInBL[2]; highInBL[2] = highInBL[3];
			highInBL[3] = highInBL[4]; highInBL[4] = highInBL[5]; highInBL[5] = highInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {highInBL[6] = bridgerectifier;}
			else {highInBL[6] = -bridgerectifier;}
			highInBL[6] *= high; highOutBL[2] = highOutBL[3];
			highOutBL[3] = highOutBL[4]; highOutBL[4] = highOutBL[5]; highOutBL[5] = highOutBL[6]; 
			highOutBL[6] = (highInBL[0] + highInBL[6]) -   0.5141967433 * (highInBL[1] + highInBL[5]) 
			- (highInBL[2] + highInBL[4]) +   1.0283934866 * highInBL[3]
			+ ( -0.2828214615 * highOutBL[2]) + (  1.0195930909 * highOutBL[3])
			+ ( -1.9633013869 * highOutBL[4]) + (  2.1020162751 * highOutBL[5]);
			bridgerectifier = fabs(highOutBL[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutBL[6] > 0.0){sumL += bridgerectifier;}
			else {sumL -= bridgerectifier;}			
			//high section L
			highInBR[0] = highInBR[1]; highInBR[1] = highInBR[2]; highInBR[2] = highInBR[3];
			highInBR[3] = highInBR[4]; highInBR[4] = highInBR[5]; highInBR[5] = highInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * high;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {highInBR[6] = bridgerectifier;}
			else {highInBR[6] = -bridgerectifier;}
			highInBR[6] *= high; highOutBR[2] = highOutBR[3];
			highOutBR[3] = highOutBR[4]; highOutBR[4] = highOutBR[5]; highOutBR[5] = highOutBR[6]; 
			highOutBR[6] = (highInBR[0] + highInBR[6]) -   0.5141967433 * (highInBR[1] + highInBR[5]) 
			- (highInBR[2] + highInBR[4]) +   1.0283934866 * highInBR[3]
			+ ( -0.2828214615 * highOutBR[2]) + (  1.0195930909 * highOutBR[3])
			+ ( -1.9633013869 * highOutBR[4]) + (  2.1020162751 * highOutBR[5]);
			bridgerectifier = fabs(highOutBR[6]);
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			bridgerectifier = sin(bridgerectifier);
			if (highOutBR[6] > 0.0){sumR += bridgerectifier;}
			else {sumR -= bridgerectifier;}			
			//high section R
			
			midInBL[0] = midInBL[1]; midInBL[1] = midInBL[2]; midInBL[2] = midInBL[3];
			midInBL[3] = midInBL[4]; midInBL[4] = midInBL[5]; midInBL[5] = midInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {midInBL[6] = bridgerectifier;}
			else {midInBL[6] = -bridgerectifier;}
			midInBL[6] *= mid; midOutBL[2] = midOutBL[3];
			midOutBL[3] = midOutBL[4]; midOutBL[4] = midOutBL[5]; midOutBL[5] = midOutBL[6]; 
			midOutBL[6] = (midInBL[0] + midInBL[6]) - 1.1790257790 * (midInBL[1] + midInBL[5]) 
			- (midInBL[2] + midInBL[4]) + 2.3580515580 * midInBL[3]
			+ ( -0.6292082828 * midOutBL[2]) + (  2.7785843605 * midOutBL[3])
			+ ( -4.6638295236 * midOutBL[4]) + (  3.5142515802 * midOutBL[5]);
			sumL += midOutBL[6];
			//mid section L
			midInBR[0] = midInBR[1]; midInBR[1] = midInBR[2]; midInBR[2] = midInBR[3];
			midInBR[3] = midInBR[4]; midInBR[4] = midInBR[5]; midInBR[5] = midInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * mid;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {midInBR[6] = bridgerectifier;}
			else {midInBR[6] = -bridgerectifier;}
			midInBR[6] *= mid; midOutBR[2] = midOutBR[3];
			midOutBR[3] = midOutBR[4]; midOutBR[4] = midOutBR[5]; midOutBR[5] = midOutBR[6]; 
			midOutBR[6] = (midInBR[0] + midInBR[6]) - 1.1790257790 * (midInBR[1] + midInBR[5]) 
			- (midInBR[2] + midInBR[4]) + 2.3580515580 * midInBR[3]
			+ ( -0.6292082828 * midOutBR[2]) + (  2.7785843605 * midOutBR[3])
			+ ( -4.6638295236 * midOutBR[4]) + (  3.5142515802 * midOutBR[5]);
			sumR += midOutBR[6];
			//mid section R
			
			lowInBL[0] = lowInBL[1]; lowInBL[1] = lowInBL[2]; lowInBL[2] = lowInBL[3];
			lowInBL[3] = lowInBL[4]; lowInBL[4] = lowInBL[5]; lowInBL[5] = lowInBL[6]; 
			bridgerectifier = fabs(inputSampleL) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleL > 0.0) {lowInBL[6] = bridgerectifier;}
			else {lowInBL[6] = -bridgerectifier;}
			lowInBL[6] *= low; lowOutBL[2] = lowOutBL[3];
			lowOutBL[3] = lowOutBL[4]; lowOutBL[4] = lowOutBL[5]; lowOutBL[5] = lowOutBL[6]; 
			lowOutBL[6] = (lowInBL[0] + lowInBL[6]) - 1.9193504547 * (lowInBL[1] + lowInBL[5]) 
			- (lowInBL[2] + lowInBL[4]) + 3.8387009093 * lowInBL[3]
			+ ( -0.9195964462 * lowOutBL[2]) + (  3.7538173833 * lowOutBL[3])
			+ ( -5.7487775603 * lowOutBL[4]) + (  3.9145559258 * lowOutBL[5]);
			sumL += lowOutBL[6];
			//low section L
			lowInBR[0] = lowInBR[1]; lowInBR[1] = lowInBR[2]; lowInBR[2] = lowInBR[3];
			lowInBR[3] = lowInBR[4]; lowInBR[4] = lowInBR[5]; lowInBR[5] = lowInBR[6]; 
			bridgerectifier = fabs(inputSampleR) * low;
			if (bridgerectifier > 1.57079633) {bridgerectifier = 1.57079633;}
			bridgerectifier = sin(bridgerectifier);
			if (inputSampleR > 0.0) {lowInBR[6] = bridgerectifier;}
			else {lowInBR[6] = -bridgerectifier;}
			lowInBR[6] *= low; lowOutBR[2] = lowOutBR[3];
			lowOutBR[3] = lowOutBR[4]; lowOutBR[4] = lowOutBR[5]; lowOutBR[5] = lowOutBR[6]; 
			lowOutBR[6] = (lowInBR[0] + lowInBR[6]) - 1.9193504547 * (lowInBR[1] + lowInBR[5]) 
			- (lowInBR[2] + lowInBR[4]) + 3.8387009093 * lowInBR[3]
			+ ( -0.9195964462 * lowOutBR[2]) + (  3.7538173833 * lowOutBR[3])
			+ ( -5.7487775603 * lowOutBR[4]) + (  3.9145559258 * lowOutBR[5]);
			sumR += lowOutBR[6];
			//low section R
		}
		
		inputSampleL = fabs(sumL) * drive;
		if (inputSampleL > 1.57079633) {inputSampleL = 1.57079633;}
		inputSampleL = sin(inputSampleL);
		if (sumL < 0) inputSampleL = -inputSampleL;
		//output L
		inputSampleR = fabs(sumR) * drive;
		if (inputSampleR > 1.57079633) {inputSampleR = 1.57079633;}
		inputSampleR = sin(inputSampleR);
		if (sumR < 0) inputSampleR = -inputSampleR;
		//output R
		
		flip = !flip;
		
		//begin 64 bit stereo floating point dither
		int expon; frexp((double)inputSampleL, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		frexp((double)inputSampleR, &expon);
		fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
		inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}


} // end namespace BassDrive

