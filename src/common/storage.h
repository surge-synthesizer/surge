//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "globals.h"
#include "parameter.h"
#include "modulation.h"
#include "sample.h"
#include "wavetable.h"
#include <vector>
#include <thread/threadsafety.h>
#include <memory>
using namespace std;

#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif
#include <tinyxml.h>

#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::experimental::filesystem;

#if WINDOWS
#define PATH_SEPARATOR '\\'
#else
#define PATH_SEPARATOR '/'
#endif

/* PATCH layer			*/

const int n_oscs = 3;
const int n_lfos_voice = 6;
const int n_lfos_scene = 6;
const int n_lfos = n_lfos_voice + n_lfos_scene;
const int n_osc_params = 7;
const int n_fx_params = 12;
const int FIRipol_M = 256;
const int FIRipol_M_bits = 8;
const int FIRipol_N = 12;
const int FIRoffset = FIRipol_N>>1;
const int FIRipolI16_N = 8;
const int FIRoffsetI16 = FIRipolI16_N>>1;

extern _MM_ALIGN16 float sinctable[(FIRipol_M+1)*FIRipol_N*2];
extern _MM_ALIGN16 float sinctable1X[(FIRipol_M+1)*FIRipol_N];
extern _MM_ALIGN16 short sinctableI16[(FIRipol_M+1)*FIRipolI16_N];
extern _MM_ALIGN16 float table_dB[512],table_pitch[512],table_pitch_inv[512],table_envrate_lpf[512],table_envrate_linear[512];
extern _MM_ALIGN16 float table_note_omega[2][512];	
extern _MM_ALIGN16 float waveshapers[8][1024];
extern float samplerate,samplerate_inv;	
extern double dsamplerate,dsamplerate_inv;
extern double dsamplerate_os,dsamplerate_os_inv;	

const int n_scene_params = 271;
const int n_global_params = 113;
const int n_global_postparams = 1;
const int n_total_params = n_global_params + 2*n_scene_params + n_global_postparams;
const int metaparam_offset = 2048;

enum sub3_scenemode{
	sm_single=0,		
	sm_split,
	sm_dual,	
	n_scenemodes,
};

const char scenemode_abberations[n_scenemodes][16] = { "Single","Split","Dual"};

enum sub3_polymode{
	pm_poly=0,	
	pm_mono,
	pm_mono_st,	
	pm_mono_fp,
	pm_mono_st_fp,
	pm_latch,
	n_polymodes,
};
const char polymode_abberations[n_polymodes][64] = { "Poly","Mono","Mono (Single Trigger EG)","Mono (Fingered Porta)","Mono (S.T. EG + F. Porta)","Latch (Monophonic)"};

enum sub3_lfomode{
	lm_freerun = 0,	
	lm_keytrigger,
	lm_random,
	n_lfomodes,
};
const char lfomode_abberations[n_lfomodes][16] = { "Freerun","Keytrigger","Random"};


enum sub3_charactermode{
	cm_warm = 0,	
	cm_neutral,
	cm_bright,
	n_charactermodes,
};
const char character_abberations[n_charactermodes][16] = { "Warm","Neutral","Bright"};


enum sub3_osctypes{
	ot_classic=0,	
	ot_sinus,	
	ot_wavetable,	
	ot_shnoise,
	ot_audioinput,
	ot_FM,
	ot_FM2,
	ot_WT2,
	num_osctypes,
};
const char osctype_abberations[num_osctypes][16] = { "Classic","Sine","Wavetable","S/H Noise","Audio In","FM3","FM2","Window" };
const char window_abberations[9][16] = { "Triangular","Cosine","Blend 1","Blend 2","Blend 3","Ramp","Sine Cycle","Square Cycle","Rectangular" };

__forceinline bool uses_wavetabledata(int i)
{
	switch(i)
	{
	case ot_wavetable:
	case ot_WT2:
		return true;
	}
	return false;
}

enum sub3_fxtypes{
	fxt_off=0,
	fxt_delay,
	fxt_reverb,
	fxt_phaser,	
	fxt_rotaryspeaker,
	fxt_distortion,
	fxt_eq,
	fxt_freqshift,
	fxt_conditioner,
	fxt_chorus4,
	fxt_vocoder,
//	fxt_emphasize,
   fxt_reverb2,
	num_fxtypes,
};
const char fxtype_abberations[num_fxtypes][16] = { "Off","Delay","Reverb","Phaser","Rotary","Distortion","EQ","Freqshift","Conditioner","Chorus","Vocoder","Reverb2"};

enum fx_bypass
{
	fxb_all_fx=0,	
	fxb_no_sends,
	fxb_scene_fx_only,
	fxb_no_fx,
	n_fx_bypass,
};

const char fxbypass_abberations[n_fx_bypass][16] = { "All FX","No Send FX","Scene FX Only","All FX Off" };

enum fb_configuration
{
	fb_serial=0,
	fb_serial2,
	fb_serial3,
	fb_dual,
	fb_dual2,
	fb_stereo,
	fb_ring,
	fb_wide,
	n_fb_configuration,
};

const char fbc_abberations[n_fb_configuration][16] = { "Serial 1","Serial 2","Serial 3","Dual 1", "Dual 2","Stereo","Ring","Wide" };

enum fm_configuration
{
	fm_off=0,
	fm_2to1,
	fm_3to2to1,
	fm_2and3to1,
	n_fm_configuration,
};

const char fmc_abberations[n_fm_configuration][16] = { "Off","2 > 1","3 > 2 > 1","2 > 1 < 3" };


enum lfoshapes
{
	ls_sine = 0,
	ls_tri,
	ls_square,
	ls_ramp,
	ls_noise,
	ls_snh,
	ls_constant1,
	ls_stepseq,
	n_lfoshapes
};

const char ls_abberations[n_lfoshapes][16] = { "Sine","Triangle", "Square", "Ramp", "Noise", "S&H", "Envelope", "Stepseq" };

enum fu_type
{
	fut_none=0,
	fut_lp12,
	fut_lp24,
	fut_lpmoog,
	fut_hp12,
	fut_hp24,	
	fut_bp12,
	fut_br12,
	fut_comb,
	fut_SNH,
	//fut_comb_neg,
	//fut_apf,
	n_fu_type,
};
const char fut_abberations[n_fu_type][32] = { "Off","Lowpass 12dB","Lowpass 24dB","Lowpass 6-24dB Ladder","Highpass 12dB","Highpass 24dB","Bandpass","Notch","Comb","Sample & Hold"/*,"APF"*/ };
const int fut_subcount[n_fu_type] = {0,3,3,4,3,3,4,2,4,0};

enum fu_subtype
{
	st_SVF = 0,
	st_Rough,	
	st_Smooth,	
	st_Medium,	// disabled
};

enum ws_type
{
	wst_none=0,
	wst_tanh,
	wst_hard,
	wst_asym,
	wst_sinus,
	wst_digi,
	n_ws_type,
};

const char wst_abberations[n_ws_type][16] = { "Off","Soft","Hard","Asymmetric","Sine","Digital" };

enum env_mode_type
{
	emt_digital=0,
	emt_analog,
	n_em_type,
};

const char em_abberations[n_em_type][16] = { "Digital", "Analog" };

struct MidiKeyState
{
   int keystate;
   char lastdetune;
};

struct MidiChannelState
{
   MidiKeyState keyState[128];
   int nrpn[2],nrpn_v[2];
   int rpn[2],rpn_v[2];
   int pitchBend;
   bool nrpn_last;
   bool hold;
   float pan;
   float pitchBendInSemitones;
   float pressure;
   float timbre;
};

struct sub3_osc
{
	parameter type;
	parameter pitch,octave;
	parameter p[n_osc_params];
	parameter keytrack,retrigger;	
	wavetable wt;
	void *queue_xmldata;
	int queue_type;
};

struct sub3_filterunit
{
	parameter type;
	parameter subtype;
	parameter cutoff;
	parameter resonance;
	parameter envmod;
	parameter keytrack;
};

struct sub3_wsunit
{
	parameter type;
	parameter drive;
};

struct sub3_adsr
{
	parameter a,d,s,r;
	parameter a_s,d_s,r_s;
   parameter mode;
};

struct sub3_lfo
{
	parameter rate,shape,start_phase,magnitude, deform;
	parameter trigmode, unipolar;
	parameter delay, hold, attack, decay, sustain, release;
};

struct sub3_fx
{
	parameter type;	
	parameter return_level;
	parameter p[n_fx_params];
};

struct sub3_scene
{
	sub3_osc osc[n_oscs];	
	parameter pitch,octave;
	parameter fm_depth,fm_switch;
	parameter drift,noise_colour,keytrack_root;
	parameter osc_solo;
	parameter level_o1,level_o2,level_o3,level_noise,level_ring_12,level_ring_23,level_pfg;
	parameter mute_o1,mute_o2,mute_o3,mute_noise,mute_ring_12,mute_ring_23;
	parameter solo_o1,solo_o2,solo_o3,solo_noise,solo_ring_12,solo_ring_23;
	parameter route_o1,route_o2,route_o3,route_noise,route_ring_12,route_ring_23;
	parameter vca_level;
	parameter pbrange_dn,pbrange_up;	
	parameter vca_velsense;	
	parameter polymode;
	parameter portamento;
	parameter volume,pan,width;
	parameter send_level[2];
	
	sub3_filterunit filterunit[2];
	parameter f2_cutoff_is_offset,f2_link_resonance;
	sub3_wsunit	wsunit;
	sub3_adsr adsr[2];
	sub3_lfo lfo[n_lfos];
	parameter feedback,filterblock_configuration,filter_balance;
	parameter lowcut;	

	vector<modulation_routing> modulation_scene,modulation_voice;
	vector<modulation_source*> modsources;			
	
	bool modsource_doprocess[n_modsources];
};

const int n_stepseqsteps = 16;
struct sub3_stepsequence
{
	float steps[n_stepseqsteps];
	int loop_start,loop_end;
	float shuffle;
	unsigned int trigmask;
};

class sub3_storage;

class sub3_patch
{
public:
	sub3_patch(sub3_storage *storage);	
	~sub3_patch();
	void init_default_values();
	void update_controls(bool init=false,void* init_osc=0);
	void do_morph();	
	void copy_scenedata(pdata*, int scene);
	void copy_globaldata(pdata*);
	
	// load/save
	//void load_xml();
	//void save_xml();
	void load_xml(const void *data,int size,bool preset);
	unsigned int save_xml(void **data);	
	unsigned int save_RIFF(void **data);

	void load_patch(const void *data,int size,bool preset);
	unsigned int save_patch(void **data);

	// data
	sub3_scene scene[2],morphscene;
	sub3_fx fx[8];
	//char name[namechars];
	int scene_start[2],scene_size;
	parameter scene_active,scenemode,scenemorph,splitkey;
	parameter volume;
	parameter polylimit;
	parameter fx_bypass,fx_disable;
	parameter character;

	sub3_stepsequence stepsequences[2][n_lfos];

	vector<parameter*> param_ptr;
	vector<int> easy_params_id;
	
	vector<modulation_routing> modulation_global;	
	pdata scenedata[2][n_scene_params];
	pdata globaldata[n_global_params];	
	void *patchptr;	
	sub3_storage *storage;

	// metadata	
	string name,category,author,comment;	
	// metaparameters
	char CustomControllerLabel[n_customcontrollers][16];	
};

struct patchlist_entry
{
	string name;
	fs::path path;
	int category;
	bool fav;
};

struct patchlist_category
{
	string name;
};

enum sub3_copysource
{
	cp_off=0,		
	cp_scene,
	cp_osc,
	cp_lfo,
	cp_oscmod,
	n_copysources,
};


/* STORAGE layer			*/

class sub3_storage
{
public:	
	_MM_ALIGN16 float audio_in[2][block_size_os];
	_MM_ALIGN16 float audio_in_nonOS[2][block_size];
//	_MM_ALIGN16 float sincoffset[(FIRipol_M)*FIRipol_N];	// deprecated

	sub3_storage();
	~sub3_storage();
	
	std::unique_ptr<sub3_patch> _patch;
	
	sub3_patch& getPatch();

	float pitch_bend;

	float vu_falloff;
	float temposyncratio,temposyncratio_inv;			// 1.f is 120 BPM 
	double songpos;			
	void init_tables();
	float nyquist_pitch;
	int last_key[2];
	TiXmlElement* getSnapshotSection(const char* name);
	void load_midi_controllers();
	void save_midi_controllers();
	void save_snapshots();	
	int controllers[n_customcontrollers];	
	float poly_aftertouch[2][128];
	float modsource_vu[n_modsources];
	void refresh_wtlist();
	void refresh_patchlist();
	void refreshPatchlistAddDir(bool userDir, string subdir);
	void perform_queued_wtloads();
	
	void load_wt(int id, wavetable *wt);		
	void load_wt(string filename, wavetable *wt);
	void load_wt_wt(string filename, wavetable *wt);
	void load_wt_wav(string filename, wavetable *wt);
	void clipboard_copy(int type, int scene, int entry);
	void clipboard_paste(int type, int scene, int entry);
	int get_clipboard_type();

	void errorbox(string message);
	
	vector<patchlist_category> patch_category,wt_category;
	vector<patchlist_entry> patch_list,wt_list;	
	int patch_category_split[2];
	string wtpath;
	string datapath;
   string userDataPath;
	string defaultsig,defaultname;
	//float table_sin[512],table_sin_offset[512];
	c_sec CS_WaveTableData,CS_ModRouting;
	wavetable WindowWT;
private:				
	TiXmlDocument snapshotloader;	
	vector<parameter> clipboard_p;	
	int clipboard_type;
	sub3_stepsequence clipboard_stepsequences[n_lfos];
	vector<modulation_routing> clipboard_modulation_scene,clipboard_modulation_voice;
	wavetable clipboard_wt[n_oscs];	
};

float note_to_pitch(float);
float note_to_pitch_inv(float);
void note_to_omega(float,float&,float&);
float db_to_linear(float);		
float lookup_waveshape(int, float);	
float lookup_waveshape_warp(int, float);
float envelope_rate_lpf(float);
float envelope_rate_linear(float);	