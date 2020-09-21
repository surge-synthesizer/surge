#pragma once
#include <vector>
#include "SurgeStorage.h"

struct MSEGModulationHelper {
   static void rebuildCache(MSEGStorage *s);
   static float valueAt(int phaseIntPart, float phaseFracPart, float deform, MSEGStorage *s );
};
