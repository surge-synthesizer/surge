/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_COMMON_DSP_MODULATORS_ADSRMODULATIONSOURCE_H
#define SURGE_SRC_COMMON_DSP_MODULATORS_ADSRMODULATIONSOURCE_H

#include "DSPUtils.h"
#include "SurgeStorage.h"
#include "SurgeVoiceState.h"
#include "ModulationSource.h"
#include "DebugHelpers.h"

enum ADSRState
{
    s_attack = 0,
    s_decay,
    s_sustain,
    s_release,
    s_uberrelease,
    s_idle_wait1,
    s_idle,
};

const float one = 1.f;
const float zero = 0.f;

class ADSRModulationSource : public ModulationSource
{
  public:
    ADSRModulationSource() {}

    void init(SurgeStorage *storage, ADSRStorage *adsr, pdata *localcopy, SurgeVoiceState *state)
    {
        this->storage = storage;
        this->adsr = adsr;
        this->state = state;
        this->lc = localcopy;

        a = adsr->a.param_id_in_scene;
        d = adsr->d.param_id_in_scene;
        s = adsr->s.param_id_in_scene;
        r = adsr->r.param_id_in_scene;

        a_s = adsr->a_s.param_id_in_scene;
        d_s = adsr->d_s.param_id_in_scene;
        r_s = adsr->r_s.param_id_in_scene;

        mode = adsr->mode.param_id_in_scene;

        envstate = s_attack;

        phase = 0;
        output = 0;
        idlecount = 0;
        scalestage = 1.f;
        _v_c1 = 0.f;
        _v_c1_delayed = 0.f;
        _discharge = 0.f;
        _ungateHold = 0.f;
    }

    void retrigger() { retriggerFrom(0.f); }

    void retriggerFrom(float start)
    {
        if (envstate < s_release)
            attackFrom(start);
    }

    virtual void attack() override { attackFrom(0.f); }

    virtual void attackFrom(float start)
    {
        phase = 0;
        output = 0;
        idlecount = 0;
        scalestage = 1.f;

        if (start > 0)
        {
            output = start;

            switch (lc[a_s].i)
            {
            case 0:
                phase = output * output;
                break;
            case 1:
                phase = output;
                break;
            case 2:
                phase = sqrt(output);
                break;
            };
        }

        // Reset the analog state machine too
        _v_c1 = start;
        _v_c1_delayed = start;
        _discharge = 0.f;
        _ungateHold = 0.f;

        envstate = s_attack;

        if ((lc[a].f - adsr->a.val_min.f) < 0.01)
        {
            envstate = s_decay;
            output = 1;
            phase = 1;
        }
    }

    virtual const char *get_title() override { return "envelope"; }
    virtual int get_type() override { return mst_adsr; }
    virtual bool per_voice() override { return true; }
    virtual bool is_bipolar() override { return false; }

    void release() override
    {
        scalestage = output;
        phase = 1;
        envstate = s_release;
    }

    void uber_release()
    {
        scalestage = output;
        phase = 1;
        envstate = s_uberrelease;
    }

    bool is_idle() { return (envstate == s_idle) && (idlecount > 0); }

    bool correctAnalogMode{false};

    virtual void process_block() override
    {
        const bool r_gated = adsr->r.deform_type;

        if (lc[mode].b)
        {
            /*
            ** This is the "analog" mode of the envelope. If you are unclear what it is doing
            ** because of the SSE the algo is pretty simple; charge up and discharge a capacitor
            ** with a gate. charge until you hit 1, discharge while the gate is open floored at
            ** the Sustain; then release.
            **
            ** There is, in src/headless/UnitTests.cpp in the "Clone the Analog" section,
            ** a non-SSE implementation of this which makes it much easier to understand.
            */

            // TODO: Use this mode in XT2 (currently this is only used by VCV Rack modules)
            if (correctAnalogMode)
            {
                doCorrectAnalogMode();
                return;
            }
            const float v_cc = 1.5f;

            auto v_c1 = SIMD_MM(load_ss)(&_v_c1);
            auto v_c1_delayed = SIMD_MM(load_ss)(&_v_c1_delayed);
            auto discharge = SIMD_MM(load_ss)(&_discharge);
            const auto one = SIMD_MM(set_ss)(1.0f); // attack->decay switch at 1 volt
            const auto v_cc_vec = SIMD_MM(set_ss)(v_cc);
            bool gate = (envstate == s_attack) || (envstate == s_decay);
            auto v_gate = gate ? SIMD_MM(set_ss)(v_cc) : SIMD_MM(set_ss)(0.f);
            auto v_is_gate = SIMD_MM(cmpgt_ss)(v_gate, SIMD_MM(set_ss)(0.0));

            // The original code here was
            // SIMD_MM(and_ps)(SIMD_MM(or_ps)(SIMD_MM(cmpgt_ss)(v_c1_delayed, one), discharge),
            // v_gate); which ORed in the v_gate value as opposed to the boolean
            discharge = SIMD_MM(and_ps)(
                SIMD_MM(or_ps)(SIMD_MM(cmpgt_ss)(v_c1_delayed, one), discharge), v_is_gate);

            v_c1_delayed = v_c1;

            float sparm = limit_range(lc[s].f, 0.f, 1.f);
            auto S = SIMD_MM(load_ss)(&sparm);
            S = SIMD_MM(mul_ss)(S, S);
            auto v_attack = SIMD_MM(andnot_ps)(discharge, v_gate);
            auto v_decay = SIMD_MM(or_ps)(SIMD_MM(andnot_ps)(discharge, v_cc_vec),
                                          SIMD_MM(and_ps)(discharge, S));
            auto v_release = v_gate;

            auto diff_v_a = SIMD_MM(max_ss)(SIMD_MM(setzero_ps)(), SIMD_MM(sub_ss)(v_attack, v_c1));

            // This change from a straight min allows sustain swells
            auto diff_vd_kernel = SIMD_MM(sub_ss)(v_decay, v_c1);
            auto diff_vd_kernel_min = SIMD_MM(min_ss)(SIMD_MM(setzero_ps)(), diff_vd_kernel);
            auto dis_and_gate = SIMD_MM(and_ps)(discharge, v_is_gate);
            auto diff_v_d = SIMD_MM(or_ps)(SIMD_MM(and_ps)(dis_and_gate, diff_vd_kernel),
                                           SIMD_MM(andnot_ps)(dis_and_gate, diff_vd_kernel_min));

            auto diff_v_r =
                SIMD_MM(min_ss)(SIMD_MM(setzero_ps)(), SIMD_MM(sub_ss)(v_release, v_c1));

            // calculate coefficients for envelope
            const float shortest = 6.f;
            const float longest = -2.f;
            const float coeff_offset = 2.f - log(storage->samplerate / BLOCK_SIZE) / log(2.f);

            float coef_A =
                powf(2.f, std::min(0.f, coeff_offset - lc[a].f * (adsr->a.temposync
                                                                      ? storage->temposyncratio
                                                                      : 1.f)));
            float coef_D =
                powf(2.f, std::min(0.f, coeff_offset - lc[d].f * (adsr->d.temposync
                                                                      ? storage->temposyncratio
                                                                      : 1.f)));
            float coef_R =
                envstate == s_uberrelease
                    ? 6.f
                    : powf(2.f,
                           std::min(0.f, coeff_offset - lc[r].f * (adsr->r.temposync
                                                                       ? storage->temposyncratio
                                                                       : 1.f)));

            v_c1 = SIMD_MM(add_ss)(v_c1, SIMD_MM(mul_ss)(diff_v_a, SIMD_MM(load_ss)(&coef_A)));
            v_c1 = SIMD_MM(add_ss)(v_c1, SIMD_MM(mul_ss)(diff_v_d, SIMD_MM(load_ss)(&coef_D)));
            v_c1 = SIMD_MM(add_ss)(v_c1, SIMD_MM(mul_ss)(diff_v_r, SIMD_MM(load_ss)(&coef_R)));

            SIMD_MM(store_ss)(&_v_c1, v_c1);
            SIMD_MM(store_ss)(&_v_c1_delayed, v_c1_delayed);
            SIMD_MM(store_ss)(&_discharge, discharge);

            SIMD_MM(store_ss)(&output, v_c1);
            if (gate)
            {
                _ungateHold = output;
            }
            else
            {
                if (r_gated)
                {
                    output = _ungateHold;
                }
            }

            const float SILENCE_THRESHOLD = 1e-6;

            if (!gate && _discharge == 0.f && _v_c1 < SILENCE_THRESHOLD)
            {
                envstate = s_idle;
                output = 0;
                idlecount++;
            }
        }
        else
        {
            switch (envstate)
            {
            case (s_attack):
            {
                phase += storage->envelope_rate_linear_nowrap(lc[a].f) *
                         (adsr->a.temposync ? storage->temposyncratio : 1.f);

                if (phase >= 1)
                {
                    phase = 1;
                    envstate = s_decay;
                    sustain = lc[s].f;
                }

                switch (lc[a_s].i)
                {
                case 0:
                    output = sqrt(phase);
                    break;
                case 1:
                    output = phase;
                    break;
                case 2:
                    output = phase * phase;
                    break;
                };
            }
            break;
            case (s_decay):
            {
                float rate = storage->envelope_rate_linear_nowrap(lc[d].f) *
                             (adsr->d.temposync ? storage->temposyncratio : 1.f);
                float l_lo, l_hi;

                switch (lc[d_s].i)
                {
                case 1:
                {
                    float sx = sqrt(phase);

                    l_lo = phase - 2 * sx * rate + rate * rate;
                    l_hi = phase + 2 * sx * rate + rate * rate;

                    /*
                    ** That + rate * rate in both means at low sustain ( < 1e-3 or so) you end up
                    ** with lo and hi both pushing us up off sustain. Unfortunately we need to
                    ** handle that case specially by pushing lo down. These limits are pretty
                    ** empirical. Git blame to see the various issues around here which show the
                    ** test cases.
                    */

                    if ((lc[s].f < 1e-3 && phase < 1e-4) || (lc[s].f == 0 && lc[d].f < -7))
                    {
                        l_lo = 0;
                    }
                    /*
                    ** Similarly if the rate is very high - larger than one - we can push l_lo well
                    ** above the sustain, which can set back a feedback cycle where we bounce onto
                    ** sustain and off again. To understand this, remove this bound and play with
                    ** test-data/patches/ADSR-Self-Oscillate.fxp
                    */
                    if (rate > 1.0 && l_lo > lc[s].f)
                    {
                        l_lo = lc[s].f;
                    }
                }
                break;
                case 2:
                {
                    float sx = powf(phase, 0.3333333f);

                    l_lo = phase - 3 * sx * sx * rate + 3 * sx * rate * rate - rate * rate * rate;
                    l_hi = phase + 3 * sx * sx * rate + 3 * sx * rate * rate + rate * rate * rate;
                }
                break;
                default:
                    l_lo = phase - rate;
                    l_hi = phase + rate;
                    break;
                };

                phase = limit_range(lc[s].f, l_lo, l_hi);
                output = phase;
            }
            break;
            case (s_release):
            {
                phase -= storage->envelope_rate_linear_nowrap(lc[r].f) *
                         (adsr->r.temposync ? storage->temposyncratio : 1.f);

                if (!r_gated)
                {
                    output = phase;

                    for (int i = 0; i < lc[r_s].i; i++)
                    {
                        output *= phase;
                    }

                    output *= scalestage;
                }

                if (phase < 0)
                {
                    envstate = s_idle;
                    output = 0;
                }
            }
            break;
            case (s_uberrelease):
            {
                phase -= storage->envelope_rate_linear_nowrap(-6.5);

                if (!r_gated)
                {
                    output = phase;

                    for (int i = 0; i < lc[r_s].i; i++)
                    {
                        output *= phase;
                    }

                    output *= scalestage;
                }

                if (phase < 0)
                {
                    envstate = s_idle;
                    output = 0;
                }
            }
            break;
            case s_idle:
                idlecount++;
                break;
            };

            output = limit_range(output, 0.f, 1.f);
        }
    }

    void doCorrectAnalogMode()
    {
        const float coeff_offset = 2.f - log(storage->samplerate / BLOCK_SIZE) / log(2.f);

        float coef_A = powf(
            2.f, std::min(0.f, coeff_offset -
                                   lc[a].f * (adsr->a.temposync ? storage->temposyncratio : 1.f)));
        float coef_D = powf(
            2.f, std::min(0.f, coeff_offset -
                                   lc[d].f * (adsr->d.temposync ? storage->temposyncratio : 1.f)));
        float coef_R =
            envstate == s_uberrelease
                ? 6.f
                : powf(2.f, std::min(0.f, coeff_offset - lc[r].f * (adsr->r.temposync
                                                                        ? storage->temposyncratio
                                                                        : 1.f)));

        const float v_cc = 1.01f;
        auto gate = (envstate == s_attack) || (envstate == s_decay);
        float v_gate = gate ? v_cc : 0.f;

        // discharge = SIMD_MM(and_ps)(SIMD_MM(or_ps)(SIMD_MM(cmpgt_ss)(v_c1_delayed, one),
        // discharge), v_gate);
        corr_discharge = ((corr_v_c1_delayed >= 1) || corr_discharge) && gate;
        corr_v_c1_delayed = corr_v_c1;

        float sparm = limit_range(lc[s].f, 0.f, 1.f);
        float S = sparm;
        float normD = std::max(0.05f, 1 - S);
        coef_D /= normD;

        float v_attack = corr_discharge ? 0 : v_gate;
        float v_decay = corr_discharge ? S : v_cc;
        float v_release = v_gate;

        float diff_v_a = std::max(0.f, v_attack - corr_v_c1);
        float diff_v_d =
            (corr_discharge && gate) ? v_decay - corr_v_c1 : std::min(0.f, v_decay - corr_v_c1);
        float diff_v_r = std::min(0.f, v_release - corr_v_c1);

        corr_v_c1 = corr_v_c1 + diff_v_a * coef_A;
        corr_v_c1 = corr_v_c1 + diff_v_d * coef_D;
        corr_v_c1 = corr_v_c1 + diff_v_r * coef_R;

        output = corr_v_c1;

        if (!gate && !corr_discharge && corr_v_c1 < 1e-6)
        {
            output = 0;
        }
    }

    int getEnvState() { return envstate; }

  private:
    ADSRStorage *adsr = nullptr;
    SurgeVoiceState *state = nullptr;
    SurgeStorage *storage = nullptr;
    float phase = 0.f;
    float sustain = 0.f;
    float scalestage = 0.f;
    int idlecount = 0;
    int envstate = 0;
    pdata *lc = nullptr;
    int a = 0, d = 0, s = 0, r = 0, a_s = 0, d_s = 0, r_s = 0, mode = 0;

    float _v_c1 = 0.f;
    float _v_c1_delayed = 0.f;
    float _discharge = 0.f;
    float _ungateHold = 0.f;

    float corr_v_c1{0.f};
    float corr_v_c1_delayed{0.f};
    bool corr_discharge{false};
};

#endif // SURGE_SRC_COMMON_DSP_MODULATORS_ADSRMODULATIONSOURCE_H
