#pragma once

#include "vstcontrols.h"
#include "CScalableBitmap.h"

/*
** A switch which paints its background and draws a glyph over the front.
** When selected the value is the given value; when unselected the
** value is 0.
*/

class CGlyphSwitch : public VSTGUI::CControl
{
public:
   CGlyphSwitch( const VSTGUI::CRect &size,
                 VSTGUI::IControlListener *listener,
                 long tag,
                 int targetvalue );

   ~CGlyphSwitch() {
      if( glyph )
         glyph->forget();
   }

   // Use this later
   enum BackgroundStyle
   {
      DrawnBackground,
      SVGBackground
   };
   BackgroundStyle bgStyle = DrawnBackground;

   // If you change this order then some of the math in drawSingleElement will break (the + 4 and +2 stuff)
   enum DrawDisplays
   {
      Off, On,
      HoverOff, HoverOn,
      PressOff, PressOn,
      n_drawstates
   };


   enum DrawState
   {
      Hover = 1,
      Press = 2,
   };
   int mouseState = 0;

   enum FGMode {
      None,
      Glyph,
      GlyphChoice,
      Text
   };
   FGMode fgMode = FGMode::None;

   enum BGMode {
      Fill,
      SVG
   };
   BGMode bgMode = BGMode::Fill;
   
   void setGlyphText(std::string t) {
      fgMode = Text;
      if( glyph )
         glyph->forget();
      glyph = nullptr;
      fgText = t;
   }

   void setChoiceLabel(int choice, std::string s) {
      fgMode = Text;

      if( choiceLabels.size() < std::max( rows * cols, choice ) )
      {
         choiceLabels.resize(std::max( rows * cols, choice ) );
      }
      choiceLabels[choice] = s;
   }
   
   void setGlyphTextFont( std::string fontName, int fontSize ) {
      fgFont = fontName;
      fgFontSize = fontSize;
   }

   void setFont( std::string fontName ) { fgFont = fontName; }
   void setFontSize( int fs ) { fgFontSize = fs; }
   
   void setGlyphBitmap(CScalableBitmap *b) {
      fgMode = Glyph;
      if( glyph )
         glyph->forget();
      glyph = b;
      glyph->remember();
   }

   void setGlyphChoiceCount(int i) {
      fgMode = GlyphChoice;
      for( auto *p : glyphChoices )
         if( p )
            p->forget();
      glyphChoices.clear();
      glyphChoices.resize(i, nullptr);
   }
   
   void setGlyphChoiceBitmap(int i, CScalableBitmap *b) {
      fgMode = GlyphChoice;
      if( glyphChoices[i] )
         glyphChoices[i]->forget();
      glyphChoices[i] = b;
      glyphChoices[i]->remember();
   }

   void setBGBitmap(int which, CScalableBitmap *b) {
      bgMode = BGMode::SVG;
      if(bgBitmap[which] != nullptr)
         bgBitmap[which]->forget();
      bgBitmap[which] = b;
      bgBitmap[which]->remember();
   }

   void setFGColor(int which, VSTGUI::CColor c) {
      fgColor[which] = c;
   }
   void setBGColor(int which, VSTGUI::CColor c) {
      bgColor[which] = c;
   }

   virtual void setValue(float f) override;
   void setMultiValue( int i );
   
   void setBackgroundColor(DrawState ds, VSTGUI::CColor c) { bgColor[ds] = c; };
   void setForegroundColor(DrawState ds, VSTGUI::CColor c) { fgColor[ds] = c; };

   void setRows(int r) { rows = r; }
   void setCols(int c) { cols = c; }
   bool isMulti() { return rows != 1 || cols != 1; }
   
   virtual void draw( VSTGUI::CDrawContext *dc );
   void drawSingleElement( VSTGUI::CDrawContext *dc, const VSTGUI::CRect &sz, int rEl, int cEl );
      
   
   
   virtual VSTGUI::CMouseEventResult onMouseDown( VSTGUI::CPoint &where,
                                                  const VSTGUI::CButtonState &buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseUp( VSTGUI::CPoint &where,
                                                  const VSTGUI::CButtonState &buttons) override;
	virtual VSTGUI::CMouseEventResult onMouseMoved (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseEntered( VSTGUI::CPoint &where,
                                                     const VSTGUI::CButtonState &buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseExited( VSTGUI::CPoint &where,
                                                     const VSTGUI::CButtonState &buttons) override;

   void setMousedRowAndCol(VSTGUI::CPoint &where);
   
private:
   CScalableBitmap *bgBitmap[n_drawstates];
   VSTGUI::CColor bgColor[n_drawstates], fgColor[n_drawstates];
   CScalableBitmap *glyph = nullptr;
   std::vector<CScalableBitmap *> glyphChoices;
   
   std::vector<std::string> choiceLabels;
   
   std::string fgText = "", fgFont="Lato";
   int fgFontSize=11;

   int rows = 1, cols = 1;
   int mouseRow = 0, mouseCol = 0;
   int multiValue = 0;
   
   CLASS_METHODS( CGlyphSwitch, VSTGUI::CControl );
};
