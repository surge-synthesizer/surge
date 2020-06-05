#include <iostream>
#include <iomanip>
#include "version.h"
#include <windows.h>
#include <string>

/*
** This is a little standalone C++ program which tries to load a dll and give you
** error messages, specifically it tries to load the vst3 dll
*/

void showError( DWORD e, const char* pfx )
{
    std::cout << "  | " << pfx << " (" << std::hex << e << std::dec << ")\n";

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, e, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);
    LocalFree(messageBuffer);

    std::cout << "  | Message: " << message << std::endl;

}

int main( int argc, char **argv )
{
    std::cout << "dllcheck\n"
            << "Built against codebase with " << Surge::Build::FullVersionStr << "\n";
    if( argc != 2 )
    {
        std::cout << "Usage: dllcheck (vst3name)\n";
        exit(1);
    }
    std::cout << "Trying to load '" << argv[1] << "'" << std::endl;

    auto h = LoadLibrary( argv[1] );
    if( h == NULL )
    {
        showError( GetLastError(), "LoadLibrary failed" );
        exit(1);
    }
    std::cout << "Able to load library : " << h << std::endl;

    auto pa = GetProcAddress( h, "GetPluginFactory" );
    if( pa == NULL ) {
        showError( GetLastError(), "GetPluginFactory not loadable" );
        exit( 1 );
    }
    std::cout << "GetPluginFactory was loadable. Things looking good" << std::endl;

    auto pf = (pa)();
    if( pf == NULL ) {
        showError( GetLastError(), "Unable to run GetPluginFactory" );
        exit( 1 );
    }
    std::cout << "Able to run GetPluginFactory - return value is " << std::hex << pf << std::endl;

}