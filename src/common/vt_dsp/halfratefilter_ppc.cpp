#if PPC

#include "halfratefilter.h"
#include "assert.h"

const unsigned int hr_block_size = 256;
const vFloat half = (vFloat)0.5f;
const vFloat zero = (vFloat)0.0f;

halfrate_stereo::halfrate_stereo(int M, bool steep)
{
	assert(!(M>halfrate_max_M));
	this->M = M;
	this->steep = steep;
	load_coefficients();
	reset();
}

void halfrate_stereo::process_block(float *L, float *R, int nsamples)
{
	vUInt8 sel_r0 = (vUInt8) (0x00,0x01,0x02,0x03,0x00,0x01,0x02,0x03,0x10,0x11,0x12,0x13,0x10,0x11,0x12,0x13);
	vUInt8 sel_r1 = (vUInt8) (0x04,0x05,0x06,0x07,0x04,0x05,0x06,0x07,0x14,0x15,0x16,0x17,0x14,0x15,0x16,0x17);
	vUInt8 sel_r2 = (vUInt8) (0x08,0x09,0x0a,0x0b,0x08,0x09,0x0a,0x0b,0x18,0x19,0x1a,0x1b,0x18,0x19,0x1a,0x1b);
	vUInt8 sel_r3 = (vUInt8) (0x0c,0x0d,0x0e,0x0f,0x0c,0x0d,0x0e,0x0f,0x1c,0x1d,0x1e,0x1f,0x1c,0x1d,0x1e,0x1f);
	vFloat o[hr_block_size];			
	// fill the buffer with interleaved stereo samples
	for(unsigned int k=0; k<nsamples; k+=4)
	{
		//[o3,o2,o1,o0] = [L0,L0,R0,R0]
		vFloat LD = vec_ld(k<<2,L);
		vFloat RD = vec_ld(k<<2,R);
		o[k] = vec_perm(LD,RD,sel_r0);
		o[k+1] = vec_perm(LD,RD,sel_r1);
		o[k+2] = vec_perm(LD,RD,sel_r2);
		o[k+3] = vec_perm(LD,RD,sel_r3);
	}		
	
	// process filters
	for(unsigned int j=0; j<M; j++)
	{
		vFloat tx0 = vx0[j];
		vFloat tx1 = vx1[j];
		vFloat tx2 = vx2[j];
		vFloat ty0 = vy0[j];
		vFloat ty1 = vy1[j];
		vFloat ty2 = vy2[j];
		vFloat ta = va[j];
		
		for(unsigned int k=0; k<nsamples; k+=2)
		{
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k] = ty0;
			
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k+1];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k+1] = ty0;
		}
		vx0[j] = tx0;
		vx1[j] = tx1;
		vx2[j] = tx2;
		vy0[j] = ty0;
		vy1[j] = ty1;
		vy2[j] = ty2;					
	}	
	
	float *fo = (float*)o;
	
	for(unsigned int k=0; k<nsamples; k++)
	{
		//	const double output=(filter_a.process(input)+oldout)*0.5;
		//	oldout=filter_b.process(input);
		
		L[k] = 0.5f * (fo[k<<2] + oldoutL);
		oldoutL = fo[1 + (k<<2)];
		R[k] = 0.5f * (fo[2 + (k<<2)] + oldoutR);
		oldoutR = fo[3 + (k<<2)];
	}
}

void halfrate_stereo::process_block_D2(float *L, float *R, int nsamples, float* outL, float* outR)
{
	vUInt8 sel_r0 = (vUInt8) (0x00,0x01,0x02,0x03,0x00,0x01,0x02,0x03,0x10,0x11,0x12,0x13,0x10,0x11,0x12,0x13);
	vUInt8 sel_r1 = (vUInt8) (0x04,0x05,0x06,0x07,0x04,0x05,0x06,0x07,0x14,0x15,0x16,0x17,0x14,0x15,0x16,0x17);
	vUInt8 sel_r2 = (vUInt8) (0x08,0x09,0x0a,0x0b,0x08,0x09,0x0a,0x0b,0x18,0x19,0x1a,0x1b,0x18,0x19,0x1a,0x1b);
	vUInt8 sel_r3 = (vUInt8) (0x0c,0x0d,0x0e,0x0f,0x0c,0x0d,0x0e,0x0f,0x1c,0x1d,0x1e,0x1f,0x1c,0x1d,0x1e,0x1f);
	vFloat o[hr_block_size];			
	// fill the buffer with interleaved stereo samples
	for(unsigned int k=0; k<nsamples; k+=4)
	{
		//[o3,o2,o1,o0] = [L0,L0,R0,R0]
		vFloat LD = vec_ld(k<<2,L);
		vFloat RD = vec_ld(k<<2,R);
		o[k] = vec_perm(LD,RD,sel_r0);
		o[k+1] = vec_perm(LD,RD,sel_r1);
		o[k+2] = vec_perm(LD,RD,sel_r2);
		o[k+3] = vec_perm(LD,RD,sel_r3);
	}		
	
	// process filters
	for(unsigned int j=0; j<M; j++)
	{
		vFloat tx0 = vx0[j];
		vFloat tx1 = vx1[j];
		vFloat tx2 = vx2[j];
		vFloat ty0 = vy0[j];
		vFloat ty1 = vy1[j];
		vFloat ty2 = vy2[j];
		vFloat ta = va[j];
		
		for(unsigned int k=0; k<nsamples; k+=2)
		{
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k] = ty0;
			
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k+1];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k+1] = ty0;
		}
		vx0[j] = tx0;
		vx1[j] = tx1;
		vx2[j] = tx2;
		vy0[j] = ty0;
		vy1[j] = ty1;
		vy2[j] = ty2;					
	}	
	
	float *fo = (float*)o;
	
	if (outL) L = outL;
	if (outR) R = outR; 
	
	for(unsigned int k=0; k<nsamples; k+=2)
	{
		
		L[k>>1] = 0.5f * (fo[(k<<2)] + oldoutL);			
		R[k>>1] = 0.5f * (fo[2 + (k<<2)] + oldoutR);
		oldoutL = fo[5 + (k<<2)];
		oldoutR = fo[7 + (k<<2)];
	}
}

void halfrate_stereo::process_block_U2(float *L_in, float *R_in, float *L, float *R, int nsamples)
{			
	vUInt8 sel_r0 = (vUInt8) (0x00,0x01,0x02,0x03,0x00,0x01,0x02,0x03,0x10,0x11,0x12,0x13,0x10,0x11,0x12,0x13);
	vUInt8 sel_r1 = (vUInt8) (0x04,0x05,0x06,0x07,0x04,0x05,0x06,0x07,0x14,0x15,0x16,0x17,0x14,0x15,0x16,0x17);
	vUInt8 sel_r2 = (vUInt8) (0x08,0x09,0x0a,0x0b,0x08,0x09,0x0a,0x0b,0x18,0x19,0x1a,0x1b,0x18,0x19,0x1a,0x1b);
	vUInt8 sel_r3 = (vUInt8) (0x0c,0x0d,0x0e,0x0f,0x0c,0x0d,0x0e,0x0f,0x1c,0x1d,0x1e,0x1f,0x1c,0x1d,0x1e,0x1f);
	vFloat o[hr_block_size];			
	// fill the buffer with interleaved stereo samples
	for(unsigned int k=0; k<nsamples; k+=8)
	{
		//[o3,o2,o1,o0] = [L0,L0,R0,R0]
		vFloat LD = vec_ld(k<<2,L_in);
		vFloat RD = vec_ld(k<<2,R_in);
		o[k] = vec_perm(LD,RD,sel_r0);
		o[k+1] = zero;
		o[k+2] = vec_perm(LD,RD,sel_r1);
		o[k+3] = zero;
		o[k+4] = vec_perm(LD,RD,sel_r2);
		o[k+5] = zero;
		o[k+6] = vec_perm(LD,RD,sel_r3);
		o[k+7] = zero;
	}		
	
	// process filters
	for(unsigned int j=0; j<M; j++)
	{
		vFloat tx0 = vx0[j];
		vFloat tx1 = vx1[j];
		vFloat tx2 = vx2[j];
		vFloat ty0 = vy0[j];
		vFloat ty1 = vy1[j];
		vFloat ty2 = vy2[j];
		vFloat ta = va[j];
		
		for(unsigned int k=0; k<nsamples; k+=2)
		{
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k] = ty0;
			
			//shuffle inputs
			tx2=tx1;
			tx1=tx0;
			tx0=o[k+1];
			//shuffle outputs
			ty2=ty1;
			ty1=ty0;		
			//allpass filter 1
			ty0 = vec_madd(vec_sub(tx0,ty2),ta,tx2);
			o[k+1] = ty0;
		}
		vx0[j] = tx0;
		vx1[j] = tx1;
		vx2[j] = tx2;
		vy0[j] = ty0;
		vy1[j] = ty1;
		vy2[j] = ty2;					
	}	
	
	float *fo = (float*)o;
	
	for(unsigned int k=0; k<nsamples; k++)
	{
		//	const double output=(filter_a.process(input)+oldout)*0.5;
		//	oldout=filter_b.process(input);
		
		L[k] = 0.5f * (fo[k<<2] + oldoutL);
		oldoutL = fo[1 + (k<<2)];
		R[k] = 0.5f * (fo[2 + (k<<2)] + oldoutR);
		oldoutR = fo[3 + (k<<2)];
	}
}

void halfrate_stereo::reset()
{
	for(unsigned int i=0; i<M; i++)
	{
		vx0[i] = zero;
		vx1[i] = zero;
		vx2[i] = zero;
		vy0[i] = zero;
		vy1[i] = zero;
		vy2[i] = zero;			
	}
	oldoutL = 0.f;
	oldoutR = 0.f;
}

void halfrate_stereo::set_coefficients(float *cA, float *cB)	
{
	float *fva = (float*)va;
	for(unsigned int i=0; i<M; i++)
	{
		
		fva[(i<<2)] = cA[i];
		fva[(i<<2)+1] = cB[i];
		fva[(i<<2)+2] = cA[i];
		fva[(i<<2)+3] = cB[i];
	}
}

void halfrate_stereo::load_coefficients()
{		
	int order = M<<1;
	if (steep)
	{
		if (order==12)	//rejection=104dB, transition band=0.01
		{
			float a_coefficients[6]=
		{0.036681502163648017f
			,0.2746317593794541f
			,0.56109896978791948f
			,0.769741833862266f
			,0.8922608180038789f
			,0.962094548378084f
		};
			
			float b_coefficients[6]=
			{0.13654762463195771f
				,0.42313861743656667f
				,0.6775400499741616f
				,0.839889624849638f
				,0.9315419599631839f
				,0.9878163707328971f
			};
			
			set_coefficients(a_coefficients,b_coefficients);			
		}
		else if (order==10)	//rejection=86dB, transition band=0.01
		{
			float a_coefficients[5]=
		{0.051457617441190984f
			,0.35978656070567017f
			,0.6725475931034693f
			,0.8590884928249939f
			,0.9540209867860787f
		};
			
			float b_coefficients[5]=
			{0.18621906251989334f
				,0.529951372847964f
				,0.7810257527489514f
				,0.9141815687605308f
				,0.985475023014907f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==8)	//rejection=69dB, transition band=0.01
		{
			float a_coefficients[4]=
		{0.07711507983241622f
			,0.4820706250610472f
			,0.7968204713315797f
			,0.9412514277740471f
		};
			
			float b_coefficients[4]=
			{0.2659685265210946f
				,0.6651041532634957f
				,0.8841015085506159f
				,0.9820054141886075f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==6)	//rejection=51dB, transition band=0.01
		{
			float a_coefficients[3]=
		{0.1271414136264853f
			,0.6528245886369117f
			,0.9176942834328115f
		};
			
			float b_coefficients[3]=
			{0.40056789819445626f
				,0.8204163891923343f
				,0.9763114515836773f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==4)	//rejection=53dB,transition band=0.05
		{
			float a_coefficients[2]=
		{0.12073211751675449f
			,0.6632020224193995f				};
			
			float b_coefficients[2]=
			{0.3903621872345006f
				,0.890786832653497f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		
		else	//order=2, rejection=36dB, transition band=0.1
		{
			float a_coefficients=0.23647102099689224f;
			float b_coefficients=0.7145421497126001f;
			
			set_coefficients(&a_coefficients,&b_coefficients);
		}
	}
	else	//softer slopes, more attenuation and less stopband ripple
	{
		if (order==12)	//rejection=150dB, transition band=0.05
		{
			float a_coefficients[6]=
		{0.01677466677723562f
			,0.13902148819717805f
			,0.3325011117394731f
			,0.53766105314488f
			,0.7214184024215805f
			,0.8821858402078155f
		};
			
			float b_coefficients[6]=
			{0.06501319274445962f
				,0.23094129990840923f
				,0.4364942348420355f
				,0.06329609551399348f
				,0.80378086794111226f
				,0.9599687404800694f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==10)	//rejection=133dB, transition band=0.05
		{
			float a_coefficients[5]=
		{0.02366831419883467f
			,0.18989476227180174f
			,0.43157318062118555f
			,0.6632020224193995f
			,0.860015542499582f
		};
			
			float b_coefficients[5]=
			{0.09056555904993387f
				,0.3078575723749043f
				,0.5516782402507934f
				,0.7652146863779808f
				,0.95247728378667541f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==8)	//rejection=106dB, transition band=0.05
		{
			float a_coefficients[4]=
		{0.03583278843106211f
			,0.2720401433964576f
			,0.5720571972357003f
			,0.827124761997324f
		};
			
			float b_coefficients[4]=
			{0.1340901419430669f
				,0.4243248712718685f
				,0.7062921421386394f
				,0.9415030941737551f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==6)	//rejection=80dB, transition band=0.05
		{
			float a_coefficients[3]=
		{0.06029739095712437f
			,0.4125907203610563f
			,0.7727156537429234f
		};
			
			float b_coefficients[3]=
			{0.21597144456092948f
				,0.6043586264658363f
				,0.9238861386532906f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		else if (order==4)	//rejection=70dB,transition band=0.1
		{
			float a_coefficients[2]=
		{0.07986642623635751f
			,0.5453536510711322f
		};
			
			float b_coefficients[2]=
			{0.28382934487410993f
				,0.8344118914807379f
			};
			
			set_coefficients(a_coefficients,b_coefficients);
		}
		
		else	//order=2, rejection=36dB, transition band=0.1
		{
			float a_coefficients=0.23647102099689224f;
			float b_coefficients=0.7145421497126001f;
			
			set_coefficients(&a_coefficients,&b_coefficients);
		}	
	}
}		


#endif