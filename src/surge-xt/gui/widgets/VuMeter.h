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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_VUMETER_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_VUMETER_H

#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"

class SurgeImage;

namespace Surge
{
namespace Widgets
{
struct VuMeter : public juce::Component,
                 public WidgetBaseMixin<VuMeter>,
                 public LongHoldMixin<VuMeter>
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

    void mouseDown(const juce::MouseEvent &event) override;

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
