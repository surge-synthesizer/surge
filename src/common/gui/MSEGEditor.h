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
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "SkinSupport.h"

struct MSEGEditor : public VSTGUI::CViewContainer, public Surge::UI::SkinConsumingComponent {
   /*
    * Because this is 'late' in the build (in the gui) there is a copy of this structure in
    * the DawExtraState. If you add something to this state, add it there too. Revisit that
    * decision in 1.9 perhaps but #3316 is very late in the 18 cycle, which is why we
    * needed the extra storage.
    */
   struct State {
      int timeEditMode = 0;
   };
   MSEGEditor(SurgeStorage *storage, LFOStorage *lfodata, MSEGStorage *ms, State *eds, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b);
   ~MSEGEditor();
   void forceRefresh();
};
