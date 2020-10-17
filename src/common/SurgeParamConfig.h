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
    kHorizontal = 1U << 0U,
    kVertical = 1U << 1U
};
}
}

/*
** This enum which CSurgeSlider used to define gets moved up here so we can include it outside the GUI 
*/
enum CControlEnum_turbodeluxe
{
   kBipolar = 1U << 15U,
   kWhite = 1U << 16U,
   kSemitone = 1U << 17U,
   kMini = 1U << 18U,
   kMeta = 1U << 19U,
   kEasy = 1U << 20U,
   kHide = 1U << 21U,
   kNoPopup = 1U << 22U,
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

// Move these here from CNumberField
enum ctrl_mode
{
   cm_none = 0,
   cm_integer,
   cm_notename,
   cm_float,
   cm_percent,
   cm_percent_bipolar,
   cm_decibel,
   cm_decibel_squared,
   cm_envelopetime,
   cm_lforate,
   cm_midichannel,
   cm_midichannel_from_127,
   cm_mutegroup,
   cm_lag,
   cm_pbdepth,
   cm_frequency20_20k,
   cm_frequency50_50k,
   cm_bitdepth_16,
   cm_frequency0_2k,
   cm_decibelboost12,
   cm_octaves3,
   cm_frequency1hz,
   cm_time1s,
   cm_frequency_audible,
   cm_frequency_samplerate,
   cm_frequency_khz,
   cm_frequency_khz_bi,
   cm_frequency_hz_bi,
   cm_eq_bandwidth,
   cm_stereowidth,
   cm_mod_decibel,
   cm_mod_pitch,
   cm_mod_freq,
   cm_mod_percent,
   cm_mod_time,
   cm_polyphony,
   cm_envshape,
   cm_osccount,
   cm_count4,
   cm_noyes,
   cm_temposync,
   cm_int_menu,
   cm_mseg_snap_h,
   cm_mseg_snap_v,
};
