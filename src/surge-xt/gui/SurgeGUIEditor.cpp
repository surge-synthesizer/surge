/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

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
#include "overlays/MSEGEditor.h"
#include "overlays/LuaEditors.h"
#include "overlays/ModulationEditor.h"
#include "overlays/TypeinParamEditor.h"
#include "overlays/Oscilloscope.h"
#include "overlays/OverlayWrapper.h"

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
#include "libMTSClient.h"

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
/*
 * See the comment in SurgeGUIUtils.cpp
 */
namespace Surge
{
namespace GUI
{
extern void setIsStandalone(bool b);
}
} // namespace Surge

SurgeGUIEditor::SurgeGUIEditor(SurgeSynthEditor *jEd, SurgeSynthesizer *synth)
{
    jassert(Surge::GUI::ModulationGrid::getModulationGrid());

    assert(n_paramslots >= n_total_params);
    synth->storage.addErrorListener(this);
    synth->storage.okCancelProvider = [](const std::string &msg, const std::string &title,
                                         SurgeStorage::OkCancel def,
                                         std::function<void(SurgeStorage::OkCancel)> callback) {
        // TODO: think about threading one day probably
        auto cb = juce::ModalCallbackFunction::create([callback](int isOk) {
            auto r = isOk ? SurgeStorage::OK : SurgeStorage::CANCEL;
            callback(r);
        });
        auto res = juce::AlertWindow::showOkCancelBox(juce::AlertWindow::NoIcon, title, msg, "Yes",
                                                      "No", nullptr, cb);
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

    minimumZoom = 50;
#if LINUX
    minimumZoom = 100; // See github issue #628
#endif

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
        loadFromDAWExtraState(synth);
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
}

SurgeGUIEditor::~SurgeGUIEditor()
{
    juce::Desktop::getInstance().removeFocusChangeListener(this);
    synth->removeModulationAPIListener(this);
    synth->storage.clearOkCancelProvider();
    auto isPop = synth->storage.getPatch().dawExtraState.isPopulated;
    populateDawExtraState(synth); // If I must die, leave my state for future generations
    synth->storage.getPatch().dawExtraState.isPopulated = isPop;
    synth->storage.removeErrorListener(this);
}

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
        juceEditor->surgeLF->updateDarkIfNeeded();
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

    if (errorItemCount)
    {
        std::vector<std::pair<std::string, std::string>> cp;
        {
            std::lock_guard<std::mutex> g(errorItemsMutex);
            cp = errorItems;
            errorItems.clear();
        }

        for (const auto &p : cp)
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::NoIcon, p.second, p.first);
        }
    }

    if (editor_open && frame && !synth->halt_engine)
    {
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
                lfoDisplay->repaint();
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
                loadFromDAWExtraState(synth);
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
                    << " running in order to load Surge patches. If the audio system is working,"
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

        if (synth->storage.getPatch()
                .scene[current_scene]
                .osc[current_osc[current_scene]]
                .wt.refresh_display)
        {
            synth->storage.getPatch()
                .scene[current_scene]
                .osc[current_osc[current_scene]]
                .wt.refresh_display = false;

            if (oscWaveform)
            {
                oscWaveform->repaint();
            }
        }

        if (polydisp)
        {
            int prior = polydisp->getPlayingVoiceCount();

            if (prior != synth->polydisplay)
            {
                polydisp->setPlayingVoiceCount(synth->polydisplay);
                polydisp->repaint();
            }
        }

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
                bool hasmts =
                    synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;

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
                    char pname[256], pdisp[256];
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
                        lfoDisplay->invalidateIfIdIsInRange(j);
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
                lfoDisplay->invalidateIfAnythingIsTemposynced();
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
                }

                if (oscWaveform)
                {
                    oscWaveform->repaintIfIdIsInRange(j);
                }

                if (lfoDisplay)
                {
                    lfoDisplay->invalidateIfIdIsInRange(j);
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
                    // This is gross hack for our reordering of scenemode. Basically take the
                    // automation value and turn it into the UI value
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

    // Force the oscilloscope, if open, to re-render for nice smooth movement.
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
            std::string modname = modulatorName(modsource_editor[current_scene], true);

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
            gui_modsrc[ms]->setTag(tag_mod_source0 + ms);
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
                    effectLabels[i]->setLabel(label);
                    effectLabels[i]->setSkin(currentSkin, bitmapStore);

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
            auto hasmts = synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;
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
            vu[0]->setType(Surge::ParamConfig::vut_vu_stereo);
            vu[0]->setStorage(&(synth->storage));

            addAndMakeVisibleWithTracking(frame.get(), *vu[0]);

            break;
        }
        case Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED:
        case Surge::Skin::Connector::NonParameterConnection::SAVE_PATCH_DIALOG:
        case Surge::Skin::Connector::NonParameterConnection::MSEG_EDITOR_WINDOW:
        case Surge::Skin::Connector::NonParameterConnection::FORMULA_EDITOR_WINDOW:
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
    if (synth->storage.oddsound_mts_active)
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
    setzero(param);
}

bool SurgeGUIEditor::open(void *parent)
{
    int platformType = 0;
    float fzf = getZoomFactor() / 100.0;

    frame = std::make_unique<Surge::Widgets::MainFrame>();
    frame->setBounds(0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY());
    frame->setSurgeGUIEditor(this);

    juceEditor->addAndMakeVisible(*frame);
    juceEditor->addKeyListener(this);

    // TODO: SET UP JUCE EDITOR BETTER!
    bitmapStore.reset(new SurgeImageStore());
    bitmapStore->setupBuiltinBitmaps();

    if (!currentSkin->reloadSkin(bitmapStore))
    {
        auto db = Surge::GUI::SkinDB::get();

        std::ostringstream oss;
        oss << "Unable to load current skin! Reverting the skin to Surge Classic.\n\nSkin Error:\n"
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
        if (mod_editor)
        {
            auto mci = dynamic_cast<Surge::Widgets::ModulatableControlInterface *>(control);
            if (mci)
            {
                undoManager()->pushModulationChange(
                    ptag, synth->storage.getPatch().param_ptr[ptag], modsource, current_scene,
                    modsource_index, mci->modValue,
                    synth->isModulationMuted(ptag, modsource, current_scene, modsource_index));
            }
        }
        else
        {
            undoManager()->pushParameterChange(ptag, synth->storage.getPatch().param_ptr[ptag],
                                               synth->storage.getPatch().param_ptr[ptag]->val);
        }
        juceEditor->beginParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
    else if (tag_mod_source0 + ms_ctrl1 <= tag &&
             tag_mod_source0 + ms_ctrl1 + n_customcontrollers > tag)
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
        juceEditor->endParameterEdit(synth->storage.getPatch().param_ptr[ptag]);
    }
    else if (tag_mod_source0 + ms_ctrl1 <= tag &&
             tag_mod_source0 + ms_ctrl1 + n_customcontrollers > tag)
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

void SurgeGUIEditor::showZoomMenu(const juce::Point<int> &where,
                                  Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeZoomMenu(where, true);
    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::showMPEMenu(const juce::Point<int> &where,
                                 Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeMpeMenu(where, true);
    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::showLfoMenu(const juce::Point<int> &where,
                                 Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeLfoMenu(where);
    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::toggleTuning()
{
    synth->storage.toggleTuningToCache();

    if (statusTune)
    {
        bool hasmts = synth->storage.oddsound_mts_client && synth->storage.oddsound_mts_active;
        statusTune->setValue(this->synth->storage.isStandardTuning ? hasmts : 1);
    }

    this->synth->refresh_editor = true;
}

void SurgeGUIEditor::showTuningMenu(const juce::Point<int> &where,
                                    Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeTuningMenu(where, true);

    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::scaleFileDropped(const string &fn)
{
    try
    {
        undoManager()->pushTuning(synth->storage.currentTuning);
        this->synth->storage.retuneToScale(Tunings::readSCLFile(fn));
        this->synth->refresh_editor = true;
    }
    catch (Tunings::TuningError &e)
    {
        synth->storage.retuneTo12TETScaleC261Mapping();
        synth->storage.reportError(e.what(), "SCL Error");
    }
    tuningChanged();
}

void SurgeGUIEditor::mappingFileDropped(const string &fn)
{
    try
    {
        undoManager()->pushTuning(synth->storage.currentTuning);
        this->synth->storage.remapToKeyboard(Tunings::readKBMFile(fn));
        this->synth->refresh_editor = true;
    }
    catch (Tunings::TuningError &e)
    {
        synth->storage.remapToConcertCKeyboard();
        synth->storage.reportError(e.what(), "KBM Error");
    }
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

void SurgeGUIEditor::resizeWindow(float zf) { setZoomFactor(zf, true); }

void SurgeGUIEditor::setZoomFactor(float zf) { setZoomFactor(zf, false); }

void SurgeGUIEditor::setZoomFactor(float zf, bool resizeWindow)
{
    zoomFactor = std::max(zf, 25.f);
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
    setBitmapZoomFactor(zoomFactor);
    rezoomOverlays();
}

void SurgeGUIEditor::setBitmapZoomFactor(float zf)
{
    float dbs = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->scale;
    int fullPhysicalZoomFactor = (int)(zf * dbs);
    if (bitmapStore != nullptr)
        bitmapStore->setPhysicalZoomFactor(fullPhysicalZoomFactor);
}

void SurgeGUIEditor::showMinimumZoomError() const
{
    std::ostringstream oss;
    oss << "The smallest zoom level possible on your platform is " << minimumZoom
        << "%. Sorry, you cannot make Surge XT any smaller!";
    synth->storage.reportError(oss.str(), "Zoom Level Error");
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
        msg << "Surge chose the largest fitting zoom level of " << zf << "%.";
    }
    synth->storage.reportError(msg.str(), "Zoom Level Adjusted");
#endif
}

void SurgeGUIEditor::showSettingsMenu(const juce::Point<int> &where,
                                      Surge::GUI::IComponentTagValue *launchFrom)
{
    auto settingsMenu = juce::PopupMenu();

    // settingsMenu.addItem("HACK DUMP", [this]() { undoManager()->dumpStack(); });

    auto zoomMenu = makeZoomMenu(where, false);
    settingsMenu.addSubMenu("Zoom", zoomMenu);

    auto skinSubMenu = makeSkinMenu(where);
    settingsMenu.addSubMenu("Skins", skinSubMenu);

    auto valueDispMenu = makeValueDisplaysMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Value Displays"), valueDispMenu);

    settingsMenu.addSeparator();

    auto dataSubMenu = makeDataMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Data Folders"), dataSubMenu);

    auto mouseMenu = makeMouseBehaviorMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Mouse Behavior"), mouseMenu);

    auto patchDefMenu = makePatchDefaultsMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Patch Defaults"), patchDefMenu);

    auto wfMenu = makeWorkflowMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Workflow"), wfMenu);

    settingsMenu.addSeparator();

    auto mpeSubMenu = makeMpeMenu(where, false);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("MPE Settings"), mpeSubMenu);

    auto midiSubMenu = makeMidiMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("MIDI Settings"), midiSubMenu);

    auto tuningSubMenu = makeTuningMenu(where, false);
    settingsMenu.addSubMenu("Tuning", tuningSubMenu);

    settingsMenu.addSeparator();

#if BUILD_IS_DEBUG
    useDevMenu = true;
#endif

    if (useDevMenu)
    {
        settingsMenu.addSeparator();

        auto devSubMenu = makeDevMenu(where);
        settingsMenu.addSubMenu(Surge::GUI::toOSCase("Developer Options"), devSubMenu);
    }

    settingsMenu.addSeparator();

    settingsMenu.addItem(Surge::GUI::toOSCase("Reach the Developers..."), []() {
        juce::URL(fmt::format("{}feedback", stringWebsite)).launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCase("Read the Code..."),
                         []() { juce::URL(stringRepository).launchInDefaultBrowser(); });

    settingsMenu.addItem(Surge::GUI::toOSCase("Download Additional Content..."), []() {
        juce::URL(fmt::format("{}surge-synthesizer.github.io/wiki/Additional-Content",
                              stringOrganization))
            .launchInDefaultBrowser();
    });

    settingsMenu.addItem(Surge::GUI::toOSCase("Skin Library..."), []() {
        juce::URL(fmt::format("{}skin-library", stringWebsite)).launchInDefaultBrowser();
    });

    Surge::GUI::addMenuWithShortcut(settingsMenu, Surge::GUI::toOSCase("Surge Manual..."),
                                    showShortcutDescription("F1"),
                                    []() { juce::URL(stringManual).launchInDefaultBrowser(); });

    settingsMenu.addItem(Surge::GUI::toOSCase("Surge Website..."),
                         []() { juce::URL(stringWebsite).launchInDefaultBrowser(); });

    settingsMenu.addSeparator();

    Surge::GUI::addMenuWithShortcut(settingsMenu, "About Surge", showShortcutDescription("F12"),
                                    [this]() { this->showAboutScreen(); });

    settingsMenu.showMenuAsync(popupMenuOptions(where),
                               Surge::GUI::makeEndHoverCallback(launchFrom));
}

juce::PopupMenu SurgeGUIEditor::makeLfoMenu(const juce::Point<int> &where)
{
    int currentLfoId = modsource_editor[current_scene] - ms_lfo1;
    int shapev = synth->storage.getPatch().scene[current_scene].lfo[currentLfoId].shape.val.i;
    std::string what;

    switch (shapev)
    {
    case lt_mseg:
        what = "MSEG";
        break;
    case lt_stepseq:
        what = "Step Seq";
        break;
    case lt_envelope:
        what = "Envelope";
        break;
    case lt_formula:
        what = "Formula";
        break;
    default:
        what = "LFO";
        break;
    }

    auto msurl = SurgeGUIEditor::helpURLForSpecial("lfo-presets");
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto lfoSubMenu = juce::PopupMenu();

    addHelpHeaderTo("LFO Presets", hurl, lfoSubMenu);
    lfoSubMenu.addSeparator();

    lfoSubMenu.addItem(
        Surge::GUI::toOSCase("Save " + what + " Preset As..."), [this, currentLfoId, what]() {
            promptForMiniEdit(
                "", "Enter the preset name:", what + " Preset Name", juce::Point<int>{},
                [this, currentLfoId](const std::string &s) {
                    this->synth->storage.modulatorPreset->savePresetToUser(
                        string_to_path(s), &(this->synth->storage), current_scene, currentLfoId);
                },
                lfoMenuButton);
        });

    auto presetCategories = this->synth->storage.modulatorPreset->getPresets(&(synth->storage));
    if (!presetCategories.empty())
    {
        lfoSubMenu.addSeparator();
    }

    std::function<void(juce::PopupMenu & m, const Surge::Storage::ModulatorPreset::Category &cat)>
        recurseCat;
    recurseCat = [this, currentLfoId, presetCategories, &recurseCat](
                     juce::PopupMenu &m, const Surge::Storage::ModulatorPreset::Category &cat) {
        for (const auto &p : cat.presets)
        {
            auto action = [this, p, currentLfoId]() {
                undoManager()->pushFullLFO(current_scene, currentLfoId);
                this->synth->storage.modulatorPreset->loadPresetFrom(
                    p.path, &(this->synth->storage), current_scene, currentLfoId);

                auto newshape = this->synth->storage.getPatch()
                                    .scene[current_scene]
                                    .lfo[currentLfoId]
                                    .shape.val.i;

                if (isAnyOverlayPresent(MSEG_EDITOR))
                {
                    bool tornOut = false;
                    juce::Point<int> tearOutPos;

                    auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);

                    if (olw && olw->isTornOut())
                    {
                        tornOut = true;
                        tearOutPos = olw->currentTearOutLocation();
                    }

                    closeOverlay(SurgeGUIEditor::MSEG_EDITOR);

                    if (newshape == lt_mseg)
                    {
                        showOverlay(SurgeGUIEditor::MSEG_EDITOR);
                        if (tornOut)
                        {
                            auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);

                            if (olw)
                            {
                                olw->doTearOut(tearOutPos);
                            }
                        }
                    }
                }

                this->synth->refresh_editor = true;
            };
            m.addItem(p.name, action);
        }
        bool haveD = false;
        for (const auto &sc : presetCategories)
        {
            if (sc.parentPath == cat.path)
            {
                if (!haveD)
                    m.addSeparator();
                haveD = true;
                juce::PopupMenu subMenu;
                recurseCat(subMenu, sc);
                m.addSubMenu(sc.name, subMenu);
            }
        }
    };

    for (auto tlc : presetCategories)
    {
        if (tlc.parentPath.empty())
        {
            juce::PopupMenu sm;
            recurseCat(sm, tlc);
            lfoSubMenu.addSubMenu(tlc.name, sm);
        }
    }

    lfoSubMenu.addSeparator();
    lfoSubMenu.addItem(Surge::GUI::toOSCase("Rescan Presets"),
                       [this]() { this->synth->storage.modulatorPreset->forcePresetRescan(); });

    return lfoSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMpeMenu(const juce::Point<int> &where, bool showhelp)
{
    auto mpeSubMenu = juce::PopupMenu();

    auto hu = helpURLForSpecial("mpe-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        addHelpHeaderTo("MPE", lurl, mpeSubMenu);
        mpeSubMenu.addSeparator();
    }

    std::string endis = "Enable MPE";

    if (synth->mpeEnabled)
    {
        endis = "Disable MPE";
    }

    mpeSubMenu.addItem(endis.c_str(), [this]() { toggleMPE(); });

    mpeSubMenu.addSeparator();

    std::ostringstream oss;
    oss << "Change MPE Pitch Bend Range (Current: " << synth->storage.mpePitchBendRange
        << " Semitones)";

    mpeSubMenu.addItem(Surge::GUI::toOSCase(oss.str().c_str()), [this, where]() {
        const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
        promptForMiniEdit(
            c, "Enter a new value:", "MPE Pitch Bend Range", where,
            [this](const std::string &c) {
                int newVal = ::atoi(c.c_str());
                this->synth->storage.mpePitchBendRange = newVal;
            },
            mpeStatus);
    });

    std::ostringstream oss2;
    int def = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                  Surge::Storage::MPEPitchBendRange, 48);
    oss2 << "Change Default MPE Pitch Bend Range (Current: " << def << " Semitones)";

    mpeSubMenu.addItem(Surge::GUI::toOSCase(oss2.str().c_str()), [this, where]() {
        const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
        promptForMiniEdit(
            c, "Enter a default value:", "Default MPE Pitch Bend Range", where,
            [this](const std::string &s) {
                int newVal = ::atoi(s.c_str());
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::MPEPitchBendRange, newVal);
                this->synth->storage.mpePitchBendRange = newVal;
            },
            mpeStatus);
    });

    auto smoothMenu = makeSmoothMenu(where, Surge::Storage::PitchSmoothingMode,
                                     (int)Modulator::SmoothingMode::DIRECT,
                                     [this](auto md) { this->resetPitchSmoothing(md); });

    mpeSubMenu.addSubMenu(Surge::GUI::toOSCase("MPE Pitch Bend Smoothing"), smoothMenu);

    return mpeSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMonoModeOptionsMenu(const juce::Point<int> &where,
                                                        bool updateDefaults)
{
    auto monoSubMenu = juce::PopupMenu();

    auto mode = synth->storage.monoPedalMode;

    if (updateDefaults)
    {
        mode = (MonoPedalMode)Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);
    }

    bool isChecked = (mode == HOLD_ALL_NOTES);

    monoSubMenu.addItem(
        Surge::GUI::toOSCase("Sustain Pedal Holds All Notes (No Note Off Retrigger)"), true,
        isChecked, [this, isChecked, updateDefaults]() {
            this->synth->storage.monoPedalMode = HOLD_ALL_NOTES;
            if (!isChecked)
            {
                synth->storage.getPatch().isDirty = true;
            }
            if (updateDefaults)
            {
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::MonoPedalMode, (int)HOLD_ALL_NOTES);
            }
        });

    isChecked = (mode == RELEASE_IF_OTHERS_HELD);

    monoSubMenu.addItem(Surge::GUI::toOSCase("Sustain Pedal Allows Note Off Retrigger"), true,
                        isChecked, [this, isChecked, updateDefaults]() {
                            this->synth->storage.monoPedalMode = RELEASE_IF_OTHERS_HELD;
                            if (!isChecked)
                            {
                                synth->storage.getPatch().isDirty = true;
                            }
                            if (updateDefaults)
                            {
                                Surge::Storage::updateUserDefaultValue(
                                    &(this->synth->storage), Surge::Storage::MonoPedalMode,
                                    (int)RELEASE_IF_OTHERS_HELD);
                            }
                        });

    return monoSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeTuningMenu(const juce::Point<int> &where, bool showhelp)
{
    bool isTuningEnabled = !synth->storage.isStandardTuning;
    bool isScaleEnabled = !synth->storage.isStandardScale;
    bool isMappingEnabled = !synth->storage.isStandardMapping;

    bool isOddsoundOn =
        this->synth->storage.oddsound_mts_active && this->synth->storage.oddsound_mts_client;

    auto tuningSubMenu = juce::PopupMenu();
    auto hu = helpURLForSpecial("tun-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        addHelpHeaderTo(isOddsoundOn ? "Tuning (MTS-ESP)" : "Tuning", lurl, tuningSubMenu);

        tuningSubMenu.addSeparator();
    }

    if (isOddsoundOn)
    {
        std::string mtsScale = MTS_GetScaleName(synth->storage.oddsound_mts_client);

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Current Tuning: ") + mtsScale, false, false,
                              []() {});

        tuningSubMenu.addSeparator();
    }

    if (!isOddsoundOn)
    {
        if (isScaleEnabled)
        {
            auto tuningLabel = Surge::GUI::toOSCase("Current Tuning: ");

            if (synth->storage.currentScale.description.empty())
            {
                tuningLabel += path_to_string(fs::path(synth->storage.currentScale.name).stem());
            }
            else
            {
                tuningLabel += synth->storage.currentScale.description;
            }

            tuningSubMenu.addItem(tuningLabel, false, false, []() {});
        }

        if (isMappingEnabled)
        {
            auto mappingLabel = Surge::GUI::toOSCase("Current Keyboard Mapping: ");
            mappingLabel += path_to_string(fs::path(synth->storage.currentMapping.name).stem());

            tuningSubMenu.addItem(mappingLabel, false, false, []() {});
        }

        if (isTuningEnabled || isMappingEnabled)
        {
            tuningSubMenu.addSeparator();
        }

        std::string openname = isAnyOverlayPresent(TUNING_EDITOR) ? "Close " : "Open ";

        Surge::GUI::addMenuWithShortcut(tuningSubMenu,
                                        Surge::GUI::toOSCase(openname + "Tuning Editor..."),
                                        showShortcutDescription("Alt + T", u8"\U00002325T"),
                                        [this]() { this->toggleOverlay(TUNING_EDITOR); });

        tuningSubMenu.addSeparator();

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Set to Standard Tuning"),
                              !this->synth->storage.isStandardTuning, false, [this]() {
                                  this->synth->storage.retuneTo12TETScaleC261Mapping();
                                  this->synth->storage.resetTuningToggle();
                                  this->synth->refresh_editor = true;
                                  tuningChanged();
                              });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Set to Standard Mapping (Concert C)"),
                              (!this->synth->storage.isStandardMapping), false, [this]() {
                                  this->synth->storage.remapToConcertCKeyboard();
                                  this->synth->refresh_editor = true;
                                  tuningChanged();
                              });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Set to Standard Scale (12-TET)"),
                              (!this->synth->storage.isStandardScale), false, [this]() {
                                  this->synth->storage.retuneTo12TETScale();
                                  this->synth->refresh_editor = true;
                                  tuningChanged();
                              });

        tuningSubMenu.addSeparator();

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Load .scl Tuning..."), [this]() {
            auto cb = [this](std::string sf) {
                std::string sfx = ".scl";
                if (sf.length() >= sfx.length())
                {
                    if (sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0)
                    {
                        synth->storage.reportError("Please select only .scl files!",
                                                   "Invalid Choice");
                        std::cout << "FILE is [" << sf << "]" << std::endl;
                        return;
                    }
                }
                try
                {
                    auto sc = Tunings::readSCLFile(sf);

                    if (!this->synth->storage.retuneToScale(sc))
                    {
                        synth->storage.reportError("This .scl file is not valid!",
                                                   "File Format Error");
                        return;
                    }
                    this->synth->refresh_editor = true;
                }
                catch (Tunings::TuningError &e)
                {
                    synth->storage.retuneTo12TETScaleC261Mapping();
                    synth->storage.reportError(e.what(), "Loading Error");
                }
                tuningChanged();
            };

            auto scl_path = this->synth->storage.datapath / "tuning_library" / "SCL";

            scl_path = Surge::Storage::getUserDefaultPath(&(this->synth->storage),
                                                          Surge::Storage::LastSCLPath, scl_path);

            fileChooser = std::make_unique<juce::FileChooser>(
                "Select SCL Scale", juce::File(path_to_string(scl_path)), "*.scl");

            fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, scl_path, cb](const juce::FileChooser &c) {
                    auto ress = c.getResults();
                    if (ress.size() != 1)
                        return;
                    auto res = ress.getFirst();
                    auto rString = res.getFullPathName().toStdString();
                    auto dir =
                        string_to_path(res.getParentDirectory().getFullPathName().toStdString());
                    cb(rString);
                    if (dir != scl_path)
                    {
                        Surge::Storage::updateUserDefaultPath(&(this->synth->storage),
                                                              Surge::Storage::LastSCLPath, dir);
                    }
                });
        });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Load .kbm Keyboard Mapping..."), [this]() {
            auto cb = [this](std::string sf) {
                std::string sfx = ".kbm";
                if (sf.length() >= sfx.length())
                {
                    if (sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0)
                    {
                        synth->storage.reportError("Please select only .kbm files!",
                                                   "Invalid Choice");
                        std::cout << "FILE is [" << sf << "]" << std::endl;
                        return;
                    }
                }
                try
                {
                    auto kb = Tunings::readKBMFile(sf);

                    if (!this->synth->storage.remapToKeyboard(kb))
                    {
                        synth->storage.reportError("This .kbm file is not valid!",
                                                   "File Format Error");
                        return;
                    }

                    this->synth->refresh_editor = true;
                }
                catch (Tunings::TuningError &e)
                {
                    synth->storage.remapToConcertCKeyboard();
                    synth->storage.reportError(e.what(), "Loading Error");
                }
                tuningChanged();
            };

            auto kbm_path = this->synth->storage.datapath / "tuning_library" / "KBM Concert Pitch";

            kbm_path = Surge::Storage::getUserDefaultPath(&(this->synth->storage),
                                                          Surge::Storage::LastKBMPath, kbm_path);
            fileChooser = std::make_unique<juce::FileChooser>(
                "Select KBM Mapping", juce::File(path_to_string(kbm_path)), "*.kbm");

            fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, cb, kbm_path](const juce::FileChooser &c)

                {
                    auto ress = c.getResults();
                    if (ress.size() != 1)
                        return;

                    auto res = c.getResult();
                    auto rString = res.getFullPathName().toStdString();
                    auto dir =
                        string_to_path(res.getParentDirectory().getFullPathName().toStdString());
                    cb(rString);
                    if (dir != kbm_path)
                    {
                        Surge::Storage::updateUserDefaultPath(&(this->synth->storage),
                                                              Surge::Storage::LastKBMPath, dir);
                    }
                });
        });

        if (synth->storage.hasPatchStoredTuning)
        {
            tuningSubMenu.addItem(Surge::GUI::toOSCase("Load Tuning Embedded in Patch"), [this]() {
                synth->storage.retuneAndRemapToScaleAndMapping(
                    synth->storage.patchStoredTuning.scale,
                    synth->storage.patchStoredTuning.keyboardMapping);
            });
        }

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Factory Tuning Library..."), [this]() {
            auto path = this->synth->storage.datapath / "tuning_library";
            Surge::GUI::openFileOrFolder(path);
        });

        tuningSubMenu.addSeparator();

        int oct = 5 - Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                          Surge::Storage::MiddleC, 1);
        string middle_A = "A" + to_string(oct);

        tuningSubMenu.addItem(
            Surge::GUI::toOSCase("Remap " + middle_A + " (MIDI Note 69) Directly to..."),
            [this, middle_A, where]() {
                char c[256];
                snprintf(c, 256, "440.0");
                promptForMiniEdit(
                    c, fmt::format("Enter a new frequency for {:s}:", middle_A),
                    fmt::format("Remap {:s} Frequency", middle_A), where,
                    [this](const std::string &s) {
                        float freq = ::atof(s.c_str());
                        auto kb = Tunings::tuneA69To(freq);
                        kb.name = fmt::format("Note 69 Retuned 440 to {:.2f}", freq);

                        if (!this->synth->storage.remapToKeyboard(kb))
                        {
                            synth->storage.reportError("This .kbm file is not valid!",
                                                       "File Format Error");
                            return;
                        }
                        tuningChanged();
                    },
                    tuneStatus);
            });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Use MIDI Channel for Octave Shift"),
                              !synth->mpeEnabled, (synth->storage.mapChannelToOctave), [this]() {
                                  this->synth->storage.mapChannelToOctave =
                                      !(this->synth->storage.mapChannelToOctave);
                              });

        tuningSubMenu.addSeparator();

        tuningSubMenu.addItem(
            Surge::GUI::toOSCase("Apply Tuning at MIDI Input"), true,
            (synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_MIDI_ONLY), [this]() {
                this->synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
            });

        tuningSubMenu.addItem(
            Surge::GUI::toOSCase("Apply Tuning After Modulation"), true,
            (synth->storage.tuningApplicationMode == SurgeStorage::RETUNE_ALL),
            [this]() { this->synth->storage.setTuningApplicationMode(SurgeStorage::RETUNE_ALL); });

        tuningSubMenu.addSeparator();
    }

    if (synth->juceWrapperType.compare("Standalone") != 0)
    {
        bool tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                          Surge::Storage::UseODDMTS, false);
        std::string txt = "Use ODDSound" + Surge::GUI::toOSCase(" MTS-ESP");

        tuningSubMenu.addItem(txt, true, tsMode, [this, tsMode]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::UseODDMTS, !tsMode);
            if (tsMode)
            {
                this->synth->storage.deinitialize_oddsound();
            }
            else
            {
                this->synth->storage.initialize_oddsound();
            }
        });

        if (tsMode && !this->synth->storage.oddsound_mts_client)
        {
            tuningSubMenu.addItem(Surge::GUI::toOSCase("Reconnect to MTS-ESP"), [this]() {
                this->synth->storage.initialize_oddsound();
                this->synth->refresh_editor = true;
            });
        }

        if (this->synth->storage.oddsound_mts_active && this->synth->storage.oddsound_mts_client)
        {
            tuningSubMenu.addItem(Surge::GUI::toOSCase("Disconnect from MTS-ESP"), [this]() {
                auto q = this->synth->storage.oddsound_mts_client;
                this->synth->storage.oddsound_mts_active = false;
                this->synth->storage.oddsound_mts_client = nullptr;
                MTS_DeregisterClient(q);
            });

            tuningSubMenu.addSeparator();

            tuningSubMenu.addItem(
                Surge::GUI::toOSCase("Query Tuning at Note On Only"), true,
                (this->synth->storage.oddsoundRetuneMode == SurgeStorage::RETUNE_NOTE_ON_ONLY),
                [this]() {
                    if (this->synth->storage.oddsoundRetuneMode == SurgeStorage::RETUNE_CONSTANT)
                    {
                        this->synth->storage.oddsoundRetuneMode = SurgeStorage::RETUNE_NOTE_ON_ONLY;
                    }
                    else
                    {
                        this->synth->storage.oddsoundRetuneMode = SurgeStorage::RETUNE_CONSTANT;
                    }
                });

            return tuningSubMenu;
        }
    }

    return tuningSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeZoomMenu(const juce::Point<int> &where, bool showhelp)
{
    auto zoomSubMenu = juce::PopupMenu();

    auto hu = helpURLForSpecial("zoom-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        addHelpHeaderTo("Zoom", lurl, zoomSubMenu);

        zoomSubMenu.addSeparator();
    }

    bool isFixed = false;
    std::vector<int> zoomTos = {{100, 125, 150, 175, 200, 300, 400}};
    std::string lab;
    auto dzf =
        Surge::Storage::getUserDefaultValue(&(synth->storage), Surge::Storage::DefaultZoom, 100);

    if (currentSkin->hasFixedZooms())
    {
        isFixed = true;
        zoomTos = currentSkin->getFixedZooms();
    }

    for (auto s : zoomTos) // These are somewhat arbitrary reasonable defaults
    {
        lab = fmt::format("Zoom to {:d}%", s);
        bool ticked = (s == zoomFactor);
#if LINUX
        if (s == 150 && zoomFactor == 149)
            ticked = true;
#endif
        zoomSubMenu.addItem(lab, true, ticked, [this, s]() { resizeWindow(s); });
    }

    zoomSubMenu.addSeparator();

    if (isFixed)
    {
        /*
        DO WE WANT SOMETHING LIKE THIS?
        zoomSubMenu.addItem(Surge::GUI::toOSCase( "About fixed zoom skins..." ), []() {});
        */
    }
    else
    {
        // These are somewhat arbitrary reasonable defaults also
        std::vector<int> jog = {-25, -10, 10, 25};
        std::vector<std::string> sdesc = {
            showShortcutDescription("Shift + -", u8"\U000021E7-"), showShortcutDescription("-"),
            showShortcutDescription("+"), showShortcutDescription("Shift + +", u8"\U000021E7+")};

        for (int i = 0; i < jog.size(); i++)
        {
            if (jog[i] > 0)
            {
                lab = fmt::format("Grow by {:d}%", jog[i]);
            }
            else
            {
                lab = fmt::format("Shrink by {:d}%", -jog[i]);
            }

            Surge::GUI::addMenuWithShortcut(zoomSubMenu, lab, sdesc[i], [this, jog, i]() {
                resizeWindow(getZoomFactor() + jog[i]);
            });
        }

        zoomSubMenu.addSeparator();

        zoomSubMenu.addItem(Surge::GUI::toOSCase("Zoom to Largest"), [this]() {
            // regarding that 90 value, see comment in setZoomFactor
            int newZF = findLargestFittingZoomBetween(100.0, 500.0, 5, 90, getWindowSizeX(),
                                                      getWindowSizeY());
            resizeWindow(newZF);
        });

        zoomSubMenu.addItem(Surge::GUI::toOSCase("Zoom to Smallest"),
                            [this]() { resizeWindow(minimumZoom); });
    }

    if ((int)zoomFactor != dzf)
    {
        zoomSubMenu.addSeparator();

        if (dzf != 0)
        {
            lab = fmt::format("Zoom to Default ({:d}%)", dzf);

            Surge::GUI::addMenuWithShortcut(zoomSubMenu, Surge::GUI::toOSCase(lab),
                                            showShortcutDescription("Shift + /", u8"\U000021E7/"),
                                            [this, dzf]() { resizeWindow(dzf); });
        }

        lab = fmt::format("Set Current Zoom Level ({:d}%) as Default", (int)zoomFactor);

        zoomSubMenu.addItem(Surge::GUI::toOSCase(lab), [this]() {
            Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::DefaultZoom,
                                                   zoomFactor);
        });
    }

    if (!isFixed)
    {
        zoomSubMenu.addItem(Surge::GUI::toOSCase("Set Default Zoom Level to..."), [this, where]() {
            char c[256];
            snprintf(c, 256, "%d", (int)zoomFactor);
            promptForMiniEdit(
                c, "Enter a new value:", "Set Default Zoom Level", where,
                [this](const std::string &s) {
                    int newVal = ::atoi(s.c_str());
                    Surge::Storage::updateUserDefaultValue(&(synth->storage),
                                                           Surge::Storage::DefaultZoom, newVal);
                    resizeWindow(newVal);
                },
                zoomStatus);
        });
    }

    return zoomSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeMouseBehaviorMenu(const juce::Point<int> &where)
{
    bool touchMode = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                         Surge::Storage::TouchMouseMode, false);

    auto mouseMenu = juce::PopupMenu();

    std::string mouseLegacy = "Legacy";
    std::string mouseSlow = "Slow";
    std::string mouseMedium = "Medium";
    std::string mouseExact = "Exact";

    bool checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
                   Surge::Widgets::ModulatableSlider::kLegacy;
    bool enabled = !touchMode;

    mouseMenu.addItem(mouseLegacy, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kLegacy;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kSlow;

    mouseMenu.addItem(mouseSlow, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kSlow;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kMedium;

    mouseMenu.addItem(mouseMedium, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kMedium;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    checked = Surge::Widgets::ModulatableSlider::sliderMoveRateState ==
              Surge::Widgets::ModulatableSlider::kExact;

    mouseMenu.addItem(mouseExact, enabled, checked, [this]() {
        Surge::Widgets::ModulatableSlider::sliderMoveRateState =
            Surge::Widgets::ModulatableSlider::kExact;
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::SliderMoveRateState,
            Surge::Widgets::ModulatableSlider::sliderMoveRateState);
    });

    mouseMenu.addSeparator();

    bool tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::ShowCursorWhileEditing, true);

    mouseMenu.addItem(
        Surge::GUI::toOSCase("Show Cursor While Editing"), enabled, tsMode, [this, tsMode]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::ShowCursorWhileEditing, !tsMode);
        });

    mouseMenu.addSeparator();

    mouseMenu.addItem(Surge::GUI::toOSCase("Touchscreen Mode"), true, touchMode,
                      [this, touchMode]() {
                          Surge::Widgets::ModulatableSlider::touchscreenMode =
                              (!touchMode == false)
                                  ? Surge::Widgets::ModulatableSlider::TouchscreenMode::kDisabled
                                  : Surge::Widgets::ModulatableSlider::TouchscreenMode::kEnabled;
                          Surge::Storage::updateUserDefaultValue(
                              &(this->synth->storage), Surge::Storage::TouchMouseMode, !touchMode);
                      });

    mouseMenu.addSeparator();

    return mouseMenu;
}

juce::PopupMenu SurgeGUIEditor::makePatchDefaultsMenu(const juce::Point<int> &where)
{
    auto patchDefMenu = juce::PopupMenu();

    patchDefMenu.addItem(Surge::GUI::toOSCase("Set Default Patch Author..."), [this, where]() {
        string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchAuthor, "");
        char txt[256];
        txt[0] = 0;
        if (Surge::Storage::isValidUTF8(s))
            strxcpy(txt, s.c_str(), 256);
        promptForMiniEdit(
            txt, "Enter a default text:", "Set Default Patch Author", where,
            [this](const std::string &s) {
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchAuthor, s);
            },
            mainMenu);
    });

    patchDefMenu.addItem(Surge::GUI::toOSCase("Set Default Patch Comment..."), [this, where]() {
        string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchComment, "");
        char txt[256];
        txt[0] = 0;
        if (Surge::Storage::isValidUTF8(s))
            strxcpy(txt, s.c_str(), 256);
        promptForMiniEdit(
            txt, "Enter a default text:", "Set Default Patch Comment", where,
            [this](const std::string &s) {
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchComment, s);
            },
            mainMenu);
    });

    patchDefMenu.addSeparator();

    auto pscid = patchSelector ? patchSelector->getCurrentCategoryId() : -1;
    auto pspid = patchSelector ? patchSelector->getCurrentPatchId() : -1;
    auto s = &(this->synth->storage);

    if (pscid >= 0 && pscid < s->patch_category.size() && pspid >= 0 &&
        pspid < s->patch_list.size())
    {
        auto catCurId =
            &(this->synth->storage).patch_category[patchSelector->getCurrentCategoryId()].name;
        auto patchCurId =
            &(this->synth->storage).patch_list[patchSelector->getCurrentPatchId()].name;

        patchDefMenu.addItem(Surge::GUI::toOSCase("Set Current Patch as Default"), [this, catCurId,
                                                                                    patchCurId]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::InitialPatchName, *patchCurId);

            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::InitialPatchCategory, *catCurId);
        });
    }

    patchDefMenu.addSeparator();

    bool appendOGPatchBy = Surge::Storage::getUserDefaultValue(
        &(synth->storage), Surge::Storage::AppendOriginalPatchBy, true);

    patchDefMenu.addItem(Surge::GUI::toOSCase("Append Original Author to Modified Patches"), true,
                         appendOGPatchBy, [this, appendOGPatchBy]() {
                             Surge::Storage::updateUserDefaultValue(
                                 &(this->synth->storage), Surge::Storage::AppendOriginalPatchBy,
                                 !appendOGPatchBy);
                         });

    patchDefMenu.addSeparator();

    auto tuningOnLoadMenu = juce::PopupMenu();

    bool overrideTuningOnLoad = Surge::Storage::getUserDefaultValue(
        &(synth->storage), Surge::Storage::OverrideTuningOnPatchLoad, false);

    tuningOnLoadMenu.addItem(Surge::GUI::toOSCase("Keep Current Tuning"), true,
                             !overrideTuningOnLoad, [this, overrideTuningOnLoad]() {
                                 Surge::Storage::updateUserDefaultValue(
                                     &(this->synth->storage),
                                     Surge::Storage::OverrideTuningOnPatchLoad, false);
                             });

    tuningOnLoadMenu.addItem(Surge::GUI::toOSCase("Override With Embedded Tuning if Available"),
                             true, overrideTuningOnLoad, [this, overrideTuningOnLoad]() {
                                 Surge::Storage::updateUserDefaultValue(
                                     &(this->synth->storage),
                                     Surge::Storage::OverrideTuningOnPatchLoad, true);
                             });

    tuningOnLoadMenu.addSeparator();

    bool overrideMappingOnLoad = Surge::Storage::getUserDefaultValue(
        &(synth->storage), Surge::Storage::OverrideMappingOnPatchLoad, false);

    tuningOnLoadMenu.addItem(Surge::GUI::toOSCase("Keep Current Mapping"), true,
                             !overrideMappingOnLoad, [this, overrideMappingOnLoad]() {
                                 Surge::Storage::updateUserDefaultValue(
                                     &(this->synth->storage),
                                     Surge::Storage::OverrideMappingOnPatchLoad, false);
                             });

    tuningOnLoadMenu.addItem(Surge::GUI::toOSCase("Override With Embedded Mapping if Available"),
                             true, overrideMappingOnLoad, [this, overrideMappingOnLoad]() {
                                 Surge::Storage::updateUserDefaultValue(
                                     &(this->synth->storage),
                                     Surge::Storage::OverrideMappingOnPatchLoad, true);
                             });

    patchDefMenu.addSubMenu(Surge::GUI::toOSCase("Tuning on Patch Load"), tuningOnLoadMenu);

    return patchDefMenu;
}

juce::PopupMenu SurgeGUIEditor::makeValueDisplaysMenu(const juce::Point<int> &where)
{
    auto dispDefMenu = juce::PopupMenu();

    bool precReadout = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, false);

    dispDefMenu.addItem(Surge::GUI::toOSCase("High Precision Value Readouts"), true, precReadout,
                        [this, precReadout]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage), Surge::Storage::HighPrecisionReadouts,
                                !precReadout);
                        });

    // modulation value readout shows bounds
    bool modValues = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ModWindowShowsValues, false);

    dispDefMenu.addItem(Surge::GUI::toOSCase("Modulation Value Readout Shows Bounds"), true,
                        modValues, [this, modValues]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage), Surge::Storage::ModWindowShowsValues,
                                !modValues);
                        });

    bool infowi = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                      Surge::Storage::InfoWindowPopupOnIdle, true);

    dispDefMenu.addItem(
        Surge::GUI::toOSCase("Show Value Readout on Mouse Hover"), true, infowi, [this, infowi]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::InfoWindowPopupOnIdle, !infowi);
            this->frame->repaint();
        });

    dispDefMenu.addSeparator();

    bool lfoone = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ShowGhostedLFOWaveReference, true);

    dispDefMenu.addItem(Surge::GUI::toOSCase("Show Ghosted LFO Waveform Reference"), true, lfoone,
                        [this, lfoone]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage),
                                Surge::Storage::ShowGhostedLFOWaveReference, !lfoone);
                            this->frame->repaint();
                        });

    dispDefMenu.addSeparator();

    bool cpumeter = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                        Surge::Storage::ShowCPUUsage, false);

    dispDefMenu.addItem(Surge::GUI::toOSCase("Show CPU Usage in VU Meter"), true, cpumeter,
                        [this, cpumeter]() {
                            Surge::Storage::updateUserDefaultValue(
                                &(this->synth->storage), Surge::Storage::ShowCPUUsage, !cpumeter);
                            this->frame->repaint();
                        });

    dispDefMenu.addSeparator();

    // Middle C submenu
    auto middleCSubMenu = juce::PopupMenu();

    auto mcValue =
        Surge::Storage::getUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 1);

    middleCSubMenu.addItem("C3", true, (mcValue == 2), [this, mcValue]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 2);
        juceEditor->keyboard->setOctaveForMiddleC(3);
        synth->refresh_editor = true;
    });

    middleCSubMenu.addItem("C4", true, (mcValue == 1), [this, mcValue]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 1);
        juceEditor->keyboard->setOctaveForMiddleC(4);
        synth->refresh_editor = true;
    });

    middleCSubMenu.addItem("C5", true, (mcValue == 0), [this, mcValue]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), Surge::Storage::MiddleC, 0);
        juceEditor->keyboard->setOctaveForMiddleC(5);
        synth->refresh_editor = true;
    });

    dispDefMenu.addSubMenu("Middle C", middleCSubMenu);

    return dispDefMenu;
}

juce::PopupMenu SurgeGUIEditor::makeWorkflowMenu(const juce::Point<int> &where)
{
    auto wfMenu = juce::PopupMenu();

    bool tabPosMem = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene, false);

    wfMenu.addItem(Surge::GUI::toOSCase("Remember Tab Positions Per Scene"), true, tabPosMem,
                   [this, tabPosMem]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::RememberTabPositionsPerScene,
                           !tabPosMem);
                   });

    bool msegSnapMem = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RestoreMSEGSnapFromPatch, true);

    wfMenu.addItem(Surge::GUI::toOSCase("Load MSEG Snap State from Patch"), true, msegSnapMem,
                   [this, msegSnapMem]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::RestoreMSEGSnapFromPatch,
                           !msegSnapMem);
                   });

    wfMenu.addSeparator();

    bool patchJogWrap = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::PatchJogWraparound, true);

    wfMenu.addItem(Surge::GUI::toOSCase("Previous/Next Patch Constrained to Current Category"),
                   true, patchJogWrap, [this, patchJogWrap]() {
                       Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                              Surge::Storage::PatchJogWraparound,
                                                              !patchJogWrap);
                   });

    bool patchStickySearchbox = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::RetainPatchSearchboxAfterLoad, true);

    wfMenu.addItem(Surge::GUI::toOSCase("Retain Patch Search Results After Loading"), true,
                   patchStickySearchbox, [this, patchStickySearchbox]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::RetainPatchSearchboxAfterLoad,
                           !patchStickySearchbox);
                   });

    int patchDirtyCheck = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::PromptToLoadOverDirtyPatch, ALWAYS);

    wfMenu.addItem(Surge::GUI::toOSCase("Confirm Patch Loading if Unsaved Changes Exist"), true,
                   (patchDirtyCheck != ALWAYS), [this, patchDirtyCheck]() {
                       int newVal = (patchDirtyCheck != ALWAYS) ? ALWAYS : DUNNO;

                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::PromptToLoadOverDirtyPatch,
                           newVal);
                   });

    wfMenu.addSeparator();
    /*  // TODO: remove completely in XT2
        bool tabArm = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                          Surge::Storage::TabKeyArmsModulators,
       false);

        wfMenu.addItem(Surge::GUI::toOSCase("Tab Key Arms Modulators"), true, tabArm, [this,
       tabArm]() { Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   Surge::Storage::TabKeyArmsModulators, !tabArm);
        });
     */
    bool kbShortcuts = getUseKeyboardShortcuts();

    wfMenu.addItem(Surge::GUI::toOSCase("Use Keyboard Shortcuts"), true, kbShortcuts,
                   [this]() { toggleUseKeyboardShortcuts(); });
    Surge::GUI::addMenuWithShortcut(wfMenu, Surge::GUI::toOSCase("Edit Keyboard Shortcuts..."),
                                    showShortcutDescription("Alt + B", u8"\U00002325B"), true,
                                    false, [this]() { toggleOverlay(KEYBINDINGS_EDITOR); });

    bool knMode = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::MenuAndEditKeybindingsFollowKeyboardFocus, true);

    auto kbfMenu = juce::PopupMenu();

    kbfMenu.addItem(Surge::GUI::toOSCase("Follow Keyboard Focus"), true, knMode, [this]() {
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::MenuAndEditKeybindingsFollowKeyboardFocus,
            true);
    });

    kbfMenu.addItem(Surge::GUI::toOSCase("Follow Mouse Hover Focus"), true, !knMode, [this]() {
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::MenuAndEditKeybindingsFollowKeyboardFocus,
            false);
    });

    std::string str = "Shift + F10 and " + Surge::GUI::toOSCase("Edit Parameter Value Shortcuts");

    wfMenu.addSubMenu(str, kbfMenu);

    wfMenu.addSeparator();

    bool doAccAnn = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseNarratorAnnouncements, false);

    wfMenu.addItem(Surge::GUI::toOSCase("Send Additional Accessibility Announcements"), true,
                   doAccAnn, [this, doAccAnn]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::UseNarratorAnnouncements,
                           !doAccAnn);
                   });

#if WINDOWS
    bool doAccAnnPatch = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead, true);

    wfMenu.addItem("Announce Patch Browser entries", true, doAccAnnPatch, [this, doAccAnnPatch]() {
        Surge::Storage::updateUserDefaultValue(
            &(this->synth->storage), Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead,
            !doAccAnnPatch);
    });
#endif

    bool doExpMen = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ExpandModMenusWithSubMenus, false);

    wfMenu.addItem(Surge::GUI::toOSCase("Add Sub-Menus for Modulation Menu Items"), true, doExpMen,
                   [this, doExpMen]() {
                       Surge::Storage::updateUserDefaultValue(
                           &(this->synth->storage), Surge::Storage::ExpandModMenusWithSubMenus,
                           !doExpMen);
                   });

    wfMenu.addSeparator();

    bool showVirtualKeyboard = getShowVirtualKeyboard();

    Surge::GUI::addMenuWithShortcut(wfMenu, Surge::GUI::toOSCase("Show Virtual Keyboard"),
                                    showShortcutDescription("Alt + K", u8"\U00002325K"), true,
                                    showVirtualKeyboard, [this]() { toggleVirtualKeyboard(); });

    bool showOscilloscope = isAnyOverlayPresent(OSCILLOSCOPE);

    Surge::GUI::addMenuWithShortcut(wfMenu, Surge::GUI::toOSCase("Open Oscilloscope"),
                                    showShortcutDescription("Alt + O", u8"\U00002325O"), true,
                                    showOscilloscope, [this]() { toggleOverlay(OSCILLOSCOPE); });

    return wfMenu;
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
}

void SurgeGUIEditor::toggleUseKeyboardShortcuts()
{
    setUseKeyboardShortcuts(!getUseKeyboardShortcuts());
}

juce::PopupMenu SurgeGUIEditor::makeSkinMenu(const juce::Point<int> &where)
{
    auto skinSubMenu = juce::PopupMenu();

    auto db = Surge::GUI::SkinDB::get();
    bool hasTests = false;

    // TODO: Later allow nesting
    std::map<std::string, std::vector<Surge::GUI::SkinDB::Entry>> entryByCategory;

    for (auto &entry : db->getAvailableSkins())
    {
        entryByCategory[entry.category].push_back(entry);
    }

    for (auto pr : entryByCategory)
    {
        auto catMen = juce::PopupMenu();
        auto addToThis = &skinSubMenu;
        auto cat = pr.first;

        if (cat != "")
        {
            addToThis = &catMen;
        }

        for (auto &entry : pr.second)
        {
            auto dname = entry.displayName;

            if (useDevMenu)
            {
                dname += " (";

                if (entry.rootType == Surge::GUI::FACTORY)
                {
                    dname += "Factory";
                }
                else if (entry.rootType == Surge::GUI::USER)
                {
                    dname += "User";
                }
                else if (entry.rootType == Surge::GUI::MEMORY)
                {
                    dname += "Internal";
                }
                else
                {
                    dname += "UNKNOWN";
                }

                dname += PATH_SEPARATOR + entry.name + ")";
            }

            auto checked = entry.matchesSkin(currentSkin);

            addToThis->addItem(dname, true, checked, [this, entry]() {
                setupSkinFromEntry(entry);
                this->synth->refresh_editor = true;
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultSkin, entry.name);
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::DefaultSkinRootType, entry.rootType);
            });
        }

        if (cat != "")
        {
            skinSubMenu.addSubMenu(cat, *addToThis);
        }
    }

    skinSubMenu.addSeparator();

    if (useDevMenu)
    {
        int pxres = Surge::Storage::getUserDefaultValue(&(synth->storage),
                                                        Surge::Storage::LayoutGridResolution, 20);

        auto m = std::string("Show Layout Grid (") + std::to_string(pxres) + " px)";

        skinSubMenu.addItem(Surge::GUI::toOSCase(m),
                            [this, pxres]() { this->showAboutScreen(pxres); });

        skinSubMenu.addItem(
            Surge::GUI::toOSCase("Change Layout Grid Resolution..."), [this, pxres]() {
                this->promptForMiniEdit(
                    std::to_string(pxres), "Enter a new value:", "Layout Grid Resolution",
                    juce::Point<int>{400, 400},
                    [this](const std::string &s) {
                        auto val = std::atoi(s.c_str());

                        if (val < 4)
                        {
                            val = 4;
                        }

                        Surge::Storage::updateUserDefaultValue(
                            &(this->synth->storage), Surge::Storage::LayoutGridResolution, val);
                    },
                    mainMenu);
            });

        skinSubMenu.addSeparator();
    }

    Surge::GUI::addMenuWithShortcut(skinSubMenu, Surge::GUI::toOSCase("Reload Current Skin"),
                                    showShortcutDescription("F5"), [this]() { refreshSkin(); });

    skinSubMenu.addItem(Surge::GUI::toOSCase("Rescan Skins"), [this]() {
        auto r = this->currentSkin->root;
        auto n = this->currentSkin->name;

        auto *db = Surge::GUI::SkinDB::get();
        db->rescanForSkins(&(this->synth->storage));

        // So go find the skin
        auto e = db->getEntryByRootAndName(r, n);
        if (e.has_value())
        {
            setupSkinFromEntry(*e);
        }
        else
        {
            setupSkinFromEntry(db->getDefaultSkinEntry());
        }
        this->synth->refresh_editor = true;
    });

    skinSubMenu.addSeparator();
    auto menuMode =
        Surge::Storage::getUserDefaultValue(&(synth->storage), Surge::Storage::MenuLightness, 2);
    auto resetMenuTo = [this](int i) {
        Surge::Storage::updateUserDefaultValue(&(synth->storage), Surge::Storage::MenuLightness, i);
        juceEditor->surgeLF->onSkinChanged();
    };

    skinSubMenu.addSeparator();

    if (useDevMenu)
    {
        skinSubMenu.addItem(Surge::GUI::toOSCase("Open Current Skin Folder..."), [this]() {
            Surge::GUI::openFileOrFolder(string_to_path(this->currentSkin->root) /
                                         this->currentSkin->name);
        });
    }
    else
    {
        skinSubMenu.addItem(Surge::GUI::toOSCase("Install a New Skin..."), [this]() {
            Surge::GUI::openFileOrFolder(this->synth->storage.userSkinsPath);
        });
    }

    skinSubMenu.addSeparator();

    skinSubMenu.addItem(Surge::GUI::toOSCase("Menu Colors Follow OS Light/Dark Mode"), true,
                        menuMode == 1, [resetMenuTo]() { resetMenuTo(1); });
    skinSubMenu.addItem(Surge::GUI::toOSCase("Menu Colors Applied from Skin"), true, menuMode == 2,
                        [resetMenuTo]() { resetMenuTo(2); });

    skinSubMenu.addSeparator();

    skinSubMenu.addItem(Surge::GUI::toOSCase("Show Skin Inspector..."),
                        [this]() { showHTML(skinInspectorHtml()); });

    skinSubMenu.addItem(Surge::GUI::toOSCase("Skin Development Guide..."), []() {
        juce::URL("https://surge-synthesizer.github.io/skin-manual.html").launchInDefaultBrowser();
    });

    return skinSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeDataMenu(const juce::Point<int> &where)
{
    auto dataSubMenu = juce::PopupMenu();

    dataSubMenu.addItem(Surge::GUI::toOSCase("Open Factory Data Folder..."),
                        [this]() { Surge::GUI::openFileOrFolder(this->synth->storage.datapath); });

    dataSubMenu.addItem(Surge::GUI::toOSCase("Open User Data Folder..."), [this]() {
        // make it if it isn't there
        fs::create_directories(this->synth->storage.userDataPath);
        Surge::GUI::openFileOrFolder(this->synth->storage.userDataPath);
    });

    dataSubMenu.addItem(Surge::GUI::toOSCase("Set Custom User Data Folder..."), [this]() {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Set Custom User Data Folder", juce::File(path_to_string(synth->storage.userDataPath)));
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [this](const juce::FileChooser &f) {
                auto r = f.getResult();
                if (!r.isDirectory())
                    return;
                auto s = f.getResult().getFullPathName().toStdString();

                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::UserDataPath, s);

                this->synth->storage.userDataPath = s;
                synth->storage.createUserDirectory();

                this->synth->storage.refresh_wtlist();
                this->synth->storage.refresh_patchlist();
            });
    });

    dataSubMenu.addSeparator();

    dataSubMenu.addItem(Surge::GUI::toOSCase("Rescan All Data Folders"), [this]() {
        this->synth->storage.refresh_wtlist();
        this->synth->storage.refresh_patchlist();
        this->scannedForMidiPresets = false;

        this->synth->storage.fxUserPreset->doPresetRescan(&(this->synth->storage), true);
        this->synth->storage.modulatorPreset->forcePresetRescan();

        // Rescan for skins
        auto r = this->currentSkin->root;
        auto n = this->currentSkin->name;

        auto *db = Surge::GUI::SkinDB::get();
        db->rescanForSkins(&(this->synth->storage));

        // So go find the skin
        auto e = db->getEntryByRootAndName(r, n);

        if (e.has_value())
        {
            setupSkinFromEntry(*e);
        }
        else
        {
            setupSkinFromEntry(db->getDefaultSkinEntry());
        }

        // Will need to rebuild the FX menu also so...
        this->synth->refresh_editor = true;
    });

    return dataSubMenu;
}

// builds a menu for setting controller smoothing, used in ::makeMidiMenu and ::makeMpeMenu
// key is the key given to getUserDefaultValue,
// default is a value to default to,
// setSmooth is a function called to set the smoothing value
juce::PopupMenu
SurgeGUIEditor::makeSmoothMenu(const juce::Point<int> &where, const Surge::Storage::DefaultKey &key,
                               int defaultValue,
                               std::function<void(Modulator::SmoothingMode)> setSmooth)
{
    auto smoothMenu = juce::PopupMenu();

    int smoothing = Surge::Storage::getUserDefaultValue(&(synth->storage), key, defaultValue);

    auto asmt = [&smoothMenu, smoothing, setSmooth](const char *label,
                                                    Modulator::SmoothingMode md) {
        smoothMenu.addItem(Surge::GUI::toOSCase(label), true, (smoothing == md),
                           [setSmooth, md]() { setSmooth(md); });
    };

    asmt("Legacy", Modulator::SmoothingMode::LEGACY);
    asmt("Slow Exponential", Modulator::SmoothingMode::SLOW_EXP);
    asmt("Fast Exponential", Modulator::SmoothingMode::FAST_EXP);
    asmt("Fast Linear", Modulator::SmoothingMode::FAST_LINE);
    asmt("No Smoothing", Modulator::SmoothingMode::DIRECT);

    return smoothMenu;
};

juce::PopupMenu SurgeGUIEditor::makeMidiMenu(const juce::Point<int> &where)
{
    auto midiSubMenu = juce::PopupMenu();

    auto smen =
        makeSmoothMenu(where, Surge::Storage::SmoothingMode, (int)Modulator::SmoothingMode::LEGACY,
                       [this](auto md) { this->resetSmoothing(md); });
    midiSubMenu.addSubMenu(Surge::GUI::toOSCase("Controller Smoothing"), smen);

    auto mmom = makeMonoModeOptionsMenu(where, true);
    midiSubMenu.addSubMenu(Surge::GUI::toOSCase("Sustain Pedal In Mono Mode"), mmom);

    bool useMIDICh2Ch3 = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseCh2Ch3ToPlayScenesIndividually, true);

    midiSubMenu.addItem(
        Surge::GUI::toOSCase("Use MIDI Channels 2 and 3 to Play Scenes Individually"), true,
        useMIDICh2Ch3, [this, useMIDICh2Ch3]() {
            Surge::Storage::updateUserDefaultValue(
                &(this->synth->storage), Surge::Storage::UseCh2Ch3ToPlayScenesIndividually,
                !useMIDICh2Ch3);
        });

    midiSubMenu.addSeparator();

    midiSubMenu.addItem(Surge::GUI::toOSCase("Save MIDI Mapping As..."), [this, where]() {
        this->scannedForMidiPresets = false; // force a rescan
        char msn[256];
        msn[0] = 0;
        promptForMiniEdit(
            msn, "Enter the preset name:", "Save MIDI Mapping", where,
            [this](const std::string &s) { this->synth->storage.storeMidiMappingToName(s); },
            mainMenu);
    });

    midiSubMenu.addItem(Surge::GUI::toOSCase("Set Current MIDI Mapping as Default"), [this]() {
        this->synth->storage.write_midi_controllers_to_user_default();
    });

    midiSubMenu.addItem(Surge::GUI::toOSCase("Clear Current MIDI Mapping"), [this]() {
        int n = n_global_params + n_scene_params;

        for (int i = 0; i < n; i++)
        {
            this->synth->storage.getPatch().param_ptr[i]->midictrl = -1;
            this->synth->storage.getPatch().dawExtraState.midictrl_map[i] = -1;

            if (i > n_global_params)
            {
                this->synth->storage.getPatch().param_ptr[i + n_scene_params]->midictrl = -1;
                this->synth->storage.getPatch().dawExtraState.midictrl_map[i + n_scene_params] = -1;
            }
        }

        for (int i = 0; i < n_customcontrollers; i++)
        {
            this->synth->storage.controllers[i] = -1;
            this->synth->storage.getPatch().dawExtraState.customcontrol_map[i] = -1;
        }
    });

    midiSubMenu.addSeparator();

    midiSubMenu.addItem(Surge::GUI::toOSCase("Show Current MIDI Mapping..."),
                        [this]() { showHTML(this->midiMappingToHtml()); });

    if (!scannedForMidiPresets)
    {
        scannedForMidiPresets = true;
        synth->storage.rescanUserMidiMappings();
    }

    bool gotOne = false;

    for (const auto &p : synth->storage.userMidiMappingsXMLByName)
    {
        if (!gotOne)
        {
            gotOne = true;
            midiSubMenu.addSeparator();
        }

        midiSubMenu.addItem(p.first,
                            [this, p] { this->synth->storage.loadMidiMappingByName(p.first); });
    }

    return midiSubMenu;
}

void SurgeGUIEditor::reloadFromSkin()
{
    if (!frame || !bitmapStore.get())
        return;

    juceEditor->surgeLF->setSkin(currentSkin, bitmapStore);

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
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        bool tornOut = false;
        juce::Point<int> tearOutPos;
        auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);

        if (olw && olw->isTornOut())
        {
            tornOut = true;
            tearOutPos = olw->currentTearOutLocation();
        }

        showOverlay(SurgeGUIEditor::MSEG_EDITOR);

        if (tornOut)
        {
            auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);

            if (olw)
            {
                olw->doTearOut(tearOutPos);
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

juce::PopupMenu SurgeGUIEditor::makeDevMenu(const juce::Point<int> &where)
{
    auto devSubMenu = juce::PopupMenu();

#if WINDOWS
    Surge::GUI::addMenuWithShortcut(devSubMenu, "Show debug console...",
                                    showShortcutDescription("Alt + D", u8"\U00002325D"),
                                    []() { Surge::Debug::toggleConsole(); });
#endif

    devSubMenu.addItem(Surge::GUI::toOSCase("Use Focus Debugger"), true, debugFocus, [this]() {
        debugFocus = !debugFocus;
        frame->debugFocus = debugFocus;
        frame->repaint();
    });

    devSubMenu.addItem(Surge::GUI::toOSCase("Dump Undo/Redo Stack To stdout"), true, false,
                       [this]() { undoManager()->dumpStack(); });

#ifdef INSTRUMENT_UI
    devSubMenu.addItem(Surge::GUI::toOSCase("Show UI Instrumentation..."),
                       []() { Surge::Debug::report(); });
#endif

    return devSubMenu;
}

int SurgeGUIEditor::findLargestFittingZoomBetween(
    int zoomLow,                     // bottom of range
    int zoomHigh,                    // top of range
    int zoomQuanta,                  // step size
    int percentageOfScreenAvailable, // How much to shrink actual screen
    float baseW, float baseH)
{
    // Here is a very crude implementation
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

    jassert(c);

    if (p)
    {
        if (!p->can_setvalue_from_string())
        {
            return;
        }
    }

    if (p)
    {
        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Param);
    }
    else
    {
        typeinParamEditor->setTypeinMode(Surge::Overlays::TypeinParamEditor::Control);
    }

    std::string lab = "";
    if (p)
    {
        if (p->ctrlgroup == cg_LFO)
        {
            char pname[1024];
            p->create_fullname(p->get_name(), pname, p->ctrlgroup, p->ctrlgroup_entry,
                               modulatorName(p->ctrlgroup_entry, true).c_str());
            lab = pname;
        }
        else
        {
            lab = p->get_full_name();
        }
    }
    else
    {
        lab = modulatorName(ms, false);
    }

    typeinParamEditor->setMainLabel(lab);

    char txt[256];
    char ptext[1024], ptext2[1024];
    ptext2[0] = 0;
    if (p)
    {
        if (ismod)
        {
            char txt2[256];

            p->get_display_of_modulation_depth(
                txt, synth->getModDepth(p->id, (modsources)ms, modScene, modidx),
                synth->isBipolarModulation((modsources)ms), Parameter::TypeIn);
            p->get_display(txt2);

            sprintf(ptext, "current: %s", txt2);
            sprintf(ptext2, "mod: %s", txt);
        }
        else
        {
            p->get_display(txt);
            sprintf(ptext, "current: %s", txt);
        }
    }
    else
    {
        int detailedMode = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::HighPrecisionReadouts, 0);
        auto cms = ((ControllerModulationSource *)synth->storage.getPatch()
                        .scene[current_scene]
                        .modsources[ms]);

        sprintf(ptext, "current: %s", txt);
        sprintf(txt, "%.*f %%", (detailedMode ? 6 : 2), 100.0 * cms->get_output(0));
    }

    typeinParamEditor->setValueLabels(ptext, ptext2);
    typeinParamEditor->setEditableText(txt);

    if (ismod)
    {
        std::string mls =
            std::string("by ") + modulatorNameWithIndex(current_scene, ms, modidx, true, false);
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

std::string SurgeGUIEditor::modulatorName(int i, bool button, int forScene)
{
    if ((i >= ms_lfo1 && i <= ms_slfo6))
    {
        int idx = i - ms_lfo1;
        bool isS = idx >= 6;
        int fnum = idx % 6;
        auto useScene = forScene >= 0 ? forScene : current_scene;
        auto *lfodata = &(synth->storage.getPatch().scene[useScene].lfo[i - ms_lfo1]);

        char sceneN[5], shortsceneS[5];
        sceneN[0] = 0;
        shortsceneS[0] = 0;
        if (forScene >= 0)
        {
            sceneN[0] = ' ';
            sceneN[1] = 'A' + forScene;
            sceneN[2] = 0;
            shortsceneS[0] = 'A' + forScene;
            shortsceneS[1] = ' ';
            shortsceneS[2] = 0;
        }
        char sceneL[16];
        snprintf(sceneL, 16, "Scene%s", sceneN);
        char shortsceneL[16];
        snprintf(shortsceneL, 16, "%sS-", shortsceneS);

        if (lfodata->shape.val.i == lt_envelope)
        {
            char txt[128];
            if (button)
                sprintf(txt, "%sENV %d", (isS ? shortsceneL : ""), fnum + 1);
            else
                sprintf(txt, "%s Envelope %d", (isS ? sceneL : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_stepseq)
        {
            char txt[128];
            if (button)
                sprintf(txt, "%sSEQ %d", (isS ? shortsceneL : ""), fnum + 1);
            else
                sprintf(txt, "%s Step Sequencer %d", (isS ? sceneL : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_mseg)
        {
            char txt[128];
            if (button)
                sprintf(txt, "%sMSEG %d", (isS ? shortsceneL : ""), fnum + 1);
            else
                sprintf(txt, "%s MSEG %d", (isS ? sceneL : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else if (lfodata->shape.val.i == lt_formula)
        {
            char txt[128];

            if (button)
                sprintf(txt, "%sFORM %d", (isS ? shortsceneL : ""), fnum + 1);
            else
                sprintf(txt, "%s Formula %d", (isS ? sceneL : "Voice"), fnum + 1);
            return std::string(txt);
        }
        else
        {
            char txt[128];
            if (button)
                sprintf(txt, "%sLFO %d", (isS ? shortsceneL : ""), fnum + 1);
            else
                sprintf(txt, "%s LFO %d", (isS ? sceneL : "Voice"), fnum + 1);
            return std::string(txt);
        }
    }

    if (i >= ms_ctrl1 && i <= ms_ctrl8)
    {
        if (button)
        {
            std::string ccl =
                std::string(synth->storage.getPatch().CustomControllerLabel[i - ms_ctrl1]);
            if (ccl == "-")
            {
                return std::string(modsource_names[i]);
            }
            else
            {
                return ccl;
            }
        }
        else
        {
            std::string ccl =
                std::string(synth->storage.getPatch().CustomControllerLabel[i - ms_ctrl1]);
            if (ccl == "-")
            {
                return std::string(modsource_names[i]);
            }
            else
            {
                return ccl + " (" + modsource_names[i] + ")";
            }
        }
    }

    if (i == ms_aftertouch && synth->mpeEnabled)
    {
        return "MPE Pressure";
    }
    if (i == ms_timbre && synth->mpeEnabled)
    {
        return "MPE Timbre";
    }
    if (button)
        return std::string(modsource_names_button[i]);
    else
        return std::string(modsource_names[i]);
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
            << " skin! Reverting the skin to Surge Classic.\n\nSkin Error:\n"
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
            char txt[1024];
            p->get_display(txt, external, f);
            return txt;
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
                    modulatorNameWithIndex(current_scene, modsource, msi, false, false, true)),
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

        hs->setBounds(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs, styleW, styleH);
        hs->setTag(tag);
        hs->addListener(this);
        hs->setStorage(&(this->synth->storage));
        hs->setSkin(currentSkin, bitmapStore, skinCtrl);
        hs->setMoveRate(p->moverate);

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
                hs->setFont(juce::Font(currentSkin->typeFaces[ff]).withPointHeight(fs));
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
            auto drgb = currentSkin->propertyValue(skinCtrl,
                                                   Surge::Skin::Component::DRAGGABLE_HSWITCH, "1");
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
        strxcpy(synth->storage.getPatch()
                    .scene[current_scene]
                    .osc[current_osc[current_scene]]
                    .wt.queue_filename,
                fname.c_str(), 255);
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
        auto cb = juce::ModalCallbackFunction::create([this, fPath](int okcs) {
            if (okcs)
            {
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
            }
        });
        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::NoIcon, "Install Skin", oss.str(),
                                           "Yes", "No", nullptr, cb);
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

        auto cb = juce::ModalCallbackFunction::create([this, zipHandler](int okcs) {
            if (okcs)
            {
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
            }
        });

        juce::AlertWindow::showOkCancelBox(juce::AlertWindow::NoIcon, "Install from ZIP archive",
                                           oss.str(), "Yes", "No", nullptr, cb);
    }
    return true;
}

void SurgeGUIEditor::enqueueFXChainClear(int fxchain)
{
    int fxSlotOrder[n_fx_slots] = {fxslot_ains1,   fxslot_ains2,   fxslot_ains3,   fxslot_ains4,
                                   fxslot_bins1,   fxslot_bins2,   fxslot_bins3,   fxslot_bins4,
                                   fxslot_send1,   fxslot_send2,   fxslot_send3,   fxslot_send4,
                                   fxslot_global1, fxslot_global2, fxslot_global3, fxslot_global4};

    for (int i = 0; i < n_fx_slots; i++)
    {
        if (fxchain == -1 || (fxchain >= 0 && (i >= (fxchain * 4) && i < ((fxchain + 1) * 4))))
        {
            synth->enqueueFXOff(fxSlotOrder[i]);
        }
    }
}

void SurgeGUIEditor::swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m)
{
    if (source < 0 || source >= n_fx_slots || target < 0 || target >= n_fx_slots)
        return;

    auto t = fxPresetName[target];
    fxPresetName[target] = fxPresetName[source];
    if (m == SurgeSynthesizer::SWAP)
        fxPresetName[source] = t;
    if (m == SurgeSynthesizer::MOVE)
        fxPresetName[source] = "";

    synth->reorderFx(source, target, m);
    // the effect chooser caches the bitmask so reset it after a swap
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
    if (isAnyOverlayPresent(MSEG_EDITOR))
    {
        auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);
        if (olw && olw->isTornOut())
        {
            isTornOut = true;
            tearOutPos = olw->currentTearOutLocation();
        }
        closeOverlay(SurgeGUIEditor::MSEG_EDITOR);
        hadExtendedEditor = true;
    }
    if (isAnyOverlayPresent(FORMULA_EDITOR))
    {
        auto olw = getOverlayWrapperIfOpen(FORMULA_EDITOR);
        if (olw && olw->isTornOut())
        {
            isTornOut = true;
            tearOutPos = olw->currentTearOutLocation();
        }
        closeOverlay(FORMULA_EDITOR);
        hadExtendedEditor = true;
    }

    if (hadExtendedEditor)
    {
        if (curr == lt_mseg)
        {
            showOverlay(SurgeGUIEditor::MSEG_EDITOR);
            if (isTornOut)
            {
                auto olw = getOverlayWrapperIfOpen(MSEG_EDITOR);
                if (olw)
                    olw->doTearOut(tearOutPos);
            }
        }
        if (curr == lt_formula)
        {
            showOverlay(FORMULA_EDITOR);
            if (isTornOut)
            {
                auto olw = getOverlayWrapperIfOpen(FORMULA_EDITOR);
                if (olw)
                    olw->doTearOut(tearOutPos);
            }
        }
    }

    // update the LFO title label
    std::string modname = modulatorName(modsource_editor[current_scene], true);
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
 * The edit state is independent per LFO. We want to sync some of ti as if it is not
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

void SurgeGUIEditor::onSurgeError(const string &msg, const string &title)
{
    std::lock_guard<std::mutex> g(errorItemsMutex);
    errorItems.emplace_back(msg, title);
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
                                                                 const std::string &title,
                                                                 const std::string &action)
{
#if MAC
    c->setDescription(title);
    c->setTitle(title);
#else
    c->setDescription(action);
    c->setTitle(title);
#endif
}

std::string SurgeGUIEditor::modulatorIndexExtension(int scene, int ms, int index, bool shortV)
{
    if (synth->supportsIndexedModulator(scene, (modsources)ms))
    {
        if (ms == ms_random_bipolar)
        {
            if (index == 0)
                return shortV ? "" : " (Uniform)";
            if (index == 1)
                return shortV ? " N" : " (Normal)";
        }
        if (ms == ms_random_unipolar)
        {
            if (index == 0)
                return shortV ? "" : " (Uniform)";
            if (index == 1)
                return shortV ? " HN" : " (Half Normal)";
        }
        if (ms == ms_lowest_key || ms == ms_latest_key || ms == ms_highest_key)
        {
            return (index == 0 ? " Key" : " Voice");
        }

        if (ms >= ms_lfo1 && ms <= ms_slfo6)
        {
            auto lf = &(synth->storage.getPatch().scene[scene].lfo[ms - ms_lfo1]);
            if (lf->shape.val.i != lt_formula)
            {
                if (index == 0)
                    return "";
                if (index == 1)
                {
                    if (shortV)
                        return " Raw";
                    return " (" + Surge::GUI::toOSCase("Raw Waveform") + ")";
                }
                if (index == 2)
                {
                    if (shortV)
                        return " EG";
                    return Surge::GUI::toOSCase(" (EG Only)");
                }
            }
        }

        if (shortV)
            return "." + std::to_string(index + 1);
        else
            return std::string(" Out ") + std::to_string(index + 1);
    }
    return "";
}

std::string SurgeGUIEditor::modulatorNameWithIndex(int scene, int ms, int index, bool forButton,
                                                   bool useScene, bool baseNameOnly)
{
    int lfo_id = ms - ms_lfo1;
    bool hasOverride = isLFO((modsources)ms) && index >= 0 &&
                       synth->storage.getPatch().LFOBankLabel[scene][lfo_id][index][0] != 0;

    if (baseNameOnly)
    {
        auto base = modulatorName(ms, true, useScene ? scene : -1);

        if (synth->supportsIndexedModulator(scene, (modsources)ms))
        {
            base += modulatorIndexExtension(scene, ms, index, true);
        }

        return base;
    }

    if (!hasOverride)
    {
        auto base = modulatorName(ms, forButton, useScene ? scene : -1);

        if (index >= 0 && synth->supportsIndexedModulator(scene, (modsources)ms))
        {
            base += modulatorIndexExtension(scene, ms, index, forButton);
        }

        return base;
    }
    else
    {
        if (forButton)
        {
            return synth->storage.getPatch().LFOBankLabel[scene][ms - ms_lfo1][index];
        }

        // Long name is alias (button name)
        auto base = modulatorName(ms, true, useScene ? scene : -1);

        if (synth->supportsIndexedModulator(scene, (modsources)ms))
        {
            base += modulatorIndexExtension(scene, ms, index, true);
        }

        std::string res = synth->storage.getPatch().LFOBankLabel[scene][ms - ms_lfo1][index];
        res = res + " (" + base + ")";
        return res;
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
            auto tl = modulatorNameWithIndex(current_scene, a, q, true, false);
            auto ll = modulatorNameWithIndex(current_scene, a, q, false, false);
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

    keyMapManager->addBinding(Surge::GUI::FOCUS_NEXT_CONTROL_GROUP,
                              {keymap_t::Modifiers::ALT, (int)'.'});
    keyMapManager->addBinding(Surge::GUI::FOCUS_PRIOR_CONTROL_GROUP,
                              {keymap_t::Modifiers::ALT, (int)','});

    keyMapManager->addBinding(Surge::GUI::REFRESH_SKIN, {juce::KeyPress::F5Key});
    keyMapManager->addBinding(Surge::GUI::SKIN_LAYOUT_GRID, {keymap_t::Modifiers::ALT, (int)'L'});

    keyMapManager->addBinding(Surge::GUI::OPEN_MANUAL, {juce::KeyPress::F1Key});
    keyMapManager->addBinding(Surge::GUI::TOGGLE_ABOUT, {juce::KeyPress::F12Key});

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
                undoManager()->undo();
                return true;
            case Surge::GUI::REDO:
                undoManager()->redo();
                return true;

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
    for (const auto &ol : juceOverlays)
    {
        auto olw = getOverlayWrapperIfOpen(ol.first);
        if (olw)
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

void SurgeGUIEditor::loadFromDAWExtraState(SurgeSynthesizer *synth)
{
    auto des = &(synth->storage.getPatch().dawExtraState);
    if (des->isPopulated)
    {
        auto sz = des->editor.instanceZoomFactor;
        if (sz > 0)
            setZoomFactor(sz);
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
    }
}

void SurgeGUIEditor::showPatchCommentTooltip(const std::string &comment)
{
    if (patchSelectorComment)
    {
        auto psb = patchSelector->getBounds();

        patchSelectorComment->setVisible(true);
        patchSelectorComment->getParentComponent()->toFront(true);
        patchSelectorComment->toFront(true);
        patchSelectorComment->positionForComment(psb.getCentre().withY(psb.getBottom()), comment,
                                                 psb.getWidth());
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
    if (okcWithToggleAlertWindow)
    {
        // This is not re-entrant, sorry
        jassertfalse;
        return false;
    }

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

    auto &lf = frame->getLookAndFeel();

    okcWithToggleCallback = std::move(okCallback);
    okcWithToggleAlertWindow.reset(lf.createAlertWindow(title, msg, "OK", "Cancel", "",
                                                        juce::AlertWindow::NoIcon, 2, nullptr));

    auto cb = std::make_unique<juce::ToggleButton>(ynMessage);

    cb->setName("");
    cb->centreWithSize(400, 20);
    cb->setColour(juce::ToggleButton::textColourId,
                  currentSkin->getColor(Colors::Dialog::Label::Text));
    cb->setColour(juce::ToggleButton::tickColourId,
                  currentSkin->getColor(Colors::Dialog::Checkbox::Tick));
    cb->setColour(juce::ToggleButton::tickDisabledColourId,
                  currentSkin->getColor(Colors::Dialog::Checkbox::Border));

    okcWithToggleAlertWindow->addCustomComponent(cb.get());
    okcWithToggleToggleButton = std::move(cb);
    okcWithToggleAlertWindow->setAlwaysOnTop(true);
    okcWithToggleAlertWindow->enterModalState(
        true, juce::ModalCallbackFunction::create([this, dontAskAgainKey](int isOK) {
            if (okcWithToggleToggleButton->getToggleState())
            {
                if (isOK == 1)
                {
                    Surge::Storage::updateUserDefaultValue(&(synth->storage), dontAskAgainKey,
                                                           ALWAYS);
                }
                else
                {
                    Surge::Storage::updateUserDefaultValue(&(synth->storage), dontAskAgainKey,
                                                           NEVER);
                }
            }

            if (isOK == 1)
            {
                okcWithToggleCallback();
            }

            okcWithToggleAlertWindow.reset(nullptr);
            okcWithToggleToggleButton.reset(nullptr);
        }),
        false);

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
