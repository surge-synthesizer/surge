/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_VUMETER_H
#define SURGE_XT_VUMETER_H

#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeImage;

namespace Surge
{
namespace Widgets
{
struct VuMeter : public juce::Component, public WidgetBaseMixin<VuMeter>
{
    VuMeter();
    ~VuMeter();

    float vL{0.f}, vR{0.f};
    void setValue(float f) override { vL = f; }
    float getValue() const override { return vL; }

    void setValueR(float f) { vR = f; }
    float getValueR() const { return vR; }

    float cpuLevel{0.f};
    void setCpuLevel(float f) { cpuLevel = f; }
    float getCpuLevel() const { return cpuLevel; }

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    Surge::ParamConfig::VUType vu_type{Surge::ParamConfig::vut_off};
    void setType(Surge::ParamConfig::VUType t) { vu_type = t; };

    void paint(juce::Graphics &g) override;

    bool isAudioActive{true};
    void setIsAudioActive(bool isIn)
    {
        auto was = isAudioActive;
        isAudioActive = isIn;
        if (was != isAudioActive)
            repaint();
    }

    void onSkinChanged() override;
    SurgeImage *hVuBars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_VUMETER_H
