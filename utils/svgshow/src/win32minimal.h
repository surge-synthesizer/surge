#pragma once

#include <windows.h>
#include <functional>
#include "vstgui/lib/vstguifwd.h"

int win32minimal_main( HINSTANCE hInstance,
		       int w, int h, int nCmdShow,
		       std::function<void(VSTGUI::CFrame *f)> frameCb);
