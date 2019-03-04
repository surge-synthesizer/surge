#include "HeadlessUtils.h"
#include "HeadlessPluginLayerProxy.h"

#include <iostream>
#include <iomanip>

namespace Surge
{
namespace Headless
{
SurgeSynthesizer* createSurge(int sr)
{
   HeadlessPluginLayerProxy* parent = new HeadlessPluginLayerProxy();
   SurgeSynthesizer* surge = new SurgeSynthesizer(parent);
   surge->setSamplerate(sr);
   return surge;
}

void writeToStream(const float* data, int nSamples, int nChannels, std::ostream& str)
{
   int overSample = 8;
   float avgOut = 0;
   for (auto i = 0; i < nSamples; i += nChannels)
   {
      for (auto j = 0; j < nChannels; ++j)
         avgOut += data[i + j] / nChannels;
      if ((i % overSample) == 0)
      {
         avgOut /= overSample;
         int gWidth = (int)((avgOut + 1) * 40);
         str << "Sample: " << std::setw(15) << avgOut << std::setw(gWidth) << "X" << std::endl;

         avgOut = 0;
      }
   }
}

} // namespace Headless
} // namespace Surge
