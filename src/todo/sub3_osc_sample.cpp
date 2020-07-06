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

osc_sample::osc_sample(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy) : oscillator(storage,oscdata,localcopy)
{	
}

osc_sample::~osc_sample()
{
}

void osc_sample::init(float pitch, bool is_display)
{
	assert(storage);	

	//Init(ialize) here
	//id_shape = oscdata->p[0].param_id_in_scene;

	this->pitch = pitch;
	update_lagvals<true>();
}

void osc_sample::init_ctrltypes()
{
	//oscdata->p[0].set_name("correlation");
	//oscdata->p[0].set_type(ct_percent_bidirectional);
}

void osc_sample::init_default_values()
{
	//oscdata->p[0].val.f = 0.f;
}

template<bool is_init>
void osc_sample::update_lagvals()
{		
	// l_sync.newValue(max(0.f,localcopy[id_sync].f));
	if(is_init)
	{
	}
}

void osc_sample::process_block(float pitch){ process_blockT<false>(pitch,0); }	
void osc_sample::process_block_fm(float pitch,float depth){ process_blockT<true>(pitch,depth); }
template<bool FM> void osc_sample::process_blockT(float pitch,float depth)
{	
	this->pitch = pitch;	
	if (FM) FMdepth.newValue(depth);

	float pitchcorrection = 0;//wave->smpl_chunk.
	resample_ratio = (unsigned int)((double)((wave->sample_rate * storage->dsamplerate_os_inv)*16777216.0*storage->note_to_pitch(pitch-pitchcorrection)));		

	update_lagvals<false>();

	for(int k=0; k<BLOCK_SIZE_OS; k++)
	{	
		uint32 m0 =	((sample_subpos>>16)&0xff)*FIRipol_N;
		float lipol0 = (float)((uint32)(sample_subpos&0xffff));				

	}
}
