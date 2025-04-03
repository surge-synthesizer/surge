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

#include "SurgeStorage.h"
#include "Parameter.h"
#include "DSPUtils.h"
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <cstdlib>
#include <regex>
#include <algorithm>
#include <cctype>
#include <utility>
#include <UserDefaults.h>
#include <variant>
#include "DebugHelpers.h"
#include "StringOps.h"
#include "Tunings.h"
#include "fmt/core.h"
#include "UnitConversions.h"
#include "sst/basic-blocks/params/ParamMetadata.h"

Parameter::Parameter()
{
    val.i = 0;
    posx = 0;
    posy = 0;
    posy_offset = 0;
    storage = nullptr;
}

Parameter::~Parameter() = default;

void get_prefix(char *txt, ControlGroup ctrlgroup, int ctrlgroup_entry, int scene)
{
#define PREFIX_SIZE 16

    char prefix[PREFIX_SIZE + 1] = {};

    switch (ctrlgroup)
    {
    case cg_OSC:
        snprintf(prefix, PREFIX_SIZE, "osc%i_", ctrlgroup_entry + 1);
        break;
    case cg_FILTER:
        snprintf(prefix, PREFIX_SIZE, "filter%i_", ctrlgroup_entry + 1);
        break;
    case cg_ENV:
        snprintf(prefix, PREFIX_SIZE, "env%i_", ctrlgroup_entry + 1);
        break;
    /*case cg_LFO:
            snprintf(prefix, PREFIX_SIZE, "ms%i_",ctrlgroup_entry+1);
            break;*/
    case cg_FX:
        snprintf(prefix, PREFIX_SIZE, "fx%i_", ctrlgroup_entry + 1);
        break;
    default:
        prefix[0] = '\0';
        break;
    };

    if (scene > 0 && scene <= n_scenes)
    {
        snprintf(txt, TXT_SIZE, "%c_%s", 'a' + scene - 1, prefix);
    }
    else
    {
        snprintf(txt, TXT_SIZE, "%s", prefix);
    }
}

void Parameter::create_fullname(const char *dn, char *fn, ControlGroup ctrlgroup,
                                int ctrlgroup_entry, const char *lfoPrefixOverride) const
{
    char prefix[PREFIX_SIZE + 1] = {};
    bool useprefix = true;
    switch (ctrlgroup)
    {
    case cg_OSC:
        snprintf(prefix, PREFIX_SIZE, "Osc %i", ctrlgroup_entry + 1);
        break;
    case cg_FILTER:
        snprintf(prefix, PREFIX_SIZE, "Filter %i", ctrlgroup_entry + 1);
        break;
    case cg_ENV:
        if (ctrlgroup_entry)
            snprintf(prefix, PREFIX_SIZE, "Filter EG");
        else
            snprintf(prefix, PREFIX_SIZE, "Amp EG");
        break;
    case cg_LFO:
    {
        int a = ctrlgroup_entry + 1 - ms_lfo1;
        if (lfoPrefixOverride)
        {
            strxcpy(prefix, lfoPrefixOverride, PREFIX_SIZE);
        }
        else
        {
            if (a > n_lfos_voice)
                snprintf(prefix, PREFIX_SIZE, "Scene LFO %i", a - n_lfos_voice);
            else
                snprintf(prefix, PREFIX_SIZE, "LFO %i", a);
        }
    }
    break;
    case cg_FX:
        if (ctrlgroup_entry >= 0 && ctrlgroup_entry < n_fx_slots)
        {
            snprintf(prefix, PREFIX_SIZE, "%s", fxslot_shortnames[ctrlgroup_entry]);
        }
        else
        {
            snprintf(prefix, PREFIX_SIZE, "N/A");
        }
        break;
    default:
        prefix[0] = '\0';
        useprefix = false;
        break;
    };

    // note for future devs: code here used to make a buffer of 256 bytes, write into it, then
    // incorrectly truncate to NAMECHARS (which was 64 bytes) leading to unterminated strings.
    // now tfn is also NAMECHARS and unterminated strings aren't possible. But if some behaviour
    // changes, this comment exists.
    char tfn[NAMECHARS];

    if (useprefix)
    {
        snprintf(tfn, NAMECHARS, "%s %s", prefix, dn);
    }
    else
    {
        snprintf(tfn, NAMECHARS, "%s", dn);
    }

    strxcpy(fn, tfn, NAMECHARS);
}

void Parameter::set_name(const char *n)
{
    strxcpy(dispname, n, NAMECHARS);
    create_fullname(dispname, fullname, ctrlgroup, ctrlgroup_entry);
    parameterNameUpdated = true;
}

Parameter *Parameter::assign(ParameterIDCounter::promise_t idp, int pid, const char *name,
                             const char *dispname, const std::string_view altOSCname, int ctrltype,

                             const Surge::Skin::Connector &c,

                             int scene, ControlGroup ctrlgroup, int ctrlgroup_entry,
                             bool modulateable, int ctrlstyle, bool defaultDeactivation)
{
    assert(c.payload);
    auto r = assign(idp, pid, name, dispname, altOSCname, ctrltype, c.payload->id, c.payload->posx,
                    c.payload->posy, scene, ctrlgroup, ctrlgroup_entry, modulateable, ctrlstyle,
                    defaultDeactivation);

    r->hasSkinConnector = true;

    return r;
}

Parameter *Parameter::assign(ParameterIDCounter::promise_t idp, int pid, const char *name,
                             const char *dispname, const std::string_view altOSCname, int ctrltype,

                             std::string ui_identifier, int posx, int posy, int scene,
                             ControlGroup ctrlgroup, int ctrlgroup_entry, bool modulateable,
                             int ctrlstyle, bool defaultDeactivation)
{
    this->id_promise = idp.get();
    this->id = -1;
    this->param_id_in_scene = pid;
    this->ctrlgroup = ctrlgroup;
    this->ctrlgroup_entry = ctrlgroup_entry;
    this->posx = posx;
    this->posy = posy;
    this->modulateable = modulateable;
    this->scene = scene;
    this->ctrlstyle = ctrlstyle;
    this->storage = nullptr;

    char prefix[TXT_SIZE + 1] = {};

    strxcpy(this->ui_identifier, ui_identifier.c_str(), NAMECHARS);
    strxcpy(this->name, name, NAMECHARS);
    set_name(dispname);

    get_prefix(prefix, ctrlgroup, ctrlgroup_entry, scene);
    snprintf(name_storage, NAMECHARS, "%s%s", prefix, name);

    this->oscName =
        fmt::format("/param/{}", (altOSCname.empty() ? this->name_storage : altOSCname));

    posy_offset = 0;
    per_voice_processing = scene ? true : false;
    midictrl = -1;
    midichan = -1;
    miditakeover_status = sts_waiting_for_first_look;

    clear_flags();

    this->deactivated = defaultDeactivation;

    set_type(ctrltype);

    if (valtype == vt_float)
    {
        val.f = val_default.f;
    }

    bound_value();

    return this;
}

void Parameter::clear_flags()
{
    temposync = false;
    set_extend_range(false);
    absolute = false;
    deactivated = true; // CHOICE: if you are a deactivatble parameter make it so you are by default
    porta_constrate = false;
    porta_gliss = false;
    porta_retrigger = false;
    porta_curve = porta_lin;
}

bool Parameter::can_temposync() const
{
    switch (ctrltype)
    {
    case ct_portatime:
    case ct_lforate:
    case ct_lforate_deactivatable:
    case ct_envtime:
    case ct_envtime_deformable:
    case ct_envtime_deactivatable:
    case ct_envtime_linkable_delay:
    case ct_envtime_lfodecay:
    case ct_reverbpredelaytime:
    case ct_floaty_delay_time:
        return true;
    }
    return false;
}

bool Parameter::can_extend_range() const
{
    switch (ctrltype)
    {
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    case ct_pitch_semi7bp:
    case ct_pitch_semi7bp_absolutable:
    case ct_pitch_extendable_very_low_minval:
    case ct_freq_fm2_offset:
    case ct_freq_reson_band1:
    case ct_freq_reson_band2:
    case ct_freq_reson_band3:
    case ct_freq_shift:
    case ct_decibel_extendable:
    case ct_decibel_narrow_extendable:
    case ct_decibel_narrow_short_extendable:
    case ct_oscspread:
    case ct_oscspread_bipolar:
    case ct_filter_feedback:
    case ct_osc_feedback:
    case ct_osc_feedback_negative:
    case ct_lfoamplitude:
    case ct_lfophaseshuffle:
    case ct_fmratio:
    case ct_reson_res_extendable:
    case ct_freq_audible_fm3_extendable:
    case ct_freq_audible_with_tunability:
    case ct_freq_audible_very_low_minval:
    case ct_percent_oscdrift:
    case ct_twist_aux_mix:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
    case ct_dly_fb_clippingmodes:
    case ct_bonsai_bass_boost:
    case ct_detuning:

    // Extendable integers are really rare and special.
    // If you add one, you may want to chat with us on Discord!
    case ct_pbdepth:
        return true;
    }
    return false;
}

bool Parameter::can_be_absolute() const
{
    switch (ctrltype)
    {
    case ct_oscspread:
    case ct_oscspread_bipolar:
    case ct_pitch_semi7bp_absolutable:
    case ct_fmratio:
        return true;
    }
    return false;
}

bool Parameter::can_deactivate() const
{
    switch (ctrltype)
    {
    case ct_percent_deactivatable:
    case ct_percent_bipolar_deactivatable:
    case ct_freq_hpf:
    case ct_freq_audible_deactivatable:
    case ct_freq_audible_deactivatable_hp:
    case ct_freq_audible_deactivatable_lp:
    case ct_lforate_deactivatable:
    case ct_rotarydrive:
    case ct_airwindows_fx:
    case ct_decibel_deactivatable:
    case ct_decibel_narrow_deactivatable:
    case ct_decibel_extra_narrow_deactivatable:
    case ct_envtime_linkable_delay:
    case ct_envtime_deactivatable:
    case ct_tape_drive:
    case ct_tape_speed:
    case ct_wstype:
    case ct_filtertype:
    case ct_amplitude_clipper:
        return true;
    }
    return false;
}

bool Parameter::has_portaoptions() const
{
    if (ctrltype == ct_portatime)
        return true;
    else
        return false;
}

bool Parameter::has_deformoptions() const
{
    switch (ctrltype)
    {
    case ct_freq_hpf:
    case ct_percent_with_string_deform_hook:
    case ct_percent_bipolar_with_string_filter_hook:
    case ct_lfodeform:
    case ct_modern_trimix:
    case ct_alias_mask:
    case ct_tape_drive:
    case ct_dly_fb_clippingmodes:
    case ct_noise_color:
    case ct_amplitude_ringmod:
    case ct_bonsai_bass_boost:
    case ct_envtime_deformable:
    case ct_filter_feedback:
    case ct_osc_feedback_negative:
    case ct_countedset_percent_extendable_wtdeform:
        return true;
    default:
        break;
    }

    return false;
}

bool Parameter::is_bipolar() const
{
    if (dynamicBipolar != nullptr)
    {
        auto res = dynamicBipolar->getValue(this);
        return res;
    }

    bool res = false;
    switch (ctrltype)
    {
    case ct_decibel:
    case ct_decibel_deactivatable:
    case ct_decibel_narrow:
    case ct_decibel_narrow_deactivatable:
    case ct_decibel_narrow_extendable:
    case ct_decibel_extra_narrow_deactivatable:
    case ct_decibel_narrow_short_extendable:
    case ct_decibel_extra_narrow:
    case ct_decibel_extendable:
    case ct_freq_mod:
    case ct_percent_bipolar:
    case ct_percent_bipolar_with_string_filter_hook:
    case ct_percent_bipolar_stereo:
    case ct_percent_bipolar_pan:
    case ct_percent_bipolar_stringbal:
    case ct_percent_bipolar_deactivatable:
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    case ct_noise_color:
    case ct_twist_aux_mix:
    case ct_freq_fm2_offset:
    case ct_freq_shift:
    case ct_filter_feedback:
    case ct_osc_feedback_negative:
    case ct_lfodeform:
    case ct_airwindows_param_bipolar:
    case ct_pitch:
    case ct_pitch4oct:
    case ct_modern_trimix:
    case ct_oscspread_bipolar:
    case ct_floaty_delay_playrate:
    case ct_bonsai_bass_boost:
        res = true;
        break;
    case ct_lfoamplitude:
    case ct_lfophaseshuffle:
        if (extend_range)
            res = true;
        break;
    case ct_fmratio:
    {
        if (extend_range && !absolute)
            res = true;
    }
    break;
    case ct_dly_fb_clippingmodes:
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    {
        if (extend_range)
            res = true;
    }
    break;
    default:
        res = false;
    }
    return res;
}

bool Parameter::is_discrete_selection() const
{
    switch (ctrltype)
    {
    case ct_sinefmlegacy:
    case ct_sineoscmode:
    case ct_ringmod_sineoscmode:
    case ct_wt2window:
    case ct_airwindows_fx:
    case ct_flangermode:
    case ct_fxlfowave:
    case ct_distortion_waveshape:
    case ct_reson_mode:
    case ct_vocoder_bandcount:
    case ct_nimbusmode:
    case ct_nimbusquality:
    case ct_stringosc_excitation_model:
    case ct_twist_engine:
    case ct_ensemble_stages:
    case ct_alias_wave:
    case ct_wstype:
    case ct_mscodec:
    case ct_reverbshape:
    case ct_bonsai_sat_filter:
    case ct_bonsai_sat_mode:
    case ct_bonsai_noise_mode:
        return true;
    default:
        break;
    }

    return false;
}

bool Parameter::is_nonlocal_on_change() const
{
    switch (ctrltype)
    {
    case ct_twist_engine:
    case ct_phaser_stages:
    case ct_nimbusmode:
    case ct_mscodec:
        return true;
    default:
        break;
    }
    return false;
}

bool Parameter::appears_deactivated() const
{
    if (dynamicDeactivation)
        return dynamicDeactivation->getValue(this);

    if (can_deactivate())
        return deactivated;

    return false;
}

Parameter *Parameter::get_primary_deactivation_driver() const
{
    if (dynamicDeactivation)
        return dynamicDeactivation->getPrimaryDeactivationDriver(this);
    return nullptr;
}

void Parameter::set_user_data(ParamUserData *ud)
{
    switch (ctrltype)
    {
    case ct_countedset_percent:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
        if (dynamic_cast<CountedSetUserData *>(ud))
        {
            user_data = ud;
        }
        else
        {
            user_data = nullptr;
        }
        break;
    case ct_airwindows_fx:
    case ct_filtertype:
    case ct_alias_wave:
    case ct_wstype:
    case ct_distortion_waveshape:
        if (dynamic_cast<ParameterDiscreteIndexRemapper *>(ud))
        {
            user_data = ud;
        }
        else
        {
            user_data = nullptr;
        }
        break;
    case ct_airwindows_param:
    case ct_airwindows_param_bipolar:
    case ct_airwindows_param_integral:
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
        if (dynamic_cast<ParameterExternalFormatter *>(ud))
        {
            user_data = ud;
        }
        else
        {
            user_data = nullptr;
        }
        break;
    default:
        std::cout << "Setting userdata on a non-supporting param ignored" << std::endl;
        user_data = nullptr;
        break;
    }
}

void Parameter::set_type(int ctrltype)
{
    this->ctrltype = ctrltype;
    posy_offset = 0;
    moverate = 1.f;

    affect_other_parameters = false;
    user_data = nullptr;
    dynamicName = nullptr;
    dynamicBipolar = nullptr;
    dynamicDeactivation = nullptr;

    basicBlocksParamMetaData = {};

    /*
    ** Note we now have two ctrltype switches. This one sets ranges
    ** and, grouped below, we set display info
    */
    switch (ctrltype)
    {
    case ct_pitch:
    case ct_pitch_extendable_very_low_minval:
        valtype = vt_float;
        val_min.f = -60;
        val_max.f = 60;
        val_default.f = 0;
        break;
    case ct_pitch4oct:
        valtype = vt_float;
        val_min.f = -48;
        val_max.f = 48;
        val_default.f = 0;
        break;
    case ct_syncpitch:
        valtype = vt_float;
        val_min.f = 0;
        val_max.f = 60;
        val_default.f = 0;
        break;
    case ct_fmratio:
        valtype = vt_float;
        val_min.f = 0;
        val_max.f = 32;
        val_default.f = 1;
        moverate = 0.5f;
        break;
    case ct_fmratio_int:
        valtype = vt_int;
        val_min.i = 1;
        val_max.i = 32;
        val_default.i = 1;
        break;
    case ct_pbdepth:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 24;
        val_default.i = 2;
        break;
    case ct_pitch_semi7bp:
    case ct_pitch_semi7bp_absolutable:
        valtype = vt_float;
        val_min.f = -7;
        val_max.f = 7;
        moverate = 0.5f;
        val_default.f = 0;
        break;
    case ct_freq_audible:
    case ct_freq_audible_deactivatable:
    case ct_freq_audible_fm3_extendable:
    case ct_freq_audible_with_tunability:
        valtype = vt_float;
        val_min.f = -60;   // 13.75 Hz
        val_max.f = 70;    // 25087.71 Hz
        val_default.f = 3; // 523.25 Hz
        break;
    case ct_freq_audible_deactivatable_hp:
        valtype = vt_float;
        val_min.f = -60;     // 13.75 Hz
        val_max.f = 70;      // 25087.71 Hz
        val_default.f = -60; // 13.75 Hz
        break;
    case ct_freq_audible_deactivatable_lp:
        valtype = vt_float;
        val_min.f = -60;    // 13.75 Hz
        val_max.f = 70;     // 25087.71 Hz
        val_default.f = 70; // 25087.71 Hz
        break;
    case ct_freq_audible_very_low_minval:
        valtype = vt_float;
        val_min.f = -117.3763; // 0.5 Hz
        val_max.f = 70;        // 25087.71 Hz
        val_default.f = 3;     // 523.25 Hz
        break;
    case ct_freq_hpf:
        valtype = vt_float;
        val_min.f = -72;     // 6.88 Hz
        val_max.f = 15;      // 1046.5 Hz
        val_default.f = -72; // 6.88 Hz
        break;
    case ct_freq_reson_band1:
        valtype = vt_float;
        val_min.f = -34.4936f;     // 60 Hz
        val_max.f = -6.6305f;      // 300 Hz
        val_default.f = -18.6305f; // 150 Hz
        break;
    case ct_freq_reson_band2:
        valtype = vt_float;
        val_min.f = -6.6305f;      // 300 Hz
        val_max.f = 21.23265f;     // 1500 Hz
        val_default.f = 8.038216f; // 700 Hz
        break;
    case ct_freq_reson_band3:
        valtype = vt_float;
        val_min.f = 21.23265f;     // 1500 Hz
        val_max.f = 49.09578;      // 7500 Hz
        val_default.f = 35.90135f; // 3500 Hz
        break;
    case ct_freq_vocoder_low:
        valtype = vt_float;
        val_min.f = -36;    // 55 Hz
        val_max.f = 36;     // 3520 Hz
        val_default.f = -3; // 370 Hz
        break;
    case ct_freq_vocoder_high:
        valtype = vt_float;
        val_min.f = 0;      // 440 Hz
        val_max.f = 60;     // 14080 kHz
        val_default.f = 49; // 7458.62 kHz
        break;
    case ct_freq_mod:
        valtype = vt_float;
        val_min.f = -96;   // semitones
        val_max.f = 96;    // semitones
        val_default.f = 0; // semitones
        moverate = 0.5f;
        break;
    case ct_freq_fm2_offset:
    case ct_freq_shift:
        valtype = vt_float;
        val_min.f = -10;   // Hz
        val_max.f = 10;    // Hz
        val_default.f = 0; // Hz
        break;
    case ct_freq_ringmod:
        valtype = vt_float;
        val_min.f = 0;      // 8.18 Hz
        val_max.f = 127;    // 12543.86 Hz
        val_default.f = 60; // 261.63 Hz
        break;
    case ct_bandwidth:
        valtype = vt_float;
        val_min.f = 0;
        val_max.f = 5;
        val_default.f = 1;
        break;
    case ct_decibel:
    case ct_decibel_extendable:
    case ct_decibel_deactivatable:
        valtype = vt_float;
        val_min.f = -48;
        val_max.f = 48;
        val_default.f = 0;
        break;
    case ct_decibel_attenuation:
    case ct_decibel_attenuation_clipper:
        valtype = vt_float;
        val_min.f = -48;
        val_max.f = 0;
        val_default.f = 0;
        break;
    case ct_decibel_attenuation_large:
        valtype = vt_float;
        val_min.f = -96;
        val_max.f = 0;
        val_default.f = 0;
        break;
    case ct_decibel_attenuation_plus12:
        valtype = vt_float;
        val_min.f = -48;
        val_max.f = 12;
        val_default.f = 0;
        break;
    case ct_decibel_fmdepth:
        valtype = vt_float;
        val_min.f = -48;
        val_max.f = 24;
        val_default.f = 0;
        break;
    case ct_decibel_narrow:
    case ct_decibel_narrow_deactivatable:
    case ct_decibel_narrow_extendable:
    case ct_decibel_narrow_short_extendable:
        valtype = vt_float;
        val_min.f = -24;
        val_max.f = 24;
        val_default.f = 0;
        break;
    case ct_decibel_extra_narrow:
    case ct_decibel_extra_narrow_deactivatable:
        valtype = vt_float;
        val_min.f = -12;
        val_max.f = 12;
        val_default.f = 0;
        break;
    case ct_portatime:
        valtype = vt_float;
        val_min.f = -8;
        val_max.f = 2;
        val_default.f = -8;
        break;
    case ct_envtime:
    case ct_envtime_deformable:
    case ct_envtime_deactivatable:
    case ct_envtime_linkable_delay:
    case ct_envtime_lfodecay:
        valtype = vt_float;
        val_min.f = -8;
        val_max.f = 5;
        val_default.f = 0;
        break;
    case ct_floaty_warp_time:
        valtype = vt_float;
        val_min.f = -3;
        val_max.f = 4;
        val_default.f = 0;
        break;
    case ct_floaty_delay_time:
        valtype = vt_float;
        val_min.f = -5.64386;     // 20ms
        val_max.f = 3;            // 8s
        val_default.f = -1.73697; // 300ms
        break;
    case ct_delaymodtime:
    case ct_chorusmodtime:
        valtype = vt_float;
        val_min.f = -11;
        val_max.f = -3;
        val_default.f = -6;
        break;
    case ct_reverbtime:
        valtype = vt_float;
        val_min.f = -4;
        val_max.f = 6;
        val_default.f = 1;
        break;
    case ct_reverbpredelaytime:
        valtype = vt_float;
        val_min.f = -8;
        val_max.f = 1;
        val_default.f = -2;
        break;
    case ct_lforate:
    case ct_lforate_deactivatable:
        valtype = vt_float;
        val_min.f = -7;
        val_max.f = 9;
        val_default.f = 0;
        moverate = 0.33f;
        break;
    case ct_ensemble_lforate:
        valtype = vt_float;
        val_min.f = log2(0.01f);
        val_max.f = log2(20.f);
        val_default.f = 0;
        moverate = 0.5f;
        break;
    case ct_ensemble_clockrate:
        valtype = vt_float;
        val_min.f = 1.5f;
        val_max.f = 100.f;
        val_default.f = 40.f;
        break;
    case ct_lfotrigmode:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_lfo_trigger_modes - 1;
        val_default.i = 0;
        break;
    case ct_pitch_octave:
        valtype = vt_int;
        val_min.i = -3;
        val_max.i = 3;
        val_default.i = 0;
        break;
    case ct_bool_mute:
    case ct_bool_solo:
    case ct_bool_keytrack:
    case ct_bool_retrigger:
    case ct_bool_relative_switch:
    case ct_bool_link_switch:
    case ct_bool_unipolar:
    case ct_bool:
        valtype = vt_bool;
        val_min.i = 0;
        val_max.i = 1;
        val_default.i = 0;
        break;
    case ct_float_toggle:
        valtype = vt_float;
        val_min.f = 0.f;
        val_max.f = 1.f;
        val_default.f = 0.f;
        break;
    case ct_osctype:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = n_osc_types - 1;
        /*
         * BP: We do, but this isn't how we load osces
         */
        affect_other_parameters = false;
        break;
    case ct_reverbshape:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = 3;
        break;
    case ct_fxtype:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = n_fx_types - 1;
        // affect_other_parameters = true;	// Can not be added, before it has a custom
        // controltype
        break;
    case ct_fxbypass:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = n_fx_bypass - 1;
        break;
    case ct_oscroute:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 2;
        val_default.i = 1;
        break;
    case ct_envshape:
    case ct_envshape_attack:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 2;
        val_default.i = 0;
        break;
    case ct_envmode:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 1;
        val_default.i = 0;
        break;
    case ct_stereowidth:
        valtype = vt_float;
        val_min.f = 0;
        val_max.f = 120;
        val_default.f = 90;
        break;
    case ct_lfotype:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_lfo_types - 1;
        val_default.i = 0;
        break;
    case ct_fbconfig:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_filter_configs - 1;
        val_default.i = 0;
        break;
    case ct_fmconfig:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_fm_routings - 1;
        val_default.i = 0;
        break;
    case ct_filtertype:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = sst::filters::num_filter_types - 1;
        val_default.i = 0;
        break;
    case ct_filtersubtype:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_max_filter_subtypes - 1;
        val_default.i = 0;
        break;
    case ct_wstype:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = (int)sst::waveshapers::WaveshaperType::n_ws_types - 1;
        val_default.i = 0;
        break;
    case ct_midikey_or_channel:
    case ct_midikey:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 127;
        val_default.i = 60;
        break;
    case ct_wt2window:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 8;
        val_default.i = 0;
        break;
    case ct_osccount:
        valtype = vt_int;
        val_min.i = 1;
        val_max.i = 16;
        val_default.i = 1;
        break;
    case ct_scenemode:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_scene_modes - 1;
        val_default.i = 0;
        break;
    case ct_polymode:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = n_play_modes - 1;
        val_default.i = 0;
        break;
    case ct_polylimit:
        valtype = vt_int;
        val_min.i = 2;
        val_max.i = 64;
        val_default.i = 16;
        break;
    case ct_scenesel:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 1;
        val_default.i = 0;
        break;
    case ct_percent:
    case ct_percent_with_string_deform_hook:
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    case ct_percent_deactivatable:
    case ct_dly_fb_clippingmodes:
    case ct_percent_oscdrift:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_oscspread:
        val_min.f = 0.f;
        val_max.f = 1.f;
        valtype = vt_float;
        val_default.f = 0.1;
        break;
    case ct_oscspread_bipolar:
        val_min.f = -1.f;
        val_max.f = 1.f;
        valtype = vt_float;
        val_default.f = 0.0;
        break;
    case ct_detuning:
        val_min.f = 0;
        val_max.f = 2;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_lfodeform:
        val_min.f = -1;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_amplitude:
    case ct_amplitude_clipper:
    case ct_lfoamplitude:
    case ct_amplitude_ringmod:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 1;
        break;
    case ct_lfophaseshuffle:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_modern_trimix:
    case ct_percent_bipolar:
    case ct_percent_bipolar_with_string_filter_hook:
    case ct_percent_bipolar_deactivatable:
    case ct_percent_bipolar_stereo:
    case ct_percent_bipolar_pan:
    case ct_percent_bipolar_stringbal:
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    case ct_twist_aux_mix:
    case ct_noise_color:
        val_min.f = -1;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_character:
        val_min.i = 0;
        val_max.i = 2;
        valtype = vt_int;
        val_default.i = 1;
        break;
    case ct_sineoscmode:
        val_min.i = 0;
        val_max.i = 27;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_ringmod_sineoscmode:
        val_min.i = 0;
        val_max.i = 28; // above sineosc + 1 for audio in
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_sinefmlegacy:
        val_min.i = 0;
        val_max.i = 1;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_reson_mode:
        val_min.i = 0;
        val_max.i = 3;
        valtype = vt_int;
        val_default.i = 1;
        break;
    case ct_reson_res_extendable:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0.75f;
        break;
    case ct_vocoder_bandcount:
        val_min.i = 4;
        val_max.i = 20;
        valtype = vt_int;
        val_default.i = 20;
        break;
    case ct_vocoder_modulator_mode:
        val_min.i = 0;
        val_max.i = 3;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_distortion_waveshape:
        val_min.i = 0;
        val_max.i = n_fxws - 1;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_mscodec:
        val_min.i = 0;
        val_max.i = 2;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_countedset_percent:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_flangerpitch:
        val_min.f = 0;
        val_max.f = 127;
        valtype = vt_float;
        val_default.f = 60;
        break;
    case ct_flangervoices:
        val_min.f = 1;
        val_max.f = 4;
        valtype = vt_float;
        val_default.f = 4;
        break;
    case ct_flangermode:
        val_min.i = 0;
        val_max.i = 3; // classic, dopler, arpmix, arpsolo
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_fxlfowave:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 5; // sin, tri, saw, s&g, s&h, square
        val_default.i = 0;
        break;
    case ct_twist_engine:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 15;
        val_default.i = 0;
        break;
    case ct_ensemble_stages:
    {
#if defined(_M_ARM64EC)
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 1;
        val_default.i = 0;
#else
        extern int ensemble_stage_count();
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = ensemble_stage_count() - 1;
        val_default.i = 0;
#endif
        break;
    }
    case ct_stringosc_excitation_model:
    {
        extern int stringosc_excitations_count();
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = stringosc_excitations_count() - 1;
        val_default.i = 0;
        break;
    }
    case ct_alias_wave:
    {
        extern int alias_waves_count();
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = alias_waves_count() - 1;
        val_default.i = 0;
        break;
    }
    case ct_alias_mask:
    {
        valtype = vt_float;
        val_min.f = 0.f;
        val_max.f = 1.f;
        val_default.f = 0.f;
        break;
    }
    case ct_alias_bits:
    {
        valtype = vt_float;
        val_min.f = 1.f;
        val_max.f = 8.f;
        val_default.f = 8.f;
        break;
    }
    case ct_flangerspacing:
        val_min.f = 0;
        val_max.f = 12;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_osc_feedback:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_filter_feedback:
    case ct_osc_feedback_negative:
        val_min.f = -1;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_percent200:
        val_min.f = 0;
        val_max.f = 2;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_phaser_stages:
        val_min.i = 1;
        val_max.i = 16;
        valtype = vt_int;
        val_default.i = 4;
        break;
    case ct_rotarydrive:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_sendlevel:
        val_min.f = 0;
        val_max.f = 1.5874011517f; // +12 dB
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_airwindows_fx:
        val_min.i = 0;
        val_max.i = 10;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_airwindows_param:
    case ct_airwindows_param_bipolar: // it's still 0 ... 1 - this is just a display thing
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;
    case ct_airwindows_param_integral:
        val_min.i = 0;
        val_max.i = 1;
        valtype = vt_int;
        val_default.i = 0;
        break;
    case ct_phaser_spread:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0.5;
        break;
    case ct_chow_ratio:
        val_min.f = 1;
        val_max.f = 20;
        valtype = vt_float;
        val_default.f = 10.f;
        break;

    case ct_nimbusmode:
        valtype = vt_int;
        val_min.i = 0;
#if EURORACK_CLOUDS_IS_SUPERPARASITES
        val_max.i = 7;
#else
        val_max.i = 3;
#endif
        val_default.i = 0;
        break;

    case ct_nimbusquality:
        valtype = vt_int;
        val_min.i = 0;
        val_max.i = 3; // mono LQ, stereo LQ, mono HQ, stereo HQ
        val_default.i = 0;
        break;

    case ct_comp_attack_ms:
    case ct_comp_release_ms:
        valtype = vt_float;
        val_min.f = 0.f;
        val_max.f = 1.f;
        val_default.f = 0.5f;
        break;

    case ct_tape_drive:
        val_min.f = 0;
        val_max.f = 1;
        valtype = vt_float;
        val_default.f = 0;
        break;

    case ct_tape_microns:
        valtype = vt_float;
        val_min.f = 0.f;
        val_max.f = 500.f;
        val_default.f = 50.0f;
        break;

    case ct_tape_speed:
        valtype = vt_float;
        val_min.f = 1.0f;
        val_max.f = 50.0f;
        val_default.f = 30.0f;
        break;

    case ct_spring_decay:
        valtype = vt_float;
        val_min.f = 0.0f;
        val_max.f = 1.0f;
        val_default.f = 0.5f;
        break;

    case ct_bonsai_bass_boost:
        valtype = vt_float;
        val_min.f = -24.0f;
        val_max.f = 24.0f;
        val_default.f = 0.f;
        break;

    case ct_bonsai_sat_filter:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = 1;
        break;

    case ct_bonsai_sat_mode:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 1;
        val_max.i = 3;
        break;

    case ct_bonsai_noise_mode:
        valtype = vt_int;
        val_min.i = 0;
        val_default.i = 0;
        val_max.i = 1;
        break;

    case ct_floaty_delay_playrate:
        valtype = vt_float;
        val_min.f = -5.f;
        val_default.f = 1.f;
        val_max.f = 5.f;
        break;

    case ct_none:
    default:
        snprintf(dispname, NAMECHARS, "-");
        valtype = vt_int;

        val_min.i = std::numeric_limits<int>::min();
        val_max.i = std::numeric_limits<int>::max();
        val_default.i = 0;
        break;
    }

    /*
    ** Setup display info here
    */
    displayType = Custom;
    DisplayInfo d; // reset everything to default
    displayInfo = d;
    displayInfo.unit[0] = 0;
    displayInfo.absoluteUnit[0] = 0;
    displayInfo.minLabel[0] = 0;
    displayInfo.maxLabel[0] = 0;

    switch (ctrltype)
    {
    case ct_percent:
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    case ct_percent_with_string_deform_hook:
    case ct_dly_fb_clippingmodes:
    case ct_percent_bipolar_deactivatable:
    case ct_percent_deactivatable:
    case ct_percent_oscdrift:
    case ct_percent200:
    case ct_percent_bipolar:
    case ct_noise_color:
    case ct_percent_bipolar_with_string_filter_hook:
    case ct_lfodeform:
    case ct_rotarydrive:
    case ct_countedset_percent:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
    case ct_lfoamplitude:
    case ct_lfophaseshuffle:
    case ct_reson_res_extendable:
    case ct_modern_trimix:
    case ct_floaty_warp_time:
    case ct_floaty_delay_time:
    case ct_floaty_delay_playrate:
    case ct_alias_mask:
    case ct_tape_drive:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        displayInfo.scale = 100.f;
        break;
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    case ct_twist_aux_mix:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        displayInfo.scale = 100.f;
        displayInfo.customFeatures = ParamDisplayFeatures::kScaleBasedOnIsBiPolar;
        break;
    case ct_percent_bipolar_stereo:
        displayType = LinearWithScale;
        displayInfo.customFeatures = ParamDisplayFeatures::kHasCustomMinString |
                                     ParamDisplayFeatures::kHasCustomMaxString |
                                     ParamDisplayFeatures::kHasCustomDefaultString;

        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        snprintf(displayInfo.minLabel, DISPLAYINFO_TXT_SIZE, "-100.00 %% (Left)");
        snprintf(displayInfo.defLabel, DISPLAYINFO_TXT_SIZE, "0.00 %% (Stereo)");
        snprintf(displayInfo.maxLabel, DISPLAYINFO_TXT_SIZE, "100.00 %% (Right)");
        displayInfo.scale = 100.f;
        break;
    case ct_percent_bipolar_pan:
        displayType = LinearWithScale;
        displayInfo.customFeatures = ParamDisplayFeatures::kHasCustomMinString |
                                     ParamDisplayFeatures::kHasCustomMaxString |
                                     ParamDisplayFeatures::kHasCustomDefaultString;

        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        snprintf(displayInfo.minLabel, DISPLAYINFO_TXT_SIZE, "-100.00 %% (Left)");
        snprintf(displayInfo.defLabel, DISPLAYINFO_TXT_SIZE, "0.00 %% (Center)");
        snprintf(displayInfo.maxLabel, DISPLAYINFO_TXT_SIZE, "100.00 %% (Right)");
        displayInfo.scale = 100.f;
        break;
    case ct_percent_bipolar_stringbal:
        displayType = LinearWithScale;
        displayInfo.customFeatures = ParamDisplayFeatures::kHasCustomMinString |
                                     ParamDisplayFeatures::kHasCustomMaxString |
                                     ParamDisplayFeatures::kHasCustomDefaultString;

        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        snprintf(displayInfo.minLabel, DISPLAYINFO_TXT_SIZE, "-100.00 %% (String 1)");
        snprintf(displayInfo.defLabel, DISPLAYINFO_TXT_SIZE, "0.00 %% (Strings 1+2)");
        snprintf(displayInfo.maxLabel, DISPLAYINFO_TXT_SIZE, "100.00 %% (String 2)");
        displayInfo.scale = 100.f;
        break;

        /*
          Again the missing breaks here are on purpose but we pick out a few for Tuning later
         */
    case ct_pitch_semi7bp_absolutable:
        displayInfo.absoluteFactor = 10.f;
        snprintf(displayInfo.absoluteUnit, DISPLAYINFO_TXT_SIZE, "Hz");
    case ct_pitch_semi7bp:
    case ct_flangerspacing:
        displayInfo.extendFactor = 12.f;
    case ct_pitch:
    case ct_pitch4oct:
    case ct_pitch_extendable_very_low_minval:
    case ct_syncpitch:
    case ct_freq_mod:
    case ct_flangerpitch:
        displayType = LinearWithScale;
        displayInfo.customFeatures = ParamDisplayFeatures::kUnitsAreSemitonesOrKeys;
        break;
    case ct_flangervoices:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Voices");
        break;

    case ct_freq_hpf:
    case ct_freq_audible:
    case ct_freq_audible_deactivatable:
    case ct_freq_audible_deactivatable_hp:
    case ct_freq_audible_deactivatable_lp:
    case ct_freq_audible_fm3_extendable:
    case ct_freq_audible_with_tunability:
    case ct_freq_audible_very_low_minval:
    case ct_freq_reson_band1:
    case ct_freq_reson_band2:
    case ct_freq_reson_band3:
    case ct_freq_vocoder_low:
    case ct_freq_vocoder_high:
    case ct_freq_ringmod:
        displayType = ATwoToTheBx;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Hz");
        if (ctrltype == ct_freq_ringmod)
        {
            displayInfo.a = 8.175798;
        }
        else if (ctrltype == ct_freq_audible_fm3_extendable)
        {
            displayInfo.a = 440 * pow(2.f, -9.f / 12.f);
        }
        else
        {
            displayInfo.a = 440.0;
        }
        displayInfo.b = 1.0f / 12.0f;
        displayInfo.decimals = 2;
        displayInfo.modulationCap = 880.f * powf(2.0, (val_max.f) / 12.0f);
        displayInfo.supportsNoteName = true;
        displayInfo.customFeatures = ParamDisplayFeatures::kAllowsModulationsInNotesAndCents;
        break;
    case ct_freq_fm2_offset:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Hz");
        displayInfo.extendFactor = 100.f;
        break;
    case ct_freq_shift:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Hz");
        displayInfo.scale = 10.f;
        displayInfo.extendFactor = 100.f;
        break;
    case ct_envtime_lfodecay:
        snprintf(displayInfo.maxLabel, DISPLAYINFO_TXT_SIZE, "Forever");
        displayInfo.customFeatures = ParamDisplayFeatures::kHasCustomMaxString;
        // THERE IS NO BREAK HERE ON PURPOSE so we group to the others
    case ct_portatime:
    case ct_envtime:
    case ct_envtime_deformable:
    case ct_envtime_deactivatable:
    case ct_envtime_linkable_delay:
    case ct_reverbtime:
    case ct_reverbpredelaytime:
    case ct_chorusmodtime:
    case ct_delaymodtime:
        displayType = ATwoToTheBx;
        displayInfo.customFeatures |= ParamDisplayFeatures::kHasCustomMinValue;
        displayInfo.customFeatures |= ParamDisplayFeatures::kSwitchesFromSecToMillisec;
        displayInfo.minLabelValue = 0.f;
        displayInfo.tempoSyncNotationMultiplier = 1.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "s");
        displayInfo.decimals = 2;
        break;

    case ct_lforate:
    case ct_lforate_deactivatable:
    case ct_ensemble_lforate:
        displayType = ATwoToTheBx;
        displayInfo.decimals = 3;
        displayInfo.tempoSyncNotationMultiplier = -1.0f;
        displayInfo.modulationCap = 512 * 8;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Hz");
        break;

    case ct_decibel_extendable:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "dB");
        displayInfo.extendFactor = 3.f;
        break;

    case ct_decibel_narrow_extendable:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "dB");
        displayInfo.extendFactor = 5.f;
        break;

    case ct_decibel_narrow_short_extendable:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "dB");
        displayInfo.extendFactor = 2.f;
        break;

    case ct_decibel:
    case ct_decibel_attenuation:
    case ct_decibel_attenuation_clipper:
    case ct_decibel_attenuation_large:
    case ct_decibel_attenuation_plus12:
    case ct_decibel_fmdepth:
    case ct_decibel_narrow:
    case ct_decibel_extra_narrow:
    case ct_decibel_deactivatable:
    case ct_decibel_narrow_deactivatable:
    case ct_decibel_extra_narrow_deactivatable:
    case ct_bonsai_bass_boost:
        displayType = LinearWithScale;
        displayInfo.extendFactor = 3.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "dB");
        break;

    case ct_bandwidth:
        displayType = LinearWithScale;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "octaves");
        break;

    case ct_detuning:
        displayType = LinearWithScale;
        displayInfo.scale = 100.f;
        displayInfo.extendFactor = 6.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "cents");
        break;

    case ct_stereowidth:
        displayType = LinearWithScale;
        displayInfo.scale = 1.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "º");
        break;

    case ct_oscspread:
    case ct_oscspread_bipolar:
        displayType = LinearWithScale;
        displayInfo.scale = 100.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "cents");
        snprintf(displayInfo.absoluteUnit, DISPLAYINFO_TXT_SIZE, "Hz");
        displayInfo.absoluteFactor =
            0.16; // absolute factor also takes scale into account hence the /100
        displayInfo.extendFactor = 12.f;
        break;

    case ct_filter_feedback:
    case ct_osc_feedback:
    case ct_osc_feedback_negative:
        displayType = LinearWithScale;
        displayInfo.scale = 100.f;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        displayInfo.extendFactor = 4.f;
        break;

    case ct_amplitude:
    case ct_amplitude_clipper:
    case ct_sendlevel:
    case ct_amplitude_ringmod:
        displayType = Decibel;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "dB");
        break;

    case ct_airwindows_param:
    case ct_airwindows_param_bipolar:
    case ct_airwindows_param_integral:
        displayType = DelegatedToFormatter;
        displayInfo.scale = 1.f;
        displayInfo.unit[0] = 0;
        displayInfo.decimals = 3;
        break;

    case ct_comp_attack_ms:
        displayType = ATwoToTheBx;
        displayInfo.a = 1.0f;
        displayInfo.b = std::log2(100.0f / 1.0f);
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "ms");
        break;

    case ct_comp_release_ms:
        displayType = ATwoToTheBx;
        displayInfo.a = 10.0f;
        displayInfo.b = std::log2(1000.0f / 10.0f);
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "ms");
        break;

    case ct_ensemble_clockrate:
        displayType = LinearWithScale;
        displayInfo.scale = 1.f;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "kHz");
        break;

    case ct_alias_bits:
        displayType = LinearWithScale;
        displayInfo.scale = 1.f;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "bits");
        break;

    case ct_tape_microns:
        displayType = LinearWithScale;
        displayInfo.scale = 1.f;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "μm");
        break;

    case ct_tape_speed:
        displayType = LinearWithScale;
        displayInfo.scale = 1.f;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "ips");
        break;

    case ct_spring_decay:
        displayType = ATwoToTheBx;
        displayInfo.a = 0.5f;
        displayInfo.b = std::log2(4.5f / 0.5f);
        displayInfo.customFeatures |= ParamDisplayFeatures::kSwitchesFromSecToMillisec;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "s");
        break;

    case ct_pbdepth:
        displayInfo.customFeatures = ParamDisplayFeatures::kUnitsAreSemitonesOrKeys;
        break;

    case ct_float_toggle:
        displayInfo.scale = 100.f;
        displayInfo.decimals = 2;
        snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "%%");
        snprintf(displayInfo.minLabel, DISPLAYINFO_TXT_SIZE, "Off");
        snprintf(displayInfo.maxLabel, DISPLAYINFO_TXT_SIZE, "On");
        break;
    }

    switch (ctrltype)
    {
    case ct_pitch_semi7bp_absolutable:
    case ct_pitch_semi7bp:
    case ct_pitch:
    case ct_pitch4oct:
    case ct_pitch_extendable_very_low_minval:
    case ct_syncpitch:
    case ct_oscspread:
        displayInfo.customFeatures |= kAllowsTuningFractionTypein;
        break;
    default:
        break;
    }
}

void Parameter::bound_value(bool force_integer)
{
    if (temposync && (valtype == vt_float))
    {
        float a, b = modff(val.f, &a);
        if (b < 0)
        {
            b += 1.f;
            a -= 1.f;
        }
        b = powf(2.0f, b);

        if (b > 1.41f)
        {
            b = log2(1.5f);
        }
        else if (b > 1.167f)
        {
            b = log2(1.3333333333f);
        }
        else
        {
            b = 0.f;
        }

        val.f = a + b;
    }

    if (force_integer && (valtype == vt_float))
    {
        switch (ctrltype)
        {
        case ct_percent:
        case ct_percent_with_extend_to_bipolar:
        case ct_percent_with_extend_to_bipolar_static_default:
        case ct_percent_with_string_deform_hook:
        case ct_percent_bipolar_deactivatable:
        case ct_percent_deactivatable:
        case ct_dly_fb_clippingmodes:
        case ct_percent_oscdrift:
        case ct_percent200:
        case ct_percent_bipolar:
        case ct_percent_bipolar_with_string_filter_hook:
        case ct_percent_bipolar_stereo:
        case ct_percent_bipolar_pan:
        case ct_percent_bipolar_stringbal:
        case ct_percent_bipolar_w_dynamic_unipolar_formatting:
        case ct_twist_aux_mix:
        case ct_noise_color:
        case ct_rotarydrive:
        case ct_filter_feedback:
        case ct_osc_feedback:
        case ct_osc_feedback_negative:
        case ct_detuning:
        case ct_lfoamplitude:
        case ct_lfophaseshuffle:
        case ct_airwindows_param:
        case ct_airwindows_param_bipolar:
        case ct_lfodeform:
        case ct_reson_res_extendable:
        case ct_modern_trimix:
        case ct_tape_drive:
        {
            val.f = floor(val.f * 100) / 100.0;
            break;
        }
        case ct_pitch:
        case ct_pitch4oct:
        case ct_pitch_semi7bp:
        case ct_syncpitch:
        {
            if (!extend_range)
            {
                val.f = floor(val.f + 0.5f);
            }

            break;
        }
        case ct_oscspread:
        case ct_oscspread_bipolar:
        {
            if (absolute)
            {
                if (extend_range)
                {
                    val.f = floor(val.f * 192) / 192.0;
                }
                else
                {
                    val.f = floor(val.f * 16) / 16.0;
                }
            }
            else if (extend_range)
            {
                val.f = floor(val.f * 120) / 120.0;
            }
            else
            {
                val.f = floor(val.f * 100) / 100.0;
            }

            break;
        }
        case ct_pitch_semi7bp_absolutable:
        {
            if (absolute)
                if (extend_range)
                {
                    val.f = floor(val.f * 120) / 120.0;
                }
                else
                {
                    val.f = floor(val.f * 12) / 12.0;
                }
            else
            {
                val.f = floor(val.f + 0.5f);
            }
            break;
        }
        case ct_amplitude:
        case ct_amplitude_clipper:
        case ct_sendlevel:
        case ct_amplitude_ringmod:
        {
            if (val.f != 0)
            {
                // we use round instead of floor because with some params
                // it wouldn't snap to max value (i.e. send levels)
                val.f = db_to_amp(round(amp_to_db(val.f)));
            }
            else
            {
                val.f =
                    -INFINITY; // this is so that the popup shows -inf proper instead of -192.0 dB
            }
            break;
        }
        case ct_decibel:
        case ct_decibel_narrow:
        case ct_decibel_narrow_extendable:
        case ct_decibel_narrow_short_extendable:
        case ct_decibel_extra_narrow:
        case ct_decibel_attenuation:
        case ct_decibel_attenuation_clipper:
        case ct_decibel_attenuation_large:
        case ct_decibel_attenuation_plus12:
        case ct_decibel_fmdepth:
        case ct_decibel_extendable:
        case ct_decibel_deactivatable:
        case ct_decibel_narrow_deactivatable:
        case ct_decibel_extra_narrow_deactivatable:
        case ct_bonsai_bass_boost:
        {
            val.f = floor(val.f);
            break;
        }
        case ct_chorusmodtime:
        {
            val.f = limit_range((float)log2(round(powf(2.0f, val.f) * 100) / 100.f), val_min.f,
                                val_max.f);
            break;
        }
        case ct_portatime:
        case ct_envtime:
        case ct_envtime_deformable:
        case ct_envtime_deactivatable:
        case ct_envtime_linkable_delay:
        case ct_envtime_lfodecay:
        case ct_reverbtime:
        case ct_reverbpredelaytime:
        case ct_delaymodtime:
            if (temposync)
            {
                val.f = floor(val.f + 0.5f);
            }
            else
            {
                val.f = log2(round(powf(2.0f, val.f) * 10) / 10.f);
            }
            break;
        case ct_ensemble_lforate:
        case ct_lforate:
        case ct_lforate_deactivatable:
        {
            if (temposync)
            {
                val.f = floor(val.f + 0.5f);
            }
            else if (val.f < 0)
            {
                val.f = limit_range((float)log2(round(powf(2.0f, val.f) * 10) / 10.f), val_min.f,
                                    val_max.f);
            }
            else
            {
                val.f = log2(round(powf(2.0f, val.f)));
            }

            break;
        }
        case ct_bandwidth:
        {
            val.f = floor(val.f * 10) / 10.0;
            break;
        }
        case ct_freq_fm2_offset:
        case ct_freq_shift:
        {
            if (extend_range)
            {
                val.f = floor((val.f * 1000.f) / 10.f) * 0.01f;
            }
            else
            {
                val.f = floor((val.f * 10.f) + 0.5f) * 0.1f;
            }

            break;
        }
        case ct_countedset_percent:
        case ct_countedset_percent_extendable:
        case ct_countedset_percent_extendable_wtdeform:
        {
            CountedSetUserData *cs = reinterpret_cast<CountedSetUserData *>(user_data);
            if (cs)
            {
                auto count = cs->getCountedSetSize();
                // OK so now val.f is between 0 and 1. So
                auto fraccount = val.f * (count - extend_range);
                auto intcount = (int)fraccount;
                val.f = limit_range(1.0 * intcount / std::max(1, (count - extend_range)) + 0.000001,
                                    0., 1.);
            }
            break;
        }
        case ct_alias_mask:
        {
            auto fraccount = val.f * 255;
            auto intcount = (int)fraccount;
            val.f = 1.0 * intcount / 255;
            break;
        }
        case ct_fmratio:
        {
            if (absolute)
            {
                auto bpv = (val.f - 16.0) / 16.0;
                auto note = 69.0 + (69.0 * bpv);
                note = round(note);

                val.f = note / 4.3125;
            }
            else if (extend_range)
            {
                float ratio;

                if (val.f > 16.f)
                {
                    ratio = round((val.f - 16.f) * 31.f / 16.f + 1.f);
                    val.f = 16.f + ((ratio - 1.f) / 1.9375); // 1.9375 = 31 / 16
                }
                else
                {
                    ratio = -round((16.f - val.f) * 31.f / 16.f + 1.f);
                    val.f = 16.f + ((ratio + 1.f) / 1.9375);
                }
            }
            else
            {
                val.f = floor(val.f + 0.5f);
            }

            break;
        }
        default:
        {
            val.f = floor(val.f + 0.5f);
            break;
        }
        }
    }

    if (ctrltype == ct_vocoder_bandcount)
    {
        val.i = val.i - val.i % 4;
    }

    switch (valtype)
    {
    case vt_float:
    {
        val.f = limit_range(val.f, val_min.f, val_max.f);
        break;
    }
    case vt_int:
    {
        val.i = limit_range(val.i, val_min.i, val_max.i);
        break;
    }
    };
}

bool Parameter::supportsDynamicName() const
{
    switch (ctrltype)
    {
    case ct_modern_trimix:
    case ct_lfophaseshuffle:
    case ct_percent:
    case ct_percent_bipolar:
    case ct_percent_bipolar_deactivatable:
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    case ct_twist_aux_mix:
    case ct_percent_deactivatable:
    case ct_pitch_extendable_very_low_minval:
    case ct_freq_audible_very_low_minval:
    case ct_tape_drive:
        return true;
    default:
        break;
    }
    return false;
}

const char *Parameter::get_name() const
{
    // We only even want to try this for specific types we know support it
    if (supportsDynamicName() && dynamicName)
        return dynamicName->getName(this);

    return dispname;
}

const char *Parameter::get_full_name() const
{
    if (supportsDynamicName() && dynamicName)
    {
        auto nm = dynamicName->getName(this);
        static char res[1024];
        create_fullname(nm, res, ctrlgroup, ctrlgroup_entry);
        return res;
    }

    return fullname;
}

const char *Parameter::get_internal_name() const { return name; }

const char *Parameter::get_storage_name() const { return name_storage; }

char *Parameter::get_storage_value(char *str) const
{
    switch (valtype)
    {
    case vt_int:
        snprintf(str, TXT_SIZE, "%i", val.i);
        break;
    case vt_bool:
        snprintf(str, TXT_SIZE, "%i", val.b ? 1 : 0);
        break;
    case vt_float:
        // While I could have moved this to the float_to_clocalestr in UnitConversions
        // when I cleaned up #7095, this is often used hard to test and not incorrect
        // so I'm going to consciously leave it.
        std::stringstream sst;
        sst.imbue(std::locale::classic());
        sst << std::fixed;
        sst << std::showpoint;
        sst << std::setprecision(14);
        sst << (double)val.f;
        strxcpy(str, sst.str().c_str(), TXT_SIZE);
        break;
    };

    return str;
}

void Parameter::set_storage_value(int i)
{
    switch (ctrltype)
    {
    default:
    {
        val.i = i;
        break;
    }
    }
}
void Parameter::set_storage_value(float f)
{
    switch (ctrltype)
    {
    default:
    {
        val.f = f;
        break;
    }
    }
}

void Parameter::set_extend_range(bool er)
{
    bool prior_extend = extend_range;

    extend_range = er;

    if (!extend_range)
    {
        switch (ctrltype)
        {
        case ct_percent_with_extend_to_bipolar:
            val_default.f = 0.f;
            break;
        case ct_pitch_extendable_very_low_minval:
        {
            val_min.f = -60.f;
            val_max.f = 60.f;
            val_default.f = 0.f;

            if (val.f < val_min.f)
            {
                val.f = val_min.f;
            }

            displayType = LinearWithScale;
            snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "semitones");
            displayInfo.supportsNoteName = false;
            displayInfo.customFeatures = ParamDisplayFeatures::kUnitsAreSemitonesOrKeys;
            displayInfo.customFeatures |= kAllowsTuningFractionTypein;
        }
        break;
        case ct_freq_audible_fm3_extendable:
        {
            val_min.f = -60; // 13.75 Hz
        }
        break;
        case ct_freq_reson_band1:
        {
            // Why the heck are we modifying this here?
            val_max.f = -6.6305f; // 300 Hz
        }
        break;
        case ct_freq_reson_band2:
        {
            val_min.f = -6.6305f;  // 300 Hz
            val_max.f = 21.23265f; // 1500 Hz
        }
        break;
        case ct_freq_reson_band3:
        {
            val_min.f = 21.23265f; // 1500 Hz
        }
        break;
        case ct_dly_fb_clippingmodes:
        case ct_lfoamplitude:
        {
            val_min.f = 0.f;
        }
        break;
        case ct_lfophaseshuffle:
        {
            val_default.f = 0.f;
        }
        break;
        case ct_pbdepth:
        {
            val_min.i = 0;
            val_max.i = 24;
            val_default.i = 2;
        }
        break;
        default:
            break;
        }
    }
    else
    {
        switch (ctrltype)
        {
        case ct_percent_with_extend_to_bipolar:
            val_default.f = 0.5f;
            break;
        case ct_pitch_extendable_very_low_minval:
        {
            val_min.f = -117.3763; // 0.5 Hz
            val_max.f = 70;        // 25087.71 Hz
            val_default.f = 3;     // 523.25 Hz

            displayType = ATwoToTheBx;
            snprintf(displayInfo.unit, DISPLAYINFO_TXT_SIZE, "Hz");
            displayInfo.a = 440.0;
            displayInfo.b = 1.0f / 12.0f;
            displayInfo.decimals = 2;
            displayInfo.modulationCap = 880.f * powf(2.0, (val_max.f) / 12.0f);
            displayInfo.supportsNoteName = true;
            displayInfo.customFeatures = ParamDisplayFeatures::kAllowsModulationsInNotesAndCents;
        }
        break;
        case ct_freq_audible_fm3_extendable:
        {
            val_min.f = -117.3763; // 0.5 Hz
        }
        break;
        case ct_freq_reson_band1:
        case ct_freq_reson_band2:
        case ct_freq_reson_band3:
        {
            val_min.f = -34.4936f; // 60 Hz
            val_max.f = 49.09578;  // 7500 Hz
        }
        break;
        case ct_dly_fb_clippingmodes:
        case ct_lfoamplitude:
        {
            val_min.f = -1.f;
        }
        break;
        case ct_lfophaseshuffle:
        {
            val_default.f = 0.5f;
        }
        break;
        case ct_pbdepth:
        {
            val_min.i = 0;
            val_max.i = 2400;
            val_default.i = 200;
        }
        break;
        default:
            break;
        }
    }
}

float Parameter::get_extended(float f) const
{
    if (!extend_range)
    {
        return f;
    }

    switch (ctrltype)
    {
    case ct_freq_fm2_offset:
    case ct_freq_shift:
        return 100.f * f;
    case ct_pitch_semi7bp:
    case ct_pitch_semi7bp_absolutable:
        return 12.f * f;
    case ct_decibel_extendable:
    case ct_bonsai_bass_boost:
        return 3.f * f;
    case ct_detuning:
        return 6.f * f;
    case ct_decibel_narrow_extendable:
        return 5.f * f;
    case ct_decibel_narrow_short_extendable:
        return 2.f * f;
    case ct_oscspread:
    case ct_oscspread_bipolar:
        return 12.f * f;
    case ct_osc_feedback:
        return 8.f * f - 4.f * f;
    case ct_filter_feedback:
    case ct_osc_feedback_negative:
        return 4.f * f;
    case ct_lfophaseshuffle:
    {
        return (2.f * f) - 1.f;
    }
    case ct_fmratio:
    {
        if (f > 16)
        {
            return ((f - 16) * 31.f / 16.f + 1);
        }
        else
        {
            return -((16 - f) * 31.f / 16.f + 1);
        }
    }
    break;
    case ct_dly_fb_clippingmodes:
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    {
        return 2 * f - 1;
    }
    break;
    default:
    {
        return f;
    }
    }
}

std::string Parameter::tempoSyncNotationValue(float f) const
{
    float a, b = modff(f, &a);

    if (b >= 0)
    {
        b -= 1.0;
        a += 1.0;
    }

    float d, q;
    std::string nn, t;
    char tmp[1024];

    if (f >= 1)
    {
        q = pow(2.0, f - 1);
        nn = "whole";
        if (q >= 3)
        {
            if (abs(q - floor(q + 0.01)) < 0.01)
            {
                snprintf(tmp, 1024, "%d whole notes", (int)floor(q + 0.01));
            }
            else
            {
                // this is the triplet case
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
            }
            std::string res = tmp;
            return res;
        }
        else if (q >= 2)
        {
            nn = "double whole";
            q /= 2;
        }

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            if (nn == "whole")
            {
                nn = "double whole";
            }
            else
            {
                q = pow(2.0, f - 1);
                snprintf(tmp, 1024, "%d whole triplets", (int)floor(q * 3.0 / 2.0 + 0.02));
                std::string res = tmp;
                return res;
            }
        }
        else
        {
            t = "dotted";
        }
    }
    else
    {
        d = pow(2.0, -(a - 2));
        q = pow(2.0, (b + 1));

        if (q < 1.3)
        {
            t = "note";
        }
        else if (q < 1.4)
        {
            t = "triplet";
            d = d / 2;
        }
        else
        {
            t = "dotted";
        }
        if (d == 1)
        {
            nn = "whole";
        }
        else
        {
            char tmp[1024];
            snprintf(tmp, 1024, "1/%d", (int)d);
            nn = tmp;
        }
    }
    std::string res = nn + " " + t;

    return res;
}

void Parameter::get_display_of_modulation_depth(char *txt, float modulationDepth, bool isBipolar,
                                                ModulationDisplayMode displaymode,
                                                ModulationDisplayInfoWindowStrings *iw) const
{
#define ITXT_SIZE 1024

    const auto isHighPrecision = Surge::Storage::getValueDisplayIsHighPrecision(storage);
    const auto displayPrecision = Surge::Storage::getValueDisplayPrecision(storage);

    if (basicBlocksParamMetaData.has_value() && basicBlocksParamMetaData->supportsStringConversion)
    {
        auto fs = sst::basic_blocks::params::ParamMetaData::FeatureState()
                      .withHighPrecision(displayPrecision)
                      .withTemposync(can_temposync() && temposync)
                      .withAbsolute(can_be_absolute() && absolute)
                      .withExtended(can_extend_range() && extend_range);

        auto res = basicBlocksParamMetaData->modulationNaturalToString(val.f, modulationDepth,
                                                                       isBipolar, fs);
        if (res.has_value())
        {
#if DEBUG_MOD_STRINGS
            std::cout << "  value   : " << res->value << std::endl;
            std::cout << "  summary : " << res->summary << std::endl;
            std::cout << "  change  : " << res->changeUp << " | " << res->changeDown << std::endl;
            std::cout << "  valUpDn : " << res->valUp << " | " << res->valDown << std::endl;
#endif
            switch (displaymode)
            {
            case TypeIn:
                strncpy(txt, res->value.c_str(), TXT_SIZE - 1);
                return;
            case Menu:
                strncpy(txt, res->summary.c_str(), TXT_SIZE - 1);
                return;
            case InfoWindow:
                iw->val = res->baseValue;
                iw->valplus = res->valUp;
                iw->valminus = res->valDown;
                iw->dvalplus = res->changeUp;
                iw->dvalminus = res->changeDown;

                strncpy(txt, res->singleLineModulationSummary.c_str(), TXT_SIZE - 1);
                return;
            }
        }
        else
        {
            std::cout << "Modulation formatting failed for [" << basicBlocksParamMetaData->name
                      << "]" << std::endl;
        }
    }

    int dp = (isHighPrecision ? 6 : displayInfo.decimals);

    const char *lowersep = "<", *uppersep = ">";

    float mf = modulationDepth;
    float f = val.f;

    switch (displayType)
    {
    case Custom:
        // handled below
        break;
    case DelegatedToFormatter:
        // For now do LinearWithScale
    case LinearWithScale:
    {
        std::string u = displayInfo.unit;

        getSemitonesOrKeys(u);

        if (displayInfo.customFeatures & ParamDisplayFeatures::kScaleBasedOnIsBiPolar)
        {
            if (!is_bipolar())
            {
                f = (f + 1) * 0.5;
                mf = mf * 0.5;
            }
        }

        if (can_extend_range())
        {
            // mf is handed to me extended already
            f = get_extended(f);
        }

        if (can_be_absolute() && absolute)
        {
            f = displayInfo.absoluteFactor * f;
            mf = displayInfo.absoluteFactor * mf;
            u = displayInfo.absoluteUnit;
        }

        f *= displayInfo.scale;
        mf *= displayInfo.scale;

        switch (displaymode)
        {
        case TypeIn:
            snprintf(txt, TXT_SIZE, "%.*f %s", dp, mf, u.c_str());
            return;
        case Menu:
            if (isBipolar)
                snprintf(txt, TXT_SIZE, "%s %.*f %s", (mf >= 0 ? "+/-" : "-/+"), dp, fabs(mf),
                         u.c_str());
            else
                snprintf(txt, TXT_SIZE, "%.*f %s", dp, mf, u.c_str());
            return;
        case InfoWindow:
        {
            if (isBipolar)
            {
                if (iw)
                {
                    char itxt[ITXT_SIZE];

                    snprintf(itxt, ITXT_SIZE, "%.*f %s", dp, f, u.c_str());
                    iw->val = itxt;
                    snprintf(itxt, ITXT_SIZE, "%.*f", dp, f + mf);
                    iw->valplus = itxt;
                    snprintf(itxt, ITXT_SIZE, "%.*f", dp, f - mf);
                    iw->valminus = itxt;
                    snprintf(itxt, ITXT_SIZE, "%s%.*f", (mf > 0 ? "+" : ""), dp, +mf);
                    iw->dvalplus = itxt;
                    snprintf(itxt, ITXT_SIZE, "%s%.*f", (mf < 0 ? "+" : ""), dp, -mf);
                    iw->dvalminus = itxt;
                }

                snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s %.*f %s", dp, f - mf, lowersep, dp, f,
                         uppersep, dp, f + mf, u.c_str());
            }
            else
            {
                if (iw)
                {
                    char itxt[ITXT_SIZE];
                    snprintf(itxt, ITXT_SIZE, "%.*f %s", dp, f, u.c_str());
                    iw->val = itxt;
                    snprintf(itxt, ITXT_SIZE, "%.*f", dp, f + mf);
                    iw->valplus = itxt;
                    iw->valminus = "";
                    snprintf(itxt, ITXT_SIZE, "%s%.*f", (mf > 0 ? "+" : ""), dp, mf);
                    iw->dvalplus = itxt;
                    iw->dvalminus = "";
                }

                snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s", dp, f, uppersep, dp, f + mf, u.c_str());
            }
            return;
        }
        }

        break;
    }
    case ATwoToTheBx:
    {
        if (temposync)
        {
            dp = (isHighPrecision ? 6 : 2);

            switch (displaymode)
            {
            case TypeIn:
                snprintf(txt, TXT_SIZE, "%.*f %c", dp, 100.f * modulationDepth, '%');
                break;
            case Menu:
                snprintf(txt, TXT_SIZE, "%s%.*f %s", (modulationDepth > 0) ? "+" : "", dp,
                         modulationDepth * 100, "%");
                break;
            case InfoWindow:
                if (iw)
                {
                    iw->val = tempoSyncNotationValue(val.f);

                    char ltxt[TXT_SIZE];
                    snprintf(ltxt, TXT_SIZE, "%.*f %c", dp, 100.f * modulationDepth, '%');
                    iw->dvalplus = ltxt;
                    snprintf(ltxt, TXT_SIZE, "%.*f %c", dp, -100.f * modulationDepth, '%');
                    iw->dvalminus = ltxt;
                    iw->valplus = iw->dvalplus;
                    iw->valminus = iw->dvalminus;
                }
                break;
            }
        }
        else
        {
            float v = displayInfo.a * powf(2.0f, displayInfo.b * val.f);
            float mp = displayInfo.a * powf(2.0f, (val.f + modulationDepth) * displayInfo.b);
            float mn = displayInfo.a * powf(2.0f, (val.f - modulationDepth) * displayInfo.b);

            std::string u = displayInfo.unit;

            getSemitonesOrKeys(u);

            if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMinValue)
            {
                if (val.f <= val_min.f)
                    v = displayInfo.minLabelValue;
                if (val.f - modulationDepth <= val_min.f)
                    mn = displayInfo.minLabelValue;
                if (val.f + modulationDepth <= val_min.f)
                    mp = displayInfo.minLabelValue;
            }

            if (displayInfo.modulationCap > 0)
            {
                mp = std::min(mp, displayInfo.modulationCap);
                mn = std::min(mn, displayInfo.modulationCap);
            }

            float dval = v;
            int vdp = dp;
            std::string vu = u;

            // Display modulation values
            float dmp = mp;
            float dmn = mn;
            float diffFac{1.f};

            if (displayInfo.customFeatures & ParamDisplayFeatures::kSwitchesFromSecToMillisec)
            {
                if (dval < 1.f)
                {
                    dval *= 1000.f;
                    vu = "ms";
                    vdp = isHighPrecision ? 2 : 1;
                    dmp *= 1000.f;
                    dmn *= 1000.f;
                    diffFac = 1000.f;
                }
            }

            switch (displaymode)
            {
            case TypeIn:
                snprintf(txt, TXT_SIZE, "%.*f %s", dp, mp - v, u.c_str());
                break;
            case Menu:
                snprintf(txt, TXT_SIZE, "%s%.*f %s", (mp - v > 0) ? "+" : "", dp, mp - v,
                         u.c_str());

                if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMaxString &&
                    mp > val_max.f)
                {
                    snprintf(txt, TXT_SIZE, "%s", displayInfo.maxLabel);
                }

                break;
            case InfoWindow:
            {
                if (isBipolar)
                {
                    if (iw)
                    {
                        char itxt[ITXT_SIZE];
                        snprintf(itxt, ITXT_SIZE, "%.*f %s", vdp, dval, vu.c_str());
                        iw->val = itxt;
                        snprintf(itxt, ITXT_SIZE, "%.*f", dp, dmp);
                        iw->valplus = itxt;
                        snprintf(itxt, ITXT_SIZE, "%.*f", dp, dmn);
                        iw->valminus = itxt;
                        snprintf(itxt, ITXT_SIZE, "%s%.*f", (mp - v > 0 ? "+" : ""), dp,
                                 diffFac * (mp - v));
                        iw->dvalplus = itxt;
                        snprintf(itxt, ITXT_SIZE, "%s%.*f", (mn - v > 0 ? "+" : ""), dp,
                                 diffFac * (mn - v));
                        iw->dvalminus = itxt;

                        if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMaxString)
                        {
                            if (v >= val_max.f)
                                iw->val = displayInfo.maxLabel;
                            if (val.f + modulationDepth >= val_max.f)
                                iw->valplus = displayInfo.maxLabel;
                            if (val.f - modulationDepth >= val_max.f)
                                iw->valminus = displayInfo.maxLabel;
                            if (val.f + modulationDepth >= val_max.f)
                                iw->dvalplus = displayInfo.maxLabel;
                            if (val.f - modulationDepth >= val_max.f)
                                iw->dvalminus = displayInfo.maxLabel;
                        }
                    }

                    snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s %.*f %s", dp, dmn, lowersep, vdp, dval,
                             uppersep, dp, dmp, vu.c_str());
                }
                else
                {
                    if (iw)
                    {
                        char itxt[ITXT_SIZE];
                        snprintf(itxt, ITXT_SIZE, "%.*f %s", vdp, dval, vu.c_str());
                        iw->val = itxt;
                        snprintf(itxt, ITXT_SIZE, "%.*f", dp, dmp);
                        iw->valplus = itxt;
                        iw->valminus = "";
                        snprintf(itxt, ITXT_SIZE, "%s%.*f", (mp - v > 0 ? "+" : ""), dp,
                                 diffFac * (mp - v));
                        iw->dvalplus = itxt;
                        iw->dvalminus = "";
                    }

                    snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s", vdp, dval, uppersep, dp, dmp,
                             vu.c_str());
                }
                break;
            }
            }
            return;
        }
        break;
    }
    case Decibel:
    {
        float v = amp_to_db(val.f);
        float mp = amp_to_db(std::max(val.f + modulationDepth, 1.e-7f));
        float mn = amp_to_db(std::max(val.f - modulationDepth, 1.e-7f));

        char posval[TXT_SIZE];
        char negval[TXT_SIZE];
        char val[TXT_SIZE];

        if (mn <= -192.f)
            snprintf(negval, TXT_SIZE, "-inf %s", displayInfo.unit);
        else
            snprintf(negval, TXT_SIZE, "%.*f %s", dp, mn, displayInfo.unit);

        if (mp <= -192.f)
            snprintf(posval, TXT_SIZE, "-inf %s", displayInfo.unit);
        else
            snprintf(posval, TXT_SIZE, "%.*f %s", dp, mp, displayInfo.unit);

        if (v <= -192.f)
            snprintf(val, TXT_SIZE, "-inf %s", displayInfo.unit);
        else
            snprintf(val, TXT_SIZE, "%.*f %s", dp, v, displayInfo.unit);

        switch (displaymode)
        {
        case TypeIn:
        case Menu:
            snprintf(txt, TXT_SIZE, "%.*f %s", dp,
                     limit_range(mp, -192.f, 500.f) - limit_range(v, -192.f, 500.f),
                     displayInfo.unit);
            break;
        case InfoWindow:
            if (iw)
            {
                iw->val = val;
                iw->valplus = posval;
                iw->valminus = isBipolar ? negval : "";

                char dtxt[TXT_SIZE];

                snprintf(dtxt, TXT_SIZE, "%s%.*f %s", (mp - v > 0 ? "+" : ""), dp,
                         limit_range(mp, -192.f, 500.f) - limit_range(v, -192.f, 500.f),
                         displayInfo.unit);
                iw->dvalplus = dtxt;

                snprintf(dtxt, TXT_SIZE, "%s%.*f %s", (mn - v > 0 ? "+" : ""), dp,
                         limit_range(mn, -192.f, 500.f) - limit_range(v, -192.f, 500.f),
                         displayInfo.unit);
                iw->dvalminus = isBipolar ? dtxt : "";
            }

            if (isBipolar)
            {
                snprintf(txt, TXT_SIZE, "%s %s %s %s %s dB", negval, lowersep, val, uppersep,
                         posval);
            }
            else
            {
                snprintf(txt, TXT_SIZE, "%s %s %s dB", val, uppersep, posval);
            }

            break;
        }
        return;
    }
    }

    switch (ctrltype)
    {
    case ct_float_toggle:
    {
        f *= displayInfo.scale;
        mf *= displayInfo.scale;

        switch (displaymode)
        {
        case TypeIn:
            snprintf(txt, TXT_SIZE, "%.*f %s", dp, mf, displayInfo.unit);
            return;
        case Menu:
            snprintf(txt, TXT_SIZE, "%.*f %s", dp, mf, displayInfo.unit);
            return;
        case InfoWindow:
        {
            if (iw)
            {
                char itxt[ITXT_SIZE];

                snprintf(itxt, ITXT_SIZE, "%.*f %s", dp, f, displayInfo.unit);
                iw->val = itxt;
                snprintf(itxt, ITXT_SIZE, "%.*f", dp, f + mf);
                iw->valplus = itxt;
                iw->valminus = "";
                snprintf(itxt, ITXT_SIZE, "%s%.*f", (mf > 0 ? "+" : ""), dp, mf);
                iw->dvalplus = itxt;
                iw->dvalminus = "";
            }

            snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s", dp, f, uppersep, dp, f + mf,
                     displayInfo.unit);
            return;
        }
        }

        break;
    }
    case ct_fmratio:
    {
        if (absolute)
        {
            float bpv = (f - 16.0) / 16.0;
            float bpu = (f + modulationDepth - 16.0) / 16.0;
            float bpd = (f - modulationDepth - 16.0) / 16.0;
            float mul = 69;
            float note = 69 + mul * bpv;
            float noteup = 69 + mul * bpu;
            float notedn = 60 + mul * bpd;

            auto freq = 440.0 * pow(2.0, (note - 69.0) / 12);
            auto frequp = 440.0 * pow(2.0, (noteup - 69.0) / 12);
            auto freqdn = 440.0 * pow(2.0, (notedn - 69.0) / 12);
            int dp = (isHighPrecision ? 6 : 2);

            switch (displaymode)
            {
            case TypeIn:
            case Menu:
                snprintf(txt, TXT_SIZE, "%.*f Hz", dp, frequp - freq);
                break;
            case InfoWindow:
                if (iw)
                {
                    auto put = [dp](std::string &tg, float val) {
                        char txt[TXT_SIZE];
                        snprintf(txt, TXT_SIZE, "%.*f Hz", dp, val);
                        tg = txt;
                    };

                    put(iw->val, freq);
                    put(iw->valplus, frequp);
                    put(iw->valminus, freqdn);
                    put(iw->dvalplus, frequp - freq);
                    put(iw->dvalminus, freq - freqdn);

                    snprintf(txt, TXT_SIZE, "%.*f Hz %.*f Hz %.*f Hz", dp, freqdn, dp, freq, dp,
                             frequp);
                    break;
                }
            }

            return;
        }

        // OK so this is already handed to us extended and this one is weird so
        float mf = modulationDepth;
        auto qq = mf;

        if (extend_range)
        {
            if (mf < 0)
            {
                qq = mf + 1;
            }
            else
            {
                qq = mf - 1;
            }

            qq = (qq + 31) / 64;
        }

        float exmf = qq;
        int dp = (isHighPrecision ? 6 : 2);

        switch (displaymode)
        {
        case TypeIn:
            if (extend_range)
            {
                snprintf(txt, TXT_SIZE, "C : %.*f", dp, qq * 31 * 2);
            }
            else
            {
                snprintf(txt, TXT_SIZE, "C : %.*f", dp, mf);
            }

            return;
        case Menu:
        {
            if (extend_range)
            {
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "C : %s %.*f", (mf >= 0 ? "+/-" : "-/+"), dp,
                             fabs(qq * 31 * 2));
                }
                else
                {
                    snprintf(txt, TXT_SIZE, "C : %.*f", dp, qq * 31 * 2);
                }
            }
            else
            {
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "C : %s %.*f", (mf >= 0 ? "+/-" : "-/+"), dp, fabs(mf));
                }
                else
                {
                    snprintf(txt, TXT_SIZE, "C : %.*f", dp, mf);
                }
            }

            return;
        }
        case InfoWindow:
            if (iw)
            {
                if (extend_range)
                {
                    char dtxt[TXT_SIZE];
                    float ev = get_extended(val.f);

                    if (ev < 0)
                    {
                        snprintf(dtxt, TXT_SIZE, "C : 1 / %.*f", dp, -ev);
                    }
                    else
                    {
                        snprintf(dtxt, TXT_SIZE, "C : %.*f", dp, ev);
                    }

                    iw->val = dtxt;

                    auto upval = get_extended(val.f + (qq * 32));
                    auto dnval = get_extended(val.f - (qq * 32));

                    if (upval < 0)
                        snprintf(dtxt, TXT_SIZE, "C : 1 / %.*f", dp, -upval);
                    else
                        snprintf(dtxt, TXT_SIZE, "C : %.*f", dp, upval);

                    iw->valplus = dtxt;
                    snprintf(dtxt, TXT_SIZE, "%.*f", dp, qq * 31 * 2);
                    iw->dvalplus = dtxt;

                    if (isBipolar)
                    {
                        if (dnval < 0)
                            snprintf(dtxt, TXT_SIZE, "C : 1/%.*f", dp, -dnval);
                        else
                            snprintf(dtxt, TXT_SIZE, "C : %.*f", dp, dnval);

                        iw->valminus = dtxt;

                        snprintf(dtxt, TXT_SIZE, "%.*f", dp, -(qq * 31 * 2));
                        iw->dvalminus = dtxt;
                    }
                }
                else
                {
                    char dtxt[TXT_SIZE];

                    snprintf(dtxt, TXT_SIZE, "C : %.*f", dp, val.f);
                    iw->val = dtxt;
                    snprintf(dtxt, TXT_SIZE, "%.*f", dp, val.f + mf);
                    iw->valplus = dtxt;
                    snprintf(dtxt, TXT_SIZE, "%.*f", dp, mf);
                    iw->dvalplus = dtxt;

                    if (isBipolar)
                    {
                        snprintf(dtxt, TXT_SIZE, "%.*f", dp, val.f - mf);
                        iw->valminus = dtxt;
                        snprintf(dtxt, TXT_SIZE, "%.*f", dp, -mf);
                        iw->dvalminus = dtxt;
                    }
                }
            }

            // not really used any more bot don't leave it uninitialized
            snprintf(txt, TXT_SIZE, "C: %.*f %s %.*f", 2, val.f, (mf >= 0 ? "+/-" : "-/+"), 2, mf);
            return;
        }
    }
    default:
    {
        if (temposync)
        {
            dp = (isHighPrecision ? 6 : 2);

            auto mp = modulationDepth;
            auto mn = -modulationDepth;

            switch (displaymode)
            {
            case TypeIn:
            {
                snprintf(txt, TXT_SIZE, "%.*f %c", dp, 100.f * modulationDepth, '%');
                break;
            }
            case Menu:
            {
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "+/- %.*f %c", dp, 100.f * mp, '%');
                }
                else
                {
                    snprintf(txt, TXT_SIZE, "%.*f %c", dp, 100.f * mp, '%');
                }
                break;
            }
            case InfoWindow:
            {
                std::string vs = tempoSyncNotationValue(val.f);
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "%.*f %s %s %s %.*f %c", dp, mn, lowersep, vs.c_str(),
                             uppersep, dp, mp, '%');
                }
                else
                {
                    snprintf(txt, TXT_SIZE, "%s %s %.*f %c", vs.c_str(), uppersep, dp, mp, '%');
                }
                break;
            }
            }
        }
        else
        {
            float v = val.f * 100.f;
            float mp = (val.f + modulationDepth) * 100.f;
            float mn = (val.f - modulationDepth) * 100.f;

            switch (displaymode)
            {
            case TypeIn:
                snprintf(txt, TXT_SIZE, "%.*f", dp, mp - v);
                break;
            case Menu:
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "%.*f ... %.*f %c", dp, mn, dp, mp, '%');
                }
                else
                    snprintf(txt, TXT_SIZE, "%.*f ... %.*f %c", dp, v, dp, mp, '%');
                break;
            case InfoWindow:
            {
                if (isBipolar)
                {
                    snprintf(txt, TXT_SIZE, "%.*f %s %.*f %s %.*f %c", dp, mn, lowersep, dp, v,
                             uppersep, dp, mp, '%');
                }
                else
                    snprintf(txt, TXT_SIZE, "%.*f %s %.*f %c", dp, v, uppersep, dp, mp, '%');
                break;
            }
            }
        }
        break;
    }
    }

#undef ITXT_SIZE
}

float Parameter::quantize_modulation(float inputval) const
{
    float res;

    if (temposync)
    {
        auto sv = inputval * (val_max.f - val_min.f); // this is now a 0 -> 1 for 0 -> 100%
        res = (float)((int)(sv * 10.0) / 10.f);

        return res / (val_max.f - val_min.f);
    }

    switch (displayType)
    {
    case Custom:
        // handled below but let's set up a good default
        res = (float)(round(inputval * 100) / 100);
        break;
    case DelegatedToFormatter:
        // fall back
    case LinearWithScale:
    {
        float ext_mul = (can_extend_range() && extend_range) ? displayInfo.extendFactor : 1.f;
        float abs_mul = (can_be_absolute() && absolute) ? displayInfo.absoluteFactor : 1.f;
        float factor = ext_mul * abs_mul;
        float tempval = (val_max.f - val_min.f) * displayInfo.scale * factor;

        res = (float)((int)(inputval * tempval) / tempval);

        break;
    }
    case Decibel:
    {
        float scaledval = val.f * (1.f / val_max.f);
        float v = amp_to_db(scaledval);
        float vmod = amp_to_db(scaledval + inputval);
        float floorvmod = floor(vmod - v) + v;

        // so we want to find a new integer value which satisfies:
        // 18 * log2(oldval + newval) = floorvmod, or
        // 2^(floorvmod / 18) - oldval = newval

        res = powf(2.f, floorvmod / 18.f) - scaledval;

        break;
    }
    case ATwoToTheBx:
    {
        // for these control types only snap to semitones
        switch (ctrltype)
        {
        case ct_freq_hpf:
        case ct_freq_audible:
        case ct_freq_audible_deactivatable:
        case ct_freq_audible_deactivatable_hp:
        case ct_freq_audible_deactivatable_lp:
        case ct_freq_audible_fm3_extendable:
        case ct_freq_audible_with_tunability:
        case ct_freq_audible_very_low_minval:
        case ct_freq_reson_band1:
        case ct_freq_reson_band2:
        case ct_freq_reson_band3:
        case ct_freq_vocoder_low:
        case ct_freq_vocoder_high:
        case ct_freq_ringmod:
        {
            auto range = val_max.f - val_min.f;
            return floor(inputval * range) / range;
            break;
        }
        }

        /*
         * OK so the display value is A e^val and we want to quantize in that space and then
         * find the res. So first of all let's find the endpoint. Remember this calculation
         * is basically a 2^bx
         */
        auto mdepth = inputval * (val_max.f - val_min.f);
        auto center = displayInfo.a * pow(2.0, displayInfo.b * val.f);
        auto modpoint = displayInfo.a * pow(2.0, displayInfo.b * (val.f + mdepth));
        auto moddist = modpoint - center;

        /*
         * OK so now we want the mod distance to be quantized but quantized in units of
         * what? Well let's use a simple heuristic that we are roughly 5% of our center
         * as a tick but, you know, nice and integral. This method has the problem that
         * the tick size gets 'smaller' as you move up. You also need to scale the integrality
         * depending on starting point.
         */
        float scaleFactor = 1;

        if (center > 100)
        {
            scaleFactor = 0.1;
        }
        if (center < 10)
        {
            scaleFactor = 10;
        }
        if (center < 1)
        {
            scaleFactor = 100;
        }

        auto stepsize = abs(ceil(0.05 * center * scaleFactor) / scaleFactor);
        moddist = round(moddist / stepsize) * stepsize;
        auto modresult = center + moddist;

        if (modresult <= 0)
        {
            res = -1.f;
        }
        else
        {
            auto modresult_exponent = log2(modresult / displayInfo.a) / displayInfo.b; // = val + d
            res = limit_range((float)(modresult_exponent - val.f) / (val_max.f - val_min.f), -1.f,
                              1.f);
        }

        break;
    }
    default:
    {
        float tempval = (val_max.f - val_min.f) * displayInfo.scale;

        res = (float)((int)(inputval * tempval) / tempval);

        break;
    }
    }

    return res;
}

void Parameter::getSemitonesOrKeys(std::string &str) const
{
    if (displayInfo.customFeatures & ParamDisplayFeatures::kUnitsAreSemitonesOrKeys && !absolute)
    {
        str = "semitones";

        if (storage && !storage->isStandardTuning &&
            storage->tuningApplicationMode == SurgeStorage::RETUNE_ALL)
        {
            str = "keys";
        }
    }
}

void Parameter::get_display_alt(char *txt, bool external, float ef) const
{
    if (basicBlocksParamMetaData.has_value() && basicBlocksParamMetaData->supportsStringConversion)
    {
        auto bbf = val.f;
        if (external)
            bbf = basicBlocksParamMetaData->normalized01ToNatural(ef);
        auto tryFormat = basicBlocksParamMetaData->valueToAlternateString(bbf);
        if (tryFormat.has_value())
        {
            strncpy(txt, tryFormat->c_str(), TXT_SIZE - 1);
            return;
        }
    }

    const auto isHighPrecision = Surge::Storage::getValueDisplayIsHighPrecision(storage);
    const auto displayPrecision = Surge::Storage::getValueDisplayPrecision(storage);

    txt[0] = 0;
    switch (ctrltype)
    {
    case ct_lforate:
    case ct_lforate_deactivatable:
    {
        float f = val.f;
        float fInv = 0.f;
        int dec = isHighPrecision ? 6 : displayInfo.decimals;
        std::string u = "s";

        fInv = 1.f / (displayInfo.a * powf(2.0f, f * displayInfo.b));

        if (temposync)
        {
            fInv *= storage->temposyncratio_inv;
        }

        if (fInv < 1.f)
        {
            fInv *= 1000.f;
            u = "ms";
        }

        snprintf(txt, TXT_SIZE, "%.*f %s", dec, fInv, u.c_str());

        break;
    }
    case ct_pitch_extendable_very_low_minval:
    case ct_freq_hpf:
    case ct_freq_audible:
    case ct_freq_audible_deactivatable:
    case ct_freq_audible_deactivatable_hp:
    case ct_freq_audible_deactivatable_lp:
    case ct_freq_audible_fm3_extendable:
    case ct_freq_audible_with_tunability:
    case ct_freq_audible_very_low_minval:
    case ct_freq_reson_band1:
    case ct_freq_reson_band2:
    case ct_freq_reson_band3:
    case ct_freq_vocoder_low:
    case ct_freq_vocoder_high:
    case ct_freq_ringmod:
    case ct_fmratio:
    {
        float f = val.f;

        if (ctrltype == ct_pitch_extendable_very_low_minval && !extend_range)
        {
            break;
        }

        if (ctrltype == ct_fmratio && absolute)
        {
            f = 69 * ((f - 16.0) / 16.0);
        }

        int i_value = round(f);
        if (ctrltype == ct_freq_ringmod)
        {
            i_value += 0;
        }
        else if (ctrltype == ct_freq_audible_fm3_extendable)
        {
            i_value += 60;
        }
        else
        {
            i_value += 69;
        }

        int oct_offset = 1;

        if (storage)
        {
            oct_offset = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        }

        snprintf(txt, TXT_SIZE, "~%s", get_notename(i_value, oct_offset).c_str());

        if (ctrltype == ct_fmratio && !absolute)
        {
            snprintf(txt, TXT_SIZE, " ");
        }

        break;
    }
    case ct_flangerpitch:
    {
        float f = val.f;
        int i_value = (int)(f);
        int oct_offset = 1;

        if (storage)
        {
            oct_offset = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        }

        snprintf(txt, TXT_SIZE, "~%s", get_notename(i_value, oct_offset).c_str());

        break;
    }
    case ct_countedset_percent:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
        if (user_data != nullptr)
        {
            // We check when set so the reinterpret cast is safe and fast
            float f = val.f;
            CountedSetUserData *cs = reinterpret_cast<CountedSetUserData *>(user_data);
            auto count = cs->getCountedSetSize();
            auto tl = (count - extend_range) * f;
            if (extend_range)
            {
                auto tlDisplay = std::clamp(tl + 1, 0.f, (1.f) * (count - extend_range) + 1);
                snprintf(txt, TXT_SIZE, "%.2f / %d", tlDisplay, (count - extend_range) + 1);
            }
            else
            {
                auto tlDisplay =
                    (int)std::floor(std::clamp(tl + 1, 0.f, (1.f) * (count - extend_range)));
                snprintf(txt, TXT_SIZE, "%d / %d", tlDisplay, (count - extend_range));
            }
        }

        break;
    case ct_alias_mask:
    {
        int bits = 8;
        int mask = 1 << bits;
        std::string bin;

        while (bits--)
        {
            mask >>= 1;
            bin += ((int)(val.f * 255) & mask) ? "1" : "0";
        }

        snprintf(txt, TXT_SIZE, "%s", bin.c_str());
        break;
    }
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    {
        auto ef = dynamic_cast<ParameterExternalFormatter *>(user_data);
        if (ef)
        {
            float f;
            char tmptxt[TXT_SIZE];
            if (ef->formatAltValue(this, val.f, tmptxt, TXT_SIZE))
            {
                strncpy(txt, tmptxt, TXT_SIZE);
                break;
            }
        }
    }
    }
}

void Parameter::get_display(char *txt, bool external, float ef) const
{
    auto str = get_display(external, ef);

    strncpy(txt, str.c_str(), TXT_SIZE - 1);
}

std::string Parameter::get_display(bool external, float ef) const
{
    std::string txt{""};

    if (ctrltype == ct_none)
    {
        return "-";
    }

    int i;
    float f;
    bool b;

    const auto isHighPrecision = Surge::Storage::getValueDisplayIsHighPrecision(storage);
    const auto displayPrecision = Surge::Storage::getValueDisplayPrecision(storage);

    if (basicBlocksParamMetaData.has_value() && basicBlocksParamMetaData->supportsStringConversion)
    {
        auto bbf = val.f;
        if (valtype == vt_int)
            bbf = (float)val.i;
        if (valtype == vt_bool)
            bbf = val.b ? 1.f : 0.f;
        if (external)
            bbf = basicBlocksParamMetaData->normalized01ToNatural(ef);

        auto fs = sst::basic_blocks::params::ParamMetaData::FeatureState()
                      .withHighPrecision(displayPrecision)
                      .withTemposync(can_temposync() && temposync)
                      .withAbsolute(can_be_absolute() && absolute)
                      .withExtended(can_extend_range() && extend_range);

        auto tryFormat = basicBlocksParamMetaData->valueToString(bbf, fs);
        if (tryFormat.has_value())
            return *tryFormat;
    }

    switch (valtype)
    {
    case vt_float:
        if (external)
        {
            f = ef * (val_max.f - val_min.f) + val_min.f;
        }
        else
        {
            f = val.f;
        }

        switch (displayType)
        {
        case Custom:
            // Custom cases are handled below
            break;
        case DelegatedToFormatter:
        {
            auto ef = dynamic_cast<ParameterExternalFormatter *>(user_data);
            char str[TXT_SIZE];

            if (ef)
            {
                // parameter called 'len' not 'size', be on the safe side here, do - 1.
                // It used to say just '64' anyway.
                if (ef->formatValue(this, f, str, TXT_SIZE - 1))
                {
                    return str;
                }
            }
            // We do not break on purpose here. DelegatedToFormatter falls back to Linear with Scale
        }
        case LinearWithScale:
        {
            std::string u = displayInfo.unit;

            getSemitonesOrKeys(u);

            if (displayInfo.customFeatures & ParamDisplayFeatures::kScaleBasedOnIsBiPolar)
            {
                if (!is_bipolar())
                {
                    f = (f + 1) * 0.5;
                }
            }

            if (can_extend_range())
            {
                f = get_extended(f);
            }

            if (can_be_absolute() && absolute)
            {
                f = displayInfo.absoluteFactor * f;
                u = displayInfo.absoluteUnit;
            }

            txt = fmt::format("{:.{}f} {:s}", displayInfo.scale * f,
                              (isHighPrecision ? 6 : displayInfo.decimals), u);

            if (f >= val_max.f &&
                (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMaxString))
            {
                txt = displayInfo.maxLabel;
            }

            if (f <= val_min.f &&
                (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMinString))
            {
                txt = displayInfo.minLabel;
            }

            if (f == val_default.f &&
                (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomDefaultString))
            {
                txt = displayInfo.defLabel;
            }

            return txt;
        }
        case ATwoToTheBx:
        {
            if (can_temposync() && temposync)
            {
                txt = tempoSyncNotationValue(displayInfo.tempoSyncNotationMultiplier * f);

                return txt;
            }

            if (can_extend_range() && extend_range)
            {
                f = get_extended(f);
            }

            std::string u = displayInfo.unit;

            getSemitonesOrKeys(u);

            float dval = displayInfo.a * powf(2.0f, f * displayInfo.b);
            int dec = isHighPrecision ? 6 : displayInfo.decimals;

            if (displayInfo.customFeatures & ParamDisplayFeatures::kSwitchesFromSecToMillisec)
            {
                if (dval < 1.f)
                {
                    dval *= 1000.f;
                    u = "ms";
                    dec = isHighPrecision ? 3 : 1;
                }
            }

            if (f >= val_max.f)
            {
                if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMaxString)
                {
                    txt = displayInfo.maxLabel;
                    return txt;
                }

                if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMaxValue)
                {
                    dval = displayInfo.maxLabelValue;
                }
            }

            if (f <= val_min.f)
            {
                if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMinString)
                {
                    txt = displayInfo.minLabel;
                    return txt;
                }

                if (displayInfo.customFeatures & ParamDisplayFeatures::kHasCustomMinValue)
                {
                    dval = displayInfo.minLabelValue;
                }
            }

            txt = fmt::format("{:.{}f} {:s}", dval, dec, u);

            return txt;
        }
        case Decibel:
        {
            if (f == 0)
            {
                txt = "-inf dB";
            }
            else
            {
                txt = fmt::format("{:.{}f} dB", amp_to_db(f), (isHighPrecision ? 6 : 2));
            }

            return txt;
        }
        }

        switch (ctrltype)
        {
        case ct_fmratio:
        {
            if (absolute)
            {
                // OK so I am 0 to 32. So let's define a note
                float bpv = (f - 16.0) / 16.0;
                auto note = 69 + 69 * bpv;
                auto freq = 440.0 * pow(2.0, (note - 69.0) / 12);

                txt = fmt::format("{:.{}f} Hz", freq, (isHighPrecision ? 6 : 2));
            }
            else
            {
                auto q = get_extended(f);

                if (extend_range && q < 0)
                {
                    txt =
                        fmt::format("C : 1 / {:.{}f}", -get_extended(f), (isHighPrecision ? 6 : 2));
                }
                else
                {
                    txt = fmt::format("C : {:.{}f}", get_extended(f), (isHighPrecision ? 6 : 2));
                }
            }
            break;
        }
        case ct_chow_ratio:
        {
            txt = fmt::format("1 : {:.{}f}", f, (isHighPrecision ? 6 : 2));
            break;
        }
        case ct_float_toggle:
        {
            txt = f > 0.5 ? "On" : "Off";
            break;
        }
        default:
            txt = fmt::format("{:.{}f}", f, (isHighPrecision ? 6 : 2));
            break;
        }
        break;
    case vt_int:
    {
        if (external)
        {
            i = Parameter::intUnscaledFromFloat(ef, val_max.i, val_min.i);
        }
        else
        {
            i = val.i;
        }

        if (displayType == DelegatedToFormatter)
        {
            float fv = Parameter::intScaledToFloat(i, val_max.i, val_min.i);
            char vt[TXT_SIZE];
            auto ef = dynamic_cast<ParameterExternalFormatter *>(user_data);

            if (ef)
            {
                if (ef->formatValue(this, fv, vt, TXT_SIZE - 1))
                {
                    txt = vt;
                    return txt;
                }
            }
        }
        switch (ctrltype)
        {
        case ct_pbdepth:
        {
            std::string unit;

            getSemitonesOrKeys(unit);

            if (extend_range)
            {
                txt = fmt::format("{:0.2f} {:s}", i * 1.f / 100, unit.c_str());
            }
            else
            {
                txt = fmt::format("{:d} {:s}", i, unit.c_str());
            }
        }
        break;
        case ct_midikey_or_channel:
        {
            auto sm = storage->getPatch().scenemode.val.i;

            if (sm == sm_chsplit)
            {
                txt = fmt::format("Channel {:d}", (val.i / 8) + 1);
                break;
            }
        }
        case ct_midikey:
        {
            int oct_offset = 1;

            if (storage)
            {
                oct_offset =
                    Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
            }

            txt = get_notename(val.i, oct_offset);

            break;
        }
        case ct_osctype:
            txt = osc_type_names[limit_range(i, 0, (int)n_osc_types - 1)];
            break;
        case ct_wt2window:
            txt = window_names[limit_range(i, 0, 8)];
            break;
        case ct_osccount:
            txt = fmt::format("{:d} voice{:s}", i, (i > 1 ? "s" : ""));
            break;
        case ct_fxtype:
            txt = fx_type_shortnames[limit_range(i, 0, (int)n_fx_types - 1)];
            break;
        case ct_reverbshape:
            txt = fmt::format("Type {:d}", i + 1);
            break;
        case ct_fxbypass:
            txt = fxbypass_names[limit_range(i, 0, (int)n_fx_bypass - 1)];
            break;
        case ct_filtertype:
            txt = sst::filters::filter_type_names[limit_range(
                i, 0, (int)sst::filters::num_filter_types - 1)];
            break;
        case ct_filtersubtype:
        {
            auto &patch = storage->getPatch();

            for (int scene = 0; scene < n_scenes; ++scene)
                for (int unit = 0; unit < n_filterunits_per_scene; ++unit)
                    if (id == patch.scene[scene].filterunit[unit].subtype.id)
                    {
                        using sst::filters::FilterType;

                        int type = patch.scene[scene].filterunit[unit].type.val.i;
                        const auto fType = (FilterType)type;
                        if (i >= sst::filters::fut_subcount[type])
                        {
                            txt = "None";
                        }
                        else
                            switch (fType)
                            {
                            case FilterType::fut_lpmoog:
                            case FilterType::fut_diode:
                                txt = sst::filters::fut_ldr_subtypes[i];
                                break;
                            case FilterType::fut_notch12:
                            case FilterType::fut_notch24:
                            case FilterType::fut_apf:
                                txt = sst::filters::fut_notch_subtypes[i];
                                break;
                            case FilterType::fut_comb_pos:
                            case FilterType::fut_comb_neg:
                                txt = sst::filters::fut_comb_subtypes[i];
                                break;
                            case FilterType::fut_vintageladder:
                                txt = sst::filters::fut_vintageladder_subtypes[i];
                                break;
                            case FilterType::fut_obxd_2pole_lp:
                            case FilterType::fut_obxd_2pole_hp:
                            case FilterType::fut_obxd_2pole_n:
                            case FilterType::fut_obxd_2pole_bp:
                                txt = sst::filters::fut_obxd_2p_subtypes[i];
                                break;
                            case FilterType::fut_obxd_4pole:
                                txt = sst::filters::fut_obxd_4p_subtypes[i];
                                break;
                            case FilterType::fut_k35_lp:
                            case FilterType::fut_k35_hp:
                                txt = sst::filters::fut_k35_subtypes[i];
                                break;
                            case FilterType::fut_cutoffwarp_lp:
                            case FilterType::fut_cutoffwarp_hp:
                            case FilterType::fut_cutoffwarp_n:
                            case FilterType::fut_cutoffwarp_bp:
                            case FilterType::fut_cutoffwarp_ap:
                            case FilterType::fut_resonancewarp_lp:
                            case FilterType::fut_resonancewarp_hp:
                            case FilterType::fut_resonancewarp_n:
                            case FilterType::fut_resonancewarp_bp:
                            case FilterType::fut_resonancewarp_ap:
                                // "i & 3" selects the lower two bits that represent the stage count
                                // "(i >> 2) & 3" selects the next two bits that represent the
                                // saturator
                                txt =
                                    fmt::format("{:s} {:s}", sst::filters::fut_nlf_subtypes[i & 3],
                                                sst::filters::fut_nlf_saturators[(i >> 2) & 3]);
                                break;
                            // don't default any more so compiler catches new ones we add
                            case FilterType::fut_none:
                            case FilterType::fut_lp12:
                            case FilterType::fut_lp24:
                            case FilterType::fut_bp12:
                            case FilterType::fut_bp24:
                            case FilterType::fut_hp12:
                            case FilterType::fut_hp24:
                            case FilterType::fut_SNH:
                                txt = sst::filters::fut_def_subtypes[i];
                                break;
                            case FilterType::fut_tripole:
                                // "i & 3" selects the lower two bits that represent the filter mode
                                // "(i >> 2) & 3" selects the next two bits that represent the
                                // output stage
                                txt = fmt::format(
                                    "{:s}, {:s}", sst::filters::fut_tripole_subtypes[i & 3],
                                    sst::filters::fut_tripole_output_stage[(i >> 2) & 3]);
                                break;
                            case FilterType::num_filter_types:
                                txt = "ERROR";
                                break;
                            }
                    }
            break;
        }
        case ct_wstype:
            txt = sst::waveshapers::wst_names[limit_range(
                i, 0, (int)sst::waveshapers::WaveshaperType::n_ws_types - 1)];
            break;
        case ct_envmode:
            txt = em_names[limit_range(i, 0, (int)n_env_modes - 1)];
            break;
        case ct_fbconfig:
            txt = fbc_names[limit_range(i, 0, (int)n_filter_configs - 1)];
            break;
        case ct_fmconfig:
            txt = fmr_names[limit_range(i, 0, (int)n_fm_routings - 1)];
            break;
        case ct_lfotype:
            txt = lt_names[limit_range(i, 0, (int)n_lfo_types - 1)];
            break;
        case ct_scenemode:

            txt = Surge::GUI::toOSCase(scene_mode_names[limit_range(i, 0, (int)n_scene_modes - 1)]);
            break;
        case ct_polymode:
            txt = Surge::GUI::toOSCase(play_mode_names[limit_range(i, 0, (int)n_play_modes - 1)]);
            break;
        case ct_lfotrigmode:
            txt = lfo_trigger_mode_names[limit_range(i, 0, (int)n_lfo_trigger_modes - 1)];
            break;
        case ct_character:
            txt = character_names[limit_range(i, 0, (int)n_character_modes - 1)];
            break;
        case ct_fmratio_int:
            txt = fmt::format("C : {:d}", i);
            break;
        case ct_phaser_stages:
            if (i == 1)
            {
                txt = "Legacy (4 stages)";
            }
            else
            {
                txt = std::to_string(i);
            }
            break;

        case ct_envshape:
            switch (i)
            {
            case 0:
                txt = "Linear";
                break;
            case 1:
                txt = "Quadratic";
                break;
            case 2:
                txt = "Cubic";
                break;
            default:
                txt = std::to_string(i);
                break;
            }
            break;
        case ct_envshape_attack:
            switch (i)
            {
            case 0:
                txt = "Convex";
                break;
            case 1:
                txt = "Linear";
                break;
            case 2:
                txt = "Concave";
                break;
            default:
                txt = std::to_string(i);
                break;
            }
            break;
        case ct_ringmod_sineoscmode:
        case ct_sineoscmode:
            switch (i)
            {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
                txt = fmt::format("Wave {:d} (TX {:d})", i + 1, i + 1);
                break;
            case 28: // only hit in ringmod_sineoscmode
                txt = "Audio In";
                break;
            default:
                txt = fmt::format("Wave {:d}", i + 1);
            }
            break;
        case ct_sinefmlegacy:
            if (i == 0)
                txt = "Legacy (<v1.6.2)";
            else
                txt = "Same as FM2/3";
            break;
        case ct_vocoder_bandcount:
            txt = fmt::format("{:d} bands", i);
            break;
        case ct_distortion_waveshape:
        {
            if (i < 0 || i >= FXWaveShapers.size())
                txt = "ERROR " + std::to_string(i);
            else
                txt = sst::waveshapers::wst_names[(int)FXWaveShapers[i]];
        }
        break;
        case ct_mscodec:
            switch (i)
            {
            case 0:
                txt = "L-R > M-S > L-R";
                break;
            case 1:
                txt = "L-R > M-S";
                break;
            case 2:
                txt = "M-S > L-R";
                break;
            }
            break;
        case ct_oscroute:
            switch (i)
            {
            case 0:
                txt = "Filter 1";
                break;
            case 1:
                txt = "Both";
                break;
            case 2:
                txt = "Filter 2";
                break;
            }
            break;
        case ct_flangermode:
        {
            int mode = i;

            switch (mode)
            {
            case 0:
                txt = "Dry + Combs";
                break;
            case 1:
                txt = "Combs Only";
                break;
            case 2:
                txt = "Dry + Arp Combs";
                break;
            case 3:
                txt = "Arp Combs Only";
                break;
            }
        }
        break;
        case ct_fxlfowave:
        {
            switch (i)
            {
            case 0:
                txt = "Sine";
                break;
            case 1:
                txt = "Triangle";
                break;
            case 2:
                txt = "Sawtooth";
                break;
            case 3:
                txt = "Noise";
                break;
            case 4:
                txt = "Sample & Hold";
                break;
            case 5:
                txt = "Square";
                break;
            }
        }
        break;
        case ct_stringosc_excitation_model:
        {
            extern std::string stringosc_excitation_name(int);
            txt = stringosc_excitation_name(i);
        }
        break;
        case ct_alias_wave:
        {
            extern const char *alias_wave_name[];
            extern int alias_waves_count();
            txt = alias_wave_name[std::max(0, std::min(i, alias_waves_count() - 1))];
        }
        break;
        case ct_twist_engine:
        {
            extern std::string twist_engine_name(int);
            txt = twist_engine_name(i);
        }
        break;
        case ct_ensemble_stages:
        {
#if defined(_M_ARM64EC)
            txt = "name";
#else
            extern std::string ensemble_stage_name(int);
            txt = ensemble_stage_name(i);
#endif
        }
        break;
        case ct_reson_mode:
            switch (i)
            {
            case 0:
                txt = "Lowpass";
                break;
            case 1:
                txt = "Bandpass";
                break;
            case 2:
                txt = "Bandpass+Notch";
                break;
            case 3:
                txt = "Highpass";
                break;
            }
            break;
        case ct_vocoder_modulator_mode:
        {
            switch (i)
            {
            case 0:
                txt = "Monosum";
                break;
            case 1:
                txt = "Left Only";
                break;
            case 2:
                txt = "Right Only";
                break;
            case 3:
                txt = "Stereo";
                break;
            }
        }
        break;

        case ct_airwindows_fx:
        {
            // These are all the ones with a ParameterDiscreteIndexRemapper
            auto pd = dynamic_cast<ParameterDiscreteIndexRemapper *>(user_data);

            if (pd)
            {
                txt = pd->nameAtStreamedIndex(i);
            }
            else
            {
                txt = std::to_string(i);
            }
            break;
        }
        case ct_nimbusmode:
        {
            switch (i)
            {
            case 0:
                txt = "Granularizer";
                break;
            case 1:
                txt = "Pitch Shifter";
                break;
            case 2:
                txt = "Looping Delay";
                break;
            case 3:
                txt = "Spectral Madness";
                break;
#if EURORACK_CLOUDS_IS_SUPERPARASITES
            case 4:
                txt = "Oliverb";
                break;
            case 5:
                txt = "Reonestor";
                break;
            case 6:
                txt = "Kammerl";
                break;
            case 7:
                txt = "Spectral Cloud";
                break;
#endif
            default:
                txt = "Unknown Model";
                break;
            }
        }
        break;
        case ct_nimbusquality:
        {
            // https://github.com/pichenettes/eurorack/blob/84f4f67aaa25bf696093b224e2a51a5c18143e4f/clouds/dsp/granular_processor.h#L125-L128
            // [input value] [channels] [low fidelity]
            //     0b00          2          false
            //     0b01          1          false
            //     0b10          2          true
            //     0b11          1          true
            switch (i)
            {
            case 0: // binary 00
                txt = "32k 16-bit Stereo";
                break;
            case 1: // 0b01
                txt = "32k 16-bit Mono";
                break;
            case 2: // 0b10
                txt = "16k 8-bit Stereo";
                break;
            default: // 0b11
                txt = "16k 8-bit Mono";
                break;
            }
        }
        break;
        default:
            txt = fmt::format("{:d}", i);
            break;
        };
        break;
    }
    case vt_bool:
        b = external ? ef > 0.5f : val.b;
        txt = b ? "On" : "Off";

        break;
    };

    return txt;
}

float Parameter::get_value_f01() const
{
    if (ctrltype == ct_none)
        return 0;
    switch (valtype)
    {
    case vt_float:
        return (val.f - val_min.f) / (val_max.f - val_min.f);
        break;
    case vt_int:
        return Parameter::intScaledToFloat(val.i, val_max.i, val_min.i);
        break;
    case vt_bool:
        return val.b ? 1.f : 0.f;
        break;
    };

    return 0;
}

float Parameter::normalized_to_value(float value) const
{
    switch (valtype)
    {
    case vt_float:
        return value * (val_max.f - val_min.f) + val_min.f;
        break;
    case vt_int:
        return value * ((float)val_max.i - (float)val_min.i) + (float)val_min.i;
        break;
    case vt_bool:
        return (value > 0.5f) ? 1.f : 0.f;
        break;
    };

    return 0;
}

float Parameter::value_to_normalized(float value) const
{
    switch (valtype)
    {
    case vt_float:
        return (value - val_min.f) / (val_max.f - val_min.f);
        break;
    case vt_int:
        return ((float)value - (float)val_min.i) / ((float)val_max.i - (float)val_min.i);
        break;
    case vt_bool:
        return (value > 0.5) ? 1.f : 0.f;
        break;
    };

    return 0;
}

const wchar_t *Parameter::getUnit() const { return L""; }

float Parameter::get_default_value_f01() const
{
    if (ctrltype == ct_none)
        return 0;
    switch (valtype)
    {
    case vt_float:
        return (val_default.f - val_min.f) / (val_max.f - val_min.f);
        break;
    case vt_int:
        return Parameter::intScaledToFloat(val_default.i, val_max.i, val_min.i);
        break;
    case vt_bool:
        return val_default.b ? 1.f : 0.f;
        break;
    };

    return 0;
}

void Parameter::set_value_f01(float v, bool force_integer)
{
    switch (valtype)
    {
    case vt_float:
        val.f = v * (val_max.f - val_min.f) + val_min.f;
        break;
    case vt_int:
        val.i = Parameter::intUnscaledFromFloat(v, val_max.i, val_min.i);
        break;
    case vt_bool:
        val.b = (v > 0.5f);
        break;
    }

    bound_value(force_integer);
}

float Parameter::get_modulation_f01(float mod) const
{
    if (ctrltype == ct_none)
    {
        return 0;
    }

    if (valtype != vt_float)
    {
        return 0;
    }

    float v = (mod) / (val_max.f - val_min.f);

    return limit_range(v, -1.0f, 1.0f);
}

float Parameter::set_modulation_f01(float v) const
{
    if (ctrltype == ct_none)
    {
        return 0;
    }

    if (valtype != vt_float)
    {
        return 0;
    }

    float mod = v * (val_max.f - val_min.f);

    return mod;
}

bool Parameter::can_be_nondestructively_modulated() const
{
    switch (ctrltype)
    {
    /*
     * Because of the way these are read when we don't have a copy of
     * the modulation state, they are best to destructively modulate.
     */
    case ct_scenemode:
    case ct_scenesel:
    case ct_midikey_or_channel:
    case ct_polylimit:
    case ct_polymode:
    case ct_character:
    case ct_pbdepth:
    case ct_midikey:
    case ct_bool_keytrack:
    case ct_bool_retrigger:
    case ct_osctype:
    case ct_osccount:
    case ct_pitch_octave:
    case ct_wt2window:
    case ct_ringmod_sineoscmode:
    case ct_sineoscmode:
    case ct_sinefmlegacy:
    case ct_fmratio_int:
    case ct_stringosc_excitation_model:
    case ct_twist_engine:
    case ct_alias_wave:
    case ct_fmconfig:
    case ct_wstype:
    case ct_fbconfig:
    case ct_filtertype:
    case ct_filtersubtype:
    case ct_bool_relative_switch:
    case ct_bool_link_switch:
    case ct_fxbypass:
    case ct_fxtype:
    case ct_bool_mute:
    case ct_bool_solo:
    case ct_oscroute:
    case ct_envmode:
    case ct_envshape_attack:
    case ct_envshape:
    case ct_lfotype:
    case ct_lfotrigmode:
    case ct_bool_unipolar:
        return false;
    }
    return true;
}

bool Parameter::can_setvalue_from_string() const
{
    switch (ctrltype)
    {
    case ct_percent:
    case ct_percent_with_extend_to_bipolar:
    case ct_percent_with_extend_to_bipolar_static_default:
    case ct_percent_with_string_deform_hook:
    case ct_percent_deactivatable:
    case ct_dly_fb_clippingmodes:
    case ct_percent_oscdrift:
    case ct_percent200:
    case ct_percent_bipolar:
    case ct_percent_bipolar_with_string_filter_hook:
    case ct_percent_bipolar_deactivatable:
    case ct_percent_bipolar_stereo:
    case ct_percent_bipolar_pan:
    case ct_percent_bipolar_stringbal:
    case ct_percent_bipolar_w_dynamic_unipolar_formatting:
    case ct_twist_aux_mix:
    case ct_noise_color:
    case ct_pitch_semi7bp:
    case ct_pitch_semi7bp_absolutable:
    case ct_pitch:
    case ct_pitch4oct:
    case ct_pitch_extendable_very_low_minval:
    case ct_fmratio:
    case ct_float_toggle:
    case ct_syncpitch:
    case ct_amplitude:
    case ct_amplitude_clipper:
    case ct_amplitude_ringmod:
    case ct_decibel:
    case ct_decibel_narrow:
    case ct_decibel_narrow_deactivatable:
    case ct_decibel_narrow_extendable:
    case ct_decibel_narrow_short_extendable:
    case ct_decibel_extra_narrow:
    case ct_decibel_extra_narrow_deactivatable:
    case ct_decibel_attenuation:
    case ct_decibel_attenuation_clipper:
    case ct_decibel_attenuation_large:
    case ct_decibel_attenuation_plus12:
    case ct_decibel_fmdepth:
    case ct_decibel_extendable:
    case ct_decibel_deactivatable:
    case ct_freq_audible:
    case ct_freq_audible_deactivatable:
    case ct_freq_audible_deactivatable_hp:
    case ct_freq_audible_deactivatable_lp:
    case ct_freq_audible_fm3_extendable:
    case ct_freq_audible_with_tunability:
    case ct_freq_audible_very_low_minval:
    case ct_freq_reson_band1:
    case ct_freq_reson_band2:
    case ct_freq_reson_band3:
    case ct_freq_fm2_offset:
    case ct_freq_shift:
    case ct_freq_hpf:
    case ct_freq_vocoder_low:
    case ct_freq_vocoder_high:
    case ct_bandwidth:
    case ct_envtime:
    case ct_envtime_deformable:
    case ct_envtime_deactivatable:
    case ct_envtime_linkable_delay:
    case ct_envtime_lfodecay:
    case ct_delaymodtime:
    case ct_reverbtime:
    case ct_reverbpredelaytime:
    case ct_portatime:
    case ct_ensemble_lforate:
    case ct_lforate:
    case ct_lforate_deactivatable:
    case ct_lfoamplitude:
    case ct_lfophaseshuffle:
    case ct_lfodeform:
    case ct_detuning:
    case ct_oscspread:
    case ct_oscspread_bipolar:
    case ct_countedset_percent:
    case ct_countedset_percent_extendable:
    case ct_countedset_percent_extendable_wtdeform:
    case ct_flangerpitch:
    case ct_flangervoices:
    case ct_flangerspacing:
    case ct_filter_feedback:
    case ct_osc_feedback:
    case ct_osc_feedback_negative:
    case ct_chorusmodtime:
    case ct_pbdepth:
    case ct_polylimit:
    case ct_midikey:
    case ct_midikey_or_channel:
    case ct_phaser_stages:
    case ct_phaser_spread:
    case ct_rotarydrive:
    case ct_sendlevel:
    case ct_freq_mod:
    case ct_airwindows_param:
    case ct_airwindows_param_bipolar:
    case ct_reson_res_extendable:
    case ct_chow_ratio:
    case ct_comp_attack_ms:
    case ct_comp_release_ms:
    case ct_freq_ringmod:
    case ct_modern_trimix:
    case ct_ensemble_clockrate:
    case ct_alias_mask:
    case ct_alias_bits:
    case ct_tape_drive:
    case ct_tape_microns:
    case ct_tape_speed:
    case ct_spring_decay:
    case ct_bonsai_bass_boost:
    case ct_floaty_warp_time:
    case ct_floaty_delay_time:
    case ct_floaty_delay_playrate:
    {
        return true;
    }
    }
    return false;
}

double Parameter::get_freq_from_note_name(const std::string s, double defv)
{
    if ((s[0] >= 'a' && s[0] <= 'g') || (s[0] >= 'A' && s[0] <= 'G'))
    {
        int note = 0, sf = 0, octPos = 1, octOffset = 0;

        if (storage)
        {
            octOffset = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::MiddleC, 1);
        }

        if (s[0] >= 'a' && s[0] <= 'g')
        {
            note = s[0] - 'a';
        }

        if (s[0] >= 'A' && s[0] <= 'G')
        {
            note = s[0] - 'A';
        }

        while (s[octPos] == '#')
        {
            sf++;
            octPos++;
        }

        while (s[octPos] == 'b')
        {
            sf--;
            octPos++;
        }

        std::vector<int> df6 = {9, 11, 0, 2, 4, 5, 7};
        auto oct = std::atoi(s.c_str() + octPos) + octOffset;
        auto mn = df6[note] + (oct * 12) + sf;

        return 440.0 * pow(2.0, (mn - 69) / 12.0);
    }
    else
    {
        return defv;
    }
}

void Parameter::set_error_message(std::string &errMsg, const std::string value,
                                  const std::string unit, const ErrorMessageMode mode)
{
    if (mode == ErrorMessageMode::Special)
    {
        errMsg = value;
    }
    else
    {
        errMsg =
            fmt::format("Input can't be {} than {} {}!",
                        (mode == ErrorMessageMode::IsLarger) ? "larger" : "smaller", value, unit);
    }
}

bool Parameter::set_value_from_string(const std::string &s, std::string &errMsg)
{
    return set_value_from_string_onto(s, val, errMsg);
}

bool Parameter::set_value_from_string_onto(const std::string &s, pdata &ontoThis,
                                           std::string &errMsg)
{
    if (basicBlocksParamMetaData.has_value() && basicBlocksParamMetaData->supportsStringConversion)
    {
        auto res = basicBlocksParamMetaData->valueFromString(s, errMsg);
        if (res.has_value())
        {
            switch (valtype)
            {
            case vt_int:
                ontoThis.i = (int)std::round(*res);
                break;
            case vt_float:
                ontoThis.f = *res;
                break;
            case vt_bool:
                ontoThis.b = (*res > 0.5);
                break;
            }
            return true;
        }
        else if (!errMsg.empty())
        {
            return false;
        }
        else
        {
            std::cout << "Value from string failed" << std::endl;
        }
    }
    if (valtype == vt_int)
    {
        // default out of range value to test against later
        int ni = val_min.i - 1;
        // extend_range helpers
        int factor = 1, offset = 0;

        try
        {
            ni = std::stoi(s);
        }
        catch (const std::invalid_argument &)
        {
            // set value of ni out of range on invalid input
            ni = val_min.i - 1;
        }
        catch (const std::out_of_range &)
        {
            // likewise for out of range input
            ni = val_min.i - 1;
        }

        switch (ctrltype)
        {
        case ct_fmconfig:
        {
            // So FMConfig has names like "2 > 1 < 3" which actually *work* with
            // std::atoi so just clobber that and use the str compare
            if (s.length() > 1 && s[1] == ' ')
                ni = val_min.i - 1;
        }
        break;
        case ct_midikey_or_channel:
        {
            auto sm = storage->getPatch().scenemode.val.i;

            if (sm == sm_chsplit)
            {
                const char *strip = &(s[0]);

                while (*strip != '\0' && !std::isdigit(*strip))
                {
                    ++strip;
                }

                ni = (std::atof(strip) * 8) - 1;
                factor = 8;
                offset = 1;

                // breaks case after channel number input, but if we're in split mode we fall
                // through to ct_midikey
                break;
            }
        }
        case ct_midikey:
        {
            // if input is invalid or out of int range, try note recognition
            if (ni < val_min.i)
            {
                float uv = get_freq_from_note_name(s, 60) / 440.f;
                float n = log2(uv) * 12 + 69;

                ni = (int)n;
            }

            break;
        }
        case ct_pbdepth:
        {
            if (extend_range && s.find('/') != std::string::npos)
            {
                if (!supports_tuning_value_from_string(s, errMsg))
                    return false;

                try
                {
                    auto a = Tunings::toneFromString(s);
                    auto cts = a.cents;

                    ni = (int)std::round(cts);
                    factor = 100;
                }
                catch (const Tunings::TuningError &e)
                {
                    ni = 200;
                }
            }
            else if (extend_range)
            {
                try
                {
                    ni = std::round(std::stof(s) * 100);
                    factor = 100;
                }
                catch (const std::invalid_argument &)
                {
                    // set value of ni out of range on invalid input
                    ni = val_min.i - 1;
                }
                catch (const std::out_of_range &)
                {
                    // likewise for out of range input
                    ni = val_min.i - 2;
                }
            }
        }
        break;
        }

        if (ni >= val_min.i && ni <= val_max.i)
        {
            ontoThis.i = ni;

            return true;
        }
        else
        {
            // Try the inverse mapping
            for (int i = val_min.i; i <= val_max.i; ++i)
            {
                char txt[TXT_SIZE];
                auto nv = Parameter::intScaledToFloat(i, val_max.i, val_min.i);

                get_display(txt, true, nv);

                if (_stricmp(txt, s.c_str()) == 0)
                {
                    ontoThis.i = i;
                    return true;
                }
            }

            ErrorMessageMode isLarger =
                (ni > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
            auto bound = isLarger ? val_max.i : val_min.i;
            std::string unit = displayInfo.unit;

            getSemitonesOrKeys(unit);

            set_error_message(errMsg, fmt::format("{:d}", (bound / factor) + offset), unit,
                              isLarger);
        }

        return false;
    }

    if (valtype == vt_bool)
    {
        if (_stricmp(s.c_str(), "On") == 0)
        {
            ontoThis.b = true;
            return true;
        }

        if (_stricmp(s.c_str(), "Off") == 0)
        {
            ontoThis.b = false;
            return true;
        }

        return false;
    }

    if (_stricmp(displayInfo.maxLabel, s.c_str()) == 0)
    {
        ontoThis.f = val_max.f;
        return true;
    }
    if (_stricmp(displayInfo.minLabel, s.c_str()) == 0)
    {
        ontoThis.f = val_min.f;
        return true;
    }

    auto nv = std::atof(s.c_str());

    switch (displayType)
    {
    case Custom:
        // handled below
        break;
    case DelegatedToFormatter:
    {
        auto ef = dynamic_cast<ParameterExternalFormatter *>(user_data);

        if (ef)
        {
            float f;

            if (ef->stringToValue(this, s.c_str(), f))
            {
                ontoThis.f = limit_range(f, val_min.f, val_max.f);
                return true;
            }
        }
        // DO NOT break. Fall through!
    }
    case LinearWithScale:
    {
        if (displayInfo.customFeatures & ParamDisplayFeatures::kAllowsTuningFractionTypein)
        {
            // Check for a fraction
            if (s.find('/') != std::string::npos)
            {
                if (!supports_tuning_value_from_string(s, errMsg))
                    return false;

                try
                {
                    auto a = Tunings::toneFromString(s);
                    auto ct = a.cents;

                    nv = ct;
                    if (displayInfo.customFeatures & ParamDisplayFeatures::kUnitsAreSemitonesOrKeys)
                        nv /= 100.0;
                }
                catch (const Tunings::TuningError &e)
                {
                }
            }
        }

        float ext_mul = (can_extend_range() && extend_range) ? displayInfo.extendFactor : 1.f;
        float abs_mul = (can_be_absolute() && absolute) ? displayInfo.absoluteFactor : 1.f;
        float factor = ext_mul * abs_mul;
        float res = nv / displayInfo.scale / factor;
        float minval = val_min.f;

        if (displayInfo.customFeatures & ParamDisplayFeatures::kScaleBasedOnIsBiPolar)
        {
            if (!is_bipolar())
            {
                res = res * 2 - 1;
                minval = 0.f;
            }
        }

        if (res < val_min.f || res > val_max.f)
        {
            ErrorMessageMode isLarger =
                (res > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
            auto bound = isLarger ? val_max.f : minval;
            std::string unit = absolute ? displayInfo.absoluteUnit : displayInfo.unit;

            getSemitonesOrKeys(unit);

            set_error_message(errMsg, fmt::format("{:g}", bound * factor * displayInfo.scale), unit,
                              isLarger);

            return false;
        }

        ontoThis.f = res;

        return true;
    }
    case ATwoToTheBx:
    {
        if (displayInfo.customFeatures & ParamDisplayFeatures::kSwitchesFromSecToMillisec)
        {
            std::smatch m;
            std::regex r{"(\\d*(\\.\\d+)?)\\s*(m|ms)$",
                         std::regex_constants::ECMAScript | std::regex_constants::icase};

            if (std::regex_search(s, m, r))
            {
                nv = std::atof(m[1].str().c_str()) * .001f;
            }
        }

        if (displayInfo.supportsNoteName)
        {
            nv = get_freq_from_note_name(s, nv);
        }

        /*
        ** v = a 2^bx
        ** log2(v/a) = bx
        ** log2(v/a)/b = x;
        */

        // Special case where we have a minLabelValue
        if (nv == 0 && displayInfo.minLabelValue == 0)
        {
            ontoThis.f = val_min.f;
            return true;
        }
        if (nv <= 0 || !std::isfinite(nv))
        {
            set_error_message(errMsg, "Invalid value input!", "", ErrorMessageMode::Special);
            return false;
        }

        float res = log2f(nv / displayInfo.a) / displayInfo.b;

        if (res < val_min.f || res > val_max.f)
        {
            ErrorMessageMode isLarger =
                (res > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
            auto bound = isLarger ? val_max.f : val_min.f;
            std::string unit = absolute ? displayInfo.absoluteUnit : displayInfo.unit;

            set_error_message(errMsg,
                              fmt::format("{:g}", displayInfo.a * pow(2.0, bound * displayInfo.b)),
                              unit, isLarger);

            return false;
        }

        ontoThis.f = res;

        return true;
    }
    case Decibel:
    {
        if (std::isnan(nv) || (std::isinf(nv) && nv > 0))
        {
            set_error_message(errMsg, "Invalid value input!", "", ErrorMessageMode::Special);

            return false;
        }

        if (std::isinf(nv) && nv < 0)
        {
            nv = 0.f;
        }
        else
        {
            nv = db_to_amp(nv);
        }

        if (nv < val_min.f || nv > val_max.f)
        {
            ErrorMessageMode isLarger =
                (nv > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
            auto bound = isLarger ? val_max.f : val_min.f;

            set_error_message(errMsg, fmt::format("{:g}", amp_to_db(bound)), "dB", isLarger);

            return false;
        }

        ontoThis.f = nv;

        return true;
    }
    }

    switch (ctrltype)
    {
    case ct_chow_ratio:
    {
        if (nv < val_min.f || nv > val_max.f)
        {
            ErrorMessageMode isLarger =
                (nv > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
            auto bound = isLarger ? val_max.f : val_min.f;

            set_error_message(errMsg, fmt::format("1 : {:g}", bound), "", isLarger);

            return false;
        }

        ontoThis.f = nv;

        return true;
    }
    case ct_fmratio:
    {
        if (absolute)
        {
            nv = get_freq_from_note_name(s, nv);

            float uv = nv / 440.f;
            float n = log2(uv) * 12 + 69;
            float bpv = (n - 69) / 69.f;
            float value = bpv * 16 + 16;

            if (value < val_min.f || value > val_max.f)
            {
                ErrorMessageMode isLarger =
                    (value > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
                auto bound = isLarger ? val_max.f : val_min.f;
                bound = 440.0 * pow(2.0, (69 * (bound - 16.0) / 16.0) / 12);

                set_error_message(errMsg, fmt::format("{:.2f}", bound), "Hz", isLarger);

                return false;
            }

            ontoThis.f = bpv * 16 + 16;
        }
        else
        {
            // In this case we have to set nv differently
            const char *strip = &(s[0]);

            while (*strip != '\0' && !std::isdigit(*strip) && *strip != '.')
            {
                ++strip;
            }

            // OK so do we contain a /?
            const char *slp;

            if ((slp = strchr(strip, '/')) != nullptr)
            {
                float num = std::atof(strip);
                float den = std::atof(slp + 1);

                if (den == 0)
                {
                    nv = 1;
                }
                else
                {
                    nv = num / den;
                }
            }
            else
            {
                nv = std::atof(strip);
            }

            if (extend_range)
            {
                if (nv < 1)
                {
                    float oonv = -1.0 / nv;
                    // oonv = - ( ( 16 - f ) * 2 + 1)
                    // -oonv-1 = (16-f)*2
                    // (1+oonv)/2 = f - 16;
                    // (1+oonv)/2 + 16 = f;
                    nv = 16.f / 31.f * (1 + oonv) + 16;
                }
                else
                {
                    // nv = ( f - 16 ) * 2 + 1
                    // (nv - 1)/2 + 16 = f
                    nv = (nv - 1) * 16.f / 31.f + 16;
                }
            }

            if (nv < val_min.f || nv > val_max.f)
            {
                ErrorMessageMode isLarger =
                    (nv > val_max.f) ? ErrorMessageMode::IsLarger : ErrorMessageMode::IsSmaller;
                auto bound = isLarger ? val_max.f : val_min.f;

                if (extend_range && !isLarger)
                {
                    // we can get away with this because we know we're symmetric in this mode
                    bound = val_max.f;
                    set_error_message(errMsg, fmt::format("C : 1 / {:.0f}", bound), "", isLarger);
                }
                else
                {
                    set_error_message(errMsg, fmt::format("C : {:.0f}", bound), "", isLarger);
                }

                return false;
            }

            ontoThis.f = nv;
        }
    }
    break;

    default:

        return false;
    }

    return true;
}

// This function returns a value in range [-1, 1], scaled by the minimum and maximums
float Parameter::calculate_modulation_value_from_string(const std::string &s, std::string &errMsg,
                                                        bool &valid)
{
    if (basicBlocksParamMetaData.has_value() && basicBlocksParamMetaData->supportsStringConversion)
    {
        auto res = basicBlocksParamMetaData->modulationNaturalFromString(s, val.f, errMsg);
        if (res.has_value())
        {
            valid = true;
            return (*res) / (basicBlocksParamMetaData->maxVal - basicBlocksParamMetaData->minVal);
        }
        else
        {
            valid = false;
            return 0;
        }
    }
    errMsg = "Input is out of bounds!";
    valid = true;

    float mv = std::atof(s.c_str());

    switch (displayType)
    {
    case Custom:
        break;
    case DelegatedToFormatter:
    case LinearWithScale:
    {
        if (displayInfo.customFeatures & ParamDisplayFeatures::kAllowsTuningFractionTypein)
        {
            // Check for a fraction
            if (s.find('/') != std::string::npos)
            {
                if (!supports_tuning_value_from_string(s, errMsg))
                    return false;

                try
                {
                    auto a = Tunings::toneFromString(s);
                    auto ct = a.cents;

                    mv = ct;

                    if (displayInfo.customFeatures & ParamDisplayFeatures::kUnitsAreSemitonesOrKeys)
                    {
                        mv /= 100.0;
                    }
                }
                catch (const Tunings::TuningError &e)
                {
                }
            }
        }

        mv /= displayInfo.scale;

        float minval = val_min.f;
        float maxval = val_max.f;
        float factor = 1.f;

        if (displayInfo.customFeatures & ParamDisplayFeatures::kScaleBasedOnIsBiPolar)
        {
            if (!is_bipolar())
            {
                mv = mv * 2;
                minval = 0;
            }
        }

        if (can_be_absolute() && absolute)
        {
            mv /= displayInfo.absoluteFactor;
            factor *= displayInfo.absoluteFactor;
        }

        auto rmv = mv / (val_max.f - val_min.f);

        if (can_extend_range() && extend_range)
        {
            // mod value is in extended units already
            rmv = mv / (get_extended(val_max.f) - get_extended(val_min.f));
            minval = get_extended(val_min.f);
            maxval = get_extended(val_max.f);
        }

        if (rmv > 1 || rmv < -1)
        {
            std::string unit = absolute ? displayInfo.absoluteUnit : displayInfo.unit;

            getSemitonesOrKeys(unit);

            /*
             * So we need modval / maxval - minval < 1
             * or modval < maxval - minval
             * or modval / max - min > -1
             * or modval > -(max - min)
             * so modval has to be between +/- (max - min)
             */

            auto bound = (maxval - minval) * displayInfo.scale * factor;
            ErrorMessageMode isLarger =
                (rmv < -1) ? ErrorMessageMode::IsSmaller : ErrorMessageMode::IsLarger;

            set_error_message(errMsg, fmt::format("{:g}", (rmv < -1) ? -bound : bound), unit,
                              isLarger);

            valid = false;
        }

        return rmv;
    }
    case ATwoToTheBx:
    {
        if (temposync)
        {
            mv /= 100.0;
            auto rmv = mv / (get_extended(val_max.f) - get_extended(val_min.f));

            return rmv;
        }

        if (displayInfo.customFeatures & ParamDisplayFeatures::kAllowsModulationsInNotesAndCents)
        {
            if (s[0] == 'N' || s[0] == 'C' || s[0] == 'n' || s[0] == 'c')
            {
                auto mv = (float)std::atof(s.c_str() + 1);

                if (s[0] == 'C' || s[0] == 'c')
                {
                    mv = mv / 100.0;
                }

                auto rmv = mv / (get_extended(val_max.f) - get_extended(val_min.f));

                return rmv;
            }

            if (s[0] == 'T' || s[0] == 't')
            {
                if (!supports_tuning_value_from_string(s, errMsg))
                    return false;

                try
                {
                    auto a = Tunings::toneFromString(s.c_str() + 1);
                    auto mv = a.cents / 100.0;
                    auto rmv = mv / (get_extended(val_max.f) - get_extended(val_min.f));

                    return rmv;
                }
                catch (const Tunings::TuningError &e)
                {
                }
            }
        }

        /* modulation is displayed as:
         *
         * mv = mp - val
         *   = a2^b(v + m) - a2^bv
         * mv / a + 2^bv = 2^b(v + m)
         * log2(mv / a + 2^bv) = b(v + m)
         * log2(mv / a + 2^bv) / b - v = m
         */

        auto a = displayInfo.a;
        auto b = displayInfo.b;
        auto max_val = val_max.f;
        auto min_val = val_min.f;
        auto l2arg = mv / a + pow(2.0, b * val.f);
        auto omv = mv;

        std::string unit = absolute ? displayInfo.absoluteUnit : displayInfo.unit;
        getSemitonesOrKeys(unit);

        if (l2arg > 0)
        {
            min_val = log2f(l2arg) / b - val.f;
        }
        else
        {
            /*
             * mv / a + 2^bv > 0
             * mv > -a 2^bv
             */
            auto bound = -a * pow(2.0, b * val.f);

            set_error_message(errMsg, fmt::format("{:g}", bound), unit,
                              ErrorMessageMode::IsSmaller);

            valid = false;
        }

        auto range = (get_extended(val_max.f) - get_extended(val_min.f));
        auto rmv = min_val / range;
        ErrorMessageMode isLarger =
            (rmv < -1) ? ErrorMessageMode::IsSmaller : ErrorMessageMode::IsLarger;
        auto bound = (rmv < -1) ? (-a * pow(2.0, b * val.f)) : a * pow(2.0, b * (val.f + range));

        if (rmv > 1 || rmv < -1)
        {
            set_error_message(errMsg, fmt::format("{:g}", bound), unit, isLarger);

            valid = false;
        }

        auto finalModReach = a * pow(2.0, b * (val.f)) + omv;

        if (displayInfo.modulationCap > 0 && finalModReach > displayInfo.modulationCap)
        {
            auto bound = displayInfo.modulationCap - a * pow(2.0, b * val.f);

            set_error_message(errMsg, fmt::format("{:g}", bound), unit, ErrorMessageMode::IsLarger);

            valid = false;
        }

        return rmv;
    }
    case Decibel:
    {
        /*
         * amp2db is 18 * log2(x)
         *
         * we have
         * d = mp - val
         *   = 18 * ( log2( m + v ) - log2( v ) )
         * d / 18 + log2(v) = log2( m + v )
         * 2^(d/18 + log2(v) ) - v = m;
         *
         * But there's a gotcha. The minimum dB is -192 so we have to set the val.f accordingly.
         * That is, we have some amp2db we have used, called av. So:
         *
         * d = mv - av
         *   = 18 ( log2(m+v) ) - av
         * d / 18 + av/18 = log2(m+v)
         * 2^(d/18 + mv/18) - v = m
         */

        auto av = amp_to_db(val.f);

        auto d = (float)std::atof(s.c_str());
        auto mv = powf(2.0, (d / 18.0 + av / 18.0)) - val.f;
        auto range = (get_extended(val_max.f) - get_extended(val_min.f));
        auto rmv = mv / range;

        // The modulator will clamp anyway. This is all a bit tricky so just punt on negative case
        if (d < -192 - amp_to_db(val_max.f))
        {
            set_error_message(errMsg, fmt::format("{:g}", -192 - amp_to_db(val_max.f)), "dB",
                              ErrorMessageMode::IsSmaller);

            valid = false;
        }

        if (rmv > 1)
        {
            /*
             * 2 ^ ( d / 18 + av / 18) - val.f < 1
             * 2 ^ ( d / 18 + av / 18) < 1 + val.f
             * d + av < 18 ( log2( 1 + bal.f ) );
             * d < 18 ( log2( 1 + val.f ) ) - av
             */
            auto bound = 18 * (log2(1 + val.f)) - av;

            set_error_message(errMsg, fmt::format("{:g}", bound), "dB", ErrorMessageMode::IsLarger);

            valid = false;
        }

        return rmv;
    }
    }

    switch (ctrltype)
    {
    case ct_float_toggle:
    {
        mv /= displayInfo.scale;

        float minval = val_min.f;
        float maxval = val_max.f;

        auto rmv = mv / (val_max.f - val_min.f);

        if (rmv > 1 || rmv < -1)
        {
            auto bound = (maxval - minval) * displayInfo.scale;

            ErrorMessageMode isLarger =
                (rmv < -1) ? ErrorMessageMode::IsSmaller : ErrorMessageMode::IsLarger;

            set_error_message(errMsg, fmt::format("{:g}", (rmv < -1) ? -bound : bound),
                              displayInfo.unit, isLarger);

            valid = false;
        }

        return rmv;
    }
    case ct_fmratio:
    {
        int pos{0};

        while (pos < s.length() && !std::isdigit(s[pos]) && !(s[pos] == '.' || s[pos] == ','))
        {
            pos++;
        }

        auto q = s.substr(pos);

        if (absolute)
        {
            auto dfreq = std::atof(q.c_str());
            float bpv = (val.f - 16.0) / 16.0;
            float mul = 69;
            float note = 69 + mul * bpv;
            auto freq = 440.0 * pow(2.0, (note - 69.0) / 12);
            auto tgfreq = freq + dfreq;
            auto tgnote = log2(tgfreq / 440.0) * 12 + 69;
            auto tgbpv = (tgnote - 69) / mul;
            auto dbpv = (tgbpv - bpv) / 2.0;

            return dbpv;
        }

        if (extend_range)
        {
            /*
             * OK so what's happening here? Well, we need to give a number that
             * when handed in after going through get_extended gives us what
             * we typed in through the formatting.
             *
             * That is p->get_extended(mf) = v / 31 / 2. So let's get to work!
             */
            float mv = 0.f;
            const char *strip = &(q.c_str()[0]);

            while (*strip != '\0' && !std::isdigit(*strip) && *strip != '.')
            {
                ++strip;
            }

            // OK so do we contain a /?
            const char *slp;

            if ((slp = strchr(strip, '/')) != nullptr)
            {
                float num = std::atof(strip);
                float den = std::atof(slp + 1);

                if (den == 0)
                {
                    mv = 1;
                }
                else
                {
                    mv = num / den;
                    if (mv < 1)
                        mv = -1 / mv;
                }
            }
            else
            {
                mv = std::atof(strip);
            }

            // Normalized value
            auto exmf = (mv) / 31.0 / 2.0;
            // This reverses the scaling for extended mode. We now have
            // p->get_extended(val) = qq so reverse extended
            auto qq = exmf * 64 - 31 + (mv < 0 ? 1 : -1);

            // so the transformation out of extended is
            // x = (f-16) * 31/16 +- 1
            // ( x -+ 1 ) * 16 / 31 + 16 = f
            auto res = (qq + ((qq >= -32) ? +1 : -1)) * 16.f / 31.f + 16.f;

            return res / 32;
        }

        auto mv = (float)std::atof(q.c_str()) / (get_extended(val_max.f) - get_extended(val_min.f));

        if (mv < -1 || mv > 1)
        {
            valid = false;
        }

        return mv;
    }
    default:
    {
        // This works in all the linear cases so we need to handle fewer above than we'd think
        auto mv = (float)std::atof(s.c_str()) / (get_extended(val_max.f) - get_extended(val_min.f));

        if (mv < -1 || mv > 1)
        {
            auto minval = get_extended(val_min.f);
            auto maxval = get_extended(val_max.f);
            auto bound = (mv < -1) ? -(maxval - minval) : (maxval - minval);
            auto isLarger = (mv < -1) ? ErrorMessageMode::IsSmaller : ErrorMessageMode::IsLarger;
            std::string unit = absolute ? displayInfo.absoluteUnit : displayInfo.unit;

            getSemitonesOrKeys(unit);

            set_error_message(errMsg, fmt::format("{:g}", bound), unit, isLarger);

            valid = false;
        }

        return mv;
    }
    }

    valid = false;

    return 0.0;
}

bool Parameter::supports_tuning_value_from_string(const std::string &s, std::string &errMsg)
{
    if (!storage)
    {
        // Well that's odd! But let it go.
        return true;
    }
    if (storage->tuningApplicationMode == SurgeStorage::TuningApplicationMode::RETUNE_ALL)
    {
        errMsg = "Ratios not allowed when tuning after modulation!";
        return false;
    }

    return true;
}

std::atomic<bool> parameterNameUpdated(false);
