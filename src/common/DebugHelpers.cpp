#include "DebugHelpers.h"

#if WINDOWS
#include "Windows.h"
#endif

#if MAC || LINUX
#include <execinfo.h>
#include <stdio.h>
#include <cstdlib>
#endif

bool Surge::Debug::toggleConsole()
{
#if WINDOWS
    static bool initialized = false;

    static FILE *fp;

    if (!initialized)
    {
        initialized = true;

        AllocConsole();
        freopen_s(&fp, "CONOUT$", "w", stdout);
        printf("SURGE DEBUG CONSOLE\nThis console is for debug output. If you close it all bets are off!\n\n");
    }
    else
    {
        initialized = false;
        fclose(fp);
        FreeConsole();
    }

    return initialized;
#else
    return false;
#endif
}

void Surge::Debug::stackTraceToStdout()
{
#if MAC || LINUX
    void* callstack[128];
    int i, frames = backtrace(callstack, 128);
    char** strs = backtrace_symbols(callstack, frames);
    printf( "-------- StackTrace (%d frames deep) --------\n", frames );
    for (i = 1; i < frames; ++i) {
        printf( "  [%3d]: %s\n", i, strs[i] );
    }
    free(strs);
#endif

}
