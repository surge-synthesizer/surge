#pragma once

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
