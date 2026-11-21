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

#ifndef SURGE_SRC_SURGE_FX_SURGEFXEDITOR_H
#define SURGE_SRC_SURGE_FX_SURGEFXEDITOR_H

#include "SurgeFXProcessor.h"
#include "SurgeLookAndFeel.h"
#include "FXWidgets.h"
#include "EffectPanel.h"
#include "FxPresetAndClipboardManager.h"

#include "juce_gui_basics/juce_gui_basics.h"

struct Picker;
struct PresetPicker;

//==============================================================================
/**
 */
class SurgefxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    juce::AsyncUpdater,
                                    SurgeStorage::ErrorListener
{
  public:
    SurgefxAudioProcessorEditor(SurgefxAudioProcessor &);
    ~SurgefxAudioProcessorEditor();

    struct FxMenu
    {
        enum Type
        {
            SECTION,
            FX
        } type;
        bool isBreak{false};
        std::string name;
        int fxtype{-1};
    };
    std::vector<FxMenu> menu;
    std::unique_ptr<Picker> picker;
    std::unique_ptr<PresetPicker> presetPicker;

    void makeMenu();
    void showMenu();
    void showHostContextMenuFor(juce::AudioProcessorParameter *param);
    void toggleLatencyMode();

    void showPresetMenu();
    void stepPreset(int direction); // -1 = prev, +1 = next
    void loadCurrentPreset();
    void rebuildCurrentPresets();
    void saveCurrentPreset();
    void finishPresetSave();
    void deleteCurrentPreset();
    void refreshPresetList();
    void rebuildCurrentPresetsKeepingSelection();

    // Selects the preset the processor's stored name refers to, or the default one if no
    // name is stored. Nothing is selected if the named preset no longer exists.
    void selectPresetByStoredName();

    // Presets are looked up by identity rather than kept by index, since a rescan can shift
    // the list. Returns -1 if there is no such preset.
    int findPresetIndex(bool isFactory, const fs::path &subPath, const std::string &name) const;

    //==============================================================================
    void paint(juce::Graphics &) override;
    juce::String getPanelStatus() const;
    void resized() override;
    void parentHierarchyChanged() override;
    void mouseEnter(const juce::MouseEvent &) override;
    void mouseExit(const juce::MouseEvent &) override;

    /**
     * findLargestFittingZoomBetween
     *
     * Finds the largest zoom which will fit your current screen between a lower and upper bound.
     * Will never return something smaller than lower or larger than upper. Default is as large as
     * possible, quantized in units of zoomQuanta, with the maximum screen usage percentages
     * protecting for screen real estate. The function also needs to know the 100% size of the UI
     * the baseW and baseH)
     *
     * for instance findLargestFittingZoomBetween( 100, 200, 5, 90, bw, bh )
     *
     * would find the largest possible zoom which uses at most 90% of your screen real estate but
     * would also guarantee that the result % 5 == 0.
     */
    int findLargestFittingZoomBetween(int zoomLow, int zoomHigh, int zoomQuanta,
                                      int percentageOfScreenAvailable, float baseW, float baseH);

    void paramsChangedCallback();
    void setEffectType(int i);

    virtual void handleAsyncUpdate() override;

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgefxAudioProcessor &processor;

    void onSurgeError(const std::string &msg, const std::string &title,
                      const SurgeStorage::ErrorType &errorType) override;

    static constexpr int baseWidth = surge::fx::effectPanel::width;
    static constexpr int baseHeight = surge::fx::effectPanel::height;

    std::vector<Surge::Storage::FxUserPreset::Preset> currentPresets;
    int currentPresetIndex{-1};
    int lastSeenEffectType{-1};

    // The user preset being saved. If saving asks for an overwrite confirmation, the save
    // is only finished once the user has confirmed it.
    fs::path pendingSaveSubPath;
    std::string pendingSaveName;
    bool awaitingSaveConfirmation{false};

    int defaultPresetIndexForCurrentType() const { return currentPresets.empty() ? -1 : 0; }

    std::vector<juce::Component *> accessibleOrderWeakRefs;
    std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;

  private:
    surge::fx::ParameterKnob fxParamKnobs[n_fx_params];
    surge::fx::ParameterCaption fxParamLabels[n_fx_params];
    surge::fx::ParameterOption fxTempoSync[n_fx_params];
    surge::fx::ParameterOption fxDeactivated[n_fx_params];
    surge::fx::ParameterOption fxExtended[n_fx_params];
    surge::fx::ParameterOption fxAbsoluted[n_fx_params];

    std::vector<std::unique_ptr<juce::Component>> panelLabels;
    int panelLabelEffect{-1};
    int hoveredParam{-1};

    void blastToggleState(int i);
    void resetLabels();

    std::unique_ptr<juce::TextEditor> valueEditor;
    void showValueEditor(int i);
    void showParameterMenu(int i);
    void addSettingsMenuItems(juce::PopupMenu &);
    juce::PopupMenu makeOSCMenu();
    void showOSCInputPortDialog();
    std::unique_ptr<juce::AlertWindow> oscPortDialog;
    juce::PopupMenu modifyHostMenu(juce::PopupMenu menu, juce::AudioProcessorParameter *param);

    std::unique_ptr<SurgeLookAndFeel> surgeLookFeel;
    std::unique_ptr<Surge::Storage::FxUserPreset> fxPresetManager;

    void addAndMakeVisibleRecordOrder(juce::Component *c)
    {
        accessibleOrderWeakRefs.push_back(c);
        addAndMakeVisible(c);
    }

    bool keyPressed(const juce::KeyPress &key) override;

    // We have a very slow idle (it just checks invalid bus configs)
    struct IdleTimer : juce::Timer
    {
        IdleTimer(SurgefxAudioProcessorEditor *ed) : ed(ed) {}
        ~IdleTimer() = default;
        void timerCallback() override;
        SurgefxAudioProcessorEditor *ed;
    };

    void idle();

    std::unique_ptr<IdleTimer> idleTimer;
    bool priorValid{true};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessorEditor)
};

#endif // SURGE_SRC_SURGE_FX_SURGEFXEDITOR_H
