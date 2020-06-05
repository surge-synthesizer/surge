#include "DebugHelpers.h"

#if WINDOWS
#include "Windows.h"
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

bool Surge::Debug::openConsole()
{
#if WINDOWS   
    if( ! winconinitialized )
    {
        winconinitialized = true;
        AllocConsole();
        freopen_s(&confp, "CONOUT$", "w", stdout);
        std::cout << "Surge Debugging Console\n"
                  << "This console shows stdout from the surge plugin. If you close it with X you might crash.\n"
                  << "Version: " << Build::FullVersionStr << " built at " << Build::BuildDate << " " << Build::BuildTime
                  << "\n\n" << std::endl;
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
