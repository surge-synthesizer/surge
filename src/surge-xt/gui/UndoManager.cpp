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

#include "UndoManager.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthesizer.h"
#include <stack>
#include <chrono>
#include <variant>
#include <fmt/core.h>

namespace Surge
{
namespace GUI
{
struct UndoManagerImpl
{
    static constexpr int max_stack = 250;
    SurgeGUIEditor *editor;
    SurgeSynthesizer *synth;
    UndoManagerImpl(SurgeGUIEditor *ed, SurgeSynthesizer *s) : editor(ed), synth(s) {}

    // for now this is super simple
    struct UndoParam
    {
        int paramId;
        pdata val;
    };
    struct UndoModulation
    {
        int paramId;
        float val;
        int scene;
        int index;
        modsources ms;
    };
    struct UndoOscillator
    {
        int oscNum;
        int scene;
        int type;
        std::vector<std::pair<int, pdata>> paramIdValues;
    };
    struct UndoFX
    {
        int fxslot;
        int type;
        std::vector<std::pair<int, pdata>> paramIdValues;
    };
    struct UndoStep
    {
        int scene;
        int lfoid;
        StepSequencerStorage storageCopy;
    };

    // If you add a new type here add it both to aboutTheSameThing, toString, and
    // to undo.
    typedef std::variant<UndoParam, UndoModulation, UndoOscillator, UndoFX, UndoStep> UndoAction;
    struct UndoRecord
    {
        UndoAction action;
        std::chrono::time_point<std::chrono::high_resolution_clock> time;
        UndoRecord(const UndoAction &a) : action(a)
        {
            time = std::chrono::high_resolution_clock::now();
        }
    };
    std::deque<UndoRecord> undoStack, redoStack;

    /* Not same value, but same pair. Used for wheel event compressing for instance */
    bool aboutTheSameThing(const UndoAction &a, const UndoAction &b)
    {
        if (auto pa = std::get_if<UndoParam>(&a))
        {
            auto pb = std::get_if<UndoParam>(&b);
            return pa->paramId == pb->paramId;
        }
        if (auto pa = std::get_if<UndoModulation>(&a))
        {
            auto pb = std::get_if<UndoModulation>(&b);
            return (pa->paramId == pb->paramId) && (pa->scene == pb->scene) && (pa->ms == pb->ms) &&
                   (pa->index == pb->index);
        }
        if (auto pa = std::get_if<UndoStep>(&a))
        {
            auto pb = std::get_if<UndoStep>(&b);
            return (pa->lfoid == pb->lfoid) && (pa->scene == pb->scene);
        }
        return false;
    }

    std::string toString(const UndoAction &a)
    {
        if (auto pa = std::get_if<UndoParam>(&a))
        {
            return fmt::format("Param[id={},val.f={},val.i={}]", pa->paramId, pa->val.f, pa->val.i);
        }
        if (auto pa = std::get_if<UndoModulation>(&a))
        {
            return fmt::format("Modulation[id={},source={},scene={},idx={},val={}]", pa->paramId,
                               editor->modulatorName(pa->ms, false, pa->scene), pa->scene,
                               pa->index, pa->val);
        }
        if (auto pa = std::get_if<UndoOscillator>(&a))
        {
            return fmt::format("Oscillator[scene={},num={},type={}]", pa->scene, pa->oscNum,
                               pa->type);
        }
        if (auto pa = std::get_if<UndoFX>(&a))
        {
            return fmt::format("FX[slot={},type={}]", pa->fxslot, pa->type);
        }
        if (auto pa = std::get_if<UndoStep>(&a))
        {
            return fmt::format("Step[scene={},lfoid={}]", pa->scene, pa->lfoid);
        }
        return "UNK";
    }

    void pushUndo(const UndoAction &r)
    {
        if (undoStack.empty())
        {
            undoStack.emplace_back(r);
            return;
        }

        auto &t = undoStack.back();
        if (r.index() != t.action.index())
        {
            undoStack.emplace_back(r);
        }
        else
        {
            auto n = std::chrono::high_resolution_clock::now();
            auto d = std::chrono::duration_cast<std::chrono::milliseconds>(n - t.time);

            if (d.count() < 200 && aboutTheSameThing(r, t.action))
            {
                t.time = n;
            }
            else
            {
                undoStack.emplace_back(r);
            }
        }
        while (undoStack.size() > max_stack)
        {
            undoStack.pop_front();
        }
    }

    void pushRedo(const UndoAction &r) { redoStack.emplace_back(r); }

    void pushParameterChange(int paramId, pdata val, UndoManager::Target to)
    {
        auto r = UndoParam();
        r.paramId = paramId;
        r.val = val;
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }
    void pushModulationChange(int paramId, modsources modsource, int sc, int idx, float val,
                              UndoManager::Target to)
    {
        auto r = UndoModulation();
        r.paramId = paramId;
        r.val = val;
        r.ms = modsource;
        r.scene = sc;
        r.index = idx;

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushOscillator(int scene, int oscnum, UndoManager::Target to = UndoManager::UNDO)
    {
        auto os = &(synth->storage.getPatch().scene[scene].osc[oscnum]);
        auto r = UndoOscillator();
        r.scene = scene;
        r.oscNum = oscnum;
        r.type = os->type.val.i;

        Parameter *p = &(os->type);
        p++;
        while (p <= &(os->retrigger))
        {
            r.paramIdValues.emplace_back(p->id, p->val);
            p++;
        }

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushFX(int fxslot, UndoManager::Target to = UndoManager::UNDO)
    {
        auto fx = &(synth->storage.getPatch().fx[fxslot]);
        auto r = UndoFX();
        r.fxslot = fxslot;
        r.type = fx->type.val.i;

        for (int i = 0; i < n_fx_params; ++i)
        {
            r.paramIdValues.emplace_back(fx->p[i].id, fx->p[i].val);
        }

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushStepSequencer(int scene, int lfoid, const StepSequencerStorage &pushValue,
                           UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoStep();
        r.scene = scene;
        r.lfoid = lfoid;
        r.storageCopy = pushValue;
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    bool undoRedoImpl(UndoManager::Target which)
    {
        auto *currStack = &undoStack;
        if (which == UndoManager::REDO)
            currStack = &redoStack;

        if (currStack->empty())
            return false;

        auto qt = currStack->back();
        auto q = qt.action;
        currStack->pop_back();

        auto opposite = (which == UndoManager::UNDO ? UndoManager::REDO : UndoManager::UNDO);

        // this would be cleaner with std:visit but visit isn't in macos libc until 10.13
        if (auto p = std::get_if<UndoParam>(&q))
        {
            editor->pushParamToUndoRedo(p->paramId, opposite);
            editor->setParamFromUndo(p->paramId, p->val);
            return true;
        }
        if (auto p = std::get_if<UndoModulation>(&q))
        {
            editor->pushModulationToUndoRedo(p->paramId, p->ms, p->scene, p->index, opposite);
            editor->setModulationFromUndo(p->paramId, p->ms, p->scene, p->index, p->val);
            return true;
        }
        if (auto p = std::get_if<UndoOscillator>(&q))
        {
            pushOscillator(p->scene, p->oscNum, opposite);
            auto os = &(synth->storage.getPatch().scene[p->scene].osc[p->oscNum]);
            os->type.val.i = p->type;
            synth->storage.getPatch().update_controls(false, os, false);

            for (auto q : p->paramIdValues)
            {
                editor->setParamFromUndo(q.first, q.second);
            }
            return true;
        }
        if (auto p = std::get_if<UndoFX>(&q))
        {
            pushFX(p->fxslot, opposite);
            std::lock_guard<std::mutex> g(synth->fxSpawnMutex);

            int cge = p->fxslot;

            synth->fxsync[cge].type.val.i = p->type;
            Effect *t_fx = spawn_effect(synth->fxsync[cge].type.val.i, &synth->storage,
                                        &synth->fxsync[cge], 0);
            if (t_fx)
            {
                t_fx->init_ctrltypes();
                t_fx->init_default_values();
                delete t_fx;
            }

            synth->switch_toggled_queued = true;
            synth->load_fx_needed = true;
            synth->fx_reload[cge] = true;
            for (int i = 0; i < n_fx_params; ++i)
            {
                synth->fxsync[cge].p[i].val = p->paramIdValues[i].second;
            }
            return true;
        }
        if (auto p = std::get_if<UndoStep>(&q))
        {
            pushStepSequencer(p->scene, p->lfoid,
                              editor->getPatch().stepsequences[p->scene][p->lfoid], opposite);
            editor->setStepSequencerFromUndo(p->scene, p->lfoid, p->storageCopy);
            return true;
        }

        return false;
    }

    bool undo() { return undoRedoImpl(UndoManager::UNDO); }
    bool redo() { return undoRedoImpl(UndoManager::REDO); }

    void dumpStack()
    {
        std::cout << "-------- UNDO/REDO -----------\n";
        for (const auto &q : undoStack)
        {
            std::cout << "  UNDO : " << toString(q.action) << " "
                      << q.time.time_since_epoch().count() << " " << q.action.index() << std::endl;
        }
        std::cout << "\n";
        for (const auto &q : redoStack)
        {
            std::cout << "  REDO : " << toString(q.action) << " "
                      << q.time.time_since_epoch().count() << " " << q.action.index() << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }
};

UndoManager::UndoManager(SurgeGUIEditor *ed, SurgeSynthesizer *synth)
{
    impl = std::make_unique<UndoManagerImpl>(ed, synth);
}

UndoManager::~UndoManager() = default;

void UndoManager::pushParameterChange(int paramId, pdata val, Target to)
{
    impl->pushParameterChange(paramId, val, to);
}

void UndoManager::pushModulationChange(int paramId, modsources modsource, int scene, int idx,
                                       float val, Target to)
{
    impl->pushModulationChange(paramId, modsource, scene, idx, val, to);
}

void UndoManager::pushOscillator(int scene, int oscnum) { impl->pushOscillator(scene, oscnum); }

void UndoManager::pushFX(int fxslot) { impl->pushFX(fxslot); }

bool UndoManager::undo() { return impl->undo(); }

bool UndoManager::redo() { return impl->redo(); }

void UndoManager::dumpStack() { impl->dumpStack(); }

void UndoManager::resetEditor(SurgeGUIEditor *ed) { impl->editor = ed; }

void UndoManager::pushStepSequencer(int scene, int lfoid, const StepSequencerStorage &pushValue)
{
    impl->pushStepSequencer(scene, lfoid, pushValue);
}

} // namespace GUI

} // namespace Surge