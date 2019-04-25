#pragma once

/*
** As described in github issue https://github.com/surge-synthesizer/surge/issues/731 SurgePatch used to include
** gui/CSurgeSlider.h so that it could define things like VSTGUI::kHorizontal in parameters. These are fundamentally
** UI ideas.
**
** In an ideal world this would all refactor https://github.com/surge-synthesizer/surge/issues/810 as described there
** but for now we want to break the dependency so do it this way
**
** 1: This header creates enums which we can or together with the same values
** 2: in gui/SurgeSynthesizer.cpp we add a checker to make sure at runtime that parameters are
**    consistent and throw an error if not.
**
** And that allows us to break the requirement in the non-gui code that we have an include link into all of vstgui
** just for a few constants while also being sure that we have consistent values when we do build GUI apparatus
*/

namespace Surge
{
namespace ParamConfig
{
enum Style
{
    kHorizontal = 1 << 0,
    kVertical = 1 << 1
};
}
}

/*
** This enum which CSurgeSlider used to define gets moved up here so we can include it outside the GUI 
*/
enum CControlEnum_turbodeluxe
{
   kBipolar = 1 << 15,
   kWhite = 1 << 16,
   kSemitone = 1 << 17,
   kMini = 1 << 18,
   kMeta = 1 << 19,
   kEasy = 1 << 20,
   kHide = 1 << 21,
   kNoPopup = 1 << 22,
};

// Similarly move from CSurgeVuMeter for now
enum vutypes
{
   vut_off,
   vut_vu,
   vut_vu_stereo,
   vut_gain_reduction,
   n_vut,
};

