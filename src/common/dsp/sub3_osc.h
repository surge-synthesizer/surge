#include "storage.h"
#include "dsputils.h"
#include <vt_dsp/lipol.h>
#include "biquadunit.h"

#if PPC
vector float vec_loadAndSplatScalar( float *scalarPtr );
#endif

class oscillator
{
public:
	_MM_ALIGN16 float output[block_size_os],outputR[block_size_os];	
	oscillator(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual ~oscillator();
	virtual void init(float pitch, bool is_display = false) {};
	virtual void init_ctrltypes() {};
	virtual void init_default_values() {};
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f) {}
	virtual void assign_fm(float *master_osc){ this->master_osc = master_osc; }
	virtual bool allow_display(){ return true; }
	inline float pitch_to_omega(float x)
	{
		return (float)(M_PI * (16.35159783) * note_to_pitch(x) * dsamplerate_os_inv);
	}	
protected:	
	sub3_storage *storage;
	sub3_osc *oscdata;
	pdata *localcopy;
	float * __restrict master_osc;	
	float drift;
	int ticker;
};

class osc_sine : public oscillator
{
public:
	osc_sine(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_sine();
	quadr_osc sinus;	
	double phase;
	float driftlfo,driftlfo2;
	lag<double> FMdepth;		
};

class osc_FM : public oscillator
{
public:
	osc_FM(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_FM();
	virtual void init_ctrltypes();
	virtual void init_default_values();	
	double phase,lastoutput;
	quadr_osc RM1,RM2,AM;
	float driftlfo,driftlfo2;	
	lag<double> FMdepth,AbsModDepth,RelModDepth1,RelModDepth2,FeedbackDepth;	
};

class osc_FM2 : public oscillator
{
public:
	osc_FM2(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_FM2();
	virtual void init_ctrltypes();
	virtual void init_default_values();	
	double phase,lastoutput;
	quadr_osc RM1,RM2;
	float driftlfo,driftlfo2;	
	lag<double> FMdepth,RelModDepth1,RelModDepth2,FeedbackDepth,PhaseOffset;	
};

class osc_audioinput : public oscillator
{
public:
	osc_audioinput(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_audioinput();
	virtual void init_ctrltypes();
	virtual void init_default_values();
	virtual bool allow_display(){ return false; }	
};

class oscillator_BLIT : public oscillator
{
public:
	oscillator_BLIT(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
protected:	
	_MM_ALIGN16 float oscbuffer[ob_length+FIRipol_N];
	_MM_ALIGN16 float oscbufferR[ob_length+FIRipol_N];
	_MM_ALIGN16 float dcbuffer[ob_length+FIRipol_N];	
#if PPC
	float osc_out, osc_out2, osc_outR, osc_out2R;
#else
	__m128 osc_out, osc_out2, osc_outR, osc_out2R;
#endif
	void prepare_unison(int voices);
	float integrator_hpf;
	float pitchmult,pitchmult_inv;
	int n_unison;
	int bufpos;
	float out_attenuation,out_attenuation_inv,detune_bias,detune_offset;	
	float oscstate[max_unison],syncstate[max_unison],rate[max_unison];		
	float driftlfo[max_unison],driftlfo2[max_unison];
	float panL[max_unison],panR[max_unison];
	int state[max_unison];
};

class osc_super : public oscillator_BLIT
{
private:	
	lipol_ps li_hpf,li_DC,li_integratormult;
	_MM_ALIGN16 float FMphase[block_size_os + 4];
public:
	osc_super(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);
	virtual void init_ctrltypes();	
	virtual void init_default_values();
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	template<bool FM> void convolute(int voice, bool stereo);
	virtual ~osc_super();
private:
	bool first_run;		
	float dc,dc_uni[max_unison],elapsed_time[max_unison],last_level[max_unison],pwidth[max_unison],pwidth2[max_unison];
	template<bool is_init> void update_lagvals();	
	float pitch;	
	lag<float> FMdepth,integrator_mult,l_pw,l_pw2,l_shape,l_sub,l_sync;	
	int id_pw,id_pw2,id_shape,id_smooth,id_sub,id_sync,id_detune;	
	int FMdelay;
	float FMmul_inv;	
	float CoefB0, CoefB1, CoefA1;
};

class osc_pluck : public oscillator
{
public:
	osc_pluck(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void process_block(float pitch, float drift);	
	virtual void process_block_fm(float pitch,float depth, float drift);
	void process_blockT(float pitch, bool FM, float depth, float drift=0);
	virtual ~osc_pluck();
};
 
class osc_snh : public oscillator_BLIT
{
private:
	lipol_ps li_hpf,li_DC,li_integratormult;
public:
	osc_snh(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);
	virtual void init_ctrltypes();	
	virtual void init_default_values();
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_snh();
private:
	void convolute(int voice, bool FM, bool stereo);
	template<bool FM> void process_blockT(float pitch,float depth, float drift=0);
	template<bool is_init> void update_lagvals();
	bool first_run;	
	float dc,dc_uni[max_unison],elapsed_time[max_unison],last_level[max_unison],last_level2[max_unison],pwidth[max_unison];		
	float pitch;
	lag<double> FMdepth,hpf_coeff,integrator_mult,l_pw,l_shape,l_smooth,l_sub,l_sync;	
	int id_pw,id_shape,id_smooth,id_sub,id_sync,id_detune;	
	int FMdelay;
	float FMmul_inv;	
};

class osc_wavetable : public oscillator_BLIT
{
public:
	lipol_ps li_hpf,li_DC,li_integratormult;
	osc_wavetable(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);
	virtual void init_ctrltypes();	
	virtual void init_default_values();
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_wavetable();
private:
	void convolute(int voice, bool FM, bool stereo);	
	template<bool is_init> void update_lagvals();	
	__forceinline float distort_level(float);
	bool first_run;	
	float oscpitch[max_unison];	
	float dc,dc_uni[max_unison],last_level[max_unison];		
	float pitch;	
	int mipmap[max_unison],mipmap_ofs[max_unison];	
	lag<float> FMdepth,hpf_coeff,integrator_mult,l_hskew,l_vskew,l_clip,l_shape;
	float formant_t,formant_last,pitch_last,pitch_t;
	float tableipol,last_tableipol;	
	float hskew,last_hskew;
	int id_shape,id_vskew,id_hskew,id_clip,id_detune,id_formant,tableid,last_tableid;	
	int FMdelay;
	float FMmul_inv;	
	int sampleloop;
	//biquadunit FMfilter;
	//float wavetable[wavetable_steps];
};

const int wt2_suboscs = 8;

class osc_WT2 : public oscillator
{
private:
	_MM_ALIGN16 int IOutputL[block_size_os];
	_MM_ALIGN16 int IOutputR[block_size_os];
	_MM_ALIGN16 struct 
	{
		unsigned int Pos[wt2_suboscs];
		unsigned int SubPos[wt2_suboscs];
		unsigned int Ratio[wt2_suboscs];
		unsigned int Table[wt2_suboscs];
		unsigned int FormantMul[wt2_suboscs];
		unsigned int DispatchDelay[wt2_suboscs];	// samples until playback should start (for per-sample scheduling)
		unsigned char Gain[wt2_suboscs][2];
		float DriftLFO[wt2_suboscs][2];
	} Sub;	
public:
	osc_WT2(sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);
	virtual void init(float pitch, bool is_display=false);	
	virtual void init_ctrltypes();	
	virtual void init_default_values();
	virtual void process_block(float pitch, float drift=0.f, bool stereo=false, bool FM=false, float FMdepth=0.f);
	virtual ~osc_WT2();
private:
	void ProcessSubOscs(bool);
	float OutAttenuation;
	float DetuneBias, DetuneOffset;
	int ActiveSubOscs;
};


oscillator* spawn_osc(int osctype, sub3_storage *storage, sub3_osc *oscdata, pdata *localcopy);