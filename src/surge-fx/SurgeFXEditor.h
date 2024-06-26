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

#include "juce_gui_basics/juce_gui_basics.h"

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

  private:
    struct AccSlider : public juce::Slider
    {
        AccSlider() { setWantsKeyboardFocus(true); }
        juce::String getTextFromValue(double v) override
        {
            // std::cout << "GTFV " << v << std::endl;
            // return juce::Slider::getTextFromValue(v);
            //  This is a bit of a hack to externalize this but
            return tv;
        }

        juce::String tv;
        void setTextValue(juce::String s)
        {
            tv = s;
            if (auto *handler = getAccessibilityHandler())
            {
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
            }
        }
        bool keyPressed(const juce::KeyPress &key) override
        {
            float amt = 0.05;
            if (key.getModifiers().isShiftDown())
                amt = 0.01;
            if (key.getKeyCode() == juce::KeyPress::upKey)
            {
                setValue(std::clamp(getValue() + amt, 0., 1.),
                         juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::downKey)
            {
                setValue(std::clamp(getValue() - amt, 0., 1.),
                         juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::homeKey)
            {
                setValue(1., juce::NotificationType::sendNotification);
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::endKey)
            {
                setValue(0., juce::NotificationType::sendNotification);
                return true;
            }
            return false;
        }
    };
    AccSlider fxParamSliders[n_fx_params];
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

  public:
    std::unique_ptr<juce::ComponentTraverser> createFocusTraverser() override;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SurgefxAudioProcessorEditor)
};

#endif // SURGE_SRC_SURGE_FX_SURGEFXEDITOR_H
