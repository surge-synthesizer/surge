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

std::string toOSCaseForMenu(const std::string &imenuName)
{
#if WINDOWS
    auto menuName = imenuName;
    for (auto i = 1; i < menuName.length() - 1; ++i)
        if (!(isupper(menuName[i]) && (isupper(menuName[i + 1]) || !isalpha(menuName[i + 1]))))
        {
            menuName[i] = std::tolower(menuName[i]);
        }
    return menuName;
#else
    return imenuName;
#endif
}

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
 * We kinda want to know if we are standalone here, but don't have reference to the processor
 * but that's a constant for a process (you can't mix standalone and not) so make it a static
 * and explicitly grab this symbol in the SGE startup path
 */
static bool isStandalone{false};
void setIsStandalone(bool b) { isStandalone = b; }

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

// Returns 1 if the lines intersect, otherwise 0. In addition, if the lines
// intersect, the intersection point may be stored in the floats i_x and i_y.
bool get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y, float p2_x, float p2_y,
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
            *i_x = p0_x + (t * s1_x);
        if (i_y != NULL)
            *i_y = p0_y + (t * s1_y);
        return true;
    }
    return false; // No collision
}

void openFileOrFolder(const std::string &f)
{
    auto path = juce::File(f);

    if (path.isDirectory())
    {
        // See this for why we branch out linux here
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
