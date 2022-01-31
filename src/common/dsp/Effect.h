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

#pragma once

#include "DSPUtils.h"
#include "SurgeStorage.h"

/*	base class			*/

class alignas(16) Effect
{
  public:
    enum
    {
        KNumVuSlots = 24
    };

    Effect(SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
    virtual ~Effect() { return; }

    virtual const char *get_effectname() { return 0; }

    virtual void init(){};
    virtual void init_ctrltypes();
    virtual void init_default_values(){};

    // No matter what path is used to reload (whether created anew or what not) this is called after
    // the loading state of an item has changed
    virtual void updateAfterReload(){};
    virtual Surge::ParamConfig::VUType vu_type(int id) { return Surge::ParamConfig::vut_off; };
    virtual int vu_ypos(int id) { return id; }; // in 'half-hslider' heights
    virtual const char *group_label(int id) { return 0; };
    virtual int group_label_ypos(int id) { return 0; };
    virtual int get_ringout_decay()
    {
        return -1;
    } // number of blocks it takes for the effect to 'ring out'

    virtual void process(float *dataL, float *dataR) { return; }
    virtual void process_only_control()
    {
        return;
    } // for controllers that should run regardless of the audioprocess
    virtual bool process_ringout(float *dataL, float *dataR,
                                 bool indata_present = true); // returns rtue if outdata is present
    // virtual void processSSE(float *dataL, float *dataR){ return; }
    // virtual void processSSE2(float *dataL, float *dataR){ return; }
    // virtual void processSSE3(float *dataL, float *dataR){ return; }
    // virtual void processT<int architecture>(float *dataL, float *dataR){ return; }
    virtual void suspend() { return; }
    float vu[KNumVuSlots]; // stereo pairs, just use every other when mono

    // Most of the fx read the sample rate at sample time but airwindows
    // keeps a cache so give loaded fx a notice when the sample rate changes
    virtual void sampleRateReset() {}

    virtual void handleStreamingMismatches(int streamingRevision, int currentSynthStreamingRevision)
    {
        // No-op here.
    }

    inline bool checkHasInvalidatedUI()
    {
        auto x = hasInvalidated;
        hasInvalidated = false;
        return x;
    }

  protected:
    SurgeStorage *storage;
    FxStorage *fxdata;
    pdata *pd;
    int ringout;
    float *f[n_fx_params];
    int *pdata_ival[n_fx_params]; // f is not a great choice for a member name, but 'i' would be
                                  // worse!
    bool hasInvalidated{false};
};

// Some common constants
const int max_delay_length = 1 << 18;
const int slowrate = 8;
const int slowrate_m1 = slowrate - 1;

Effect *spawn_effect(int id, SurgeStorage *storage, FxStorage *fxdata, pdata *pd);
