#include <iostream>
#include <iomanip>

#if MAC
#include "cocoa_minimal_main.h"
#endif

#if WINDOWS
#include <windows.h>
#include "win32minimal.h"
#endif

#include "CScalableBitmap.h"

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/controls/ctextlabel.h"

#include <sstream>
#include <vector>


struct SvgBrowser : public VSTGUI::IKeyboardHook
{
   struct SurgeSVGRenderComponent : public VSTGUI::CControl,  public VSTGUI::IDropTarget {
      SurgeSVGRenderComponent(const VSTGUI::CRect &size, VSTGUI::CFrame *f, VSTGUI::CTextLabel *ll ) : VSTGUI::CControl(size), frame(f)
         {
            l = ll;

            std::string defload =  "/Users/paul/Desktop/SVG3031/single_transp.svg";

            csb = new CScalableBitmap( defload, f );
            /* if( l )
               l->remeber(); */
         }
      ~SurgeSVGRenderComponent() {
         /* if( l )
            l->forget(); */
      }
      virtual void draw( VSTGUI::CDrawContext *dc ) override {
         auto s = getViewSize();
         dc->setFillColor( VSTGUI::kWhiteCColor );
         dc->drawRect( s, VSTGUI::kDrawFilled );

         for( auto i=5; i>=0; --i )
         {
            dc->setFrameColor( VSTGUI::CColor( i * 50, i * 50, i * 50 ) );
            s.inset( 1, 1 );
            dc->drawRect( s );
         }
         
         s.inset( 1, 1 );
         if( csb )
         {
            auto tf = VSTGUI::CGraphicsTransform().scale( zoom, zoom ).translate( s.left, s.top );
            VSTGUI::CDrawContext::Transform tr(*dc, tf);
            csb->setPhysicalZoomFactor( zoom * 100 );
            csb->draw( dc, VSTGUI::CRect( 0, 0, s.getWidth(), s.getHeight() ), VSTGUI::CPoint( 0, 0 ), 1.0 );
         }
      }
      virtual VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override {
         doingDrag = true;
         return VSTGUI::DragOperation::Copy;
      }
      virtual VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override {
         return VSTGUI::DragOperation::Copy;
      }
      virtual void onDragLeave(VSTGUI::DragEventData data) override {
         doingDrag = false;
      }
      
      virtual bool onDrop(VSTGUI::DragEventData data) override {
         auto drag = data.drag;
         auto where = data.pos;
         uint32_t ct = drag->getCount();
         if (ct != 1) return false;

         auto t = drag->getDataType(0);
         if (t != VSTGUI::IDataPackage::kFilePath) return false;

         const void* fn;
         drag->getData(0, fn, t);
         const char* fName = static_cast<const char*>(fn);
         auto sl = strlen(fName);

         if( sl > 4 )
         {
            auto dsvg = fName + sl - 4;
            if( strcmp( dsvg, ".svg" ) != 0 )
            {
               std::ostringstream oss;
               oss << "FILE does not end in '.svg' (file is '" << fName << "')";
               l->setText( oss.str() );
               l->invalid();
               return false;
            }
         }
         l->setText( fName );
         l->invalid();

         if( csb )
            delete csb;
         csb = new CScalableBitmap( std::string(fName), frame );
         invalid();
         
         return true;
      }
      virtual VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget () override { return this; }
      bool doingDrag = false;
      VSTGUI::CTextLabel *l;
      VSTGUI::CFrame *frame;
      CScalableBitmap *csb;

      float zoom = 1.0;
      
      CLASS_METHODS( SurgeSVGRenderComponent, VSTGUI::CControl )
   };

   struct ARedBox : public VSTGUI::CView
   {
      explicit ARedBox(const VSTGUI::CRect &r ) : VSTGUI::CView(r) {}
      ~ARedBox() = default;
      VSTGUI::CColor bg = VSTGUI::CColor(0,0,0);
      void draw(VSTGUI::CDrawContext *dc ) override
      {
         auto s = getViewSize();

         dc->setFillColor( bg );
         dc->drawRect(s, VSTGUI::kDrawFilled );
         dc->setFrameColor(VSTGUI::kBlueCColor);
         dc->drawRect(s,VSTGUI::kDrawStroked);

         auto r = VSTGUI::CRect(VSTGUI::CPoint(20,20), VSTGUI::CPoint(10,15));

         dc->setFillColor( VSTGUI::kRedCColor );
         dc->setFrameColor(VSTGUI::CColor(0,255,0));
         dc->drawRect( r, VSTGUI::kDrawFilledAndStroked);

         dc->setFrameColor(VSTGUI::CColor(255,180,0));
         dc->drawLine(VSTGUI::CPoint(0,0),VSTGUI::CPoint(40,40));

         auto mr = VSTGUI::CRect(mouseLoc, VSTGUI::CPoint(10,10));
         dc->setFillColor(VSTGUI::kWhiteCColor);
         dc->drawRect(mr, VSTGUI::kDrawFilled);
      }

      VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint& where,
                                               const VSTGUI::CButtonState& buttons) override
      {
         bg = VSTGUI::CColor(40,20,0);
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
      VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint& where,
                                              const VSTGUI::CButtonState& buttons) override
      {
         bg = VSTGUI::CColor(0,0,0);
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
      VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                            const VSTGUI::CButtonState& buttons) override
      {
         bg = VSTGUI::CColor(200,100,0);
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
      VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where,
                                          const VSTGUI::CButtonState& buttons) override
      {
         bg = VSTGUI::CColor(70,20,0);
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
      VSTGUI::CPoint mouseLoc;
      VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where,
                                             const VSTGUI::CButtonState& buttons) override
      {
         mouseLoc = where;
         invalid();
         return VSTGUI::kMouseEventHandled;
      }
   };

   SvgBrowser(VSTGUI::CFrame *f) : frame(f)
      {
         f->setBackgroundColor( VSTGUI::CColor( 200, 200, 210 ) );

         VSTGUI::CRect sz;
         f->getSize(sz);
         auto rb = new ARedBox(VSTGUI::CRect(10,10,200,200));
         f->addView(rb);
#if 0
         {
            VSTGUI::CRect pos( VSTGUI::CPoint( 0, 0 ), VSTGUI::CPoint( sz.getWidth(), 20 ) );
            l = new VSTGUI::CTextLabel(pos);
            l->setText( "Drop an SVG In" );
            l->setBackColor( VSTGUI::kWhiteCColor );
            l->setFontColor( VSTGUI::kBlueCColor );
            l->setFrameColor( VSTGUI::kBlueCColor );
            f->addView( l, pos );
         }

         auto subsz = sz;
         subsz.top += 20;
         svg = new SurgeSVGRenderComponent(subsz, f, l);
         svg->zoom = 4.00;
         f->addView( svg );
         
         f->registerKeyboardHook( this );
#endif
      }

   virtual int32_t onKeyDown(const VstKeyCode &code, VSTGUI::CFrame *f) override {
      switch( code.character )
      {
      case '1':
         if( svg->csb )
         {
            svg->zoom = 1.0;
            svg->invalid();
         }
         return 1;
         break;
      case '2':
         if( svg->csb )
         {
            svg->zoom = 1.25;
            svg->invalid();
         }
         return 1;
         break;
      case '3':
         if( svg->csb )
         {
            svg->zoom = 1.50;
            svg->invalid();
         }
         return 1;
         break;
      case '4':
         if( svg->csb )
         {
            svg->zoom = 2.00;
            svg->invalid();
         }
         return 1;
         break;
      case '5':
         if( svg->csb )
         {
            svg->zoom = 3.00;
            svg->invalid();
         }
         return 1;
         break;
      case '6':
         if( svg->csb )
         {
            svg->zoom = 4.00;
            svg->invalid();
         }
         return 1;
         break;

      }
      return -1;
   }
   virtual int32_t onKeyUp( const VstKeyCode &code, VSTGUI::CFrame *f) override {
      return -1;
   }
    
   VSTGUI::CFrame *frame;
   VSTGUI::CTextLabel *l;
   SurgeSVGRenderComponent *svg;
   std::vector<std::string> files;
   size_t filePos;
};

void svgExampleCB( VSTGUI::CFrame *f )
{
   new SvgBrowser(f); // yeah I leak it but look this is just demo code, OK?
    
}

#define BUILDCB svgExampleCB

#if MAC
int main( int argc, char **argv )
{
   cocoa_minimal_main(1100, 800, BUILDCB);
}
#endif

#if WINDOWS
void *hInstance;

int WINAPI wWinMain(HINSTANCE _hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
   hInstance = _hInstance;
   return win32minimal_main(_hInstance, 1100, 800, nCmdShow, BUILDCB);
}
#endif
 
