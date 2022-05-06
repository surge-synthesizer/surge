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

class AliasOscillator : public Oscillator
{
  public:
    enum ao_params
    {
        ao_wave = 0,
        ao_wrap,
        ao_mask,
        ao_threshold,
        ao_bit_depth,

        ao_unison_detune = 5,
        ao_unison_voices,
    };

    enum ao_waves
    {
        aow_sine,
        aow_ramp,
        aow_pulse,
        aow_noise,

        aow_mem_alias,
        aow_mem_oscdata,
        aow_mem_scenedata,
        aow_mem_dawextra,
        aow_mem_stepseqdata,

        aow_audiobuffer,

        aow_sine_tx2,
        aow_sine_tx3,
        aow_sine_tx4,
        aow_sine_tx5,
        aow_sine_tx6,
        aow_sine_tx7,
        aow_sine_tx8,

        aow_additive,

        ao_n_waves
    };

    static constexpr int n_additive_partials = 16;

    AliasOscillator(SurgeStorage *s, OscillatorStorage *o, pdata *p)
        : Oscillator(s, o, p), charFilt(s)
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
    virtual void init_extra_config()
    {
        oscdata->extraConfig.nData = 16;
        for (auto i = 0; i < oscdata->extraConfig.nData; ++i)
            oscdata->extraConfig.data[i] = 1.f / (i + 1);
    }
    virtual void process_block(float pitch, float drift = 0.f, bool stereo = false, bool FM = false,
                               float FMdepth = 0.f);
    template <bool do_FM, bool do_bitcrush, AliasOscillator::ao_waves wavetype>
    void process_block_internal(const float pitch, const float drift, const bool stereo,
                                const float fmdepthV, const float crush_bits);

    lag<float, true> fmdepth;

    // character filter
    Surge::Oscillator::CharacterFilter<float> charFilt;

    int n_unison = 1;
    uint32_t phase[MAX_UNISON];
    float unisonOffsets[MAX_UNISON];
    float mixL[MAX_UNISON], mixR[MAX_UNISON];
    uint8_t dynamic_wavetable[256];
    unsigned dynamic_wavetable_sleep = 0; // blocks to wait before recalculating dynamic wavetable

    struct UInt8RNG
    {
        // Based on http://en.wikipedia.org/wiki/Xorshift
        // and inspired by https://github.com/edrosten/8bit_rng and
        // http://www.donnelly-house.net/programming/cdp1802/8bitPRNGtest.html

        uint8_t x, y, z, a;
        uint8_t stepCount;
        UInt8RNG() : x(21), y(229), z(181), a(rand() & 0xFF), stepCount(0) {}

        inline uint8_t step()
        {
            uint8_t t = x ^ ((x << 3U) & 0xFF);
            x = y;
            y = z;
            z = a;
            a = a ^ (a >> 5U) ^ (t ^ (t >> 2U));
            return a;
        }
        inline uint8_t stepTo(uint8_t sc, uint8_t every)
        {
            uint8_t r = a;
            while (stepCount != sc)
            {
                stepCount++;
                if (stepCount % every == 0)
                    r = step();
            }
            return r;
        }
    };
    UInt8RNG urng8[MAX_UNISON];

    Surge::Oscillator::DriftLFO driftLFO[MAX_UNISON];
};

struct Always255CountedSet
    : public CountedSetUserData // Something to feed to a ct_countedset_percent control
{
    virtual int getCountedSetSize() const { return 255; }
};

const Always255CountedSet ALWAYS255COUNTEDSET;

extern const char *ao_type_names[];
