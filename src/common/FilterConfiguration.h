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

#include "Parameter.h"
#pragma once

/*
 * OK so what the heck is happening here you may ask? Well let me explain. Grab a cup of tea.
 *
 * As we approached Surge 18 with new filters, we realized our filter list was getting
 * long. So we decided to use submenus. But when doing that we realized that the
 * natural grouping of filters (LP, BP, HP, Notch, Special) didn't work with the way
 * our filter models had worked. We had things like "bandpass doesn't split 12/24" and
 * "obxd is kinda all sorts of filters". So, over in issue #3006, we decided that some filters
 * needed splitting, kinda like Luna had done in the nlfb set. But splitting means that
 * subtype counts change and streaming breaks. And we wanted to split filters which weren't
 * in sv14 (17->18) either.
 *
 * So what we did was add in streaming version 15 a "post patch streaming fixup" operation
 * which allows you to see the prior version, the current version, and adjust. That way we can
 * do things like "OBXD subtype 7 in streaming version 14 is actually OBXD HighPass Subtype 3
 * in streaming version 14 and above". Or whatever.
 *
 * But to do *that* we need to keep the old enums around so we can write that code. So these
 * are the old enums.
 *
 * Then the only question left is how to split. I chose the 'add at end for splits' method. That
 * is, fut_14_bp12 split into fut_bp12 and fut_bp24 but I added fut_bp24 at the end of the list.
 * Pros and cons. If I added it adjacent the names in the name array would line up, but the remapping
 * code would be wildly more complicated. I chose simple remapping code (that is SNH and vintage ladder
 * are no-ops in remap) at the cost of an oddly ordered filter name list. That's the right choice
 * but when you curse me for the odd name list, you can come back and read this cumment and feel
 * slightly better. Finally, items which split and changed meaning got a new name (so fut_comb is now fut_comp_pos
 * and fut_comb_neg, say) which requires us to go and fix up any code which refered to the
 * old values.
 */
enum fu_type_sv14
{
   fut_14_none = 0,
   fut_14_lp12,
   fut_14_lp24,
   fut_14_lpmoog,
   fut_14_hp12,
   fut_14_hp24,
   fut_14_bp12,
   fut_14_notch12,
   fut_14_comb,
   fut_14_SNH,
   fut_14_vintageladder,
   fut_14_obxd_2pole,
   fut_14_obxd_4pole,
   fut_14_k35_lp,
   fut_14_k35_hp,
   fut_14_diode,
   fut_14_nonlinearfb_lp,
   fut_14_nonlinearfb_hp,
   fut_14_nonlinearfb_n,
   fut_14_nonlinearfb_bp,
   n_fu_14_types,
};

enum fu_type
{
   fut_none = 0,
   fut_lp12,
   fut_lp24,
   fut_lpmoog,
   fut_hp12,
   fut_hp24,
   fut_bp12, // ADJ
   fut_notch12, // ADH
   fut_comb_pos,
   fut_SNH,
   fut_vintageladder,
   fut_obxd_2pole_lp, // ADJ
   fut_obxd_4pole,
   fut_k35_lp,
   fut_k35_hp,
   fut_diode,
   fut_nonlinearfb_lp,
   fut_nonlinearfb_hp,
   fut_nonlinearfb_n,
   fut_nonlinearfb_bp,
   fut_obxd_2pole_hp,
   fut_obxd_2pole_n,
   fut_obxd_2pole_bp,
   fut_bp24,
   fut_notch24,
   fut_comb_neg,
   fut_apf,
   n_fu_types,
};

/*
 * Each filter needs w names (alas). there's the name we show in the automation parameter and
 * so on (the value for get_display_name) which is in 'fut_names'. There's the value we put
 * in the menu which generally strips out LowPass and HighPass and stuff since they are already
 * groupsed which is fut_menu
 */
const char fut_names[n_fu_types][32] =
    {
        "Off",
        "LP 12 dB/oct",
        "LP 24 dB/oct",
        "Legacy Ladder",
        "HP 12 dB/oct",
        "HP 24 dB/oct",
        "BP 12 dB/oct",
        "N 12 dB/oct",
        "Comb +ve fb",
        "Sample & Hold",
        "Vintage Ladder",
        "LP OB-Xd 12 dB/o", // o to avoid truncation
        "OB-Xd 24 dB/oct",
        "LP K35",
        "HP K35",
        "Diode Ladder",
        "LP NL Feedback",
        "HP NL Feedback",
        "N NL Feedback",
        "BP NL Feedback",
        "HP OB-Xd 12 dB/o", // o to avoid truncation
        "N OB-Xd 12 dB/o",
        "BP OB-Xd 12 dB/o",
        "BP 24 dB/oct",
        "N 24 dB/oct",
        "Comb -ve fb",
        "AllPass",
        /* this is a ruler to ensure names do not exceed 31 characters
        0123456789012345678901234567890
          */
    };

const char fut_menu_names[n_fu_types][32] =
    {
        "Off",
        "12 dB/oct", // LP
        "24 dB/oct", // LP
        "Legacy Ladder",
        "12 dB/oct", // HP
        "24 dB/oct", // HP
        "12 dB/oct", // BP
        "12 dB/oct", // N
        "Comb +ve fb",
        "Sample & Hold",
        "Vintage Ladder",
        "OB-Xd 12 dB/oct", // LP
        "OB-Xd 24 dB/oct",
        "K35", // LP
        "K35", // HP
        "Diode Ladder",
        "NL Feedback", // LP
        "NL Feedback", // LP
        "NL Feedback", // LP
        "NL Feedback", // LP
        "OB-Xd 12 dB/oct", // HP
        "OB-Xd 12 dB/oct", // N
        "OB-Xd 12 dB/oct", // BP
        "24 dB/oct", // BP
        "24 dB/oct", // N
        "Comb -ve fb",
        "AllPass",
        /* this is a ruler to ensure names do not exceed 31 characters
        0123456789012345678901234567890
          */
    };

const char fut_bp_subtypes[3][32] =
    {
        "Clean",
        "Driven",
        "Smooth",
    };

const char fut_notch_subtypes[2][32] =
    {
        "Standard",
        "Mild",
    };

const char fut_comb_subtypes[2][64] =
    {
        "50% Wet",
        "100% Wet",
    };

const char fut_def_subtypes[3][32] =
    {
        "Clean",
        "Driven",
        "Smooth",
    };

const char fut_ldr_subtypes[4][32] =
    {
        "6 dB/oct",
        "12 dB/oct",
        "18 dB/oct",
        "24 dB/oct",
    };

const char fut_vintageladder_subtypes[6][32] =
    {
        "Strong",
        "Strong Compensated",
        "Dampened",
        "Dampened Compensated",
    };

const char fut_obxd_2p_subtypes[2][32] =
    {
        "Standard", "Pushed"
    };

const char fut_obxd_4p_subtypes[4][32] =
    {
        "6 dB/oct",
        "12 dB/oct",
        "18 dB/oct",
        "24 dB/oct",
    };

const char fut_k35_subtypes[5][32] =
    {
        "No Saturation",
        "Mild Saturation",
        "Moderate Saturation",
        "Heavy Saturation",
        "Extreme Saturation"
    };

const float fut_k35_saturations[5] =
    {
        0.0f,
        1.0f,
        2.0f,
        3.0f,
        4.0f
    };

const char fut_nlf_subtypes[4][32] =
    {
        "1 stage ",
        "2 stages",
        "3 stages",
        "4 stages",
    };

const char fut_nlf_saturators[4][6] =
    {
        "tanh",
        "soft",
        "sine",
        "OJD",
    };

const int fut_subcount[n_fu_types] =
    {
        0, // fut_none
        3, // fut_lp12
        3, // fut_lp24
        4, // fut_lpmoog
        3, // fut_hp12
        3, // fut_hp24
        3, // fut_bp12
        2, // fut_notch12
        2, // fut_comb_pos
        0, // fut_SNH
        4, // fut_vintageladder
        2, // fut_obxd_2pole
        4, // fut_obxd_4pole
        5, // fut_k35_lp
        5, // fut_k35_hp
        4, // fut_diode
        16, // fut_nonlinearfb_lp
        16, // fut_nonlinearfb_hp
        16, // fut_nonlinearfb_n
        16,  // fut_nonlinearfb_bp
        2, // fut_obxd_2pole_hp,
        2, // fut_obxd_2pole_n,
        2, // fut_obxd_2pole_bp,
        3, // fut_bp24,
        2, // fut_notch24,
        2, // fut_comb_neg,
        2, // fut_apf
    };

enum fu_subtype
{
   st_SVF = 0,
   st_Rough = 1,
   st_Smooth = 2,
   st_Medium = 3, // disabled
   st_Notch = 0,
   st_NotchMild = 1,
};


struct FilterSelectorMapper : public ParameterDiscreteIndexRemapper {
   std::vector<std::pair<int,std::string>> mapping;
   std::unordered_map<int,int> inverseMapping;
   void p( int i, std::string s ) { mapping.push_back(std::make_pair(i,s)); }
   FilterSelectorMapper()
   {
      p(fut_none, "" );

      p(fut_lp12, "Low Pass" );
      p(fut_lp24, "Low Pass" );
      p(fut_lpmoog, "Low Pass" );
      p(fut_vintageladder, "Low Pass" );
      p(fut_k35_lp, "Low Pass" );
      p(fut_diode, "Low Pass" );
      p(fut_obxd_2pole_lp, "Low Pass" ); // ADJ
      p(fut_obxd_4pole, "Low Pass" );
      p(fut_nonlinearfb_lp, "Low Pass" );

      p(fut_bp12, "Band Pass" );
      p(fut_bp24, "Band Pass" );
      p(fut_obxd_2pole_bp, "Band Pass" );
      p(fut_nonlinearfb_bp, "Band Pass" );

      p(fut_hp12, "High Pass" );
      p(fut_hp24, "High Pass" );
      p(fut_k35_hp, "High Pass" );
      p(fut_obxd_2pole_hp, "High Pass" );
      p(fut_nonlinearfb_hp, "High Pass" );

      p(fut_notch12, "Notch" );
      p(fut_notch24, "Notch" );
      p(fut_obxd_2pole_n, "Notch" );
      p(fut_nonlinearfb_n, "Notch" );

      p(fut_comb_pos, "Special" );
      p(fut_comb_neg, "Special" );
      p(fut_SNH, "Special" );
      p( fut_apf, "Special" );

      int c = 0;
      for( auto e : mapping )
         inverseMapping[e.first] = c++;

      if( mapping.size() != n_fu_types )
         std::cout << "BAD MAPPING TYPES" << std::endl;
   }

   virtual int remapStreamedIndexToDisplayIndex( int i ) override {
      return inverseMapping[i];
   }
   virtual std::string nameAtStreamedIndex( int i ) override {
      return fut_menu_names[i];
   }
   virtual bool hasGroupNames() override { return true; };

   virtual std::string groupNameAtStreamedIndex( int i ) override {
      return mapping[inverseMapping[i]].second;
   }

   virtual bool sortGroupNames() override {
      return false;
   }
};

/*
 * Finally we need to map streaming indices to positions on the glyph display. This
 * should *really* be in UI code but it is just a declaration and having all the declarations
 * together is useful. In the far distant future perhaps we customize this by skin.
 */

const int fut_glyph_index[n_fu_types] =
    {
        0, // fut_none
        1, // fut_lp12
        2, // fut_lp24
        3, // fut_lpmoog
        4, // fut_hp12
        5, // fut_hp24
        6, // fut_bp12
        7, // fut_notch12
        8, // fut_comb_pos
        9, // fut_SNH
        10, // fut_vintageladder
        11, // fut_obxd_2pole
        12, // fut_obxd_4pole
        13, // fut_k35_lp
        14, // fut_k35_hp
        15, // fut_diode
        2, // fut_nonlinearfb_lp
        5, // fut_nonlinearfb_hp
        7, // fut_nonlinearfb_n
        6,  // fut_nonlinearfb_bp
        11, // fut_obxd_2pole_hp,
        11, // fut_obxd_2pole_n,
        11, // fut_obxd_2pole_bp,
        6, // fut_bp24,
        7, // fut_notch24,
        8  // fut_comb_neg,
    };
