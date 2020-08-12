#pragma once
#include <vector>
#include "SurgeStorage.h"

struct MSEGModulationHelper {
   static void rebuildCache(MSEGStorage *s);
   static float valueAt(float unwrappedPhase, float deform, MSEGStorage *s );
};
