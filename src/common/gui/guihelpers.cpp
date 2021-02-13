#include "guihelpers.h"

#include <cctype>

namespace Surge
{
namespace UI
{

std::string toOSCaseForMenu(std::string menuName)
{
#if WINDOWS
    for (auto i = 1; i < menuName.length() - 1; ++i)
        if (!(isupper(menuName[i]) && (isupper(menuName[i + 1]) || !isalpha(menuName[i + 1]))))
        {
            menuName[i] = std::tolower(menuName[i]);
        }
#endif
    return menuName;
}

bool showCursor(SurgeStorage *storage)
{
    bool sc = Surge::Storage::getUserDefaultValue(storage, "showCursorWhileEditing", 0);
    bool tm = Surge::Storage::getUserDefaultValue(storage, "touchMouseMode", false);

    return sc || tm;
};

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

} // namespace UI
} // namespace Surge
