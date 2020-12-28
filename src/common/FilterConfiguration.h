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
 * OK so what the heck is happening here, you may ask? Well, let me explain. Grab a cup of tea.
 *
 * As we approached Surge 1.8 with new filters, we realized our filter list was getting
 * long. So we decided to use submenus. But when doing that, we realized that the
 * natural grouping of filters (LP, BP, HP, Notch, Special...) didn't work with the way
 * our filter models had worked. We had things like "bandpass doesn't split 12 dB/24 dB" into
 * separate types, and "OB-Xd is kinda all sorts of filters". So, over in issue #3006,
 * we decided that some filters needed splitting, kinda like what Luna had done in the non-linear
 * feedback set. But splitting means that subtype counts change and streaming breaks.
 * And we wanted to split filters which weren't in streaming version 14 (1.7->1.8) either.
 *
 * So what we did was add in streaming version 15 a "post patch streaming fixup" operation
 * which allows you to see the prior version, the current version, and adjust. That way we can
 * do things like "OB-Xd subtype 7 in streaming version 14 is actually OB-Xd highpass subtype 3
 * in streaming version 15 and above". Or whatever.
 *
 * But to do *that*, we need to keep the old enums around so we can write that code. So these
 * are the old enums.
 *
 * Then the only question left is - how to split? I chose the 'add at end for splits' method. That
 * is, fut_14_bp12 splits into fut_bp12 and fut_bp24, but I added fut_bp24 at the end of the list.
 * Pros and cons: if I added it adjacent, the names in the name array would line up, but the remapping
 * code would be wildly more complicated. I chose simple remapping code (that is S&H and vintage ladder
 * are no-ops in remap) at the cost of an oddly ordered filter name list. That's the right choice,
 * but when you curse me for the odd name list, you can come back and read this comment and feel
 * slightly better. Finally, items which split and changed meaning got a new name (so fut_comb is now
 * fut_comp_pos and fut_comb_neg, say), which requires us to go and fix up any code which refered to the
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
   fut_14_cutoffwarp_lp,
   fut_14_cutoffwarp_hp,
   fut_14_cutoffwarp_n,
   fut_14_cutoffwarp_bp,
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
   fut_cutoffwarp_lp,
   fut_cutoffwarp_hp,
   fut_cutoffwarp_n,
   fut_cutoffwarp_bp,
   fut_obxd_2pole_hp,
   fut_obxd_2pole_n,
   fut_obxd_2pole_bp,
   fut_bp24,
   fut_notch24,
   fut_comb_neg,
   fut_apf,
   fut_cutoffwarp_ap,
   fut_resonancewarp_lp,
   fut_resonancewarp_hp,
   fut_resonancewarp_n,
   fut_resonancewarp_bp,
   fut_resonancewarp_ap,
   n_fu_types,
};

/*
 * Each filter needs w names (alas). there's the name we show in the automation parameter and
 * so on (the value for get_display_name) which is in 'fut_names'. There's the value we put
 * in the menu which generally strips out Lowpass and Highpass and stuff, since they are already
 * grouped in submenus, and this is in fut_menu array
 */
const char fut_names[n_fu_types][32] =
    {
        "Off",              // fut_none
        "LP 12 dB",         // fut_lp12
        "LP 24 dB",         // fut_lp24
        "LP Legacy Ladder", // fut_lpmoog
        "HP 12 dB",         // fut_hp12
        "HP 24 dB",         // fut_hp24
        "BP 12 dB",         // fut_bp12
        "N 12 dB",          // fut_notch12
        "FX Comb +",        // fut_comb_pos
        "FX Sample & Hold", // fut_SNH
        "LP Vintage Ladder",// fut_vintageladder
        "LP OB-Xd 12 dB",   // fut_obxd_2pole_lp
        "LP OB-Xd 24 dB",   // fut_obxd_4pole
        "LP K35",           // fut_k35_lp
        "HP K35",           // fut_k35_hp
        "LP Diode Ladder",  // fut_diode
        "LP Cutoff Warp",   // fut_cutoffwarp_lp
        "HP Cutoff Warp",   // fut_cutoffwarp_hp
        "N Cutoff Warp",    // fut_cutoffwarp_n
        "BP Cutoff Warp",   // fut_cutoffwarp_bp
        "HP OB-Xd 12 dB",   // fut_obxd_2pole_hp
        "N OB-Xd 12 dB",    // fut_obxd_2pole_n
        "BP OB-Xd 12 dB",   // fut_obxd_2pole_bp
        "BP 24 dB",         // fut_bp24
        "N 24 dB",          // fut_notch24
        "FX Comb -",        // fut_comb_neg
        "FX Allpass",       // fut_apf
        "FX Cutoff Warp AP",  // fut_cutoffwarp_ap
        "LP Res Warp",     // fut_resonancewarp_lp
        "HP Res Warp",     // fut_resonancewarp_hp
        "N Res Warp",      // fut_resonancewarp_n
        "BP Res Warp",     // fut_resonancewarp_bp
        "FX Res Warp AP",  // fut_resonancewarp_ap
        /* this is a ruler to ensure names do not exceed 31 characters
         0123456789012345678901234567890
        */
    };

const char fut_menu_names[n_fu_types][32] =
    {
        "Off",
        "12 dB", // LP
        "24 dB", // LP
        "Legacy Ladder",
        "12 dB", // HP
        "24 dB", // HP
        "12 dB", // BP
        "12 dB", // N
        "Comb +",
        "Sample & Hold",
        "Vintage Ladder",
        "OB-Xd 12 dB", // LP
        "OB-Xd 24 dB", // LP
        "K35", // LP
        "K35", // HP
        "Diode Ladder",
        "Cutoff Warp", // LP
        "Cutoff Warp", // HP
        "Cutoff Warp", // N
        "Cutoff Warp", // BP
        "OB-Xd 12 dB", // HP
        "OB-Xd 12 dB", // N
        "OB-Xd 12 dB", // BP
        "24 dB", // BP
        "24 dB", // N
        "Comb -",
        "Allpass",
        "Cutoff Warp Allpass",
        "Resonance Warp", // LP
        "Resonance Warp", // HP
        "Resonance Warp", // N
        "Resonance Warp", // BP
        "Resonance Warp Allpass",
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
        "6 dB",
        "12 dB",
        "18 dB",
        "24 dB",
    };

const char fut_vintageladder_subtypes[6][32] =
    {
        "Type 1",
        "Type 1 Compensated",
        "Type 2",
        "Type 2 Compensated",
    };

const char fut_obxd_2p_subtypes[2][32] =
    {
        "Standard", "Pushed"
    };

const char fut_obxd_4p_subtypes[4][32] =
    {
        "6 dB",
        "12 dB",
        "18 dB",
        "24 dB",
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
        "1 Stage",
        "2 Stages",
        "3 Stages",
        "4 Stages",
    };

const char fut_nlf_saturators[4][16] =
    {
        "tanh",
        "Soft Clip",
        "OJD",
        "Sine",
    };

const int fut_subcount[n_fu_types] =
    {
        0,  // fut_none
        3,  // fut_lp12
        3,  // fut_lp24
        4,  // fut_lpmoog
        3,  // fut_hp12
        3,  // fut_hp24
        3,  // fut_bp12
        2,  // fut_notch12
        2,  // fut_comb_pos
        0,  // fut_SNH
        4,  // fut_vintageladder
        2,  // fut_obxd_2pole
        4,  // fut_obxd_4pole
        5,  // fut_k35_lp
        5,  // fut_k35_hp
        4,  // fut_diode
        12, // fut_cutoffwarp_lp
        12, // fut_cutoffwarp_hp
        12, // fut_cutoffwarp_n
        12, // fut_cutoffwarp_bp
        2,  // fut_obxd_2pole_hp,
        2,  // fut_obxd_2pole_n,
        2,  // fut_obxd_2pole_bp,
        3,  // fut_bp24,
        2,  // fut_notch24,
        2,  // fut_comb_neg,
        0,  // fut_apf
        12, // fut_cutoffwarp_ap
        8, // fut_resonancewarp_lp
        8, // fut_resonancewarp_hp
        8, // fut_resonancewarp_n
        8, // fut_resonancewarp_bp
        8, // fut_resonancewarp_ap
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

      p(fut_lp12, "Lowpass" );
      p(fut_lp24, "Lowpass" );
      p(fut_lpmoog, "Lowpass" );
      p(fut_vintageladder, "Lowpass" );
      p(fut_k35_lp, "Lowpass" );
      p(fut_diode, "Lowpass" );
      p(fut_obxd_2pole_lp, "Lowpass" ); // ADJ
      p(fut_obxd_4pole, "Lowpass" );
      p(fut_cutoffwarp_lp, "Lowpass" );
      p(fut_resonancewarp_lp, "Lowpass" );

      p(fut_bp12, "Bandpass" );
      p(fut_bp24, "Bandpass" );
      p(fut_obxd_2pole_bp, "Bandpass" );
      p(fut_cutoffwarp_bp, "Bandpass" );
      p(fut_resonancewarp_bp, "Bandpass" );

      p(fut_hp12, "Highpass" );
      p(fut_hp24, "Highpass" );
      p(fut_k35_hp, "Highpass" );
      p(fut_obxd_2pole_hp, "Highpass" );
      p(fut_cutoffwarp_hp, "Highpass" );
      p(fut_resonancewarp_hp, "Highpass" );

      p(fut_notch12, "Notch" );
      p(fut_notch24, "Notch" );
      p(fut_obxd_2pole_n, "Notch" );
      p(fut_cutoffwarp_n, "Notch" );
      p(fut_resonancewarp_n, "Notch" );

      p(fut_apf, "Effect" );
      p(fut_cutoffwarp_ap, "Effect" );
      p(fut_resonancewarp_ap, "Effect" );
      p(fut_comb_pos, "Effect" );
      p(fut_comb_neg, "Effect" );
      p(fut_SNH, "Effect" );

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

   virtual bool supportsTotalIndexOrdering() override { return true; }
   virtual const std::vector<int> totalIndexOrdering() override {
      auto res = std::vector<int>();
      for( auto m : mapping ) res.push_back( m.first );
      return res;
   }

};

/*
 * Finally we need to map streaming indices to positions on the glyph display. This
 * should *really* be in UI code but it is just a declaration and having all the declarations
 * together is useful. In the far distant future perhaps we customize this by skin.
 */

const int lprow = 1;
const int bprow = 2;
const int hprow = 3;
const int nrow  = 4;
const int fxrow = 5;
const int fut_glyph_index[n_fu_types][2] =
    {
        { 0,     0 }, // fut_none
        { 0, lprow }, // fut_lp12
        { 1, lprow }, // fut_lp24
        { 3, lprow }, // fut_lpmoog
        { 0, hprow }, // fut_hp12
        { 1, hprow }, // fut_hp24
        { 0, bprow }, // fut_bp12
        { 0,  nrow }, // fut_notch12
        { 1, fxrow }, // fut_comb_pos
        { 3, fxrow }, // fut_SNH
        { 4, lprow }, // fut_vintageladder
        { 6, lprow }, // fut_obxd_2pole
        { 7, lprow }, // fut_obxd_4pole
        { 2, lprow }, // fut_k35_lp
        { 2, hprow }, // fut_k35_hp
        { 5, lprow }, // fut_diode
        { 8, lprow }, // fut_cutoffwarp_lp
        { 4, hprow }, // fut_cutoffwarp_hp
        { 3,  nrow }, // fut_cutoffwarp_n
        { 3, bprow }, // fut_cutoffwarp_bp
        { 3, hprow }, // fut_obxd_2pole_hp,
        { 2,  nrow }, // fut_obxd_2pole_n,
        { 2, bprow }, // fut_obxd_2pole_bp,
        { 1, bprow }, // fut_bp24,
        { 1,  nrow }, // fut_notch24,
        { 2, fxrow }, // fut_comb_neg,
        { 0, fxrow }, // fut_apf
        { 0, fxrow }, // fut_cutoffwarp_ap (this is temporarily set to just use the regular AP glyph)
        { 9, lprow }, // fut_resonancewarp_lp
        { 5, hprow }, // fut_resonancewarp_hp
        { 4,  nrow }, // fut_resonancewarp_n
        { 4, bprow }, // fut_resonancewarp_bp
        { 0, fxrow }, // fut_resonancewarp_ap (also temporarily set to just use the regular AP glyph)
    };
