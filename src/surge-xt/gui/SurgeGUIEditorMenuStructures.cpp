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

#include "melatonin_inspector/melatonin_inspector.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIEditorTags.h"
#include "SurgeGUIUtils.h"
#include "SurgeSynthEditor.h"
#include "SurgeJUCELookAndFeel.h"

#include "AccessibleHelpers.h"
#include "DebugHelpers.h"
#include "ModulatorPresetManager.h"

#include "fmt/core.h"

#include "widgets/ModulatableSlider.h"
#include "widgets/PatchSelector.h"
#include "widgets/XMLConfiguredMenus.h"

#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#include "libMTSMaster.h"
#endif

#if HAS_JUCE
#include "SurgeSharedBinary.h"
#endif

#include <regex>

/* MAKE */

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

                        closeOverlay(ov);

                        if (newshape == lt_mseg || newshape == lt_formula)
                        {
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
                }

                this->synth->refresh_editor = true;
            };

            m.addItem(p.name, action);
        }

        bool haveD = false;

        if (cat.path.empty())
        {
            // This is a preset in the root
        }
        else
        {
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
        }
    };

    for (auto tlc : presetCategories)
    {
        if (tlc.path.empty())
        {
            // We have presets in the root! Don't recurse forever and put them in the root
            recurseCat(lfoSubMenu, tlc);
            lfoSubMenu.addSeparator();
        }
        else if (tlc.parentPath.empty())
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

    bool isOddsoundOnAsClient = this->synth->storage.oddsound_mts_active_as_client &&
                                this->synth->storage.oddsound_mts_client;

    auto tuningSubMenu = juce::PopupMenu();
    auto hu = helpURLForSpecial("tun-menu");

    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);

        addHelpHeaderTo(isOddsoundOnAsClient ? "Tuning (MTS-ESP)" : "Tuning", lurl, tuningSubMenu);

        tuningSubMenu.addSeparator();
    }

#ifndef SURGE_SKIP_ODDSOUND_MTS
    if (isOddsoundOnAsClient)
    {
        std::string mtsScale = MTS_GetScaleName(synth->storage.oddsound_mts_client);

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Current Tuning: ") + mtsScale, false, false,
                              []() {});

        std::string openname = isAnyOverlayPresent(TUNING_EDITOR) ? "Close " : "Open ";

        Surge::GUI::addMenuWithShortcut(tuningSubMenu,
                                        Surge::GUI::toOSCase(openname + "Tuning Visualizer..."),
                                        showShortcutDescription("Alt + T", u8"\U00002325T"),
                                        [this]() { this->toggleOverlay(TUNING_EDITOR); });

        tuningSubMenu.addSeparator();
    }
#endif

    if (!isOddsoundOnAsClient)
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

        tuningSubMenu.addItem(
            Surge::GUI::toOSCase("Set to Standard Tuning"), !this->synth->storage.isStandardTuning,
            false, [this]() {
                this->synth->storage.retuneTo12TETScaleC261Mapping();
                this->synth->storage.resetTuningToggle();
                this->synth->refresh_editor = true;
                tuningChanged();
                juceEditor->processor.paramChangeToListeners(
                    nullptr, true, juceEditor->processor.SCT_TUNING_SCL, .0, .0, .0, "(standard)");
                juceEditor->processor.paramChangeToListeners(
                    nullptr, true, juceEditor->processor.SCT_TUNING_KBM, .0, .0, .0, "(standard)");
            });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Set to Standard Mapping (Concert C)"),
                              (!this->synth->storage.isStandardMapping), false, [this]() {
                                  this->synth->storage.remapToConcertCKeyboard();
                                  this->synth->refresh_editor = true;
                                  tuningChanged();
                                  juceEditor->processor.paramChangeToListeners(
                                      nullptr, true, juceEditor->processor.SCT_TUNING_KBM, .0, .0,
                                      .0, "(standard)");
                              });

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Set to Standard Scale (12-TET)"),
                              (!this->synth->storage.isStandardScale), false, [this]() {
                                  this->synth->storage.retuneTo12TETScale();
                                  this->synth->refresh_editor = true;
                                  tuningChanged();
                                  juceEditor->processor.paramChangeToListeners(
                                      nullptr, true, juceEditor->processor.SCT_TUNING_SCL, .0, .0,
                                      .0, "(standard)");
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
                auto tuningLabel = path_to_string(fs::path(synth->storage.currentScale.name));
                tuningLabel = tuningLabel.substr(0, tuningLabel.find_last_of("."));
                juceEditor->processor.paramChangeToListeners(
                    nullptr, true, juceEditor->processor.SCT_TUNING_SCL, .0, .0, .0, tuningLabel);
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
                auto mappingLabel = synth->storage.currentMapping.name;
                mappingLabel = mappingLabel.substr(0, mappingLabel.find_last_of("."));
                juceEditor->processor.paramChangeToListeners(
                    nullptr, true, juceEditor->processor.SCT_TUNING_KBM, .0, .0, .0, mappingLabel);
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
        std::string middle_A = "A" + std::to_string(oct);

        tuningSubMenu.addItem(
            Surge::GUI::toOSCase("Remap " + middle_A + " (MIDI Note 69) Directly to..."),
            [this, middle_A, where]() {
                std::string c = "440.0";
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

#ifndef SURGE_SKIP_ODDSOUND_MTS
    auto canMaster = MTS_CanRegisterMaster();

    auto tsMode = true; // Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                        // Surge::Storage::UseODDMTS,
                        //                      false);

    if (tsMode)
    {
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

        tuningSubMenu.addItem(Surge::GUI::toOSCase("Use MIDI Channel for Octave Shift"),
                              !synth->mpeEnabled, (synth->storage.mapChannelToOctave), [this]() {
                                  this->synth->storage.mapChannelToOctave =
                                      !(this->synth->storage.mapChannelToOctave);
                              });

        tuningSubMenu.addSeparator();

        std::string mtxt = "Act as" + Surge::GUI::toOSCase(" MTS-ESP Source");

        tuningSubMenu.addItem(mtxt, canMaster || getStorage()->oddsound_mts_active_as_main,
                              getStorage()->oddsound_mts_active_as_main, [this]() {
                                  if (getStorage()->oddsound_mts_active_as_main)
                                  {
                                      this->synth->storage.disconnect_as_oddsound_main();
                                  }
                                  else
                                  {
                                      this->synth->storage.connect_as_oddsound_main();
                                  }
                              });
    }

    if (getStorage()->oddsound_mts_active_as_main)
    {
        auto nc = MTS_GetNumClients();
        tuningSubMenu.addItem("MTS-ESP has " + std::to_string(nc) + " clients", false, false,
                              []() {});
    }

    if (getStorage()->oddsound_mts_active_as_main || !canMaster)
    {
        if (MTS_HasIPC())
        {
            tuningSubMenu.addItem("Reinitialize MTS Library and IPC", true, false, [this]() {
                std::string msg =
                    "Reinitializing MTS will disconnect all clients, including "
                    "this one, and will generally require you to restart your DAW session, "
                    "but it will clear up after particularly nasty crashes or IPC issues. "
                    "Are you sure you want to do this?";
                alertYesNo("Reinitialize MTS-ESP", msg, MTS_Reinitialize);
            });
        }
    }

    if (tsMode && !this->synth->storage.oddsound_mts_client &&
        !getStorage()->oddsound_mts_active_as_main)
    {
        tuningSubMenu.addItem(Surge::GUI::toOSCase("Connect Instance to MTS-ESP"), [this]() {
            this->synth->storage.initialize_oddsound();
            this->synth->refresh_editor = true;
            this->synth->storage.getPatch().dawExtraState.disconnectFromOddSoundMTS = false;
        });
    }

    if (this->synth->storage.oddsound_mts_active_as_client &&
        this->synth->storage.oddsound_mts_client)
    {
        tuningSubMenu.addItem(Surge::GUI::toOSCase("Disconnect Instance from MTS-ESP"), [this]() {
            auto q = this->synth->storage.oddsound_mts_client;
            this->synth->storage.oddsound_mts_active_as_client = false;
            this->synth->storage.oddsound_mts_client = nullptr;
            this->synth->storage.getPatch().dawExtraState.disconnectFromOddSoundMTS = true;
            MTS_DeregisterClient(q);
        });
    }

#endif

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
            std::string c = fmt::format("{:d}", (int)zoomFactor);
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

#if 0
    if (Surge::GUI::getIsStandalone())
    {
        juce::Component *comp = frame.get();

        while (comp)
        {
            auto *cdw = dynamic_cast<juce::ResizableWindow *>(comp);

            if (cdw)
            {
                zoomSubMenu.addSeparator();

                if (cdw->isFullScreen())
                {
                    Surge::GUI::addMenuWithShortcut(
                        zoomSubMenu, Surge::GUI::toOSCase("Exit Fullscreen Mode"),
                        showShortcutDescription("F11"),
                        [this, w = juce::Component::SafePointer(cdw)]() {
                            if (w)
                            {
                                w->setFullScreen(false);
                            }
                        });
                }
                else
                {
                    Surge::GUI::addMenuWithShortcut(
                        zoomSubMenu, Surge::GUI::toOSCase("Enter Fullscreen Mode"),
                        showShortcutDescription("F11"),
                        [this, w = juce::Component::SafePointer(cdw)]() {
                            if (w)
                            {
                                w->setFullScreen(true);
                            }
                        });
                }

                comp = nullptr;
            }
            else
            {
                comp = comp->getParentComponent();
            }
        }
    }
#endif

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
        std::string txt = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::DefaultPatchAuthor, "");
        if (!Surge::Storage::isValidUTF8(txt))
        {
            txt = "";
        }

        promptForMiniEdit(
            txt, "Enter a default text:", "Set Default Patch Author", where,
            [this](const std::string &s) {
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       Surge::Storage::DefaultPatchAuthor, s);
            },
            mainMenu);
    });

    patchDefMenu.addItem(Surge::GUI::toOSCase("Set Default Patch Comment..."), [this, where]() {
        std::string txt = Surge::Storage::getUserDefaultValue(
            &(this->synth->storage), Surge::Storage::DefaultPatchComment, "");

        if (!Surge::Storage::isValidUTF8(txt))
        {
            txt = "";
        }

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

    patchDefMenu.addItem(Surge::GUI::toOSCase("Append Original Author Name to Modified Patches"),
                         true, appendOGPatchBy, [this, appendOGPatchBy]() {
                             Surge::Storage::updateUserDefaultValue(
                                 &(this->synth->storage), Surge::Storage::AppendOriginalPatchBy,
                                 !appendOGPatchBy);
                         });

    patchDefMenu.addSeparator();

    if (Surge::GUI::getIsStandalone())
    {
        auto tempoOnLoadMenu = juce::PopupMenu();

        bool overrideTempoOnLoad = Surge::Storage::getUserDefaultValue(
            &(synth->storage), Surge::Storage::OverrideTempoOnPatchLoad, true);

        tempoOnLoadMenu.addItem(Surge::GUI::toOSCase("Keep Current Tempo"), true,
                                !overrideTempoOnLoad, [this, overrideTempoOnLoad]() {
                                    Surge::Storage::updateUserDefaultValue(
                                        &(this->synth->storage),
                                        Surge::Storage::OverrideTempoOnPatchLoad, false);
                                });

        tempoOnLoadMenu.addItem(Surge::GUI::toOSCase("Override With Embedded Tempo if Available"),
                                true, overrideTempoOnLoad, [this, overrideTempoOnLoad]() {
                                    Surge::Storage::updateUserDefaultValue(
                                        &(this->synth->storage),
                                        Surge::Storage::OverrideTempoOnPatchLoad, true);
                                });

        patchDefMenu.addSubMenu(Surge::GUI::toOSCase("Tempo on Patch Load"), tempoOnLoadMenu);
    }

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

    patchDefMenu.addSeparator();
    patchDefMenu.addItem(Surge::GUI::toOSCase("Export Patch as Text (Non-Default Parameters Only)"),
                         true, false, [this]() { showHTML(patchToHtml()); });
    patchDefMenu.addItem(Surge::GUI::toOSCase("Export Patch as Text (All Parameters)"), true, false,
                         [this]() { showHTML(patchToHtml(true)); });

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

    bool showVirtualKeyboard = getShowVirtualKeyboard();

    Surge::GUI::addMenuWithShortcut(wfMenu, Surge::GUI::toOSCase("Virtual Keyboard"),
                                    showShortcutDescription("Alt + K", u8"\U00002325K"), true,
                                    showVirtualKeyboard, [this]() { toggleVirtualKeyboard(); });

    makeScopeEntry(wfMenu);

    return wfMenu;
}

juce::PopupMenu SurgeGUIEditor::makeAccesibilityMenu(const juce::Point<int> &rect)
{
    auto accMenu = juce::PopupMenu();

    accMenu.addItem(Surge::GUI::toOSCase("Set All Recommended Accessibility Options"), true, false,
                    [this]() { setRecommendedAccessibility(); });
    accMenu.addSeparator();

    bool doAccAnn = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseNarratorAnnouncements, false);

    accMenu.addItem(Surge::GUI::toOSCase("Send Additional Accessibility Announcements"), true,
                    doAccAnn, [this, doAccAnn]() {
                        Surge::Storage::updateUserDefaultValue(
                            &(this->synth->storage), Surge::Storage::UseNarratorAnnouncements,
                            !doAccAnn);
                    });

#if WINDOWS
    bool doAccAnnPatch = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead, true);

    accMenu.addItem(Surge::GUI::toOSCase("Announce Patch Browser Entries"), true, doAccAnnPatch,
                    [this, doAccAnnPatch]() {
                        Surge::Storage::updateUserDefaultValue(
                            &(this->synth->storage),
                            Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead,
                            !doAccAnnPatch);
                    });
#endif

    bool doExpMen = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::ExpandModMenusWithSubMenus, false);

    accMenu.addItem(Surge::GUI::toOSCase("Add Sub-Menus for Modulation Menu Items"), true, doExpMen,
                    [this, doExpMen]() {
                        Surge::Storage::updateUserDefaultValue(
                            &(this->synth->storage), Surge::Storage::ExpandModMenusWithSubMenus,
                            !doExpMen);
                    });

    bool focusModEditor = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::FocusModEditorAfterAddModulationFrom, false);

    accMenu.addItem(Surge::GUI::toOSCase("Focus Modulator Editor on \"") +
                        Surge::GUI::toOSCase("Add Modulation From\" Actions"),
                    true, focusModEditor, [this, focusModEditor]() {
                        Surge::Storage::updateUserDefaultValue(
                            &(this->synth->storage),
                            Surge::Storage::FocusModEditorAfterAddModulationFrom, !focusModEditor);
                    });

    return accMenu;
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
        juceEditor->getSurgeLookAndFeel()->onSkinChanged();
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
        juce::URL("https://surge-synthesizer.github.io/skin-manual/").launchInDefaultBrowser();
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
        Surge::GUI::toOSCase("Use MIDI Channels 2 and 3 to Play Scenes Individually"),
        !synth->storage.mapChannelToOctave, useMIDICh2Ch3, [this, useMIDICh2Ch3]() {
            Surge::Storage::updateUserDefaultValue(
                &(this->synth->storage), Surge::Storage::UseCh2Ch3ToPlayScenesIndividually,
                !useMIDICh2Ch3);
        });

    midiSubMenu.addSeparator();

    auto chanSubMenu = juce::PopupMenu();

    auto learnChan = Surge::Storage::getUserDefaultValue(
        &(this->synth->storage), Surge::Storage::MenuBasedMIDILearnChannel, -1);

    chanSubMenu.addItem("Omni", true, (learnChan == -1), [this]() {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                               Surge::Storage::MenuBasedMIDILearnChannel, -1);
    });

    for (int ch = 0; ch < 16; ch++)
    {
        chanSubMenu.addItem(
            fmt::format("Channel {}", ch + 1), true, (learnChan == ch), [this, ch]() {
                Surge::Storage::updateUserDefaultValue(
                    &(this->synth->storage), Surge::Storage::MenuBasedMIDILearnChannel, ch);
            });
    }

    midiSubMenu.addSubMenu(Surge::GUI::toOSCase("Default Channel For Menu-Based MIDI Learn"),
                           chanSubMenu);

    bool softTakeover = Surge::Storage::getUserDefaultValue(&(this->synth->storage),
                                                            Surge::Storage::MIDISoftTakeover, 0);

    midiSubMenu.addItem(Surge::GUI::toOSCase("Soft Takeover MIDI Learned Parameters"), true,
                        softTakeover, [this, softTakeover]() {
                            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                                   Surge::Storage::MIDISoftTakeover,
                                                                   !softTakeover);
                            this->synth->midiSoftTakeover = !softTakeover;
                        });

    midiSubMenu.addSeparator();

    midiSubMenu.addItem(Surge::GUI::toOSCase("Save MIDI Mapping As..."), [this, where]() {
        this->scannedForMidiPresets = false; // force a rescan

        promptForMiniEdit(
            "", "Enter the preset name:", "Save MIDI Mapping", where,
            [this](const std::string &s) { this->synth->storage.storeMidiMappingToName(s); },
            mainMenu);
    });

    midiSubMenu.addItem(Surge::GUI::toOSCase("Set Current MIDI Mapping as Default"), [this]() {
        this->synth->storage.write_midi_controllers_to_user_default();
    });

    midiSubMenu.addItem(Surge::GUI::toOSCase("Clear Current MIDI Mapping"), [this]() {
        int n = n_global_params + (n_scene_params * n_scenes);

        for (int i = 0; i < n; i++)
        {
            this->synth->storage.getPatch().param_ptr[i]->midictrl = -1;
            this->synth->storage.getPatch().param_ptr[i]->midichan = -1;

            this->synth->storage.getPatch().dawExtraState.midictrl_map[i] = -1;
            this->synth->storage.getPatch().dawExtraState.midichan_map[i] = -1;
        }

        for (int i = 0; i < n_customcontrollers; i++)
        {
            this->synth->storage.controllers[i] = -1;
            this->synth->storage.controllers_chan[i] = -1;

            this->synth->storage.getPatch().dawExtraState.customcontrol_map[i] = -1;
            this->synth->storage.getPatch().dawExtraState.customcontrol_chan_map[i] = -1;
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
            midiSubMenu.addSectionHeader("USER MIDI MAPPINGS");
        }

        midiSubMenu.addItem(p.first,
                            [this, p] { this->synth->storage.loadMidiMappingByName(p.first); });
    }

    return midiSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeOSCMenu(const juce::Point<int> &where)
{
    auto storage = &(synth->storage);
    auto des = &(synth->storage.getPatch().dawExtraState);
    auto oscSubMenu = juce::PopupMenu();

#if HAS_JUCE
    oscSubMenu.addItem(Surge::GUI::toOSCase("Show OSC Settings..."),
                       [this]() { showOverlay(OPEN_SOUND_CONTROL_SETTINGS); });

    oscSubMenu.addItem(Surge::GUI::toOSCase("Show OSC Specification..."), [this]() {
        auto oscSpec = std::string(SurgeSharedBinary::oscspecification_html,
                                   SurgeSharedBinary::oscspecification_htmlSize) +
                       "\n";
        showHTML(oscSpec);
    });

    oscSubMenu.addSeparator();

    oscSubMenu.addItem(Surge::GUI::toOSCase("Download TouchOSC Template..."), []() {
        juce::URL(fmt::format("{}touchosc", stringWebsite)).launchInDefaultBrowser();
    });

#endif

    return oscSubMenu;
}

juce::PopupMenu SurgeGUIEditor::makeDevMenu(const juce::Point<int> &where)
{
    auto devSubMenu = juce::PopupMenu();

#if WINDOWS
    Surge::GUI::addMenuWithShortcut(devSubMenu, Surge::GUI::toOSCase("Show Debug Console..."),
                                    showShortcutDescription("Alt + D", u8"\U00002325D"),
                                    []() { Surge::Debug::toggleConsole(); });
#endif

    devSubMenu.addItem(Surge::GUI::toOSCase("Use Focus Debugger"), true, debugFocus, [this]() {
        debugFocus = !debugFocus;
        frame->debugFocus = debugFocus;
        frame->repaint();
    });

    devSubMenu.addItem(Surge::GUI::toOSCase("Dump Undo/Redo Stack to stdout"), true, false,
                       [this]() { undoManager()->dumpStack(); });

    if (melatoninInspector)
    {
        devSubMenu.addItem("Close Melatonin Inspector", [this]() {
            if (melatoninInspector)
            {
                melatoninInspector->setVisible(false);
                melatoninInspector.reset();
            }
        });
    }
    else
    {
        devSubMenu.addItem("Launch Melatonin Inspector", [this] {
            melatoninInspector = std::make_unique<melatonin::Inspector>(*frame);
            melatoninInspector->onClose = [this]() { melatoninInspector.reset(); };

            melatoninInspector->setVisible(true);
        });
    }

#ifdef INSTRUMENT_UI
    devSubMenu.addItem(Surge::GUI::toOSCase("Show UI Instrumentation..."),
                       []() { Surge::Debug::report(); });
#endif

    return devSubMenu;
}

/* SHOW */

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

void SurgeGUIEditor::showTuningMenu(const juce::Point<int> &where,
                                    Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeTuningMenu(where, true);

    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::showLfoMenu(const juce::Point<int> &where,
                                 Surge::GUI::IComponentTagValue *launchFrom)
{
    auto m = makeLfoMenu(where);
    m.showMenuAsync(popupMenuOptions(where), Surge::GUI::makeEndHoverCallback(launchFrom));
}

void SurgeGUIEditor::showSettingsMenu(const juce::Point<int> &where,
                                      Surge::GUI::IComponentTagValue *launchFrom)
{
    auto settingsMenu = juce::PopupMenu();

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
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Patch Settings"), patchDefMenu);

    auto wfMenu = makeWorkflowMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Workflow"), wfMenu);

    auto accMenu = makeAccesibilityMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("Accessibility"), accMenu);

    settingsMenu.addSeparator();

    auto mpeSubMenu = makeMpeMenu(where, false);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("MPE Settings"), mpeSubMenu);

    auto midiSubMenu = makeMidiMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("MIDI Settings"), midiSubMenu);

    auto oscSubMenu = makeOSCMenu(where);
    settingsMenu.addSubMenu(Surge::GUI::toOSCase("OSC Settings"), oscSubMenu);

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

    Surge::GUI::addMenuWithShortcut(settingsMenu, Surge::GUI::toOSCase("Surge XT Manual..."),
                                    showShortcutDescription("F1"),
                                    []() { juce::URL(stringManual).launchInDefaultBrowser(); });

    settingsMenu.addItem(Surge::GUI::toOSCase("Surge XT Website..."),
                         []() { juce::URL(stringWebsite).launchInDefaultBrowser(); });

    settingsMenu.addSeparator();

    Surge::GUI::addMenuWithShortcut(settingsMenu, "About Surge XT", showShortcutDescription("F12"),
                                    [this]() { this->showAboutScreen(); });

    settingsMenu.showMenuAsync(popupMenuOptions(where),
                               Surge::GUI::makeEndHoverCallback(launchFrom));
}
