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
#include "CSwitchControl.h"
#include "SurgeGUIEditor.h"

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
   MSEGControlRegion(const CRect &size, MSEGCanvas *c, LFOStorage *lfos, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b ): CViewContainer( size ) {
      setSkin( skin, b );
      this->ms = ms;
      this->lfodata = lfos;
      this->canvas = c;
      setBackgroundColor( VSTGUI::CColor( 0xFF, 0x90, 0x00 ) );
      rebuild();
   };

   enum {
      tag_segment_nodeedit_mode = 300000,
      tag_segment_movement_mode,
      tag_vertical_snap,
      tag_vertical_value,
      tag_horizontal_snap,
      tag_horizontal_value,
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
   LFOStorage *lfodata = nullptr;
};



struct MSEGCanvas : public CControl, public Surge::UI::SkinConsumingComponent {
   MSEGCanvas(const CRect &size, LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b ): CControl( size ) {
      setSkin( skin, b );
      this->ms = ms;
      this->lfodata = lfodata;
      Surge::MSEG::rebuildCache( ms );
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
      
      // More coming soon
      enum Type
      {
         MOUSABLE_NODE,
      } type;

      enum SegmentMousableType
      {
         SEGMENT_ENDPOINT,
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
   void adjustDuration( int idx, float d, float snapResolution, float upperBound)
   {
      if( snapResolution <= 0 )
      {
         offsetDuration(ms->segments[idx].duration, d);
      }
      else
      {
         offsetDuration( ms->segments[idx].dragDuration, d );
         auto target = (float)(round( ( ms->segmentStart[idx] + ms->segments[idx].dragDuration ) / snapResolution ) * snapResolution ) - ms->segmentStart[idx];
         if( upperBound > 0 && target > upperBound )
            target = ms->segments[idx].duration;
         if( target < MSEGStorage::minimumDuration )
            target = ms->segments[idx].duration;
         ms->segments[idx].duration = target;
      }
   }
   void offsetValue( float &v, float d ) {
      v = limit_range( v + d, -1.f, 1.f );
   }
   void adjustValue( int idx, bool cpvNotV0, float d, float snapResolution )
   {
      if( cpvNotV0 )
      {
         offsetValue( ms->segments[idx].dragcpv, d );
      }
      else
      {
         offsetValue( ms->segments[idx].dragv0, d );
         if( snapResolution <= 0 )
            ms->segments[idx].v0 = ms->segments[idx].dragv0;
         else
         {
            float q = ms->segments[idx].dragv0 + 1;
            float pos = round( q / snapResolution ) * snapResolution;
            float adj = pos - 1.0;
            ms->segments[idx].v0 = limit_range(adj, -1.f, 1.f );
         }
      }
   }

   enum TimeEdit
   {
      RIPPLE,
      CONSTRAINED,
      DRAW_MODE,
   } timeEditMode = CONSTRAINED;

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

         // Now add the mousable zones
         auto &s = ms->segments[i];
         auto rectForPoint = [&]( float t, float v, hotzone::SegmentMousableType mt, std::function<void(float,float)> onDrag ){
                                auto h = hotzone();
                                h.rect = CRect( t - handleRadius,
                                                valpx(v) - handleRadius,
                                                t + handleRadius,
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
                                  case DRAW_MODE:
                                     break;
                                  case RIPPLE:
                                     if( prior >= 0 )
                                     {
                                        auto rcv = this->ms->segments[prior].cpduration / this->ms->segments[prior].duration;
                                        adjustDuration( prior, dx, ms->hSnap, 0);
                                        this->ms->segments[prior].cpduration = this->ms->segments[prior].duration * rcv;
                                     }
                                     break;
                                  case CONSTRAINED:
                                     if( prior >= 0 && (this->ms->segments[prior].duration+dx) <= MSEGStorage::minimumDuration && dx < 0 ) dx = 0;
                                     if( next < ms->n_activeSegments && (this->ms->segments[next].duration-dx) <= MSEGStorage::minimumDuration && dx > 0 ) dx = 0;

                                     auto csum = 0.f, pd = 0.f;
                                     if( prior >= 0 ) {
                                        csum += ms->segments[prior].duration;
                                        pd = ms->segments[prior].duration;
                                     }
                                     if( next < ms->n_activeSegments ) csum += ms->segments[next].duration;
                                     if( prior >= 0 )
                                     {
                                        auto rcv = this->ms->segments[prior].cpduration / this->ms->segments[prior].duration;
                                        adjustDuration( prior, dx, ms->hSnap, csum);
                                        this->ms->segments[prior].cpduration = this->ms->segments[prior].duration * rcv;
                                        pd = ms->segments[prior].duration;
                                     }
                                     if( next < ms->n_activeSegments )
                                     {
                                        auto rcv = this->ms->segments[next].cpduration / this->ms->segments[next].duration;

                                        // offsetDuration( this->ms->segments[next].duration, -dx );
                                        this->ms->segments[next].duration = csum - pd;
                                        this->ms->segments[next].cpduration = this->ms->segments[next].duration * rcv;
                                     }
                                     
                                     break;
                                  }
                               };


         // We get a mousable point at the start of the line
         rectForPoint( t0, s.v0, hotzone::SEGMENT_ENDPOINT,
                       [i,this,vscale,tscale,timeConstraint](float dx, float dy) {
                          adjustValue(i, false, -2 * dy / vscale, ms->vSnap );

                          if( i != 0 )
                          {
                             timeConstraint( i-1, i, dx/tscale );
                          }
                       } );


         // Control point editor
         float tn = tpx(ms->segmentStart[i] + ms->segments[i].cpduration );
         rectForPoint( tn, s.cpv, hotzone::SEGMENT_CONTROL,
                       [i, this, tscale, vscale]( float dx, float dy ) {
                          adjustValue(i, true, -2 * dy / vscale, 0.0 ); // we never want to snap CPV

                          this->ms->segments[i].cpduration += dx / tscale;
                          this->ms->segments[i].cpduration = limit_range( (float)this->ms->segments[i].cpduration,
                                                                          (float)MSEGStorage::minimumDuration,
                                                                          (float)(this->ms->segments[i].duration - MSEGStorage::minimumDuration) );
                       } );

         // Specially we have to add an endpoint editor
         if( i == ms->n_activeSegments - 1 )
         {
            rectForPoint( tpx(ms->totalDuration), ms->segments[0].v0, hotzone::SEGMENT_ENDPOINT,
                          [this,vscale,tscale](float dx, float dy) {
                             adjustValue( 0, false, -2 * dy / vscale, ms->vSnap );
                             // We need to deal with the cpduration also
                             auto cpv = this->ms->segments[ms->n_activeSegments-1].cpduration / this->ms->segments[ms->n_activeSegments-1].duration;

                             adjustDuration(ms->n_activeSegments-1, dx/tscale, ms->hSnap, 0 );
                             this->ms->segments[ms->n_activeSegments-1].cpduration = this->ms->segments[ms->n_activeSegments-1].duration * cpv;
                          } );
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

      auto drawArea = getDrawArea();
      float maxt = drawDuration();
      float msegEvaluationState[5];
      int lseval = -1;

      CGraphicsPath *path = dc->createGraphicsPath();
      CGraphicsPath *fillpath = dc->createGraphicsPath();
      float pathFirstY, pathLastX, pathLastY, minpx = 1000000;
      for( int i=0; i<drawArea.getWidth(); ++i )
      {
         float up = pxt( i + drawArea.left );
         float iup = (int)up;
         float fup = up - iup;
         float v = Surge::MSEG::valueAt( iup, fup, 0, ms, lseval, msegEvaluationState );
         v = valpx( v );
         if( up <= ms->totalDuration )
         {
            if( i == 0 )
            {
               path->beginSubpath( i, v  );
               fillpath->beginSubpath( i, v );
               pathFirstY = v;
            }
            else
            {
               path->addLine( i, v );
               fillpath->addLine( i, v );
            }
            pathLastX = i; pathLastY = v;
            minpx = std::min( v, minpx );
         }
      }

      auto tfpath = CGraphicsTransform().translate( drawArea.left, 0 );

      VSTGUI::CGradient::ColorStopMap csm;
      VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);
      cg->addColorStop(0, CColor( 0xFF, 0x90, 0x00, 0x80 ) );
      cg->addColorStop(1, CColor( 0xFF, 0x90, 0x00, 0x10 ) );

      fillpath->addLine( pathLastX, valpx( -1 ) );
      fillpath->addLine( 0, valpx( -1 ) );
      fillpath->addLine( 0, pathFirstY );
      dc->fillLinearGradient( fillpath, *cg, CPoint( 0, minpx ), CPoint( 0, valpx( -1 ) ), false, &tfpath );
      fillpath->forget();
      cg->forget();

      // Draw the axis here after the gradient fill
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
      

      
      dc->setLineWidth( 2 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Line, kWhiteCColor));
      dc->drawGraphicsPath( path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

      
      path->forget();

      dc->setLineWidth(1);
      for( const auto &h : hotzones )
      {
         if( h.type == hotzone::MOUSABLE_NODE )
         {
            int sz = 12;
            int offx = 0;
            if( h.mousableNodeType == hotzone::SEGMENT_CONTROL )
               offx = 3;
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
   bool inDrawDrag = false;
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

         // Check if I'm on a hotzone
         for( auto &h : hotzones )
         {
            if( h.rect.pointInside(where) && h.type == hotzone::MOUSABLE_NODE )
            {
               switch( h.mousableNodeType )
               {
               case hotzone::SEGMENT_ENDPOINT:
               {
                  Surge::MSEG::unsplitSegment( ms, t );
                  modelChanged();
                  return kMouseEventHandled;
               }
               case hotzone::SEGMENT_CONTROL:
               {
                  // Reset the controlpoint to duration half value middle
                  Surge::MSEG::resetControlPoint( ms, t );
                  modelChanged();
                  return kMouseEventHandled;
               }
               }
            }
         }
         
         if( t < ms->totalDuration )
         {
            Surge::MSEG::splitSegment( ms, t, v );
            modelChanged();
            return kMouseEventHandled;
         }
         else
         {
            Surge::MSEG::extendTo( ms, t, v );
            modelChanged();
            return kMouseEventHandled;
         }
      }

      for( auto &s : ms->segments )
      {
         s.dragv0 = s.v0;
         s.dragcpv = s.cpv;
         s.dragDuration = s.duration;
      }

      if( timeEditMode == DRAW_MODE )
      {
         inDrawDrag = true;
         return kMouseEventHandled;
      }
      else
      {
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
         }

         return kMouseEventHandled;
      }
   }

   virtual CMouseEventResult onMouseUp(CPoint &where, const CButtonState &buttons ) override {
      inDrag = false;
      inDrawDrag = false;
      for( auto &h : hotzones )
      {
         h.dragging = false;
      }
      return kMouseEventHandled;
   }

   virtual CMouseEventResult onMouseMoved(CPoint &where, const CButtonState &buttons ) override {
      if( inDrawDrag )
      {
         auto tf = pxToTime( );
         auto t = tf( where.x );
         auto pv = pxToVal();
         auto v = limit_range( pv( where.y ), -1.f, 1.f );

         int seg = Surge::MSEG::timeToSegment( this->ms, t );

         if( seg >= 0 && seg < ms->n_activeSegments && t <= ms->totalDuration + MSEGStorage::minimumDuration )
         {
            bool ch = false;
            // OK are we near the endpoint of that segment
            if( t - ms->segmentStart[seg] < std::max( ms->totalDuration * 0.05, 0.1 * ms->segments[seg].duration ) )
            {
               ms->segments[seg].v0 = v;
               ch = true;
            }
            else if( ms->segmentEnd[seg] - t < std::max( ms->totalDuration * 0.05, 0.1 * ms->segments[seg].duration ) )
            {
               int nx = seg + 1;
               if( nx >= ms->n_activeSegments )
                  nx = 0;
               ms->segments[nx].v0 = v;
               ch = true;
            }
            if( ch )
               modelChanged();
         }

         
         return kMouseEventHandled;
      }

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
               modelChanged(); // HACK FIXME
               mouseDownOrigin = where;
               break;
            }
            idx++;
         }
         if( gotOne ) {
            Surge::MSEG::rebuildCache(ms);
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
      
      addCb( contextMenu,  "Split", [this, t, v](){
                                       Surge::MSEG::splitSegment( this->ms, t, v );
                                       modelChanged();
                                    } );
      addCb( contextMenu,  "Delete", [this,t ]() {
                                        Surge::MSEG::deleteSegment( this->ms, t );
                                        modelChanged();
                                     } );

      contextMenu->addSeparator();

      auto typeTo = [this, contextMenu, t, addCb](std::string n, MSEGStorage::segment::Type type) {
                       addCb( contextMenu, n, [this,t, type]() {
                                                 Surge::MSEG::changeTypeAt( this->ms, t, type );
                                                 modelChanged();
                                              } );
                    };
      typeTo( "Linear", MSEGStorage::segment::Type::LINEAR );
      typeTo( "Quadratic Bezier", MSEGStorage::segment::Type::QUADBEZ );
      typeTo( "S-Curve", MSEGStorage::segment::Type::SCURVE );
      typeTo( "Sin Wave", MSEGStorage::segment::Type::SINWAVE );
      typeTo( "Square Wave", MSEGStorage::segment::Type::SQUAREWAVE );
      typeTo( "Quantized Line", MSEGStorage::segment::Type::DIGILINE );
      typeTo( "Brownian Bridge", MSEGStorage::segment::Type::BROWNIAN );
            
      
      getFrame()->addView( contextMenu );
      contextMenu->setDirty();
      contextMenu->popup();
      getFrame()->removeView(contextMenu, true);
   }

   void modelChanged() {
      Surge::MSEG::rebuildCache( ms );
      recalcHotZones(mouseDownOrigin); // FIXME
      // Do this more heavy handed version
      getFrame()->invalid();
      // invalid();
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
   case tag_segment_movement_mode: {
      int m = floor(val * 2 + 0.5);
      canvas->timeEditMode = (MSEGCanvas::TimeEdit)m;
      canvas->recalcHotZones(CPoint(0, 0));
      canvas->invalid();
   }
   break;
   case tag_horizontal_snap:
      if( val < 0.5 ) ms->hSnap = 0; else ms->hSnap = 0.05;
      break;
   case tag_vertical_snap:
      if( val < 0.5 ) ms->vSnap = 0; else ms->vSnap = ms->vSnapDefault;
      break;
   case tag_vertical_value:
   {
      // FIXME locales
      auto fv = 2.f / ( std::max( std::atoi( ((CTextEdit *)p)->getText() ), 1 )  ); // -1 to 1 remeber
      ms->vSnapDefault = fv;
      if( ms->vSnap > 0 )
         ms->vSnap = ms->vSnapDefault;
   }
      break;
   case tag_horizontal_value:
   {
      auto fv = 1.f / ( std::max( std::atoi( ((CTextEdit *)p)->getText() ), 1 ) );
      ms->hSnapDefault = fv;
      if( ms->hSnap > 0 )
         ms->hSnap = ms->hSnapDefault;
   }
      break;
   default:
      break;
   }
}
void MSEGControlRegion::rebuild()
{
   auto labelFont = new VSTGUI::CFontDesc( "Lato", 12 );
   
   int height = getViewSize().getHeight();
   int margin = 2;
   int labelHeight = 18;
   int controlHeight = 20;
   
   int xpos = 0;
   // sliders are the first 150 px
   {
      int sliderWidth = 150;
      int margin = 5;
      int marginPos = xpos + margin;
      int ypos = margin;

      auto envAndLoop = new CTextLabel( CRect( CPoint( marginPos, ypos ), CPoint( sliderWidth, height - ypos ) ), "Loop" );
      envAndLoop->setFont( labelFont );
      envAndLoop->setFontColor( kWhiteCColor );
      envAndLoop->setTransparency( true );
      addView( envAndLoop );
      xpos += sliderWidth;
   }
   
   // Now the segment controls which are 130 wide
   {
      int segWidth = 140;

      int marginPos = xpos + margin;
      int marginWidth = segWidth - 2 * margin;
      int ypos = margin;

      // Now the movement label
      auto mml = new CTextLabel(CRect(CPoint(marginPos, ypos), CPoint(marginWidth, labelHeight)),
                                "Movement mode");
      mml->setTransparency(true);
      mml->setFont(labelFont);
      mml->setFontColor(kBlackCColor);
      mml->setHoriAlign(kLeftText);
      addView(mml);
      ypos += margin + labelHeight;

      // now the button
      auto mw = new CHSwitch2(
          CRect(CPoint(marginPos, ypos), CPoint(120, controlHeight)), this, tag_segment_movement_mode, 3, 20,
          1, 3, associatedBitmapStore->getBitmap(IDB_MSEG_MOVEMENT), CPoint(0, 0), true);
      addView(mw);
      mw->setValue(canvas->timeEditMode / 2.f);

      ypos += margin + controlHeight;
      xpos += segWidth;
   }

   // Snap Section
   {
      int segWidth = 140;
      int margin = 2;
      int marginPos = xpos + margin;
      int marginWidth = segWidth - 2 * margin;
      int ypos = margin;

      auto snapC = new CTextLabel( CRect( CPoint( xpos, ypos ), CPoint( segWidth, labelHeight) ), "Snap to Grid" );
      snapC->setFont( labelFont );
      snapC->setFontColor( kBlackCColor );
      snapC->setHoriAlign(kLeftText);
      snapC->setTransparency( true );
      addView( snapC );

      ypos += margin + labelHeight;

      auto vbut = new CSwitchControl(CRect( CPoint( xpos, ypos ), CPoint( 80, controlHeight)), this, tag_vertical_snap, associatedBitmapStore->getBitmap(IDB_MSEG_VERTICAL_SNAP));
      addView( vbut );
      vbut->setValue( ms->vSnap < 0.001? 0 : 1 );

      char svt[256];
      snprintf(svt, 255, "%5d", (int)round( 2.f / ms->vSnapDefault ));
      auto vtxt = new CTextEdit( CRect( CPoint( xpos + 80 + margin , ypos ), CPoint( 70, controlHeight ) ), this, tag_vertical_value, svt );
      vtxt->setFontColor(kBlackCColor);
      vtxt->setFrameColor(kBlackCColor);
      vtxt->setBackColor(kWhiteCColor);
      addView( vtxt );

      ypos += margin + controlHeight;

      auto hbut = new CSwitchControl(CRect( CPoint( xpos, ypos ), CPoint( 80, controlHeight)), this, tag_horizontal_snap, associatedBitmapStore->getBitmap(IDB_MSEG_HORIZONTAL_SNAP));
      addView( hbut );
      hbut->setValue( ms->hSnap  < 0.001? 0 : 1 );
      snprintf(svt, 255, "%5d", (int)round( 1.f / ms->hSnapDefault ));
      auto htxt = new CTextEdit( CRect( CPoint( xpos + 80 + margin , ypos ), CPoint( 70, controlHeight ) ), this, tag_horizontal_value, svt );
      htxt->setFontColor(kBlackCColor);
      htxt->setFrameColor(kBlackCColor);
      htxt->setBackColor(kWhiteCColor);
      addView( htxt );

      xpos += segWidth;
   }
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

      int controlHeight = 85;
      auto msegCanv = new MSEGCanvas( CRect( CPoint( 0, 0 ), CPoint( size.getWidth(), size.getHeight() - controlHeight ) ), lfodata, ms, skin, bmp );
            
      auto msegControl = new MSEGControlRegion(CRect( CPoint( 0, size.getHeight() - controlHeight ), CPoint(  size.getWidth(), controlHeight ) ), msegCanv,
                                               lfodata, ms, skin, bmp );


      msegCanv->controlregion = msegControl;
      msegControl->canvas = msegCanv;
      
      addView( msegCanv );
      addView( msegControl );
   }

   Surge::UI::Skin::ptr_t skin;
   MSEGStorage *ms;

};

MSEGEditor::MSEGEditor(LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b) : CViewContainer( CRect( 0, 0, 760, 405 ) )
{
   // Leave these in for now
   std::cout << "MSEG Editor Constructed" << std::endl;
   setSkin( skin, b );
   setBackgroundColor( kRedCColor );
   addView( new MSEGMainEd( getViewSize(), lfodata, ms, skin, b ) );
}

MSEGEditor::~MSEGEditor() {
   std::cout << "MSEG Editor Destroyed" << std::endl;
}
