#pragma once
#include <vector>

struct MSEGModulationHelper {
   struct segment {
      float duration;
      float v0, v1;
      float cpduration, cpv;
      enum {
         CONSTANT,
         LINEAR,
         QUADBEZ
      } type;
   };

   std::vector<segment> segments;
   float valueAt(float unwrappedPhase, float deform );

   MSEGModulationHelper();
};
