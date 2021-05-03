/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_CPUFEATURES_H
#define SURGE_XT_CPUFEATURES_H

#include <string>

namespace Surge
{
namespace CPUFeatures
{
std::string cpuBrand();
bool isArm();
bool isX86();
bool hasSSE2();
bool hasAVX();

struct FPUStateGuard
{
    FPUStateGuard();
    ~FPUStateGuard();

    int priorS;
};
}; // namespace CPUFeatures
} // namespace Surge

#endif // SURGE_XT_CPUFEATURES_H
