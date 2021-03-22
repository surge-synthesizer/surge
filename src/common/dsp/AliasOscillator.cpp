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
#include "DebugHelpers.h"

// says ao_n_types is undefined. makes no sense, whatever, for now I'm hardcoding it. TODO FIX.
const int ALIAS_OSCILLATOR_WAVE_TYPES = /*ao_n_types*/ 4;
const char *ao_type_names[4] = {"Saw", "Triangle", "Pulse", "Sine"};

const uint8_t ALIAS_SINETABLE[256] = {
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

void AliasOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    n_unison = is_display ? 1 : oscdata->p[ao_unison_voices].val.i;

    auto us = Surge::Oscillator::UnisonSetup<float>(n_unison);

    for (int u = 0; u < n_unison; ++u)
    {
        unisonOffsets[u] = us.detune(u);
        us.attenuatedPanLaw(u, mixL[u], mixR[u]);

        // We want phase to be a random 32-bit number.
        // The quality of the randomness doesn't matter.
        // rand() is only guaranteed to produce 15 bits but on most modern platforms it's 31 bits.
        // So just add rand() to rand() for now, it's good enough.
        phase[u] = oscdata->retrigger.val.b || is_display ? 0.f : (rand() + rand());

        driftLFO[u].init(nonzero_init_drift);
    }

    charFilt.init(storage->getPatch().character.val.i);
}

void AliasOscillator::process_block(float pitch, float drift, bool stereo, bool FM, float fmdepthV)
{
    float ud = oscdata->p[ao_unison_detune].get_extended(
        localcopy[oscdata->p[ao_unison_detune].param_id_in_scene].f);

    double fv = 16 * fmdepthV * fmdepthV * fmdepthV;
    fmdepth.newValue(fv);

    const ao_types wavetype = (ao_types)oscdata->p[ao_wave].val.i;

    // how many bits to take from the 32bit phase counter
    const uint32_t bit_depth =
        limit_range(localcopy[oscdata->p[ao_depth].param_id_in_scene].i, 1, 16);
    const uint32_t bit_mask = (1 << bit_depth) - 1;
    oscdata->wt.n_tables = bit_mask; // see init_default_values

    const float inverseBits = 1.0 / ((float)bit_mask);
    const float inverse255 = 1.0 / ((float)0xFF);

    const uint32_t shift =
        limit_range((int)(float)(bit_mask * localcopy[oscdata->p[ao_shift].param_id_in_scene].f), 0,
                    (int)bit_mask);

    const uint32_t mask =
        limit_range((int)(float)(bit_mask * localcopy[oscdata->p[ao_mask].param_id_in_scene].f), 0,
                    (int)bit_mask);

    const uint32_t threshold = limit_range(
        (int)(float)(bit_mask * localcopy[oscdata->p[ao_threshold].param_id_in_scene].f), 0,
        (int)bit_mask);

    const double two32 = 4294967296.0;

    // compute once for each unison voice here, then apply per sample
    uint32_t phase_increments[MAX_UNISON];

    for (int u = 0; u < n_unison; ++u)
    {
        const float lfodrift = drift * driftLFO[u].next();
        phase_increments[u] = pitch_to_dphase(pitch + lfodrift + ud * unisonOffsets[u]) * two32;
    }

    for (int i = 0; i < BLOCK_SIZE_OS; ++i)
    {
        float fmPhaseShift = 0.0;

        if (FM)
        {
            fmPhaseShift = fmdepth.v * master_osc[i] * two32;
        }

        float vL = 0.0f, vR = 0.0f;
        for (int u = 0; u < n_unison; ++u)
        {
            const uint32_t upper = phase[u] >> (32 - bit_depth);

            const uint32_t shifted = upper + shift;

            uint32_t shaped = shifted; // default to untransformed (wrap any overflow)
            if (wavetype == aot_tri)
            {
                if (shifted > threshold)
                { // flip wave to make a triangle shape (has a DC offset, needs fixing)
                    shaped = bit_mask - shifted;
                }
            }
            else if (wavetype == aot_pulse)
            {
                // test highest bit to make pulse shape
                shaped = (shifted > threshold) ? bit_mask : 0x00;
            }

            const uint8_t masked = shaped ^ mask;

            // default to this for all waves except sine
            float out = ((float)masked - (float)(bit_mask >> 1)) * inverseBits;
            // but for sine...
            if (wavetype == aot_sine)
            {
                if (bit_depth >= 8)
                {
                    out = ((float)ALIAS_SINETABLE[masked >> (8 - bit_depth)] - (float)0x7F) *
                          inverse255;
                }
                else
                {
                    out = ((float)ALIAS_SINETABLE[masked << (8 - bit_depth)] - (float)0x7F) *
                          inverse255;
                }
            }

            vL += out * mixL[u];
            vR += out * mixR[u];

            phase[u] += phase_increments[u] + fmPhaseShift;
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

void AliasOscillator::init_ctrltypes()
{
    oscdata->p[ao_wave].set_name("Shape");
    oscdata->p[ao_wave].set_type(ct_alias_wave);

    oscdata->p[ao_shift].set_name("Shift");
    oscdata->p[ao_shift].set_type(ct_countedset_percent);
    oscdata->p[ao_shift].set_user_data(oscdata);

    oscdata->p[ao_mask].set_name("Mask");
    oscdata->p[ao_mask].set_type(ct_countedset_percent);
    oscdata->p[ao_mask].set_user_data(oscdata);

    oscdata->p[ao_threshold].set_name("Threshold");
    oscdata->p[ao_threshold].set_type(ct_countedset_percent);
    oscdata->p[ao_threshold].set_user_data(oscdata);

    oscdata->p[ao_depth].set_name("Bit Depth");
    oscdata->p[ao_depth].set_type(ct_fmratio_int);

    oscdata->p[ao_unison_detune].set_name("Unison Detune");
    oscdata->p[ao_unison_detune].set_type(ct_oscspread);
    oscdata->p[ao_unison_voices].set_name("Unison Voices");
    oscdata->p[ao_unison_voices].set_type(ct_osccount);
}

void AliasOscillator::init_default_values()
{
    oscdata->p[ao_wave].val.i = 0;
    oscdata->p[ao_shift].val.f = 0.0f;
    oscdata->p[ao_mask].val.f = 0.0f;
    oscdata->p[ao_threshold].val.f = 0.5f;
    oscdata->p[ao_depth].val.i = 8;
    // OscillatorStorage's implementation of CountedSetUserData is used to display
    // the range of the parameters. This abuses the wavetable's n_tables field.
    // As there are no actual wavetables used by this oscillator, this is safe even if
    // somewhat messy.
    oscdata->wt.n_tables = 255;
    oscdata->p[ao_unison_detune].val.f = 0.2f;
    oscdata->p[ao_unison_voices].val.i = 1;
}

/*int AliasOscillator::getCountedSetSize() {
    return bit_mask;
}*/
