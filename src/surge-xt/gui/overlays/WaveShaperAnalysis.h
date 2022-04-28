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
#include "widgets/MultiSwitch.h"

class SurgeStorage;
class SurgeGUIEditor;

namespace Surge
{
namespace Overlays
{
struct WaveShaperAnalysis : public OverlayComponent, Surge::GUI::SkinConsumingComponent
{
    SurgeGUIEditor *editor{nullptr};
    SurgeStorage *storage{nullptr};
    WaveShaperAnalysis(SurgeGUIEditor *e, SurgeStorage *s);
    void paint(juce::Graphics &g) override;
    void onSkinChanged() override;
    void resized() override;

    void recalcFromSlider();

    void setWSType(int wst);
    sst::waveshapers::WaveshaperType wstype{sst::waveshapers::WaveshaperType::wst_none};

    bool shouldRepaintOnParamChange(const SurgePatch &patch, Parameter *p) override;

    float getDbValue();
    float lastDbValue{-100};

    float getPFG();
    float lastPFG{-100};

    static constexpr int npts = 256;

    typedef std::vector<std::tuple<float, float, float>> curve_t; // x, in, out
    curve_t sliderDrivenCurve;
};
} // namespace Overlays
} // namespace Surge

#endif // SURGE_XT_WAVESHAPERANALYSIS_H
