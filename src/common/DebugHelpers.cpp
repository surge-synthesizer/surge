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
#include "DebugHelpers.h"

#if WINDOWS
#include "windows.h"
#endif

#if MAC || LINUX
#include <stdio.h>
#include <cstdlib>
#endif

#if __GLIBC__ || MAC
#include <execinfo.h>
#endif

#include "version.h"
#include <iostream>
#include <iomanip>

#if WINDOWS
static bool winconinitialized = false;
static FILE *confp;
#endif

#include <atomic>

bool Surge::Debug::openConsole()
{
#if WINDOWS
    if (!winconinitialized)
    {
        winconinitialized = true;
        AllocConsole();
        freopen_s(&confp, "CONOUT$", "w", stdout);
        std::cout << "SURGE DEBUG CONSOLE\n\n"
                  << "This is where we show stdout from Surge XT, for debugging purposes. If you "
                     "close this window, Surge XT will crash!\n"
                  << "Version: " << Build::FullVersionStr << ", built on " << Build::BuildDate
                  << " at " << Build::BuildTime << "\n\n"
                  << std::endl;
    }
    return winconinitialized;
#else
    return true;
#endif
}
bool Surge::Debug::toggleConsole()
{
#if WINDOWS

    if (!winconinitialized)
    {
        openConsole();
    }
    else
    {
        winconinitialized = false;
        fclose(confp);
        FreeConsole();
    }

    return winconinitialized;
#else
    return false;
#endif
}

void Surge::Debug::stackTraceToStdout(int depth)
{
#if __GLIBC__ || MAC
    void *callstack[128];
    int i, frames = backtrace(callstack, 128);
    char **strs = backtrace_symbols(callstack, frames);
    if (depth < 0)
        depth = frames;
    printf("-------- Stack Trace (%d frames of %d depth showing) --------\n", depth, frames);
    for (i = 1; i < frames && i < depth; ++i)
    {
        printf("  [%3d]: %s\n", i, strs[i]);
    }
    free(strs);
#endif
}

static std::atomic<int> lcdepth(0);
Surge::Debug::LifeCycleToConsole::LifeCycleToConsole(const std::string &st) : s(st)
{
    lcdepth++;
    for (int i = 0; i < lcdepth; ++i)
        printf(">--");
    printf("> %s\n", st.c_str());
}
Surge::Debug::LifeCycleToConsole::~LifeCycleToConsole()
{
    for (int i = 0; i < lcdepth; ++i)
        printf("<--");
    printf("< %s\n", s.c_str());
    lcdepth--;
}

Surge::Debug::TimeBlock::TimeBlock(const std::string &itag) : tag(itag)
{
    start = std::chrono::high_resolution_clock::now();
}

Surge::Debug::TimeBlock::~TimeBlock()
{
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "TimeBlock[" << tag << "]=" << duration.count() << " microsec" << std::endl;
}
