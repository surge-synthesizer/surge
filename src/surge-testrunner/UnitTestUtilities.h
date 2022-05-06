#pragma once

// #include "catch2/catch2.hpp" // do NOT include this here since we want it included by
// the includer so we can set CATCH_CONFIG_RUNNER properly

#include "SurgeSynthesizer.h"

namespace Surge
{
namespace Test
{

/*
** An approximation of the frequency of a signal using a simple zero crossing
** frequency measure (which works great for the sine patch and poorly for others
** At one day we could do this with autocorrelation instead but no need now.
*/
double frequencyForNote(std::shared_ptr<SurgeSynthesizer> surge, int note, int seconds = 2,
                        int audioChannel = 0, int midiChannel = 0);

std::pair<double, double> frequencyAndRMSForNote(std::shared_ptr<SurgeSynthesizer> surge, int note,
                                                 int seconds = 2, int audioChannel = 0,
                                                 int midiChannel = 0);

double frequencyFromData(float *buffer, int nS, int nC, int audioChannel, int start, int trimTo,
                         float sampleRate);
double RMSFromData(float *buffer, int nS, int nC, int audioChannel, int start, int trimTo);

double frequencyForEvents(std::shared_ptr<SurgeSynthesizer> surge,
                          Surge::Headless::playerEvents_t &events, int audioChannel,
                          int startSample, int endSample);

void copyScenedataSubset(SurgeStorage *storage, int scene, int start, int end);
void setupStorageRanges(Parameter *start, Parameter *endIncluding, int &storage_id_start,
                        int &storage_id_end);

void makePlotPNGFromData(std::string pngFileName, std::string plotTitle, float *buffer, int nS,
                         int nC, int startSample = -1, int endSample = -1);

std::shared_ptr<SurgeSynthesizer> surgeOnPatch(const std::string &patchName);
std::shared_ptr<SurgeSynthesizer> surgeOnTemplate(const std::string &, float sr = 44100);
std::shared_ptr<SurgeSynthesizer> surgeOnSine(float sr = 44100);
std::shared_ptr<SurgeSynthesizer> surgeOnSaw(float sr = 44100);
} // namespace Test
} // namespace Surge
