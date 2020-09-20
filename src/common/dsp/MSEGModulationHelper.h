#pragma once
#include <vector>
#include "SurgeStorage.h"

namespace Surge
{
   namespace MSEG {
      void rebuildCache(MSEGStorage *s);
      float valueAt(int phaseIntPart, float phaseFracPart, float deform, MSEGStorage *s );

      /*
      ** Edit and Utility functions. After the call to all of these you will want to rebuild cache
      */
      int timeToSegment( MSEGStorage *s, float t );
      void changeTypeAt( MSEGStorage *s, float t, MSEGStorage::segment::Type type );
      void insertAfter( MSEGStorage *s, float t );
      void insertBefore( MSEGStorage *s, float t );
      void insertAtIndex( MSEGStorage *s, int idx );

      void extendTo( MSEGStorage *s, float t, float mv );
      void splitSegment( MSEGStorage *s, float t, float nv );
      void unsplitSegment( MSEGStorage *s, float t );
      void deleteSegment( MSEGStorage *s, float t );

      void resetControlPoint( MSEGStorage *s, float t );
   }
}
