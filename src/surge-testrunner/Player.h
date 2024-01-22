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

#ifndef SURGE_SRC_SURGE_TESTRUNNER_PLAYER_H
#define SURGE_SRC_SURGE_TESTRUNNER_PLAYER_H

#include "HeadlessUtils.h"
#include <vector>

namespace Surge
{
namespace Headless
{

/*
** Event and the std::vector<Event> (typedefed as playerEvents_t in case we need to
** change that data structure) hold a sequence of midi-like events. Right now we
** only support note on and note off at a particular sample, or NO_EVENT, which is
** useful for holding for tails. channel, data1, and data2 follow the MIDI 1.0 naming
** convention.
*/
struct Event
{
    typedef enum Type
    {
        NOTE_ON,
        NOTE_OFF,
        LAMBDA_EVENT,

        NO_EVENT // Useful if you want to keep the player running and have nothing happen
    } Type;      // FIXME: Controllers etc...

    Type type;
    char channel;
    char data1;
    char data2;

    std::function<void(std::shared_ptr<SurgeSynthesizer>)> surgeLambda;

    long atSample;
};

typedef std::vector<Event> playerEvents_t; // We assume these are monotonic in Event.atSample

/**
 * makeHoldMiddleC
 *
 * Returns an event stream holding middle C
 */
playerEvents_t makeHoldMiddleC(int forSamples, int withTail = 0);

playerEvents_t makeHoldNoteFor(int note, int forSamples, int withTail = 0, int midiChannel = 0);

playerEvents_t make120BPMCMajorQuarterNoteScale(long sample0 = 0, int sr = 44100);

/**
 * playAsConfigured
 *
 * given a surge, play the events from first to last accumulating the result in the audiodata
 */
void playAsConfigured(std::shared_ptr<SurgeSynthesizer> synth, const playerEvents_t &events,
                      float **resultData, int *nSamples, int *nChannels);

/**
 * playOnPatch
 *
 * given a surge and a patch, play the events accumulating the data. This is a convenience
 * for loadpatch / playAsConfigured
 */
void playOnPatch(std::shared_ptr<SurgeSynthesizer> synth, int patch, const playerEvents_t &events,
                 float **resultData, int *nSamples, int *nChannels);

/**
 * playOnEveryPatch
 *
 * Play the events on every patch Surge knows callign the callback for each one with
 * the result.
 */
void playOnEveryPatch(std::shared_ptr<SurgeSynthesizer> synth, const playerEvents_t &events,
                      std::function<void(const Patch &p, const PatchCategory &c, const float *data,
                                         int nSamples, int nChannels)>
                          completedCallback);

/**
 * playOnEveryNRandomPatches
 *
 * Play the events on every patch Surge knows callign the callback for each one with
 * the result.
 */
void playOnNRandomPatches(std::shared_ptr<SurgeSynthesizer> synth, const playerEvents_t &events,
                          int nPlays,
                          std::function<void(const Patch &p, const PatchCategory &c,
                                             const float *data, int nSamples, int nChannels)>
                              completedCallback);

} // namespace Headless
} // namespace Surge

#endif // SURGE_SRC_SURGE_TESTRUNNER_PLAYER_H
