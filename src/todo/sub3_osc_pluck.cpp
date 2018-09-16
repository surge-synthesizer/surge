//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "sub3_osc.h"
#include "dsputils.h"

/* sine osc */

osc_pluck::osc_pluck(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy) : oscillator(storage,oscdata,localcopy)
{		
}

void osc_pluck::init(float pitch, bool is_display)
{
}

osc_pluck::~osc_pluck()
{
}

void osc_pluck::process_block(float pitch,float drift)
{		

}

void osc_pluck::process_block_fm(float pitch, float fmdepth,float drift)
{		
	process_block(pitch,drift);
}