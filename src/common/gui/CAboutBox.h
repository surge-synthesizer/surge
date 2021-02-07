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
#include "SkinSupport.h"

class SurgeGUIEditor;

class CAboutBox : public VSTGUI::CViewContainer, public VSTGUI::IControlListener
{
  public:
    CAboutBox(const VSTGUI::CRect &size, SurgeGUIEditor *editor, SurgeStorage *storage,
              const std::string &host, Surge::UI::Skin::ptr_t skin,
              std::shared_ptr<SurgeBitmaps> bitmapStore, int devModeGrid);
    VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint &where,
                                        const VSTGUI::CButtonState &buttons) override;
    VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint &where,
                                          const VSTGUI::CButtonState &buttons) override;
    void valueChanged(VSTGUI::CControl *pControl) override;

    SurgeGUIEditor *editor;
    SurgeStorage *storage;
    std::string infoStringForClipboard = "";
    int devGridResolution = -1;
};