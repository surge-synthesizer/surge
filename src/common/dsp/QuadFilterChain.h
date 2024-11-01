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
#ifndef SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H
#define SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H

/*
 * The QuadFilterChain is the starting point of the Surge filter architecture and so we've placed
 * the overall filter architecture documentation in this header file.
 *
 * There are five basic objects you need to understand to understand the Surge filter architecture
 *
 * QuadFilterUnitState - the collection of states for a particular individual filter
 * FilterCoefficientMaker - an object which creates QuadFilterUniltStates for particular filters
 * QuadFilterChainState - the state of a filter bank, which comprises a pair of filters (perhaps
 *      in stereo) and a waveshaper
 * FilterUnitQFPtr - A function which, given a QuadFilterUnitState and an input sample, produces
 * output (the filter) WaveShaperQFPtr - A function which waveshapes an input to an output
 *
 * The three things that make this set of classes somewhat tricky are:
 *
 * 1. The variable names and state updates are kinda opaque and are scattered about a bit and
 * 2. Far more importantly, they are all parallelized across voices with SSE instructions
 * 3. We calculate coefficients less frequently than filters and interpolate changes across the
 * block
 *
 * So, let's deal with the second thing first. At any given time, Surge can be playing a collection
 * of voices. Each of those voices has a distinct filter configuration since resonance, cutoff,
 * feedback, drive and more can be modulated. As such, you have to evaluate the filter for each
 * voice.
 *
 * But in Surge, the filter *type* for a set of voices in a scene is constant. You may have
 * individual modulation of the cutoff of your filter, but all the voices have the same filter type.
 * So in theory, you can share calculations. Imagine a simplest filter: out = 0.2 * in + 0.8 * in-1.
 * It's a cruddy filter but you could imagine each SurgeVoice calling it, or you could imagine
 * clusters of four Surge voices loading their individual input and states into a single SSE vector
 * _m128, then calling a single SSE operation. That would make one voice and four voices the same
 * CPU load.
 *
 * Once you have that abstraction for your individual filters and waveshapers, you can extend it
 * to your entire filter bank. This is the job of QuadFilterChainState, which contains a pair
 * of filter units, a waveshaper, and information about various configurations. Using that
 * information QuadFilterChain.cpp then takes the state with the vector information set up and runs
 * the various topologies, whether that's a simple serial filter or the more complicated ring
 * modulator routing. These all look like 'given my sets of states run these SSE operations across
 * the voices'. Simple enough, once you realize what it's doing.
 *
 * That also tells us what the expected state in the QuadFilterChainState would be. You would have
 * 4 filter units (2 needed for mono processing; 4 for stereo). You would have configuration for the
 * pre-filter gain, feedback, waveshaper drive. And you would have input and output that you can
 * read and write. That's basically what you see below. The object also has dGain, which allows for
 * modulation changes (more on that later) and captures the entire wave data for convenience, but
 * logically it is a pair of filters, inputs, and outputs, all SSE vectorized.
 *
 * To use that, what do you need to do though? Well, clearly you need to write all of your
 * operators as SSE operators. So the FilterUnitQFPtr and WaveShaperQFPtr need to be _m128 -> _m128,
 * not float to float. You can see those in QuadFilterUnit.cpp and so on. These functions are
 * returned by the GetQFPtrFilterUnit function and so forth, which does a switch on type to return a
 * function of uniform signature (QuadFilterUnitState, _m128) for filter and _m128 for waveshaper.
 *
 * Moreover, you need to load up the voices into the filters before you process the filters. This
 * is done in SurgeVoice. SurgeVoice::process_block takes two arguments. A QuadFilterUnitChainState
 * and a Qe. Importantly, SurgeVoice does *not* take a float in and out. Rather it has as a job
 * to update the Qe'th SSE position in the QuadFilterUnitChainState handed to it. You can see
 * that SurgeSynth processes the voice in scene 's' as:
 *
 * bool resume = v->process_block(FBQ[s][FBentry[s] >> 2], FBentry[s] & 3);
 *
 * that is for a given voice number (which is basically FBentry) modulo it by 3 and update that
 * point in the filter bank. And that FBQ is created aligned all the way at the
 * outset of SurgeSynth.
 *
 * So cool. We now know how we go from synth to filter. The synth creates QaudFilterChainStates. It
 * then assigns a particular voice to update the input data of that chain state in a block. That
 * means the SSE-wide application is done. Then it tells the filter 'go' which goes and runs the
 * various connected filters (QuadFilterChain.cpp) down to the individual filters
 * (QuadFilterUnit.cpp) which then do the parallel evaluation.
 *
 * Those evaluators get an 'active' mask on the QuadFilterChainState which tells them which voice
 * is on and off, useful if you need to unroll.
 *
 * So now the only thing we are missing is: how do we make coefficients and maintain state, since
 * filters are stateful things? Well, this is the job of FilterCoefficientMaker. It updates a set
 * of registers (storage) and coefficients (inputs) to each filter based on the filter type at
 * the start of every block. Moreover, it provides the derivatives-with-respect-to-sample of
 * those items, such that as the filter goes across the block it can smoothly interpolate the
 * coefficients. These are set up either by direct change (with FromDirect) or individually
 * based on filters. And of course, this is where a lot of the math goes. The SSE filters need
 * to set up the various polynomial and non-linear coefficients here. But once they have the
 * individual operator, they basically do a "Coefficient += dCoeff" for each block and then evaluate
 * at the new coefficient, making the filters respond to modulation seamlessly.
 *
 * And now we are really almost done. The only thing left is the new filters we started adding
 * in Surge 1.8. They use a much more friendly pattern than the old ones. Basically, for a given
 * filter, we add a .h and a .cpp which contain namespaced makeCoefficients and process functions,
 * bind those into FilterCoefficientMaker, and voila. Take a look, for instance, at VintageLadders.h
 * and VintageLadders.cpp.
 *
 * Finally, the filter types, subtypes and names thereof are enumerated in SurgeStorage.h.
 *
 * There are a few more Surge filter functions - the Allpass BiquadFilter and VectorizedSVFFilter
 * are used by various FX in a different context and they don't follow this architecture. That code
 * is fairly clear, though.
 */

#include "globals.h"
#include "sst/filters.h"
#include "sst/waveshapers.h"

struct QuadFilterChainState
{
    sst::filters::QuadFilterUnitState FU[4];      // 2 filters left and right
    sst::waveshapers::QuadWaveshaperState WSS[2]; // 1 shaper left and right

    SIMD_M128 Gain, FB, Mix1, Mix2, Drive;
    SIMD_M128 dGain, dFB, dMix1, dMix2, dDrive;

    SIMD_M128 wsLPF, FBlineL, FBlineR;

    SIMD_M128 DL[BLOCK_SIZE_OS], DR[BLOCK_SIZE_OS]; // wavedata

    SIMD_M128 OutL, OutR, dOutL, dOutR;
    SIMD_M128 Out2L, Out2R, dOut2L, dOut2R; // fc_stereo only
};

/*
** I originally had this as a member but since moved it out of line so as to
** not run any risk of alignment problems in QuadFilterChainState where
** only the head of the array is __align_malloced
*/
void InitQuadFilterChainStateToZero(QuadFilterChainState *Q);

struct fbq_global
{
    sst::filters::FilterUnitQFPtr FU1ptr, FU2ptr;
    sst::waveshapers::QuadWaveshaperPtr WSptr;
};

typedef void (*FBQFPtr)(QuadFilterChainState &, fbq_global &, float *, float *);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);

#endif // SURGE_SRC_COMMON_DSP_QUADFILTERCHAIN_H
