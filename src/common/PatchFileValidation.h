// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "filesystem/import.h"
#include <string>
#include <vector>

namespace Surge::PatchStorage
{
// Read and validate without touching synthesis, editor, or tuning state.
// The returned bytes retain the existing raw Surge patch format.
bool readValidatedPatch(const fs::path &path, std::vector<char> &data, std::string &error);
}
