#pragma once // this doesn't seem like the norm in this repo but still a wip

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/data/Continuous.h>
#include <sst/jucegui/data/Discrete.h>
#include <sst/jucegui/util/DebugHelpers.h>

struct SurgeFXContModParam : sst::jucegui::data::ContinuousModulatable
{
    SurgeFXContModParam(std::string label = "leo") { label = label; };

    std::string label{"default"};
    std::string getLabel() const override { return label; }

    float value{0};
    float min{0}, max{1};
    float mv{0.2};

    float getValue() const override { return value; }
    float getDefaultValue() const override { return (getMax() - getMin()) / 2.0; }
    void setValueFromGUI(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
        for (auto *l : modellisteners)
            l->dataChanged();
    }
    void setValueFromModel(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
    }

    float getMin() const override { return min; }
    float getMax() const override { return max; }

    float getModulationValuePM1() const override { return mv; }
    void setModulationValuePM1(const float &f) override { mv = f; }
    bool isModulationBipolar() const override { return isBipolar(); } // sure why not
};