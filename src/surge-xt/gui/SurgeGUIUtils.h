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
#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEGUIUTILS_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEGUIUTILS_H

#include "SurgeStorage.h"
#include "UserDefaults.h"
#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace GUI
{
extern bool showCursor(SurgeStorage *storage);
extern bool allowKeyboardEdits(SurgeStorage *storage);
extern bool isTouchMode(SurgeStorage *storage);

bool getLineIntersection(float p0_x, float p0_y, float p1_x, float p1_y, float p2_x, float p2_y,
                         float p3_x, float p3_y, float *i_x, float *i_y);

void constrainPointOnLineWithinRectangle(const juce::Rectangle<float> rect,
                                         const juce::Line<float> l, juce::Point<float> &p);

void openFileOrFolder(const std::string &f);
inline void openFileOrFolder(const fs::path &f) { openFileOrFolder(path_to_string(f)); }

// Are we standalone exe vs a plugin (clap, vst, au, etc...)
void setIsStandalone(bool);
bool getIsStandalone();

} // namespace GUI
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_SURGEGUIUTILS_H
