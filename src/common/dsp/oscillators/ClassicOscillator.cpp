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

#include "ClassicOscillator.h"
#include "DSPUtils.h"

#include "sst/basic-blocks/mechanics/block-ops.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"
namespace mech = sst::basic_blocks::mechanics;

/*
**
** ## Overview
**
** The AbstractBlitOperator handles a model where an oscillator generates
** an impulse buffer, but requires pitch tuning, drift, FM, and DAC emulation.
**
** Unison just replicates the voices in memory running the entire oscillator set for
** each voice, with a few parameters split - notably drift - and the state storage
** outlined below split by voice. As such the state is often indexed by a voice. For
** the rest of this description I'll leave out the unison splits. Similarly the stereo
** implementation just adds pairs (for oscbuffer there is oscbufferR and so on)
** and here I'll just document the mono implementation
**
** ## Overall operating model
**
** Assume we have some underlying waveform which we either have in memory or which
** we generate using an algorithm. At different pitches we want to advance through
** that waveform at different speeds, take the implied impulses for the moment in time
** and simulate a DAC outputting that. The common form of that waveform is that it is
** digital - namely it is represented as a set of impulse values at a set of times -
** but those times do not align with the sample points.
**
** In code that looks as follows
**
** - The oscillator has a phase pointer (oscstate) which indicates where we are in the
**   internal stream.
** - At any given moment, we can generate the next chunk of samples for our frequency
**   which is done in the 'convolute' method and store them in a buffer. This buffer
**   is a fixed number of samples, but convolute generates a fixed amount of
**   phase space coverage, so at higher frequency we need to convolute more
**   often (cover more phase space at constant sample space).
** - in our process loop, we extract those samples from the buffer to the output.
**   At any given convolution moment, we store into the buffer at the state variable bufpos.
** - If we have extracted our set of samples, we re-generate the next chunk by convoluting.
**
** So basically we have a couple of arrows pointing around. oscstate, which shows us
** how much phase space is covered up to bufpos of buffer and the simple
** march of time that comes from calling ::process_block. When we are out of state
** space (namely, oscstate < BLOCK_SIZE * wavelength) we need to reconvolve and
** fill our buffer and increase our oscstate pointer. So in ::process_block it
** looks like oscstate counts down and convolute pushes it up, but what is really
** happening is: oscstate counts down because bufpos moves forwards, and
** convolute gives us more valid buffer ahead of us. When we're beyond the end of the
** oscillator buffer we need to wrap our pointer.
**
** The storage buffer is sized so there is enough room to run a FIR model of the DAC
** forward in time from the point of the current buffer. This means when we wrap
** the buffer position we need to copy the back-end FIR buffer into the front of the new
** buffer. Other than that subtlety, the buffer is just a ring.
**
** There's lots more details but that's the basic operating model you will see in
** ::process_block once you know that ::convolute generates the next blast
** of samples into the oscbuffer structure.
**
** The calculation which happens when we do the convolution exists in the
** various oscbuffers and the current position we have extracted lives in the
** bufpos variable. So at a given point, oscstate tells us how much phase space
** is left if we extract from bufpos onwards.
**
** The convolute method, then, is the heart of the oscillator. It generates the
** signal moving forwards which we push out through the buffer. In the AbstractBlitOscillator
** subclasses, it works on a principle of simulating a DAC for a voice. A little theory:
**
** We know that in a theoretical basis, a digital signal is a stream of delta impulses at
** the sample point, but we also know that delta impulses have infinite frequency response,
** so especially as you get closer to the Nyquist frequency, you end up with very nasty
** aliasing problems. Sample a 60 Hz sine wave at 100 Hz and you can immediately see the 40
** Hz artefact. So what you want to do is replace the delta with a function that has the
** time response matching a perfect lowpass filter, which is a rectangle in frequency space or
** a sinc in time space. So basically at each point where you generate signal you want to
** rather than just taking that signal, increase the signal by the sinc-smeared energy
** of the change in signal.
**
** Or: Rather than "output = zero-order samples of underlyer", do:
**     output += (change in underlyer) x (sinc)
** where x is the convolution operator. Since sinc has infinite support, though, we can't use that
** really, so have to use windowed sinc.
**
** Once we have committed to convolving an exact but differently aligned impulse stream into
** our sample output, though, we have the opportunity to exactly align the time of that
** impulse convolution with the moment between the samples when the actual impulse occurs.
**
** So the convolution has to manage a couple of dimensions of time. When we call ::convolute,
** remember, it is because we don't have enough buffer phase space computed for our current block.
** So ::convolute is filling a block in the "future" of our current pointer. That means we can
** actually use a slightly non-causal filter into the oscstate future. So, mechanically,
** we end up implementing:
**     oscbuffer [i + futurelook] = sum(impulse change) * impulse[i]
**
** Surge adds one last wrinkle, which is that impulse function depends on how far between a sample
** you are. The peak of the function should happen exactly at the point intra-sample. To do that it
** makes two tables. The first is a table of the windowed sinc at 256 steps between 0 and 1 sample.
** The second is the derivative of that windowed function with respect to position which allows us
** to make a first order Taylor correction to the window. Somewhat confusingly, but very efficiently
** these two tables are stored in one data structure "sinctable", with an indexing structure that
** gives a window, a window derivative, the next window, the next window derivative, etc...
**
** But the end result is we do a calculation which amounts to:
**
** while (our remaining oscstate doesn't cover enough phase space) <<- this is in ::process_block
**    convolute <<- do this call
**    Figure out our next impulse and change in impulse. Call that change g.
**    Figure out how far in the future that impulse spans. Call that delay.
**    Fill in the oscbuffer in that future with the windowed impulse
**        oscbuffer[pos + i] = oscbuffer[pos + i] + g * (sincwindow[i] + dt * dsincwindow[i])
**    advance oscstate by the amount of phase space we have covered
**
** Unfortunately, to do this efficiently, the code is a bit inscrutable, hence this comment. Also
** some of the variable names (lipol128 is not an obvious name for the 'dt' above) makes the code
** hard to follow. As such, in this implementation I've added quite a lot of comments to the
** ::convolute method.
**
** At the final stage, the system layers on a simple 3-coefficient one delay biquad filter
** into the stream based on Character parameter, copies the buffer to the output, and then manages
** pointer wraparounds and stuff. That's all pretty mechanical.
**
*/

using namespace std;

AbstractBlitOscillator::AbstractBlitOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                               pdata *localcopy)
    : Oscillator(storage, oscdata, localcopy)
{
    integrator_hpf = (1.f - 2.f * 20.f * storage->samplerate_inv);
    integrator_hpf *= integrator_hpf;
}

void AbstractBlitOscillator::prepare_unison(int voices)
{
    auto us = Surge::Oscillator::UnisonSetup<float>(voices);

    out_attenuation_inv = us.attenuation_inv();
    ;
    out_attenuation = 1.0f / out_attenuation_inv;

    detune_bias = us.detuneBias();
    detune_offset = us.detuneOffset();
    for (int v = 0; v < voices; ++v)
    {
        us.panLaw(v, panL[v], panR[v]);
    }
}

ClassicOscillator::ClassicOscillator(SurgeStorage *storage, OscillatorStorage *oscdata,
                                     pdata *localcopy)
    : AbstractBlitOscillator(storage, oscdata, localcopy), charFilt(storage)
{
}

ClassicOscillator::~ClassicOscillator() {}

void ClassicOscillator::init(float pitch, bool is_display, bool nonzero_init_drift)
{
    assert(storage);
    first_run = true;
    charFilt.init(storage->getPatch().character.val.i);

    osc_out = SIMD_MM(set1_ps)(0.f);
    osc_out2 = SIMD_MM(set1_ps)(0.f);
    osc_outR = SIMD_MM(set1_ps)(0.f);
    osc_out2R = SIMD_MM(set1_ps)(0.f);
    bufpos = 0;
    dc = 0;

    id_shape = oscdata->p[co_shape].param_id_in_scene;
    id_pw = oscdata->p[co_width1].param_id_in_scene;
    id_pw2 = oscdata->p[co_width2].param_id_in_scene;
    id_sub = oscdata->p[co_mainsubmix].param_id_in_scene;
    id_sync = oscdata->p[co_sync].param_id_in_scene;
    id_detune = oscdata->p[co_unison_detune].param_id_in_scene;

    float rate = 0.05;
    l_pw.setRate(rate);
    l_pw2.setRate(rate);
    l_shape.setRate(rate);
    l_sub.setRate(rate);
    l_sync.setRate(rate);

    n_unison = limit_range(oscdata->p[co_unison_voices].val.i, 1, MAX_UNISON);

    if (is_display)
    {
        n_unison = 1;
    }

    prepare_unison(n_unison);

    memset(oscbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(oscbufferR, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(dcbuffer, 0, sizeof(float) * (OB_LENGTH + FIRipol_N));
    memset(last_level, 0, MAX_UNISON * sizeof(float));
    memset(elapsed_time, 0, MAX_UNISON * sizeof(float));

    this->pitch = pitch;
    update_lagvals<true>();

    for (int i = 0; i < n_unison; i++)
    {
        if (oscdata->retrigger.val.b || is_display)
        {
            oscstate[i] = 0.f;
            syncstate[i] = 0.f;
            last_level[i] = 0.f;
        }
        else
        {
            double drand = (double)storage->rand_01();
            double detune = oscdata->p[co_unison_detune].get_extended(localcopy[id_detune].f) *
                            (detune_bias * float(i) + detune_offset);
            double st = 0.5 * drand * storage->note_to_pitch_inv_tuningctr(detune);
            oscstate[i] = st;
            syncstate[i] = st;
            last_level[i] = 0.f;
        }

        dc_uni[i] = 0.f;
        state[i] = 0.f;
        pwidth[i] = limit_range(l_pw.v, 0.001f, 0.999f);
        driftLFO[i].init(nonzero_init_drift);
    }
}

void ClassicOscillator::init_ctrltypes()
{
    oscdata->p[co_shape].set_name("Shape");
    oscdata->p[co_shape].set_type(ct_percent_bipolar);
    oscdata->p[co_width1].set_name("Width 1");
    oscdata->p[co_width1].set_type(ct_percent);
    oscdata->p[co_width1].val_default.f = 0.5f;
    oscdata->p[co_width2].set_name("Width 2");
    oscdata->p[co_width2].set_type(ct_percent);
    oscdata->p[co_width2].val_default.f = 0.5f;
    oscdata->p[co_mainsubmix].set_name("Sub Mix");
    oscdata->p[co_mainsubmix].set_type(ct_percent);
    oscdata->p[co_sync].set_name("Sync");
    oscdata->p[co_sync].set_type(ct_syncpitch);
    oscdata->p[co_unison_detune].set_name("Unison Detune");
    oscdata->p[co_unison_detune].set_type(ct_oscspread);
    oscdata->p[co_unison_voices].set_name("Unison Voices");
    oscdata->p[co_unison_voices].set_type(ct_osccount);
}
void ClassicOscillator::init_default_values()
{
    oscdata->p[co_shape].val.f = 0.f;
    oscdata->p[co_width1].val.f = 0.5f;
    oscdata->p[co_width2].val.f = 0.5f;
    oscdata->p[co_mainsubmix].val.f = 0.f;
    oscdata->p[co_sync].val.f = 0.f;
    oscdata->p[co_unison_detune].val.f = 0.1f;
    oscdata->p[co_unison_voices].val.i = 1;
}

template <bool FM> void ClassicOscillator::convolute(int voice, bool stereo)
{
    /*
    ** I've carefully documented the non-FM non-sync case here. The other cases are
    ** similar. See the comment above. Remember, this function exists to calculate
    ** the next impulse in our digital sequence, which occurs at time 'oscstate',
    ** convolve it into our output stream, and advance our phase state space by
    ** the amount just covered.
    */

    /*
    ** Detune by a combination of the LFO drift and the unison voice spread.
    */
    float detune = drift * driftLFO[voice].val();
    if (n_unison > 1)
    {
        detune += oscdata->p[co_unison_detune].get_extended(localcopy[id_detune].f) *
                  (detune_bias * (float)voice + detune_offset);
    }

    float wf = l_shape.v;
    float sub = l_sub.v;
    const float p24 = (1 << 24);

    /*
    ** ipos is a value between 0 and 2^24 indicating how far along in oscstate (phase space for
    ** our state) we are
    */
    unsigned int ipos;

    if ((l_sync.v > 0) && syncstate[voice] < oscstate[voice])
    {
        if (FM)
        {
            ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv * FMmul_inv));
        }
        else
        {
            ipos = (unsigned int)(p24 * (syncstate[voice] * pitchmult_inv));
        }

        float t;

        // See the extensive comment below
        if (!oscdata->p[co_unison_detune].absolute)
        {
            t = storage->note_to_pitch_inv_tuningctr(detune) * 2;
        }
        else
        {
            // Copy the mysterious * 2 and drop the +sync
            t = storage->note_to_pitch_inv_ignoring_tuning(
                    detune * storage->note_to_pitch_inv_ignoring_tuning(pitch) * 16 / 0.9443) *
                2;
        }

        state[voice] = 0;
        last_level[voice] += dc_uni[voice] * (oscstate[voice] - syncstate[voice]);

        oscstate[voice] = syncstate[voice];
        syncstate[voice] += t;
        syncstate[voice] = max(0.f, syncstate[voice]);
    }
    else
    {
        if (FM)
        {
            ipos = (unsigned int)(p24 * (oscstate[voice] * pitchmult_inv * FMmul_inv));
        }
        else
        {
            ipos = (unsigned int)(p24 * (oscstate[voice] * pitchmult_inv));
        }
    }

    /*
    ** delay is the number of samples ahead of bufpos that oscstate implies at current pitch.
    ** Basically the 'integer part' of the position.
    */
    unsigned int delay;

    if (FM)
    {
        delay = FMdelay;
    }
    else
    {
        delay = ((ipos >> 24) & 0x3f);
    }

    /*
    ** m and lipol128 are the integer and fractional part of the number of 256ths
    ** (FIRipol_N-ths really) that our current position places us at. These are obviously
    ** not great variable names. Especially lipolui16 doesn't seem to be fractional at all
    ** it seems to range between 0 and 0xffff, but it is multiplied by the sinctable
    ** derivative block (see comment above and also see the SurgeStorage constructor
    ** second sinctable block), which is pre-scaled down by 65536, so lipol * sinctable[j * + 1]
    ** is the fractional derivative of the sinctable with respect to time. (The calculation is
    ** numerical, not analytical in SurgeStorage).
    */
    unsigned int m = ((ipos >> 16) & 0xff) * (FIRipol_N << 1);
    unsigned int lipolui16 = (ipos & 0xffff);
    auto lipol128 = SIMD_MM(setzero_ps)();
    lipol128 = SIMD_MM(cvtsi32_ss)(lipol128, lipolui16);
    lipol128 = SIMD_MM(shuffle_ps)(lipol128, lipol128, SIMD_MM_SHUFFLE(0, 0, 0, 0));

    int k;
    const float s = 0.99952f;
    float sync = min((float)l_sync.v, (12 + 72 + 72) - pitch);
    float t;

    if (oscdata->p[co_unison_detune].absolute)
    {
        /*
        ** Oh so this line of code. What is it doing?
        **
        **  t = storage->note_to_pitch_inv_tuningctr(detune * pitchmult_inv * (1.f / 440.f) + sync);
        ** Let's for a moment assume standard tuning. So note_to_pitch_inv will give you, say, 1/32
        *for note 60 and 1/1 for note 0. Cool.
        ** It is the inverse of frequency. That's why below with detune = +/- 1 for the extreme 2
        *voice case we just use it directly.
        ** It is the time distance of one note.
        **
        ** But in absolute mode we want to scale that note. So the calculation here (assume sync is
        *0 for a second) is
        ** detune * pitchmult_inv / 440
        ** pitchmult_inv =  dsamplerate_os / 8.17 * note_to_pitch_inv(pitch)
        ** so this is using
        ** detune * 1.0 / 440 * 1.0 / 8.17 * dsamplerate * note_to_pitch_inv(pitch)
        ** Or:
        ** detune / note_to_pitch(pitch) * ( 1.0 / (440 * 8.17 ) ) * dsamplerate
        **
        ** So there's a couple of things wrong with that. First of all this should not be samplerate
        *dependent.
        ** Second of all, what's up with 1.0 / ( 8.17 * 440 )
        **
        ** Well the answer is that we want the time to be pushed around in Hz. So it turns out that
        ** 44100 * 2 / ( 440 * 8.175 ) =~ 24.2 and 24.2 / 16 = 1.447 which is almost how much
        *absolute is off. So
        ** let's set the multiplier here so that the regtests exactly match the display frequency.
        *That is the
        ** frequency desired spread / 0.9443. 0.9443 is empirically determined by running the 2
        *unison voices case
        ** over a bunch of tests.
        */
        t = storage->note_to_pitch_inv_ignoring_tuning(
            detune * storage->note_to_pitch_inv_ignoring_tuning(pitch) * 16 / 0.9443 + sync);

        // With extended range and low frequencies we can have an implied negative frequency; cut
        // that off by setting a lower bound here.
        if (t < 0.01)
        {
            t = 0.01;
        }
    }
    else
    {
        t = storage->note_to_pitch_inv_tuningctr(detune + sync);
    }

    float t_inv = mech::rcp(t);
    float g = 0.0, gR = 0.0;

    /*
    ** This is the SuperOscillator state machine; basically a 4-impulse cycle to generate
    ** squares, saws, and subs. The output of this is 'g' which is the change from the prior
    ** level at this impulse. Each time we convolve we advance the state pointer and move to the
    ** next case.
    */
    switch (state[voice])
    {
    case 0:
    {
        pwidth[voice] = l_pw.v;
        pwidth2[voice] = 2.f * l_pw2.v;

        // calculate the height of the first impulse of the cycle
        float tg = ((1 + wf) * 0.5f + (1 - pwidth[voice]) * (-wf)) * (1 - sub) +
                   0.5f * sub * (2.f - pwidth2[voice]);

        g = tg - last_level[voice];
        last_level[voice] = tg;

        // calculate the level sub-cycle will have at the end of its duration taking DC into account
        last_level[voice] -= (pwidth[voice]) * (pwidth2[voice]) * (1.f + wf) * (1.f - sub);
        break;
    }
    case 1:
        g = wf * (1.f - sub) - sub;
        last_level[voice] += g;
        last_level[voice] -= (1 - pwidth[voice]) * (2 - pwidth2[voice]) * (1 + wf) * (1.f - sub);
        break;
    case 2:
        g = 1.f - sub;
        last_level[voice] += g;
        last_level[voice] -= (pwidth[voice]) * (2 - pwidth2[voice]) * (1 + wf) * (1.f - sub);
        break;
    case 3:
        g = wf * (1.f - sub) + sub;
        last_level[voice] += g;
        last_level[voice] -= (1 - pwidth[voice]) * (pwidth2[voice]) * (1 + wf) * (1.f - sub);
        break;
    };

    g *= out_attenuation;

    if (stereo)
    {
        gR = g * panR[voice];
        g *= panL[voice];
    }

    if (stereo)
    {
        auto g128L = SIMD_MM(load_ss)(&g);
        g128L = SIMD_MM(shuffle_ps)(g128L, g128L, SIMD_MM_SHUFFLE(0, 0, 0, 0));
        auto g128R = SIMD_MM(load_ss)(&gR);
        g128R = SIMD_MM(shuffle_ps)(g128R, g128R, SIMD_MM_SHUFFLE(0, 0, 0, 0));

        for (k = 0; k < FIRipol_N; k += 4)
        {
            float *obfL = &oscbuffer[bufpos + k + delay];
            float *obfR = &oscbufferR[bufpos + k + delay];
            auto obL = SIMD_MM(loadu_ps)(obfL);
            auto obR = SIMD_MM(loadu_ps)(obfR);
            auto st = SIMD_MM(load_ps)(&storage->sinctable[m + k]);
            auto so = SIMD_MM(load_ps)(&storage->sinctable[m + k + FIRipol_N]);
            so = SIMD_MM(mul_ps)(so, lipol128);
            st = SIMD_MM(add_ps)(st, so);
            obL = SIMD_MM(add_ps)(obL, SIMD_MM(mul_ps)(st, g128L));
            SIMD_MM(storeu_ps)(obfL, obL);
            obR = SIMD_MM(add_ps)(obR, SIMD_MM(mul_ps)(st, g128R));
            SIMD_MM(storeu_ps)(obfR, obR);
        }
    }
    else
    {
        /*
        ** This is SSE for the convolution described above
        */
        auto g128 = SIMD_MM(load_ss)(&g);
        g128 = SIMD_MM(shuffle_ps)(g128, g128, SIMD_MM_SHUFFLE(0, 0, 0, 0));

        for (k = 0; k < FIRipol_N; k += 4)
        {
            float *obf = &oscbuffer[bufpos + k + delay]; // Get buffer[pos + delay + k ]
            auto ob = SIMD_MM(loadu_ps)(obf);
            auto st = SIMD_MM(load_ps)(
                &storage->sinctable[m + k]); // get the sinctable for our fractional position
            auto so =
                SIMD_MM(load_ps)(&storage->sinctable[m + k + FIRipol_N]); // get the sinctable deriv
            so = SIMD_MM(mul_ps)(so, lipol128); // scale the deriv by the lipol fractional time
            st = SIMD_MM(add_ps)(st, so);       // this is now st = sinctable + dt * dsinctable
            st = SIMD_MM(mul_ps)(st, g128); // so this is now the convolved difference, g * kernel
            ob = SIMD_MM(add_ps)(ob, st);   // which we add back onto the buffer
            SIMD_MM(storeu_ps)(obf, ob);    // and store.
        }
    }

    float olddc = dc_uni[voice];
    dc_uni[voice] = t_inv * (1.f + wf) * (1 - sub);
    dcbuffer[(bufpos + FIRoffset + delay)] += (dc_uni[voice] - olddc);

    if (state[voice] & 1)
    {
        rate[voice] = t * (1.0 - pwidth[voice]);
    }
    else
    {
        rate[voice] = t * pwidth[voice];
    }

    if ((state[voice] + 1) & 2)
    {
        rate[voice] *= (2.0f - pwidth2[voice]);
    }
    else
    {
        rate[voice] *= pwidth2[voice];
    }

    oscstate[voice] += rate[voice];
    oscstate[voice] = max(0.f, oscstate[voice]);
    state[voice] = (state[voice] + 1) & 3;
}

// 290 samples to fall by 50% (British)  (Is probably a 2-pole HPF)
// 202 samples (American)
// const float integrator_hpf = 0.999f;
// pow(ln(0.5)/(samplerate/50hz)
const float hpf_cycle_loss = 0.995f;

template <bool is_init> void ClassicOscillator::update_lagvals()
{
    l_sync.newValue(max(0.f, localcopy[id_sync].f));
    l_pw.newValue(limit_range(localcopy[id_pw].f, 0.001f, 0.999f));
    l_pw2.newValue(limit_range(localcopy[id_pw2].f, 0.001f, 0.999f));
    l_shape.newValue(limit_range(localcopy[id_shape].f, -1.f, 1.f));
    l_sub.newValue(limit_range(localcopy[id_sub].f, 0.f, 1.f));

    // keytracked highpass filter that deforms the mathematically perfect BLIT waveforms
    auto pp = storage->note_to_pitch_tuningctr(pitch + l_sync.v);
    float invt = 4.f * min(1.0, (8.175798915 * pp * storage->dsamplerate_os_inv));
    // TODO: Make a lookup table
    float hpf2 = min(integrator_hpf, powf(hpf_cycle_loss, invt));

    li_hpf.set_target(hpf2);

    if (is_init)
    {
        l_pw.instantize();
        l_pw2.instantize();
        l_shape.instantize();
        l_sub.instantize();
        l_sync.instantize();
        li_DC.instantize();

        li_hpf.instantize();
    }
}

void ClassicOscillator::process_block(float pitch0, float drift, bool stereo, bool FM, float depth)
{
    /*
    ** So let's tie these comments back to the description at the top. Start by setting up your
    ** time and wavelength based on the note
    */
    this->pitch = min(148.f, pitch0);
    this->drift = drift;
    pitchmult_inv = std::max(1.0, storage->dsamplerate_os * (1.f / 8.175798915f) *
                                      storage->note_to_pitch_inv(pitch));
    // This must be a real division, reciprocal approximation is not precise enough
    pitchmult = 1.f / pitchmult_inv;

    int k, l;

    /*
    ** And step all my internal parameters
    */
    update_lagvals<false>();
    l_pw.process();
    l_pw2.process();
    l_shape.process();
    l_sub.process();
    l_sync.process();

    if (FM)
    {
        // FIXME - document the FM branch
        for (l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();
        }

        for (int s = 0; s < BLOCK_SIZE_OS; s++)
        {
            float fmmul = limit_range(1.f + depth * master_osc[s], 0.1f, 1.9f);
            float a = pitchmult * fmmul;

            FMdelay = s;

            for (l = 0; l < n_unison; l++)
            {
                while (((l_sync.v > 0) && (syncstate[l] < a)) || (oscstate[l] < a))
                {
                    FMmul_inv = mech::rcp(fmmul);

                    // The division races with the growth of the oscstate so that it never comes out
                    // of/gets out of the loop this becomes unsafe, don't fuck with the oscstate but
                    // make a division within the convolute instead.
                    convolute<true>(l, stereo);
                }

                oscstate[l] -= a;

                if (l_sync.v > 0)
                {
                    syncstate[l] -= a;
                }
            }
        }
    }
    else
    {
        /*
        ** The amount of phase space we need to cover is the oversample block size * the wavelength
        */
        float a = (float)BLOCK_SIZE_OS * pitchmult;

        for (l = 0; l < n_unison; l++)
        {
            driftLFO[l].next();

            /*
            ** Either while sync is active and we need to fill syncstate traversal,
            ** or while we need to fill oscstate traversal to cover the expected request,
            */
            while (((l_sync.v > 0) && (syncstate[l] < a)) || (oscstate[l] < a))
            {
                /*
                ** Fill the buffer for the voice
                */
                convolute<false>(l, stereo);
            }

            /*
            ** And take the amount of phase space we just covered from both the
            ** oscillator and sync state
            */
            oscstate[l] -= a;

            if (l_sync.v > 0)
            {
                syncstate[l] -= a;
            }

            /*
            ** At this point we are guaranteed that the oscbuffer contains enough
            ** generated samples to cover at least the amount of sample space (which
            ** is block size * wavelength as above) that we need to cover. So we can go
            ** ahead and process
            */
        }
    }

    /*
    ** OK so load up the HPF across the block (linearly moving to target if target has changed)
    */
    float hpfblock alignas(16)[BLOCK_SIZE_OS];
    li_hpf.store_block(hpfblock, BLOCK_SIZE_OS_QUAD);

    /*
    ** And the DC offset and pitch-scaled output attenuation
    */
    auto mdc = SIMD_MM(load_ss)(&dc);
    auto oa = SIMD_MM(load_ss)(&out_attenuation);
    oa = SIMD_MM(mul_ss)(oa, SIMD_MM(load_ss)(&pitchmult));

    /*
    ** The Coefs here are from the character filter, and are set in ::init
    */
    const auto mmone = SIMD_MM(set_ss)(1.0f);
    auto char_b0 = SIMD_MM(load_ss)(&(charFilt.CoefB0));
    auto char_b1 = SIMD_MM(load_ss)(&(charFilt.CoefB1));
    auto char_a1 = SIMD_MM(load_ss)(&(charFilt.CoefA1));

    for (k = 0; k < BLOCK_SIZE_OS; k++)
    {
        auto dcb = SIMD_MM(load_ss)(&dcbuffer[bufpos + k]);
        auto hpf = SIMD_MM(load_ss)(&hpfblock[k]);
        auto ob = SIMD_MM(load_ss)(&oscbuffer[bufpos + k]);

        /*
        ** a = prior output * HPF value
        */
        auto a = SIMD_MM(mul_ss)(osc_out, hpf);

        /*
        ** mdc += DC level
        */
        mdc = SIMD_MM(add_ss)(mdc, dcb);

        /*
        ** output buffer += DC * out attenuation
        */
        ob = SIMD_MM(sub_ss)(ob, SIMD_MM(mul_ss)(mdc, oa));

        /*
        ** Stow away the last output and make the new output the oscbuffer + the filter controbution
        */
        auto LastOscOut = osc_out;
        osc_out = SIMD_MM(add_ss)(a, ob);

        /*
        ** So at that point osc_out = a + ob; = prior_out * HPF + oscbuffer + DC * attenuation;
        */

        /*
        ** character filter (hifalloff/neutral/boost)
        **
        ** This formula is out2 = out2 * char_a1 + out * char_b0 + last_out * char_b1
        **
        ** which is the classic biquad formula.
        */

        osc_out2 = SIMD_MM(add_ss)(SIMD_MM(mul_ss)(osc_out2, char_a1),
                                   SIMD_MM(add_ss)(SIMD_MM(mul_ss)(osc_out, char_b0),
                                                   SIMD_MM(mul_ss)(LastOscOut, char_b1)));

        /*
        ** And so store the output of the HPF as the output
        */
        SIMD_MM(store_ss)(&output[k], osc_out2);

        // And do it all again if we are stereo
        if (stereo)
        {
            ob = SIMD_MM(load_ss)(&oscbufferR[bufpos + k]);

            a = SIMD_MM(mul_ss)(osc_outR, hpf);

            ob = SIMD_MM(sub_ss)(ob, SIMD_MM(mul_ss)(mdc, oa));
            auto LastOscOutR = osc_outR;
            osc_outR = SIMD_MM(add_ss)(a, ob);

            osc_out2R = SIMD_MM(add_ss)(SIMD_MM(mul_ss)(osc_out2R, char_a1),
                                        SIMD_MM(add_ss)(SIMD_MM(mul_ss)(osc_outR, char_b0),
                                                        SIMD_MM(mul_ss)(LastOscOutR, char_b1)));

            SIMD_MM(store_ss)(&outputR[k], osc_out2R);
        }
    }

    /*
    ** Store the DC accumulation
    */
    SIMD_MM(store_ss)(&dc, mdc);

    /*
    ** And clean up and advance our buffer pointer
    */
    mech::clear_block<BLOCK_SIZE_OS>(&oscbuffer[bufpos]);

    if (stereo)
    {
        mech::clear_block<BLOCK_SIZE_OS>(&oscbufferR[bufpos]);
    }

    mech::clear_block<BLOCK_SIZE_OS>(&dcbuffer[bufpos]);

    bufpos = (bufpos + BLOCK_SIZE_OS) & (OB_LENGTH - 1);

    /*
    ** each block overlap FIRipol_N samples into the next (due to impulses not being wrapped around
    ** the block edges copy the overlapping samples to the new block position
    */
    if (bufpos == 0) // only needed if the new bufpos == 0
    {
        SIMD_M128 overlap[FIRipol_N >> 2], dcoverlap[FIRipol_N >> 2], overlapR[FIRipol_N >> 2];
        const auto zero = SIMD_MM(setzero_ps)();

        for (k = 0; k < (FIRipol_N); k += 4)
        {
            overlap[k >> 2] = SIMD_MM(load_ps)(&oscbuffer[OB_LENGTH + k]);
            SIMD_MM(store_ps)(&oscbuffer[k], overlap[k >> 2]);
            SIMD_MM(store_ps)(&oscbuffer[OB_LENGTH + k], zero);

            dcoverlap[k >> 2] = SIMD_MM(load_ps)(&dcbuffer[OB_LENGTH + k]);
            SIMD_MM(store_ps)(&dcbuffer[k], dcoverlap[k >> 2]);
            SIMD_MM(store_ps)(&dcbuffer[OB_LENGTH + k], zero);

            if (stereo)
            {
                overlapR[k >> 2] = SIMD_MM(load_ps)(&oscbufferR[OB_LENGTH + k]);
                SIMD_MM(store_ps)(&oscbufferR[k], overlapR[k >> 2]);
                SIMD_MM(store_ps)(&oscbufferR[OB_LENGTH + k], zero);
            }
        }
    }

    first_run = false;
}
