#pragma once
#include <iostream>

namespace Surge
{
namespace Headless
{
namespace NonTest
{
void restreamTemplatesWithModifications();
void statsFromPlayingEveryPatch();
void playSomeBach();
void filterAnalyzer(int ft, int fst, std::ostream &os);
void generateNLFeedbackNorms();
void performancePlay(const std::string &patchName, int mode);
} // namespace NonTest
} // namespace Headless
} // namespace Surge
