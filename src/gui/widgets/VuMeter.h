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

#include <JuceHeader.h>
#include "WidgetBaseMixin.h"

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

    int vu_type{vut_off};
    void setType(int t) { vu_type = t; };

    void paint(juce::Graphics &g) override;

    void onSkinChanged() override;
    juce::Drawable *hVuBars;
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_VUMETER_H
