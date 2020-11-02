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
#include <string>
#include "UserInteractions.h"


class CSurgeHyperlink : public VSTGUI::CControl {
public:
   CSurgeHyperlink( const VSTGUI::CRect &r ) : VSTGUI::CControl( r, nullptr ) {}

   void setURL( const std::string &iurl ) { this->url = iurl; }
   void setLabel( const std::string &lab ) {this->label = lab; }
   void setBitmap( VSTGUI::CBitmap *ibmp ) { this->bmp = ibmp; }
   void setHorizOffset( VSTGUI::CCoord xoffset) { this->xoffset = xoffset; }
   void setTextAlignment( VSTGUI::CHoriTxtAlign textalign) { this->textalign = textalign; }

   void setLabelColor( VSTGUI::CColor col ) { labelColor = col; }
   void setHoverColor( VSTGUI::CColor col ) { hoverColor= col; }
   void setFont( VSTGUI::CFontRef f ) { font = f; }
   // void setMultiLabel();

public:
   VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
   {
      return VSTGUI::kMouseEventHandled;
   }

   VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
   {
      Surge::UserInteractions::openURL(url);
      return VSTGUI::kMouseEventHandled;
   }

   VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
   {
      isHovered = true;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }

   VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override
   {
      isHovered = false;
      invalid();
      return VSTGUI::kMouseEventHandled;
   }

   void draw( VSTGUI::CDrawContext *pContext ) override;


private:
   std::string url;
   std::string label;
   VSTGUI::CBitmap *bmp = nullptr;
   bool isHovered = false;
   VSTGUI::CCoord xoffset = 0;
   VSTGUI::CHoriTxtAlign textalign = VSTGUI::kLeftText;

   VSTGUI::CColor labelColor, hoverColor;
   VSTGUI::CFontRef font;

   CLASS_METHODS(CSurgeHyperlink, VSTGUI::CControl);
};
