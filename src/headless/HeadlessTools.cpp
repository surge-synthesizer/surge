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

bool loadPatchByIndex(SurgeSynthesizer* s, int index)
{
   if (index > s->storage.patch_list.size() || index < 0)
      return false;

   s->patchid_queue = index;
   s->processThreadunsafeOperations();

   return true;
}

} // namespace Headless
} // namespace Surge
