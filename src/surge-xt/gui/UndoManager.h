/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2022 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_UNDOMANAGER_H
#define SURGE_UNDOMANAGER_H

#include "Parameter.h"
#include "ModulationSource.h"
#include "Tunings.h"

struct SurgeSynthesizer;
struct SurgeGUIEditor;
struct StepSequencerStorage;
struct MSEGStorage;
struct FormulaModulatorStorage;

namespace Surge
{
namespace GUI
{
struct UndoManagerImpl;
struct UndoManager
{
    UndoManager(SurgeGUIEditor *ed, SurgeSynthesizer *synth);
    ~UndoManager();

    void resetEditor(SurgeGUIEditor *ed);

    enum Target
    {
        UNDO,
        REDO
    };

    std::unique_ptr<UndoManagerImpl> impl;
    void pushParameterChange(int paramId, const Parameter *p, pdata val, Target to = UNDO);
    void pushMacroChange(int macroid, float val);
    void pushModulationChange(int paramId, const Parameter *p, modsources modsource, int scene,
                              int index, float val, bool muted, Target to = UNDO);
    void pushOscillator(int scene, int oscnum);
    void pushWavetable(int scene, int oscnum);
    void pushOscillatorExtraConfig(int scene, int oscnum);

    void pushStepSequencer(int scene, int lfoid, const StepSequencerStorage &pushValue);
    void pushMSEG(int scene, int lfoid, const MSEGStorage &pushValue);
    void pushFullLFO(int scene, int lfoid);
    void pushFormula(int scene, int lfoid, const FormulaModulatorStorage &pushValue);
    void pushFX(int fxslot);

    void pushMacroRename(int macro, const std::string &oldName);
    void pushLFORename(int scene, int lfoid, int index, const std::string &oldName);

    void pushTuning(const Tunings::Tuning &t);

    void pushPatch(); // args TK

    bool undo();
    bool redo();

    bool canUndo();
    bool canRedo();

    void dumpStack();

    std::vector<std::string> textStack(Target t, int maxDepth = 10);
};
} // namespace GUI
} // namespace Surge

#endif // SURGE_UNDOMANAGER_H
