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
#ifndef SURGE_SRC_COMMON_SURGEPARAMCONFIG_H
#define SURGE_SRC_COMMON_SURGEPARAMCONFIG_H

/*
 * So look: Surge is an evolving system. We keep moving things and cleaning them up
 * and making things more separate. But that's the kind of work that's never done.
 *
 * Parameters in Surge contain some information about the intended orientation of a slider.
 * Should they? Well no, of course not. We should have a separate map. We should not require
 * the non-UI code to know "Horizontal". But we still have that kicking around.
 *
 * Similarly, effects can have a VU as a display item. Cool. But do you need to know
 * what kind of VU? So we could have a 'logical' type here and a 'UI' type in src/gui
 * and we should. I know we should. But we don't yet.
 *
 * So this is the place we put all the enums which should "strictly" be UI or plugin-only
 * concepts, but which bind to parameters and DSP configuration. The list is super small so
 * that's good. But it's not zero.
 */

namespace Surge
{
namespace ParamConfig
{
enum Orientation
{
    kHorizontal = 1U << 0U,
    kVertical = 1U << 1U
};

enum VUType
{
    vut_off = 0,
    vut_vu,
    vut_vu_stereo,
    vut_gain_reduction,
    n_vut,
};

enum
{
    kWhite = 1U << 16U,
    kSemitone = 1U << 17U,
    kMini = 1U << 18U,
    kMeta = 1U << 19U,
    kEasy = 1U << 20U,
    kHide = 1U << 21U,
    kNoPopup = 1U << 22U,
};

} // namespace ParamConfig
} // namespace Surge

#endif // SURGE_SRC_COMMON_SURGEPARAMCONFIG_H
