//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "dsputils.h"
#include "storage.h"

/*	base class			*/

class baseeffect
{
public:

	enum
	{
		KNumVuSlots = 24
	};

	baseeffect(sub3_storage *storage, sub3_fx *fxdata, pdata* pd);
	virtual ~baseeffect(){ return; }
	
	virtual const char*	get_effectname(){ return 0; }
	
	virtual void init() {};
	virtual void init_ctrltypes();
	virtual void init_default_values() {};
	virtual int vu_type(int id){ return 0; };
	virtual int vu_ypos(int id){ return id; };	// in 'half-hslider' heights
	virtual const char* group_label(int id){ return 0; };
	virtual int group_label_ypos(int id){ return 0; };	
	virtual int get_ringout_decay(){ return -1; }	// number of blocks it takes for the effect to 'ring out'
    		
	virtual void process(float *dataL, float *dataR){ return; }
	virtual void process_only_control(){return;}	// for controllers that should run regardless of the audioprocess
	virtual bool process_ringout(float *dataL, float *dataR, bool indata_present=true);	// returns rtue if outdata is present
	//virtual void processSSE(float *dataL, float *dataR){ return; }
	//virtual void processSSE2(float *dataL, float *dataR){ return; }
	//virtual void processSSE3(float *dataL, float *dataR){ return; }
	//virtual void processT<int architecture>(float *dataL, float *dataR){ return; }
	virtual void suspend(){ return; }
	float vu[KNumVuSlots];	// stereo pairs, just use every other when mono

protected:
	sub3_storage *storage;
	sub3_fx *fxdata;	
	pdata* pd;
	int ringout;
	float *f[n_fx_params];	
};

baseeffect* spawn_effect(int id,sub3_storage *storage,sub3_fx *fxdata, pdata* pd);