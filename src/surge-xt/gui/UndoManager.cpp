/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "UndoManager.h"
#include "SurgeGUIEditor.h"
#include "SurgeSynthesizer.h"
#include <stack>
#include <chrono>
#include <variant>
#include <fmt/core.h>
#include "widgets/MainFrame.h" // so i can repaint without rebuild

namespace Surge
{
namespace GUI
{
struct UndoManagerImpl
{
    static constexpr int maxUndoStackMem = 1024 * 1024 * 25;
    static constexpr int maxRedoStackMem = 1024 * 1024 * 25;
    SurgeGUIEditor *editor;
    SurgeSynthesizer *synth;
    UndoManagerImpl(SurgeGUIEditor *ed, SurgeSynthesizer *s) : editor(ed), synth(s) {}
    ~UndoManagerImpl()
    {
        for (auto &r : undoStack)
        {
            freeAction(r.action);
        }
        for (auto &r : redoStack)
        {
            freeAction(r.action);
        }
    }
    bool doPush{true};
    struct SelfPushGuard
    {
        UndoManagerImpl *that;
        SelfPushGuard(UndoManagerImpl *imp) : that(imp) { that->doPush = false; }
        ~SelfPushGuard() { that->doPush = true; }
    };

    bool clearRedoOnUndo{true};
    struct DontClearRedoOnUndoGuard
    {
        UndoManagerImpl *that;
        DontClearRedoOnUndoGuard(UndoManagerImpl *imp) : that(imp)
        {
            that->clearRedoOnUndo = false;
        }
        ~DontClearRedoOnUndoGuard() { that->clearRedoOnUndo = true; }
    };
    struct CleanupGuard
    {
        UndoManagerImpl *that;
        CleanupGuard(UndoManagerImpl *imp) : that(imp) {}
        ~CleanupGuard() { that->doCleanup(); }
    };
    // for now this is super simple
    struct UndoParam
    {
        int paramId;
        std::string name;
        std::string formattedValue;
        bool temposync, absolute, deactivated, extend_range, deform_type;
        bool porta_constrate, porta_gliss, porta_retrigger;
        int porta_curve;
        pdata val;
    };
    struct UndoModulation
    {
        int paramId;
        float val;
        int scene;
        int index;
        bool muted;
        modsources ms;
        std::string source_name, target_name;
    };
    struct UndoOscillator
    {
        int oscNum;
        int scene;
        int type;
        std::vector<UndoParam> undoParamValues;
        std::vector<UndoModulation> undoModulations;
    };
    struct UndoWavetable
    {
        int oscNum;
        int scene;

        int current_id;
        // Why shared? Well this is only used in one edge case and it makes it easier
        std::shared_ptr<Wavetable> wt;

        std::string displayName;
    };
    struct UndoOscillatorExtraConfig
    {
        int oscNum;
        int scene;
        OscillatorStorage::ExtraConfigurationData extraConfig;
    };
    struct UndoFX
    {
        int fxslot;
        int type;
        std::vector<UndoParam> undoParamValues;
        std::vector<UndoModulation> undoModulations;
    };
    struct UndoStep
    {
        int scene;
        int lfoid;
        StepSequencerStorage storageCopy;
    };
    struct UndoMSEG
    {
        int scene;
        int lfoid;
        MSEGStorage storageCopy;
    };
    struct UndoFormula
    {
        int scene;
        int lfoid;
        FormulaModulatorStorage storageCopy;
    };
    struct UndoFullLFO
    {
        int scene;
        int lfoid;
        std::vector<UndoParam> undoParamValues;
        std::variant<bool, MSEGStorage, StepSequencerStorage, FormulaModulatorStorage> extraStorage;
    };
    struct UndoRename
    {
        bool isMacro;
        std::string name;
        int scene;
        int itemid;
        int index;
    };
    struct UndoMacro
    {
        int macro;
        float val;
    };
    struct UndoTuning
    {
        Tunings::Tuning tuning;
    };
    struct UndoPatch
    {
        void *data{nullptr};
        size_t dataSz{0};
        fs::path path{};
    };
    struct UndoFilterAnalysisMovement
    {
        UndoParam cutoff, resonance;
    };
    // If you add a new type here add it both to aboutTheSameThing, toString, and
    // to undo.
    typedef std::variant<UndoParam, UndoModulation, UndoOscillator, UndoOscillatorExtraConfig,
                         UndoWavetable, UndoFX, UndoStep, UndoMSEG, UndoFormula, UndoRename,
                         UndoMacro, UndoTuning, UndoPatch, UndoFullLFO, UndoFilterAnalysisMovement>
        UndoAction;
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
    size_t undoStackMem{0}, redoStackMem{0};

    size_t actionSize(const UndoAction &a)
    {
        auto res = sizeof(a);

        if (auto pt = std::get_if<UndoFormula>(&a))
        {
            res += pt->storageCopy.formulaString.size();
        }
        if (auto pt = std::get_if<UndoPatch>(&a))
        {
            res += pt->dataSz;
        }
        return res;
    }

    void freeAction(UndoAction &a)
    {
        if (auto pt = std::get_if<UndoPatch>(&a))
        {
            free(pt->data);
            pt->dataSz = 0;
        }
    }

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
        if (auto pa = std::get_if<UndoOscillatorExtraConfig>(&a))
        {
            auto pb = std::get_if<UndoOscillatorExtraConfig>(&b);
            return (pa->oscNum == pb->oscNum) && (pa->scene == pb->scene);
        }
        if (auto pa = std::get_if<UndoMSEG>(&a))
        {
            auto pb = std::get_if<UndoMSEG>(&b);
            return (pa->lfoid == pb->lfoid) && (pa->scene == pb->scene);
        }
        if (auto pa = std::get_if<UndoFormula>(&a))
        {
            auto pb = std::get_if<UndoFormula>(&b);
            return (pa->lfoid == pb->lfoid) && (pa->scene == pb->scene);
        }
        if (auto pa = std::get_if<UndoMacro>(&a))
        {
            auto pb = std::get_if<UndoMacro>(&b);
            return (pa->macro == pb->macro);
        }
        if (auto pa = std::get_if<UndoTuning>(&a))
        {
            // We've already checked B is a tuning. If we have simultaneous tuning actions
            // we know they are compressible.
            return true;
        }
        if (auto pa = std::get_if<UndoTuning>(&a))
        {
            // UndoPatch is always different
            return false;
        }
        if (auto pa = std::get_if<UndoFullLFO>(&a))
        {
            // No way to generate these other than discretely
            return false;
        }
        return false;
    }

    std::string toString(const UndoAction &a)
    {
        if (auto pa = std::get_if<UndoParam>(&a))
        {
            return fmt::format("Parameter {} : {} f={} i={}", pa->name, pa->formattedValue,
                               pa->val.f, pa->val.i);
        }
        if (auto pa = std::get_if<UndoModulation>(&a))
        {
            return fmt::format(
                "Modulation[id={},source={},scene={},idx={},val={},muted={}]", pa->paramId,
                ModulatorName::modulatorName(&synth->storage, pa->ms, false, pa->scene), pa->scene,
                pa->index, pa->val, pa->muted);
        }
        if (auto pa = std::get_if<UndoOscillator>(&a))
        {
            return fmt::format("Scene {} Oscillator {} Type : {}", (char)('A' + pa->scene),
                               pa->oscNum, osc_type_names[pa->type]);
        }
        if (auto pa = std::get_if<UndoOscillatorExtraConfig>(&a))
        {
            return fmt::format("OscillatorConfig[scene={},num={}]", pa->scene, pa->oscNum);
        }
        if (auto pa = std::get_if<UndoWavetable>(&a))
        {
            return fmt::format("OscillatorWavetable[scene={},num={}]", pa->scene, pa->oscNum);
        }
        if (auto pa = std::get_if<UndoFX>(&a))
        {
            return fmt::format("FX[slot={},type={}]", pa->fxslot, pa->type);
        }
        if (auto pa = std::get_if<UndoStep>(&a))
        {
            return fmt::format("Step[scene={},lfoid={}]", pa->scene, pa->lfoid);
        }
        if (auto pa = std::get_if<UndoMSEG>(&a))
        {
            return fmt::format("MSEG[scene={},lfoid={}]", pa->scene, pa->lfoid);
        }
        if (auto pa = std::get_if<UndoFormula>(&a))
        {
            return fmt::format("FORMULA[scene={},lfoid={}]", pa->scene, pa->lfoid);
        }
        if (auto pa = std::get_if<UndoMacro>(&a))
        {
            return fmt::format("Macro[id={},val={}]", pa->macro, pa->val);
        }
        if (auto pa = std::get_if<UndoRename>(&a))
        {
            return fmt::format("Rename{}[label='{}',itemid={},scene={},index={}]",
                               (pa->isMacro ? "Macro" : "Modulator"), pa->name, pa->itemid,
                               pa->scene, pa->index);
        }
        if (auto pa = std::get_if<UndoTuning>(&a))
        {
            return fmt::format("Tuning[]");
        }
        if (auto pa = std::get_if<UndoPatch>(&a))
        {
            return fmt::format("Patch[]");
        }
        if (auto pa = std::get_if<UndoFullLFO>(&a))
        {
            return fmt::format("FullLFO[]");
        }
        return "UNK";
    }

    void pushUndo(const UndoAction &r)
    {
        if (!doPush)
            return;

        auto g = CleanupGuard(this);
        if (undoStack.empty())
        {
            undoStack.emplace_back(r);
            undoStackMem += actionSize(r);
            if (clearRedoOnUndo)
            {
                clearRedo();
            }
            return;
        }

        auto &t = undoStack.back();
        if (r.index() != t.action.index())
        {
            undoStack.emplace_back(r);
            undoStackMem += actionSize(r);
            if (clearRedoOnUndo)
            {
                clearRedo();
            }
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
                undoStackMem += actionSize(r);
                if (clearRedoOnUndo)
                {
                    clearRedo();
                }
            }
        }
    }

    void clearRedo()
    {
        for (auto &r : redoStack)
        {
            freeAction(r.action);
        }
        redoStack.clear();
        redoStackMem = 0;
    }

    void pushRedo(const UndoAction &r)
    {
        if (!doPush)
            return;
        auto g = CleanupGuard(this);
        redoStack.emplace_back(r);
        redoStackMem += actionSize(r);
    }

    void doCleanup()
    {
        while (undoStackMem > maxUndoStackMem)
        {
            auto r = undoStack.front();
            freeAction(r.action);
            undoStackMem -= actionSize(r.action);
            undoStack.pop_front();
        }
        while (redoStackMem > maxRedoStackMem)
        {
            auto r = redoStack.front();
            freeAction(r.action);
            redoStackMem -= actionSize(r.action);
            redoStack.pop_front();
        }
    }

    void populateUndoParamFromP(const Parameter *p, pdata val, UndoParam &r)
    {
        std::string txt;
        char buf[TXT_SIZE];
        synth->getParameterName(synth->idForParameter(p), buf);
        txt = buf;
        r.name = txt;
        r.temposync = p->temposync;
        r.absolute = p->absolute;
        r.deactivated = p->deactivated;
        r.extend_range = p->extend_range;
        r.deform_type = p->deform_type;

        r.porta_constrate = p->porta_constrate;
        r.porta_gliss = p->porta_gliss;
        r.porta_retrigger = p->porta_retrigger;
        r.porta_curve = p->porta_curve;

        r.val = val;

        if (p->ctrltype == vt_float)
        {
            txt = p->get_display(true, val.f);
        }
        else if (p->ctrltype == vt_int)
        {
            txt = p->get_display(true,
                                 Parameter::intScaledToFloat(val.i, p->val_max.i, p->val_min.i));
        }
        else
        {
            txt = p->val.b ? "On" : "Off";
        }

        r.formattedValue = txt;
    }

    void restoreParamToEditor(const UndoParam *pa)
    {
        editor->setParamFromUndo(pa->paramId, pa->val);
        editor->applyToParamForUndo(pa->paramId, [pa](Parameter *p) {
            p->temposync = pa->temposync;
            p->absolute = pa->absolute;
            p->deactivated = pa->deactivated;
            p->set_extend_range(pa->extend_range);
            p->deform_type = pa->deform_type;

            p->porta_constrate = pa->porta_constrate;
            p->porta_gliss = pa->porta_gliss;
            p->porta_retrigger = pa->porta_retrigger;
            p->porta_curve = pa->porta_curve;
        });
    }

    void pushParameterChange(int paramId, const Parameter *p, pdata val, UndoManager::Target to)
    {
        auto r = UndoParam();
        r.paramId = paramId;
        populateUndoParamFromP(p, val, r);
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }
    void populateUndoModulation(int paramId, const Parameter *p, modsources modsource, int sc,
                                int idx, float val, bool muted, UndoModulation &r)
    {
        r.paramId = paramId;
        char txt[TXT_SIZE];
        synth->getParameterName(synth->idForParameter(p), txt);
        r.target_name = txt;
        r.source_name = modsource_names[modsource];
        r.val = val;
        r.ms = modsource;
        r.scene = sc;
        r.index = idx;
        r.muted = muted;
    }

    void pushModulationChange(int paramId, const Parameter *p, modsources modsource, int sc,
                              int idx, float val, bool muted, UndoManager::Target to)
    {
        auto r = UndoModulation();
        populateUndoModulation(paramId, p, modsource, sc, idx, val, muted, r);

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
            auto pu = UndoParam();
            pu.paramId = p->id;
            populateUndoParamFromP(p, p->val, pu);
            r.undoParamValues.emplace_back(pu);

            if (synth->isModDestUsed(p->id))
            {
                for (int ms = 1; ms < n_modsources; ms++)
                {
                    for (int sc = 0; sc < n_scenes; ++sc)
                    {
                        auto indices =
                            synth->getModulationIndicesBetween(p->id, (modsources)ms, sc);
                        for (auto idx : indices)
                        {
                            auto mr = UndoModulation();
                            float val = synth->getModDepth01(p->id, (modsources)ms, sc, idx);
                            bool muted = synth->isModulationMuted(p->id, (modsources)ms, sc, idx);
                            populateUndoModulation(p->id, p, (modsources)ms, sc, idx, val, muted,
                                                   mr);
                            r.undoModulations.push_back(mr);
                        }
                    }
                }
            }

            p++;
        }

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushWavetable(int scene, int oscnum, UndoManager::Target to = UndoManager::UNDO)
    {
        auto os = &(synth->storage.getPatch().scene[scene].osc[oscnum]);
        auto r = UndoWavetable();

        r.current_id = os->wt.current_id;

        r.wt = nullptr;
        if (r.current_id < 0)
        {
            r.wt = std::make_shared<Wavetable>();
            r.wt->Copy(&(os->wt));
        }

        r.scene = scene;
        r.oscNum = oscnum;
        r.displayName = os->wavetable_display_name;

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushOscillatorExtraConfig(int scene, int oscnum,
                                   UndoManager::Target to = UndoManager::UNDO)
    {
        auto os = &(synth->storage.getPatch().scene[scene].osc[oscnum]);
        auto r = UndoOscillatorExtraConfig();
        r.scene = scene;
        r.oscNum = oscnum;
        r.extraConfig = os->extraConfig;

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
            auto *p = &(fx->p[i]);
            auto pu = UndoParam();
            pu.paramId = p->id;
            populateUndoParamFromP(p, p->val, pu);
            r.undoParamValues.emplace_back(pu);
        }

        const auto &gr = synth->storage.getPatch().modulation_global;
        for (const auto &rt : gr)
        {
            if (rt.destination_id >= fx->p[0].id && rt.destination_id <= fx->p[n_fx_params - 1].id)
            {
                auto rm = UndoModulation();
                auto *p = synth->storage.getPatch().param_ptr[rt.destination_id];
                auto dt = rt.depth / (p->val_max.f - p->val_min.f); // want the 01

                populateUndoModulation(
                    rt.destination_id, synth->storage.getPatch().param_ptr[rt.destination_id],
                    (modsources)rt.source_id, rt.source_scene, rt.source_index, dt, rt.muted, rm);

                r.undoModulations.emplace_back(rm);
            }
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

    void pushMSEG(int scene, int lfoid, const MSEGStorage &pushValue,
                  UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoMSEG();
        r.scene = scene;
        r.lfoid = lfoid;
        r.storageCopy = pushValue;
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushFullLFO(int scene, int lfoid, UndoManager::Target to = UndoManager::UNDO)
    {
        UndoFullLFO r;
        r.scene = scene;
        r.lfoid = lfoid;
        auto lf = &(editor->getPatch().scene[scene].lfo[lfoid]);
        if (lf->shape.val.i == lt_mseg)
        {
            r.extraStorage = editor->getPatch().msegs[scene][lfoid];
        }
        else if (lf->shape.val.i == lt_formula)
        {
            r.extraStorage = editor->getPatch().formulamods[scene][lfoid];
        }
        else if (lf->shape.val.i == lt_stepseq)
        {
            r.extraStorage = editor->getPatch().stepsequences[scene][lfoid];
        }
        else
        {
            r.extraStorage = false;
        }

        Parameter *p = &(lf->rate);

        while (p <= &(lf->release))
        {
            auto pu = UndoParam();
            pu.paramId = p->id;
            populateUndoParamFromP(p, p->val, pu);
            r.undoParamValues.emplace_back(pu);
            p++;
        }

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushFormula(int scene, int lfoid, const FormulaModulatorStorage &pushValue,
                     UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoFormula();
        r.scene = scene;
        r.lfoid = lfoid;
        r.storageCopy = pushValue;
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushMacroOrLFORename(bool isMacro, const std::string &oldName, int scene, int itemid,
                              int index, UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoRename();
        r.isMacro = isMacro;
        r.name = oldName;
        r.scene = scene;
        r.itemid = itemid;
        r.index = index;

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushMacroChange(int m, float f, UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoMacro();
        r.macro = m;
        r.val = f;

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushTuning(const Tunings::Tuning &t, UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoTuning();
        r.tuning = t;
        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushPatch(UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoPatch();
        r.data = nullptr;
        r.dataSz = 0;
        r.path = fs::path{};
        static int qq = 0;
        bool doStream = editor->getPatch().isDirty;
        if (!doStream)
        {
            auto pt = editor->pathForCurrentPatch();
            if (pt.empty())
            {
                doStream = true;
            }
            else
            {
                r.path = pt;
            }
        }
        if (doStream)
        {
            void *data{nullptr};
            auto dsz = editor->getPatch().save_patch(&data);
            // Now the pointer which is returned will be the patches 'patchptr'
            // which on the lext load will get clobbered so we need to make a copy.
            r.dataSz = dsz;
            r.data = malloc(r.dataSz);
            memcpy(r.data, data, r.dataSz);
        }

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    void pushFilterAnalysisMovement(int cutoffParamId, const Parameter *cutoff_p,
                                    int resonanceParamId, const Parameter *resonance_p,
                                    UndoManager::Target to = UndoManager::UNDO)
    {
        auto r = UndoFilterAnalysisMovement();
        r.cutoff = UndoParam();
        r.cutoff.paramId = cutoffParamId;
        r.resonance = UndoParam();
        r.resonance.paramId = resonanceParamId;
        populateUndoParamFromP(cutoff_p, cutoff_p->val, r.cutoff);
        populateUndoParamFromP(resonance_p, resonance_p->val, r.resonance);

        if (to == UndoManager::UNDO)
            pushUndo(r);
        else
            pushRedo(r);
    }

    bool undoRedoImpl(UndoManager::Target which)
    {
        auto dcroug = DontClearRedoOnUndoGuard(this);
        auto *currStack = &undoStack;
        auto *currStackMem = &undoStackMem;

        if (which == UndoManager::REDO)
        {
            currStack = &redoStack;
            currStackMem = &redoStackMem;
        }

        if (currStack->empty())
            return false;

        auto qt = currStack->back();
        auto q = qt.action;
        *currStackMem -= actionSize(q);
        currStack->pop_back();

        auto opposite = (which == UndoManager::UNDO ? UndoManager::REDO : UndoManager::UNDO);
        std::string verb = (which == UndoManager::UNDO ? "Undo" : "Redo");
        // this would be cleaner with std:visit but visit isn't in macos libc until 10.13
        if (auto pa = std::get_if<UndoParam>(&q))
        {
            editor->pushParamToUndoRedo(pa->paramId, opposite);
            auto g = SelfPushGuard(this);
            restoreParamToEditor(pa);
            auto ann = fmt::format("{} Parameter Value, {}", verb, pa->name);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoModulation>(&q))
        {
            editor->pushModulationToUndoRedo(p->paramId, p->ms, p->scene, p->index, opposite);
            auto g = SelfPushGuard(this);
            editor->setModulationFromUndo(p->paramId, p->ms, p->scene, p->index, p->val, p->muted);

            auto ann =
                fmt::format("{} Modulation to {} from {}", verb, p->target_name, p->source_name);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoOscillator>(&q))
        {
            pushOscillator(p->scene, p->oscNum, opposite);
            auto g = SelfPushGuard(this);
            auto os = &(synth->storage.getPatch().scene[p->scene].osc[p->oscNum]);
            os->type.val.i = p->type;
            synth->storage.getPatch().update_controls(false, os, false);

            for (const auto &qp : p->undoParamValues)
            {
                restoreParamToEditor(&qp);
            }

            for (const auto &qp : p->undoModulations)
            {
                editor->setModulationFromUndo(qp.paramId, qp.ms, qp.scene, qp.index, qp.val,
                                              qp.muted);
            }
            auto ann = fmt::format("{} Oscillator Type to {}, Scene {} Oscillator {}", verb,
                                   osc_type_names[p->type], (char)('A' + p->scene), p->oscNum + 1);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoOscillatorExtraConfig>(&q))
        {
            pushOscillatorExtraConfig(p->scene, p->oscNum, opposite);
            auto g = SelfPushGuard(this);
            auto os = &(synth->storage.getPatch().scene[p->scene].osc[p->oscNum]);

            os->extraConfig = p->extraConfig;
            editor->frame->repaint();

            auto ann = fmt::format("{} Oscillator Configuration in Scene {} Oscillator {}", verb,
                                   (char)('A' + p->scene), p->oscNum + 1);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoWavetable>(&q))
        {
            pushWavetable(p->scene, p->oscNum, opposite);
            auto g = SelfPushGuard(this);
            auto os = &(synth->storage.getPatch().scene[p->scene].osc[p->oscNum]);

            if (!p->displayName.empty())
            {
                os->wavetable_display_name = p->displayName;
            }
            if (p->current_id >= 0)
            {
                os->wt.queue_id = p->current_id;
            }
            else if (p->wt)
            {
                os->wt.Copy(p->wt.get());
                synth->refresh_editor = true;
            }
            else
            {
                jassertfalse;
            }

            auto ann = fmt::format("{} Oscillator Wavetable in Scene {} Oscillator {}", verb,
                                   (char)('A' + p->scene), p->oscNum + 1);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoFullLFO>(&q))
        {
            pushFullLFO(p->scene, p->lfoid, opposite);
            auto g = SelfPushGuard(this);
            auto lf = &(synth->storage.getPatch().scene[p->scene].lfo[p->lfoid]);

            for (const auto &qp : p->undoParamValues)
            {
                restoreParamToEditor(&qp);
            }
            if (lf->shape.val.i == lt_mseg)
            {
                auto ms = std::get_if<MSEGStorage>(&p->extraStorage);
                if (ms)
                {
                    editor->setMSEGFromUndo(p->scene, p->lfoid, *ms);
                }
            }
            else if (lf->shape.val.i == lt_formula)
            {
                auto ms = std::get_if<FormulaModulatorStorage>(&p->extraStorage);
                if (ms)
                {
                    editor->setFormulaFromUndo(p->scene, p->lfoid, *ms);
                }
            }
            else if (lf->shape.val.i == lt_stepseq)
            {
                auto ms = std::get_if<StepSequencerStorage>(&p->extraStorage);
                if (ms)
                {
                    editor->setStepSequencerFromUndo(p->scene, p->lfoid, *ms);
                }
            }

            auto ann = fmt::format("{} Full Modulator, Scene {} Modulator {}", verb,
                                   (char)('A' + p->scene), p->lfoid + 1);
            editor->enqueueAccessibleAnnouncement(ann);

            return true;
        }
        if (auto p = std::get_if<UndoFX>(&q))
        {
            pushFX(p->fxslot, opposite);
            auto spg = SelfPushGuard(this);
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
                auto *pa = &(synth->fxsync[cge].p[i]);
                pa->val = p->undoParamValues[i].val;
                pa->temposync = p->undoParamValues[i].temposync;
                pa->absolute = p->undoParamValues[i].absolute;
                pa->deactivated = p->undoParamValues[i].deactivated;
                pa->set_extend_range(p->undoParamValues[i].extend_range);
                pa->deform_type = p->undoParamValues[i].deform_type;
            }

            if (!p->undoModulations.empty())
            {
                synth->fx_reload_mod[cge] = true;
                synth->fxmodsync[cge].clear();
                for (const auto &mod : p->undoModulations)
                {
                    synth->fxmodsync[cge].push_back({mod.ms, mod.scene, mod.index,
                                                     mod.paramId - synth->fxsync[cge].p[0].id,
                                                     mod.val, mod.muted});
                }
            }

            auto ann = fmt::format("{} FX Type to {}, FX Slot {}", verb, fx_type_names[p->type],
                                   fxslot_names[p->fxslot]);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoStep>(&q))
        {
            pushStepSequencer(p->scene, p->lfoid,
                              editor->getPatch().stepsequences[p->scene][p->lfoid], opposite);
            auto g = SelfPushGuard(this);
            editor->setStepSequencerFromUndo(p->scene, p->lfoid, p->storageCopy);
            auto ann = fmt::format("{} Step Sequencer Setting, Scene {} Modulator {}", verb,
                                   (char)('A' + p->scene), p->lfoid + 1);
            editor->enqueueAccessibleAnnouncement(ann);

            return true;
        }
        if (auto p = std::get_if<UndoMSEG>(&q))
        {
            pushMSEG(p->scene, p->lfoid, editor->getPatch().msegs[p->scene][p->lfoid], opposite);
            auto g = SelfPushGuard(this);
            editor->setMSEGFromUndo(p->scene, p->lfoid, p->storageCopy);
            auto ann = fmt::format("{} MSEG, Scene {} Modulator {}", verb, (char)('A' + p->scene),
                                   p->lfoid + 1);
            editor->enqueueAccessibleAnnouncement(ann);

            return true;
        }
        if (auto p = std::get_if<UndoFormula>(&q))
        {
            pushFormula(p->scene, p->lfoid, editor->getPatch().formulamods[p->scene][p->lfoid],
                        opposite);
            auto g = SelfPushGuard(this);
            editor->setFormulaFromUndo(p->scene, p->lfoid, p->storageCopy);
            auto ann = fmt::format("{} Formula, Scene {} Modulator {}", verb,
                                   (char)('A' + p->scene), p->lfoid + 1);
            editor->enqueueAccessibleAnnouncement(ann);

            return true;
        }
        if (auto p = std::get_if<UndoRename>(&q))
        {
            if (p->isMacro)
            {
                auto nm = editor->getPatch().CustomControllerLabel[p->itemid];
                pushMacroOrLFORename(true, nm, -1, p->itemid, -1, opposite);
                auto g = SelfPushGuard(this);

                editor->setMacroNameFromUndo(p->itemid, p->name);
                auto ann = fmt::format("{} Macro {} Rename", verb, p->itemid);
                editor->enqueueAccessibleAnnouncement(ann);
            }
            else
            {
                auto nm = editor->getPatch().LFOBankLabel[p->scene][p->itemid][p->index];
                pushMacroOrLFORename(false, nm, p->scene, p->itemid, p->index, opposite);
                auto g = SelfPushGuard(this);
                editor->setLFONameFromUndo(p->scene, p->itemid, p->index, p->name);

                auto ann = fmt::format("{} Modulator Rename", verb);
                editor->enqueueAccessibleAnnouncement(ann);
            }
            return true;
        }
        if (auto p = std::get_if<UndoMacro>(&q))
        {
            auto cms = ((ControllerModulationSource *)editor->getPatch()
                            .scene[editor->current_scene]
                            .modsources[p->macro + ms_ctrl1]);
            pushMacroChange(p->macro, cms->get_target01(0), opposite);
            auto g = SelfPushGuard(this);
            editor->setMacroValueFromUndo(p->macro, p->val);

            auto ann = fmt::format("{} Macro {} Value", verb, p->macro);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoTuning>(&q))
        {
            pushTuning(editor->getTuningForRedo(), opposite);
            auto g = SelfPushGuard(this);
            editor->setTuningFromUndo(p->tuning);

            auto ann = fmt::format("{} Tuning Change", verb);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoPatch>(&q))
        {
            pushPatch(opposite);
            auto g = SelfPushGuard(this);
            if (p->dataSz == 0)
            {
                editor->queuePatchFileLoad(p->path.u8string());
            }
            else
            {
                editor->setPatchFromUndo(p->data, p->dataSz);
            }

            auto ann = fmt::format("{} Patch Change", verb);
            editor->enqueueAccessibleAnnouncement(ann);
            return true;
        }
        if (auto p = std::get_if<UndoFilterAnalysisMovement>(&q))
        {
            pushFilterAnalysisMovement(
                p->cutoff.paramId, synth->storage.getPatch().param_ptr[p->cutoff.paramId],
                p->resonance.paramId, synth->storage.getPatch().param_ptr[p->resonance.paramId],
                opposite);
            auto g = SelfPushGuard(this);
            restoreParamToEditor(&p->cutoff);
            restoreParamToEditor(&p->resonance);
            auto ann = fmt::format("{} Parameter Value, {}", verb, "Filter Analysis Movement");
            editor->enqueueAccessibleAnnouncement(ann);
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
            std::cout << "  UNDO : " << toString(q.action) << " " << actionSize(q.action) << " "
                      << q.time.time_since_epoch().count() << " " << q.action.index() << std::endl;
        }
        std::cout << "\n";
        for (const auto &q : redoStack)
        {
            std::cout << "  REDO : " << toString(q.action) << " " << actionSize(q.action) << " "
                      << q.time.time_since_epoch().count() << " " << q.action.index() << std::endl;
        }
        std::cout << "-------------------------------" << std::endl;
    }

    std::vector<std::string> textStack(UndoManager::Target t, int maxDepth = 10)
    {
        auto *currStack = &undoStack;
        auto *currStackMem = &undoStackMem;

        if (t == UndoManager::REDO)
        {
            currStack = &redoStack;
            currStackMem = &redoStackMem;
        }

        int ct = 0;
        std::vector<std::string> res;

        for (const auto &q : *currStack)
        {
            res.push_back(toString(q.action));
            if (ct++ >= maxDepth)
                break;
        }

        std::reverse(res.begin(), res.end());
        return res;
    }
};

UndoManager::UndoManager(SurgeGUIEditor *ed, SurgeSynthesizer *synth)
{
    impl = std::make_unique<UndoManagerImpl>(ed, synth);
}

UndoManager::~UndoManager() = default;

void UndoManager::pushParameterChange(int paramId, const Parameter *p, pdata val, Target to)
{
    impl->pushParameterChange(paramId, p, val, to);
}

void UndoManager::pushModulationChange(int paramId, const Parameter *p, modsources modsource,
                                       int scene, int idx, float val, bool muted, Target to)
{
    impl->pushModulationChange(paramId, p, modsource, scene, idx, val, muted, to);
}

void UndoManager::pushOscillator(int scene, int oscnum) { impl->pushOscillator(scene, oscnum); }

void UndoManager::pushFX(int fxslot) { impl->pushFX(fxslot); }

bool UndoManager::undo() { return impl->undo(); }

bool UndoManager::redo() { return impl->redo(); }

void UndoManager::dumpStack() { impl->dumpStack(); }

std::vector<std::string> UndoManager::textStack(Target t, int maxDepth)
{
    return impl->textStack(t, maxDepth);
}

void UndoManager::resetEditor(SurgeGUIEditor *ed) { impl->editor = ed; }

void UndoManager::pushStepSequencer(int scene, int lfoid, const StepSequencerStorage &pushValue)
{
    impl->pushStepSequencer(scene, lfoid, pushValue);
}

void UndoManager::pushMSEG(int scene, int lfoid, const MSEGStorage &pushValue)
{
    impl->pushMSEG(scene, lfoid, pushValue);
}

void UndoManager::pushFormula(int scene, int lfoid, const FormulaModulatorStorage &pushValue)
{
    impl->pushFormula(scene, lfoid, pushValue);
}

void UndoManager::pushMacroRename(int macro, const std::string &oldName)
{
    impl->pushMacroOrLFORename(true, oldName, -1, macro, -1);
}

void UndoManager::pushLFORename(int scene, int lfoid, int index, const std::string &oldName)
{
    impl->pushMacroOrLFORename(false, oldName, scene, lfoid, index);
}

void UndoManager::pushTuning(const Tunings::Tuning &t) { impl->pushTuning(t); }

void UndoManager::pushMacroChange(int macroid, float val) { impl->pushMacroChange(macroid, val); }

void UndoManager::pushPatch() { impl->pushPatch(); }

void UndoManager::pushFullLFO(int scene, int lfoid) { impl->pushFullLFO(scene, lfoid); }

void UndoManager::pushWavetable(int scene, int oscnum) { impl->pushWavetable(scene, oscnum); };
void UndoManager::pushOscillatorExtraConfig(int scene, int oscnum)
{
    impl->pushOscillatorExtraConfig(scene, oscnum);
}

bool UndoManager::canUndo() { return !impl->undoStack.empty(); }

bool UndoManager::canRedo() { return !impl->redoStack.empty(); }
void UndoManager::pushFilterAnalysisMovement(int cutoffParamId, const Parameter *cutoff_p,
                                             int resonanceParamId, const Parameter *resonance_p,
                                             UndoManager::Target to)
{
    impl->pushFilterAnalysisMovement(cutoffParamId, cutoff_p, resonanceParamId, resonance_p, to);
}
} // namespace GUI

} // namespace Surge