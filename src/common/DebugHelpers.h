/*
** DebugHelpers.h
**
** Cotnains debug APIs. Do not use them in production code.
** Pull requests with references to these functions will not be merged!
**
*/

#pragma once

#define _D(x) " " << (#x) << "=" << x

namespace Surge {
namespace Debug {


bool openConsole(); // no-op on Linux and macOS; force console open on Windows
bool toggleConsole(); // no-op on Linux and macOS; shows or closes console on Windows

void stackTraceToStdout(); // no-op on Windows; shows stack trace on macOS and Linunx

}
}