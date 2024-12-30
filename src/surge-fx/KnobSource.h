#ifndef SURGE_SRC_SURGE_FX_KNOBSOURCE_H
#define SURGE_SRC_SURGE_FX_KNOBSOURCE_H

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/data/Continuous.h>
#include <sst/jucegui/util/DebugHelpers.h>

struct KnobSource : sst::jucegui::data::ContinuousModulatable
{
    KnobSource(SurgefxAudioProcessor &p, int i) : processor(p), id(i) {}

    std::string label{"KnobSource"};
    std::string getLabel() const override { return label; }
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
    }

    void setValueFromModel(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
    }

    SurgefxAudioProcessor &processor;
    int id;

    float min{0}, max{1};
    float getMin() const override { return min; }
    float getMax() const override { return max; }

    float mv{0.2};
    float getModulationValuePM1() const override { return mv; }
    void setModulationValuePM1(const float &f) override { mv = f; }
    bool isModulationBipolar() const override { return isBipolar(); } // sure why not
};
#endif // SURGE_SRC_SURGE_FX_KNOBSOURCE_H
