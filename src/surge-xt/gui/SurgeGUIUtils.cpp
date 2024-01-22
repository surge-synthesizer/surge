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
#include "SurgeGUIUtils.h"

#include "juce_core/juce_core.h"
#include "UserDefaults.h"

#if LINUX
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Surge
{
namespace GUI
{
bool showCursor(SurgeStorage *storage)
{
    bool sc =
        Surge::Storage::getUserDefaultValue(storage, Surge::Storage::ShowCursorWhileEditing, 0);
    bool tm = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::TouchMouseMode, false);

    return sc || tm;
};

bool isTouchMode(SurgeStorage *storage)
{
    bool tm = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::TouchMouseMode, false);
    return tm;
}

/*
 * We want to know if we are standalone here, but we don't have a reference to the processor
 * Since that is a constant for a process (you can't mix standalone and not), make it a static
 * and explicitly grab this symbol in the SurgeGUIEditor startup path
 */
static bool isStandalone{false};
void setIsStandalone(bool b) { isStandalone = b; }
bool getIsStandalone() { return isStandalone; }

bool allowKeyboardEdits(SurgeStorage *storage)
{
    if (!storage)
        return false;

    bool res{false};
    if (isStandalone)
    {
        res = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::UseKeyboardShortcuts_Standalone, true);
    }
    else
    {
        res = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::UseKeyboardShortcuts_Plugin, false);
    }

    return res;
}

/*
 * Returns true if the two lines (p0-p1 and p2-p3) intersect.
 * In addition, if the lines intersect, the intersection point may be stored to floats i_x and i_y.
 */
bool getLineIntersection(float p0_x, float p0_y, float p1_x, float p1_y, float p2_x, float p2_y,
                         float p3_x, float p3_y, float *i_x, float *i_y)
{
    float s1_x, s1_y, s2_x, s2_y;
    float s, t;

    s1_x = p1_x - p0_x;
    s1_y = p1_y - p0_y;
    s2_x = p3_x - p2_x;
    s2_y = p3_y - p2_y;

    s = (-s1_y * (p0_x - p2_x) + s1_x * (p0_y - p2_y)) / (-s2_x * s1_y + s1_x * s2_y);
    t = (s2_x * (p0_y - p2_y) - s2_y * (p0_x - p2_x)) / (-s2_x * s1_y + s1_x * s2_y);

    if (s >= 0 && s <= 1 && t >= 0 && t <= 1)
    {
        // Collision detected
        if (i_x != NULL)
        {
            *i_x = p0_x + (t * s1_x);
        }

        if (i_y != NULL)
        {
            *i_y = p0_y + (t * s1_y);
        }

        return true;
    }

    return false; // No collision
}

void constrainPointOnLineWithinRectangle(const juce::Rectangle<float> rect,
                                         const juce::Line<float> l, juce::Point<float> &p)
{
    auto th = juce::Line<float>{rect.getTopLeft(), rect.getTopRight()};
    auto bh = juce::Line<float>{rect.getBottomLeft(), rect.getBottomRight()};
    auto lv = juce::Line<float>{rect.getTopLeft(), rect.getBottomLeft()};
    auto rv = juce::Line<float>{rect.getTopRight(), rect.getBottomRight()};
    juce::Point<float> pt;

    if (l.intersects(th, pt))
    {
        p = pt;
    }
    else if (l.intersects(bh, pt))
    {
        p = pt;
    }
    else if (l.intersects(lv, pt))
    {
        p = pt;
    }
    else if (l.intersects(rv, pt))
    {
        p = pt;
    }
}

void openFileOrFolder(const std::string &f)
{
    auto path = juce::File(f);

    if (path.isDirectory())
    {
        // See this for why we branch out Linux here
        // https://forum.juce.com/t/linux-spaces-in-path-startasprocess-and-process-opendocument/47296
#if LINUX
        if (vfork() == 0)
        {
            if (execlp("xdg-open", "xdg-open", f.c_str(), (char *)nullptr) < 0)
            {
                _exit(0);
            }
        }
#else
        path.startAsProcess();
#endif
    }
    else
    {
        path.revealToUser();
    }
}

} // namespace GUI
} // namespace Surge
