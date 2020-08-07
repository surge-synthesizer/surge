#include "MSEGEditor.h"


using namespace VSTGUI;

MSEGEditor::MSEGEditor() : CViewContainer( CRect( 0, 0, 720, 425 ) )
{
   setBackgroundColor( kRedCColor );
   
   auto containerSize = getViewSize();
   containerSize.inset( 4, 4 );
   auto standin = new CTextLabel( containerSize, "One Day I Will Be an MSEG Editor" );
   standin->setTransparency(false);
   standin->setBackColor( kGreenCColor );
   standin->setFontColor( kBlackCColor );
   addView( standin );
}
