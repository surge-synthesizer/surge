/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#ifndef SURGE_XT_WAVESHAPERANALYSIS_H
#define SURGE_XT_WAVESHAPERANALYSIS_H

#include "OverlayComponent.h"
#include "SkinSupport.h"
#include "FilterConfiguration.h"
#include "SurgeGUICallbackInterfaces.h"
#include "widgets/ModulatableSlider.h"

class SurgeStorage;

namespace Surge
{
namespace Overlays
{
struct WaveShaperAnalysis : public OverlayComponent,
                            Surge::GUI::SkinConsumingComponent,
                            Surge::GUI::IComponentTagValue::Listener
{
    WaveShaperAnalysis(SurgeStorage *s);
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;
    void resized() override;

    void valueChanged(Surge::GUI::IComponentTagValue *p) override;
    void recalcFromSlider();

    void setWSType(int wst);
    int wstype{0};

    std::unique_ptr<Surge::Widgets::ModulatableSlider> tryitSlider;

    static constexpr int npts = 256;

    typedef std::vector<std::pair<float, float>> curve_t;
    curve_t sliderDrivenCurve;
    float sliderDb;
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_WAVESHAPERANALYSIS_H
