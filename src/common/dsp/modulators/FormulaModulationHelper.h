/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_FORMULAMODULATIONHELPER_H
#define SURGE_XT_FORMULAMODULATIONHELPER_H

#include "SurgeStorage.h"
#include "StringOps.h"
#include "LuaSupport.h"
#include <variant>

class SurgeVoice;

namespace Surge
{
namespace Formula
{

struct GlobalData
{
    std::unordered_set<std::string> knownBadFunctions; // these are functions which cause an error
    std::unordered_map<FormulaModulatorStorage *, std::unordered_set<std::string>> functionsPerFMS;
    void *audioState{nullptr}, *displayState{nullptr};
};

static constexpr int max_formula_outputs{max_lfo_indices};

struct EvaluatorState
{
    bool released;
    char funcName[TXT_SIZE];
    char funcNameInit[TXT_SIZE];
    char stateName[TXT_SIZE];

    bool isvalid = false;
    bool useEnvelope = true;
    bool isFinite = true;

    bool subVoice{false}, subLfoParams{true}, subLfoEnvelope{false}, subTiming{true};
    bool subMacros[n_customcontrollers], subAnyMacro{false};

    float del, a, h, dec, s, r;
    float rate, amp, phase, deform;
    float tempo, songpos;

    bool retrigger_AEG, retrigger_FEG;

    // voice features
    bool isVoice;
    int key{60}, channel{0}, velocity{0};

    // patch features
    float macrovalues[n_customcontrollers];

    std::string error;
    bool raisedError = false;
    void adderror(const std::string &msg)
    {
        error += msg;
        raisedError = true;
    }

    int activeoutputs;

    lua_State *L; // This is assigned by prepareForEvaluation to be one per thread
};

void setupStorage(SurgeStorage *s);

bool initEvaluatorState(EvaluatorState &s);
bool cleanEvaluatorState(EvaluatorState &s);
void removeFunctionsAssociatedWith(SurgeStorage *,
                                   FormulaModulatorStorage *fs); // audio thread only please
bool prepareForEvaluation(SurgeStorage *storage, FormulaModulatorStorage *fs, EvaluatorState &s,
                          bool is_display);

void setupEvaluatorStateFrom(EvaluatorState &s, const SurgePatch &p);
void setupEvaluatorStateFrom(EvaluatorState &s, const SurgeVoice *v);

void valueAt(int phaseIntPart, float phaseFracPart, SurgeStorage *, FormulaModulatorStorage *fs,
             EvaluatorState *state, float output[max_formula_outputs]);

struct DebugRow
{
    explicit DebugRow(int r, const std::string &s, const std::string &v)
        : depth(r), label(s), value(v)
    {
    }

    explicit DebugRow(int r, const std::string &s, const float f) : depth(r), label(s), value(f) {}

    explicit DebugRow(int r, const std::string &s) : depth(r), label(s), hasValue(false) {}

    int depth;
    std::string label;
    bool hasValue{true};
    bool isInternal{false};
    std::variant<float, std::string> value;
};
std::vector<DebugRow> createDebugDataOfModState(const EvaluatorState &s);
std::string createDebugViewOfModState(const EvaluatorState &s);

/*
 * Our test harness wants to send bits of lua to the modstate to get results out for
 * regtests. Send a function query(modstate) which returns something leaf like
 */
std::variant<float, std::string, bool> runOverModStateForTesting(const std::string &query,
                                                                 const EvaluatorState &s);

std::variant<float, std::string, bool> extractModStateKeyForTesting(const std::string &key,
                                                                    const EvaluatorState &s);

void createInitFormula(FormulaModulatorStorage *fs);

} // namespace Formula
} // namespace Surge
#endif // SURGE_XT_FORMULAMODULATIONHELPER_H
