#pragma once

#include "SurgeStorage.h"
#include "UserDefaults.h"

namespace Surge
{
namespace GUI
{
extern bool showCursor(SurgeStorage *storage);
extern bool allowKeyboardEdits(SurgeStorage *storage);
extern bool isTouchMode(SurgeStorage *storage);

bool get_line_intersection(float p0_x, float p0_y, float p1_x, float p1_y, float p2_x, float p2_y,
                           float p3_x, float p3_y, float *i_x, float *i_y);

void openFileOrFolder(const std::string &f);
inline void openFileOrFolder(const fs::path &f) { openFileOrFolder(path_to_string(f)); }
} // namespace GUI
} // namespace Surge
