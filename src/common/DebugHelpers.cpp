#include "DebugHelpers.h"

#if WINDOWS
#include "Windows.h"
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
#endif
}

void Surge::Debug::stackTraceToStdout()
{
    printf("Hey @baconpaul do this!\n");
}
