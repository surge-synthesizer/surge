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

// This oscillator is intentionally bad! Not recommended as an example of good DSP!

#include "AliasOscillator.h"
#include "SineOscillator.h"

// This linear representation is required for VST3 automation and the like and needs to
// match the param ID the UI is driven by the remapper code in init_ctrltypes
int alias_waves_count() { return AliasOscillator::ao_waves::ao_n_waves; }
const char *alias_wave_name[] = {
    "Sine",      "Ramp",          "Pulse",        "Noise",     "Alias Mem", "Osc Mem",
    "Scene Mem", "DAW Chunk Mem", "Step Seq Mem", "Audio In",  "TX 2 Wave", "TX 3 Wave",
    "TX 4 Wave", "TX 5 Wave",     "TX 6 Wave",    "TX 7 Wave", "TX 8 Wave", "Additive"};

const uint8_t alias_sinetable[256] = {
    0x7F, 0x82, 0x85, 0x88, 0x8B, 0x8F, 0x92, 0x95, 0x98, 0x9B, 0x9E, 0xA1, 0xA4, 0xA7, 0xAA, 0xAD,
    0xB0, 0xB3, 0xB6, 0xB8, 0xBB, 0xBE, 0xC1, 0xC3, 0xC6, 0xC8, 0xCB, 0xCD, 0xD0, 0xD2, 0xD5, 0xD7,
    0xD9, 0xDB, 0xDD, 0xE0, 0xE2, 0xE4, 0xE5, 0xE7, 0xE9, 0xEB, 0xEC, 0xEE, 0xEF, 0xF1, 0xF2, 0xF4,
    0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFB, 0xFC, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE,
    0xFF, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFD, 0xFD, 0xFC, 0xFB, 0xFB, 0xFA, 0xF9, 0xF8, 0xF7, 0xF6,
    0xF5, 0xF4, 0xF2, 0xF1, 0xEF, 0xEE, 0xEC, 0xEB, 0xE9, 0xE7, 0xE5, 0xE4, 0xE2, 0xE0, 0xDD, 0xDB,
    0xD9, 0xD7, 0xD5, 0xD2, 0xD0, 0xCD, 0xCB, 0xC8, 0xC6, 0xC3, 0xC1, 0xBE, 0xBB, 0xB8, 0xB6, 0xB3,
    0xB0, 0xAD, 0xAA, 0xA7, 0xA4, 0xA1, 0x9E, 0x9B, 0x98, 0x95, 0x92, 0x8F, 0x8B, 0x88, 0x85, 0x82,
    0x7F, 0x7C, 0x79, 0x76, 0x73, 0x6F, 0x6C, 0x69, 0x66, 0x63, 0x60, 0x5D, 0x5A, 0x57, 0x54, 0x51,
    0x4E, 0x4B, 0x48, 0x46, 0x43, 0x40, 0x3D, 0x3B, 0x38, 0x36, 0x33, 0x31, 0x2E, 0x2C, 0x29, 0x27,
    0x25, 0x23, 0x21, 0x1E, 0x1C, 0x1A, 0x19, 0x17, 0x15, 0x13, 0x12, 0x10, 0x0F, 0x0D, 0x0C, 0x0A,
    0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x03, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x02, 0x03, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
    0x09, 0x0A, 0x0C, 0x0D, 0x0F, 0x10, 0x12, 0x13, 0x15, 0x17, 0x19, 0x1A, 0x1C, 0x1E, 0x21, 0x23,
    0x25, 0x27, 0x29, 0x2C, 0x2E, 0x31, 0x33, 0x36, 0x38, 0x3B, 0x3D, 0x40, 0x43, 0x46, 0x48, 0x4B,
    0x4E, 0x51, 0x54, 0x57, 0x5A, 0x5D, 0x60, 0x63, 0x66, 0x69, 0x6C, 0x6F, 0x73, 0x76, 0x79, 0x7C,
};

static uint8_t shaped_sinetable[7][256];
static bool initializedShapedSinetable = false;

void AliasOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    if (!initializedShapedSinetable)
    {
        initializedShapedSinetable = true;
        float dPhase = 2.0 * M_PI / (256 - 1);
        for (int i = 0; i < 7; ++i)
        {
            for (int k = 0; k < 256; ++k)
            {
                float c = std::cos(k * dPhase);
                float s = std::sin(k * dPhase);
                auto r = SineOscillator::valueFromSinAndCos(s, c, i + 1);
                auto r01 = (r + 1) * 0.5;
                shaped_sinetable[i][k] = (uint8_t)(r01 * 0xFF);
            }
        }
    }

    n_unison = is_display ? 1 : oscdata->p[ao_unison_voices].val.i;

    auto us = Surge::Oscillator::UnisonSetup<float>(n_unison);

    for (int u = 0; u < n_unison; ++u)
    {
        unisonOffsets[u] = us.detune(u);
        us.attenuatedPanLaw(u, mixL[u], mixR[u]);

        phase[u] = oscdata->retrigger.val.b || is_display ? 0.f : storage->rand_u32();

        driftLFO[u].init(nonzero_init_drift);
        // Seed the RNGs in display mode
        if (is_display)
            urng8[u].a = 73;
    }

    charFilt.init(storage->getPatch().character.val.i);
}

template <typename T> inline T localClamp(const T &a, const T &l, const T &h)
{
    return std::max(l, std::min(a, h));
}

// this templating makes the bool ifs etc faster
template <bool do_FM, bool do_bitcrush, AliasOscillator::ao_waves wavetype>
void AliasOscillator::process_block_internal(const float pitch, const float drift,
                                             const bool stereo, const float fmdepthV,
                                             const float crush_bits)
{
    float ud = oscdata->p[ao_unison_detune].get_extended(
        localcopy[oscdata->p[ao_unison_detune].param_id_in_scene].f);
    float absOff = 0;
    if (oscdata->p[ao_unison_detune].absolute)
    {
        absOff = ud * 16;
        ud = 0;
    }

    if (do_FM)
    {
        const float fv = 16.0f * fmdepthV * fmdepthV * fmdepthV;
        fmdepth.newValue(fv);
    }

    bool wavetable_mode = false;
    const uint8_t *wavetable = alias_sinetable;
    switch (wavetype)
    {
    case AliasOscillator::ao_waves::aow_sine:
        wavetable_mode = true;
        break;
    case AliasOscillator::ao_waves::aow_mem_alias:
        static_assert(sizeof(*this) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");
        wavetable_mode = true;
        wavetable = (const uint8_t *)this;
        break;
    case AliasOscillator::ao_waves::aow_mem_oscdata:
        static_assert(sizeof(*oscdata) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");
        wavetable_mode = true;
        wavetable = (const uint8_t *)oscdata;
        break;
    case AliasOscillator::ao_waves::aow_mem_scenedata:
        static_assert(sizeof(storage->getPatch().scenedata) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");
        wavetable_mode = true;
        wavetable = (const uint8_t *)storage->getPatch().scenedata;
        break;
    case AliasOscillator::ao_waves::aow_mem_dawextra:
        static_assert(sizeof(storage->getPatch().dawExtraState) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");
        wavetable_mode = true;
        wavetable = (const uint8_t *)&storage->getPatch().dawExtraState;
        break;
    case AliasOscillator::ao_waves::aow_mem_stepseqdata:
        static_assert(sizeof(storage->getPatch().stepsequences) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");
        wavetable_mode = true;
        wavetable = (const uint8_t *)storage->getPatch().stepsequences;
        break;
    case AliasOscillator::ao_waves::aow_audiobuffer:
        // TODO: correct size check here.
        // should really check BLOCK_SIZE_OS etc, make sure everything is as expected
        static_assert(sizeof(storage->audio_in) > 0xFF,
                      "Memory region not large enough to be indexed by Alias");

        wavetable_mode = true;
        wavetable = (const uint8_t *)dynamic_wavetable;

        // make sure we definitely rebuild the dynamic wavetable next time after e.g. switching
        // to additive mode after using this mode.
        dynamic_wavetable_sleep = 0;

        for (int qs = 0; qs < BLOCK_SIZE_OS; ++qs)
        {
            auto llong = (uint32_t)(((double)storage->audio_in[0][qs]) * (double)0xFFFFFFFF);
            auto rlong = (uint32_t)(((double)storage->audio_in[1][qs]) * (double)0xFFFFFFFF);

            llong = (llong >> 24) & 0xFF;
            rlong = (rlong >> 24) & 0xFF;

            dynamic_wavetable[4 * qs] = llong;
            dynamic_wavetable[4 * qs + 1] = rlong;
            dynamic_wavetable[4 * qs + 2] = llong;
            dynamic_wavetable[4 * qs + 3] = rlong;
        }
        break;
    case AliasOscillator::ao_waves::aow_additive:
    {
        wavetable_mode = true;
        wavetable = (const uint8_t *)dynamic_wavetable;

        if (dynamic_wavetable_sleep)
        { // skip if recently generated
            dynamic_wavetable_sleep--;
            break;
        } // else...

        // obtain normalization factor for amp array.
        // normally we'd multiply the array by 1/sqrt(squared sum) but since we want the output
        // to go -127..127, instead make it 127/sqrt(squared sum).
        // n.b. this will mean that the max amplitude of a single harmonnic is 1/2 of full scale.
        float norm = 0.f;
        for (int h = 0; h < n_additive_partials; h++)
        {
            norm += oscdata->extraConfig.data[h] * oscdata->extraConfig.data[h];
        }
        norm = 127.f / sqrtf(norm);

        // convert [-1, 1] floats into 8-bit ints
        int8_t amps[n_additive_partials];
        for (int h = 0; h < n_additive_partials; h++)
        {
            amps[h] = oscdata->extraConfig.data[h] * norm;
        }

        // s for sample
        for (int s = 0; s < 256; s++)
        {
            int16_t sample = 0;

            // h for harmonic
            for (int h = 0; h < n_additive_partials; h++)
            {
                // sine table is unsigned where zero is 0x7F, correct for this
                const int16_t scaled =
                    ((int16_t)alias_sinetable[s * (h + 1) & 0xFF] - 0x7F) * amps[h];

                // above is fixed point 0.8*0.8->8.8 multiplication; so accumulate the top 8 bits
                sample += scaled >> 8;
            }

            // clamp sample
            if (sample > 0x7F)
            {
                sample = 0x7F;
            }
            else if (sample < -0x7F)
            { // TODO: should we actually go down to -0x80?
                sample = -0x7F;
            }

            // shift our zero-is-zero signed integer to a zero-is-7F unsigned
            dynamic_wavetable[s] = sample + 0x7F;
        }

        // wait some blocks before generating again. TODO: define this better
        dynamic_wavetable_sleep = 20;
        break;
    }
    default:
        break;
    }

    if (wavetype >= aow_sine_tx2 && wavetype <= aow_sine_tx8)
    {
        wavetable_mode = true;
        wavetable = (const uint8_t *)(shaped_sinetable[wavetype - aow_sine_tx2]);
    }

    const uint32_t bit_mask = (1 << 8) - 1;           // 0xFF 255
    const float inv_bit_mask = 1.0 / (float)bit_mask; // 1/255

    const float wrap = 1.f + (clamp01(localcopy[oscdata->p[ao_wrap].param_id_in_scene].f) * 15.f);

    const uint32_t mask =
        localClamp((uint32_t)(float)(bit_mask * localcopy[oscdata->p[ao_mask].param_id_in_scene].f),
                   0U, (uint32_t)bit_mask);

    const bool ramp_unmasked_after_threshold = (bool)oscdata->p[ao_mask].deform_type;

    // clang-format off
    // I don't know why the pipeline behaves differently?
    const uint8_t threshold =
        (uint8_t)((float)bit_mask *
                  clamp01(localcopy[oscdata->p[ao_threshold].param_id_in_scene].f));
    // clang-format on

    const double two32 = 4294967296.0;

    // when we're not using the bit crusher we want to avoid these expensive operations.
    // the branch is free as it will be done at compile time with a decent compiler (e.g. g++)
    // it's just really ugly because in order to have it be const, with C scoping rules the only
    // option is to use ternary ? operator.
    const float quant = do_bitcrush ? powf(2, crush_bits) : 0.f;
    const float dequant = do_bitcrush ? 1.f / quant : 0.f;

    // compute once for each unison voice here, then apply per sample
    uint32_t phase_increments[MAX_UNISON];

    for (int u = 0; u < n_unison; ++u)
    {
        const float lfodrift = drift * driftLFO[u].next();
        phase_increments[u] =
            pitch_to_dphase_with_absolute_offset(pitch + lfodrift + ud * unisonOffsets[u],
                                                 absOff * unisonOffsets[u]) *
            two32;
    }

    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        // int64_t since I can span +/- two32 or beyond
        int64_t fmPhaseShift = 0;

        if (do_FM)
        {
            fmPhaseShift = (int64_t)(fmdepth.v * master_osc[i] * two32);
        }

        float vL = 0.f, vR = 0.f;

        for (int u = 0; u < n_unison; ++u)
        {
            uint32_t _phase = phase[u]; // default to this
            if (wavetype == aow_pulse)
            { // but for pulse...
                // fake hardsync
                _phase = (uint32_t)((float)phase[u] * wrap);
            }
            const uint8_t upper = _phase >> 24; // upper 8 bits
            const uint8_t masked = upper ^ mask;

            uint8_t result = masked; // default to this

            if (wavetype == aow_ramp)
            {
                // flip wave to make a triangle shape (n.b. has a DC offset)
                if (upper > threshold)
                {
                    if (ramp_unmasked_after_threshold)
                    {
                        result = bit_mask - upper;
                    }
                    else
                    {
                        result = bit_mask - masked;
                    }
                }
            }
            else if (wavetype == aow_pulse)
            {
                result = (masked > threshold) ? bit_mask : 0x00;
            }
            else if (wavetype == aow_noise)
            {
                result = urng8[u].stepTo((upper & 0xFF), threshold | 8U);
                // OK so we want to wrap towards 255/0 so
                int32_t shapes = result - 0x7F;
                shapes = localClamp((int32_t)(shapes * wrap), -0x7F, 0x7F - 1);
                result = (uint8_t)(shapes + 0x7F);
            }

            if (wavetype != aow_noise && wavetype != aow_pulse)
            {
                // wraparound. scales the result by a float, then casts back down to a byte
                result = (uint8_t)((float)result * wrap);
            }

            // default to this
            float out = ((float)result - (float)0x7F) * inv_bit_mask;

            // but for wavetable modes, index a table instead
            if (wavetable_mode) // TODO: maybe move this bool into the template?
            {
                if (result > threshold)
                {
                    result += 0x7F - threshold;
                }

                out = ((float)wavetable[0xFF - result] - (float)0x7F) * inv_bit_mask;
            }

            if (do_bitcrush)
            {
                // bitcrush
                out = dequant * (int)(out * quant);
            }

            vL += out * mixL[u];
            vR += out * mixR[u];

            // this order actually kinda matters in 32-bit especially
            if (do_FM)
            {
                phase[u] += fmPhaseShift;
            }
            phase[u] += phase_increments[u];
        }

        output[i] = vL;
        outputR[i] = vR;

        fmdepth.process();
    }

    if (!stereo)
    {
        for (int s = 0; s < BLOCK_SIZE_OS; ++s)
        {
            output[s] = 0.5 * (output[s] + outputR[s]);
        }
    }
    if (charFilt.doFilter)
    {
        if (stereo)
        {
            charFilt.process_block_stereo(output, outputR, BLOCK_SIZE_OS);
        }
        else
        {
            charFilt.process_block(output, BLOCK_SIZE_OS);
        }
    }
}

// this function calls template-specialised versions of the above function
void AliasOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepthV)
{
    const float crush_bits =
        limit_range(localcopy[oscdata->p[ao_bit_depth].param_id_in_scene].f, 1.f, 8.f);

    const ao_waves wavetype = (ao_waves)oscdata->p[ao_wave].val.i;

#define P(m)                                                                                       \
    case m:                                                                                        \
        if (crush_bits < 8.f)                                                                      \
        {                                                                                          \
            if (FM)                                                                                \
            {                                                                                      \
                AliasOscillator::process_block_internal<true, true, m>(pitch, drift, stereo,       \
                                                                       fmdepthV, crush_bits);      \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                AliasOscillator::process_block_internal<false, true, m>(pitch, drift, stereo,      \
                                                                        fmdepthV, crush_bits);     \
            }                                                                                      \
        }                                                                                          \
        else                                                                                       \
        {                                                                                          \
            if (FM)                                                                                \
            {                                                                                      \
                AliasOscillator::process_block_internal<true, false, m>(pitch, drift, stereo,      \
                                                                        fmdepthV, crush_bits);     \
            }                                                                                      \
            else                                                                                   \
            {                                                                                      \
                AliasOscillator::process_block_internal<false, false, m>(pitch, drift, stereo,     \
                                                                         fmdepthV, crush_bits);    \
            }                                                                                      \
        }                                                                                          \
        break;

    switch (wavetype)
    {
        P(aow_sine)
        P(aow_ramp)
        P(aow_pulse)
        P(aow_noise)
        P(aow_mem_alias)
        P(aow_mem_oscdata)
        P(aow_mem_scenedata)
        P(aow_mem_dawextra)
        P(aow_mem_stepseqdata)
        P(aow_audiobuffer)
        P(aow_sine_tx2)
        P(aow_sine_tx3)
        P(aow_sine_tx4)
        P(aow_sine_tx5)
        P(aow_sine_tx6)
        P(aow_sine_tx7)
        P(aow_sine_tx8)
        P(aow_additive)
    default:
        // getting here is an error but for now just do nothing
        break;
    }
}

void AliasOscillator::init_ctrltypes()
{
    static struct WaveRemapper : public ParameterDiscreteIndexRemapper
    {
        bool hasGroupNames() const override { return true; }
        bool supportsTotalIndexOrdering() const override { return true; };
        bool sortGroupNames() const override { return false; }
        bool useRemappedOrderingForGroupsIfNotSorted() const override { return true; }

        std::vector<std::pair<int, std::string>> mapping;
        std::unordered_map<int, int> inverseMapping;

        void p(int i, std::string s) { mapping.push_back(std::make_pair(i, s)); }

        WaveRemapper()
        {
            p(aow_sine, "");
            p(aow_ramp, "");
            p(aow_pulse, "");
            p(aow_noise, "");
            p(aow_additive, "");
            p(aow_audiobuffer, "");

            p(aow_sine_tx2, "Quadrant Shaping");
            p(aow_sine_tx3, "Quadrant Shaping");
            p(aow_sine_tx4, "Quadrant Shaping");
            p(aow_sine_tx5, "Quadrant Shaping");
            p(aow_sine_tx6, "Quadrant Shaping");
            p(aow_sine_tx7, "Quadrant Shaping");
            p(aow_sine_tx8, "Quadrant Shaping");

            p(aow_mem_alias, "Memory From...");
            p(aow_mem_oscdata, "Memory From...");
            p(aow_mem_stepseqdata, "Memory From...");
            p(aow_mem_scenedata, "Memory From...");
            p(aow_mem_dawextra, "Memory From...");

            int c = 0;
            for (auto e : mapping)
            {
                inverseMapping[e.first] = c++;
            }

            if (mapping.size() != ao_n_waves)
            {
                std::cout << "BAD MAPPING TYPES" << std::endl;
            }
        }

        int remapStreamedIndexToDisplayIndex(int i) const override
        {
            switch ((ao_waves)i)
            {
            case aow_sine:
            case aow_ramp:
            case aow_pulse:
            case aow_noise:
                return i;
            case aow_additive:
                return 4;
            case aow_audiobuffer:
                return 5;
            case aow_sine_tx2:
            case aow_sine_tx3:
            case aow_sine_tx4:
            case aow_sine_tx5:
            case aow_sine_tx6:
            case aow_sine_tx7:
            case aow_sine_tx8:
                return 6 + (i - aow_sine_tx2);
            case aow_mem_alias:
            case aow_mem_oscdata:
                return 13 + (i - aow_mem_alias);
            case aow_mem_stepseqdata:
                return 15;
            case aow_mem_scenedata:
                return 16;
            case aow_mem_dawextra:
                return 17;
            default:
                return i;
            }
        }

        std::string nameAtStreamedIndex(int i) const override
        {
            if (i <= aow_noise)
                return alias_wave_name[i];

            if (i >= aow_sine_tx2 && i <= aow_sine_tx8)
            {
                int txi = i - aow_sine_tx2 + 2;
                return std::string("TX ") + std::to_string(txi);
            }

            switch ((ao_waves)i)
            {
            case aow_mem_alias:
                return "This Alias Instance";
            case aow_mem_oscdata:
                return "Oscillator Data";
            case aow_mem_scenedata:
                return "Scene Data";
            case aow_mem_dawextra:
                return "DAW Chunk Data";
            case aow_mem_stepseqdata:
                return "Step Sequencer Data";
            case aow_audiobuffer:
                return "Audio In";
            case aow_additive:
                return "Additive";
            default:
                return "ERROR";
            }
        }

        std::string groupNameAtStreamedIndex(int i) const override
        {
            return mapping.at(inverseMapping.at(i)).second;
        }

        const std::vector<int> totalIndexOrdering() const override
        {
            auto res = std::vector<int>();

            for (auto m : mapping)
            {
                res.push_back(m.first);
            }

            return res;
        }
    } waveRemapper;

    oscdata->p[ao_wave].set_name("Shape");
    oscdata->p[ao_wave].set_type(ct_alias_wave);
    oscdata->p[ao_wave].set_user_data(&waveRemapper);

    oscdata->p[ao_wrap].set_name("Wrap");
    oscdata->p[ao_wrap].set_type(ct_percent);

    oscdata->p[ao_mask].set_name("Mask");
    oscdata->p[ao_mask].set_type(ct_alias_mask);

    // Threshold slider needs a ParamUserData* implementing CountedSetUserData.
    // Always255CountedSet does this, and we have a constant for that, ALWAYS255COUNTEDSET.
    // But we need a ParamUserData*, no const. As Always255CountedSet contains no actual data,
    // it is safe to discard the const from the pointer via a cast.
    ParamUserData *ud = (ParamUserData *)&ALWAYS255COUNTEDSET;
    oscdata->p[ao_threshold].set_name("Threshold");
    oscdata->p[ao_threshold].set_type(ct_countedset_percent);
    oscdata->p[ao_threshold].set_user_data(ud);
    oscdata->p[ao_threshold].val_default.f = 0.5;

    oscdata->p[ao_bit_depth].set_name("Bitcrush");
    oscdata->p[ao_bit_depth].set_type(ct_alias_bits);

    oscdata->p[ao_unison_detune].set_name("Unison Detune");
    oscdata->p[ao_unison_detune].set_type(ct_oscspread);
    oscdata->p[ao_unison_voices].set_name("Unison Voices");
    oscdata->p[ao_unison_voices].set_type(ct_osccount);
}

void AliasOscillator::init_default_values()
{
    oscdata->p[ao_wave].val.i = 0;
    oscdata->p[ao_wrap].val.f = 0.f;
    oscdata->p[ao_mask].val.f = 0.f;
    oscdata->p[ao_mask].deform_type = 0;
    oscdata->p[ao_threshold].val.f = 0.5f;
    oscdata->p[ao_bit_depth].val.f = 8.f;
    oscdata->p[ao_unison_detune].val.f = 0.2f;
    oscdata->p[ao_unison_voices].val.i = 1;
}
