/*
 ** Surge Synthesizer is Free and Open Source Software
 **
 ** Surge is made available under the Gnu General Public License, v3.0
 ** https://www.gnu.org/licenses/gpl-3.0.en.html
 **
 ** Copyright 2004-2021 by various individuals as described by the Git transaction log
 **
 ** All source at: https://github.com/surge-synthesizer/surge.git
 **
 ** Surge was a commercial product from 2004-2018, with Copyright and ownership
 ** in that period held by Claes Johanson at Vember Audio. Claes made Surge
 ** open source in September 2018.
 */

#ifndef SURGE_XT_SURGEGUIEDITORTAGS_H
#define SURGE_XT_SURGEGUIEDITORTAGS_H

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
    start_paramtags,
};

#endif // SURGE_XT_SURGEGUIEDITORTAGS_H
