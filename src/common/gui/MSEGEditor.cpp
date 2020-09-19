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

#include "MSEGEditor.h"
#include "MSEGModulationHelper.h"
#include "DebugHelpers.h"
#include "SkinColors.h"
#include "basic_dsp.h" // for limit_range
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "CHSwitch2.h"

/*
** The SVG MSEG Ids
**
** 301 - IDB_MSEG_SEGMENT_HANDLES 48x36, 3 rows (unhover, hover, grab) x 4 columns (center, leftside, rightside, cp) for the mseg grab handles
** 302 - IDB_MSEG_MOVEMENT 120x60 3 rows (ripple, bound draw) x 3 columns (the states)
** 303 - IDB_MSEG_EDITMODE 120x40 2 rows (simple/advanced) x 2 columns
*/


using namespace VSTGUI;

struct MSEGCanvas;

// This is 720 x 120
struct MSEGControlRegion : public CViewContainer, public Surge::UI::SkinConsumingComponent, public VSTGUI::IControlListener {
   MSEGControlRegion(const CRect &size, MSEGCanvas *c, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b ): CViewContainer( size ) {
      setSkin( skin, b );
      this->ms = ms;
      this->canvas = c;
      setBackgroundColor( VSTGUI::CColor( 0xFF, 0x90, 0x00 ) );
      rebuild();
   };

   enum {
      tag_segment_nodeedit_mode = 3000,
      tag_segment_movement_mode 
   } tags;
   
   void rebuild();
   virtual void valueChanged( CControl *p ) override;

   virtual void draw( CDrawContext *dc) override {
      auto r = getViewSize();
      dc->setFillColor( VSTGUI::CColor( 0xFF, 0x90, 0x00 ) );
      dc->drawRect( r, kDrawFilled );
   }
   
   MSEGStorage *ms = nullptr;
   MSEGCanvas *canvas = nullptr;
};



struct MSEGCanvas : public CControl, public Surge::UI::SkinConsumingComponent {
   MSEGCanvas(const CRect &size, LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b ): CControl( size ) {
      setSkin( skin, b );
      this->ms = ms;
      this->lfodata = lfodata;
      MSEGModulationHelper::rebuildCache( ms );
      handleBmp = b->getBitmap( IDB_MSEG_SEGMENT_HANDLES );
   };

   /*
   ** We make a list of hotzones when we draw so we don't have to recalculate the 
   ** mouse locations in drag and so on events
   */
   struct hotzone {
      CRect rect;
      int associatedSegment;
      bool active = false;
      bool dragging = false;
      bool selected = false;
      // More coming soon
      enum Type
      {
         SEGMENT_BG,
         MOUSABLE_NODE,
      } type;

      enum SegmentMousableType
      {
         SEGMENT_JOINED,
         SEGMENT_END,
         SEGMENT_START,
         SEGMENT_CONTROL
      } mousableNodeType = SEGMENT_CONTROL;
      
      std::function<void(float,float)> onDrag;
   };
   std::vector<hotzone> hotzones;

   static constexpr int drawInsertX = 10, drawInsertY = 10, axisSpace = 20;

   inline float drawDuration() {
      if( ms->n_activeSegments == 0 )
         return 1.0;
      return std::ceil( ms->totalDuration );
   }
   
   inline CRect getDrawArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.bottom -= axisSpace;
      drawArea.left += axisSpace;
      return drawArea;
   }

   inline CRect getHAxisArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.top = drawArea.bottom - axisSpace;
      drawArea.left += axisSpace;
      return drawArea;
   }
   inline CRect getVAxisArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.right = drawArea.left + axisSpace;
      drawArea.bottom -= axisSpace;
      return drawArea;
   }

   std::function<float(float)> valToPx() {
      auto drawArea = getDrawArea();
      float vscale = drawArea.getHeight();
      return [vscale, drawArea](float vp) {
                auto v = 1 - ( vp + 1 ) * 0.5;
                return v * vscale + drawArea.top;
             };
   }
   std::function<float(float)> pxToVal() {
      auto drawArea = getDrawArea();
      float vscale = drawArea.getHeight();
      return [vscale, drawArea](float vx) {
                auto v = ( vx - drawArea.top ) / vscale;
                auto vp = ( 1 - v ) * 2 - 1;
                return vp;
             };
   }
   std::function<float(float)> timeToPx() {
      auto drawArea = getDrawArea();
      float maxt = drawDuration();
      float tscale = 1.f * drawArea.getWidth() / maxt;
      return [tscale, drawArea](float t) { return t * tscale + drawArea.left; };
   }
   std::function<float(float)> pxToTime() { // INVESTIGATE
      auto drawArea = getDrawArea();
      float maxt = drawDuration();
      float tscale = 1.f * drawArea.getWidth() / maxt;

      // So px = t * tscale + drawarea;
      // So t = ( px - drawarea ) / tscale;
      return [tscale, drawArea](float px) {
                return (px - drawArea.left) / tscale;
             };
   }

   void offsetDuration( float &v, float d ) {
      v = std::max( MSEGStorage::minimumDuration, v + d );
   }
   void offsetValue( float &v, float d ) {
      v = limit_range( v + d, -1.f, 1.f );
   }

   enum TimeEdit
   {
      RIPPLE,
      CONSTRAINED,
      FIXED_TIME,
   } timeEditMode = CONSTRAINED;

   enum NodeEdit
   {
      ENDPOINTS_JOINED,
      ENDPOINTS_INDEPENDENT
   } nodeEditMode = ENDPOINTS_JOINED;
   
   void recalcHotZones( const CPoint &where ) {
      hotzones.clear();

      auto drawArea = getDrawArea();
      
      int handleRadius = 6;

      float maxt = drawDuration();
      float tscale = 1.f * drawArea.getWidth() / maxt;
      float vscale = drawArea.getHeight();
      auto valpx = valToPx();
      auto tpx = timeToPx();
         
      for( int i=0; i<ms->n_activeSegments; ++i )
      {
         auto t0 = tpx( ms->segmentStart[i] );
         auto t1 = tpx( ms->segmentEnd[i] );

         auto segrec = CRect( t0, drawArea.top, t1, drawArea.bottom );
         auto h = hotzone();
         h.rect = segrec;
         h.type = hotzone::Type::SEGMENT_BG;
         h.associatedSegment = i;
         if( segrec.pointInside( where ) )
            h.active = true;

         hotzones.push_back( h );

         // Now add the mousable zones
         auto &s = ms->segments[i];

         auto rectForPoint = [&]( float t, float v, float xmul1, float xmul2, hotzone::SegmentMousableType mt, std::function<void(float,float)> onDrag ){
                                auto h = hotzone();
                                h.rect = CRect( t + xmul1 * handleRadius,
                                                valpx(v) - handleRadius,
                                                t + xmul2 * handleRadius,
                                                valpx(v) + handleRadius );
                                h.type = hotzone::Type::MOUSABLE_NODE;
                                if( h.rect.pointInside( where ) )
                                   h.active = true;
                                h.onDrag = onDrag;
                                h.associatedSegment = i;
                                h.mousableNodeType = mt;
                                hotzones.push_back(h);
                             };

         auto timeConstraint = [&]( int prior, int next, float dx ) {
                                  switch( this->timeEditMode )
                                  {
                                  case FIXED_TIME:
                                     break;
                                  case RIPPLE:
                                     if( prior >= 0 )
                                        offsetDuration( this->ms->segments[prior].duration, dx);
                                     break;
                                  case CONSTRAINED:
                                     if( prior >= 0 && this->ms->segments[prior].duration <= MSEGStorage::minimumDuration && dx < 0 ) dx = 0;
                                     if( next < ms->n_activeSegments && this->ms->segments[next].duration <= MSEGStorage::minimumDuration && dx > 0 ) dx = 0;

                                     if( prior >= 0 )
                                        offsetDuration( this->ms->segments[prior].duration, dx );
                                     if( next < ms->n_activeSegments )
                                        offsetDuration( this->ms->segments[next].duration, -dx );
                                     
                                     break;
                                  }
                               };

           
         int offp = ( nodeEditMode == ENDPOINTS_INDEPENDENT ? 2 : 1 );
         int offm = ( nodeEditMode == ENDPOINTS_INDEPENDENT ? 0 : -1 );
         auto smt = ( nodeEditMode == ENDPOINTS_INDEPENDENT ? hotzone::SEGMENT_START : hotzone::SEGMENT_JOINED );

         switch( s.type )
         {
         case MSEGStorage::segment::CONSTANT:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, offm, offp, smt,
                          [i,this,timeConstraint,vscale,tscale](float dx, float dy) {
                             offsetValue( this->ms->segments[i].v0, -2 * dy / vscale );
                             if( this->nodeEditMode == ENDPOINTS_JOINED )
                             {
                                int prior = i-1;
                                if( prior < 0 ) prior = ms->n_activeSegments - 1;
                                if( prior >= 0 && prior != i )
                                   this->ms->segments[prior].v1 = this->ms->segments[i].v0;
                             }
                             this->ms->segments[i].v1 = this->ms->segments[i].v0;

                             if( i != 0 )
                                timeConstraint( i-1, i, dx/tscale );
                          } );

            if( nodeEditMode == ENDPOINTS_INDEPENDENT )
            {
               rectForPoint( t1, s.v0, -2, 0, hotzone::SEGMENT_END,
                             [i,this,timeConstraint,vscale,tscale](float dx, float dy) {
                                offsetValue( this->ms->segments[i].v0, -2 * dy / vscale );
                                this->ms->segments[i].v1 = this->ms->segments[i].v0;
                                timeConstraint( i, i+1, dx/tscale );
                             } );
            }
            break;
         }
         // this is the no control point case
         case MSEGStorage::segment::LINEAR:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, offm, offp, smt, 
                          [i,this,vscale,tscale, timeConstraint](float dx, float dy) {
                             offsetValue( this->ms->segments[i].v0, -2 * dy / vscale );

                             if( this->nodeEditMode == ENDPOINTS_JOINED )
                             {
                                int prior = i-1;
                                if( prior < 0 ) prior = ms->n_activeSegments - 1;
                                if( prior >= 0 && prior != i )
                                   this->ms->segments[prior].v1 = this->ms->segments[i].v0;
                             }

                             if( i != 0 )
                             {
                                timeConstraint( i-1, i, dx/tscale );
                             }
                          } );
            if( nodeEditMode == ENDPOINTS_INDEPENDENT )
               rectForPoint( t1, s.v1, -2, 0, hotzone::SEGMENT_END,
                             [i,this,vscale,tscale,timeConstraint](float dx, float dy) {
                                offsetValue( this->ms->segments[i].v1, -2 * dy / vscale );
                                timeConstraint( i, i + 1, dx/tscale );
                             } );
                          
            break;
         }
         case MSEGStorage::segment::SCURVE:
         case MSEGStorage::segment::DIGILINE:
         case MSEGStorage::segment::WAVE:
         case MSEGStorage::segment::QUADBEZ:
         case MSEGStorage::segment::BROWNIAN:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, offm, offp, smt,
                          [i,this,vscale,tscale,timeConstraint](float dx, float dy) {
                             offsetValue( this->ms->segments[i].v0,  -2 * dy / vscale );

                             if( this->nodeEditMode == ENDPOINTS_JOINED )
                             {
                                int prior = i-1;
                                if( prior < 0 ) prior = ms->n_activeSegments - 1;
                                if( prior >= 0 && prior != i )
                                   this->ms->segments[prior].v1 = this->ms->segments[i].v0;
                             }

                             if( i != 0 )
                             {
                                float cpvm = this->ms->segments[i-1].cpduration / this->ms->segments[i-1].duration;
                                float cpv0 = this->ms->segments[i].cpduration / this->ms->segments[i].duration;
                                timeConstraint( i-1, i, dx/tscale );
                                // Update control point proportionally
                                this->ms->segments[i-1].cpduration = this->ms->segments[i-1].duration * cpvm;
                                this->ms->segments[i].cpduration = this->ms->segments[i].duration * cpv0;
                             }
                          } );

            if( nodeEditMode == ENDPOINTS_INDEPENDENT )
            {
               rectForPoint( t1, s.v1, -2, 0, hotzone::SEGMENT_END,
                             [i,this,vscale,tscale,timeConstraint](float dx, float dy) {
                                offsetValue( this->ms->segments[i].v1, -2 * dy / vscale );
                                float cpv0 = this->ms->segments[i].cpduration / this->ms->segments[i].duration;
                                
                                timeConstraint( i, i+1, dx/tscale );
                                this->ms->segments[i].cpduration = cpv0 * this->ms->segments[i].duration;
                             } );
            }
            
            float tn = tpx(ms->segmentStart[i] + ms->segments[i].cpduration );
            rectForPoint( tn, s.cpv, -1, 1, hotzone::SEGMENT_CONTROL,
                          [i, this, tscale, vscale]( float dx, float dy ) {
                             offsetValue( this->ms->segments[i].cpv, -2 * dy / vscale );
                             this->ms->segments[i].cpduration += dx / tscale;
                             this->ms->segments[i].cpduration = limit_range( (float)this->ms->segments[i].cpduration,
                                                                             (float)MSEGStorage::minimumDuration,
                                                                             (float)(this->ms->segments[i].duration - MSEGStorage::minimumDuration) );
                          } );
                          
            // TODO control point
            break;
         }
         }
      }
   }

   

   inline void drawAxis( CDrawContext *dc ) {
      auto haxisArea = getHAxisArea();
      float maxt = drawDuration();
      int skips = 4;
      auto tpx = timeToPx();
            
      dc->setLineWidth( 1 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Axis::Line, CColor(220, 220, 240)));
      dc->drawLine( haxisArea.getTopLeft(), haxisArea.getTopRight() );
      for( int gi = 0; gi < maxt * skips + 1; ++gi )
      {
         float t = 1.0f * gi / skips;
         float px = tpx( t );
         float off = haxisArea.getHeight() / 2;
         if( gi % skips == 0 )
            off = 0;
         dc->drawLine( CPoint( px, haxisArea.top ), CPoint( px, haxisArea.bottom - off ) );
      }

      auto vaxisArea = getVAxisArea();
      dc->setLineWidth( 1 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Axis::Line, CColor(220, 220, 240)));
      dc->drawLine( vaxisArea.getTopRight(), vaxisArea.getBottomRight() );
      auto valpx = valToPx();
      for( float i=-1; i<=1; i += 0.25 )
      {
         float p = valpx( i );
         float off = vaxisArea.getWidth() / 2;
         if( i == -1 || i == 0 || i == 1 )
            off = 0;
         dc->drawLine( CPoint( vaxisArea.left + off, p ), CPoint( vaxisArea.right, p ) ); 
      }
   }
   
   virtual void draw( CDrawContext *dc) override {
      dc->setDrawMode(kAntiAliasing);
      auto vs = getViewSize();
      if( hotzones.empty() )
      {
         recalcHotZones( CPoint( vs.left, vs.top ) );
      }
      /* 
      dc->setFillColor( kYellowCColor );
      dc->drawRect( vs, kDrawFilled );
      */
      
      auto valpx = valToPx();
      auto tpx = timeToPx();
      auto pxt = pxToTime();

      for( const auto &h : hotzones )
      {
         if( h.active && h.type == hotzone::SEGMENT_BG )
         {
            dc->setFillColor( skin->getColor(Colors::MSEGEditor::Segment::Hover, CColor(80, 80, 100, 128)));
            dc->drawRect( h.rect, kDrawFilled );
         }
         if( h.selected && h.type == hotzone::SEGMENT_BG )
         {
            dc->setFillColor( skin->getColor(Colors::MSEGEditor::Segment::Hover, CColor(120, 120, 180)));
            dc->drawRect( h.rect, kDrawFilled );
         }
      }

      auto drawArea = getDrawArea();
      float maxt = drawDuration();

      drawAxis( dc );
      
      // draw horizontal grid
      dc->setLineWidth( 1 );
      int skips = 4;
      auto primaryGridColor = skin->getColor(Colors::MSEGEditor::Grid::Primary, CColor(220, 220, 240));
      auto secondaryGridColor = skin->getColor(Colors::MSEGEditor::Grid::Secondary, CColor(100, 100, 110));
      for( int gi = 0; gi < maxt * skips + 1; ++gi )
      {
         float t = 1.0f * gi / skips;
         float px = tpx( t );
         if( gi % skips == 0 )
            dc->setFrameColor( primaryGridColor );
         else
            dc->setFrameColor( secondaryGridColor );
         dc->drawLine( CPoint( px, drawArea.top ), CPoint( px, drawArea.bottom ) );
         // std::cout << _D(t) << _D(px) << _D(pxt(px)) << std::endl;
      }

      // draw vertical grid
      for( int vi = 0; vi < 2 * skips + 1; vi ++ )
      {
         float v = valpx( 1.f * vi / skips - 1 );
         if( vi % skips == 0 )
            dc->setFrameColor( primaryGridColor );
         else
            dc->setFrameColor( secondaryGridColor );
         dc->drawLine( CPoint( drawArea.left, v ), CPoint( drawArea.right, v ) );
      }
      

      CGraphicsPath *path = dc->createGraphicsPath();
      CGraphicsPath *looppath = dc->createGraphicsPath();
      bool startedLoopPath = 0;
      for( int i=0; i<drawArea.getWidth(); ++i )
      {
         float up = pxt( i + drawArea.left );
         float iup = (int)up;
         float fup = up - iup;
         float v = MSEGModulationHelper::valueAt( iup, fup, lfodata->deform.val.f, ms );
         v = valpx( v );
         if( up <= ms->totalDuration )
         {
            if( i == 0 )
               path->beginSubpath( i, v  );
            else
               path->addLine( i, v );
         }
         else
         {
            if( ! startedLoopPath )
            {
               startedLoopPath = true;
               looppath->beginSubpath( i, v );
            }
            else
               looppath->addLine( i, v );
         }
      }

      dc->setLineWidth( 2 );
      auto tfpath = CGraphicsTransform().translate( drawArea.left, 0 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Line, kWhiteCColor));
      dc->drawGraphicsPath( path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
      path->forget();

      if( startedLoopPath )
      {
         dc->setLineWidth(1);
         dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Loop::Line, CColor(180, 180, 200)));
         dc->drawGraphicsPath( looppath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
         looppath->forget();
      }

      dc->setLineWidth(1);
      for( const auto &h : hotzones )
      {
         if( h.type == hotzone::MOUSABLE_NODE )
         {
            int sz = 12;
            int offx = (int)h.mousableNodeType;
            int offy = 0;
            if( h.active )
            {
               offy = 1;
            }
            if( h.dragging )
            {
               offy = 2;
            }
            if( ! handleBmp )
            {
               dc->setFrameColor( kRedCColor );
               dc->drawRect( h.rect, kDrawStroked );
            }
            else
            {
               handleBmp->draw( dc, h.rect, CPoint( offx * sz, offy * sz ), 0xFF );
            }
         }
      }
   }

   CPoint mouseDownOrigin;
   bool inDrag = false;
   virtual CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons ) override {
      if( buttons & kRButton )
      {
         openPopup( where );
         return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
      }
      if( buttons & kDoubleClick )
      {
         auto tf = pxToTime( );
         auto t = tf( where.x );
         auto pv = pxToVal();
         auto v = pv( where.y );
         if( t < ms->totalDuration )
         {
            splitSegment( t, v );
         }
         else
         {
            extendTo( t, v );
         }
      }

      mouseDownOrigin = where;
      inDrag = true;
      for( auto &h : hotzones )
      {
         if( h.rect.pointInside(where) && h.type == hotzone::MOUSABLE_NODE )
         {
            h.active = true;
            h.dragging = true;
            invalid();
            break;
         }
         if( h.type == hotzone::SEGMENT_BG )
         {
            bool old = h.selected;
            h.selected = h.rect.pointInside( where );
            if( old != h.selected )
               invalid();
            //if( h.selected && segmentpanel )
            //segmentpanel->segmentChanged( h.associatedSegment );
               
         }
      }

      return kMouseEventHandled;
   }

   virtual CMouseEventResult onMouseUp(CPoint &where, const CButtonState &buttons ) override {
      inDrag = false;
      for( auto &h : hotzones )
      {
         h.dragging = false;
      }
      return kMouseEventHandled;
   }

   virtual CMouseEventResult onMouseMoved(CPoint &where, const CButtonState &buttons ) override {
      bool flip = false;
      for( auto &h : hotzones )
      {
         if( h.dragging ) continue;
         
         if( h.rect.pointInside(where) )
         {
            if( ! h.active )
            {
               h.active = true;
               flip = true;
            }
         }
         else
         {
            if( h.active )
            {
               h.active = false;
               flip = true;
            }
         }
      }

      if( flip )
         invalid();

      if( inDrag )
      {
         bool gotOne = false;
         int idx = 0;
         for( auto &h : hotzones )
         {
            if( h.dragging )
            {
               gotOne = true;
               h.onDrag( where.x - mouseDownOrigin.x, where.y - mouseDownOrigin.y );
               mouseDownOrigin = where;
               break;
            }
            idx++;
         }
         if( gotOne ) {
            MSEGModulationHelper::rebuildCache(ms);
            recalcHotZones(where);
            hotzones[idx].dragging = true;
            invalid();
         }
      }
      
      return kMouseEventHandled;
   }

   void openPopup(const VSTGUI::CPoint &iw) {
      CPoint w = iw;
      localToFrame(w);

      COptionMenu* contextMenu = new COptionMenu(CRect( w, CPoint(0,0)), 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

      auto tf = pxToTime( );
      auto t = tf( iw.x );
      contextMenu->addEntry( "[?] MSEG Segments" );
      contextMenu->addSeparator();


      auto addCb = [](COptionMenu *p, const std::string &l, std::function<void()> op ) -> CCommandMenuItem * {
                      auto m = new CCommandMenuItem( CCommandMenuItem::Desc( l.c_str() ) );
                      m->setActions([op](CCommandMenuItem* m) { op(); });
                      p->addEntry( m );
                      return m;
                   };

      auto pv = pxToVal();
      auto v = pv( iw.y );
      
      addCb( contextMenu,  "Insert Before", [this, t](){ this->insertBefore( t ); } );
      addCb( contextMenu,  "Insert After", [this, t](){ this->insertAfter( t ); } );
      addCb( contextMenu,  "Split", [this, t, v](){ this->splitSegment( t, v );} );
      addCb( contextMenu,  "Delete", [this,t ]() { this->deleteSegment( t ); } );

      contextMenu->addSeparator();

      auto typeTo = [this, contextMenu, t, addCb](std::string n, MSEGStorage::segment::Type type) {
                       addCb( contextMenu, n, [this,t, type]() { this->changeTypeAt( t, type ); } );
                    };
      typeTo( "Constant", MSEGStorage::segment::Type::CONSTANT );
      typeTo( "Linear", MSEGStorage::segment::Type::LINEAR );
      typeTo( "Quadratic Bezier", MSEGStorage::segment::Type::QUADBEZ );
      typeTo( "S-Curve", MSEGStorage::segment::Type::SCURVE );
      typeTo( "Wave", MSEGStorage::segment::Type::WAVE );
      typeTo( "Quantized Line", MSEGStorage::segment::Type::DIGILINE );
      typeTo( "Brownian Bridge", MSEGStorage::segment::Type::BROWNIAN );
            
      
      getFrame()->addView( contextMenu );
      contextMenu->setDirty();
      contextMenu->popup();
      getFrame()->removeView(contextMenu, true);
   }

   int timeToSegment( float t ) {
      if( ms->totalDuration < MSEGStorage::minimumDuration ) return -1;

      while( t > ms->totalDuration ) t -= ms->totalDuration;
      while( t < 0 ) t += ms->totalDuration;

      int idx = -1;
      for( int i=0; i<ms->n_activeSegments; ++i ) 
      {
         if( t >= ms->segmentStart[i] && t < ms->segmentEnd[i] )
         {
            idx = i;
            break;
         }
      }
      return idx;
   }

   void changeTypeAt( float t, MSEGStorage::segment::Type type ) {
      auto idx = timeToSegment( t );
      if( idx < ms->n_activeSegments )
      {
         ms->segments[idx].type = type;
      }

      modelChanged();
   }
   
   void insertAfter( float t ) {
      auto idx = timeToSegment( t );
      if( idx < 0 ) idx = 0;
      idx++;
      insertAtIndex( idx );
      modelChanged();
   }

   void insertBefore( float t ) {
      auto idx = timeToSegment( t );
      if( idx < 0 ) idx = 0;
      insertAtIndex( idx );
      modelChanged();
   }

   void extendTo( float t, float nv ) {
      if( t < ms->totalDuration ) return;

      insertAtIndex( ms->n_activeSegments );
      modelChanged();
      
      auto sn = ms->n_activeSegments - 1;
      ms->segments[sn].type = MSEGStorage::segment::LINEAR;
      if( sn == 0 )
         ms->segments[sn].v0 = 0;
      else
         ms->segments[sn].v0 = ms->segments[sn-1].v1;

      ms->segments[sn].v1 = nv;
      
      float dt = t - ms->totalDuration;
      ms->segments[sn].duration += dt;

      modelChanged();
   }
   void splitSegment( float t, float nv ) {
      int idx = timeToSegment( t );
      if( idx >= 0 )
      {
         while( t > ms->totalDuration ) t -= ms->totalDuration;
         while( t < 0 ) t += ms->totalDuration;
         
         float dt = (t - ms->segmentStart[idx]) / ( ms->segments[idx].duration);
         auto q = ms->segments[idx]; // take a copy
         insertAtIndex(idx + 1);

         if( ms->segments[idx].type == MSEGStorage::segment::CONSTANT )
            ms->segments[idx].type = MSEGStorage::segment::LINEAR;
         
         ms->segments[idx].duration *= dt;
         ms->segments[idx+1].duration = q.duration * ( 1 - dt );

         ms->segments[idx+1].v0 = nv;
         ms->segments[idx+1].v1 = q.v1;
         ms->segments[idx].v1 = nv;
         ms->segments[idx+1].type = ms->segments[idx].type;

         ms->segments[idx].cpduration = std::min( ms->segments[idx].duration, ms->segments[idx].cpduration );
         ms->segments[idx+1].cpduration = std::min( ms->segments[idx+1].duration, ms->segments[idx+1].cpduration );
         
         modelChanged();
      }
   }

   void deleteSegment( float t ) {
      auto idx = timeToSegment( t );
      for( int i=idx; i<ms->n_activeSegments - 1; ++i )
      {
         ms->segments[i] = ms->segments[i+1];
      }
      ms->n_activeSegments --;
      modelChanged();
   }
   
   void insertAtIndex( int insertIndex ) {
      for( int i=std::max( ms->n_activeSegments + 1, max_msegs - 1 ); i > insertIndex; --i )
      {
         ms->segments[i] = ms->segments[i-1];
      }
      ms->segments[insertIndex].type = MSEGStorage::segment::CONSTANT;
      ms->segments[insertIndex].v0 = -1.f * rand() / ((float)RAND_MAX);
      ms->segments[insertIndex].v1 = ms->segments[insertIndex].v0;
      ms->segments[insertIndex].duration = 0.25;
      ms->n_activeSegments ++;
   }
   
   void modelChanged() {
      MSEGModulationHelper::rebuildCache( ms );
      recalcHotZones(mouseDownOrigin); // FIXME
      invalid();
   }
   
   MSEGStorage *ms;
   LFOStorage *lfodata;
   MSEGControlRegion *controlregion = nullptr;

   CScalableBitmap *handleBmp;
   
   CLASS_METHODS( MSEGCanvas, CControl );
};

void MSEGControlRegion::valueChanged( CControl *p )
{
   auto tag = p->getTag();
   auto val = p->getValue();
   switch( tag )
   {
   case tag_segment_movement_mode:
   {
      int m = floor( val * 2 + 0.5 );
      canvas->timeEditMode = (MSEGCanvas::TimeEdit)m;
      canvas->recalcHotZones( CPoint( 0, 0 ) );
      canvas->invalid();
   }
      break;
   case tag_segment_nodeedit_mode:
   {
      if( val > 0.5 )
         canvas->nodeEditMode = MSEGCanvas::NodeEdit::ENDPOINTS_INDEPENDENT;
      else
         canvas->nodeEditMode = MSEGCanvas::NodeEdit::ENDPOINTS_JOINED;
      canvas->recalcHotZones( CPoint( 0, 0 ) );
      canvas->invalid();
   }
      break;
   }
}
void MSEGControlRegion::rebuild()
{
   auto labelFont = new VSTGUI::CFontDesc( "Lato", 12 );
   
   int height = 120;
   
   int xpos = 0;
   // sliders are the first 150 px
   int sliderWidth = 160;
   auto sliders = new CTextLabel( CRect( CPoint( xpos, 0 ), CPoint( sliderWidth, height ) ), "Sliders Go Here soon" );
   sliders->setFont( labelFont );
   sliders->setFontColor( kWhiteCColor );
   sliders->setBackColor( VSTGUI::CColor( 0xAA, 0xAA, 0xFF ) );
   addView( sliders );
   xpos += sliderWidth;
   
   // Envelope and Loop next for 130 px
   int envWidth = 140;
   auto envAndLoop = new CTextLabel( CRect( CPoint( xpos, 0 ), CPoint( envWidth, height ) ), "Envelope/Loop" );
   envAndLoop->setFont( labelFont );
   envAndLoop->setFontColor( kWhiteCColor );
   envAndLoop->setBackColor( VSTGUI::CColor( 0xAA, 0xFF, 0xAA ) );
   addView( envAndLoop );
   xpos += envWidth;
   
   // Now the segment controls which are 130 wide
   {
      int segWidth = 140;
      int margin = 5;
      int marginPos = xpos + margin;
      int marginWidth = segWidth - 2 * margin;
      int ypos = margin;
      auto sml = new CTextLabel( CRect( CPoint( marginPos, ypos ), CPoint( marginWidth, 20 ) ), "Segment editing mode" );
      sml->setTransparency( true );
      sml->setFont( labelFont );
      sml->setFontColor( kBlackCColor );
      sml->setHoriAlign( kLeftText );
      addView( sml );
      ypos += margin + 20;
      
      // Now the button
      auto sw = new CHSwitch2( CRect( CPoint( marginPos, ypos ), CPoint( 120, 20 ) ),
                               this, tag_segment_nodeedit_mode,
                               2, 20, 1, 2,
                               associatedBitmapStore->getBitmap( IDB_MSEG_EDITMODE ), CPoint( 0, 0 ), true );
      addView( sw );
      sw->setValue( canvas->nodeEditMode );
      ypos += margin + 20;
      
      // Now the movement label
      auto mml = new CTextLabel( CRect( CPoint( marginPos, ypos ), CPoint( marginWidth, 20 ) ), "Movement mode" );
      mml->setTransparency( true );
      mml->setFont( labelFont );
      mml->setFontColor( kBlackCColor );
      mml->setHoriAlign( kLeftText );
      addView( mml );
      ypos += margin + 20;
      
      // now the button
      auto mw = new CHSwitch2( CRect( CPoint( marginPos, ypos ), CPoint( 120, 20 ) ),
                               this, tag_segment_movement_mode,
                               3, 20, 1, 3,
                               associatedBitmapStore->getBitmap( IDB_MSEG_MOVEMENT ), CPoint( 0, 0 ), true );
      addView( mw );
      mw->setValue( canvas->timeEditMode / 2.f );
      
      ypos += margin + 20;
      
      xpos += segWidth;
   }
   
   
   // and finally the snap
   int snapWidth = 140;
   auto snapC = new CTextLabel( CRect( CPoint( xpos, 0 ), CPoint( snapWidth, height ) ), "Snap" );
   snapC->setFont( labelFont );
   snapC->setFontColor( kWhiteCColor );
   snapC->setBackColor( VSTGUI::CColor( 0xAA, 0xAA, 0xFF ) );
   addView( snapC );
   xpos += snapWidth;
}

#if 0
void MSEGSegmentPanel::valueChanged(CControl *c) {
   auto tag = c->getTag();

   int addAfterSeg = currSeg;
   
   switch(tag )
   {
   case seg_type_0:
   case seg_type_0+1:
   case seg_type_0+2:
   case seg_type_0+3:
   case seg_type_0+4:
   case seg_type_0+5:
   case seg_type_0+6:
   case seg_type_0+7:
   {
      if( currSeg >= 0 && canvas && c->getValue() == 1 )
      {
         ms->segments[currSeg].type = (MSEGStorage::segment::Type)(tag - seg_type_0);
         canvas->modelChanged();
      }
      break;
   }
   case add_after:
   {
      addAfterSeg ++;
   }
   case add_before:
   {
      if( addAfterSeg < 0 ) addAfterSeg = 0;
      if( c->getValue() == 1 )
      {
         for( int i=std::max( ms->n_activeSegments + 1, max_msegs - 1 ); i > addAfterSeg; --i )
         {
            ms->segments[i] = ms->segments[i-1];
         }
         ms->segments[addAfterSeg].type = MSEGStorage::segment::LINEAR;
         ms->segments[addAfterSeg].v0 = -0.5;
         ms->segments[addAfterSeg].v1 = 0.5;
         ms->segments[addAfterSeg].duration = 0.25;
         ms->n_activeSegments ++;
         
         if( canvas ) canvas->modelChanged();
      }
      break;
   }
   case deletenode:
   {
      if( c->getValue() == 1 )
      {
         if( currSeg >= 0 && ms->n_activeSegments > 0 )
         {
            for( int i = currSeg + 1; i < ms->n_activeSegments; ++i )
               ms->segments[i-1] = ms->segments[i];
            ms->n_activeSegments --;
            if( canvas ) canvas->modelChanged();
         }
      }
   }
   }
   
}

void MSEGControlPanel::valueChanged( CControl *control ) {
   if( control->getValue() == 1 )
   {
      if( control->getTag() == mag_only ||
          control->getTag() == time_only ||
          control->getTag() == mag_and_time )
      {
         editmode = control->getTag();
         if( canvas )
            canvas->modelChanged();
      }
   }
}
#endif

struct MSEGMainEd : public CViewContainer {

   MSEGMainEd(const CRect &size, LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> bmp) : CViewContainer(size) {
      this->ms = ms;
      this->skin = skin;

      int controlHeight = 120;
      auto msegCanv = new MSEGCanvas( CRect( CPoint( 0, 0 ), CPoint( size.getWidth(), size.getHeight() - controlHeight ) ), lfodata, ms, skin, bmp );
            
      auto msegControl = new MSEGControlRegion(CRect( CPoint( 0, size.getHeight() - controlHeight ), CPoint( size.getWidth(), controlHeight ) ), msegCanv,
                                               ms, skin, bmp );

      msegCanv->controlregion = msegControl;
      msegControl->canvas = msegCanv;
      
      addView( msegCanv );
      addView( msegControl );
   }

   Surge::UI::Skin::ptr_t skin;
   MSEGStorage *ms;

};

MSEGEditor::MSEGEditor(LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b) : CViewContainer( CRect( 0, 0, 720, 475 ) )
{
   setSkin( skin, b );
   setBackgroundColor( kRedCColor );
   addView( new MSEGMainEd( getViewSize(), lfodata, ms, skin, b ) );
}
