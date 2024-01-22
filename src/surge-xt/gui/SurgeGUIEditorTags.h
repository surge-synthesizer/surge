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

#ifndef SURGE_SRC_SURGE_XT_GUI_SURGEGUIEDITORTAGS_H
#define SURGE_SRC_SURGE_XT_GUI_SURGEGUIEDITORTAGS_H

enum SurgeGUIEditorTags
{
    tag_scene_select = 1,

    tag_osc_select,
    tag_osc_menu,

    tag_fx_select,
    tag_fx_menu,

    tag_patchname,

    tag_mp_category,
    tag_mp_patch,

    tag_mp_jogwaveshape,

    tag_analyzewaveshape,
    tag_analyzefilters,

    tag_store,

    tag_mod_source0,
    tag_mod_source_end = tag_mod_source0 + n_modsources,

    tag_settingsmenu,

    tag_mp_jogfx,

    tag_value_typein,

    tag_status_mpe,
    tag_status_zoom,
    tag_status_tune,

    tag_action_undo,
    tag_action_redo,

    tag_mseg_edit,
    tag_lfo_menu,

    tag_main_vu_meter,

    start_paramtags,
};

#endif // SURGE_XT_SURGEGUIEDITORTAGS_H
