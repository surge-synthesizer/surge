#pragma once

#include <functional>
#include "vstgui/lib/vstguifwd.h"

void cocoa_minimal_main(int w, int h, std::function<void(VSTGUI::CFrame *f)> frameCb);
