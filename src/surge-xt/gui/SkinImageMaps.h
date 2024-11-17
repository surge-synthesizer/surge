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
inline std::unordered_map<std::string, int> createIdNameMap()
{
    std::unordered_map<std::string, int> res;
    res["MAIN_BG"] = 102;
    res["SLIDER_VERT_BG"] = 105;
    res["FILTER_CONFIG"] = 112;
    res["SCENE_SELECT"] = 113;
    res["SCENE_MODE"] = 114;
    res["OSC_OCTAVE"] = 117;
    res["SCENE_OCTAVE"] = 118;
    res["OSC_MENU"] = 119;
    res["WAVESHAPER_MODE"] = 120;
    res["FILTER2_OFFSET"] = 121;
    res["OSC_SELECT"] = 122;
    res["PLAY_MODE"] = 123;
    res["MODSOURCE_BG"] = 124;
    res["OSC_KEYTRACK"] = 125;
    res["OSC_RETRIGGER"] = 126;
    res["MIXER_SOLO"] = 132;
    res["MIXER_MUTE"] = 134;
    res["FX_TYPE_ICONS"] = 136;
    res["FX_GRID"] = 137;
    res["FILTER2_RESONANCE_LINK"] = 140;
    res["MIXER_OSC_ROUTING"] = 143;
    res["FX_GLOBAL_BYPASS"] = 144;
    res["ENV_SHAPE"] = 145;
    res["LFO_TRIGGER_MODE"] = 146;
    res["SAVE_PATCH"] = 148;
    res["PREVNEXT_JOG"] = 149;
    res["OSC_FM_ROUTING"] = 151;
    res["LFO_UNIPOLAR"] = 152;
    res["SLIDER_HORIZ_HANDLE"] = 153;
    res["SLIDER_HORIZ_BG"] = 154;
    res["SLIDER_VERT_HANDLE"] = 157;
    res["ABOUT_BG"] = 158;
    res["FILTER_SUBTYPE"] = 160;
    res["OSC_CHARACTER"] = 161;
    res["ENV_MODE"] = 162;
    res["MAIN_MENU"] = 164;
    res["LFO_TYPE"] = 166;
    res["MENU_AS_SLIDER"] = 167;
    res["FILTER_MENU"] = 168;
    res["FILTER_ICONS"] = 169;
    res["SURGE_ICON"] = 170;
    res["MPE_BUTTON"] = 171;
    res["ZOOM_BUTTON"] = 172;
    res["TUNE_BUTTON"] = 173;
    res["NUMFIELD_POLY_SPLIT"] = 174;
    res["NUMFIELD_PITCHBEND"] = 175;
    res["NUMFIELD_KEYTRACK_ROOT"] = 176;
    res["LFO_MSEG_EDIT"] = 177;
    res["LFO_PRESET_MENU"] = 178;
    res["MODSOURCE_SHOW_LFO"] = 179;
    res["ABOUT_LOGOS"] = 180;
    res["VUMETER_BARS"] = 181;
    res["MIDI_LEARN"] = 182;
    res["WAVESHAPER_BG"] = 183;
    res["WAVESHAPER_ANALYSIS"] = 184;
    res["MODMENU_ICONS"] = 185;
    res["FAVORITE_BUTTON"] = 186;
    res["SEARCH_BUTTON"] = 187;
    res["FAVICON_MENU"] = 188;
    res["UNDO_BUTTON"] = 189;
    res["REDO_BUTTON"] = 190;
    res["MSEG_NODES"] = 301;
    res["MSEG_MOVEMENT_MODE"] = 302;
    res["MSEG_VERTICAL_SNAP"] = 303;
    res["MSEG_HORIZONTAL_SNAP"] = 304;
    res["MSEG_LOOP_MODE"] = 305;
    res["MSEG_SNAPVALUE_NUMFIELD"] = 306;
    res["MSEG_EDIT_MODE"] = 307;
    return res;
}

inline std::unordered_set<int> allowedImageIds()
{
    std::unordered_set<int> allowed;
    allowed.insert(102);
    allowed.insert(105);
    allowed.insert(112);
    allowed.insert(113);
    allowed.insert(114);
    allowed.insert(117);
    allowed.insert(118);
    allowed.insert(119);
    allowed.insert(120);
    allowed.insert(121);
    allowed.insert(122);
    allowed.insert(123);
    allowed.insert(124);
    allowed.insert(125);
    allowed.insert(126);
    allowed.insert(132);
    allowed.insert(134);
    allowed.insert(136);
    allowed.insert(137);
    allowed.insert(140);
    allowed.insert(143);
    allowed.insert(144);
    allowed.insert(145);
    allowed.insert(146);
    allowed.insert(148);
    allowed.insert(149);
    allowed.insert(151);
    allowed.insert(152);
    allowed.insert(153);
    allowed.insert(154);
    allowed.insert(157);
    allowed.insert(158);
    allowed.insert(160);
    allowed.insert(161);
    allowed.insert(162);
    allowed.insert(164);
    allowed.insert(166);
    allowed.insert(167);
    allowed.insert(168);
    allowed.insert(169);
    allowed.insert(170);
    allowed.insert(171);
    allowed.insert(172);
    allowed.insert(173);
    allowed.insert(174);
    allowed.insert(175);
    allowed.insert(176);
    allowed.insert(177);
    allowed.insert(178);
    allowed.insert(179);
    allowed.insert(180);
    allowed.insert(181);
    allowed.insert(182);
    allowed.insert(183);
    allowed.insert(184);
    allowed.insert(185);
    allowed.insert(186);
    allowed.insert(187);
    allowed.insert(188);
    allowed.insert(189);
    allowed.insert(190);
    allowed.insert(191);
    allowed.insert(301);
    allowed.insert(302);
    allowed.insert(303);
    allowed.insert(304);
    allowed.insert(305);
    allowed.insert(306);
    allowed.insert(307);

    return allowed;
}
