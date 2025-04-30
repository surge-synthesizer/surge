
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

#ifndef SURGE_SRC_SURGE_XT_UTIL_LUATOKENISERSURGE_H
#define SURGE_SRC_SURGE_XT_UTIL_LUATOKENISERSURGE_H

#include <juce_gui_extra/juce_gui_extra.h>

namespace Surge
{

class LuaTokeniserSurge : public juce::CodeTokeniser
{
  public:
    //==============================================================================
    LuaTokeniserSurge();
    ~LuaTokeniserSurge() override;

    //==============================================================================
    int readNextToken(juce::CodeDocument::Iterator &) override;
    juce::CodeEditorComponent::ColourScheme getDefaultColourScheme() override;

    /** The token values returned by this tokeniser. */
    enum TokenType
    {
        tokenType_error = 0,
        tokenType_comment,
        tokenType_keyword,
        tokenType_operator,
        tokenType_identifier,
        tokenType_integer,
        tokenType_float,
        tokenType_string,
        tokenType_bracket,
        tokenType_punctuation
    };
};

} // namespace Surge

#endif
