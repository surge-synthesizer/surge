#pragma once

#include <string>
#if ESCAPE_FROM_VSTGUI
#include <efvg/escape_from_vstgui.h>
#else
#include <vstgui/vstgui.h>
#endif
#include "SurgeStorage.h"
#include "UserDefaults.h"

namespace Surge
{
namespace UI
{
std::string toOSCaseForMenu(std::string menuName);

extern bool showCursor(SurgeStorage *storage);

bool get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y, float p2_x, float p2_y,
                           float p3_x, float p3_y, float *i_x, float *i_y);

struct NonIntegralAntiAliasGuard
{
    NonIntegralAntiAliasGuard(VSTGUI::CDrawContext *dc)
    {
        this->mode = dc->getDrawMode();
        this->dc = dc;
        this->dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);
    }

    ~NonIntegralAntiAliasGuard() { this->dc->setDrawMode(mode); }

    VSTGUI::CDrawContext *dc;
    VSTGUI::CDrawMode mode;
};
} // namespace UI
} // namespace Surge
