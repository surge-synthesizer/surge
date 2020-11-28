/*
 * The QuadFilterChain is the starting point of the Surge Filter Architecture and so we've placed
 * the overall filter architecture documentation in this header file.
 *
 * There are five basic objects you need to understand to understand the Surge filter architecture
 *
 * QuadFilterUnitState - the collection of state for a particular individual filter
 * FilterCoefficientMaker - an object which creates QuadFilterUniltStates for particular filters
 * QuadFilterChainState - the state of a filter bank, which comprises a pair of filters (perhaps
 *      in stereo) and a waeshaper
 * FilterUnitQFPtr - A function which given a QuadFilterUnitState and an input sample produces output (the filter)
 * WaveShaperQFPtr - A function which waveshapes an input to an output
 *
 * The three things that makes this set of classes somewhat tricky are
 *
 * 1. The variable names and state updates are kinda opaque and are scattered about a bit and
 * 2. Far more importantly they are all parallelized across voices with SSE instructions
 * 3. We calculate coefficients less frequently than filters and interpolate changes across the block
 *
 * So lets deal with the second thing first. At any given time surge can be playing a collection
 * of voices. Each of those voices has a distinct filter configuration since resonance, cutoff,
 * feedback, drive and more can be modulated. As such you have to evaluate the filter for each voice.
 *
 * But in surge the filter *type* for a set of voices in a scene is constant. You may have individual
 * modulation of the cutoff of your filter, but all the voices have the same filter type. So in theory
 * you can share calculations. Imagine a simplest filter out = 0.2 * in + 0.8 * in-1. Its a cruddy filter
 * but you could imagine each SurgeVoice calling it, or you could imagine clusters of four surge voices
 * loading their individual input and state into a single SSE vector _m128 then calling a single SSE
 * operation. That would make one voice and four voices the same CPU load.
 *
 * Once you have that abstraction for your individual filters and wavetables, you can extend it
 * to your entire filter back. This is the job of QuadFilterChainState which contains a pair
 * of filter units, a waveshaper, and various configuration information. Using that configuration information
 * then QuadFilterChain.cpp takes the state with the vector information set up and runs the various
 * topologies, whether that's a simple serial filter or the more complicated ring modulators. These
 * all look like 'given my sets of states run these SSE operations across the voices'. Simple enough
 * once you realie what it doing.
 *
 * That also tells us what the expected state in the QuadFilterChainState would be. You would have
 * 4 filter units (2 needed for mono processing; 4 for stero). You would have configuration for the
 * gain mix feedback, drive. And you would have input and output that you can read and write.
 * That's basically waht you see below. The object also has dGain, which allows for modulation
 * changes (more on that later) and captures the entire wave data for convenience, but logically
 * it is a pair of filters, inputs, and outputs, all SSE vectorized.
 *
 * To use that what do you need to do though? Well, clearly you need to write all of your
 * operators as SSE operators. So the FilterUnitQFPtr and WaveShaperQFPtr need to be _m128 -> _m128
 * not float to float. You can see those in QuadFilterUnit.cpp and so on. These functions are returned
 * by the GetQFPtrFilterUnit function and so forth which does a switch on type to return a function
 * of uniform signature (QuadFilterUnitState, _m128) for filter and _m128 for shaper.
 *
 * Moreover you need to load up the voices into the filters before you process the filters. This
 * is done in SurgeVoice. SurgeVoice::process_block takes two arguments. A QuadFilterUnitChainState
 * and a Qe. Importantly, SurgeVoice does *not* take a float in and out. Rather it has as a job
 * to update the Qe'th SSE position in the QuadFilterUnitChainState handed to it. You can see
 * that SurgeSynth processes the voice in scene 's' as
 * `bool resume = v->process_block(FBQ[s][FBentry[s] >> 2], FBentry[s] & 3);` that is
 * for a given voice number (which is basically FBentry) mod it by 3 and update that point in the
 * filter bank. And that FBQ is created with the _aligned_malloc all the way at the outset of SurgeSynth
 *
 * So cool. We now know how we go form synth to filter. The Synth creates QaudFilterChainStates. It
 * then assigns a particular voice to update the input data of that chain state in a block. That
 * means the SSE-wide application is done. Then it tells the filter 'go' which goes and runs the
 * various connected filters (QuadFilterChain.cpp) down to the individual filters (QuadFilterUnit.cpp)
 * which then do the parallel evaluation.
 *
 * Those evluators get an 'active' mask on the QuadFilterChainState which tells them which voice
 * is on and off, useful if you need to unroll.
 *
 * So now the only thing we are missing is how do we make coefficients and maintain state, since
 * filters are stateful things. Well this is the job of FilterCoefficientMaker. It updates a set
 * of registers (storage) and coefficnents (inputs) to each filter based on the filter type at
 * the start of every block. More over, it provides the derivatives-with-respect-to-sample of
 * those items such that as the filter goes across the block it can smoothly interpolate the
 * coeffiecients. These are set up either by direct change (with FromDirect) or individually
 * based on filters. And of course, this is where a lot of the math goes. The SSE filters need
 * to set up the various polynomial and non-linear coefficients here. But once they have the individual
 * operator basicaly does a "Coefficient += dCoeff" for each block and then evaluates at the
 * new coefficient, makign the filters respond to modulation seamlessly.
 *
 * And now we are really almost done. The only thing left is the new filters we started adding
 * in Surge 1.8. They use a much more friendly pattern than the old ones. Basically for a given
 * filter we add a .h and a .cpp which contains a namespaced makeCoefficnetn and process function,
 * bind those into the coefficient maker, and voila. Take a look for instance at VintageLadders.h
 * and VintageLadders.cpp.
 *
 * Finally the filter types and subtypes and names thereof are enumerated in SurgeStorage.h.
 *
 * There are a few more Surge filer functions - the AllPass Biquadfilter and VectorizedSVFFilter are used
 * by various FX in a different context and they don't follow this architecutre. That code is fairly
 * clear though.
 */

#include "QuadFilterUnit.h"
#pragma once

struct QuadFilterChainState
{
   QuadFilterUnitState FU[4];

   __m128 Gain, FB, Mix1, Mix2, Drive;
   __m128 dGain, dFB, dMix1, dMix2, dDrive;

   __m128 wsLPF, FBlineL, FBlineR;

   __m128 DL[BLOCK_SIZE_OS], DR[BLOCK_SIZE_OS]; // wavedata

   __m128 OutL, OutR, dOutL, dOutR;
   __m128 Out2L, Out2R, dOut2L, dOut2R; // fc_stereo only

};

/*
** I originally had this as a member but since moved it out of line so as to
** not run any risk of alignment problems in QuadFilterChainState where
** only the head of the array is __align_malloced
*/
void InitQuadFilterChainStateToZero(QuadFilterChainState *Q);

struct fbq_global
{
   FilterUnitQFPtr FU1ptr, FU2ptr;
   WaveshaperQFPtr WSptr;
};

typedef void (*FBQFPtr)(QuadFilterChainState&, fbq_global&, float*, float*);

FBQFPtr GetFBQPointer(int config, bool A, bool WS, bool B);
