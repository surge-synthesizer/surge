/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#pragma once

#include "vstgui/vstgui.h"

const VSTGUI::CColor col_plain_std = VSTGUI::CColor(214, 209, 198, 255);
const VSTGUI::CColor col_plain_shadow = VSTGUI::CColor(192, 188, 175, 255);
const VSTGUI::CColor col_plain_highlight = VSTGUI::CColor(236, 234, 230, 255);
const VSTGUI::CColor col_plain_hover = VSTGUI::CColor(219, 216, 209, 255);

const VSTGUI::CColor col_line_weak = VSTGUI::CColor(128, 128, 128, 255);
const VSTGUI::CColor col_line_strong = VSTGUI::CColor(54, 65, 73, 255);

/*const VSTGUI::CColor col_shade1_std = VSTGUI::CColor(101,115,126,0);
const VSTGUI::CColor col_shade1_highlight = VSTGUI::CColor(121,138,151,0);*/
const VSTGUI::CColor col_shade1_std = VSTGUI::CColor(57, 127, 181, 255);
const VSTGUI::CColor col_shade1_highlight = VSTGUI::CColor(135, 180, 215, 255);
const VSTGUI::CColor col_shade1_shadow = VSTGUI::CColor(78, 89, 97, 255);

/*const VSTGUI::CColor col_shade2_std = VSTGUI::CColor(101,126,117,0);
const VSTGUI::CColor col_shade2_highlight = VSTGUI::CColor(120,150,139,0);*/
const VSTGUI::CColor col_shade2_std = VSTGUI::CColor(69, 131, 108, 255);
const VSTGUI::CColor col_shade2_highlight = VSTGUI::CColor(126, 184, 163, 255);
const VSTGUI::CColor col_shade2_shadow = VSTGUI::CColor(78, 97, 90, 255);
const VSTGUI::CColor col_shade3_std = VSTGUI::CColor(181, 57, 57, 255);
const VSTGUI::CColor col_shade3_highlight = VSTGUI::CColor(215, 135, 135, 255);
const VSTGUI::CColor col_shade3_shadow = VSTGUI::CColor(97, 78, 78, 255);

const VSTGUI::CColor col_shade4_std = VSTGUI::CColor(181, 174, 57, 255);
const VSTGUI::CColor col_shade4_highlight = VSTGUI::CColor(215, 200, 100, 255);
const VSTGUI::CColor col_shade4_shadow = VSTGUI::CColor(97, 78, 57, 255);

// greyed out
const VSTGUI::CColor col_shade0_std = VSTGUI::CColor(76, 76, 76, 255);
const VSTGUI::CColor col_shade0_highlight = VSTGUI::CColor(104, 104, 104, 255);
const VSTGUI::CColor col_shade0_shadow = VSTGUI::CColor(51, 51, 51, 255);

const VSTGUI::CColor col_white = VSTGUI::CColor(255, 255, 255, 255);
const VSTGUI::CColor col_black = VSTGUI::CColor(0, 0, 0, 255);
const VSTGUI::CColor col_dark_grey = VSTGUI::CColor(42, 42, 42, 255);
const VSTGUI::CColor col_red = VSTGUI::CColor(255, 0, 0, 255);
const VSTGUI::CColor col_green = VSTGUI::CColor(0, 255, 0, 255);
const VSTGUI::CColor col_blue = VSTGUI::CColor(0, 0, 255, 255);
const VSTGUI::CColor col_yellow = VSTGUI::CColor(255, 255, 0, 255);
const VSTGUI::CColor col_orange = VSTGUI::CColor(255, 144, 0, 255);

const VSTGUI::CColor col_loopline = VSTGUI::CColor(155, 157, 187, 255);
const VSTGUI::CColor col_sampleline = VSTGUI::CColor(155, 187, 159, 255);

/*const VSTGUI::CColor col_disp = VSTGUI::CColor(245,173,0);
const VSTGUI::CColor col_disp_dark = VSTGUI::CColor(91,57,8,0);
const VSTGUI::CColor col_disp_off = VSTGUI::CColor(157,114,9,0);*/

// yellowish displays
/*const VSTGUI::CColor col_disp = VSTGUI::CColor(104,102,91,0);
const VSTGUI::CColor col_disp_dark = VSTGUI::CColor(233,230,206,0);
const VSTGUI::CColor col_disp_off = VSTGUI::CColor(168,166,151,0);
const VSTGUI::CColor col_disp_faint = VSTGUI::CColor(218,215,193,0);
const VSTGUI::CColor col_disp_semifaint = VSTGUI::CColor(196,193,174,0);*/

// blueish displays
const VSTGUI::CColor col_disp = VSTGUI::CColor(89, 89, 106, 255);
const VSTGUI::CColor col_disp_dark = VSTGUI::CColor(214, 218, 223, 255);
const VSTGUI::CColor col_disp_off = VSTGUI::CColor(154, 154, 170, 255);
const VSTGUI::CColor col_disp_faint = VSTGUI::CColor(199, 203, 208, 255);
const VSTGUI::CColor col_disp_semifaint = VSTGUI::CColor(176, 178, 190, 255);
const VSTGUI::CColor col_hitpoints = VSTGUI::CColor(215, 61, 45, 255);

const VSTGUI::CColor col_ctrl_text = col_dark_grey;
const VSTGUI::CColor col_ctrl_frame = col_dark_grey;
const VSTGUI::CColor col_ctrl_fill = col_white; // col_plain_highlight;
const VSTGUI::CColor col_ctrl_selected = col_shade1_std;
