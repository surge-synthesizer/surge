/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
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
