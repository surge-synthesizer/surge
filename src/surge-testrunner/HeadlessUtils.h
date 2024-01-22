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
#ifndef SURGE_SRC_SURGE_TESTRUNNER_HEADLESSUTILS_H
#define SURGE_SRC_SURGE_TESTRUNNER_HEADLESSUTILS_H

#include "SurgeSynthesizer.h"

namespace Surge
{
namespace Headless
{

std::shared_ptr<SurgeSynthesizer> createSurge(int sr, bool loadAllPatches = false);

void writeToStream(const float *data, int nSamples, int nChannels, std::ostream &str);

/*
** One imagines expansions along these lines:

void writeGnuplotFile(const float *data, int nSamples, int nChannels, std::string fname);

*/

} // namespace Headless
} // namespace Surge

#endif // SURGE_SRC_SURGE_TESTRUNNER_HEADLESSUTILS_H
