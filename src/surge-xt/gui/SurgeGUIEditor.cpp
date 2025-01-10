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

#if SURGE_INCLUDE_MELATONIN_INSPECTOR
#include "melatonin_inspector/melatonin_inspector.h"
#endif

#include "SurgeGUIEditor.h"
#include "resource.h"

#include "SurgeImageStore.h"
#include "SurgeImage.h"

#include "UserDefaults.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "SurgeGUIUtils.h"
#include "DebugHelpers.h"
#include "StringOps.h"
#include "ModulatorPresetManager.h"
#include "ModulationSource.h"

#include "SurgeSynthEditor.h"
#include "SurgeJUCELookAndFeel.h"

#include "SurgeGUIEditorTags.h"
#include "fmt/core.h"

#include "overlays/AboutScreen.h"
#include "overlays/MiniEdit.h"
#include "overlays/Alert.h"
#include "overlays/MSEGEditor.h"
#include "overlays/LuaEditors.h"
#include "overlays/ModulationEditor.h"
#include "overlays/TypeinParamEditor.h"
#include "overlays/Oscilloscope.h"
#include "overlays/OverlayWrapper.h"
#include "overlays/FilterAnalysis.h"

#include "widgets/EffectLabel.h"
#include "widgets/EffectChooser.h"
#include "widgets/LFOAndStepDisplay.h"
#include "widgets/MainFrame.h"
#include "widgets/MenuForDiscreteParams.h"
#include "widgets/MenuCustomComponents.h"
#include "widgets/ModulationSourceButton.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"
#include "widgets/NumberField.h"
#include "widgets/OscillatorWaveformDisplay.h"
#include "widgets/ParameterInfowindow.h"
#include "widgets/PatchSelector.h"
#include "widgets/Switch.h"
#include "widgets/VerticalLabel.h"
#include "widgets/VuMeter.h"
#include "widgets/WaveShaperSelector.h"
#include "widgets/XMLConfiguredMenus.h"

#include "ModulationGridConfiguration.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stack>
#include <numeric>
#include <unordered_map>
#include <codecvt>
#include "version.h"
#include "ModernOscillator.h"
#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#include "libMTSMaster.h"
#endif

#include "filesystem/import.h"
#include "RuntimeFont.h"

#include "juce_core/juce_core.h"
#include "AccessibleHelpers.h"

const int yofs = 10;

using namespace std;
using namespace Surge::ParamConfig;

struct DroppedUserDataEntries
{
    std::vector<int> fxPresets;
    std::vector<int> midiMappings;
    std::vector<int> modulatorSettings;
    std::vector<int> patches;
    std::vector<std::vector<int>> skins;
    std::vector<int> wavetables;

    void clear()
    {
        fxPresets.clear();
        midiMappings.clear();
        modulatorSettings.clear();
        patches.clear();
        skins.clear();
        wavetables.clear();
    }

    int totalSize() const
    {
        return fxPresets.size() + midiMappings.size() + modulatorSettings.size() + patches.size() +
               skins.size() + wavetables.size();
    }
};

class DroppedUserDataHandler
{
    std::unique_ptr<juce::ZipFile> zipFile;
    DroppedUserDataEntries entries;

    void initEntries()
    {
        if (zipFile == nullptr)
        {
            return;
        }

        entries.clear();
        zipFile->sortEntriesByFilename();
        int numEntries = zipFile->getNumEntries();
        if (numEntries <= 0)
        {
            return;
        }

        int iEntry = 0;
        while (iEntry < numEntries)
        {
            auto entry = zipFile->getEntry(iEntry);
            if (!entry || entry->isSymbolicLink)
            {
                iEntry += 1;
                continue;
            }

            if (entry->filename.endsWithIgnoreCase(".srgfx"))
            {
                entries.fxPresets.push_back(iEntry);
            }
            else if (entry->filename.endsWithIgnoreCase(".srgmid"))
            {
                entries.midiMappings.push_back(iEntry);
            }
            else if (entry->filename.endsWithIgnoreCase(".modpreset"))
            {
                entries.modulatorSettings.push_back(iEntry);
            }
            else if (entry->filename.endsWithIgnoreCase(".fxp"))
            {
                entries.patches.push_back(iEntry);
            }
            else if (entry->filename.endsWithIgnoreCase(".wt") ||
                     entry->filename.endsWithIgnoreCase(".wav"))
            {
                entries.wavetables.push_back(iEntry);
            }
            else if (entry->filename.containsIgnoreCase(".surge-skin"))
            {
                /*
                ** The topmost skin directory is not returned
                ** by juce::ZipFile (at least on windows).
                ** For example for a zip structure like
                **
                ** |- default.surge-skin/
                **    |- SVG/
                **       |- svgs
                **       |- ...
                **    |- skin.xml
                **
                ** the first ZipEntry returned is for 'default.surge-skin/SVG/'.
                ** To find all files which belong to one skin, the starting directory name
                ** is first searched for.
                */
                int endOfSkinDirectory = entry->filename.indexOfIgnoreCase(".surge-skin") +
                                         std::string(".surge-skin/").length();
                auto skinDirectoryName = entry->filename.substring(0, endOfSkinDirectory);
                std::vector<int> skinEntries;
                skinEntries.push_back(iEntry);
                int iSkinEntry = iEntry + 1;
                while (iSkinEntry < numEntries)
                {
                    entry = zipFile->getEntry(iSkinEntry);
                    if (!entry->filename.startsWithIgnoreCase(skinDirectoryName))
                    {
                        break;
                    }
                    skinEntries.push_back(iSkinEntry);
                    iSkinEntry += 1;
                }
                entries.skins.push_back(skinEntries);
                iEntry = iSkinEntry - 1;
            }
            iEntry += 1;
        }
    }

    bool uncompressEntry(int iEntry, fs::path uncompressTo)
    {
        auto res = zipFile->uncompressEntry(iEntry, juce::File(path_to_string(uncompressTo)));
        if (res.failed())
        {
            std::cout << "patches unzip failed for entry " << iEntry << " to " << uncompressTo
                      << std::endl;
            return false;
        }

        // mac zip can create a place for the finder bundle that juce
        // unzips as opposed to applies. Nuke it. See #7249
        if (fs::is_directory(uncompressTo / "__MACOSX"))
        {
            try
            {
                fs::remove_all(uncompressTo / "__MACOSX");
            }
            catch (const fs::filesystem_error &)
            {
            }
        }
        return true;
    }

  public:
    bool init(const std::string &fname)
    {
        juce::File file(fname);
        zipFile = std::make_unique<juce::ZipFile>(file);
        initEntries();
        return true;
    }

    DroppedUserDataEntries getEntries() const { return entries; }

    bool extractEntries(SurgeStorage *storage)
    {
        if (zipFile == nullptr)
        {
            return false;
        }

        for (int iEntry : entries.fxPresets)
        {
            if (!uncompressEntry(iEntry, storage->userFXPath))
            {
                return false;
            }
        }

        for (int iEntry : entries.midiMappings)
        {
            if (!uncompressEntry(iEntry, storage->userMidiMappingsPath))
            {
                return false;
            }
        }

        for (int iEntry : entries.modulatorSettings)
        {
            if (!uncompressEntry(iEntry, storage->userModulatorSettingsPath))
            {
                return false;
            }
        }

        for (int iEntry : entries.patches)
        {
            if (!uncompressEntry(iEntry, storage->userPatchesPath))
            {
                return false;
            }
        }

        for (auto skin : entries.skins)
        {
            for (int iEntry : skin)
            {
                if (!uncompressEntry(iEntry, storage->userSkinsPath))
                {
                    return false;
                }
            }
        }

        for (int iEntry : entries.wavetables)
        {
            if (!uncompressEntry(iEntry, storage->userWavetablesPath))
            {
                return false;
            }
        }

        return true;
    }
};

Surge::GUI::ModulationGrid *Surge::GUI::ModulationGrid::grid = nullptr;

SurgeGUIEditor::SurgeGUIEditor(SurgeSynthEditor *jEd, SurgeSynthesizer *synth)
{
    jassert(Surge::GUI::ModulationGrid::getModulationGrid());

    assert(n_paramslots >= n_total_params);
    synth->storage.addErrorListener(this);
    synth->storage.okCancelProvider = [this](const std::string &msg, const std::string &title,
                                             SurgeStorage::OkCancel def,
                                             std::function<void(SurgeStorage::OkCancel)> callback) {
        // TODO: think about threading one day probably
        alertYesNo(
            title, msg, [callback]() { callback(SurgeStorage::OkCancel::OK); },
            [callback]() { callback(SurgeStorage::OkCancel::CANCEL); });
    };
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeGUIEditor::SurgeGUIEditor");
#endif
    blinktimer = 0.f;
    blinkstate = false;
    midiLearnOverlay = nullptr;
    patchCountdown = -1;

    mod_editor = false;
    editor_open = false;
    editor_open = false;
    queue_refresh = false;

    memset(param, 0, n_paramslots * sizeof(void *));

    for (int i = 0; i < n_fx_slots; ++i)
    {
        selectedFX[i] = -1;
        fxPresetName[i] = "";
    }

    for (auto i = 0; i < n_scenes; ++i)
    {
        for (auto j = 0; j < n_lfos; ++j)
        {
            modsource_index_cache[i][j] = 0;
        }
    }

    juceEditor = jEd;
    this->synth = synth;

    Surge::GUI::setIsStandalone(juceEditor->processor.wrapperType ==
                                juce::AudioProcessor::wrapperType_Standalone);

    currentSkin = Surge::GUI::SkinDB::get()->defaultSkin(&(this->synth->storage));

    // init the size of the plugin
    initialZoomFactor =
        Surge::Storage::getUserDefaultValue(&(synth->storage), Surge::Storage::DefaultZoom, 0);

    if (initialZoomFactor == 0)
    {
        float baseW = getWindowSizeX();
        float baseH = getWindowSizeY();

        int maxScreenUsage = 70;

        auto correctedZf =
            findLargestFittingZoomBetween(100.0, 250.0, 25, maxScreenUsage, baseW, baseH);

        /*
         * If there's nothing, probably a fresh install but may be no default. So be careful if
         * we have constrained zooms.
         */
        if (currentSkin->hasFixedZooms())
        {
            int zz = 100;

            for (auto z : currentSkin->getFixedZooms())
            {
                if (z <= correctedZf)
                {
                    zz = z;
                }
            }

            correctedZf = zz;
        }

        initialZoomFactor = correctedZf;
    }

    int instanceZoomFactor = synth->storage.getPatch().dawExtraState.editor.instanceZoomFactor;

    if (instanceZoomFactor > 0)
    {
        // dawExtraState zoomFactor wins defaultZoom
        initialZoomFactor = instanceZoomFactor;
    }

    zoom_callback = [](SurgeGUIEditor *f, bool) {};
    auto rg = SurgeSynthEditor::BlockRezoom(juceEditor);
    setZoomFactor(initialZoomFactor);
    zoomInvalid = (initialZoomFactor != 100);

    reloadFromSkin();

    auto des = &(synth->storage.getPatch().dawExtraState);

    if (des->isPopulated)
    {
        loadFromDawExtraState(synth);
    }

    paramInfowindow = std::make_unique<Surge::Widgets::ParameterInfowindow>();
    paramInfowindow->setVisible(false);

    patchSelectorComment = std::make_unique<Surge::Widgets::PatchSelectorCommentTooltip>();
    patchSelectorComment->setVisible(false);

    typeinParamEditor = std::make_unique<Surge::Overlays::TypeinParamEditor>();
    typeinParamEditor->setVisible(false);
    typeinParamEditor->setSurgeGUIEditor(this);

    miniEdit = std::make_unique<Surge::Overlays::MiniEdit>();
    miniEdit->setVisible(false);

    synth->addModulationAPIListener(this);

    juce::Desktop::getInstance().addFocusChangeListener(this);

    setupKeymapManager();

    if (!juceEditor->processor.undoManager)
    {
        juceEditor->processor.undoManager =
            std::make_unique<Surge::GUI::UndoManager>(this, this->synth);
    }

    juceEditor->processor.undoManager->resetEditor(this);

    synth->storage.uiThreadChecksTunings = true;
}

SurgeGUIEditor::~SurgeGUIEditor()
{
    juce::PopupMenu::dismissAllActiveMenus();
    juce::Desktop::getInstance().removeFocusChangeListener(this);
    synth->removeModulationAPIListener(this);
    synth->storage.clearOkCancelProvider();
    auto isPop = synth->storage.getPatch().dawExtraState.isPopulated;
    populateDawExtraState(synth); // If I must die, leave my state for future generations
    synth->storage.getPatch().dawExtraState.isPopulated = isPop;
    synth->storage.removeErrorListener(this);
    synth->storage.uiThreadChecksTunings = false;
}

void SurgeGUIEditor::forceLFODisplayRebuild() { lfoDisplay->repaint(); }

void SurgeGUIEditor::idle()
{
    if (!synth)
    {
        return;
    }

    if (noProcessingOverlay)
    {
        if (synth->processRunning == 0)
        {
            frame->removeChildComponent(noProcessingOverlay.get());
            noProcessingOverlay.reset(nullptr);
        }
    }
    else
    {
        if (processRunningCheckEvery++ > 3 || processRunningCheckEvery < 0)
        {
            synth->processRunning++;

            if (synth->processRunning > 10)
            {
                synth->audio_processing_active = false;
            }

            processRunningCheckEvery = 0;
        }
    }

    if (pause_idle_updates)
    {
        return;
    }

    if (slowIdleCounter++ == 600)
    {
        slowIdleCounter = 0;
        juceEditor->getSurgeLookAndFeel()->updateDarkIfNeeded();
    }

    if (needsModUpdate)
    {
        refresh_mod();
        needsModUpdate = false;
    }

    if (sendStructureChangeIn > 0)
    {
        sendStructureChangeIn--;

        if (sendStructureChangeIn == 0)
        {
            if (auto *handler = frame->getAccessibilityHandler())
            {
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::structureChanged);
            }
        }
    }

    if (!accAnnounceStrings.empty())
    {
        auto h = frame->getAccessibilityHandler();

        if (h)
        {
            auto &nm = accAnnounceStrings.front();

            if (nm.second == 0)
            {
                h->postAnnouncement(nm.first,
                                    juce::AccessibilityHandler::AnnouncementPriority::high);
            }
        }

        accAnnounceStrings.front().second--;

        if (accAnnounceStrings.front().second < 0)
        {
            accAnnounceStrings.pop_front();
        }
    }

    if (getShowVirtualKeyboard() && synth->hasUpdatedMidiCC)
    {
        synth->hasUpdatedMidiCC = false;
        juceEditor->setPitchModSustainGUI(synth->pitchbendMIDIVal, synth->modwheelCC,
                                          synth->sustainpedalCC);
    }

#ifndef SURGE_SKIP_ODDSOUND_MTS
    getStorage()->send_tuning_update();
#endif
    if (editor_open && frame && !synth->halt_engine)
    {
        bool patchChanged = false;

        if (patchSelector)
        {
            patchChanged = patchSelector->sel_id != synth->patchid;

            if (synth->storage.getPatch().isDirty != patchSelector->isDirty)
            {
                patchSelector->isDirty = synth->storage.getPatch().isDirty;
                patchSelector->repaint();
            }
        }

        if (firstErrorIdleCountdown > 0)
        {
            firstErrorIdleCountdown--;
        }
        else if (queue_refresh || synth->refresh_editor || patchChanged)
        {
            // dont show an alert during a rebuild
        }
        else if (errorItemCount)
        {
            if (errorItemCount > 1 && alert && alert->isVisible())
            {
                // don't show the double error
            }
            else
            {
                decltype(errorItems)::value_type cp;
                {
                    std::lock_guard<std::mutex> g(errorItemsMutex);
                    cp = errorItems.front();
                    errorItems.pop_front();
                    errorItemCount--;
                }

                auto &[msg, title, code] = cp;
                if (code == SurgeStorage::AUDIO_INPUT_LATENCY_WARNING)
                {
                    audioLatencyNotified = true;
                    frame->repaint();
                }
                else
                {
                    messageBox(title, msg);
                }
            }
        }

        if (lastObservedMidiNoteEventCount != synth->midiNoteEvents)
        {
            lastObservedMidiNoteEventCount = synth->midiNoteEvents;

            auto tun = getOverlayIfOpenAs<Surge::Overlays::TuningOverlay>(TUNING_EDITOR);

            if (tun)
            {
                // If there are things subscribed to keys update them here
                std::bitset<128> keyOn{0};

                for (int sc = 0; sc < n_scenes; ++sc)
                {
                    for (int k = 0; k < 128; ++k)
                    {
                        if (synth->midiKeyPressedForScene[sc][k] > 0)
                        {
                            keyOn[k] = 1;
                        }
                    }
                }

                tun->setMidiOnKeys(keyOn);
            }
        }

        idleInfowindow();
        juceDeleteOnIdle.clear();

        /*
         * USEFUL for testing stress patch changes
         *
        static int runct = 0;
        if( runct++ == 5 )
        {
           synth->patchid_queue = rand() % 1800;
           runct = 0;
        }
          */

        if (firstIdleCountdown)
        {
            // Linux VST3 in JUCE hosts (maybe others?) sets up the run loop out of order, it seems
            // sometimes missing the very first invalidation. Force a redraw on the first idle
            // isFirstIdle = false;
            firstIdleCountdown--;

            frame->repaint();
        }

        if (synth->learn_param_from_cc < 0 && synth->learn_macro_from_cc < 0 &&
            synth->learn_param_from_note < 0 && midiLearnOverlay != nullptr)
        {
            hideMidiLearnOverlay();
        }

        if (lfoDisplayRepaintCountdown > 0)
        {
            lfoDisplayRepaintCountdown--;

            if (lfoDisplayRepaintCountdown == 0)
            {
                forceLFODisplayRebuild();
            }
        }

        if (undoButton && redoButton)
        {
            auto us = dynamic_cast<Surge::Widgets::Switch *>(undoButton);
            auto rs = dynamic_cast<Surge::Widgets::Switch *>(redoButton);
            auto hasU = undoManager()->canUndo();
            auto hasR = undoManager()->canRedo();

            if (hasU != (!us->isDeactivated))
            {
                us->setDeactivated(!hasU);
                us->repaint();
            }

            if (hasR != (!rs->isDeactivated))
            {
                rs->setDeactivated(!hasR);
                rs->repaint();
            }
        }

        {
            bool expected = true;

            if (synth->rawLoadNeedsUIDawExtraState.compare_exchange_weak(expected, true) &&
                expected)
            {
                std::lock_guard<std::mutex> g(synth->rawLoadQueueMutex);
                synth->rawLoadNeedsUIDawExtraState = false;
                loadFromDawExtraState(synth);
            }
        }

        if (patchCountdown >= 0 && !pause_idle_updates)
        {
            patchCountdown--;

            if (patchCountdown < 0 && synth->patchid_queue >= 0)
            {
                std::ostringstream oss;

                oss << "Loading patch " << synth->patchid_queue
                    << " has not occurred after 200 idle cycles. This means that the audio system"
                    << " is delayed while loading many patches in a row. The audio system has to be"
                    << " running in order to load Surge XT patches. If the audio system is working,"
                       " you can probably ignore this message and continue once Surge XT catches "
                       "up.";

                synth->storage.reportError(oss.str(), "Patch Loading Error");
            }
        }

        if (zoomInvalid)
        {
            auto rg = SurgeSynthEditor::BlockRezoom(juceEditor);

            setZoomFactor(getZoomFactor());
            zoomInvalid = false;
        }

        if (showMSEGEditorOnNextIdleOrOpen)
        {
            showOverlay(SurgeGUIEditor::MSEG_EDITOR);
            showMSEGEditorOnNextIdleOrOpen = false;
        }

        if (!overlaysForNextIdle.empty())
        {
            for (auto ol : overlaysForNextIdle)
            {
                auto tag = (OverlayTags)ol.whichOverlay;

                showOverlay(tag);

                if (ol.isTornOut)
                {
                    auto olw = getOverlayWrapperIfOpen(tag);

                    if (olw)
                    {
                        auto p =
                            juce::Point<int>(ol.tearOutPosition.first, ol.tearOutPosition.second);
                        olw->doTearOut(p);
                    }
                }
            }

            overlaysForNextIdle.clear();
        }

        auto ol = getOverlayIfOpenAs<Surge::Overlays::FormulaModulatorEditor>(FORMULA_EDITOR);

        if (ol)
        {
            ol->updateDebuggerIfNeeded();
        }

        auto wt =
            &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]].wt;
        if (wt->refresh_display)
        {
            wt->refresh_display = false;

            if (oscWaveform)
            {
                if (wt->is_dnd_imported)
                {
                    oscWaveform->repaintForceForWT();
                }
                oscWaveform->repaint();
            }
        }

        if (polydisp)
        {
            int prior = polydisp->getPlayingVoiceCount();

            if (prior != synth->storage.activeVoiceCount)
            {
                polydisp->setPlayingVoiceCount(synth->storage.activeVoiceCount);
                polydisp->repaint();
            }
        }

        if (statusMPE)
        {
            auto v = statusMPE->getValue();

            if ((v < 0.5 && synth->mpeEnabled) || (v > 0.5 && !synth->mpeEnabled))
            {
                statusMPE->setValue(synth->mpeEnabled ? 1 : 0);
                statusMPE->asJuceComponent()->repaint();
            }
        }

        if (statusTune)
        {
            auto v = statusTune->getValue();

            if ((v < 0.5 && !synth->storage.isStandardTuning) ||
                (v > 0.5 && synth->storage.isStandardTuning))
            {
                bool hasmts = synth->storage.oddsound_mts_client &&
                              synth->storage.oddsound_mts_active_as_client;

                statusTune->setValue(!synth->storage.isStandardTuning || hasmts);
                statusTune->asJuceComponent()->repaint();
            }
        }

        if (patchSelector)
        {
            patchSelector->idle();
        }

        if (patchChanged)
        {
            for (int i = 0; i < n_fx_slots; ++i)
            {
                fxPresetName[i] = "";
            }
        }

        if (patchChanged)
        {
            if (!firstTimePatchLoad)
            {
                std::ostringstream oss;
                oss << synth->storage.getPatch().name << " from category "
                    << synth->storage.getPatch().category << " by "
                    << synth->storage.getPatch().author;
                enqueueAccessibleAnnouncement(oss.str());
            }

            firstTimePatchLoad = false;
        }

        if (queue_refresh || synth->refresh_editor || patchChanged)
        {
            queue_refresh = false;
            synth->refresh_editor = false;

            if (frame)
            {
                if (synth->patch_loaded)
                {
                    mod_editor = false;
                }

                synth->patch_loaded = false;

                openOrRecreateEditor();
            }

            if (patchSelector)
            {
                patchSelector->sel_id = synth->patchid;
                patchSelector->setLabel(synth->storage.getPatch().name);
                patchSelector->setCategory(synth->storage.getPatch().category);
                patchSelector->setAuthor(synth->storage.getPatch().author);
                patchSelector->setComment(synth->storage.getPatch().comment);
                patchSelector->setIsFavorite(isPatchFavorite());
                patchSelector->setIsUser(isPatchUser());
                patchSelector->setTags(synth->storage.getPatch().tags);
                patchSelector->repaint();
            }
        }

        if (patchChanged)
        {
            refreshOverlayWithOpenClose(MSEG_EDITOR);
            refreshOverlayWithOpenClose(FORMULA_EDITOR);
            refreshOverlayWithOpenClose(WT_EDITOR);
            refreshOverlayWithOpenClose(TUNING_EDITOR);
            refreshOverlayWithOpenClose(MODULATION_EDITOR);
        }

        bool vuInvalid = false;

        if (vu[0])
        {
            if (synth->vu_peak[0] != vu[0]->getValue())
            {
                vuInvalid = true;
                vu[0]->setValue(synth->vu_peak[0]);
            }

            if (synth->vu_peak[1] != vu[0]->getValueR())
            {
                vu[0]->setValueR(synth->vu_peak[1]);
                vuInvalid = true;
            }

            vu[0]->setIsAudioActive(synth->audio_processing_active);

            if (synth->cpu_level != vu[0]->getCpuLevel())
            {
                vu[0]->setCpuLevel(synth->cpu_level);
                vuInvalid = true;
            }

            if (vuInvalid)
            {
                vu[0]->repaint();
            }
        }

        for (int i = 0; i < n_fx_slots; i++)
        {
            assert(i + 1 < Effect::KNumVuSlots);

            if (vu[i + 1] && synth->fx[current_fx])
            {
                vu[i + 1]->setValue(synth->fx[current_fx]->vu[(i << 1)]);
                vu[i + 1]->setValueR(synth->fx[current_fx]->vu[(i << 1) + 1]);
                vu[i + 1]->repaint();
            }
        }

        for (int i = 0; i < 8; i++)
        {
            if (synth->refresh_ctrl_queue[i] >= 0)
            {
                int j = synth->refresh_ctrl_queue[i];

                synth->refresh_ctrl_queue[i] = -1;

                if (param[j])
                {
                    char pname[TXT_SIZE], pdisp[TXT_SIZE];
                    SurgeSynthesizer::ID jid;

                    if (synth->fromSynthSideId(j, jid))
                    {
                        synth->getParameterName(jid, pname);
                        synth->getParameterDisplay(jid, pdisp);
                    }

                    param[j]->asControlValueInterface()->setValue(
                        synth->refresh_ctrl_queue_value[i]);
                    param[j]->setQuantitizedDisplayValue(synth->refresh_ctrl_queue_value[i]);
                    param[j]->asJuceComponent()->repaint();

                    if (oscWaveform)
                    {
                        oscWaveform->repaintIfIdIsInRange(j);
                    }

                    if (lfoDisplay)
                    {
                        lfoDisplay->repaintIfIdIsInRange(j);
                    }

                    auto sp = getStorage()->getPatch().param_ptr[j];

                    if (sp)
                    {
                        if (sp->ctrlgroup == cg_FILTER)
                        {
                            // force repaint any filter overlays
                            auto fa = getOverlayIfOpenAs<Surge::Overlays::FilterAnalysis>(
                                OverlayTags::FILTER_ANALYZER);

                            if (fa)
                            {
                                fa->forceDataRefresh();
                            }
                        }
                    }
                }
            }
        }

        if (lastTempo != synth->time_data.tempo || lastTSNum != synth->time_data.timeSigNumerator ||
            lastTSDen != synth->time_data.timeSigDenominator)
        {
            lastTempo = synth->time_data.tempo;
            lastTSNum = synth->time_data.timeSigNumerator;
            lastTSDen = synth->time_data.timeSigDenominator;

            if (lfoDisplay)
            {
                lfoDisplay->setTimeSignature(synth->time_data.timeSigNumerator,
                                             synth->time_data.timeSigDenominator);
                lfoDisplay->repaintIfAnythingIsTemposynced();
            }
        }

        std::vector<int> refreshIndices;

        if (synth->refresh_overflow)
        {
            refreshIndices.resize(n_total_params);
            std::iota(std::begin(refreshIndices), std::end(refreshIndices), 0);
            frame->repaint();
        }
        else
        {
            for (int i = 0; i < 8; ++i)
            {
                if (synth->refresh_parameter_queue[i] >= 0)
                {
                    refreshIndices.push_back(synth->refresh_parameter_queue[i]);
                }
            }
        }

        synth->refresh_overflow = false;

        for (int i = 0; i < 8; ++i)
        {
            synth->refresh_parameter_queue[i] = -1;
        }

        for (auto j : refreshIndices)
        {
            if ((j < n_total_params) && param[j])
            {
                SurgeSynthesizer::ID jid;

                if (synth->fromSynthSideId(j, jid))
                {
                    param[j]->asControlValueInterface()->setValue(synth->getParameter01(jid));
                    param[j]->setQuantitizedDisplayValue(synth->getParameter01(jid));
                    param[j]->asJuceComponent()->repaint();

                    { // force repaint any filter overlays when the host changes filter params
                        bool refreshFilterAnalyser =
                            param[j]->asControlValueInterface()->getTag() ==
                                filterCutoffSlider[0]->asControlValueInterface()->getTag() ||
                            param[j]->asControlValueInterface()->getTag() ==
                                filterResonanceSlider[0]->asControlValueInterface()->getTag() ||
                            param[j]->asControlValueInterface()->getTag() ==
                                filterCutoffSlider[1]->asControlValueInterface()->getTag() ||
                            param[j]->asControlValueInterface()->getTag() ==
                                filterResonanceSlider[1]->asControlValueInterface()->getTag();

                        if (refreshFilterAnalyser)
                        {
                            auto sp = getStorage()->getPatch().param_ptr[j];

                            if (sp)
                            {
                                if (sp->ctrlgroup == cg_FILTER)
                                {
                                    // force repaint any filter overlays
                                    auto fa = getOverlayIfOpenAs<Surge::Overlays::FilterAnalysis>(
                                        OverlayTags::FILTER_ANALYZER);

                                    if (fa)
                                    {
                                        fa->forceDataRefresh();
                                    }
                                }
                            }
                        }
                    }
                }

                if (oscWaveform)
                {
                    oscWaveform->repaintIfIdIsInRange(j);
                }

                if (lfoDisplay)
                {
                    lfoDisplay->repaintIfIdIsInRange(j);
                }
            }
            else if ((j >= 0) && (j < n_total_params) && nonmod_param[j])
            {
                /*
                ** What the heck is this NONMOD_PARAM thing?
                **
                ** There are a set of params - like discrete things like
                ** octave and filter type - which are not LFO modulatable
                ** and aren't in the params[] array. But they are exposed
                ** properties, so you can control them from a DAW. The
                ** DAW control works - everything up to this path (as described
                ** in #160) works fine and sets the value but since there's
                ** no CControl in param the above bails out. But adding
                ** all these controls to param[] would have the unintended
                ** side effect of giving them all the other param[] behaviours.
                ** So have a second array and drop select items in here so we
                ** can actually get them redrawing when an external param set occurs.
                */
                auto *cc = nonmod_param[j];

                // Some state changes enable and disable sliders. If this is one of those state
                // changes and a value has changed, then we need to invalidate them.
                // See https://github.com/surge-synthesizer/surge/issues/2056
                auto tag = cc->getTag();
                SurgeSynthesizer::ID jid;

                auto sv = 0.f;

                if (synth->fromSynthSideId(j, jid))
                {
                    sv = synth->getParameter01(jid);
                }

                auto cv = cc->getValue();

                if ((sv != cv) && ((tag == fmconfig_tag || tag == filterblock_tag)))
                {
                    std::unordered_map<int, bool> resetMap;

                    if (tag == fmconfig_tag)
                    {
                        auto targetTag =
                            synth->storage.getPatch().scene[current_scene].fm_depth.id +
                            start_paramtags;
                        auto targetState =
                            (Parameter::intUnscaledFromFloat(sv, n_fm_routings - 1) == fm_off);

                        resetMap[targetTag] = targetState;
                    }

                    if (tag == filterblock_tag)
                    {
                        auto pval = Parameter::intUnscaledFromFloat(sv, n_filter_configs - 1);

                        auto targetTag =
                            synth->storage.getPatch().scene[current_scene].feedback.id +
                            start_paramtags;
                        auto targetState = (pval == fc_serial1);

                        resetMap[targetTag] = targetState;
                        targetTag = synth->storage.getPatch().scene[current_scene].width.id +
                                    start_paramtags;
                        targetState = (pval != fc_stereo && pval != fc_wide);
                        resetMap[targetTag] = targetState;
                    }

                    for (int i = 0; i < n_paramslots; i++)
                    {
                        if (param[i] &&
                            (resetMap.find(param[i]->asControlValueInterface()->getTag()) !=
                             resetMap.end()))
                        {
                            param[i]->setDeactivated(
                                resetMap[param[i]->asControlValueInterface()->getTag()]);
                            param[i]->asJuceComponent()->repaint();
                        }
                    }
                }

                if (synth->storage.getPatch().param_ptr[j]->ctrltype == ct_scenemode)
                {
                    // This is gross hack for our reordering of scene mode
                    // Basically take the automation value and turn it into the UI value
                    auto pval = Parameter::intUnscaledFromFloat(sv, n_scene_modes - 1);

                    if (pval == sm_dual)
                    {
                        pval = sm_chsplit;
                    }
                    else if (pval == sm_chsplit)
                    {
                        pval = sm_dual;
                    }

                    sv = Parameter::intScaledToFloat(pval, n_scene_modes - 1);
                }

                if (sv != cv)
                {
                    cc->setValue(sv);
                }

                // Integer switches also work differently
                auto assw = dynamic_cast<Surge::Widgets::Switch *>(cc);

                if (assw)
                {
                    if (assw->isMultiIntegerValued())
                    {
                        assw->setIntegerValue(synth->storage.getPatch().param_ptr[j]->val.i + 1);
                    }
                }

                auto bvf = cc->asJuceComponent();

                if (bvf)
                {
                    bvf->repaint();
                }
                else
                {
                    jassert(false);
                }
            }
#if 0
         /*
         ** A set of special things which invalidate the entire UI.
         ** See #2226 for why this is currently off.
         */
         else if( j == synth->storage.getPatch().scene_active.id )
         {
            if( current_scene != synth->storage.getPatch().scene_active.val.i )
            {
               synth->refresh_editor = true;
               current_scene = synth->storage.getPatch().scene_active.val.i;
            }
         }
#endif
        }

        for (int i = 0; i < n_customcontrollers; i++)
        {
            if (((ControllerModulationSource *)synth->storage.getPatch()
                     .scene[current_scene]
                     .modsources[ms_ctrl1 + i])
                    ->has_changed(0, true))
            {
                gui_modsrc[ms_ctrl1 + i]->setValue(
                    ((ControllerModulationSource *)synth->storage.getPatch()
                         .scene[current_scene]
                         .modsources[ms_ctrl1 + i])
                        ->get_target01(0));
            }
        }
    }

    // refresh waveform display if the mute state of the currently displayed osc changes
    if (oscWaveform)
    {
        oscWaveform->repaintBasedOnOscMuteState();
    }

    // force the oscilloscope, if open, to re-render for nice smooth movement.
    auto scope = getOverlayIfOpenAs<Surge::Overlays::Oscilloscope>(OSCILLOSCOPE);

    if (scope)
    {
        scope->updateDrawing();
    }

    if (scanJuceSkinComponents)
    {
        std::vector<Surge::GUI::Skin::Control::sessionid_t> toRemove;

        for (const auto &c : juceSkinComponents)
        {
            if (!currentSkin->containsControlWithSessionId(c.first))
            {
                toRemove.push_back(c.first);
            }
        }

        for (const auto &sid : toRemove)
        {
            juceSkinComponents.erase(sid);
        }

        scanJuceSkinComponents = false;
    }

    if (synth->storage.oddsound_mts_active_as_client)
    {
        auto w = getOverlayWrapperIfOpen(TUNING_EDITOR);
        if (w && (slowIdleCounter % 30 == 0))
        {
            w->repaint();
        }
    }

    juceEditor->fireListenersOnEndEdit = false;
    for (int s = 0; s < n_scenes; ++s)
    {
        for (int o = 0; o < n_oscs; ++o)
        {
            if (synth->resendOscParam[s][o])
            {
                auto &par = synth->storage.getPatch().scene[s].osc[o].type;
                SurgeSynthesizer::ID eid = synth->idForParameter(&par);

                juceEditor->beginParameterEdit(&par);
                synth->getParent()->surgeParameterUpdated(eid, par.get_value_f01());
                synth->resendOscParam[s][o] = false;

                juceEditor->endParameterEdit(&par);
            }
        }
    }

    for (int s = 0; s < n_fx_slots; ++s)
    {
        if (synth->resendFXParam[s])
        {
            auto &par = synth->storage.getPatch().fx[s].type;
            SurgeSynthesizer::ID eid = synth->idForParameter(&par);

            juceEditor->beginParameterEdit(&par);
            synth->getParent()->surgeParameterUpdated(eid, par.get_value_f01());
            synth->resendFXParam[s] = false;

            juceEditor->endParameterEdit(&par);
        }
    }
    juceEditor->fireListenersOnEndEdit = true;
}

void SurgeGUIEditor::toggle_mod_editing()
{
    bool doModEditing = true;

    if (currentSkin->getVersion() >= 2)
    {
        auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");

        if (!skinCtrl)
        {
            skinCtrl = currentSkin->getOrCreateControlForConnector(
                Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
        }

        if (skinCtrl->classname == Surge::GUI::NoneClassName)
        {
            doModEditing = false;
        }
    }

    if (doModEditing)
    {
        mod_editor = !mod_editor;
        refresh_mod();
    }
}

void SurgeGUIEditor::setModsourceSelected(modsources ms, int ms_idx)
{
    modsource = ms;
    modsource_editor[current_scene] = modsource;

    modsource_index = ms_idx;
    // FIXME: assumed scene LFOs are always listed after all voice LFOs in the enum
    modsource_index_cache[current_scene][ms_idx - ms_lfo1] = ms_idx;

    if (gui_modsrc[modsource])
    {
        gui_modsrc[modsource]->modlistIndex = modsource_index;
        gui_modsrc[modsource]->repaint();
    }

    refresh_mod();
    synth->refresh_editor = true;
}

void SurgeGUIEditor::refresh_mod()
{
    modsources thisms = modsource;

    synth->storage.modRoutingMutex.lock();

    for (int i = 0; i < n_paramslots; i++)
    {
        if (param[i])
        {
            auto s = param[i];
            auto p = synth->storage.getPatch().param_ptr[i];

            if (p)
            {
                s->setIsValidToModulate(synth->isValidModulation(p->id, thisms));
            }

            if (s->getIsValidToModulate())
            {
                auto use_scene = 0;

                if (this->synth->isModulatorDistinctPerScene(thisms))
                {
                    use_scene = current_scene;
                }

                s->setIsEditingModulation(mod_editor);
                s->setModulationState(
                    synth->isModDestUsed(i),
                    synth->isActiveModulation(i, thisms, use_scene, modsource_index));
                s->setIsModulationBipolar(synth->isBipolarModulation(thisms));
                s->setModValue(synth->getModDepth01(i, thisms, use_scene, modsource_index));
            }
            else
            {
                s->setIsEditingModulation(false);
            }

            s->asJuceComponent()->repaint();
        }
    }

    synth->storage.modRoutingMutex.unlock();

    // This loop is the problem
    for (int i = 1; i < n_modsources; i++)
    {
        int state = 0;

        if (i == modsource_editor[current_scene] && lfoNameLabel)
        {
            state |= 4;

            // update the LFO title label
            std::string modname = ModulatorName::modulatorName(
                &synth->storage, modsource_editor[current_scene], true, current_scene);

            lfoNameLabel->setText(modname.c_str());
        }

        if (gui_modsrc[i])
        {
            // this could change if I cleared the last one
            gui_modsrc[i]->setUsed(synth->isModsourceUsed((modsources)i));
            gui_modsrc[i]->setState(state);
            setupAlternates((modsources)i);
            gui_modsrc[i]->repaint();
        }
    }

    // Now find and set the state for the right modsource
    if (modsource > 0)
    {
        int state = mod_editor ? 2 : 1;

        if (modsource == modsource_editor[current_scene])
        {
            state |= 4;
        }

        for (int i = 1; i < n_modsources; i++)
        {
            if (gui_modsrc[i] && gui_modsrc[i]->containsModSource(modsource))
            {
                gui_modsrc[i]->setState(state);
                gui_modsrc[i]->repaint();
            }
        }
    }
}

bool SurgeGUIEditor::isControlVisible(ControlGroup controlGroup, int controlGroupEntry)
{
    switch (controlGroup)
    {
    case cg_OSC:
        return (controlGroupEntry == current_osc[current_scene]);
    case cg_LFO:
        return (controlGroupEntry == modsource_editor[current_scene]);
    case cg_FX:
        return (controlGroupEntry == current_fx);
    default:
        return true;
    }
}

juce::Rectangle<int> SurgeGUIEditor::positionForModulationGrid(modsources entry)
{
    bool isMacro = isCustomController(entry);
    int gridX = Surge::GUI::ModulationGrid::getModulationGrid()->get(entry).x;
    int gridY = Surge::GUI::ModulationGrid::getModulationGrid()->get(entry).y;
    int width = isMacro ? 90 : 72;
    auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");

    if (!skinCtrl)
    {
        skinCtrl = currentSkin->getOrCreateControlForConnector(
            Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
    }

    if (skinCtrl->classname == Surge::GUI::NoneClassName && currentSkin->getVersion() >= 2)
    {
        return juce::Rectangle<int>();
    }

    auto r = juce::Rectangle<int>(skinCtrl->x, skinCtrl->y, width - 1, 14);

    if (isMacro)
    {
        r = r.withTrimmedBottom(-8);
    }

    int offsetX = 23;

    for (int i = 0; i < gridX; i++)
    {
        offsetX += width;
    }

    r = r.translated(offsetX, 8 * gridY);

    return r;
}

juce::Rectangle<int> SurgeGUIEditor::positionForModOverview()
{
    auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");

    if (!skinCtrl)
    {
        skinCtrl = currentSkin->getOrCreateControlForConnector(
            Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
    }

    if (skinCtrl->classname == Surge::GUI::NoneClassName && currentSkin->getVersion() >= 2)
    {
        return juce::Rectangle<int>();
    }

    auto r = juce::Rectangle<int>(skinCtrl->x, skinCtrl->y - 1, 22, 16 * 4 + 8).reduced(1);

    return r;
}

void SurgeGUIEditor::setDisabledForParameter(Parameter *p,
                                             Surge::Widgets::ModulatableControlInterface *s)
{
    if (p->id == synth->storage.getPatch().scene[current_scene].fm_depth.id)
    {
        s->setDeactivated(!(synth->storage.getPatch().scene[current_scene].fm_switch.val.i));
    }
}

void SurgeGUIEditor::openOrRecreateEditor()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SurgeGUIEditor::openOrRecreateEditor");
#endif

    if (!synth)
    {
        return;
    }

    assert(frame);

    bool somethingHasFocus{false};

    if (editor_open)
    {
        somethingHasFocus = (juce::Component::getCurrentlyFocusedComponent() != nullptr);
        hideMidiLearnOverlay();
        close_editor();
    }

    std::unordered_map<std::string, std::string> uiidToSliderLabel;
    current_scene = synth->storage.getPatch().scene_active.val.i;

    // In Surge 1.8, the skin engine can change the position of this panel as a whole,
    // but not anything else about it. The skin query happens inside positionForModulationGrid
    for (int k = 1; k < n_modsources; k++)
    {
        modsources ms = (modsources)k;
        auto e = Surge::GUI::ModulationGrid::getModulationGrid()->get(ms);

        if (e.isPrimary)
        {
            int state = 0;
            auto r = positionForModulationGrid(ms);

            if (ms == modsource)
            {
                state = mod_editor ? 2 : 1;
            }

            if (ms == modsource_editor[current_scene])
            {
                state |= 4;
            }

            if (!gui_modsrc[ms])
            {
                gui_modsrc[ms] = std::make_unique<Surge::Widgets::ModulationSourceButton>();
            }

            auto ff = currentSkin->getFont(Fonts::Widgets::ModButtonFont);

            gui_modsrc[ms]->setFont(ff);
            gui_modsrc[ms]->setTag(tag_mod_source0 + int(ms));
            gui_modsrc[ms]->addListener(this);
            gui_modsrc[ms]->setSkin(currentSkin, bitmapStore);
            gui_modsrc[ms]->setStorage(&(synth->storage));
            gui_modsrc[ms]->update_rt_vals(false, 0, synth->isModsourceUsed(ms));

            setupAlternates(ms);

            if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
            {
                gui_modsrc[ms]->setCurrentModLabel(
                    synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
                gui_modsrc[ms]->setIsMeta(true);
                gui_modsrc[ms]->setIsBipolar(
                    synth->storage.getPatch().scene[current_scene].modsources[ms]->is_bipolar());
                gui_modsrc[ms]->setValue(((ControllerModulationSource *)synth->storage.getPatch()
                                              .scene[current_scene]
                                              .modsources[ms])
                                             ->get_target01(0));
            }

            // setBounds needs to happen after setIsMeta since resized activates bounds based on
            // metaness
            gui_modsrc[ms]->setBounds(r);
            addAndMakeVisibleWithTracking(frame->getModButtonLayer(), *gui_modsrc[ms]);

            if (ms >= ms_ctrl1 && ms <= ms_ctrl8 && synth->learn_macro_from_cc == ms - ms_ctrl1)
            {
                showMidiLearnOverlay(r);
            }
        }
    }

    // reset the lfo rate slider pointer
    lfoRateSlider = nullptr;
    filterCutoffSlider[0] = nullptr;
    filterCutoffSlider[1] = nullptr;
    filterResonanceSlider[0] = nullptr;
    filterResonanceSlider[1] = nullptr;

    auto moRect = positionForModOverview();

    if (!modOverviewLauncher)
    {
        modOverviewLauncher = std::make_unique<Surge::Widgets::ModulationOverviewLaunchButton>(
            this, &(synth->storage));
    }

    modOverviewLauncher->setBounds(moRect);
    modOverviewLauncher->setSkin(currentSkin);
    addAndMakeVisibleWithTracking(frame->getModButtonLayer(), *modOverviewLauncher);

    // FX VU meters and labels. This is all a bit hacky still
    {
        std::lock_guard<std::mutex> g(synth->fxSpawnMutex);

        if (synth->fx[current_fx])
        {
            auto fxpp = currentSkin->getOrCreateControlForConnector("fx.param.panel");
            auto fxRect = juce::Rectangle<int>(fxpp->x, fxpp->y, 123, 13);

            for (int i = 0; i < 15; i++)
            {
                auto t = synth->fx[current_fx]->vu_type(i);

                if (t)
                {
                    auto vr = fxRect.translated(6, yofs * synth->fx[current_fx]->vu_ypos(i))
                                  .translated(0, -14);

                    if (!vu[i + 1])
                    {
                        vu[i + 1] = std::make_unique<Surge::Widgets::VuMeter>();
                    }

                    vu[i + 1]->setBounds(vr);
                    vu[i + 1]->setSkin(currentSkin, bitmapStore);
                    vu[i + 1]->setType(t);

                    addAndMakeVisibleWithTracking(frame.get(), *vu[i + 1]);
                }
                else
                {
                    vu[i + 1] = 0;
                }

                const char *label = synth->fx[current_fx]->group_label(i);

                if (label)
                {
                    auto vr = fxRect.withTrimmedTop(-1)
                                  .withTrimmedRight(-5)
                                  .translated(5, -12)
                                  .translated(0, yofs * synth->fx[current_fx]->group_label_ypos(i));

                    if (!effectLabels[i])
                    {
                        effectLabels[i] = std::make_unique<Surge::Widgets::EffectLabel>();
                    }

                    effectLabels[i]->setBounds(vr);
                    effectLabels[i]->setSkin(currentSkin, bitmapStore);

                    if (i == 0 &&
                        synth->storage.getPatch().fx[current_fx].type.val.i == fxt_vocoder)
                    {
                        effectLabels[i]->setLabel(
                            fmt::format("Input delayed {} samples", BLOCK_SIZE));
                    }
                    else
                    {
                        effectLabels[i]->setLabel(label);
                    }

                    addAndMakeVisibleWithTracking(frame.get(), *effectLabels[i]);
                }
                else
                {
                    effectLabels[i].reset(nullptr);
                }
            }
        }
    }

    // Loop over the non-associated controls
    for (auto i = Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED + 1;
         i < Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED; ++i)
    {
        Surge::Skin::Connector::NonParameterConnection npc =
            (Surge::Skin::Connector::NonParameterConnection)i;
        auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
        auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

        currentSkin->resolveBaseParentOffsets(skinCtrl);

        if (!skinCtrl)
        {
            std::cout << "Unable to find SkinCtrl" << std::endl;
            continue;
        }

        if (skinCtrl->classname == Surge::GUI::NoneClassName)
        {
            continue;
        }

        // Many of the controls are special and require non-generalizable constructors handled here.
        // Some are standard, so once we know the tag, we can use layoutComponentForSkin,
        // But it's not worth generalizing the OSCILLATOR_DISPLAY beyond this, say.
        switch (npc)
        {
        case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_DISPLAY:
        {
            if (!oscWaveform)
            {
                oscWaveform = std::make_unique<Surge::Widgets::OscillatorWaveformDisplay>();
            }

            oscWaveform->setBounds(skinCtrl->getRect());
            oscWaveform->setSkin(currentSkin, bitmapStore);
            oscWaveform->setStorage(&(synth->storage));
            oscWaveform->setOscStorage(&(synth->storage.getPatch()
                                             .scene[synth->storage.getPatch().scene_active.val.i]
                                             .osc[current_osc[current_scene]]));
            oscWaveform->setSurgeGUIEditor(this);
            oscWaveform->onOscillatorTypeChanged();

            setAccessibilityInformationByTitleAndAction(oscWaveform.get(),
                                                        "Oscillator Waveform Display", "Display");

            addAndMakeVisibleWithTracking(frame->getControlGroupLayer(cg_OSC), *oscWaveform);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::SURGE_MENU:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_settingsmenu);
            mainMenu = q->asJuceComponent();
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Main Menu", "Open");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_SELECT:
        {
            auto oscswitch = layoutComponentForSkin(skinCtrl, tag_osc_select);
            oscswitch->setValue((float)current_osc[current_scene] / 2.0f);
            setAccessibilityInformationByTitleAndAction(oscswitch->asJuceComponent(),
                                                        "Oscillator Number", "Select");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_PATCHCATEGORY:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_mp_category);
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Jog Patch Category",
                                                        "Jog");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_PATCH:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_mp_patch);
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Jog Patch", "Jog");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_WAVESHAPE:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_mp_jogwaveshape);
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Jog Waveshape",
                                                        "Jog");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::ANALYZE_WAVESHAPE:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_analyzewaveshape);
            q->setValue(isAnyOverlayPresent(WAVESHAPER_ANALYZER) ? 1 : 0);
            // setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Analyze
            // Waveshape",
            //                                             "Open");
            //  This is a purely visual representation so dont show the button
            q->asJuceComponent()->setAccessible(false);
            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::ANALYZE_FILTERS:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_analyzefilters);
            q->setValue(isAnyOverlayPresent(FILTER_ANALYZER) ? 1 : 0);
            // setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Analyze Filters",
            //                                             "Open");
            q->asJuceComponent()->setAccessible(false);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::JOG_FX:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_mp_jogfx);
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "FX Preset", "Jog");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_MPE:
        {
            statusMPE = layoutComponentForSkin(skinCtrl, tag_status_mpe);
            statusMPE->setValue(synth->mpeEnabled ? 1 : 0);
            setAccessibilityInformationByTitleAndAction(statusMPE->asJuceComponent(), "MPE",
                                                        "Configure");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_TUNE:
        {
            statusTune = layoutComponentForSkin(skinCtrl, tag_status_tune);
            auto hasmts =
                synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active_as_client;
            statusTune->setValue(synth->storage.isStandardTuning ? hasmts
                                                                 : synth->storage.isToggledToCache);
            setAccessibilityInformationByTitleAndAction(statusTune->asJuceComponent(), "Tune",
                                                        "Configure");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::STATUS_ZOOM:
        {
            statusZoom = layoutComponentForSkin(skinCtrl, tag_status_zoom);
            setAccessibilityInformationByTitleAndAction(statusZoom->asJuceComponent(), "Zoom",
                                                        "Configure");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::ACTION_UNDO:
        {
            actionUndo = layoutComponentForSkin(skinCtrl, tag_action_undo);
            setAccessibilityInformationByTitleAndAction(actionUndo->asJuceComponent(), "Undo",
                                                        "Undo");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::ACTION_REDO:
        {
            actionRedo = layoutComponentForSkin(skinCtrl, tag_action_redo);
            setAccessibilityInformationByTitleAndAction(actionRedo->asJuceComponent(), "Redo",
                                                        "Redo");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::SAVE_PATCH:
        {
            auto q = layoutComponentForSkin(skinCtrl, tag_store);
            setAccessibilityInformationByTitleAndAction(q->asJuceComponent(), "Save Patch", "Save");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_OPEN:
        {
            lfoEditSwitch = layoutComponentForSkin(skinCtrl, tag_mseg_edit);
            setAccessibilityInformationByTitleAndAction(lfoEditSwitch->asJuceComponent(),
                                                        "Show MSEG Editor", "Show");

            auto q = modsource_editor[current_scene];
            auto msejc = dynamic_cast<juce::Component *>(lfoEditSwitch);

            jassert(msejc);

            msejc->setVisible(false);
            lfoEditSwitch->setValue(isAnyOverlayPresent(MSEG_EDITOR) ||
                                    isAnyOverlayPresent(FORMULA_EDITOR));

            if ((q >= ms_lfo1 && q <= ms_lfo6) || (q >= ms_slfo1 && q <= ms_slfo6))
            {
                auto *lfodata = &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);

                if (lfodata->shape.val.i == lt_mseg || lfodata->shape.val.i == lt_formula)
                {
                    msejc->setVisible(true);
                }

                if (lfodata->shape.val.i == lt_formula)
                {
                    setAccessibilityInformationByTitleAndAction(lfoEditSwitch->asJuceComponent(),
                                                                "Show Formula Editor", "Show");
                }
            }

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::LFO_MENU:
        {
            auto r = layoutComponentForSkin(skinCtrl, tag_lfo_menu);
            setAccessibilityInformationByTitleAndAction(r->asJuceComponent(), "LFO Menu", "Open");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::LFO_LABEL:
        {
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, lfoNameLabel);
            addAndMakeVisibleWithTracking(frame.get(), *lfoNameLabel);

            lfoNameLabel->setBounds(skinCtrl->getRect());
            lfoNameLabel->setFont(currentSkin->fontManager->getLatoAtSize(9, juce::Font::bold));
            lfoNameLabel->setFontColour(currentSkin->getColor(Colors::LFO::Title::Text));

            break;
        }

        case Surge::Skin::Connector::NonParameterConnection::FXPRESET_LABEL:
        {
            // Room for improvement, obviously
            if (!fxPresetLabel)
            {
                fxPresetLabel = std::make_unique<juce::Label>("FX Preset Name");
            }

            fxPresetLabel->setColour(juce::Label::textColourId,
                                     currentSkin->getColor(Colors::Effect::Preset::Name));
            fxPresetLabel->setFont(currentSkin->fontManager->displayFont);
            fxPresetLabel->setJustificationType(juce::Justification::centredRight);

            fxPresetLabel->setText(fxPresetName[current_fx], juce::dontSendNotification);
            fxPresetLabel->setBounds(skinCtrl->getRect());
            setAccessibilityInformationByTitleAndAction(fxPresetLabel.get(), "FX Preset", "Show");

            addAndMakeVisibleWithTracking(frame->getControlGroupLayer(cg_FX), *fxPresetLabel);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::PATCH_BROWSER:
        {
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, patchSelector);

            patchSelector->addListener(this);
            patchSelector->setStorage(&(this->synth->storage));
            patchSelector->setTag(tag_patchname);
            patchSelector->setSkin(currentSkin, bitmapStore);
            patchSelector->setLabel(synth->storage.getPatch().name);
            patchSelector->setIsFavorite(isPatchFavorite());
            patchSelector->setIsUser(isPatchUser());
            patchSelector->setCategory(synth->storage.getPatch().category);
            patchSelector->setIDs(synth->current_category_id, synth->patchid);
            patchSelector->setAuthor(synth->storage.getPatch().author);
            patchSelector->setComment(synth->storage.getPatch().comment);
            patchSelector->setTags(synth->storage.getPatch().tags);
            patchSelector->setBounds(skinCtrl->getRect());

            setAccessibilityInformationByTitleAndAction(patchSelector.get(), "Patch Selector",
                                                        "Browse");

            addAndMakeVisibleWithTracking(frame.get(), *patchSelector);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::FX_SELECTOR:
        {
            // FIXOWN
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, effectChooser);

            effectChooser->addListener(this);
            effectChooser->setStorage(&(synth->storage));
            effectChooser->setBounds(skinCtrl->getRect());
            effectChooser->setTag(tag_fx_select);
            effectChooser->setSkin(currentSkin, bitmapStore);
            effectChooser->setBackgroundDrawable(bitmapStore->getImage(IDB_FX_GRID));
            effectChooser->setCurrentEffect(current_fx);

            for (int fxi = 0; fxi < n_fx_slots; fxi++)
            {
                effectChooser->setEffectType(fxi, synth->storage.getPatch().fx[fxi].type.val.i);
            }

            effectChooser->setBypass(synth->storage.getPatch().fx_bypass.val.i);
            effectChooser->setDeactivatedBitmask(synth->storage.getPatch().fx_disable.val.i);

            addAndMakeVisibleWithTracking(frame->getControlGroupLayer(cg_FX), *effectChooser);

            setAccessibilityInformationByTitleAndAction(effectChooser->asJuceComponent(),
                                                        "FX Slots", "Select");

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::MAIN_VU_METER:
        {
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, vu[0]);

            vu[0]->setBounds(skinCtrl->getRect());
            vu[0]->setSkin(currentSkin, bitmapStore);
            vu[0]->setTag(tag_main_vu_meter);
            vu[0]->addListener(this);
            vu[0]->setType(Surge::ParamConfig::vut_vu_stereo);
            vu[0]->setStorage(&(synth->storage));

            addAndMakeVisibleWithTracking(frame.get(), *vu[0]);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED:
        case Surge::Skin::Connector::NonParameterConnection::SAVE_PATCH_DIALOG:
        case Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::FORMULA_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::WT_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::TUNING_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::MOD_LIST_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::FILTER_ANALYSIS_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::OSCILLOSCOPE_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::WAVESHAPER_ANALYSIS_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED:
            break;
        }
    }

    memset(param, 0, n_paramslots * sizeof(void *));
    memset(nonmod_param, 0, n_paramslots * sizeof(void *));

    int i = 0;
    vector<Parameter *>::iterator iter;

    for (iter = synth->storage.getPatch().param_ptr.begin();
         iter != synth->storage.getPatch().param_ptr.end(); iter++)
    {
        if (i == n_paramslots)
        {
            // This would only happen if we added new parameters
            synth->storage.reportError(
                "INTERNAL ERROR: List of parameters is larger than maximum number of parameter "
                "slots. Increase n_paramslots in SurgeGUIEditor.h!",
                "Error");
        }

        Parameter *p = *iter;
        bool paramIsVisible = ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
                              isControlVisible(p->ctrlgroup, p->ctrlgroup_entry) &&
                              (p->ctrltype != ct_none) && !isAHiddenSendOrReturn(p);

        auto conn = Surge::Skin::Connector::connectorByID(p->ui_identifier);
        std::string uiid = p->ui_identifier;

        long style = p->ctrlstyle;

        if (p->ctrltype == ct_fmratio)
        {
            p->val_default.f = (p->extend_range || p->absolute) ? 16.f : 1.f;
        }

        if (p->hasSkinConnector &&
            conn.payload->defaultComponent != Surge::Skin::Components::None && paramIsVisible)
        {
            // Some special cases where we don't add a control
            bool addControl = true;

            // Analog filter/amp envelope mode has no segment shape controls
            if (p->ctrltype == ct_envshape || p->ctrltype == ct_envshape_attack)
            {
                addControl = (synth->storage.getPatch()
                                  .scene[current_scene]
                                  .adsr[p->ctrlgroup_entry]
                                  .mode.val.i == emt_digital);
            }

            if (addControl)
            {
                auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);

                currentSkin->resolveBaseParentOffsets(skinCtrl);
                auto comp = layoutComponentForSkin(skinCtrl, p->id + start_paramtags, i, p,
                                                   style | conn.payload->controlStyleFlags);

                if (strcmp(p->ui_identifier, "filter.cutoff_1") == 0 && comp)
                {
                    filterCutoffSlider[0] = param[p->id];
                }
                if (strcmp(p->ui_identifier, "filter.cutoff_2") == 0 && comp)
                {
                    filterCutoffSlider[1] = param[p->id];
                }
                if (strcmp(p->ui_identifier, "filter.resonance_1") == 0 && comp)
                {
                    filterResonanceSlider[0] = param[p->id];
                }
                if (strcmp(p->ui_identifier, "filter.resonance_2") == 0 && comp)
                {
                    filterResonanceSlider[1] = param[p->id];
                }

                if (strcmp(p->ui_identifier, "lfo.rate") == 0 && comp)
                {
                    lfoRateSlider = comp->asJuceComponent();
                }
                uiidToSliderLabel[p->ui_identifier] = p->get_name();

                if (p->id == synth->learn_param_from_cc || p->id == synth->learn_param_from_note)
                {
                    showMidiLearnOverlay(
                        param[p->id]->asControlValueInterface()->asJuceComponent()->getBounds());
                }
            }
        }

        i++;
    }

    // resonance link mode
    if (synth->storage.getPatch().scene[current_scene].f2_link_resonance.val.b)
    {
        i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;

        if (param[i])
        {
            param[i]->setDeactivated(true);
        }
    }
    else
    {
        i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;

        if (param[i])
        {
            param[i]->setDeactivated(false);
        }
    }

    // feedback control
    if (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i ==
        fc_serial1)
    {
        i = synth->storage.getPatch().scene[current_scene].feedback.id;
        bool curr = param[i]->getDeactivated();
        param[i]->setDeactivated(true);

        if (!curr)
        {
            param[i]->asJuceComponent()->repaint();
        }
    }

    // width control
    if ((synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
         fc_stereo) &&
        (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i != fc_wide))
    {
        i = synth->storage.getPatch().scene[current_scene].width.id;

        if (param[i])
        {
            bool curr = param[i]->getDeactivated();

            param[i]->setDeactivated(true);

            if (!curr)
            {
                param[i]->asJuceComponent()->repaint();
            }
        }
    }

    // Make sure the infowindow typein
    paramInfowindow->setVisible(false);
    patchSelectorComment->setVisible(false);

    addComponentWithTracking(frame.get(), *paramInfowindow);
    addComponentWithTracking(frame.get(), *patchSelectorComment);

    // Mouse behavior
    if (Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
        Surge::Widgets::ModulatableSlider::MoveRateState::kUnInitialized)
    {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            (Surge::Widgets::ModulatableSlider::MoveRateState)Surge::Storage::getUserDefaultValue(
                &(synth->storage), Surge::Storage::SliderMoveRateState,
                (int)Surge::Widgets::ModulatableSlider::kLegacy);
    }

    if (Surge::Widgets::ModulatableSlider::touchscreenMode ==
        Surge::Widgets::ModulatableSlider::TouchscreenMode::kUnassigned)
    {
        bool touchMode = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                             Surge::Storage::TouchMouseMode, false);
        Surge::Widgets::ModulatableSlider::touchscreenMode =
            (touchMode == false) ? Surge::Widgets::ModulatableSlider::TouchscreenMode::kDisabled
                                 : Surge::Widgets::ModulatableSlider::TouchscreenMode::kEnabled;
    }

    // skin labels
    auto labels = currentSkin->getLabels();

    for (auto &l : labels)
    {
        auto mtext = currentSkin->propertyValue(l, Surge::Skin::Component::TEXT);
        auto ctext = currentSkin->propertyValue(l, Surge::Skin::Component::CONTROL_TEXT);

        if (ctext.has_value() && uiidToSliderLabel.find(*ctext) != uiidToSliderLabel.end())
        {
            mtext = ctext;
        }

        if (mtext.has_value())
        {
            auto txtalign = Surge::GUI::Skin::setJuceTextAlignProperty(
                currentSkin->propertyValue(l, Surge::Skin::Component::TEXT_ALIGN, "left"));

            auto fs = currentSkin->propertyValue(l, Surge::Skin::Component::FONT_SIZE, "12");
            auto fsize = std::atof(fs.c_str());

            auto fstyle = Surge::GUI::Skin::setFontStyleProperty(
                currentSkin->propertyValue(l, Surge::Skin::Component::FONT_STYLE, "normal"));

            auto coln =
                currentSkin->propertyValue(l, Surge::Skin::Component::TEXT_COLOR, "#FF0000");
            auto col = currentSkin->getColor(coln, juce::Colours::red);

            auto dcol = juce::Colour(255, 255, 255).withAlpha((uint8_t)0);
            auto bgcoln = currentSkin->propertyValue(l, Surge::Skin::Component::BACKGROUND_COLOR,
                                                     "#FFFFFF00");
            auto bgcol = currentSkin->getColor(bgcoln, dcol);

            auto frcoln =
                currentSkin->propertyValue(l, Surge::Skin::Component::FRAME_COLOR, "#FFFFFF00");
            auto frcol = currentSkin->getColor(frcoln, dcol);

            auto lb = componentForSkinSession<juce::Label>(l->sessionid);

            lb->setColour(juce::Label::textColourId, col);
            lb->setColour(juce::Label::backgroundColourId, bgcol);
            lb->setBounds(l->getRect());
            lb->setText(*mtext, juce::dontSendNotification);

            addAndMakeVisibleWithTracking(frame.get(), *lb);

            juceSkinComponents[l->sessionid] = std::move(lb);
        }
        else
        {
            auto image = currentSkin->propertyValue(l, Surge::Skin::Component::IMAGE);

            if (image.has_value())
            {
                auto bmp = bitmapStore->getImageByStringID(*image);

                if (bmp)
                {
                    auto r = l->getRect();
                    auto db = bmp->getDrawableButUseWithCaution();

                    if (db)
                    {
                        db->setBounds(r);
                        addAndMakeVisibleWithTracking(frame.get(), *db);
                    }
                }
            }
        }
    }

    for (const auto &el : juceOverlays)
    {
        if (el.second->getPrimaryChildAsOverlayComponent())
        {
            el.second->getPrimaryChildAsOverlayComponent()->forceDataRefresh();
        }

        if (!el.second->isTornOut())
        {
            addAndMakeVisibleWithTracking(frame.get(), *(el.second));
        }

        updateOverlayContentIfPresent(el.first);
    }

    if (showMSEGEditorOnNextIdleOrOpen)
    {
        showOverlay(SurgeGUIEditor::MSEG_EDITOR);
        showMSEGEditorOnNextIdleOrOpen = false;
    }

    // We need this here in case we rebuild when opening a new patch.
    // closeMSEGEditor() does nothing if the MSEG editor isn't open
    auto lfoidx = modsource_editor[current_scene] - ms_lfo1;

    if (lfoidx >= 0 && lfoidx <= n_lfos)
    {
        auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[lfoidx]);

        if (ld->shape.val.i != lt_mseg)
        {
            auto olc = getOverlayWrapperIfOpen(MSEG_EDITOR);

            if (olc && !olc->isTornOut())
            {
                closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
            }
        }

        if (ld->shape.val.i != lt_formula)
        {
            auto olc = getOverlayWrapperIfOpen(FORMULA_EDITOR);

            if (olc && !olc->isTornOut())
            {
                closeOverlay(SurgeGUIEditor::FORMULA_EDITOR);
            }
        }
    }

    // if the Tuning Editor was open and ODDSound was activated (which causes a refresh), close it
    if (synth->storage.oddsound_mts_active_as_client)
    {
        closeOverlay(TUNING_EDITOR);
    }

    // Finally make sure the Z-order fronting for our overlays is still OK
    std::vector<juce::Component *> frontthese;
    std::vector<juce::Component *> thenFrontThese;

    for (auto c : frame->getChildren())
    {
        if (auto ol = dynamic_cast<Surge::Overlays::OverlayWrapper *>(c))
        {
            if (ol->getIsModal())
            {
                thenFrontThese.push_back(c);
            }
            else
            {
                frontthese.push_back(c);
            }
        }
    }

    for (auto c : frontthese)
    {
        c->toFront(true);
    }

    for (auto c : thenFrontThese)
    {
        c->toFront(true);
    }

    tuningChanged(); // a patch load could change tuning
    refresh_mod();

    removeUnusedTrackedComponents();

    editor_open = true;
    queue_refresh = false;

    if (!somethingHasFocus && patchSelector && patchSelector->isShowing())
    {
        patchSelector->grabKeyboardFocus();
    }

    sendStructureChangeIn = 120;

    frame->repaint();
}

void SurgeGUIEditor::close_editor()
{
    editor_open = false;

    resetComponentTracking();
    memset(param, 0, sizeof(param));
}

bool SurgeGUIEditor::open(void *parent)
{
    int platformType = 0;
    float fzf = getZoomFactor() / 100.0;

    frame = std::make_unique<Surge::Widgets::MainFrame>();
    frame->setBounds(0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY());
    frame->setSurgeGUIEditor(this);

    // Comment this in to always start with focus debugger
    // debugFocus = true;
    // y-frame->debugFocus = true;

    juceEditor->topLevelContainer->addAndMakeVisible(*frame);
    juceEditor->addKeyListener(this);

    // TODO: SET UP JUCE EDITOR BETTER!
    bitmapStore.reset(new SurgeImageStore());
    bitmapStore->setupBuiltinBitmaps();

    if (!currentSkin->reloadSkin(bitmapStore))
    {
        auto db = Surge::GUI::SkinDB::get();

        std::ostringstream oss;
        oss << "Unable to load current skin! Reverting the skin to Surge XT Classic.\n\nSkin "
               "Error:\n"
            << db->getAndResetErrorString();

        auto msg = std::string(oss.str());
        this->currentSkin = db->defaultSkin(&(this->synth->storage));
        this->currentSkin->reloadSkin(this->bitmapStore);

        synth->storage.reportError(msg, "Skin Loading Error");
    }

    reloadFromSkin();
    openOrRecreateEditor();

    if (getZoomFactor() != 100)
    {
        zoom_callback(this, true);
        zoomInvalid = true;
    }

    auto *des = &(synth->storage.getPatch().dawExtraState);

    if (des->isPopulated && des->editor.isMSEGOpen)
    {
        showOverlay(SurgeGUIEditor::MSEG_EDITOR);
    }

    return true;
}

void SurgeGUIEditor::close()
{
    populateDawExtraState(synth);
    firstIdleCountdown = 0;
}

void SurgeGUIEditor::setParameter(long index, float value)
{
    if (!frame)
    {
        return;
    }

    if (!editor_open)
    {
        return;
    }

    if (index > synth->storage.getPatch().param_ptr.size())
    {
        return;
    }

    {
        int j = 0;

        while (j < 7)
        {
            if ((synth->refresh_ctrl_queue[j] > -1) && (synth->refresh_ctrl_queue[j] != index))
                j++;
            else
                break;
        }

        synth->refresh_ctrl_queue[j] = index;
        synth->refresh_ctrl_queue_value[j] = value;
    }
}

void SurgeGUIEditor::addHelpHeaderTo(const std::string &lab, const std::string &hu,
                                     juce::PopupMenu &m) const
{
    auto tc = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(lab, hu);

    tc->setSkin(currentSkin, bitmapStore);
    auto ht = tc->getTitle();
    m.addCustomItem(-1, std::move(tc), nullptr, ht);
}

void SurgeGUIEditor::effectSettingsBackgroundClick(int whichScene, Surge::Widgets::EffectChooser *c)
{
    auto fxGridMenu = juce::PopupMenu();

    auto msurl = SurgeGUIEditor::helpURLForSpecial("fx-selector");
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    addHelpHeaderTo("FX Unit Selector", hurl, fxGridMenu);

    fxGridMenu.addSeparator();

    std::string sc = std::string("Scene ") + (char)('A' + whichScene);

    bool isChecked =
        (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::BYPASS_HARDCLIP);

    fxGridMenu.addItem(sc + Surge::GUI::toOSCase(" Hard Clip Disabled"), true, isChecked,
                       [this, isChecked, whichScene]() {
                           synth->storage.sceneHardclipMode[whichScene] =
                               SurgeStorage::BYPASS_HARDCLIP;
                           if (!isChecked)
                               synth->storage.getPatch().isDirty = true;
                       });

    isChecked = (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::HARDCLIP_TO_0DBFS);

    fxGridMenu.addItem(sc + Surge::GUI::toOSCase(" Hard Clip at 0 dBFS"), true, isChecked,
                       [this, isChecked, whichScene]() {
                           synth->storage.sceneHardclipMode[whichScene] =
                               SurgeStorage::HARDCLIP_TO_0DBFS;
                           if (!isChecked)
                               synth->storage.getPatch().isDirty = true;
                       });

    isChecked = (synth->storage.sceneHardclipMode[whichScene] == SurgeStorage::HARDCLIP_TO_18DBFS);

    fxGridMenu.addItem(sc + Surge::GUI::toOSCase(" Hard Clip at +18 dBFS"), true, isChecked,
                       [this, isChecked, whichScene]() {
                           synth->storage.sceneHardclipMode[whichScene] =
                               SurgeStorage::HARDCLIP_TO_18DBFS;
                           if (!isChecked)
                               synth->storage.getPatch().isDirty = true;
                       });

    fxGridMenu.showMenuAsync(popupMenuOptions(c), Surge::GUI::makeEndHoverCallback(c));
}

void SurgeGUIEditor::controlBeginEdit(Surge::GUI::IComponentTagValue *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;

    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        bool isModEdit = mod_editor;
        if (isModEdit)
        {
            auto *pp = synth->storage.getPatch().param_ptr[ptag];
            isModEdit = isModEdit && synth->isValidModulation(pp->id, modsource);
        }
        if (isModEdit)
        {
            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
            if (mci)
            {
                undoManager()->pushModulationChange(
                    ptag, synth->storage.getPatch().param_ptr[ptag], modsource, current_scene,
                    modsource_index, mci->modValue,
                    synth->isModulationMuted(ptag, modsource, current_scene, modsource_index));

                for (auto l : synth->modListeners)
                {
                    l->modBeginEdit(ptag, modsource, current_scene, modsource_index, mci->modValue);
                }
            }
        }
        else
        {
            undoManager()->pushParameterChange(ptag, synth->storage.getPatch().param_ptr[ptag],
                                               synth->storage.getPatch().param_ptr[ptag]->val);
        }
        juceEditor->beginParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
    else if (tag_mod_source0 + int(ms_ctrl1) <= tag &&
             tag_mod_source0 + int(ms_ctrl1) + int(n_customcontrollers) > tag)
    {
        juceEditor->beginMacroEdit(tag - tag_mod_source0 - ms_ctrl1);
    }
    else
    {
        // jassert(false);
    }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlEndEdit(Surge::GUI::IComponentTagValue *control)
{
    long tag = control->getTag();
    int ptag = tag - start_paramtags;

    if (ptag >= 0 && ptag < synth->storage.getPatch().param_ptr.size())
    {
        bool isModEdit = mod_editor;
        if (isModEdit)
        {
            auto *pp = synth->storage.getPatch().param_ptr[ptag];
            isModEdit = isModEdit && synth->isValidModulation(pp->id, modsource);
        }
        if (isModEdit)
        {
            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
            if (mci)
            {
                for (auto l : synth->modListeners)
                {
                    l->modEndEdit(ptag, modsource, current_scene, modsource_index, mci->modValue);
                }
            }
        }
        else
        {
            juceEditor->endParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
        }
    }
    else if (tag_mod_source0 + int(ms_ctrl1) <= tag &&
             tag_mod_source0 + int(ms_ctrl1) + int(n_customcontrollers) > tag)
    {
        juceEditor->endMacroEdit(tag - tag_mod_source0 - ms_ctrl1);
    }
    else
    {
        // jassert(false);
    }
}

const std::unique_ptr<Surge::GUI::UndoManager> &SurgeGUIEditor::undoManager()
{
    return juceEditor->processor.undoManager;
}

void SurgeGUIEditor::setParamFromUndo(int paramId, pdata val)
{
    auto p = synth->storage.getPatch().param_ptr[paramId];
    auto id = synth->idForParameter(p);
    juceEditor->beginParameterEdit(p);
    switch (p->valtype)
    {
    case vt_float:
    {
        auto fval = p->value_to_normalized(val.f);
        synth->setParameter01(id, fval);
        break;
    }
    case vt_int:
        synth->setParameter01(id, (val.i - p->val_min.i) * 1.f / (p->val_max.i - p->val_min.i));
        break;
    case vt_bool:
        synth->setParameter01(id, val.b ? 1.f : 0.f);
        break;
    }
    synth->sendParameterAutomation(id, synth->getParameter01(id));
    juceEditor->endParameterEdit(p);

    for (const auto &[olTag, ol] : juceOverlays)
    {
        auto olpc = ol->getPrimaryChildAsOverlayComponent();
        if (olpc && olpc->shouldRepaintOnParamChange(getPatch(), p))
        {
            olpc->repaint();
        }
    }

    ensureParameterItemIsFocused(p);
    synth->refresh_editor = true;
}

void SurgeGUIEditor::setTuningFromUndo(const Tunings::Tuning &t)
{
    try
    {
        synth->storage.retuneAndRemapToScaleAndMapping(t.scale, t.keyboardMapping);
        synth->refresh_editor = true;
    }
    catch (Tunings::TuningError &e)
    {
        synth->storage.retuneTo12TETScaleC261Mapping();
        synth->storage.reportError(e.what(), "SCL Error");
    }
    tuningChanged();
}
const Tunings::Tuning &SurgeGUIEditor::getTuningForRedo() { return synth->storage.currentTuning; }
const fs::path SurgeGUIEditor::pathForCurrentPatch()
{
    // TODO: rewrite how patch prev-next works so that it doesn't depend on patchSelector widget
    if (patchSelector && patchSelector->sel_id >= 0 &&
        patchSelector->sel_id < synth->storage.patch_list.size())
    {
        return synth->storage.patch_list[patchSelector->sel_id].path;
    }
    return {};
}

void SurgeGUIEditor::setPatchFromUndo(void *data, size_t datasz)
{
    synth->enqueuePatchForLoad(data, datasz);
    synth->processAudioThreadOpsWhenAudioEngineUnavailable();
    synth->refresh_editor = true;
}

void SurgeGUIEditor::ensureParameterItemIsFocused(Parameter *p)
{
    if (p->scene > 0 && p->scene - 1 != current_scene)
    {
        changeSelectedScene(p->scene - 1);
    }
    if (p->ctrlgroup == cg_FX && p->ctrlgroup_entry != current_fx)
    {
        current_fx = p->ctrlgroup_entry;
        activateFromCurrentFx();
    }
    if (p->ctrlgroup == cg_LFO && p->ctrlgroup_entry != modsource_editor[p->scene - 1])
    {
        modsource = (modsources)(p->ctrlgroup_entry);
        modsource_editor[p->scene - 1] = modsource;
        refresh_mod();
    }
    if (p->ctrlgroup == cg_OSC && p->ctrlgroup_entry != current_osc[p->scene - 1])
    {
        current_osc[p->scene - 1] = p->ctrlgroup_entry;
    }
}
void SurgeGUIEditor::setStepSequencerFromUndo(int scene, int lfoid, const StepSequencerStorage &val)
{
    if (scene != current_scene || lfoid != modsource - ms_lfo1)
    {
        changeSelectedScene(scene);
        modsource = (modsources)(lfoid + ms_lfo1);
        modsource_index = 0;
        modsource_editor[scene] = modsource;
        refresh_mod();
    }
    synth->storage.getPatch().stepsequences[scene][lfoid] = val;
    synth->refresh_editor = true;
}

void SurgeGUIEditor::setMSEGFromUndo(int scene, int lfoid, const MSEGStorage &val)
{
    if (scene != current_scene || lfoid != modsource - ms_lfo1)
    {
        changeSelectedScene(scene);
        modsource = (modsources)(lfoid + ms_lfo1);
        modsource_index = 0;
        modsource_editor[scene] = modsource;
        refresh_mod();
    }
    synth->storage.getPatch().msegs[scene][lfoid] = val;
    synth->refresh_editor = true;
    if (auto ol = getOverlayIfOpenAs<Surge::Overlays::MSEGEditor>(MSEG_EDITOR))
    {
        ol->forceRefresh();
    }
}

void SurgeGUIEditor::setFormulaFromUndo(int scene, int lfoid, const FormulaModulatorStorage &val)
{
    if (scene != current_scene || lfoid != modsource - ms_lfo1)
    {
        changeSelectedScene(scene);
        modsource = (modsources)(lfoid + ms_lfo1);
        modsource_index = 0;
        modsource_editor[scene] = modsource;
        refresh_mod();
    }
    synth->storage.getPatch().formulamods[scene][lfoid] = val;
    synth->refresh_editor = true;
    if (auto ol = getOverlayIfOpenAs<Surge::Overlays::FormulaModulatorEditor>(FORMULA_EDITOR))
    {
        ol->forceRefresh();
    }
}

void SurgeGUIEditor::setLFONameFromUndo(int scene, int lfoid, int index, const std::string &n)
{
    if (scene != current_scene)
    {
        changeSelectedScene(scene);
    }
    strxcpy(synth->storage.getPatch().LFOBankLabel[scene][lfoid][index], n.c_str(),
            CUSTOM_CONTROLLER_LABEL_SIZE - 1);
    synth->refresh_editor = true;
}
void SurgeGUIEditor::setMacroNameFromUndo(int ccid, const std::string &n)
{
    strxcpy(synth->storage.getPatch().CustomControllerLabel[ccid], n.c_str(),
            CUSTOM_CONTROLLER_LABEL_SIZE - 1);
    synth->refresh_editor = true;
}

void SurgeGUIEditor::setMacroValueFromUndo(int ccid, float val)
{
    ((ControllerModulationSource *)synth->storage.getPatch()
         .scene[current_scene]
         .modsources[ccid + ms_ctrl1])
        ->set_target01(val, false);
    synth->getParent()->surgeMacroUpdated(ccid, val);
    synth->refresh_editor = true;
}

void SurgeGUIEditor::pushParamToUndoRedo(int paramId, Surge::GUI::UndoManager::Target which)
{
    auto p = synth->storage.getPatch().param_ptr[paramId];
    undoManager()->pushParameterChange(paramId, p, p->val, which);
}

void SurgeGUIEditor::applyToParamForUndo(int paramId, std::function<void(Parameter *)> f)
{
    auto p = synth->storage.getPatch().param_ptr[paramId];
    f(p);
    synth->refresh_editor = true;
}

void SurgeGUIEditor::setModulationFromUndo(int paramId, modsources ms, int scene, int idx,
                                           float val, bool muted)
{
    auto p = synth->storage.getPatch().param_ptr[paramId];
    // FIXME scene and index
    synth->setModDepth01(p->id, ms, scene, idx, val);
    synth->muteModulation(p->id, ms, scene, idx, muted);
    ensureParameterItemIsFocused(p);
    modsource = ms;
    modsource_index = idx;
    mod_editor = true;
    // TODO: arm the modulator also
    synth->refresh_editor = true;
}

void SurgeGUIEditor::pushModulationToUndoRedo(int paramId, modsources ms, int scene, int idx,
                                              Surge::GUI::UndoManager::Target which)
{
    undoManager()->pushModulationChange(
        paramId, synth->storage.getPatch().param_ptr[paramId], ms, scene, idx,
        synth->getModDepth01(paramId, ms, scene, idx),
        synth->isModulationMuted(paramId, modsource, current_scene, modsource_index), which);
}
//------------------------------------------------------------------------------------------------

long SurgeGUIEditor::applyParameterOffset(long id) { return id - start_paramtags; }

long SurgeGUIEditor::unapplyParameterOffset(long id) { return id + start_paramtags; }

// Status Panel Callbacks
void SurgeGUIEditor::toggleMPE()
{
    this->synth->mpeEnabled = !this->synth->mpeEnabled;

    if (statusMPE)
    {
        this->synth->resetPitchBend(-1);

        statusMPE->setValue(this->synth->mpeEnabled ? 1 : 0);
        statusMPE->asJuceComponent()->repaint();
    }

    synth->refresh_editor = true;
}

juce::PopupMenu::Options SurgeGUIEditor::popupMenuOptions(const juce::Point<int> &where)
{
    auto o = juce::PopupMenu::Options();
    o = o.withTargetComponent(juceEditor);
    if (where.x > 0 && where.y > 0)
    {
        auto r = juce::Rectangle<int>().withWidth(1).withHeight(1).withPosition(
            frame->localPointToGlobal(where));
        o = o.withTargetScreenArea(r);
    }
    return o;
}

juce::PopupMenu::Options SurgeGUIEditor::popupMenuOptions(const juce::Component *c,
                                                          bool useComponentBounds)
{
    juce::Point<int> where;

    if (!c || !useComponentBounds)
    {
        where = frame->getLocalPoint(nullptr, juce::Desktop::getMousePosition());
    }
    else
    {
        where = c->getBounds().getBottomLeft();
    }

    if (c && Surge::GUI::isTouchMode(&(synth->storage)))
        where = c->getBounds().getBottomLeft();

    return popupMenuOptions(where);
}

void SurgeGUIEditor::toggleTuning()
{
    synth->storage.toggleTuningToCache();

    if (statusTune)
    {
        bool hasmts =
            synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active_as_client;
        statusTune->setValue(this->synth->storage.isStandardTuning ? hasmts : 1);
    }

    this->synth->refresh_editor = true;
}

void SurgeGUIEditor::scaleFileDropped(const string &fn)
{
    undoManager()->pushTuning(synth->storage.currentTuning);
    synth->storage.loadTuningFromSCL(fn);
    tuningChanged();
}

void SurgeGUIEditor::mappingFileDropped(const string &fn)
{
    undoManager()->pushTuning(synth->storage.currentTuning);
    synth->storage.loadMappingFromKBM(fn);
    tuningChanged();
}

void SurgeGUIEditor::tuningChanged()
{
    auto tc = dynamic_cast<Surge::Overlays::TuningOverlay *>(getOverlayIfOpen(TUNING_EDITOR));
    if (tc)
    {
        tc->setTuning(synth->storage.currentTuning);
        tc->repaint();
    }
}

bool SurgeGUIEditor::doesZoomFitToScreen(float zf, float &correctedZf)
{
#if !LINUX
    correctedZf = zf;
    return true;
#endif

    auto screenDim = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;

    float baseW = getWindowSizeX();
    float baseH = getWindowSizeY();

    /*
    ** Window decoration takes up some of the screen so don't zoom to full screen dimensions.
    ** This heuristic seems to work on Windows 10 and macOS 10.14 well enough.
    ** Keep these as integers to be consistent with the other zoom factors, and to make
    ** the error message cleaner.
    */
    int maxScreenUsage = 90;

    /*
    ** In the startup path we may not have a clean window yet to give us a trustworthy
    ** screen dimension; so allow callers to suppress this check with an optional
    ** variable and set it only in the constructor of SurgeGUIEditor
    */
    if (zf != 100.0 && zf > 100 && screenDim.getHeight() > 0 && screenDim.getWidth() > 0 &&
        ((baseW * zf / 100.0) > maxScreenUsage * screenDim.getWidth() / 100.0 ||
         (baseH * zf / 100.0) > maxScreenUsage * screenDim.getHeight() / 100.0))
    {
        correctedZf = findLargestFittingZoomBetween(100.0, zf, 5, maxScreenUsage, baseW, baseH);
        return false;
    }
    else
    {
        correctedZf = zf;
        return true;
    }
}

void SurgeGUIEditor::moveTopLeftTo(float tx, float ty)
{
    /* This pushes every item so that it co-alighns with frame at tx, ty */
    if (frame && frame->isVisible())
    {
        auto b = frame->getBounds();
        auto bx = b.getX();
        auto by = b.getY();
        auto dx = tx - bx;
        auto dy = ty - by;

        for (auto c : juceEditor->getChildren())
        {
            auto q = c->getBounds();
            auto nx = q.getX() + dx;
            auto ny = q.getY() + dy;
            c->setTopLeftPosition(nx, ny);
        }
    }
}

void SurgeGUIEditor::resizeWindow(float zf) { setZoomFactor(zf, true); }

/*
   Called when compiling the lua code from formula editor.
   It is called before the draw call.
*/

void SurgeGUIEditor::forceLfoDisplayRepaint()
{
    if (lfoDisplay)
        lfoDisplay->forceRepaint = true;
}

void SurgeGUIEditor::setZoomFactor(float zf) { setZoomFactor(zf, false); }

void SurgeGUIEditor::setZoomFactor(float zf, bool resizeWindow)
{

    zoomFactor = std::max(zf, static_cast<float>(minimumZoom));

#if LINUX
    if (zoomFactor == 150)
        zoomFactor = 149;
#endif

    float zff = zoomFactor * 0.01;

    if (currentSkin && resizeWindow)
    {
        int yExtra = 0;

        if (getShowVirtualKeyboard())
        {
            yExtra = SurgeSynthEditor::extraYSpaceForVirtualKeyboard;
        }

        juceEditor->setSize(zff * currentSkin->getWindowSizeX(),
                            zff * (currentSkin->getWindowSizeY() + yExtra));
    }

    if (frame)
    {
        frame->setTransform(juce::AffineTransform().scaled(zff));
    }

    if (oscWaveform)
        oscWaveform->setZoomFactor(zoomFactor);

    if (lfoDisplay)
        lfoDisplay->setZoomFactor(zoomFactor);

    setBitmapZoomFactor(zoomFactor);
    rezoomOverlays();
}

void SurgeGUIEditor::setBitmapZoomFactor(float zf)
{
    if (juce::Desktop::getInstance().isHeadless() == false)
    {
        float dbs = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
        int fullPhysicalZoomFactor = (int)(zf * dbs);

        if (bitmapStore != nullptr)
        {
            bitmapStore->setPhysicalZoomFactor(fullPhysicalZoomFactor);
        }
    }
}

void SurgeGUIEditor::showTooLargeZoomError(double width, double height, float zf) const
{
#if !LINUX
    std::ostringstream msg;
    msg << "Surge XT adjusts the maximum zoom level in order to prevent the interface becoming "
           "larger than available screen area. Your screen resolution is "
        << width << "x" << height << " for which the target zoom level of " << zf
        << "% would be too large." << std::endl
        << std::endl;
    if (currentSkin && currentSkin->hasFixedZooms())
    {
        msg << "Surge XT chose the largest fitting fixed zoom which is provided by this skin.";
    }
    else
    {
        msg << "Surge XT chose the largest fitting zoom level of " << zf << "%.";
    }
    synth->storage.reportError(msg.str(), "Zoom Level Adjusted");
#endif
}

void SurgeGUIEditor::makeScopeEntry(juce::PopupMenu &menu)
{
    bool showOscilloscope = isAnyOverlayPresent(OSCILLOSCOPE);

    Surge::GUI::addMenuWithShortcut(menu, Surge::GUI::toOSCase("Oscilloscope..."),
                                    showShortcutDescription("Alt + O", u8"\U00002325O"), true,
                                    showOscilloscope, [this]() { toggleOverlay(OSCILLOSCOPE); });
}

void SurgeGUIEditor::setRecommendedAccessibility()
{
    std::ostringstream oss;
    oss << "Set Accessibility Options: ";

    Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                           Surge::Storage::UseKeyboardShortcuts_Plugin, true);
    Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                           Surge::Storage::UseKeyboardShortcuts_Standalone, true);
    oss << "Keyboard shortcuts on; ";

    Surge::Storage::updateUserDefaultValue(
        &(this->synth->storage), Surge::Storage::MenuAndEditKeybindingsFollowKeyboardFocus, true);
    oss << "Menu Follows Keyboard; ";

    Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                           Surge::Storage::UseNarratorAnnouncements, true);
    Surge::Storage::updateUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead, true);
    oss << "Narrator announcements on; ";

    Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                           Surge::Storage::ExpandModMenusWithSubMenus, true);
    Surge::Storage::updateUserDefaultValue(
        &(this->synth->storage), Surge::Storage::FocusModEditorAfterAddModulationFrom, true);
    oss << "Expanded Modulation Menus and Modulation Focus.";

    enqueueAccessibleAnnouncement(oss.str());
}

bool SurgeGUIEditor::getShowVirtualKeyboard()
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;

    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::ShowVirtualKeyboard_Standalone;
    }

    return Surge::Storage::getUserDefaultValue(&(this->synth->storage), key, false);
}

void SurgeGUIEditor::setShowVirtualKeyboard(bool b)
{
    auto key = Surge::Storage::ShowVirtualKeyboard_Plugin;

    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::ShowVirtualKeyboard_Standalone;
    }

    Surge::Storage::updateUserDefaultValue(&(this->synth->storage), key, b);
}

void SurgeGUIEditor::toggleVirtualKeyboard()
{
    auto mc =
        Surge::Storage::getUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 1);

    juceEditor->keyboard->setOctaveForMiddleC(5 - mc);

    setShowVirtualKeyboard(!getShowVirtualKeyboard());
    resizeWindow(zoomFactor);
}

bool SurgeGUIEditor::getUseKeyboardShortcuts()
{
    auto key = Surge::Storage::UseKeyboardShortcuts_Plugin;
    bool defaultVal = false;

    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::UseKeyboardShortcuts_Standalone;
        defaultVal = true;
    }

    return Surge::Storage::getUserDefaultValue(&(this->synth->storage), key, defaultVal);
}

void SurgeGUIEditor::setUseKeyboardShortcuts(bool b)
{
    auto key = Surge::Storage::UseKeyboardShortcuts_Plugin;

    if (juceEditor->processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        key = Surge::Storage::UseKeyboardShortcuts_Standalone;
    }

    Surge::Storage::updateUserDefaultValue(&(this->synth->storage), key, b);

    // If binding changes means VKB and Accessible keys conflict we
    // need to remask so just reset the layout to current on toggle.
    juceEditor->setVKBLayout(juceEditor->currentVKBLayout);
}

void SurgeGUIEditor::toggleUseKeyboardShortcuts()
{
    setUseKeyboardShortcuts(!getUseKeyboardShortcuts());
}

void SurgeGUIEditor::reloadFromSkin()
{
    if (!frame || !bitmapStore.get())
    {
        return;
    }

    juceEditor->getSurgeLookAndFeel()->setSkin(currentSkin, bitmapStore);

    float dbs = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;

    bitmapStore->setPhysicalZoomFactor(getZoomFactor() * dbs);
    paramInfowindow->setSkin(currentSkin, bitmapStore);
    patchSelectorComment->setSkin(currentSkin, bitmapStore);

    auto bg = currentSkin->customBackgroundImage();

    if (bg != "")
    {
        auto *cbm = bitmapStore->getImageByStringID(bg);
        frame->setBackground(cbm);
    }
    else
    {
        auto *cbm = bitmapStore->getImage(IDB_MAIN_BG);
        frame->setBackground(cbm);
    }

    wsx = currentSkin->getWindowSizeX();
    wsy = currentSkin->getWindowSizeY();

    float sf = 1;

    frame->setSize(wsx * sf, wsy * sf);

    {
        auto rg = SurgeSynthEditor::BlockRezoom(juceEditor);
        setZoomFactor(getZoomFactor(), true);
    }

    // update overlays, if opened
    for (const auto &ov : {MSEG_EDITOR, FORMULA_EDITOR})
    {
        if (isAnyOverlayPresent(ov))
        {
            bool tornOut = false;
            juce::Point<int> tearOutPos;
            auto olw = getOverlayWrapperIfOpen(ov);

            if (olw && olw->isTornOut())
            {
                tornOut = true;
                tearOutPos = olw->currentTearOutLocation();
            }

            showOverlay(ov);

            if (tornOut)
            {
                auto olw = getOverlayWrapperIfOpen(ov);

                if (olw)
                {
                    olw->doTearOut(tearOutPos);
                }
            }
        }
    }

    for (const auto &ol : juceOverlays)
    {
        auto component = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(ol.second.get());

        if (component)
        {
            component->setSkin(currentSkin, bitmapStore);
            ol.second->repaint();
        }
    }

    synth->refresh_editor = true;
    scanJuceSkinComponents = true;
    juceEditor->reapplySurgeComponentColours();
    juceEditor->repaint();
}

int SurgeGUIEditor::findLargestFittingZoomBetween(
    int zoomLow,                     // bottom of range
    int zoomHigh,                    // top of range
    int zoomQuanta,                  // step size
    int percentageOfScreenAvailable, // How much to shrink actual screen
    float baseW, float baseH)
{
    // Here is a very crude implementation
    if (juce::Desktop::getInstance().isHeadless() == true)
        return 1;

    int result = zoomHigh;
    auto screenDim = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->totalArea;
    float sx = screenDim.getWidth() * percentageOfScreenAvailable / 100.0;
    float sy = screenDim.getHeight() * percentageOfScreenAvailable / 100.0;

    while (result > zoomLow)
    {
        if (result * baseW / 100.0 <= sx && result * baseH / 100.0 <= sy)
            break;
        result -= zoomQuanta;
    }
    if (result < zoomLow)
        result = zoomLow;

    return result;
}

void SurgeGUIEditor::broadcastPluginAutomationChangeFor(Parameter *p)
{
    juceEditor->beginParameterEdit(p);
    repushAutomationFor(p);
    juceEditor->endParameterEdit(p);
}
//------------------------------------------------------------------------------------------------

bool SurgeGUIEditor::promptForUserValueEntry(Surge::Widgets::ModulatableControlInterface *mci)
{
    auto t = mci->asControlValueInterface()->getTag() - start_paramtags;
    if (t < 0 || t >= n_total_params)
        return false;

    auto p = synth->storage.getPatch().param_ptr[t];

    if (p->valtype == vt_float)
    {
        if (mod_editor && synth->isValidModulation(p->id, modsource))
        {
            promptForUserValueEntry(p, mci->asJuceComponent(), modsource, current_scene,
                                    modsource_index);
        }
        else
        {
            promptForUserValueEntry(p, mci->asJuceComponent());
        }

        return true;
    }
    return false;
}

void SurgeGUIEditor::promptForUserValueEntry(Parameter *p, juce::Component *c, int ms, int modScene,
                                             int modidx)
{
    if (typeinParamEditor->isVisible())
    {
        typeinParamEditor->setReturnFocusTarget(nullptr);
        typeinParamEditor->setVisible(false);
    }

    typeinParamEditor->setSkin(currentSkin, bitmapStore);

    bool ismod = p && ms > 0;
    std::string lab = "";

    jassert(c);

    if (p)
    {
        if (!p->can_setvalue_from_string())
        {
            synth->storage.reportError(
                "This parameter does not support editing its value by text input.\n\n"
                "Please report this to Surge Synth Team in order to fix the problem!",
                "Error");
            return;
        }

        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Param);
    }
    else
    {
        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Control);
    }

    if (p)
    {
        if (p->ctrlgroup == cg_LFO)
        {
            char pname[1024];

            p->create_fullname(p->get_name(), pname, p->ctrlgroup, p->ctrlgroup_entry,
                               ModulatorName::modulatorName(&synth->storage, p->ctrlgroup_entry,
                                                            true, current_scene)
                                   .c_str());
            lab = pname;
        }
        else
        {
            lab = p->get_full_name();
        }
    }
    else
    {
        lab = ModulatorName::modulatorName(&synth->storage, ms, false, current_scene);
    }

    typeinParamEditor->setMainLabel(lab);

    char txt[TXT_SIZE];
    std::string ptxt1, ptxt2, ptxt3;

    if (p)
    {
        if (ismod)
        {
            std::string txt2;

            p->get_display_of_modulation_depth(
                txt, synth->getModDepth(p->id, (modsources)ms, modScene, modidx),
                synth->isBipolarModulation((modsources)ms), Parameter::TypeIn);
            txt2 = p->get_display();

            ptxt1 = fmt::format("current: {:s}", txt2);
            ptxt2 = fmt::format("mod: {:s}", txt);
        }
        else
        {
            p->get_display(txt);
            ptxt1 = fmt::format("current: {:s}", txt);
        }
    }
    else
    {
        const bool detailedMode = Surge::Storage::getValueDispPrecision(&(this->synth->storage));
        auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                        .scene[current_scene]
                        .modsources[ms]);

        ptxt3 = fmt::format("{:.{}f} %", 100.0 * cms->get_output(0), (detailedMode ? 6 : 2));
        strncpy(txt, ptxt3.c_str(), TXT_SIZE - 1);
        ptxt1 = fmt::format("current: {:s}", txt);
    }

    typeinParamEditor->setValueLabels(ptxt1, ptxt2);
    typeinParamEditor->setEditableText(txt);

    if (ismod)
    {
        std::string mls = std::string("by ") +
                          ModulatorName::modulatorNameWithIndex(&synth->storage, current_scene, ms,
                                                                modidx, true, false);
        typeinParamEditor->setModByLabel(mls);
    }

    typeinParamEditor->setEditedParam(p);
    typeinParamEditor->setModDepth01(p && ms > 0, (modsources)ms, modScene, modidx);

    addAndMakeVisibleWithTracking(frame.get(), *typeinParamEditor);

    auto cBounds = c->getBounds();
    auto cParent = cBounds.getTopLeft();
    auto q = c->getParentComponent();

    while (q && q != frame.get())
    {
        auto qc = q->getBounds().getTopLeft();
        cParent = cParent + qc;
        q = q->getParentComponent();
    }

    cBounds = cBounds.withTop(cParent.getY()).withLeft(cParent.getX());
    typeinParamEditor->setBoundsToAccompany(cBounds, frame->getBounds());
    typeinParamEditor->setVisible(true);
    typeinParamEditor->toFront(true);
    typeinParamEditor->setReturnFocusTarget(c);
    typeinParamEditor->grabFocus();
}

bool SurgeGUIEditor::promptForUserValueEntry(uint32_t tag, juce::Component *c)
{
    auto t = tag - start_paramtags;

    if (t < 0 || t >= n_total_params)
    {
        return false;
    }

    auto p = synth->storage.getPatch().param_ptr[t];

    promptForUserValueEntry(p, c);
    return true;
}

std::string SurgeGUIEditor::helpURLFor(Parameter *p)
{
    auto storage = &(synth->storage);
#if 0 // useful debug
   static bool once = false;
   if( ! once )
   {
      once = true;
      for( auto hp : storage->helpURL_paramidentifier )
      {
         auto k = hp.first;
         bool found = false;
         for (auto iter = synth->storage.getPatch().param_ptr.begin();
              iter != synth->storage.getPatch().param_ptr.end(); iter++)
         {
            Parameter* q = *iter;
            if( ! q ) break;
            if( q->ui_identifier == k )
            {
               found = true;
               break;
            }
         }
         if( ! found )
            std::cout << "UNFOUND : " << k << std::endl;
      }
   }
#endif
    std::string id = p->ui_identifier;
    int type = -1;
    if (p->ctrlgroup == cg_OSC)
    {
        type = storage->getPatch().scene[current_scene].osc[current_osc[current_scene]].type.val.i;
    }
    if (p->ctrlgroup == cg_FX)
    {
        type = storage->getPatch().fx[current_fx].type.val.i;
    }
    if (type >= 0)
    {
        auto key = std::make_pair(id, type);
        if (storage->helpURL_paramidentifier_typespecialized.find(key) !=
            storage->helpURL_paramidentifier_typespecialized.end())
        {
            auto r = storage->helpURL_paramidentifier_typespecialized[key];
            if (r != "")
                return r;
        }
    }
    if (storage->helpURL_paramidentifier.find(id) != storage->helpURL_paramidentifier.end())
    {
        auto r = storage->helpURL_paramidentifier[id];
        if (r != "")
            return r;
    }
    if (storage->helpURL_controlgroup.find(p->ctrlgroup) != storage->helpURL_controlgroup.end())
    {
        auto r = storage->helpURL_controlgroup[p->ctrlgroup];
        if (r != "")
            return r;
    }
    return "";
}

std::string SurgeGUIEditor::helpURLForSpecial(const string &special)
{
    auto storage = &(synth->storage);
    return helpURLForSpecial(storage, special);
}

std::string SurgeGUIEditor::helpURLForSpecial(SurgeStorage *storage, const std::string &special)
{
    if (storage->helpURL_specials.find(special) != storage->helpURL_specials.end())
    {
        auto r = storage->helpURL_specials[special];
        if (r != "")
            return r;
    }
    return "";
}
std::string SurgeGUIEditor::fullyResolvedHelpURL(const string &helpurl)
{
    std::string lurl = helpurl;
    if (helpurl[0] == '#')
    {
        lurl = stringManual + helpurl;
    }
    return lurl;
}

void SurgeGUIEditor::setupSkinFromEntry(const Surge::GUI::SkinDB::Entry &entry)
{
    auto *db = Surge::GUI::SkinDB::get();
    auto s = db->getSkin(entry);
    this->currentSkin = s;
    this->bitmapStore.reset(new SurgeImageStore());
    this->bitmapStore->setupBuiltinBitmaps();
    if (!this->currentSkin->reloadSkin(this->bitmapStore))
    {
        std::ostringstream oss;
        oss << "Unable to load " << entry.root << entry.name
            << " skin! Reverting the skin to Surge XT Classic.\n\nSkin Error:\n"
            << db->getAndResetErrorString();

        auto msg = std::string(oss.str());
        this->currentSkin = db->defaultSkin(&(this->synth->storage));
        this->currentSkin->reloadSkin(this->bitmapStore);
        synth->storage.reportError(msg, "Skin Loading Error");
    }
    reloadFromSkin();
}

void SurgeGUIEditor::sliderHoverStart(int tag)
{
    int ptag = tag - start_paramtags;
    for (int k = 1; k < n_modsources; k++)
    {
        modsources ms = (modsources)k;
        if (synth->isActiveModulation(ptag, ms, current_scene, modsource_index))
        {
            if (gui_modsrc[k])
            {
                gui_modsrc[k]->setSecondaryHover(true);
            }
        }
    };
}
void SurgeGUIEditor::sliderHoverEnd(int tag)
{
    for (int k = 1; k < n_modsources; k++)
    {
        if (gui_modsrc[k])
        {
            gui_modsrc[k]->setSecondaryHover(false);
        }
    }
}

std::string SurgeGUIEditor::getDisplayForTag(long tag, bool external, float f)
{
    if (tag < start_paramtags)
    {
        std::string res = "";
        switch (tag)
        {
        case tag_mp_category:
        case tag_mp_patch:
        case tag_mp_jogwaveshape:
        case tag_mp_jogfx:
            res = (f < 0.5) ? "Down" : "Up";
            break;
        case tag_scene_select:
            res = (f < 0.5) ? "Scene A" : "Scene B";
            break;
        case tag_osc_select:
            res = (f < 0.3333) ? "Osc 1" : ((f < 0.6666) ? "Osc 2" : "Osc 3");
            break;
        default:
            res = "Non-param tag " + std::to_string(tag) + "=" + std::to_string(f);
        }
        return res;
    }

    int ptag = tag - start_paramtags;
    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];

        if (p->ctrltype == ct_scenemode)
        {
            // TODO FIXME - break streaming order for XT2!
            // Ahh that decision in 1.6 to not change streaming order continues to be painful
            auto iv = Parameter::intUnscaledFromFloat(f, p->val_max.i, p->val_min.i);

            if (iv == 3)
                iv = 2;
            else if (iv == 2)
                iv = 3;

            f = Parameter::intScaledToFloat(iv, p->val_max.i, p->val_min.i);
        }

        if (p)
        {
            return p->get_display(external, f);
        }
    }

    return "Unknown";
}

float SurgeGUIEditor::getF01FromString(long tag, const std::string &s)
{
    if (tag < start_paramtags)
        return 0.f;

    int ptag = tag - start_paramtags;
    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];
        if (p)
        {
            pdata pd;
            std::string errMsg;
            p->set_value_from_string_onto(s, pd, errMsg);
            return p->value_to_normalized(pd.f);
        }
    }

    return 0.f;
}

float SurgeGUIEditor::getModulationF01FromString(long tag, const std::string &s)
{
    if (tag < start_paramtags)
        return 0.f;

    int ptag = tag - start_paramtags;
    if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
    {
        Parameter *p = synth->storage.getPatch().param_ptr[ptag];
        if (p)
        {
            bool valid = false;
            std::string errMsg;
            float mv = p->calculate_modulation_value_from_string(s, errMsg, valid);
            if (valid)
                return mv;
        }
    }

    return 0.f;
}

void SurgeGUIEditor::promptForMiniEdit(const std::string &value, const std::string &prompt,
                                       const std::string &title, const juce::Point<int> &iwhere,
                                       std::function<void(const std::string &)> onOK,
                                       juce::Component *returnFocusTo)
{
    miniEdit->setSkin(currentSkin, bitmapStore);
    miniEdit->setEditor(this);
    miniEdit->setDescription(title);
    miniEdit->setWindowTitle(title);

    addComponentWithTracking(frame.get(), *miniEdit);
    miniEdit->setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    miniEdit->setLabel(prompt);
    miniEdit->setValue(value);
    miniEdit->callback = std::move(onOK);
    miniEdit->setBounds(0, 0, getWindowSizeX(), getWindowSizeY());
    miniEdit->setVisible(true);
    miniEdit->toFront(true);
    miniEdit->setFocusReturnTarget(returnFocusTo);
    miniEdit->grabFocus();
}

void SurgeGUIEditor::alertBox(const std::string &title, const std::string &prompt,
                              std::function<void()> onOk, std::function<void()> onCancel,
                              AlertButtonStyle buttonStyle)
{
    alert = std::make_unique<Surge::Overlays::Alert>();
    alert->setSkin(currentSkin, bitmapStore);
    alert->setDescription(title);
    alert->setWindowTitle(title);

    addAndMakeVisibleWithTracking(frame.get(), *alert);

    alert->setLabel(prompt);

    switch (buttonStyle)
    {
    case AlertButtonStyle::OK_CANCEL:
    {
        alert->setOkCancelButtonTexts("OK", "Cancel");
        break;
    }
    case AlertButtonStyle::YES_NO:
    {
        alert->setOkCancelButtonTexts("Yes", "No");
        break;
    }
    case AlertButtonStyle::OK:
    {
        alert->setSingleButtonText("OK");
        break;
    }
    }

    alert->onOk = std::move(onOk);
    alert->onCancel = std::move(onCancel);
    alert->setBounds(0, 0, getWindowSizeX(), getWindowSizeY());
    alert->setVisible(true);
    alert->toFront(true);
}

bool SurgeGUIEditor::modSourceButtonDraggedOver(Surge::Widgets::ModulationSourceButton *msb,
                                                const juce::Point<int> &pt)
{
    juce::Component *target = nullptr;

    auto msrc = msb->getCurrentModSource();
    auto isDroppable = [this, msrc](juce::Component *c) {
        auto tMCI = dynamic_cast<Surge::Widgets::ModulatableSlider *>(c);
        if (tMCI)
        {
            auto ptag = tMCI->getTag() - start_paramtags;

            if (this->synth->isValidModulation(ptag, msrc))
            {
                return true;
            }
        }

        return false;
    };

    auto recC = [isDroppable, msb, pt](juce::Component *p, auto rec) -> juce::Component * {
        for (auto kid : p->getChildren())
        {
            if (kid && kid->isVisible() && kid != msb && kid->getBounds().contains(pt))
            {
                if (isDroppable(kid))
                {
                    return kid;
                }

                auto q = rec(kid, rec);

                if (q)
                {
                    return q;
                }
            }
        }

        return nullptr;
    };

    target = recC(frame.get(), recC);

    auto tMCI = dynamic_cast<Surge::Widgets::ModulatableSlider *>(target);

    // detect if our mouse cursor is on top of an overlay, skip DnD hover over slider if so
    if (isPointWithinOverlay(pt))
    {
        if (modSourceDragOverTarget)
        {
            modSourceDragOverTarget->setModulationState(priorModulationState);
            modSourceDragOverTarget->asJuceComponent()->repaint();
        }

        modSourceDragOverTarget = nullptr;

        return false;
    }

    if (tMCI != modSourceDragOverTarget)
    {
        if (modSourceDragOverTarget)
        {
            modSourceDragOverTarget->setModulationState(priorModulationState);
            modSourceDragOverTarget->asJuceComponent()->repaint();
        }

        modSourceDragOverTarget = tMCI;

        if (tMCI)
        {
            priorModulationState = tMCI->modulationState;
            tMCI->setModulationState(
                Surge::Widgets::ModulatableControlInterface::MODULATED_BY_ACTIVE);
            tMCI->asJuceComponent()->repaint();
        }
    }

    return tMCI != nullptr;
}

void SurgeGUIEditor::modSourceButtonDroppedAt(Surge::Widgets::ModulationSourceButton *msb,
                                              const juce::Point<int> &pt)
{
    // We need to do this search vs componentAt because componentAt will return self since I am
    // there being dropped
    juce::Component *target = nullptr;

    auto isDroppable = [msb](juce::Component *c) {
        auto tMSB = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(c);
        auto tMCI = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(c);

        if (tMSB && msb->isMeta && tMSB && tMSB->isMeta)
        {
            return true;
        }

        if (tMCI)
        {
            return true;
        }

        return false;
    };

    auto recC = [isDroppable, msb, pt](juce::Component *p, auto rec) -> juce::Component * {
        for (auto kid : p->getChildren())
        {
            if (kid && kid->isVisible() && kid != msb && kid->getBounds().contains(pt))
            {
                if (isDroppable(kid))
                {
                    return kid;
                }

                auto q = rec(kid, rec);

                if (q)
                {
                    return q;
                }
            }
        }

        return nullptr;
    };

    target = recC(frame.get(), recC);

    if (!target)
    {
        return;
    }

    auto tMSB = dynamic_cast<Surge::Widgets::ModulationSourceButton *>(target);
    auto tMCI = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(target);

    if (msb->isMeta && tMSB && tMSB->isMeta)
    {
        swapControllers(msb->getTag(), tMSB->getTag());
    }
    else if (tMCI)
    {
        // detect if our mouse cursor is on top of an overlay, skip DnD mod assign if so
        if (isPointWithinOverlay(pt))
        {
            if (modSourceDragOverTarget)
            {
                tMCI->setModulationState(priorModulationState);
                tMCI->asJuceComponent()->repaint();

                modSourceDragOverTarget = nullptr;
            }

            return;
        }

        if (modSourceDragOverTarget)
        {
            tMCI->setModulationState(priorModulationState);
            tMCI->asJuceComponent()->repaint();

            modSourceDragOverTarget = nullptr;
        }

        openModTypeinOnDrop(msb->getCurrentModSource(), tMCI,
                            tMCI->asControlValueInterface()->getTag(), msb->getCurrentModIndex());
    }
}

void SurgeGUIEditor::swapControllers(int t1, int t2)
{
    undoManager()->pushPatch();
    synth->storage.getPatch().isDirty = true;
    synth->swapMetaControllers(t1 - tag_mod_source0 - ms_ctrl1, t2 - tag_mod_source0 - ms_ctrl1);
}

void SurgeGUIEditor::openModTypeinOnDrop(modsources ms,
                                         Surge::Widgets::ModulatableControlInterface *sl,
                                         int slidertag, int modidx)
{
    auto p = synth->storage.getPatch().param_ptr[slidertag - start_paramtags];

    if (synth->isValidModulation(p->id, ms))
        promptForUserValueEntry(p, sl->asControlValueInterface()->asJuceComponent(), (int)ms,
                                current_scene, modidx);
}

void SurgeGUIEditor::openMacroRenameDialog(const int ccid, const juce::Point<int> where,
                                           Surge::Widgets::ModulationSourceButton *msb)
{
    std::string pval = synth->storage.getPatch().CustomControllerLabel[ccid];

    if (pval == "-")
    {
        pval = "";
    }

    promptForMiniEdit(
        pval, fmt::format("Enter a new name for Macro {:d}:", ccid + 1), "Rename Macro", where,
        [this, ccid, msb](const std::string &s) {
            auto useS = s;

            if (useS == "")
            {
                useS = "-";
            }

            undoManager()->pushMacroRename(ccid,
                                           synth->storage.getPatch().CustomControllerLabel[ccid]);
            strxcpy(synth->storage.getPatch().CustomControllerLabel[ccid], useS.c_str(),
                    CUSTOM_CONTROLLER_LABEL_SIZE - 1);
            synth->storage.getPatch()
                .CustomControllerLabel[ccid][CUSTOM_CONTROLLER_LABEL_SIZE - 1] = 0; // to be sure
            parameterNameUpdated = true;

            if (msb)
            {
                msb->setCurrentModLabel(synth->storage.getPatch().CustomControllerLabel[ccid]);

                if (msb->asJuceComponent())
                {
                    msb->asJuceComponent()->repaint();
                }

                synth->storage.getPatch().isDirty = true;
                synth->refresh_editor = true;
            }
        },
        msb);
}

void SurgeGUIEditor::openLFORenameDialog(const int lfo_id, const juce::Point<int> where,
                                         juce::Component *c)
{
    auto msi = modsource_index;

    auto callback = [this, lfo_id, msi](const std::string &nv) {
        auto cp = synth->storage.getPatch().LFOBankLabel[current_scene][lfo_id][msi];
        undoManager()->pushLFORename(current_scene, lfo_id, msi, cp);
        strxcpy(cp, nv.c_str(), CUSTOM_CONTROLLER_LABEL_SIZE);
        synth->storage.getPatch().isDirty = true;
        synth->refresh_editor = true;
    };

    promptForMiniEdit(
        synth->storage.getPatch().LFOBankLabel[current_scene][lfo_id][msi],
        fmt::format("Enter a new name for {:s}:",
                    ModulatorName::modulatorNameWithIndex(&synth->storage, current_scene, modsource,
                                                          msi, false, false, true)),
        "Rename Modulator", juce::Point<int>(10, 10), callback, c);
}

void SurgeGUIEditor::resetSmoothing(Modulator::SmoothingMode t)
{
    // Reset the default value and tell the synth it is updated
    Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::SmoothingMode,
                                           (int)t);
    synth->changeModulatorSmoothing(t);
}

void SurgeGUIEditor::resetPitchSmoothing(Modulator::SmoothingMode t)
{
    // Reset the default value and update it in storage for newly created voices to use
    Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::PitchSmoothingMode,
                                           (int)t);
    synth->storage.pitchSmoothingMode = t;
}

Surge::GUI::IComponentTagValue *
SurgeGUIEditor::layoutComponentForSkin(std::shared_ptr<Surge::GUI::Skin::Control> skinCtrl,
                                       long tag, int paramIndex, Parameter *p, int style)
{
    // Special cases to preserve things
    if (p && p->ctrltype == ct_fmconfig)
    {
        fmconfig_tag = tag;
    }
    if (p && p->ctrltype == ct_fbconfig)
    {
        filterblock_tag = tag;
    }
    if (p && p->ctrltype == ct_fxbypass)
    {
        fxbypass_tag = tag;
    }

    assert(!p || (p->id < n_paramslots && p->id >= 0));

    // Basically put this in a function
    if (skinCtrl->defaultComponent == Surge::Skin::Components::Slider)
    {
        if (!p)
        {
            // FIXME ERROR
            return nullptr;
        }

        auto loc = juce::Point<int>(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs);

        // See the discussion in #7238 - for the FX this allows us to brin ghte group name
        // into the FX, albeit in a somewhat hacky/clumsy way because of the FX group structure
        // and general memory model errors in SGE
        std::string extendedAccessibleGroupName{};
        if (p->ctrlgroup == cg_FX)
        {
            auto p0 = &synth->storage.getPatch().fx[p->ctrlgroup_entry].p[0];
            auto df = p - p0;
            if (df >= 0 && df < n_fx_params)
            {
                auto &fxi = synth->fx[p->ctrlgroup_entry];
                if (fxi)
                {
                    auto gi = fxi->groupIndexForParamIndex(df);
                    if (gi >= 0)
                    {
                        auto gn = fxi->group_label(gi);
                        if (gn)
                        {
                            extendedAccessibleGroupName = gn;
                        }
                    }
                }
            }
        }

        if (p->is_discrete_selection())
        {
            loc = loc.translated(2, 4);
            auto hs =
                componentForSkinSession<Surge::Widgets::MenuForDiscreteParams>(skinCtrl->sessionid);

            /*auto hs = new CMenuAsSlider(loc, this, p->id + start_paramtags, bitmapStore,
                                        &(synth->storage));*/
            hs->setTag(p->id + start_paramtags);
            hs->addListener(this);
            hs->setStorage(&(synth->storage));
            hs->setBounds(loc.x, loc.y, 133, 22);
            hs->setSkin(currentSkin, bitmapStore);
            hs->setValue(p->get_value_f01());
            hs->setMinMax(p->val_min.i, p->val_max.i);
            hs->setLabel(p->get_name());
            p->ctrlstyle = p->ctrlstyle | Surge::ParamConfig::kNoPopup;
            if (p->can_deactivate())
                hs->setDeactivated(p->deactivated);

            auto *parm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
            if (parm && parm->supportsTotalIndexOrdering())
                hs->setIntOrdering(parm->totalIndexOrdering());

            auto dbls = currentSkin->standardHoverAndHoverOnForIDB(IDB_MENU_AS_SLIDER, bitmapStore);
            hs->setBackgroundDrawable(dbls[0]);
            hs->setHoverBackgroundDrawable(dbls[1]);
            hs->setDeactivatedFn([p]() { return p->appears_deactivated(); });
            hs->setExtendedAccessibleGroupName(extendedAccessibleGroupName);

            setAccessibilityInformationByParameter(hs.get(), p, "Adjust");
            param[p->id] = hs.get();
            addAndMakeVisibleWithTracking(frame->getControlGroupLayer(p->ctrlgroup), *hs);
            juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
        }
        else
        {
            p->ctrlstyle = p->ctrlstyle & ~kNoPopup;
        }

        auto hs = componentForSkinSession<Surge::Widgets::ModulatableSlider>(skinCtrl->sessionid);

        hs->setIsValidToModulate(p && synth->isValidModulation(p->id, modsource));

        int styleW = (style & Surge::ParamConfig::kHorizontal) ? 140 : 22;
        int styleH = (style & Surge::ParamConfig::kHorizontal) ? 26 : 84;
        hs->setOrientation((style & Surge::ParamConfig::kHorizontal)
                               ? Surge::ParamConfig::kHorizontal
                               : Surge::ParamConfig::kVertical);
        hs->setIsSemitone(style & kSemitone);
        hs->setIsLightStyle(style & kWhite);
        hs->setIsMiniVertical(style & kMini);
        hs->parameterType = (ctrltypes)p->ctrltype;

        hs->setBounds(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs, styleW, styleH);
        hs->setTag(tag);
        hs->addListener(this);
        hs->setStorage(&(this->synth->storage));
        hs->setSkin(currentSkin, bitmapStore, skinCtrl);
        hs->setMoveRate(p->moverate);

        hs->setExtendedAccessibleGroupName(extendedAccessibleGroupName);
        setAccessibilityInformationByParameter(hs.get(), p, "Adjust");

        if (p->can_temposync())
            hs->setTempoSync(p->temposync);
        else
            hs->setTempoSync(false);
        hs->setValue(p->get_value_f01());
        hs->setQuantitizedDisplayValue(hs->getValue());

        if (p->supportsDynamicName() && p->dynamicName)
        {
            hs->setDynamicLabel([p]() { return std::string(p->get_name()); });
        }
        else
        {
            hs->setLabel(p->get_name());
        }

        hs->setBipolarFn([p]() { return p->is_bipolar(); });
        hs->setFontStyle(Surge::GUI::Skin::setFontStyleProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_STYLE, "normal")));

        hs->setTextAlign(Surge::GUI::Skin::setJuceTextAlignProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALIGN, "right")));

        // Control is using labfont = currentSkin->fontManager->displayFont, which is
        // currently 9 pt in size
        // TODO: Pull the default font size from some central location at a later date
        hs->setFont(currentSkin->fontManager->displayFont);

        auto ff = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_FAMILY, "");
        auto fs = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_SIZE, "9").c_str());

        if (fs < 1)
        {
            fs = 9;
        }

        if (ff.size() > 0)
        {
            if (currentSkin->typeFaces.find(ff) != currentSkin->typeFaces.end())
            {
                hs->setFont(
                    juce::Font(juce::FontOptions(currentSkin->typeFaces[ff])).withPointHeight(fs));
                hs->setFontSize(fs);
            }
        }
        else
        {
            hs->setFont(currentSkin->fontManager->getLatoAtSize(fs));
        }

        hs->setTextHOffset(std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOFFSET, "0")
                .c_str()));

        hs->setTextVOffset(std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_VOFFSET, "0")
                .c_str()));

        hs->setDeactivated(false);
        hs->setDeactivatedFn([p]() { return p->appears_deactivated(); });

        if (p->valtype == vt_int || p->valtype == vt_bool)
        {
            hs->setIsStepped(true);
            hs->setIntStepRange(p->val_max.i - p->val_min.i);
        }
        else
        {
            hs->setIsStepped(false);
        }

        setDisabledForParameter(p, hs.get());

        hs->setIsEditingModulation(mod_editor);
        hs->setModulationState(
            synth->isModDestUsed(p->id),
            synth->isActiveModulation(p->id, modsource, current_scene, modsource_index));
        if (synth->isValidModulation(p->id, modsource))
        {
            hs->setModValue(synth->getModDepth01(p->id, modsource, current_scene, modsource_index));
            hs->setIsModulationBipolar(synth->isBipolarModulation(modsource));
        }

        param[p->id] = hs.get();

        auto cgl = frame->getControlGroupLayer(p->ctrlgroup);

        /*
         * wsunit is its own thing and since there's only one the controlgroup
         * is global (grossly) not filter since filter has one-per-filter in the entries
         * or at least thats how it has been forever, so hack here and reorg it inxt2
         */
        if (p->id == synth->storage.getPatch().scene[0].wsunit.drive.id ||
            p->id == synth->storage.getPatch().scene[0].wsunit.type.id ||
            p->id == synth->storage.getPatch().scene[1].wsunit.drive.id ||
            p->id == synth->storage.getPatch().scene[1].wsunit.type.id)
        {
            cgl = frame->getControlGroupLayer(cg_FILTER);
        }
        addAndMakeVisibleWithTracking(cgl, *hs);
        juceSkinComponents[skinCtrl->sessionid] = std::move(hs);

        return dynamic_cast<Surge::GUI::IComponentTagValue *>(
            juceSkinComponents[skinCtrl->sessionid].get());
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::MultiSwitch)
    {
        auto rect = juce::Rectangle<int>(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);

        // Make this a function on skin
        auto drawables = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);

        if (std::get<0>(drawables))
        {
            // Special case that scene select parameter is "odd"
            if (p && p->ctrltype == ct_scenesel)
            {
                tag = tag_scene_select;
            }

            auto frames = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FRAMES, "1");
            auto rows = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::ROWS, "1");
            auto cols = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::COLUMNS, "1");
            auto frameoffset =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FRAME_OFFSET, "0");
            auto drgb =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::DRAGGABLE_SWITCH, "1");
            auto whl = currentSkin->propertyValue(
                skinCtrl, Surge::Skin::Component::MOUSEWHEELABLE_SWITCH, "1");
            auto accAsBut = currentSkin->propertyValue(
                skinCtrl, Surge::Skin::Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, "0");
            // std::cout << skinCtrl->ui_id << " accAsBut = " << accAsBut << std::endl;
            auto hsw = componentForSkinSession<Surge::Widgets::MultiSwitch>(skinCtrl->sessionid);
            hsw->setStorage(&(synth->storage));
            hsw->setIsAlwaysAccessibleMomentary(std::atoi(accAsBut.c_str()));
            hsw->setRows(std::atoi(rows.c_str()));
            hsw->setColumns(std::atoi(cols.c_str()));
            hsw->setTag(tag);
            hsw->addListener(this);
            hsw->setDraggable(std::atoi(drgb.c_str()));
            hsw->setMousewheelable(std::atoi(whl.c_str()));
            hsw->setHeightOfOneImage(skinCtrl->h);
            hsw->setFrameOffset(std::atoi(frameoffset.c_str()));

            setAccessibilityInformationByParameter(hsw.get(), p, "Select");
            hsw->setupAccessibility();

            hsw->setSwitchDrawable(std::get<0>(drawables));
            hsw->setHoverSwitchDrawable(std::get<1>(drawables));
            hsw->setHoverOnSwitchDrawable(std::get<2>(drawables));

            if (tag == tag_mp_category || tag == tag_mp_patch)
            {
                hsw->setMiddleClickable(true);
            }

            auto bg = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::IMAGE);

            if (bg.has_value())
            {
                auto hdb = bitmapStore->getImageByStringID(*bg);
                hsw->setSwitchDrawable(hdb);
            }

            auto ho = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::HOVER_IMAGE);

            if (ho.has_value())
            {
                auto hdb = bitmapStore->getImageByStringID(*ho);
                hsw->setHoverSwitchDrawable(hdb);
            }

            auto hoo = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::HOVER_ON_IMAGE);

            if (hoo.has_value())
            {
                auto hdb = bitmapStore->getImageByStringID(*hoo);
                hsw->setHoverOnSwitchDrawable(hdb);
            }

            hsw->setBounds(rect);
            hsw->setSkin(currentSkin, bitmapStore, skinCtrl);

            if (p)
            {
                hsw->setDeactivated(p->appears_deactivated());

                auto fval = p->get_value_f01();

                if (p->ctrltype == ct_scenemode)
                {
                    /*
                    ** SceneMode is special now because we have a streaming vs UI difference.
                    ** The streamed integer value is 0, 1, 2, 3 which matches the scene_mode
                    ** SurgeStorage enum. But our display would look gross in that order, so
                    ** our display order is single, split, channel split, dual which is 0, 1,
                    *3, 2.
                    ** Fine. So just deal with that here.
                    */
                    auto guiscenemode = p->val.i;
                    if (guiscenemode == 3)
                        guiscenemode = 2;
                    else if (guiscenemode == 2)
                        guiscenemode = 3;
                    fval = Parameter::intScaledToFloat(guiscenemode, n_scene_modes - 1);
                }
                hsw->setValue(fval);
            }

            if (p)
            {
                addAndMakeVisibleWithTracking(frame->getControlGroupLayer(p->ctrlgroup), *hsw);
            }
            else
            {
                auto cg = endCG;
                bool addToGlobalControls = false;

                switch (tag)
                {
                case tag_osc_select:
                    cg = cg_OSC;
                    break;
                case tag_mp_jogwaveshape:
                    cg = cg_FILTER;
                    break;
                case tag_mp_jogfx:
                    cg = cg_FX;
                    break;
                default:
                    cg = endCG;
                    break;
                }

                if (cg != endCG)
                {
                    addAndMakeVisibleWithTracking(frame->getControlGroupLayer(cg), *hsw);
                }
                else if (addToGlobalControls)
                {
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                }
                else
                {
                    // Really just the main menu
                    addAndMakeVisibleWithTracking(frame.get(), *hsw);
                }
            }

            if (hsw->isShowing() && hsw->hasKeyboardFocus(true))
            {
                hsw->updateAccessibleStateOnUserValueChange();
            }

            juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

            if (paramIndex >= 0)
            {
                nonmod_param[paramIndex] = dynamic_cast<Surge::GUI::IComponentTagValue *>(
                    juceSkinComponents[skinCtrl->sessionid].get());
            }

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
        }
        else
        {
            std::cout << "Can't get a MultiSwitch background" << std::endl;
        }
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::Switch)
    {
        auto rect = juce::Rectangle<int>(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);
        auto drawables = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);

        if (std::get<0>(drawables))
        {
            auto hsw = componentForSkinSession<Surge::Widgets::Switch>(skinCtrl->sessionid);
            hsw->setStorage(&(synth->storage));
            auto accAsBut = currentSkin->propertyValue(
                skinCtrl, Surge::Skin::Component::ACCESSIBLE_AS_MOMENTARY_BUTTON, "0");
            hsw->setIsAlwaysAccessibleMomentary(std::atoi(accAsBut.c_str()));

            if (p)
            {
                addAndMakeVisibleWithTrackingInCG(p->ctrlgroup, *hsw);
            }
            else
            {
                switch (tag)
                {
                case tag_status_mpe:
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                    mpeStatus = hsw.get();
                    break;
                case tag_status_zoom:
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                    zoomStatus = hsw.get();
                    break;
                case tag_status_tune:
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                    tuneStatus = hsw.get();
                    break;
                case tag_action_undo:
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                    undoButton = hsw.get();
                    break;
                case tag_action_redo:
                    addAndMakeVisibleWithTracking(frame->getSynthControlsLayer(), *hsw);
                    redoButton = hsw.get();
                    break;
                case tag_mseg_edit:
                    addAndMakeVisibleWithTrackingInCG(cg_LFO, *hsw);
                    break;
                case tag_lfo_menu:
                    addAndMakeVisibleWithTrackingInCG(cg_LFO, *hsw);
                    lfoMenuButton = hsw.get();
                    break;
                case tag_analyzewaveshape:
                case tag_analyzefilters:
                    addAndMakeVisibleWithTrackingInCG(cg_FILTER, *hsw);
                    break;

                default:
                    std::cout << "Unable to figure out home for tag = " << tag << std::endl;
                    jassert(false);
                    addAndMakeVisibleWithTracking(frame.get(), *hsw);
                    break;
                }
            }

            hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
            hsw->setBounds(rect);
            hsw->setTag(tag);
            hsw->addListener(this);

            hsw->setSwitchDrawable(std::get<0>(drawables));
            hsw->setHoverSwitchDrawable(std::get<1>(drawables));

            setAccessibilityInformationByParameter(hsw.get(), p, "Toggle");

            if (paramIndex >= 0)
                nonmod_param[paramIndex] = hsw.get();

            if (p)
            {
                hsw->setValue(p->get_value_f01());
                hsw->setDeactivated(p->appears_deactivated());

                // Carry over this filter type special case from the default control path
                if (p->ctrltype == ct_filtersubtype)
                {
                    auto filttype = synth->storage.getPatch()
                                        .scene[current_scene]
                                        .filterunit[p->ctrlgroup_entry]
                                        .type.val.i;
                    auto stc = sst::filters::fut_subcount[filttype];
                    hsw->setIsMultiIntegerValued(true);
                    hsw->setIntegerMax(stc);
                    hsw->setIntegerValue(std::min(p->val.i + 1, stc));
                    if (sst::filters::fut_subcount[filttype] == 0)
                        hsw->setIntegerValue(0);

                    if (p->ctrlgroup_entry == 1)
                    {
                        f2subtypetag = p->id + start_paramtags;
                        filtersubtype[1] = hsw.get();
                    }
                    else
                    {
                        f1subtypetag = p->id + start_paramtags;
                        filtersubtype[0] = hsw.get();
                    }
                }
            }

            // frame->deferredJuceAdds.push_back(hsw.get());
            juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
        }
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::LFODisplay)
    {
        if (!p)
            return nullptr;
        if (p->ctrltype != ct_lfotype)
        {
            // FIXME - warning?
        }
        auto rect = juce::Rectangle<int>(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);

        int lfo_id = p->ctrlgroup_entry - ms_lfo1;
        if ((lfo_id >= 0) && (lfo_id < n_lfos))
        {
            if (!lfoDisplay)
                lfoDisplay = std::make_unique<Surge::Widgets::LFOAndStepDisplay>(this);
            lfoDisplay->setBounds(skinCtrl->getRect());
            lfoDisplay->setSkin(currentSkin, bitmapStore, skinCtrl);
            lfoDisplay->setTag(p->id + start_paramtags);
            lfoDisplay->setLFOStorage(&synth->storage.getPatch().scene[current_scene].lfo[lfo_id]);
            lfoDisplay->setModSource((modsources)p->ctrlgroup_entry);
            lfoDisplay->setLFOID(lfo_id);
            lfoDisplay->setScene(current_scene);

            auto msi = 0;
            if (gui_modsrc[p->ctrlgroup_entry])
                msi = gui_modsrc[p->ctrlgroup_entry]->getCurrentModIndex();
            lfoDisplay->setModIndex(msi);
            lfoDisplay->setStorage(&synth->storage);
            lfoDisplay->setStepSequencerStorage(
                &synth->storage.getPatch().stepsequences[current_scene][lfo_id]);
            lfoDisplay->setMSEGStorage(&synth->storage.getPatch().msegs[current_scene][lfo_id]);
            lfoDisplay->setFormulaStorage(
                &synth->storage.getPatch().formulamods[current_scene][lfo_id]);
            lfoDisplay->setCanEditEnvelopes(lfo_id >= 0 && lfo_id <= (ms_lfo6 - ms_lfo1));
            lfoDisplay->setupAccessibility();

            lfoDisplay->addListener(this);
            addAndMakeVisibleWithTrackingInCG(cg_LFO, *lfoDisplay);
            nonmod_param[paramIndex] = lfoDisplay.get();
            return lfoDisplay.get();
        }
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::OscMenu)
    {
        if (!oscMenu)
            oscMenu = std::make_unique<Surge::Widgets::OscillatorMenu>();
        oscMenu->setTag(tag_osc_menu);
        oscMenu->addListener(this);
        oscMenu->setStorage(&(synth->storage));
        oscMenu->setSkin(currentSkin, bitmapStore, skinCtrl);
        oscMenu->setBackgroundDrawable(bitmapStore->getImage(IDB_OSC_MENU));
        auto id = currentSkin->hoverImageIdForResource(IDB_OSC_MENU, Surge::GUI::Skin::HOVER);
        auto bhov = bitmapStore->getImageByStringID(id);
        oscMenu->setHoverBackgroundDrawable(bhov);
        oscMenu->setBounds(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);
        oscMenu->setOscillatorStorage(
            &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]]);
        oscMenu->populate();

        oscMenu->text_allcaps = Surge::GUI::Skin::setAllCapsProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALL_CAPS, "false"));
        oscMenu->font_style = Surge::GUI::Skin::setFontStyleProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_STYLE, "normal"));
        oscMenu->text_align = Surge::GUI::Skin::setJuceTextAlignProperty(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_ALIGN, "center"));
        oscMenu->font_size = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::FONT_SIZE, "8").c_str());
        oscMenu->text_hoffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOFFSET, "0")
                .c_str());
        oscMenu->text_voffset = std::atoi(
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_VOFFSET, "0")
                .c_str());
        addAndMakeVisibleWithTrackingInCG(cg_OSC, *oscMenu);
        return oscMenu.get();
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::FxMenu)
    {
        if (!fxMenu)
            fxMenu = std::make_unique<Surge::Widgets::FxMenu>();
        fxMenu->setTag(tag_fx_menu);
        fxMenu->addListener(this);
        fxMenu->setStorage(&(synth->storage));
        fxMenu->setSkin(currentSkin, bitmapStore, skinCtrl);
        fxMenu->setBackgroundDrawable(bitmapStore->getImage(IDB_MENU_AS_SLIDER));
        auto id = currentSkin->hoverImageIdForResource(IDB_MENU_AS_SLIDER, Surge::GUI::Skin::HOVER);
        auto bhov = bitmapStore->getImageByStringID(id);
        fxMenu->setHoverBackgroundDrawable(bhov);
        fxMenu->setBounds(skinCtrl->x, skinCtrl->y, skinCtrl->w, skinCtrl->h);
        fxMenu->setFxStorage(&synth->storage.getPatch().fx[current_fx]);
        fxMenu->setFxBuffer(&synth->fxsync[current_fx]);
        fxMenu->setCurrentFx(current_fx);
        fxMenu->selectedIdx = selectedFX[current_fx];
        // TODO: set the fxs fxb, cfx

        fxMenu->populateForContext(false);
        addAndMakeVisibleWithTrackingInCG(cg_FX, *fxMenu);
        return fxMenu.get();
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::NumberField)
    {
        // some are managed outside of the skin session management
        std::unique_ptr<Surge::Widgets::NumberField> pbd;
        switch (p->ctrltype)
        {
        case ct_polylimit:
            pbd = std::move(polydisp);
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, pbd);
            break;
        case ct_midikey_or_channel:
            pbd = std::move(splitpointControl);
            componentForSkinSessionOwnedByMember(skinCtrl->sessionid, pbd);
            break;
        default:
            pbd = componentForSkinSession<Surge::Widgets::NumberField>(skinCtrl->sessionid);
            break;
        }

        pbd->addListener(this);
        pbd->setSkin(currentSkin, bitmapStore, skinCtrl);
        pbd->setTag(tag);
        pbd->setStorage(&synth->storage);

        auto images = currentSkin->standardHoverAndHoverOnForControl(skinCtrl, bitmapStore);
        pbd->setBackgroundDrawable(images[0]);
        pbd->setHoverBackgroundDrawable(images[1]);

        // TODO: extra from properties
        auto nfcm =
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::NUMBERFIELD_CONTROLMODE,
                                       std::to_string(Surge::Skin::Parameters::NONE));
        pbd->setControlMode(
            (Surge::Skin::Parameters::NumberfieldControlModes)std::atoi(nfcm.c_str()),
            p->extend_range);
        pbd->setValue(p->get_value_f01());
        pbd->setBounds(skinCtrl->getRect());

        auto colorName = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_COLOR,
                                                    Colors::NumberField::Text.name);
        auto hoverColorName =
            currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::TEXT_HOVER_COLOR,
                                       Colors::NumberField::TextHover.name);
        pbd->setTextColour(currentSkin->getColor(colorName));
        pbd->setHoverTextColour(currentSkin->getColor(hoverColorName));

        if (p)
        {
            setAccessibilityInformationByParameter(pbd.get(), p, "Set");
            addAndMakeVisibleWithTrackingInCG(p->ctrlgroup, *pbd);
        }
        else
        {
            addAndMakeVisibleWithTracking(frame.get(), *pbd);
        }

        nonmod_param[paramIndex] = pbd.get();

        if (p && p->ctrltype == ct_midikey_or_channel)
        {
            auto sm = this->synth->storage.getPatch().scenemode.val.i;

            switch (sm)
            {
            case sm_single:
            case sm_dual:
                pbd->setControlMode(Surge::Skin::Parameters::NONE);
                break;
            case sm_split:
                pbd->setControlMode(Surge::Skin::Parameters::NOTENAME);
                break;
            case sm_chsplit:
                pbd->setControlMode(Surge::Skin::Parameters::MIDICHANNEL_FROM_127);
                break;
            }
        }

        // Save some of these for later reference
        switch (p->ctrltype)
        {
        case ct_polylimit:
            polydisp = std::move(pbd);
            return polydisp.get();
            break;
        case ct_midikey_or_channel:
            splitpointControl = std::move(pbd);
            return splitpointControl.get();
            break;
        default:
            juceSkinComponents[skinCtrl->sessionid] = std::move(pbd);
            return dynamic_cast<Surge::GUI::IComponentTagValue *>(
                juceSkinComponents[skinCtrl->sessionid].get());
            break;
        }
        return nullptr;
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::FilterSelector)
    {
        // Obviously exposing this widget as a controllable widget would be better
        if (!p)
            return nullptr;

        auto rect = skinCtrl->getRect();
        auto hsw =
            componentForSkinSession<Surge::Widgets::MenuForDiscreteParams>(skinCtrl->sessionid);
        hsw->addListener(this);
        hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
        hsw->setTag(p->id + start_paramtags);
        hsw->setStorage(&(synth->storage));
        hsw->setBounds(rect);
        hsw->setValue(p->get_value_f01());
        hsw->setDeactivated(p->appears_deactivated());
        p->ctrlstyle = p->ctrlstyle | kNoPopup;

        setAccessibilityInformationByParameter(hsw.get(), p, "Select");

        auto *parm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
        if (parm && parm->supportsTotalIndexOrdering())
            hsw->setIntOrdering(parm->totalIndexOrdering());

        hsw->setMinMax(0, sst::filters::num_filter_types - 1);
        hsw->setLabel(p->get_name());

        auto pv = currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::BACKGROUND);
        if (pv.has_value())
        {
            hsw->setBackgroundDrawable(bitmapStore->getImageByStringID(*pv));
            jassert(false); // hover
        }
        else
        {
            hsw->setBackgroundDrawable(bitmapStore->getImage(IDB_FILTER_MENU));
            auto id =
                currentSkin->hoverImageIdForResource(IDB_FILTER_MENU, Surge::GUI::Skin::HOVER);
            auto bhov = bitmapStore->getImageByStringID(id);
            hsw->setHoverBackgroundDrawable(bhov);
        }

        bool activeGlyph = true;
        if (currentSkin->getVersion() >= 2)
        {
            auto pval =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLPYH_ACTIVE, "true");
            if (pval == "false")
                activeGlyph = false;
        }

        hsw->setGlyphMode(true);

        if (activeGlyph)
        {
            for (int i = 0; i < sst::filters::num_filter_types; i++)
                hsw->addGlyphIndexMapEntry(fut_glyph_index[i][0], fut_glyph_index[i][1]);

            auto glpc = currentSkin->propertyValue(skinCtrl,
                                                   Surge::Skin::Component::GLYPH_PLACEMENT, "left");
            auto glw = std::atoi(
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_W, "18")
                    .c_str());
            auto glh = std::atoi(
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_H, "18")
                    .c_str());
            auto gli =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_IMAGE, "");
            auto glih =
                currentSkin->propertyValue(skinCtrl, Surge::Skin::Component::GLYPH_HOVER_IMAGE, "");

            // These are the V1 hardcoded defaults
            if (glw == 18 && glh == 18 && glpc == "left" && gli == "")
            {
                auto drr = rect.withWidth(18);

                hsw->setDragRegion(drr);
                hsw->setDragGlyph(bitmapStore->getImage(IDB_FILTER_ICONS), 18);
                hsw->setDragGlyphHover(
                    bitmapStore->getImageByStringID(currentSkin->hoverImageIdForResource(
                        IDB_FILTER_ICONS, Surge::GUI::Skin::HOVER)));
            }
            else
            {
                jassert(false);
                // hsw->setGlyphSettings(gli, glih, glpc, glw, glh);
            }
        }

        addAndMakeVisibleWithTrackingInCG(cg_FILTER, *hsw);
        nonmod_param[paramIndex] = hsw.get();

        juceSkinComponents[skinCtrl->sessionid] = std::move(hsw);

        return dynamic_cast<Surge::GUI::IComponentTagValue *>(
            juceSkinComponents[skinCtrl->sessionid].get());
    }

    if (skinCtrl->defaultComponent == Surge::Skin::Components::WaveShaperSelector)
    {
        // Obviously exposing this widget as a controllable widget would be better
        if (!p)
            return nullptr;

        /*
         * This doesn't participate in the juceSkinComponents but that's OK
         */
        auto rect = skinCtrl->getRect();

        if (!waveshaperSelector)
            waveshaperSelector = std::make_unique<Surge::Widgets::WaveShaperSelector>();

        waveshaperSelector->addListener(this);
        waveshaperSelector->setStorage(&(synth->storage));
        waveshaperSelector->setSkin(currentSkin, bitmapStore, skinCtrl);
        waveshaperSelector->setTag(p->id + start_paramtags);
        waveshaperSelector->setBounds(rect);
        waveshaperSelector->setValue(p->get_value_f01());

        waveshaperSelector->setDeactivated(p->appears_deactivated());

        auto *parm = dynamic_cast<ParameterDiscreteIndexRemapper *>(p->user_data);
        if (parm && parm->supportsTotalIndexOrdering())
            waveshaperSelector->setIntOrdering(parm->totalIndexOrdering());

        setAccessibilityInformationByParameter(waveshaperSelector.get(), p, "Select");
        addAndMakeVisibleWithTrackingInCG(cg_FILTER, *waveshaperSelector);
        nonmod_param[paramIndex] = waveshaperSelector.get();

        return dynamic_cast<Surge::GUI::IComponentTagValue *>(waveshaperSelector.get());
    }

    if (skinCtrl->ultimateparentclassname != Surge::GUI::NoneClassName)
        std::cout << "Unable to make control with UPC " << skinCtrl->ultimateparentclassname
                  << std::endl;

    return nullptr;
}

bool SurgeGUIEditor::canDropTarget(const std::string &fname)
{
    static std::unordered_set<std::string> extensions;

    if (extensions.empty())
    {
        extensions.insert(".scl");
        extensions.insert(".kbm");
        extensions.insert(".wav");
        extensions.insert(".wt");
        extensions.insert(".fxp");
        extensions.insert(".surge-skin");
        extensions.insert(".zip");
    }

    fs::path fPath(fname);
    std::string fExt(path_to_string(fPath.extension()));
    std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (extensions.find(fExt) != extensions.end())
        return true;
    return false;
}

bool SurgeGUIEditor::onDrop(const std::string &fname)
{
    fs::path fPath(fname);
    std::string fExt(path_to_string(fPath.extension()));
    std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (fExt == ".wav" || fExt == ".wt")
    {
        synth->storage.getPatch()
            .scene[current_scene]
            .osc[current_osc[current_scene]]
            .wt.queue_filename = fname;
    }
    else if (fExt == ".scl")
    {
        scaleFileDropped(fname);
    }
    else if (fExt == ".kbm")
    {
        mappingFileDropped(fname);
    }
    else if (fExt == ".fxp")
    {
        queuePatchFileLoad(fname);
    }
    else if (fExt == ".surge-skin")
    {
        std::ostringstream oss;
        oss << "Would you like to install the skin from\n"
            << fname << "\ninto Surge XT user folder?";
        auto cb = [this, fPath]() {
            auto db = Surge::GUI::SkinDB::get();
            auto me = db->installSkinFromPathToUserDirectory(&(this->synth->storage), fPath);
            if (me.has_value())
            {
                this->setupSkinFromEntry(*me);
            }
            else
            {
                std::cout << "Could not find the skin after loading!" << std::endl;
            }
        };
        alertYesNo("Install Skin", oss.str(), cb);
    }
    else if (fExt == ".zip")
    {
        std::ostringstream oss;
        std::shared_ptr<DroppedUserDataHandler> zipHandler =
            std::make_shared<DroppedUserDataHandler>();

        if (!zipHandler->init(fname))
        {
            return false;
        }

        auto entries = zipHandler->getEntries();

        if (entries.totalSize() <= 0)
        {
            std::cout << "No entries in ZIP file!" << std::endl;
            return false;
        }

        oss << "Would you like to install ";

        if (entries.fxPresets.size() > 0)
        {
            auto sz = entries.fxPresets.size();
            oss << fmt::format("{0} FX preset{1}", sz, sz != 1 ? "s" : "");
        }

        if (entries.midiMappings.size() > 0)
        {
            auto sz = entries.midiMappings.size();
            oss << fmt::format("{0} MIDI mapping{1}", sz, sz != 1 ? "s" : "");
        }

        if (entries.modulatorSettings.size() > 0)
        {
            auto sz = entries.modulatorSettings.size();
            oss << fmt::format("{0} modulator preset{1}", sz, sz != 1 ? "s" : "");
        }

        if (entries.patches.size() > 0)
        {
            auto sz = entries.patches.size();
            oss << fmt::format("{0} patch{1}", sz, sz != 1 ? "es" : "");
        }

        if (entries.skins.size() > 0)
        {
            auto sz = entries.skins.size();
            oss << fmt::format("{0} skin{1}", sz, sz != 1 ? "s" : "");
        }

        if (entries.wavetables.size() > 0)
        {
            auto sz = entries.wavetables.size();
            oss << fmt::format("{0} wavetable{1}", sz, sz != 1 ? "s" : "");
        }

        oss << " from\n" << fname << "\ninto Surge XT user folder?";

        auto cb = [this, zipHandler]() {
            auto storage = &this->synth->storage;

            if (!zipHandler->extractEntries(storage))
            {
                return;
            }

            auto entries = zipHandler->getEntries();

            if (entries.fxPresets.size() > 0)
            {
                storage->fxUserPreset->doPresetRescan(storage, true);
                this->queueRebuildUI();
            }

            if (entries.modulatorSettings.size() > 0)
            {
                storage->modulatorPreset->forcePresetRescan();
            }

            if (entries.patches.size() > 0)
            {
                storage->refresh_patchlist();
            }

            if (entries.skins.size() > 0)
            {
                auto db = Surge::GUI::SkinDB::get();
                db->rescanForSkins(storage);
            }

            if (entries.wavetables.size() > 0)
            {
                storage->refresh_wtlist();
            }
        };

        alertYesNo("Install from ZIP archive", oss.str(), cb);
    }
    return true;
}

void SurgeGUIEditor::enqueueFXChainClear(int fxchain)
{
    for (int i = 0; i < n_fx_slots; i++)
    {
        if (fxchain == -1 || (fxchain >= 0 && (i >= (fxchain * 4) && i < ((fxchain + 1) * 4))))
        {
            synth->enqueueFXOff(fxslot_order[i]);
            effectChooser->setEffectSlotDeactivation(fxslot_order[i], false);
        }
    }
}

void SurgeGUIEditor::swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m)
{
    if (source < 0 || source >= n_fx_slots || target < 0 || target >= n_fx_slots ||
        source == target)
    {
        return;
    }

    undoManager()->pushPatch();

    auto t = fxPresetName[target];

    fxPresetName[target] = fxPresetName[source];

    if (m == SurgeSynthesizer::FXReorderMode::SWAP)
    {
        fxPresetName[source] = t;
    }

    if (m == SurgeSynthesizer::FXReorderMode::MOVE ||
        (source == target && m == SurgeSynthesizer::FXReorderMode::NONE))
    {
        fxPresetName[source] = "";
    }

    synth->reorderFx(source, target, m);

    // effectChooser caches the bitmask, so reset it after a swap
    effectChooser->setDeactivatedBitmask(synth->storage.getPatch().fx_disable.val.i);
}

void SurgeGUIEditor::lfoShapeChanged(int prior, int curr)
{
    if (prior != curr)
    {
        auto lfoid = modsource - ms_lfo1;
        auto id = synth->storage.getPatch().scene[current_scene].lfo[lfoid].shape.id;
        auto pd = pdata();
        pd.i = prior;
        undoManager()->pushParameterChange(
            id, &(synth->storage.getPatch().scene[current_scene].lfo[lfoid].shape), pd);
        synth->storage.getPatch().isDirty = true;
    }

    bool needs_refresh{false};
    // Currently only formula is indexed
    if (prior == lt_formula && curr != lt_formula)
    {
        auto lfoid = modsource - ms_lfo1;
        modsource_index_cache[current_scene][lfoid] = modsource_index;
        modsource_index = 0;
        lfoDisplay->setModIndex(modsource_index);
        needs_refresh = true;
    }
    else if (curr == lt_formula && prior != lt_formula)
    {
        auto lfoid = modsource - ms_lfo1;
        modsource_index = modsource_index_cache[current_scene][lfoid];
        if (gui_modsrc[modsource])
        {
            gui_modsrc[modsource]->modlistIndex = modsource_index;
            gui_modsrc[modsource]->repaint();
        }
        lfoDisplay->setModIndex(modsource_index);
        needs_refresh = true;
    }

    if (prior != curr || prior == lt_mseg || curr == lt_mseg || prior == lt_formula ||
        curr == lt_formula)
    {
        if (lfoEditSwitch)
        {
            auto msejc = dynamic_cast<juce::Component *>(lfoEditSwitch);
            msejc->setVisible(curr == lt_mseg || curr == lt_formula);

            if (curr == lt_formula)
                setAccessibilityInformationByTitleAndAction(lfoEditSwitch->asJuceComponent(),
                                                            "Show Formula Editor", "Show");
            else
                setAccessibilityInformationByTitleAndAction(lfoEditSwitch->asJuceComponent(),
                                                            "Show MSEG Editor", "Show");
        }
    }

    bool hadExtendedEditor = false;
    bool isTornOut = false;
    juce::Point<int> tearOutPos;

    for (const auto &ov : {MSEG_EDITOR, FORMULA_EDITOR})
    {
        if (isAnyOverlayPresent(ov))
        {
            auto olw = getOverlayWrapperIfOpen(ov);

            if (olw && olw->isTornOut())
            {
                isTornOut = true;
                tearOutPos = olw->currentTearOutLocation();
            }

            closeOverlay(ov);

            hadExtendedEditor = true;
        }
    }
    if (hadExtendedEditor && (curr == lt_mseg || curr == lt_formula))
    {
        showOverlay(curr == lt_mseg ? MSEG_EDITOR : FORMULA_EDITOR);
        if (isTornOut)
        {
            auto olw = getOverlayWrapperIfOpen(curr == lt_mseg ? MSEG_EDITOR : FORMULA_EDITOR);

            if (olw)
            {
                olw->doTearOut(tearOutPos);
            }
        }
    }

    // update the LFO title label
    std::string modname = ModulatorName::modulatorName(
        &synth->storage, modsource_editor[current_scene], true, current_scene);
    lfoNameLabel->setText(modname.c_str());
    lfoNameLabel->repaint();

    setupAlternates(modsource_editor[current_scene]);
    // And now we have dynamic labels really anything
    auto modol =
        dynamic_cast<Surge::Overlays::ModulationEditor *>(getOverlayIfOpen(MODULATION_EDITOR));

    if (modol)
    {
        modol->rebuildContents();
    }

    if (needs_refresh)
    {
        refresh_mod();
    }

    frame->repaint();
}

/*
 * The edit state is independent per LFO. We want to sync some of it as if it is not
 * so this is called at the appropriate time.
 */
void SurgeGUIEditor::broadcastMSEGState()
{
    if (msegIsOpenFor >= 0 && msegIsOpenInScene >= 0)
    {
        for (int s = 0; s < n_scenes; ++s)
        {
            for (int lf = 0; lf < n_lfos; ++lf)
            {
                msegEditState[s][lf].timeEditMode =
                    msegEditState[msegIsOpenInScene][msegIsOpenFor].timeEditMode;
            }
        }
    }
    msegIsOpenFor = -1;
    msegIsOpenInScene = -1;
}

void SurgeGUIEditor::repushAutomationFor(Parameter *p)
{
    auto id = synth->idForParameter(p);

    synth->sendParameterAutomation(id, synth->getParameter01(id));
}

void SurgeGUIEditor::showAboutScreen(int devModeGrid)
{
    aboutScreen = std::make_unique<Surge::Overlays::AboutScreen>();
    aboutScreen->setEditor(this);
    aboutScreen->setHostProgram(synth->hostProgram);
    aboutScreen->setWrapperType(synth->juceWrapperType);
    aboutScreen->setStorage(&(this->synth->storage));
    aboutScreen->setSkin(currentSkin, bitmapStore);
    aboutScreen->setDevModeGrid(devModeGrid);
    aboutScreen->populateData();
    aboutScreen->setBounds(frame->getLocalBounds());

    // this is special - it can't make a rebuild so just add it normally
    frame->addAndMakeVisible(*aboutScreen);

    auto ann = std::string("Surge XT About Screen. Version ") + Surge::Build::FullVersionStr;
    enqueueAccessibleAnnouncement(ann);
}

void SurgeGUIEditor::hideAboutScreen()
{
    frame->removeChildComponent(aboutScreen.get());
    aboutScreen = nullptr;
}

void SurgeGUIEditor::showMidiLearnOverlay(const juce::Rectangle<int> &r)
{
    midiLearnOverlay = bitmapStore->getImage(IDB_MIDI_LEARN)->createCopy();
    midiLearnOverlay->setInterceptsMouseClicks(false, false);
    midiLearnOverlay->setBounds(r);
    addAndMakeVisibleWithTracking(frame.get(), *midiLearnOverlay);
}

void SurgeGUIEditor::hideMidiLearnOverlay()
{
    if (midiLearnOverlay)
    {
        frame->removeChildComponent(midiLearnOverlay.get());
    }
}

void SurgeGUIEditor::onSurgeError(const string &msg, const string &title,
                                  const SurgeStorage::ErrorType &et)
{
    std::lock_guard<std::mutex> g(errorItemsMutex);
    errorItems.emplace_back(msg, title, et);
    errorItemCount++;
}

void SurgeGUIEditor::setAccessibilityInformationByParameter(juce::Component *c, Parameter *p,
                                                            const std::string &action)
{
    if (p)
    {
        char txt[TXT_SIZE];
        synth->getParameterAccessibleName(synth->idForParameter(p), txt);
        setAccessibilityInformationByTitleAndAction(c, txt, action);
    }
}

void SurgeGUIEditor::setAccessibilityInformationByTitleAndAction(juce::Component *c,
                                                                 const std::string &titleIn,
                                                                 const std::string &action)
{
    auto currT = c->getTitle().toStdString();
    std::string title = titleIn;
    if (auto heg = dynamic_cast<Surge::Widgets::HasExtendedAccessibleGroupName *>(c))
    {
        if (!heg->extendedAccessibleGroupName.empty())
        {
            title = title + " - " + heg->extendedAccessibleGroupName;
        }
    }

#if MAC
    c->setDescription(title);
    c->setTitle(title);
#else
    c->setDescription(action);
    c->setTitle(title);
#endif

    if (currT != title)
    {
        if (auto h = c->getAccessibilityHandler())
        {
            h->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
    }
}

void SurgeGUIEditor::setupAlternates(modsources ms)
{
    jassert(gui_modsrc[ms]);
    if (!gui_modsrc[ms])
        return;

    auto e = Surge::GUI::ModulationGrid::getModulationGrid()->get(ms);
    Surge::Widgets::ModulationSourceButton::modlist_t indexedAlternates;
    std::vector<modsources> traverse;
    traverse.push_back(ms);
    for (auto a : e.alternates)
        traverse.push_back(a);

    for (auto a : traverse)
    {
        int idxc = 1;
        if (synth->supportsIndexedModulator(current_scene, a))
            idxc = synth->getMaxModulationIndex(current_scene, a);
        for (int q = 0; q < idxc; ++q)
        {
            auto tl = ModulatorName::modulatorNameWithIndex(&synth->storage, current_scene, a, q,
                                                            true, false);
            auto ll = ModulatorName::modulatorNameWithIndex(&synth->storage, current_scene, a, q,
                                                            false, false);
            indexedAlternates.emplace_back(a, q, tl, ll);
        }
    }
    gui_modsrc[ms]->setModList(indexedAlternates);
}

bool SurgeGUIEditor::isAHiddenSendOrReturn(Parameter *p)
{
    if (p->ctrlgroup != cg_GLOBAL)
        return false;

    int whichS{-1}, whichR{-1};
    for (int i = 0; i < n_send_slots; ++i)
        if (p->id == synth->storage.getPatch().scene[current_scene].send_level[i].id)
            whichS = i;

    if (whichS != -1)
    {
        int group = whichS % 2;
        if (whichS == whichSendActive[group])
            return false;
        return true;
    }
    int i = 0;
    for (auto sl : {fxslot_send1, fxslot_send2, fxslot_send3, fxslot_send4})
    {
        if (p->id == synth->storage.getPatch().fx[sl].return_level.id)
            whichR = i;
        ++i;
    }
    if (whichR != -1)
    {
        int group = whichR % 2;
        if (whichR == whichReturnActive[group])
            return false;
        return true;
    }

    return false;
}

void SurgeGUIEditor::hideTypeinParamEditor()
{
    typeinParamEditor->setReturnFocusTarget(nullptr);
    typeinParamEditor->setVisible(false);
}

void SurgeGUIEditor::activateFromCurrentFx()
{
    switch (current_fx)
    {
        // this version means we always have 1/2 or 3/4 on screen
    case fxslot_send1:
    case fxslot_send2:
        whichSendActive[0] = 0;
        whichReturnActive[0] = 0;
        whichSendActive[1] = 1;
        whichReturnActive[1] = 1;
        break;
    case fxslot_send3:
    case fxslot_send4:
        whichSendActive[0] = 2;
        whichReturnActive[0] = 2;
        whichSendActive[1] = 3;
        whichReturnActive[1] = 3;
        break;
    default:
        break;
    }
}

void SurgeGUIEditor::setupKeymapManager()
{
    if (!keyMapManager)
    {
        keyMapManager =
            std::make_unique<keymap_t>(synth->storage.userDataPath, "SurgeXT",
                                       Surge::GUI::keyboardActionName, [](auto a, auto b) {});
    }

    keyMapManager->clearBindings();

    keyMapManager->addBinding(Surge::GUI::UNDO, {keymap_t::Modifiers::COMMAND, (int)'Z'});
    keyMapManager->addBinding(Surge::GUI::REDO, {keymap_t::Modifiers::COMMAND, (int)'Y'});

    keyMapManager->addBinding(Surge::GUI::PREV_PATCH,
                              {keymap_t::Modifiers::COMMAND, juce::KeyPress::leftKey});
    keyMapManager->addBinding(Surge::GUI::NEXT_PATCH,
                              {keymap_t::Modifiers::COMMAND, juce::KeyPress::rightKey});
    keyMapManager->addBinding(Surge::GUI::PREV_CATEGORY,
                              {keymap_t::Modifiers::SHIFT, juce::KeyPress::leftKey});
    keyMapManager->addBinding(Surge::GUI::NEXT_CATEGORY,
                              {keymap_t::Modifiers::SHIFT, juce::KeyPress::rightKey});

    keyMapManager->addBinding(Surge::GUI::FAVORITE_PATCH, {keymap_t::Modifiers::ALT, (int)'F'});
    keyMapManager->addBinding(Surge::GUI::FIND_PATCH, {keymap_t::Modifiers::COMMAND, (int)'F'});
    keyMapManager->addBinding(Surge::GUI::SAVE_PATCH, {keymap_t::Modifiers::COMMAND, (int)'S'});

    // TODO: UPDATE WHEN ADDING MORE OSCILLATORS
    keyMapManager->addBinding(Surge::GUI::OSC_1, {keymap_t::Modifiers::ALT, (int)'1'});
    keyMapManager->addBinding(Surge::GUI::OSC_2, {keymap_t::Modifiers::ALT, (int)'2'});
    keyMapManager->addBinding(Surge::GUI::OSC_3, {keymap_t::Modifiers::ALT, (int)'3'});

    // TODO: FIX SCENE ASSUMPTION
    keyMapManager->addBinding(Surge::GUI::TOGGLE_SCENE, {keymap_t::Modifiers::ALT, (int)'S'});
    keyMapManager->addBinding(Surge::GUI::TOGGLE_MODULATOR_ARM,
                              {keymap_t::Modifiers::ALT, (int)'A'});

#if WINDOWS
    keyMapManager->addBinding(Surge::GUI::TOGGLE_DEBUG_CONSOLE,
                              {keymap_t::Modifiers::ALT, (int)'D'});
#endif
    keyMapManager->addBinding(Surge::GUI::SHOW_KEYBINDINGS_EDITOR,
                              {keymap_t::Modifiers::ALT, (int)'B'});
    keyMapManager->addBinding(Surge::GUI::SHOW_LFO_EDITOR, {keymap_t::Modifiers::ALT, (int)'E'});
#if INCLUDE_WT_SCRIPTING_EDITOR
    keyMapManager->addBinding(Surge::GUI::SHOW_WT_EDITOR, {keymap_t::Modifiers::ALT, (int)'W'});
#endif
    keyMapManager->addBinding(Surge::GUI::SHOW_MODLIST, {keymap_t::Modifiers::ALT, (int)'M'});
    keyMapManager->addBinding(Surge::GUI::SHOW_TUNING_EDITOR, {keymap_t::Modifiers::ALT, (int)'T'});
    keyMapManager->addBinding(Surge::GUI::TOGGLE_OSCILLOSCOPE,
                              {keymap_t::Modifiers::ALT, (int)'O'});
    keyMapManager->addBinding(Surge::GUI::TOGGLE_VIRTUAL_KEYBOARD,
                              {keymap_t::Modifiers::ALT, (int)'K'});

    keyMapManager->addBinding(Surge::GUI::VKB_OCTAVE_DOWN, {(int)'X'});
    keyMapManager->addBinding(Surge::GUI::VKB_OCTAVE_UP, {(int)'C'});
    keyMapManager->addBinding(Surge::GUI::VKB_VELOCITY_DOWN_10PCT, {(int)'9'});
    keyMapManager->addBinding(Surge::GUI::VKB_VELOCITY_UP_10PCT, {(int)'0'});

    keyMapManager->addBinding(Surge::GUI::ZOOM_TO_DEFAULT, {keymap_t::Modifiers::SHIFT, '/'});
    keyMapManager->addBinding(Surge::GUI::ZOOM_PLUS_10, {keymap_t::Modifiers::NONE, '+'});
    keyMapManager->addBinding(Surge::GUI::ZOOM_PLUS_25, {keymap_t::Modifiers::SHIFT, '+'});
    keyMapManager->addBinding(Surge::GUI::ZOOM_MINUS_10, {keymap_t::Modifiers::NONE, '-'});
    keyMapManager->addBinding(Surge::GUI::ZOOM_MINUS_25, {keymap_t::Modifiers::SHIFT, '-'});

#if 0
    if (Surge::GUI::getIsStandalone())
    {
        keyMapManager->addBinding(Surge::GUI::ZOOM_FULLSCREEN, {juce::KeyPress::F11Key});
    }
#endif

    keyMapManager->addBinding(Surge::GUI::FOCUS_NEXT_CONTROL_GROUP,
                              {keymap_t::Modifiers::ALT, (int)'.'});
    keyMapManager->addBinding(Surge::GUI::FOCUS_PRIOR_CONTROL_GROUP,
                              {keymap_t::Modifiers::ALT, (int)','});

    keyMapManager->addBinding(Surge::GUI::REFRESH_SKIN, {juce::KeyPress::F5Key});
    keyMapManager->addBinding(Surge::GUI::SKIN_LAYOUT_GRID, {keymap_t::Modifiers::ALT, (int)'L'});

    keyMapManager->addBinding(Surge::GUI::OPEN_MANUAL, {juce::KeyPress::F1Key});
    keyMapManager->addBinding(Surge::GUI::TOGGLE_ABOUT, {juce::KeyPress::F12Key});
    keyMapManager->addBinding(Surge::GUI::ANNOUNCE_STATE, {keymap_t::Modifiers::ALT, (int)'0'});

    keyMapManager->unstreamFromXML();
}

bool SurgeGUIEditor::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent)
{
    auto textChar = key.getTextCharacter();
    auto keyCode = key.getKeyCode();

    // TODO: Remove this Tab branch in XT2, leave the modulation arm action to the key manager
    // We treat Escape and Tab separately outside of the key manager
    if (textChar == juce::KeyPress::tabKey)
    {
        auto tk = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::DefaultKey::TabKeyArmsModulators, 0);

        if (!tk)
        {
            return false;
        }

        if (isAnyOverlayOpenAtAll())
        {
            return false;
        }

        toggle_mod_editing();

        return true;
    }

    if (keyCode == juce::KeyPress::escapeKey)
    {
        Surge::Overlays::OverlayWrapper *topOverlay{nullptr};

        for (auto c : frame->getChildren())
        {
            auto q = dynamic_cast<Surge::Overlays::OverlayWrapper *>(c);

            if (q)
            {
                topOverlay = q;
            }
        }

        if (topOverlay)
        {
            topOverlay->onClose();
            return true;
        }
    }

    bool triedKey = Surge::Widgets::isAccessibleKey(key);
    bool shortcutsUsed = getUseKeyboardShortcuts();
    auto mapMatch = keyMapManager->matches(key);

    bool editsFollowKeyboardFocus = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::MenuAndEditKeybindingsFollowKeyboardFocus, true);

    if (mapMatch.has_value())
    {
        triedKey = true;

        if (shortcutsUsed)
        {
            auto action = *mapMatch;

            switch (action)
            {
            case Surge::GUI::UNDO:
            {
#if INCLUDE_WT_SCRIPTING_EDITOR
                auto ol = getOverlayIfOpenAs<Surge::Overlays::WavetableScriptEditor>(WT_EDITOR);

                if (ol)
                {
                    ol->mainDocument->undo();
                }
                else
                {
                    undoManager()->undo();
                }
#else
                undoManager()->undo();
#endif
                return true;
            }
            case Surge::GUI::REDO:
            {
#if INCLUDE_WT_SCRIPTING_EDITOR
                auto ol = getOverlayIfOpenAs<Surge::Overlays::WavetableScriptEditor>(WT_EDITOR);

                if (ol)
                {
                    ol->mainDocument->redo();
                }
                else
                {
                    undoManager()->redo();
                }
#else
                undoManager()->redo();
#endif
                return true;
            }
            case Surge::GUI::SAVE_PATCH:
                showOverlay(SurgeGUIEditor::SAVE_PATCH);
                return true;
            case Surge::GUI::FIND_PATCH:
                patchSelector->isTypeaheadSearchOn = !patchSelector->isTypeaheadSearchOn;
                patchSelector->toggleTypeAheadSearch(patchSelector->isTypeaheadSearchOn);
                return true;
            case Surge::GUI::FAVORITE_PATCH:
                setPatchAsFavorite(synth->storage.getPatch().name, !isPatchFavorite());
                patchSelector->setIsFavorite(isPatchFavorite());
                return true;
            case Surge::GUI::INITIALIZE_PATCH:
                undoManager()->pushPatch();
                patchSelector->loadInitPatch();
                return true;
            case Surge::GUI::RANDOM_PATCH:
                undoManager()->pushPatch();
                synth->selectRandomPatch();
                return true;

            case Surge::GUI::PREV_PATCH:
            case Surge::GUI::NEXT_PATCH:
            {
                return promptForOKCancelWithDontAskAgain(
                    "Confirm Patch Change",
                    fmt::format("You have used a keyboard shortcut to load another patch,\n"
                                "which will discard any changes made so far.\n\n"
                                "Do you want to proceed?"),
                    Surge::Storage::PromptToActivateCategoryAndPatchOnKeypress, [this, action]() {
                        closeOverlay(SAVE_PATCH);

                        auto insideCategory = Surge::Storage::getUserDefaultValue(
                            &(this->synth->storage), Surge::Storage::PatchJogWraparound, 1);

                        loadPatchWithDirtyCheck(action == Surge::GUI::NEXT_PATCH, false,
                                                insideCategory);
                    });
            }
            case Surge::GUI::PREV_CATEGORY:
            case Surge::GUI::NEXT_CATEGORY:
            {
                return promptForOKCancelWithDontAskAgain(
                    "Confirm Category Change",
                    fmt::format("You have used a keyboard shortcut to select another category,\n"
                                "which will discard any changes made so far.\n\n"
                                "Do you want to proceed?"),
                    Surge::Storage::PromptToActivateCategoryAndPatchOnKeypress, [this, action]() {
                        closeOverlay(SAVE_PATCH);

                        loadPatchWithDirtyCheck(action == Surge::GUI::NEXT_CATEGORY, true);
                    });
            }

            // TODO: UPDATE WHEN ADDING MORE OSCILLATORS
            case Surge::GUI::OSC_1:
                changeSelectedOsc(0);
                return true;
            case Surge::GUI::OSC_2:
                changeSelectedOsc(1);
                return true;
            case Surge::GUI::OSC_3:
                changeSelectedOsc(2);
                return true;

            // TODO: FIX SCENE ASSUMPTION
            case Surge::GUI::TOGGLE_SCENE:
            {
                auto s = current_scene + 1;

                if (s >= n_scenes)
                {
                    s = 0;
                }

                changeSelectedScene(s);
                return true;
            }

            case Surge::GUI::TOGGLE_MODULATOR_ARM:
            {
                toggle_mod_editing();
                return true;
            }

#if WINDOWS
            case Surge::GUI::TOGGLE_DEBUG_CONSOLE:
                Surge::Debug::toggleConsole();
                return true;
#endif
            case Surge::GUI::SHOW_KEYBINDINGS_EDITOR:
                toggleOverlay(SurgeGUIEditor::KEYBINDINGS_EDITOR);
                frame->repaint();
                return true;
            case Surge::GUI::SHOW_LFO_EDITOR:
                if (lfoDisplay->isMSEG())
                {
                    toggleOverlay(SurgeGUIEditor::MSEG_EDITOR);
                }

                if (lfoDisplay->isFormula())
                {
                    toggleOverlay(SurgeGUIEditor::FORMULA_EDITOR);
                }

                return true;
#if INCLUDE_WT_SCRIPTING_EDITOR
            case Surge::GUI::SHOW_WT_EDITOR:
                toggleOverlay(SurgeGUIEditor::WT_EDITOR);
                frame->repaint();
                return true;
#endif
            case Surge::GUI::SHOW_MODLIST:
                toggleOverlay(SurgeGUIEditor::MODULATION_EDITOR);
                frame->repaint();
                return true;
            case Surge::GUI::SHOW_TUNING_EDITOR:
                toggleOverlay(SurgeGUIEditor::TUNING_EDITOR);
                frame->repaint();
                return true;
            case Surge::GUI::TOGGLE_VIRTUAL_KEYBOARD:
                toggleVirtualKeyboard();
                return true;
            case Surge::GUI::TOGGLE_OSCILLOSCOPE:
                toggleOverlay(SurgeGUIEditor::OSCILLOSCOPE);
                return true;

            case Surge::GUI::ZOOM_TO_DEFAULT:
            {
                auto dzf = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                               Surge::Storage::DefaultZoom, 100);
                resizeWindow(dzf);
                return true;
            }
            case Surge::GUI::ZOOM_MINUS_10:
            case Surge::GUI::ZOOM_MINUS_25:
            case Surge::GUI::ZOOM_PLUS_10:
            case Surge::GUI::ZOOM_PLUS_25:
            {
                auto zl =
                    (action == Surge::GUI::ZOOM_MINUS_10 || action == Surge::GUI::ZOOM_PLUS_10)
                        ? 10
                        : 25;
                auto di =
                    (action == Surge::GUI::ZOOM_MINUS_10 || action == Surge::GUI::ZOOM_MINUS_25)
                        ? -1
                        : 1;
                auto jog = zl * di;

                resizeWindow(getZoomFactor() + jog);

                return true;
            }
            case Surge::GUI::ZOOM_FULLSCREEN:
            {
#if 0
                if (Surge::GUI::getIsStandalone())
                {
                    juce::Component *comp = frame.get();

                    while (comp)
                    {
                        auto *cdw = dynamic_cast<juce::ResizableWindow *>(comp);

                        if (cdw)
                        {
                            auto w = juce::Component::SafePointer(cdw);

                            if (w)
                            {
                                w->setFullScreen(!cdw->isFullScreen());
                            }

                            comp = nullptr;
                        }
                        else
                        {
                            comp = comp->getParentComponent();
                        }
                    }

                    return true;
                }
#endif

                return false;
            }
            case Surge::GUI::FOCUS_NEXT_CONTROL_GROUP:
            case Surge::GUI::FOCUS_PRIOR_CONTROL_GROUP:
            {
                auto dir = (action == Surge::GUI::FOCUS_NEXT_CONTROL_GROUP ? 1 : -1);
                auto fc = frame->getCurrentlyFocusedComponent();

                if (fc == frame.get())
                {
                }
                else
                {
                    while (fc->getParentComponent() && fc->getParentComponent() != frame.get())
                    {
                        fc = fc->getParentComponent();
                    }
                }

                if (fc == nullptr || fc == frame.get())
                {
                    return false;
                }

                auto cg = (int)fc->getProperties().getWithDefault("ControlGroup", -1);

                // special case - allow jump off the patch selector
                if (dynamic_cast<Surge::Widgets::PatchSelector *>(fc))
                {
                    cg = dir < 0 ? 10000000 : 0;
                }
                if (cg < 0)
                {
                    return false;
                }

                juce::Component *focusThis{nullptr};

                for (auto c : frame->getChildren())
                {
                    auto ccg = (int)c->getProperties().getWithDefault("ControlGroup", -1);

                    if (ccg < 0)
                    {
                        continue;
                    }

                    if (dir < 0)
                    {
                        if (ccg < cg)
                        {
                            if (focusThis)
                            {
                                auto ncg = (int)focusThis->getProperties().getWithDefault(
                                    "ControlGroup", -1);

                                if (ncg < ccg)
                                {
                                    focusThis = c;
                                }
                            }
                            else
                            {
                                focusThis = c;
                            }
                        }
                    }
                    else
                    {
                        if (ccg > cg)
                        {
                            if (focusThis)
                            {
                                auto ncg = (int)focusThis->getProperties().getWithDefault(
                                    "ControlGroup", -1);

                                if (ncg > ccg)
                                {
                                    focusThis = c;
                                }
                            }
                            else
                            {
                                focusThis = c;
                            }
                        }
                    }
                }

                if (focusThis)
                {
                    // So now focus the first element of focusThis
                    if (focusThis->getWantsKeyboardFocus())
                    {
                        focusThis->grabKeyboardFocus();

                        return true;
                    }

                    for (auto c : focusThis->getChildren())
                    {
                        if (c->getWantsKeyboardFocus() && c->isShowing())
                        {
                            c->grabKeyboardFocus();

                            return true;
                        }
                    }

                    return false;
                }
                else
                {
                    // We are off the end. If dir = +1 grab the smallest, else the largest
                    juce::Component *wrapper;
                    int lccg = (dir > 0 ? std::numeric_limits<int>::max() : 0);

                    for (auto c : frame->getChildren())
                    {
                        auto ccg = (int)c->getProperties().getWithDefault("ControlGroup", -1);

                        if (ccg < 0)
                        {
                            continue;
                        }

                        if (dir > 0 && ccg < lccg)
                        {
                            lccg = ccg;
                            wrapper = c;
                        }

                        if (dir < 0 && ccg > lccg)
                        {
                            lccg = ccg;
                            wrapper = c;
                        }
                    }

                    if (wrapper)
                    {
                        for (auto c : wrapper->getChildren())
                        {
                            if (c->getWantsKeyboardFocus() && c->isShowing())
                            {
                                c->grabKeyboardFocus();

                                return true;
                            }
                        }
                    }

                    return false;
                }
            }

            case Surge::GUI::REFRESH_SKIN:
                refreshSkin();
                return true;

            case Surge::GUI::OPEN_MANUAL:
            {
                juce::Component *target = nullptr;
                if (!editsFollowKeyboardFocus)
                {
                    auto fc = frame->recursivelyFindFirstChildMatching([](juce::Component *c) {
                        auto h = dynamic_cast<Surge::GUI::Hoverable *>(c);
                        if (h && h->isCurrentlyHovered())
                            return true;
                        return false;
                    });
                    target = fc;
                }
                else
                {
                    auto fc = frame->recursivelyFindFirstChildMatching(
                        [](juce::Component *c) { return (c && c->hasKeyboardFocus(false)); });
                    target = fc;
                }

                while (target && !dynamic_cast<Surge::GUI::IComponentTagValue *>(target))
                {
                    target = target->getParentComponent();
                }

                auto thingToOpen = std::string(stringManual);
                if (target)
                {
                    auto icv = dynamic_cast<Surge::GUI::IComponentTagValue *>(target);
                    auto tag = icv->getTag();
                    if (tag >= start_paramtags)
                    {
                        auto pid = tag - start_paramtags;
                        if (pid >= 0 && pid < n_total_params &&
                            synth->storage.getPatch().param_ptr[pid])
                        {
                            thingToOpen =
                                thingToOpen +
                                helpURLFor(
                                    synth->storage.getPatch().param_ptr[tag - start_paramtags]);
                        }
                    }
                }
                juce::URL(thingToOpen).launchInDefaultBrowser();
                return true;
            }
            case Surge::GUI::ANNOUNCE_STATE:
                announceGuiState();
                break;
            case Surge::GUI::SKIN_LAYOUT_GRID:
            case Surge::GUI::TOGGLE_ABOUT:
            {
                int pxres = -1;

                if (action == Surge::GUI::SKIN_LAYOUT_GRID)
                {
                    pxres = Surge::Storage::getUserDefaultValue(
                        &(synth->storage), Surge::Storage::LayoutGridResolution, 20);
                };

                if (frame->getIndexOfChildComponent(aboutScreen.get()) >= 0)
                {
                    bool doShowAgain = false;

                    if ((action == Surge::GUI::SKIN_LAYOUT_GRID &&
                         aboutScreen->devModeGrid == -1) ||
                        (action == Surge::GUI::TOGGLE_ABOUT && aboutScreen->devModeGrid > -1))
                    {
                        doShowAgain = true;
                    }

                    hideAboutScreen();

                    if (doShowAgain)
                    {
                        showAboutScreen(pxres);
                    }
                }
                else
                {
                    showAboutScreen(pxres);
                }

                return true;
            }

            default:
                break;
            }
        }
    }

    if (!editsFollowKeyboardFocus)
    {
        if (Surge::Widgets::isAccessibleKey(key))
        {
            auto fc = frame->recursivelyFindFirstChildMatching([](juce::Component *c) {
                auto h = dynamic_cast<Surge::GUI::Hoverable *>(c);
                if (h && h->isCurrentlyHovered())
                    return true;
                return false;
            });

            if (fc)
            {
                synth->storage.userDefaultsProvider->addOverride(
                    Surge::Storage::DefaultKey::MenuAndEditKeybindingsFollowKeyboardFocus, true);

                auto res = fc->keyPressed(key);
                synth->storage.userDefaultsProvider->clearOverride(
                    Surge::Storage::DefaultKey::MenuAndEditKeybindingsFollowKeyboardFocus);
                if (res)
                    return res;
            }
        }
    }

    if (triedKey && !shortcutsUsed)
    {
        promptForOKCancelWithDontAskAgain(
            "Enable Keyboard Shortcuts",
            "You have used a keyboard shortcut that Surge XT has assigned to a certain "
            "action,\n"
            "but the option to use keyboard shortcuts is currently disabled. "
            "\nWould you like to enable it?\n\n"
            "You can change this option later in the Workflow menu.",
            Surge::Storage::DefaultKey::PromptToActivateShortcutsOnAccKeypress,
            [this]() { toggleUseKeyboardShortcuts(); });
    }

    return false;
}

std::string SurgeGUIEditor::showShortcutDescription(const std::string &shortcutDesc,
                                                    const std::string &shortcutDescMac)
{
    if (getUseKeyboardShortcuts())
    {
#if MAC
        return shortcutDescMac;
#else
        return shortcutDesc;
#endif
    }
    else
    {
        return "";
    }
}

void SurgeGUIEditor::announceGuiState()
{
    std::ostringstream oss;

    auto s = getStorage();
    auto ot = s->getPatch().scene[current_scene].osc[current_osc[current_scene]].type.val.i;
    auto ft = s->getPatch().fx[current_fx].type.val.i;
    auto ms = s->getPatch()
                  .scene[current_scene]
                  .lfo[modsource_editor[current_scene] - ms_lfo1]
                  .shape.val.i;

    oss << "Patch '" << s->getPatch().name << "'. Scene " << (current_scene == 0 ? "A" : "B")
        << ". "
        << " Oscillator " << (current_osc[current_scene] + 1) << " " << osc_type_names[ot] << "."
        << " Modulator "
        << ModulatorName::modulatorName(&synth->storage, modsource_editor[current_scene], false,
                                        current_scene)
        << " " << lt_names[ms] << ". " << fxslot_names[current_fx] << " " << fx_type_names[ft]
        << "." << std::endl;
    enqueueAccessibleAnnouncement(oss.str());
}

std::string SurgeGUIEditor::showShortcutDescription(const std::string &shortcutDesc)
{
    if (getUseKeyboardShortcuts())
    {
        return shortcutDesc;
    }
    else
    {
        return "";
    }
}

void SurgeGUIEditor::setPatchAsFavorite(const std::string &pname, bool b)
{
    std::ostringstream oss;
    oss << pname << (b ? " added to " : " removed from ") << "favorite patches.";
    enqueueAccessibleAnnouncement(oss.str());

    setSpecificPatchAsFavorite(synth->patchid, b);
}

void SurgeGUIEditor::setSpecificPatchAsFavorite(int patchid, bool b)
{
    if (patchid >= 0 && patchid < synth->storage.patch_list.size())
    {
        synth->storage.patch_list[patchid].isFavorite = b;
        synth->storage.patchDB->setUserFavorite(synth->storage.patch_list[patchid].path.u8string(),
                                                b);
    }
}

bool SurgeGUIEditor::isPatchFavorite()
{
    if (synth->patchid >= 0 && synth->patchid < synth->storage.patch_list.size())
    {
        return synth->storage.patch_list[synth->patchid].isFavorite;
    }
    return false;
}

bool SurgeGUIEditor::isPatchUser()
{
    if (synth->patchid >= 0 && synth->patchid < synth->storage.patch_list.size())
    {
        auto p = synth->storage.patch_list[synth->patchid];
        return !synth->storage.patch_category[p.category].isFactory;
    }
    return false;
}

void SurgeGUIEditor::populateDawExtraState(SurgeSynthesizer *synth)
{
    auto des = &(synth->storage.getPatch().dawExtraState);

    des->isPopulated = true;
    des->editor.instanceZoomFactor = zoomFactor;
    des->editor.current_scene = current_scene;
    des->editor.current_fx = current_fx;
    des->editor.modsource = modsource;

    for (int i = 0; i < n_scenes; ++i)
    {
        des->editor.current_osc[i] = current_osc[i];
        des->editor.modsource_editor[i] = modsource_editor[i];

        des->editor.msegStateIsPopulated = true;

        for (int lf = 0; lf < n_lfos; ++lf)
        {
            des->editor.msegEditState[i][lf].timeEditMode = msegEditState[i][lf].timeEditMode;
        }
    }

    des->editor.isMSEGOpen = isAnyOverlayPresent(MSEG_EDITOR);

    des->editor.activeOverlays.clear();

    des->tuningApplicationMode = (int)synth->storage.getTuningApplicationMode();

    for (const auto &ol : juceOverlays)
    {
        auto olw = getOverlayWrapperIfOpen(ol.first);

        if (olw && olw->getRetainOpenStateOnEditorRecreate())
        {
            DAWExtraStateStorage::EditorState::OverlayState os;

            os.whichOverlay = ol.first;
            os.isTornOut = olw->isTornOut();
            os.tearOutPosition = std::make_pair(-1, -1);

            if (olw->isTornOut())
            {
                auto ps = olw->currentTearOutLocation();
                os.tearOutPosition = std::make_pair(ps.x, ps.y);
            }

            des->editor.activeOverlays.push_back(os);
        }
    }
}

void SurgeGUIEditor::clearNoProcessingOverlay()
{
    if (noProcessingOverlay)
    {
        showNoProcessingOverlay = false;
        frame->removeChildComponent(noProcessingOverlay.get());
        noProcessingOverlay.reset(nullptr);
    }
}

void SurgeGUIEditor::loadFromDawExtraState(SurgeSynthesizer *synth)
{
    auto des = &(synth->storage.getPatch().dawExtraState);

    if (des->isPopulated)
    {
        auto sz = des->editor.instanceZoomFactor;

        if (sz > 0)
        {
            setZoomFactor(sz);
        }

        current_scene = des->editor.current_scene;
        current_fx = des->editor.current_fx;
        modsource = des->editor.modsource;

        activateFromCurrentFx();

        for (int i = 0; i < n_scenes; ++i)
        {
            current_osc[i] = des->editor.current_osc[i];
            modsource_editor[i] = des->editor.modsource_editor[i];

            if (des->editor.msegStateIsPopulated)
            {
                for (int lf = 0; lf < n_lfos; ++lf)
                {
                    msegEditState[i][lf].timeEditMode =
                        des->editor.msegEditState[i][lf].timeEditMode;
                }
            }
        }

        // This is mostly legacy treatment but I'm leaving it in for now
        if (des->editor.isMSEGOpen)
        {
            showMSEGEditorOnNextIdleOrOpen = true;
        }

        overlaysForNextIdle.clear();

        if (!des->editor.activeOverlays.empty())
        {
            showMSEGEditorOnNextIdleOrOpen = false;
            overlaysForNextIdle = des->editor.activeOverlays;
        }

        switch (des->tuningApplicationMode)
        {
        case 0:
            synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_ALL);
            break;
        case 1:
            synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
            break;
        }
    }
}

void SurgeGUIEditor::showPatchCommentTooltip(const std::string &comment)
{
    if (patchSelectorComment)
    {
        auto psb = patchSelector->getBounds();

        patchSelectorComment->positionForComment(psb.getCentre().withY(psb.getBottom()), comment,
                                                 psb.getWidth());
        patchSelectorComment->setVisible(true);
        patchSelectorComment->getParentComponent()->toFront(true);
        patchSelectorComment->toFront(true);
    }
}

void SurgeGUIEditor::hidePatchCommentTooltip()
{
    if (patchSelectorComment && patchSelectorComment->isVisible())
    {
        patchSelectorComment->setVisible(false);
    }
}

void SurgeGUIEditor::addComponentWithTracking(juce::Component *target, juce::Component &source)
{
    if (target->getIndexOfChildComponent(&source) >= 0)
    {
        // std::cout << "Not double adding component " << source.getTitle() << std::endl;
    }
    else
    {
        // std::cout << "Primary Adding Component " << source.getTitle() << std::endl;
        target->addChildComponent(source);
    }
    auto cf = containedComponents.find(&source);
    if (cf != containedComponents.end())
        containedComponents.erase(cf);

    /* Place this accessibility invalidation here in either case */
    if (auto *handler = source.getAccessibilityHandler())
    {
        if (handler->getValueInterface())
        {
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
        }
        handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
    }
}
void SurgeGUIEditor::addAndMakeVisibleWithTracking(juce::Component *target, juce::Component &source)
{
    addComponentWithTracking(target, source);
    source.setVisible(true);
}
void SurgeGUIEditor::addAndMakeVisibleWithTrackingInCG(ControlGroup cg, juce::Component &source)
{
    addAndMakeVisibleWithTracking(frame->getControlGroupLayer(cg), source);
}

void SurgeGUIEditor::resetComponentTracking()
{
    containedComponents.clear();

    std::function<void(juce::Component * comp)> rec;
    rec = [this, &rec](juce::Component *comp) {
        bool track = true;
        bool recurse = true;

        if (dynamic_cast<Surge::Widgets::MainFrame::OverlayComponent *>(comp))
            track = false;
        if (dynamic_cast<Surge::Widgets::MainFrame *>(comp))
            track = false;

        if (dynamic_cast<Surge::Widgets::OscillatorWaveformDisplay *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::Widgets::ModulationSourceButton *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::GUI::IComponentTagValue *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::Overlays::TypeinParamEditor *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::Overlays::MiniEdit *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::Overlays::Alert *>(comp))
            recurse = false;
        if (dynamic_cast<Surge::Overlays::OverlayWrapper *>(comp))
            recurse = false;
        if (dynamic_cast<juce::ListBox *>(comp)) // special case of the typeahead
        {
            recurse = false;
            track = false;
        }

        if (track)
        {
            containedComponents[comp] = comp->getParentComponent();
        }

        if (recurse)
            for (auto c : comp->getChildren())
                rec(c);
    };
    rec(frame.get());
}
void SurgeGUIEditor::removeUnusedTrackedComponents()
{
    for (auto c : containedComponents)
    {
        auto p = c.second;
        p->removeChildComponent(c.first);
    }
    frame->repaint();
}

void SurgeGUIEditor::globalFocusChanged(juce::Component *fc)
{
    if (!frame)
        return;

    auto newRect = juce::Rectangle<int>();
    if (fc)
    {
        newRect = frame->getLocalArea(fc->getParentComponent(), fc->getBounds());
        frame->focusRectangle = newRect;
    }
    else
    {
        frame->focusRectangle = juce::Rectangle<int>();
    }
    if (debugFocus)
    {
        frame->repaint();

        std::cout << "FC [" << fc << "] ";
        if (fc)
        {
            std::cout << fc->getTitle() << " " << typeid(*fc).name() << " " << newRect.toString()
                      << " " << fc->getAccessibilityHandler() << " " << fc->getTitle();
        }
        std::cout << std::endl;
    }
}

bool SurgeGUIEditor::promptForOKCancelWithDontAskAgain(
    const ::std::string &title, const std::string &msg, Surge::Storage::DefaultKey dontAskAgainKey,
    std::function<void()> okCallback, std::string ynMessage, AskAgainStates askAgainDef)
{
    auto bypassed =
        Surge::Storage::getUserDefaultValue(&(synth->storage), dontAskAgainKey, askAgainDef);

    if (bypassed == NEVER)
    {
        return false;
    }

    if (bypassed == ALWAYS)
    {
        okCallback();
        return true;
    }

    alert = std::make_unique<Surge::Overlays::Alert>();
    alert->setSkin(currentSkin, bitmapStore);
    alert->setDescription(title);
    alert->setWindowTitle(title);
    addAndMakeVisibleWithTracking(frame.get(), *alert);
    alert->setLabel(msg);
    alert->addToggleButtonAndSetText(ynMessage);
    alert->setOkCancelButtonTexts("OK", "Cancel");
    alert->onOkForToggleState = [this, dontAskAgainKey, okCallback](bool dontAskAgain) {
        if (dontAskAgain)
        {
            Surge::Storage::updateUserDefaultValue(&(synth->storage), dontAskAgainKey, ALWAYS);
        }
        okCallback();
    };
    alert->onCancelForToggleState = [this, dontAskAgainKey](bool dontAskAgain) {
        if (dontAskAgain)
        {
            Surge::Storage::updateUserDefaultValue(&(synth->storage), dontAskAgainKey, NEVER);
        }
    };
    alert->setBounds(0, 0, getWindowSizeX(), getWindowSizeY());
    alert->setVisible(true);
    alert->toFront(true);
    return false;
}

void SurgeGUIEditor::loadPatchWithDirtyCheck(bool increment, bool isCategory, bool insideCategory)
{
    if (synth->storage.getPatch().isDirty)
    {
        promptForOKCancelWithDontAskAgain(
            "Confirm Patch Loading",
            fmt::format("The currently loaded patch has unsaved changes.\n"
                        "Loading a new patch will discard any such changes.\n\n"
                        "Do you want to proceed?"),
            Surge::Storage::PromptToLoadOverDirtyPatch,
            [this, increment, isCategory, insideCategory]() {
                closeOverlay(SAVE_PATCH);

                synth->jogPatchOrCategory(increment, isCategory, insideCategory);
            },
            "Don't ask me again", ALWAYS);
    }
    else
    {
        closeOverlay(SAVE_PATCH);

        synth->jogPatchOrCategory(increment, isCategory, insideCategory);
    }
}

void SurgeGUIEditor::enqueueAccessibleAnnouncement(const std::string &s)
{
    auto doAcc = Surge::Storage::getUserDefaultValue(
        &(synth->storage), Surge::Storage::UseNarratorAnnouncements, false);

    if (doAcc)
    {
        accAnnounceStrings.push_back({s, 3});
    }
}

void SurgeGUIEditor::playNote(char key, char vel)
{
    auto ed = juceEditor;
    auto &proc = ed->processor;

    proc.midiFromGUI.push(SurgeSynthProcessor::midiR(0, key, vel, true));
}

void SurgeGUIEditor::releaseNote(char key, char vel)
{
    auto ed = juceEditor;
    auto &proc = ed->processor;

    proc.midiFromGUI.push(SurgeSynthProcessor::midiR(0, key, vel, false));
}