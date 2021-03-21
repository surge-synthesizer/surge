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
const char ao_type_names[4][16] = {"Saw", "Triangle", "Pulse", "Sine"};

void AliasOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    n_unison = is_display ? 1 : oscdata->p[ao_unison_voices].val.i;

    auto us = Surge::Oscillator::UnisonSetup<float>(n_unison);

    float atten = us.attenuation();

    for (int u = 0; u < n_unison; ++u)
    {
        unisonOffsets[u] = us.detune(u);
        us.attenuatedPanLaw(u, mixL[u], mixR[u]);

        // TODO: replace this with something that actually randomises the full 32 bits
        phase[u] = oscdata->retrigger.val.b || is_display ? 0 : rand();

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

    uint8_t shift = limit_range(
        (int)(float)(0xFF * localcopy[oscdata->p[ao_shift].param_id_in_scene].f),
        0, 0xFF);

    uint8_t mask  = limit_range(
        (int)(float)(0xFF * localcopy[oscdata->p[ao_mask ].param_id_in_scene].f),
        0, 0xFF);

    const double two32 = 4294967296.0;
    const float  inverse256 = 1.0 / 255.0;
   
    // compute once for each unison voice here, then apply per sample
    uint32_t phase_increments[MAX_UNISON];
    for (int u = 0; u < n_unison; ++u){
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
            const uint8_t upper = phase[u] >> 24;
            
            const uint16_t shifted = upper + shift;
            
            uint8_t shaped = shifted; // default to untransformed (wrap any offset)
            if(wavetype == aot_tri){
                if(shifted > 0x7F){ // flip wave to make a triangle shape
                    shaped = -shifted;
                }
            }
            else if(wavetype == aot_pulse){
                // test highest bit to make pulse shape
                shaped = (shifted >= 0x80) ? 0xFF : 0x00;
            }

            const uint8_t masked = shaped ^ mask;

            const uint8_t result = (wavetype == aot_sine) ? ALIAS_SINETABLE[masked] : masked;

            const float out = ((float)result - (float)0x7F) * inverse256;

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

    starting = false;
}

void AliasOscillator::init_ctrltypes()
{
    oscdata->p[ao_wave].set_name("Shape");
    oscdata->p[ao_wave].set_type(ct_alias_wave);

    // Shift and Mask need a user data parameter implementing CountedSetUserData.
    // Always256CountedSet does this, and we have a constant that does this, ALWAYS256COUNTEDSET.
    // But we need a ParamUserData*, no const. As Always256CountedSet contains no actual data,
    // it is safe to discard the const from the pointer via a cast.
    ParamUserData* ud = (ParamUserData*) &ALWAYS256COUNTEDSET;

    oscdata->p[ao_shift].set_name("Shift");
    oscdata->p[ao_shift].set_type(ct_countedset_percent);
    oscdata->p[ao_shift].set_user_data(ud);

    oscdata->p[ao_mask].set_name("Mask");
    oscdata->p[ao_mask].set_type(ct_countedset_percent);
    oscdata->p[ao_mask].set_user_data(ud);

    oscdata->p[ao_unison_detune].set_name("Unison Detune");
    oscdata->p[ao_unison_detune].set_type(ct_oscspread);
    oscdata->p[ao_unison_voices].set_name("Unison Voices");
    oscdata->p[ao_unison_voices].set_type(ct_osccount);
}

void AliasOscillator::init_default_values()
{
    oscdata->p[ao_wave].val.i  = 0;
    oscdata->p[ao_shift].val.f = 0.0f;
    oscdata->p[ao_mask ].val.f = 0.0f;
    oscdata->p[ao_unison_detune].val.f = 0.2f;
    oscdata->p[ao_unison_voices].val.i = 1;
}
