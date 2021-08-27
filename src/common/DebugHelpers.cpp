#include "DebugHelpers.h"

#if WINDOWS
#include "windows.h"
#endif

#if MAC || LINUX
#include <execinfo.h>
#include <stdio.h>
#include <cstdlib>
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
                  << "This is where we show stdout from Surge, for debugging purposes. If you "
                     "close this window, Surge will crash!\n"
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
#if MAC || LINUX
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
