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

#include "SkinSupport.h"
#include <fstream>

void Surge::GUI::loadTypefacesFromPath(const fs::path &p,
                                       std::unordered_map<std::string, juce::Typeface::Ptr> &result)
{
    for (const fs::path &d : fs::directory_iterator(p))
    {
        if (d.has_extension())
        {
            if (path_to_string(d.extension()) == ".ttf")
            {
                auto key = path_to_string(d.stem());

                std::ifstream stream(d, std::ios::in | std::ios::binary);
                std::vector<uint8_t> contents((std::istreambuf_iterator<char>(stream)),
                                              std::istreambuf_iterator<char>());

                auto tf = juce::Typeface::createSystemTypefaceFor((void *)(&contents[0]),
                                                                  contents.size());
                result[key] = tf;
            }
        }
    }
}
