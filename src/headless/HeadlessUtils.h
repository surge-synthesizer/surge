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

SurgeSynthesizer* createSurge(int sr);

void writeToStream(const float* data, int nSamples, int nChannels, std::ostream& str);

/*
** One imagines expansions along these lines:

void writeWavFile(const float * data, int nSamples, int nChannels, std::string fname);
void writeGnuplotFile(const float *data, int nSamples, int nChannels, std::string fname);

*/

} // namespace Headless
} // namespace Surge
