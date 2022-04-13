#pragma once
#include <vector>
#include "SurgeStorage.h"
#include <random>

namespace Surge
{
namespace MSEG
{
void rebuildCache(MSEGStorage *ms);

struct EvaluatorState
{
    EvaluatorState()
    {
        std::random_device rd;
        gen = std::minstd_rand(rd());
        urd = std::uniform_real_distribution<float>(-1.0, 1.0);
    }
    int lastEval = -1;
    float lastOutput = 0;
    // 6 is NOT the number of LFOs, but number of MSEG state elements!
    // TODO: replace 6 with a constexpr!
    float msegState[6] = {0};
    bool released = false, retrigger_FEG = false, retrigger_AEG = false, has_triggered = false;
    enum LoopState
    {
        PLAYING,
        RELEASING
    } loopState = PLAYING;
    double releaseStartPhase;
    double timeAlongSegment;
    float releaseStartValue;
    std::minstd_rand gen;
    std::uniform_real_distribution<float> urd;

    void seed(long l) { gen = std::minstd_rand(l); }
};
float valueAt(int phaseIntPart, float phaseFracPart, float deform, MSEGStorage *ms,
              EvaluatorState *state, bool forceOneShot = false);

/*
** Edit and Utility functions. After the call to all of these you will want to rebuild cache
*/
int timeToSegment(MSEGStorage *ms, double t); // these are double to deal with very long phases
int timeToSegment(MSEGStorage *ms, double t, bool ignoreLoops, float &timeAlongSegment);
void changeTypeAt(MSEGStorage *ms, float t, MSEGStorage::segment::Type type);
void insertAfter(MSEGStorage *ms, float t);
void insertBefore(MSEGStorage *ms, float t);
void insertAtIndex(MSEGStorage *ms, int idx);

void extendTo(MSEGStorage *ms, float t, float mv);
void splitSegment(MSEGStorage *ms, float t, float nv);
void unsplitSegment(MSEGStorage *ms, float t,
                    bool wrapTime = false); // for unsplit we often want past-end-is-last
void deleteSegment(MSEGStorage *ms, float t);
void deleteSegment(MSEGStorage *ms, int idx);

void adjustDurationShiftingSubsequent(MSEGStorage *ms, int idx, float dx, float snap = 0,
                                      float maxDuration = -1);
void adjustDurationConstantTotalDuration(MSEGStorage *ms, int idx, float dx, float snap = 0);

void resetControlPoint(MSEGStorage *ms, float t);
void resetControlPoint(MSEGStorage *ms, int idx);
void constrainControlPointAt(MSEGStorage *ms, int idx);

void scaleDurations(MSEGStorage *ms, float factor, float maxDuration = -1);
void scaleValues(MSEGStorage *ms, float factor);
void setAllDurationsTo(MSEGStorage *ms, float value);
void mirrorMSEG(MSEGStorage *ms);

void clearMSEG(MSEGStorage *ms);
void createInitVoiceMSEG(MSEGStorage *ms);
void createInitSceneMSEG(MSEGStorage *ms);
void createStepseqMSEG(MSEGStorage *ms, int numSegments);
void createSawMSEG(MSEGStorage *ms, int numSegments, float curve);
void createSinLineMSEG(MSEGStorage *ms, int numSegments);

void modifyEditMode(MSEGStorage *ms, MSEGStorage::EditMode mode);

void setLoopStart(MSEGStorage *ms, int seg);
void setLoopEnd(MSEGStorage *ms, int seg);
} // namespace MSEG
} // namespace Surge
