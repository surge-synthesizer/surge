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
#include "KnobSource.h"

#include "juce_gui_basics/juce_gui_basics.h"

#include <sst/jucegui/style/StyleSheet.h>
#include <sst/jucegui/components/Knob.h>

//==============================================================================
/**
 */
class SurgefxAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    juce::AsyncUpdater,
                                    SurgeStorage::ErrorListener,
                                    sst::jucegui::style::StyleConsumer
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
    std::unique_ptr<juce::Component> picker;

    static constexpr int topSection = 80;

    void makeMenu();
    void showMenu();
    void toggleLatencyMode();
    void changeOSCInputPort();

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

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

    enum RadioGroupIds
    {
        FxTypeGroup = 1776
    };

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    SurgefxAudioProcessor &processor;

    void promptForTypeinValue(const std::string &prompt, const std::string &initValue,
                              std::function<void(const std::string &)> cb);
    struct PromptOverlay;
    std::unique_ptr<PromptOverlay> promptOverlay;

    void onSurgeError(const std::string &msg, const std::string &title,
                      const SurgeStorage::ErrorType &errorType) override;

    static constexpr int baseWidth = 600, baseHeight = 55 * 6 + 80 + topSection;

    std::vector<std::unique_ptr<sst::jucegui::components::Knob>> knobs;
    std::vector<std::unique_ptr<KnobSource>> sources;

  private:
    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];
    SurgeTempoSyncSwitch fxDeactivated[n_fx_params];
    SurgeTempoSyncSwitch fxExtended[n_fx_params];
    SurgeTempoSyncSwitch fxAbsoluted[n_fx_params];

    void blastToggleState(int i);
    void resetLabels();

    juce::PopupMenu makeOSCMenu();

    std::unique_ptr<SurgeLookAndFeel> surgeLookFeel;
    std::unique_ptr<juce::Label> fxNameLabel;

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

  public:
    std::vector<juce::Component *> accessibleOrderWeakRefs;
    std::shared_ptr<sst::jucegui::style::StyleSheet> styleSheet;

  public:
    std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessorEditor)
};

#endif // SURGE_SRC_SURGE_FX_SURGEFXEDITOR_H
