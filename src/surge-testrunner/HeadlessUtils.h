/*
** HeadlessUtils provides a collection of utility functions to manage and output
** data and construct and manipulate surge
*/
#pragma once

#include "SurgeSynthesizer.h"

namespace Surge
{
namespace Headless
{

std::shared_ptr<SurgeSynthesizer> createSurge(int sr, bool loadAllPatches = false);

void writeToStream(const float *data, int nSamples, int nChannels, std::ostream &str);

/*
** One imagines expansions along these lines:

void writeGnuplotFile(const float *data, int nSamples, int nChannels, std::string fname);

*/

} // namespace Headless
} // namespace Surge
