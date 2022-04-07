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

struct SurgeSynthesizer;
struct SurgeGUIEditor;
struct StepSequencerStorage;

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
    void pushParameterChange(int paramId, pdata val, Target to = UNDO);
    void pushMacroChange(int macroid, float val);
    void pushModulationChange(int paramId, modsources modsource, int scene, int index, float val,
                              Target to = UNDO);
    void pushOscillator(int scene, int oscnum);
    void pushStepSequencer(int scene, int lfoid, const StepSequencerStorage &pushValue);
    void pushFX(int fxslot);
    bool undo();
    bool redo();
    void dumpStack();
};
} // namespace GUI
} // namespace Surge

#endif // SURGE_UNDOMANAGER_H
