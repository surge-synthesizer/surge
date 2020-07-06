/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "sub3_osc.h"
#include "dsputils.h"

const __int64 large = 0x10000000000;
//const float integrator_hpf = 0.99999999f;
//const float integrator_hpf = 0.9992144f;		// 44.1 kHz
//const float integrator_hpf = 0.9964f;		// 44.1 kHz
//const float integrator_hpf = 0.9982f;		// 44.1 kHz	 Magic Moog Frequency
const float integrator_hpf = 0.999f;
// 290 samples to fall 50% (British) (is probably a 2-pole HPF)
// 202 samples (American)  
//const float integrator_hpf = 0.999f;
//pow(ln(0.5)/(samplerate/50hz)
const float hpf_cycle_loss = 0.90f;

const int n_steps = 9;
const float dotwave[2][n_steps][2] = {	{ {0,0}, {0.125,0.5}, {0.25,1}, {0.375,0.5}, {0.5,0}, {0.625,-0.5}, {0.75,-1}, {0.875,-0.5}, {1.0,0} },
										{ {0,0}, {0.0001,1}, {0.25,1}, {0.4999,1}, {0.5,0}, {0.5001,-1}, {0.75,-1}, {0.9999,-1}, {1.0,0} }};

osc_dotwave::osc_dotwave(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy) : oscillator(storage,oscdata,localcopy)
{	
}

osc_dotwave::~osc_dotwave()
{
}

void osc_dotwave::init(float pitch, bool is_display)
{
	assert(storage);
	first_run = true;	
	osc_out = 0;	
	osc_out_2 = 0;
	bufpos = 0;	
	
	// init här
	id_shape = oscdata->p[0].param_id_in_scene;
	id_vskew = oscdata->p[1].param_id_in_scene;	
	id_hskew = oscdata->p[2].param_id_in_scene;
	id_formant = oscdata->p[3].param_id_in_scene;
	id_sync = oscdata->p[4].param_id_in_scene;	
	id_detune = oscdata->p[5].param_id_in_scene;

	n_unison = limit_range(oscdata->p[6].val.i,1,MAX_UNISON);	
	if(is_display) n_unison = 1;
	out_attenuation = 1.0f/sqrt((float)n_unison);

	if(n_unison == 1)	// Make dynamic baskets (?) later.. (gör dynamic honk sen..)
	{
		detune_bias = 1;
		detune_offset = 0;
	} else {
		detune_bias = (float)2/(n_unison);
		detune_offset = -1;
	}	
	memset(oscbuffer,0,sizeof(float)*OB_LENGTH);	
	memset(last_level,0,MAX_UNISON*sizeof(float));
	memset(last_level2,0,MAX_UNISON*sizeof(float));

	this->pitch = pitch;
	update_lagvals<true>();

	int i;
	for(i=0; i<n_unison; i++){				
		if(oscdata->retrigger.val.b || is_display)
		{
			oscstate[i] = 0; 
			syncstate[i] = 0;
		}
		else
		{
			double drand = (double) rand() / RAND_MAX;
			double detune = localcopy[id_detune].f*(detune_bias*float(i) + detune_offset);			
			//double t = drand * max(2.0,storage->dsamplerate_os / (16.35159783 * pow((double)1.05946309435,(double)pitch)));	
			double st = drand * max(2.0,storage->dsamplerate_os / (8.175798915 * storage->note_to_pitch(pitch + detune - 12)));	
			drand = (double) rand() / RAND_MAX;
			double ot = drand * max(2.0,storage->dsamplerate_os / (8.175798915 * storage->note_to_pitch(pitch + detune + l_sync.v)));				
			oscstate[i] = (__int64)(double)(65536.0*16777216.0 * ot);
			syncstate[i] = (__int64)(double)(65536.0*16777216.0 * st);
		}		
		state[i] = 0;
		last_level[i] = 0.0;
	}
}

void osc_dotwave::init_ctrltypes()
{
	oscdata->p[0].set_name("shape");	oscdata->p[0].set_type(ct_percent);
	oscdata->p[1].set_name("skew v");	oscdata->p[1].set_type(ct_percent_bidirectional);
	oscdata->p[2].set_name("skew h");	oscdata->p[2].set_type(ct_percent_bidirectional);
	oscdata->p[3].set_name("formant");	oscdata->p[3].set_type(ct_syncpitch);
	oscdata->p[4].set_name("sync");		oscdata->p[4].set_type(ct_syncpitch);	
	oscdata->p[5].set_name("detune");	oscdata->p[5].set_type(ct_percent);	
	oscdata->p[6].set_name("osc count");	oscdata->p[6].set_type(ct_osccount);	
}
void osc_dotwave::init_default_values()
{
	oscdata->p[0].val.f = 0.1f;
	oscdata->p[1].val.f = 0.0f;
	oscdata->p[2].val.f = 0.f;
	oscdata->p[3].val.f = 0.f;
	oscdata->p[4].val.f = 0.f;
	oscdata->p[5].val.f = 0.2f;
	oscdata->p[6].val.i = 1;	
}

float osc_dotwave::distort_phase(float x)
{
	double skew = l_hskew.v;	
	double formant = storage->note_to_pitch(-l_formant.v);	
	
	x = 2*x - 1;
	if(x>0) x = skew + x*(1-skew);
	else x = skew + x*(1+skew);
	x = x*0.5 + 0.5;

	return x*formant;
}

float osc_dotwave::distort_level(float x)
{
	double skew = l_vskew.v;	

	if(x>0) x = skew + x*(1-skew);
	else x = skew + x*(1+skew);

	return x;
}

template<bool use_sync>
void osc_dotwave::convolute(int voice)
{
	double detune = localcopy[id_detune].f*(detune_bias*float(voice) + detune_offset);		

	int ipos = (large+oscstate[voice])>>16;	
	
	/*if (syncstate[voice]<oscstate[voice])
	{
		ipos = ((large+syncstate[voice])>>16) & 0xFFFFFFFF;;		
		double t = max(0.5,storage->dsamplerate_os / (8.175798915 * storage->note_to_pitch(pitch + detune - 12)));
		__int64 syncrate = (__int64)(double)(65536.0*16777216.0 * t);
		state[voice] = 0;						
		
		oscstate[voice] = syncstate[voice];
		syncstate[voice] += syncrate;
	}*/
	
	// generate pulse
	int m = ((ipos>>16)&0xff)*FIRipol_N;
	float lipol = ((float)((unsigned int)(ipos&0xffff)));

	int k;			
	const float s = 0.99952f;
	// add time until next statechange			
	double t = max(0.5,storage->dsamplerate_os / (8.175798915 * storage->note_to_pitch(pitch + detune + l_sync.v)));	

	float g;	
	double shape = l_shape.v;

	double dt =	distort_phase( (1-shape)*dotwave[0][(state[voice]+1)%n_steps][0] +  (shape)*dotwave[1][(state[voice]+1)%n_steps][0] ) 
				- distort_phase((1-shape)*dotwave[0][state[voice]][0] + (shape)*dotwave[1][state[voice]][0]);	
	if (state[voice] == (n_steps-1))	
	{	
		dt = 1 - distort_phase((1-shape)*dotwave[0][state[voice]][0] + (shape)*dotwave[1][state[voice]][0]);
		
		if (dt <= 0)
		{
			state[voice] = (state[voice]+1)%n_steps;
//			last_level[voice] = 0;
			return;
		}
	}
	dt = max(0.000001,dt);	// Temporary, so it doesn't hang/crash(?) (temp, så den inte hänger sig)
	
	float newlevel = distort_level((1-shape)*dotwave[0][(state[voice]+1) % n_steps][1] + (shape)*dotwave[1][(state[voice]+1) % n_steps][1]);	
	//float newlevel = distort_level((1-shape)*dotwave[0][(state[voice]) % n_steps][1] + (shape)*dotwave[1][(state[voice]) % n_steps][1]);	
				
	//if(state[voice] == 0) pwidth[voice] = l_pw.v;
	
	g = newlevel - last_level[voice];
	last_level[voice] = newlevel;

	float pwmult = 1/dt; //((state[voice] & 1)?pwidth[voice]:(1.0-pwidth[voice]));
	float g2 = g*pwmult - last_level2[voice];
	last_level2[voice] = g*pwmult;	

	float a;
	/*float a = (1-smooth)*(1-smooth);

	g2 = g*a + g2*(1-a);*/
	
	for(k=0; k<FIRipol_N; k++)
	{
		a = storage->sinctable[m+k] + lipol*storage->sincoffset[m+k];		
		oscbuffer[bufpos+k&(OB_LENGTH-1)] += a*g2;
	//	s += a;	
	}	
		
	rate[voice] = (__int64)(double)((65536.0*16777216.0) * t * dt);		

	oscstate[voice] += rate[voice];	
	state[voice] = (state[voice]+1)%n_steps;
}

template<bool is_init>
void osc_dotwave::update_lagvals()
{		
	l_sync.newValue(max(0.f,localcopy[id_sync].f));
	l_vskew.newValue(limit_range(localcopy[id_vskew].f,-0.999f,0.999f));
	l_hskew.newValue(limit_range(localcopy[id_hskew].f,-0.999f,0.999f));
	l_shape.newValue(limit_range(localcopy[id_shape].f,0.f,1.f));
	l_formant.newValue(max(0.f,localcopy[id_formant].f));
	
	float invt = min(1.0,(8.175798915 * storage->note_to_pitch(pitch + l_sync.v)) / storage->dsamplerate_os);		
	float hpf2 = min(integrator_hpf,powf(hpf_cycle_loss,4*invt));
		// ACHTUNG! gör lookup-table
		// ACHTUNG!/WARNING!: Make a lookup table
	
	hpf_coeff.newValue(hpf2);
	integrator_mult.newValue(invt);	

	/*for(int voice=0; voice<n_unison; voice++)
	{
		double detune = localcopy[id_detune].f*(detune_bias*float(voice) + detune_offset);
		double t = max(0.1,storage->dsamplerate_os / (8.175798915 * storage->note_to_pitch(pitch + detune + l_sync.v)));	
		dc_uni[voice] = (1.f/t)*(1+l_shape.v)*(1-l_formant.v);
	}*/

	if(is_init)
	{
		hpf_coeff.instantize();
		integrator_mult.instantize();
		l_shape.instantize();
		l_vskew.instantize();		
		l_hskew.instantize();
		l_formant.instantize();
		l_sync.instantize();
	}
}

void osc_dotwave::process_block(float pitch){ process_blockT<false>(pitch,0); }	
void osc_dotwave::process_block_fm(float pitch,float depth){ process_blockT<true>(pitch,depth); }
template<bool FM> void osc_dotwave::process_blockT(float pitch,float depth)
{	
	this->pitch = pitch;
	int k,l;

	if (FM) FMdepth.newValue(depth);
	update_lagvals<false>();
	
	__int64 largeFM;
	double FMmult;	 	
	
	for(k=0; k<BLOCK_SIZE_OS; k++)
	{	
		hpf_coeff.process();
		integrator_mult.process();
		l_shape.process();
		l_vskew.process();
		l_hskew.process();				
		l_formant.process();
		l_sync.process();
		if (FM) 
		{
 			FMdepth.process();
			FMmult = pow(2.0,4.0 * master_osc[k] * FMdepth.v) - 1.0;
			//FMmult = (double)master_osc[k] * FMdepth.v;
			double dl = (double)large;
			double dt = dl + dl*FMmult;
			//largeFM = (__int64) large + ((double)large * FMmult);
			largeFM = (__int64) dt;			
		}		
		for(l=0; l<n_unison; l++)
		{
			if (FM)
			{
				oscstate[l] -= largeFM;
				if (l_sync.v>0) syncstate[l] -= largeFM;
			}
			else
			{
				oscstate[l] -= large;
				if (l_sync.v>0) syncstate[l] -= large;
			}					

			//while(syncstate[l]<0) this->convolute<true>(l);
			while(oscstate[l]<0) this->convolute<true>(l);			
			
			//elapsed_time[l] += 1.f;
		}		
		


		osc_out = osc_out*hpf_coeff.v + oscbuffer[bufpos];
		osc_out_2 = (osc_out_2*hpf_coeff.v + osc_out);
		output[k] = (osc_out_2*integrator_mult.v) * this->out_attenuation;
		//output[k] = osc_out;
		oscbuffer[bufpos] = 0.f;
		
		bufpos++;
		bufpos = bufpos&(OB_LENGTH-1);			
	}	
}
