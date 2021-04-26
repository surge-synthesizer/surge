#pragma once

struct QuadFilterUnitState;
class FilterCoefficientMaker;
class SurgeStorage;

namespace VintageLadder
{
namespace RK
{
void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso,
                      bool applyGainCompensation, SurgeStorage *storage);
__m128 process(QuadFilterUnitState *__restrict f, __m128 in);
} // namespace RK

namespace Huov
{
void makeCoefficients(FilterCoefficientMaker *cm, float freq, float reso,
                      bool applyGainCompensation, SurgeStorage *storage);
__m128 process(QuadFilterUnitState *__restrict f, __m128 in);
} // namespace Huov
} // namespace VintageLadder
