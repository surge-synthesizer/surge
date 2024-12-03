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

#ifndef SURGE_SRC_COMMON_DSP_WAVETABLESCRIPTEVALUATOR_H
#define SURGE_SRC_COMMON_DSP_WAVETABLESCRIPTEVALUATOR_H

#include "SurgeStorage.h"
#include "StringOps.h"
#include "Wavetable.h"

namespace Surge
{
namespace WavetableScript
{
struct LuaWTEvaluator
{
    struct Details;
    std::unique_ptr<Details> details;
    LuaWTEvaluator();
    ~LuaWTEvaluator();

    void setStorage(SurgeStorage *);
    void setScript(const std::string &);
    void setResolution(size_t);
    void setFrameCount(size_t);

    using validFrame_t = std::vector<float>;
    using frame_t = std::optional<validFrame_t>;

    /*
     * Generate all the data required to call BuildWT. The wavdata here is data you
     * must free with delete[]
     */
    bool populateWavetable(wt_header &wh, float **wavdata);

    frame_t getFrame(size_t frame);

    std::string getSuggestedWavetableName();

    static std::string defaultWavetableScript();
};

} // namespace WavetableScript
} // namespace Surge
#endif // SURGE_XT_WAVETABLESCRIPTEVALUATOR_H
