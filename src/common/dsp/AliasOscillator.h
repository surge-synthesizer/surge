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
        ao_wave = 0,
        ao_shift,
        ao_mask,
        ao_threshold,
        ao_depth,

        ao_unison_detune = 5,
        ao_unison_voices,
    };

    enum ao_waves
    {
        aow_sine,
        aow_triangle,
        aow_pulse,
        aow_noise,
        aow_mem_alias,
        aow_mem_oscdata,
        aow_mem_scenedata,
        aow_mem_dawextra,

        ao_n_waves
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

    template <ao_waves wavetype, bool subOctave, bool FM>
    void process_sblk(float pitch, float drift = 0.f, bool stereo = false, float FMdepth = 0.f);

    lag<float, true> fmdepth;

    // character filter
    Surge::Oscillator::CharacterFilter<float> charFilt;

    int n_unison = 1;
    uint32_t phase[MAX_UNISON];
    float unisonOffsets[MAX_UNISON];
    float mixL[MAX_UNISON], mixR[MAX_UNISON];

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

extern const char *ao_type_names[];
