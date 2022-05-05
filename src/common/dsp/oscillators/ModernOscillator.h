/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "OscillatorBase.h"
#include "DSPUtils.h"
#include "OscillatorCommonFunctions.h"

class ModernOscillator : public Oscillator
{
  public:
    enum mo_params
    {
        mo_saw_mix = 0,
        mo_pulse_mix,
        mo_tri_mix,
        mo_pulse_width,
        mo_sync,
        mo_unison_detune,
        mo_unison_voices,
    };

    enum mo_multitypes
    {
        momt_triangle,
        momt_square,
        momt_sine,
    } multitype = momt_triangle;

    enum mo_submask
    {
        mo_subone = 1U << 10,
        mo_subskipsync = 1U << 11,
    };

    static constexpr int sigbuf_len = 6;
    ModernOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p)
        : Oscillator(s, o, p), charFilt(s)
    {
        for (auto u = 0; u < MAX_UNISON; ++u)
        {
            phase[u] = 0;
            sphase[u] = 0;
            sReset[u] = false;
            mixL[u] = 1.f;
            mixR[u] = 1.f;
        }
    }

    virtual void init(float pitch, bool is_display = false, bool nonzero_init_drift = true);
    virtual void init_ctrltypes(int scene, int oscnum) { init_ctrltypes(); };
    virtual void init_ctrltypes();
    virtual void init_default_values();
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f);

    template <mo_multitypes multitype, bool subOctave, bool FM>
    void process_sblk(float pitch, float drift = 0.f, bool stereo = false, float FMdepth = 0.f);

    lag<double, true> sawmix, trimix, sqrmix, pwidth, sync, dpbase[MAX_UNISON], dspbase[MAX_UNISON],
        subdpbase, subdpsbase, detune, pitchlag, fmdepth;

    // character filter
    Surge::Oscillator::CharacterFilter<double> charFilt;
    // double charfiltB0 = 0.0, charfiltB1 = 0.0, charfiltA1 = 0.0;
    double priorY_L = 0.0, priorY_R = 0.0, priorX_L = 0.0, priorX_R = 0.0;

    int n_unison = 1;
    bool starting = true;
    double phase[MAX_UNISON], sphase[MAX_UNISON], sprior[MAX_UNISON], sTurnFrac[MAX_UNISON],
        sTurnVal[MAX_UNISON], subphase, subsphase;
    bool sReset[MAX_UNISON];
    bool subReset;
    double unisonOffsets[MAX_UNISON];
    double mixL[MAX_UNISON], mixR[MAX_UNISON];

    Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];

    int cachedDeform = -1;
};

const char mo_multitype_names[3][16] = {"Triangle", "Square", "Sine"};
