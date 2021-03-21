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
#include "DspUtilities.h"
#include "OscillatorCommonFunctions.h"

class AliasOscillator : public Oscillator
{
  public:
    enum ao_params
    {
        ao_wave,
        ao_shift,
        ao_mask,
        ao_unison_detune,
        ao_unison_voices,
    };

    enum ao_types
    {
        aot_saw,
        aot_tri,
        aot_pulse,
        aot_sine,

        ao_n_types
    };

    AliasOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p) : Oscillator(s, o, p)
    {
        for (auto u = 0; u < MAX_UNISON; ++u)
        {
            phase[u] = 0;
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

    template <ao_types wavetype, bool subOctave, bool FM>
    void process_sblk(float pitch, float drift = 0.f, bool stereo = false, float FMdepth = 0.f);

    lag<float, true> fmdepth;

    // character filter
    Surge::Oscillator::CharacterFilter<float> charFilt;

    int n_unison = 1;
    bool starting = true;
    uint32_t phase[MAX_UNISON];
    float unisonOffsets[MAX_UNISON];
    float mixL[MAX_UNISON], mixR[MAX_UNISON];

    Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];

    int cachedDeform = -1;
};

extern const char* ao_type_names[4];

extern const uint8_t ALIAS_SINETABLE[256];

struct Always256CountedSet: public CountedSetUserData // Something to feed to a ct_countedset_percent control
{
    virtual int getCountedSetSize() { return 256; }
};

const Always256CountedSet ALWAYS256COUNTEDSET;
