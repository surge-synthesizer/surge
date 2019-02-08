#include "HeadlessApps.h"
#include "HeadlessPluginLayerProxy.h"

namespace Surge
{
namespace Headless
{

SurgeSynthesizer* createSurge()
{
   HeadlessPluginLayerProxy* parent = new HeadlessPluginLayerProxy();
   SurgeSynthesizer* surge = new SurgeSynthesizer(parent);
   surge->setSamplerate(44100);
   return surge;
}

} // namespace Headless
} // namespace Surge
