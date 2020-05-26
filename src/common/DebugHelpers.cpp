#include "DebugHelpers.h"

#if WINDOWS
#include "Windows.h"
#endif

void Surge::Debug::toggleConsole()
{
#if WINDOWS
    static bool initialized = false;

    FILE *fp;

    if (!initialized)
    {
        initialized = true;

        AllocConsole();
        freopen_s(&fp, "CONOUT$", "w", stdout);
    }
    else
    {
        initialized = false;
        fclose(fp);
        FreeConsole();
    }
#endif
}

void Surge::Debug::stackTraceToStdout()
{
    printf("Hey @baconpaul do this!\n");
}
