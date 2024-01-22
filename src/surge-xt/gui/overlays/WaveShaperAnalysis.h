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

#ifndef SURGE_SRC_SURGE_XT_GUI_OVERLAYS_WAVESHAPERANALYSIS_H
#define SURGE_SRC_SURGE_XT_GUI_OVERLAYS_WAVESHAPERANALYSIS_H

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
