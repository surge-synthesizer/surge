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

#ifndef SURGE_SRC_COMMON_DEBUGHELPERS_H
#define SURGE_SRC_COMMON_DEBUGHELPERS_H
#include <string>
#include <chrono>

#define _DBGCOUT std::cout << __FILE__ << ":" << __LINE__ << "|" << __func__ << "| "
#define _D(x) " " << (#x) << "=" << x
#define _DTN(x) " typeof:" << (#x) << "=" << typeid(x).name()
#define _R(x, y) Surge::Debug::LifeCycleToConsole y(x);

namespace Surge
{
namespace Debug
{

bool openConsole();   // no-op on Linux and macOS; force console open on Windows
bool toggleConsole(); // no-op on Linux and macOS; shows or closes console on Windows

void stackTraceToStdout(int depth = -1); // no-op on Windows; shows stack trace on macOS and Linunx

struct LifeCycleToConsole
{ // simple class which printfs in ctor and dtor
    LifeCycleToConsole(const std::string &s);
    ~LifeCycleToConsole();
    std::string s;
};

struct TimeBlock
{
    TimeBlock(const std::string &tag);
    ~TimeBlock();
    std::string tag;
    std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

} // namespace Debug
} // namespace Surge

#endif // SURGE_SRC_COMMON_DEBUGHELPERS_H
