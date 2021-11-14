#pragma once
#include <iostream>

namespace Surge
{
namespace Headless
{
namespace NonTest
{
void initializePatchDB();
void restreamTemplatesWithModifications();
void statsFromPlayingEveryPatch();
void filterAnalyzer(int ft, int fst, std::ostream &os);
void generateNLFeedbackNorms();
[[noreturn]] void performancePlay(const std::string &patchName, int mode);
} // namespace NonTest
} // namespace Headless
} // namespace Surge
