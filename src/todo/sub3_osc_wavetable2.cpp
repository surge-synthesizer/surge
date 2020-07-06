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

const float integrator_hpf = 0.9995f;
const float hpf_cycle_loss = 0.99f;

osc_wavetable2::osc_wavetable2(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy) : oscillator(storage,oscdata,localcopy)
{	
}

osc_wavetable2::~osc_wavetable2()
{
}

void osc_wavetable2::init(float pitch, bool is_display)
{
	assert(storage);
	first_run = true;	
	osc_out = _mm_set1_ps(0.f);		
	bufpos = 0;	
	
	// init här
	id_shape = oscdata->p[0].param_id_in_scene;
	id_vskew = oscdata->p[1].param_id_in_scene;	
	id_clip = oscdata->p[2].param_id_in_scene;
	id_formant = oscdata->p[3].param_id_in_scene;
	id_hskew = oscdata->p[4].param_id_in_scene;	
	id_detune = oscdata->p[5].param_id_in_scene;

	float rate = 0.05;
	l_shape.setRate(rate);
	l_clip.setRate(rate);
	l_vskew.setRate(rate);	
	l_hskew.setRate(rate);	

	n_unison = limit_range(oscdata->p[6].val.i,1,MAX_UNISON);	
	if(oscdata->wt.flags & wtf_is_sample)
	{
		sampleloop = n_unison;
		n_unison = 1;

	}
	if(is_display) n_unison = 1;
	out_attenuation = 1.0f/sqrt((float)n_unison);

	if(n_unison == 1)	// Make dynamic baskets (?) later... (gör dynamic honk sen..)
	{
		detune_bias = 1;
		detune_offset = 0;
	} else {
		detune_bias = (float)2/(n_unison);
		detune_offset = -1;
	}		
	memset(oscbuffer,0,sizeof(float)*(OB_LENGTH+FIRipol_N));
	memset(last_level,0,MAX_UNISON*sizeof(float));	

	pitch_last = pitch;
	pitch_t = pitch;	
	update_lagvals<true>();	

	float shape = oscdata->p[0].val.f;
	float intpart;
	shape *= ((float)oscdata->wt.n_tables-1.f)*0.99999f;			
	tableipol = modff(shape,&intpart);			
	tableid = limit_range((int)intpart,0,oscdata->wt.n_tables-2);	
	last_tableipol= tableipol;
	last_tableid = tableid;
	hskew = 0.f;
	last_hskew = 0.f;	
	if(oscdata->wt.flags & wtf_is_sample)
	{
		tableipol = 0.f;
		tableid -= 1;
	}

	int i;
	for(i=0; i<n_unison; i++){				
		if(oscdata->retrigger.val.b || is_display)
		{
			oscstate[i] = 0; 
			state[i] = 0;
		}
		else
		{
			double drand = (double) rand() / RAND_MAX;
			double detune = localcopy[id_detune].f*(detune_bias*float(i) + detune_offset);						
			double ot = drand * storage->note_to_pitch(detune);
			oscstate[i] = ot;			
			state[i] = 0;
		}				
		last_level[i] = 0.0;
		mipmap[i] = 0;
		mipmap_ofs[i] = 0;
		driftlfo2[i] = 0.f;	
		driftlfo[i] = 0.f;	
	}
}

void osc_wavetable2::init_ctrltypes()
{
	oscdata->p[0].set_name("shape");	oscdata->p[0].set_type(ct_percent);
	oscdata->p[1].set_name("skew v");	oscdata->p[1].set_type(ct_percent_bidirectional);
	oscdata->p[2].set_name("saturate");	oscdata->p[2].set_type(ct_percent);
	oscdata->p[3].set_name("formant");	oscdata->p[3].set_type(ct_syncpitch);
	oscdata->p[4].set_name("skew h");	oscdata->p[4].set_type(ct_percent_bidirectional);	
	oscdata->p[5].set_name("osc spread");	oscdata->p[5].set_type(ct_percent);	
	oscdata->p[6].set_name("osc count");	oscdata->p[6].set_type(ct_osccount);	
}
void osc_wavetable2::init_default_values()
{
	oscdata->p[0].val.f = 0.0f;
	oscdata->p[1].val.f = 0.0f;
	oscdata->p[2].val.f = 0.f;
	oscdata->p[3].val.f = 0.f;
	oscdata->p[4].val.f = 0.f;
	oscdata->p[5].val.f = 0.2f;
	oscdata->p[6].val.i = 1;	
}

float osc_wavetable2::distort_level(float x)
{
	float a = l_vskew.v * 0.5;	
	float clip = l_clip.v;
	
	x = x - a*x*x + a; 
	
	//x = limit_range(x*(1+3*clip),-1,1);
	x = limit_range(x*(1-clip) + clip*x*x*x,-1,1);

	return x;
}

void osc_wavetable2::convolute(int voice, bool FM)
{
	float block_pos = oscstate[voice] * BLOCK_SIZE_OS_INV * pitchmult_inv;

	double detune = drift*driftlfo[voice] + localcopy[id_detune].f*(detune_bias*float(voice) + detune_offset);		

	//int ipos = (large+oscstate[voice])>>16;	
	const float p24 = (1<<24);
	unsigned int ipos;
	if(FM) ipos = (unsigned int)((float)p24*(oscstate[voice]*pitchmult_inv*FMmul_inv));		
	else ipos = (unsigned int)((float)p24*(oscstate[voice]*pitchmult_inv));		
	
	if (state[voice] == 0) 
	{
		formant_last = formant_t;
		last_hskew = hskew;
		hskew = l_hskew.v;

		if (oscdata->wt.flags & wtf_is_sample)
		{
			tableid++;
			if (tableid > oscdata->wt.n_tables-3) 
			{				
				if (sampleloop < MAX_UNISON) sampleloop--;
				
				if (sampleloop > 0)
				{					
					tableid = 0;
				}
				else 
				{
					tableid = oscdata->wt.n_tables-2; 
					oscstate[voice] = 100000000000.f; // rather large number
					return;
				}
			}
		}

		
		int ts = oscdata->wt.size;
		float a = oscdata->wt.dt * pitchmult_inv;		

		const float wtbias = 1.8f;

		mipmap[voice] = 0;

		if((a < 0.015625*wtbias)&&(ts>=128)) mipmap[voice] = 6;
		else if((a < 0.03125*wtbias)&&(ts>=64)) mipmap[voice] = 5;
		else if((a < 0.0625*wtbias)&&(ts>=32)) mipmap[voice] = 4;
		else if((a < 0.125*wtbias)&&(ts>=16)) mipmap[voice] = 3;
		else if((a < 0.25*wtbias)&&(ts>=8)) mipmap[voice] = 2;
		else if((a < 0.5*wtbias)&&(ts>=4)) mipmap[voice] = 1;

		//wt_inc = (1<<mipmap[i]);
		mipmap_ofs[voice] = 0;
		for(int i=0; i<mipmap[voice]; i++) mipmap_ofs[voice] += (ts>>i);
	}
	
	// generate pulse
	unsigned int delay = ((ipos>>24)&0x3f);
	if(FM) delay = FMdelay;
	unsigned int m = ((ipos>>16)&0xff)*(FIRipol_N<<1);
	unsigned int lipolui16 = (ipos&0xffff);
	float lipol = ((float)((unsigned int)(lipolui16)));
	__m128 lipol128 = _mm_cvtsi32_ss(lipol128,lipolui16);
	lipol128 = _mm_shuffle_ps(lipol128,lipol128,_MM_SHUFFLE(0,0,0,0));

	int k;			
	const float s = 0.99952f;

	float g;
	int wt_inc = (1<<mipmap[voice]);
	int wt_ofs = mipmap_ofs[voice];
	float dt = (oscdata->wt.dt)*wt_inc;	

	// add time until next statechange				
	float tempt = storage->note_to_pitch_inv(detune);
	float t;
	float xt = ((float)state[voice] + 0.5f) * dt;
	//xt = (1 - hskew + 2*hskew*xt);	
	//xt = (1 + hskew *sin(xt*2.0*M_PI));
	// 1 + a.*(1 - 2.*x + (2.*x-1).^3).*sqrt(27/4) = 1 + 4*x*a*(x-1)*(2x-1)	
	const float taylorscale = sqrt((float)27.f/4.f);
	xt = 1 + hskew * 4*xt*(xt-1)*(2*xt-1)*taylorscale;

	//t = dt * tempt;
	/*while (t<0.5 && (wt_inc < wavetable_steps))
	{
		wt_inc = wt_inc << 1;
		t = dt * tempt * wt_inc;
	}	*/		
	float ft = block_pos*formant_t + (1.f-block_pos)*formant_last;
	float formant = storage->note_to_pitch(-ft);	
	dt *= formant*xt;
	
	//if(state[voice] >= (oscdata->wt.size-wt_inc)) dt += (1-formant);
	if(state[voice] >= (oscdata->wt.size>>mipmap[voice])-1) dt += (1-formant);
	t = dt * tempt;		
	
	float tblip_ipol = (1-block_pos)*last_tableipol + block_pos*tableipol;	
	float newlevel = distort_level( oscdata->wt.table[tableid][wt_ofs+state[voice]]*(1.f-tblip_ipol) + oscdata->wt.table[tableid+1][wt_ofs+state[voice]]*tblip_ipol );	
		
	g = newlevel - last_level[voice];
	last_level[voice] = newlevel;

	g *= out_attenuation;

	__m128 g128 = _mm_load_ss(&g);
	g128 = _mm_shuffle_ps(g128,g128,_MM_SHUFFLE(0,0,0,0));

	for(k=0; k<FIRipol_N; k+=4)
	{
		float *obf = &oscbuffer[bufpos+k+delay];
//		assert((void*)obf < (void*)(storage-3));
		__m128 ob = _mm_loadu_ps(obf);
		__m128 st = _mm_load_ps(&storage->sinctable[m+k]);
		__m128 so = _mm_load_ps(&storage->sinctable[m+k+FIRipol_N]);
		so = _mm_mul_ps(so,lipol128);
		st = _mm_add_ps(st,so);
		st = _mm_mul_ps(st,g128);
		ob = _mm_add_ps(ob,st);
		_mm_storeu_ps(obf,ob);
	}

	rate[voice] = t;		

	oscstate[voice] += rate[voice];	
	oscstate[voice] = max(0.f,oscstate[voice]);
	//state[voice] = (state[voice]+wt_inc)&(oscdata->wt.size-wt_inc);
	state[voice] = (state[voice]+1)&((oscdata->wt.size>>mipmap[voice])-1);
}

template<bool is_init>
void osc_wavetable2::update_lagvals()
{		
	l_vskew.newValue(limit_range(localcopy[id_vskew].f,-1.f,1.f));
	l_hskew.newValue(limit_range(localcopy[id_hskew].f,-1.f,1.f));
	float a = limit_range(localcopy[id_clip].f,0,1.f);
	l_clip.newValue(-8*a*a*a);
	l_shape.newValue(limit_range(localcopy[id_shape].f,0.f,1.f));
	formant_t = max(0.f,localcopy[id_formant].f);
	
	float invt = min(1.0,(8.175798915 * storage->note_to_pitch(pitch_t)) / storage->dsamplerate_os);		
	float hpf2 = min(integrator_hpf,powf(hpf_cycle_loss,4*invt));	// ACHTUNG!/WARNING! Make a lookup-table
		
	hpf_coeff.newValue(hpf2);
	integrator_mult.newValue(invt);	

	li_hpf.set_target(hpf2);

	if(is_init)
	{
		hpf_coeff.instantize();
		integrator_mult.instantize();
		l_shape.instantize();
		l_vskew.instantize();		
		l_hskew.instantize();		
		l_clip.instantize();

		formant_last = formant_t;
	}
}

void osc_wavetable2::process_block(float pitch,float drift){ process_blockT<false>(pitch,0,drift); }	
void osc_wavetable2::process_block_fm(float pitch,float depth,float drift){ process_blockT<true>(pitch,depth,drift); }
template<bool FM> void osc_wavetable2::process_blockT(float pitch0,float depth,float drift)
{			
	pitch_last = pitch_t;
	pitch_t = min(148.f,pitch0);
	pitchmult_inv = max(1.0,storage->dsamplerate_os * (1 / 8.175798915) * storage->note_to_pitch_inv(pitch_t));
	pitchmult = 1.f / pitchmult_inv; // try to hide the latency
	this->drift = drift;
	int k,l;
	
	update_lagvals<false>();	
	l_shape.process();
	l_vskew.process();
	l_hskew.process();
	l_clip.process();	

	if(oscdata->wt.n_tables == 1)
	{
		tableipol = 0.f;
		tableid = 0;
		last_tableid = 0;
		last_tableipol = 0.f;
	}
	else if (oscdata->wt.flags & wtf_is_sample)
	{
		tableipol = 0.f;
		last_tableipol = 0.f;
	}
	else
	{		
		last_tableipol= tableipol;
		last_tableid = tableid;

		float shape = l_shape.v;
		float intpart;
		shape *= ((float)oscdata->wt.n_tables-1.f)*0.99999f;
		tableipol = modff(shape,&intpart);			
		tableid = limit_range((int)intpart,0,oscdata->wt.n_tables-2);				

		if (tableid > last_tableid) 
		{
			if(last_tableipol != 1.f)
			{
				tableid = last_tableid;
				tableipol = 1.f;
			} 
			else last_tableipol = 0.0f;
		}
		else if (tableid < last_tableid)
		{
			if (last_tableipol != 0.f)
			{
				tableid = last_tableid;
				tableipol = 0.f;
			}
			else last_tableipol = 1.0f;
		}
	}

	/*wt_inc = 1;
	float a = oscdata->wt.dt * pitchmult_inv;		
	if(a < 0.125) wt_inc = 8;
	else if(a < 0.25) wt_inc = 4;
	else if(a < 0.5) wt_inc = 2;*/

	if(FM)
	{				
		for(l=0; l<n_unison; l++) driftlfo[l] = drift_noise(driftlfo2[l]);
		for(int s=0; s<BLOCK_SIZE_OS; s++)
		{
			float fmmul = limit_range(1.f + depth*master_osc[s],0.1f,1.9f);			
			float a = pitchmult * fmmul;		
			FMdelay=s;

			for(l=0; l<n_unison; l++)
			{					
				while(oscstate[l] < a) 
				{										
					FMmul_inv = 1.f/fmmul;											
					convolute(l,true);					
				}
				
				oscstate[l] -= a;				
			}			
		}		
	}
	else
	{
		float a = (float)BLOCK_SIZE_OS*pitchmult;
		for(l=0; l<n_unison; l++)
		{	
			driftlfo[l] = drift_noise(driftlfo2[l]);
			while(oscstate[l] < a) convolute(l);
			oscstate[l] -= a;			
		}		
		//li_DC.set_target(dc);	
	}	
		
	float hpfblock alignas(16)[BLOCK_SIZE_OS];
	li_hpf.store_block(hpfblock,BLOCK_SIZE_OS_QUAD);		

	for(k=0; k<BLOCK_SIZE_OS; k++)
	{					
		__m128 hpf = _mm_load_ss(&hpfblock[k]);
		__m128 ob = _mm_load_ss(&oscbuffer[bufpos+k]);			
		__m128 a = _mm_mul_ss(osc_out,hpf);						
		osc_out = _mm_add_ss(a, ob);
		_mm_store_ss(&output[k],osc_out);		
	}	

	vt_dsp::clear_block(&oscbuffer[bufpos],BLOCK_SIZE_OS_QUAD);	

	bufpos = (bufpos+BLOCK_SIZE_OS)&(OB_LENGTH-1);		
	
	// each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around the block edges
	// copy the overlapping samples to the new block position

	if(!bufpos)	// only needed if the new bufpos == 0
	{
		__m128 overlap[FIRipol_N>>2];
		const __m128 zero = _mm_setzero_ps();
		for(k=0; k<(FIRipol_N); k+=4) 
		{
			overlap[k>>2] = _mm_load_ps(&oscbuffer[OB_LENGTH+k]);	
			_mm_store_ps(&oscbuffer[k],overlap[k>>2]);
			_mm_store_ps(&oscbuffer[OB_LENGTH+k],zero);			
		}
	}
}
