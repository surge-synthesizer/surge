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
#ifndef SURGE_SRC_SURGE_FX_PARAMETERPANEL_H
#define SURGE_SRC_SURGE_FX_PARAMETERPANEL_H

#include "SurgeFXProcessor.h"
#include "SurgeLookAndFeel.h"
#include "KnobSource.h"

#include <sst/jucegui/style/StyleSheet.h>
#include <sst/jucegui/components/Knob.h>

class ParameterPanel : public juce::Component
{
  public:
    ParameterPanel(SurgefxAudioProcessor &, std::vector<juce::Component *> &accessibleOrder);

    ~ParameterPanel() override;

    void paint(juce::Graphics &) override;
    void resized() override;
    void reset();

    SurgeFXParamDisplay fxParamDisplay[n_fx_params];
    std::vector<std::unique_ptr<KnobSource>> sources;
    std::shared_ptr<sst::jucegui::style::StyleSheet> styleSheet;

  private:
    static constexpr int topSection = 80;
    static constexpr int baseWidth = 600;

    SurgefxAudioProcessor &processor;

    SurgeTempoSyncSwitch fxTempoSync[n_fx_params];
    SurgeTempoSyncSwitch fxDeactivated[n_fx_params];
    SurgeTempoSyncSwitch fxExtended[n_fx_params];
    SurgeTempoSyncSwitch fxAbsoluted[n_fx_params];

    std::vector<std::unique_ptr<sst::jucegui::components::Knob>> knobs;

    juce::Colour backgroundColour = juce::Colour(205, 206, 212);
    juce::Colour surgeOrange = juce::Colour(255, 144, 0);
    std::vector<juce::Component *> &accessibleOrderWeakRefs; // Reference to the vector

    void addAndMakeVisibleRecordOrder(juce::Component *c)
    {
        accessibleOrderWeakRefs.push_back(c);
        addAndMakeVisible(c);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ParameterPanel);
};

#endif // SURGE_SRC_SURGE_FX_PARAMETERPANEL_H
