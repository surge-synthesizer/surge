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

#if HAS_FORMULA_MODULATOR
extern "C"
{
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "luajit.h"

#include "lj_arch.h"
}
#endif

namespace Surge
{
namespace Formula
{
#if !HAS_FORMULA_MODULATOR
struct EvaluatorState
{
};
bool prepareForEvaluation(FormulaModulatorStorage *fs, EvaluatorState &s, bool is_display)
{
    return false;
}

float valueAt(int phaseIntPart, float phaseFracPart, float deform, FormulaModulatorStorage *fs,
              EvaluatorState *state)
{
    return 0;
}

#else
struct EvaluatorState
{
    bool released;
    char funcName[TXT_SIZE];
    char stateName[TXT_SIZE];

    bool isvalid = false;
    bool useEnvelope = true;

    float del, a, h, dec, s, r;
    float rate, amp, phase, deform;
    float tempo, songpos;

    bool retrigger_AEG, retrigger_FEG;

    std::string error;
    bool raisedError = false;
    void adderror(const std::string &s)
    {
        error += s;
        raisedError = true;
    }

    lua_State *L; // This is assigned by prepareForEvaluation to be one per thread
};

bool initEvaluatorState(EvaluatorState &s);
bool cleanEvaluatorState(EvaluatorState &s);
bool prepareForEvaluation(FormulaModulatorStorage *fs, EvaluatorState &s, bool is_display);

float valueAt(int phaseIntPart, float phaseFracPart, FormulaModulatorStorage *fs,
              EvaluatorState *state);

void createInitFormula(FormulaModulatorStorage *fs);

#endif
} // namespace Formula
} // namespace Surge
#endif // SURGE_XT_FORMULAMODULATIONHELPER_H
