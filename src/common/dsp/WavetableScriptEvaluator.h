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

#ifndef SURGE_XT_WAVETABLESCRIPTEVALUATOR_H
#define SURGE_XT_WAVETABLESCRIPTEVALUATOR_H

#include "SurgeStorage.h"
#include "StringOps.h"
#include "Wavetable.h"

namespace Surge
{
namespace WavetableScript
{
/*
 * Unlike the LFO modulator this is called at render time of the wavetable
 * not at the evaluation or synthesis time. As such I expect you call it from
 * one thread at a time and just you know generally be careful.
 */
std::vector<float> evaluateScriptAtFrame(const std::string &eqn, int resolution, int frame,
                                         int nFrames);

/*
 * Generate all the data required to call BuildWT. The wavdata here is data you
 * must free with delete[]
 */
bool constructWavetable(const std::string &eqn, int resolution, int frames, wt_header &wh,
                        float **wavdata);

std::string defaultWavetableFormula();

} // namespace WavetableScript
} // namespace Surge
#endif // SURGE_XT_WAVETABLESCRIPTEVALUATOR_H
