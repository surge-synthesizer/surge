#pragma once
#include <vector>
#include "SurgeStorage.h"
#include <random>

namespace Surge
{
   namespace MSEG {
      void rebuildCache(MSEGStorage *s);

      struct EvaluatorState {
         EvaluatorState()
         {
            std::random_device rd;
            gen = std::minstd_rand(rd());
            urd = std::uniform_real_distribution<float>(-1.0,1.0);
         }
         int lastEval = -1;
         float lastOutput = 0;
         float msegState[6] = {0};
         bool released = false;
         enum LoopState {
            PLAYING,
            RELEASING
         } loopState = PLAYING;
         double releaseStartPhase;
         float releaseStartValue;
         std::minstd_rand gen;
         std::uniform_real_distribution<float> urd;

         void seed( long l )
         {
            gen = std::minstd_rand(l);
         }
      };
      float valueAt(int phaseIntPart, float phaseFracPart, float deform, MSEGStorage *s,
                    EvaluatorState *state, bool forceOneShot = false);

      /*
      ** Edit and Utility functions. After the call to all of these you will want to rebuild cache
      */
      int timeToSegment( MSEGStorage *s, double t ); // these are double to deal with very long phases
      int timeToSegment( MSEGStorage *s, double t, bool ignoreLoops, float &timeAlongSegment );
      void changeTypeAt( MSEGStorage *s, float t, MSEGStorage::segment::Type type );
      void insertAfter( MSEGStorage *s, float t );
      void insertBefore( MSEGStorage *s, float t );
      void insertAtIndex( MSEGStorage *s, int idx );

      void extendTo( MSEGStorage *s, float t, float mv );
      void splitSegment( MSEGStorage *s, float t, float nv );
      void unsplitSegment( MSEGStorage *s, float t );
      void deleteSegment( MSEGStorage *s, float t );

      void resetControlPoint( MSEGStorage *s, float t );
      void resetControlPoint( MSEGStorage *s, int idx );
      void constrainControlPointAt( MSEGStorage *s, int idx );

      void scaleDurations(MSEGStorage* s, float factor);
      void scaleValues(MSEGStorage* s, float factor);
      void setAllDurationsTo(MSEGStorage* s, float value);
   }
}
