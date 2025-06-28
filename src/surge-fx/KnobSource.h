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
#ifndef SURGE_SRC_SURGE_FX_KNOBSOURCE_H
#define SURGE_SRC_SURGE_FX_KNOBSOURCE_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/data/Continuous.h>
#include <sst/jucegui/util/DebugHelpers.h>

struct KnobSource : sst::jucegui::data::Continuous
{
    KnobSource(SurgefxAudioProcessor &p, SurgeFXParamDisplay &d, int i)
        : processor(p), display(d), id(i)
    {
    }

    std::string label{"KnobSource"};
    std::string getLabel() const override { return label; }
    void setLabel(std::string input) { label = input; }
    float value{0};
    float getValue() const override { return value; }
    float getDefaultValue() const override { return processor.getFXStorageDefaultValue01(id); }

    void setValueFromGUI(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
        for (auto *l : modellisteners)
            l->dataChanged();

        this->processor.setFXParamValue01(id, value);
        display.setDisplay(getValueAsStringFor(value));
    }

    std::string getValueAsStringFor(float f) const override
    {
        return processor.getParamValue(id).c_str();
    }

    void setValueFromModel(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
        display.setDisplay(getValueAsStringFor(value));
    }

    SurgefxAudioProcessor &processor;
    SurgeFXParamDisplay &display;
    int id;
};
#endif // SURGE_SRC_SURGE_FX_KNOBSOURCE_H
