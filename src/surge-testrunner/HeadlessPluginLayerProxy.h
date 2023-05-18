/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2023, various authors, as described in the GitHub
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
#ifndef SURGE_SRC_SURGE_TESTRUNNER_HEADLESSPLUGINLAYERPROXY_H
#define SURGE_SRC_SURGE_TESTRUNNER_HEADLESSPLUGINLAYERPROXY_H

#include <iostream>
#include <iomanip>

class HeadlessPluginLayerProxy : public SurgeSynthesizer::PluginLayer
{
  public:
    void surgeParameterUpdated(const SurgeSynthesizer::ID &id, float d) override {}
    void surgeMacroUpdated(long macroNum, float d) override {}
};

#endif // SURGE_SRC_SURGE_TESTRUNNER_HEADLESSPLUGINLAYERPROXY_H
