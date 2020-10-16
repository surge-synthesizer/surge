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
#include "guihelpers.h"
#include "DebugHelpers.h"
#include "SkinColors.h"
#include "basic_dsp.h" // for limit_range
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "CHSwitch2.h"
#include "CSwitchControl.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"

/*
** MSEG SVG IDs
**
** 301 - IDB_MSEG_SEGMENT_HANDLES 24x36, 3 rows (normal, hover, pressed) x 2 columns (segment node, control point) for the MSEG handles
** 302 - IDB_MSEG_MOVEMENT 120x60 3 rows (ripple, bound, draw) x 3 columns (the states)
** 303 - IDB_MSEG_VERTICAL_SNAP 80x40 2 rows (off, on)
** 304 - IDB_MSEG_HORIZONTAL_SNAP 80x40 2 rows (off, on)
** 305 - IDB_MSEG_LOOP_MODES 120x60 3 rows (off, on, gate) x 3 columns (the states)
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
      setBackgroundColor(skin->getColor(Colors::MSEGEditor::Panel));
      rebuild();
   };

   enum {
      tag_segment_nodeedit_mode = 300000,
      tag_segment_movement_mode,
      tag_vertical_snap,
      tag_vertical_value,
      tag_horizontal_snap,
      tag_horizontal_value,
      tag_loop_mode,
   } tags;
   
   void rebuild();
   virtual void valueChanged( CControl *p ) override;

   virtual void draw( CDrawContext *dc) override {
      auto r = getViewSize();
      dc->setFillColor(skin->getColor(Colors::MSEGEditor::Panel));
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
      
      std::function<void(float,float, const CPoint &)> onDrag;
   };
   std::vector<hotzone> hotzones;

   static constexpr int drawInsertX = 10, drawInsertY = 10, axisSpaceX = 18, axisSpaceY = 8;

   inline float drawDuration() {
      if( ms->n_activeSegments == 0 )
         return 1.0;
      return std::max( 1.0f, ms->totalDuration );
   }
   
   inline CRect getDrawArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.bottom -= axisSpaceY;
      drawArea.left += axisSpaceX;
      drawArea.top += 5;
      return drawArea;
   }

   inline CRect getHAxisArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.top = drawArea.bottom - axisSpaceY;
      drawArea.left += axisSpaceX;
      return drawArea;
   }
   inline CRect getVAxisArea() {
      auto vs = getViewSize();
      auto drawArea = vs.inset( drawInsertX, drawInsertY );
      drawArea.right = drawArea.left + axisSpaceX;
      drawArea.bottom -= axisSpaceY;
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
         offsetValue( ms->segments[idx].cpv, d );
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

   /*
    * This little struct acts as a SmapGuard so that shift drags can reset snap and
    * unreset snap
    */
   struct SnapGuard {
      SnapGuard( MSEGStorage *ms, MSEGCanvas *c ) : ms(ms), c(c) {
         hSnapO = ms->hSnap;
         vSnapO = ms->vSnap;
         c->invalid();
      }
      ~SnapGuard() {
         ms->hSnap = hSnapO;
         ms->vSnap = vSnapO;
         c->invalid();
      }
      MSEGStorage *ms;
      MSEGCanvas *c;
      float hSnapO, vSnapO;
   };
   std::shared_ptr<SnapGuard> snapGuard;


   enum TimeEdit
   {
      SINGLE,   // movement bound between two neighboring nodes
      SHIFT,    // shifts all following nodes along, relatively
      DRAW,     // only change amplitude of nodes as the cursor passes along the timeline
   } timeEditMode = SHIFT;

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
         auto rectForPoint = [&]( float t, float v, hotzone::SegmentMousableType mt, std::function<void(float,float, const CPoint &)> onDrag ){
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
                                  case DRAW:
                                     break;
                                  case SHIFT:
                                     if( prior >= 0 )
                                     {
                                        auto rcv = this->ms->segments[prior].cpduration / this->ms->segments[prior].duration;
                                        adjustDuration( prior, dx, ms->hSnap, 0);
                                        this->ms->segments[prior].cpduration = this->ms->segments[prior].duration * rcv;
                                     }
                                     break;
                                  case SINGLE:
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
                       [i,this,vscale,tscale,timeConstraint](float dx, float dy, const CPoint &where) {
                          adjustValue(i, false, -2 * dy / vscale, ms->vSnap );

                          if( i != 0 )
                          {
                             timeConstraint( i-1, i, dx/tscale );
                          }
                       } );


         // Control point editor
         float tn = tpx(ms->segmentStart[i] + ms->segments[i].cpduration );
         rectForPoint( tn, s.cpv, hotzone::SEGMENT_CONTROL,
                       [i, this, tscale, vscale]( float dx, float dy, const CPoint &where) {
                          adjustValue(i, true, -2 * dy / vscale, 0.0 ); // we never want to snap CPV

                          this->ms->segments[i].cpduration += dx / tscale;
                          this->ms->segments[i].cpduration = limit_range( (float)this->ms->segments[i].cpduration,
                                                                          (float)MSEGStorage::minimumDuration,
                                                                          (float)(this->ms->segments[i].duration - MSEGStorage::minimumDuration) );
                       } );

         // Specially we have to add an endpoint editor
         if( i == ms->n_activeSegments - 1 )
         {
            rectForPoint( tpx(ms->totalDuration), ms->segments[ms->n_activeSegments-1].nv1, /* which is [0].v0 in lock mode only */
                         hotzone::SEGMENT_ENDPOINT,
                          [this,vscale,tscale](float dx, float dy, const CPoint &where) {
                             if( ms->endpointMode == MSEGStorage::EndpointMode::FREE )
                             {
                                float d = -2 * dy / vscale;
                                float snapResolution = ms->vSnap;
                                int idx = ms->n_activeSegments - 1;
                                offsetValue( ms->segments[idx].dragv1, d );
                                if( snapResolution <= 0 )
                                   ms->segments[idx].nv1 = ms->segments[idx].dragv1;
                                else
                                {
                                   float q = ms->segments[idx].dragv1+ 1;
                                   float pos = round( q / snapResolution ) * snapResolution;
                                   float adj = pos - 1.0;
                                   ms->segments[idx].nv1= limit_range(adj, -1.f, 1.f );
                                }
                             }
                             else
                             {
                                adjustValue(0, false, -2 * dy / vscale, ms->vSnap);
                             }
                             // We need to deal with the cpduration also
                             auto cpv = this->ms->segments[ms->n_activeSegments-1].cpduration / this->ms->segments[ms->n_activeSegments-1].duration;

                             if( !this->getViewSize().pointInside(where))
                             {
                                auto howFar = where.x - this->getViewSize().right;
                                if( howFar > 0 )
                                   dx *= howFar * 0.1; // this is really just a speedup as our axes shrinks. Just a fudge
                             }

                             adjustDuration(ms->n_activeSegments-1, dx/tscale, ms->hSnap, 0 );
                             this->ms->segments[ms->n_activeSegments-1].cpduration = this->ms->segments[ms->n_activeSegments-1].duration * cpv;
                          } );
         }
      }
                
   }

   inline void drawAxis( CDrawContext *dc ) {
      auto haxisArea = getHAxisArea();
      float maxt = drawDuration();
      int skips = (ms->hSnap > 0 ? 1/ms->hSnap : 4 );
      auto tpx = timeToPx();

      dc->setFont( displayFont );
      dc->setFontColor(skin->getColor(Colors::MSEGEditor::Axis::Text));
      dc->setLineWidth( 1 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Axis::Line));
      dc->drawLine( haxisArea.getTopLeft(), haxisArea.getTopRight() );
      for( int gi = 0; gi < maxt * skips + 1; ++gi )
      {
         float t = 1.0f * gi / skips;
         float px = tpx( t );
         float off = haxisArea.getHeight() / 2;
         if( gi % skips == 0 )
            off = 0;
         dc->drawLine( CPoint( px, haxisArea.top ), CPoint( px, haxisArea.bottom - off ) );
         char txt[16];
         snprintf( txt, 16, "%5.2f", t );
         int xoff =0;
         if( maxt < 1.1 && t > 0.99 )
            xoff = -25;
         dc->drawString( txt, CRect( CPoint( px + 4 + xoff, haxisArea.top + 2), CPoint( 15, 10 )));
      }

      auto vaxisArea = getVAxisArea();
      dc->setLineWidth( 1 );
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Axis::Line));
      dc->drawLine( vaxisArea.getTopRight(), vaxisArea.getBottomRight() );
      auto valpx = valToPx();
      float step = (ms->vSnap > 0 ? ms->vSnap : 0.25 );
      for( float i=-1; i<=1.001 /* for things like "3" rounding*/; i += step )
      {
         float p = valpx( i );
         float off = vaxisArea.getWidth() / 2;
         if( i == -1 || i == 0 || i == 1 )
            off = 0;
         dc->drawLine( CPoint( vaxisArea.left + off, p ), CPoint( vaxisArea.right, p ) );
         char txt[16];
         snprintf( txt, 16, "%4.2f", i );
         dc->drawString(txt, CRect( CPoint( vaxisArea.left, p - 10 ), CPoint( 10, 10 )));
      }
   }
   
   virtual void draw( CDrawContext *dc) override {
      auto vs = getViewSize();

      if (hotzones.empty())
         recalcHotZones( CPoint( vs.left, vs.top ) );

      dc->setFillColor(skin->getColor(Colors::MSEGEditor::Background));
      dc->drawRect( vs, kDrawFilled );
      
      // we want to draw the background rectangle always filling the area without smearing
      // so draw the rect first then set AA drawing mode
      dc->setDrawMode(kAntiAliasing);
      
      auto valpx = valToPx();
      auto tpx = timeToPx();
      auto pxt = pxToTime();

      auto drawArea = getDrawArea();
      float maxt = drawDuration();
      float msegEvaluationState[5];
      int lseval = -1;

      float dfmsegEvaluationState[5];
      int dflseval = -1;

      CGraphicsPath *path = dc->createGraphicsPath();
      CGraphicsPath *defpath = dc->createGraphicsPath();
      CGraphicsPath *fillpath = dc->createGraphicsPath();

      float pathFirstY, pathLastX, pathLastY, pathLastDef;

      for( int i=0; i<drawArea.getWidth(); ++i )
      {
         float up = pxt( i + drawArea.left );
         float iup = (int)up;
         float fup = up - iup;
         float v = Surge::MSEG::valueAt( iup, fup, 0, ms, lseval, msegEvaluationState, false );
         float vdef = Surge::MSEG::valueAt( iup, fup, lfodata->deform.val.f, ms, dflseval, dfmsegEvaluationState, false );
         v = valpx( v );
         vdef = valpx( vdef );
         // Brownian doesn't deform and the second display is confusing since it is indepdently random
         if( dflseval >= 0 && dflseval <= ms->n_activeSegments - 1 && ms->segments[dflseval].type == MSEGStorage::segment::Type::BROWNIAN )
            vdef = v;
         if( up <= ms->totalDuration )
         {
            if( i == 0 )
            {
               path->beginSubpath( i, v  );
               defpath->beginSubpath( i, vdef );
               fillpath->beginSubpath( i, v );
               pathFirstY = v;
            }
            else
            {
               path->addLine( i, v );
               defpath->addLine( i, vdef );
               fillpath->addLine( i, v );
            }
            pathLastX = i; pathLastY = v; pathLastDef = vdef;
         }
      }

      auto tfpath = CGraphicsTransform().translate(drawArea.left, 0);

      VSTGUI::CGradient::ColorStopMap csm;
      VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);
      cg->addColorStop(0, skin->getColor(Colors::MSEGEditor::GradientFill::StartColor));
      cg->addColorStop(0.5, skin->getColor(Colors::MSEGEditor::GradientFill::EndColor));
      cg->addColorStop(1, skin->getColor(Colors::MSEGEditor::GradientFill::StartColor));


      fillpath->addLine(pathLastX, valpx(0));
      fillpath->addLine(0, valpx(0));
      fillpath->addLine(0, pathFirstY);
      dc->fillLinearGradient(fillpath, *cg, CPoint(0, 0), CPoint(0, valpx(-1)), false, &tfpath);
      fillpath->forget();
      cg->forget();

      // Draw the axis here after the gradient fill
      drawAxis(dc);
      
      // draw horizontal grid
      dc->setLineWidth(1);
      int skips = (ms->hSnap > 0 ? 1.f / ms->hSnap : 4 );
      auto primaryGridColor = skin->getColor(Colors::MSEGEditor::Grid::Primary);
      auto secondaryGridColor = skin->getColor(Colors::MSEGEditor::Grid::Secondary);

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

      skips = (ms->vSnap > 0 ? 1.f / ms->vSnap : 4 );
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

      // draw segment curve
      dc->setLineWidth( 1.0 );
      dc->setFrameColor( skin->getColor( Colors::MSEGEditor::DeformCurve));
      dc->drawGraphicsPath( defpath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

      dc->setLineWidth(1.5);
      dc->setFrameColor(skin->getColor(Colors::MSEGEditor::Curve));
      dc->drawGraphicsPath( path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

      path->forget();
      defpath->forget();

      for( const auto &h : hotzones )
      {
         if( h.type == hotzone::MOUSABLE_NODE )
         {
            int sz = 12;
            int offx = 0;

            if( h.mousableNodeType == hotzone::SEGMENT_CONTROL )
               offx = 1;

            int offy = 0;

            if( h.active )
               offy = 1;

            if( h.dragging )
               offy = 2;

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
         s.dragv1 = s.nv1;
         s.dragcpv = s.cpv;
         s.dragDuration = s.duration;
      }

      if( timeEditMode == DRAW )
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

               /*
                * Activate temporary snap
                */
               bool s = buttons & kShift;
               bool c = buttons & kControl;
               if( s || c  )
               {
                  snapGuard = std::make_shared<SnapGuard>(ms, this);
                  if( c ) ms->hSnap = ms->hSnapDefault;
                  if( s ) ms->vSnap = ms->vSnapDefault;
               }
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
      snapGuard = nullptr;
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
               if( ms->endpointMode == MSEGStorage::EndpointMode::FREE )
               {
                  if( nx == ms->n_activeSegments  )
                     ms->segments[seg].nv1 = v;
                  else
                     ms->segments[nx].v0 = v;
               }
               else
               {
                  if( nx >= ms->n_activeSegments )
                     nx = 0;
                  ms->segments[nx].v0 = v;
               }
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
               h.onDrag( where.x - mouseDownOrigin.x, where.y - mouseDownOrigin.y, where );
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

      COptionMenu* contextMenu = new COptionMenu(CRect( w, CPoint(0,0)), 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);

      auto tf = pxToTime( );
      auto t = tf( iw.x );
      contextMenu->addEntry( "[?] MSEG Segment" );
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

      auto tts = Surge::MSEG::timeToSegment(ms, t );

      auto typeTo = [this, contextMenu, t, addCb, tts](std::string n, MSEGStorage::segment::Type type) {
                       auto m = addCb( contextMenu, n, [this,t, type]() {
                                                 Surge::MSEG::changeTypeAt( this->ms, t, type );
                                                 modelChanged();
                                              } );
                       if( tts >= 0 )
                          m->setChecked( this->ms->segments[tts].type == type );
                    };
      typeTo( "Line", MSEGStorage::segment::Type::LINEAR );
      typeTo( "Bezier", MSEGStorage::segment::Type::QUAD_BEZIER );
      typeTo( "S-Curve", MSEGStorage::segment::Type::SCURVE );
      typeTo( "Sine", MSEGStorage::segment::Type::SINE );
      typeTo( "Triangle", MSEGStorage::segment::Type::TRIANGLE );
      typeTo( "Square", MSEGStorage::segment::Type::SQUARE );
      typeTo( "Steps", MSEGStorage::segment::Type::STEPS );
      typeTo( "Brownian Bridge", MSEGStorage::segment::Type::BROWNIAN );

      contextMenu->addSeparator();
      if( tts >= 0 )
      {
         auto def = ms->segments[tts].useDeform;
         auto cm = addCb(contextMenu, Surge::UI::toOSCaseForMenu("Deform Applied to Segment"), [this, tts]() {
            this->ms->segments[tts].useDeform = ! this->ms->segments[tts].useDeform;
            modelChanged();
         });
         cm->setChecked(def);
      }

      if( tts == 0 || tts == ms->n_activeSegments - 1 )
      {
         auto cm = addCb(contextMenu, Surge::UI::toOSCaseForMenu("Link Start and End Nodes"),
                         [this]() {
                            if( this->ms->endpointMode == MSEGStorage::EndpointMode::LOCKED )
                               this->ms->endpointMode = MSEGStorage::EndpointMode::FREE;
                            else
                            {
                               this->ms->endpointMode = MSEGStorage::EndpointMode::LOCKED;
                               this->ms->segments[ms->n_activeSegments-1].nv1 = this->ms->segments[0].v0;
                               this->modelChanged();
                            }
                         });
         cm->setChecked( this->ms->endpointMode == MSEGStorage::EndpointMode::LOCKED );
      }
      
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
   case tag_loop_mode: {
      int m = floor((val * 3) + 1);
      ms->loopMode = (MSEGStorage::LoopMode)m;
      canvas->modelChanged();
   }
   break;
   case tag_segment_movement_mode: {
      int m = floor(val * 2 + 0.5);
      canvas->timeEditMode = (MSEGCanvas::TimeEdit)m;
      canvas->recalcHotZones(CPoint(0, 0));
      canvas->invalid();
   }
   break;
   case tag_horizontal_snap:
      if( val < 0.5 ) ms->hSnap = 0; else ms->hSnap = ms->hSnapDefault;
      canvas->invalid();
      break;
   case tag_vertical_snap:
      if( val < 0.5 ) ms->vSnap = 0; else ms->vSnap = ms->vSnapDefault;
      canvas->invalid();
      break;
   case tag_vertical_value:
   {
      // FIXME locales
      auto fv = 1.f / (std::max( std::atoi( ((CTextEdit *)p)->getText() ), 1 ) ); // -1 to 1 remeber
      ms->vSnapDefault = fv;
      if( ms->vSnap > 0 )
         ms->vSnap = ms->vSnapDefault;
      canvas->invalid();
   }
      break;
   case tag_horizontal_value:
   {
      auto fv = 1.f / ( std::max( std::atoi(((CTextEdit *)p)->getText()), 1 ) );
      ms->hSnapDefault = fv;
      if( ms->hSnap > 0 )
         ms->hSnap = ms->hSnapDefault;
      canvas->invalid();
   }
      break;
   default:
      break;
   }
}
void MSEGControlRegion::rebuild()
{
   auto labelFont = new VSTGUI::CFontDesc("Lato", 9, kBoldFace);
   auto editFont = new VSTGUI::CFontDesc("Lato", 9);
   
   int height = getViewSize().getHeight();
   int margin = 2;
   int labelHeight = 12;
   int controlHeight = 12;
   int xpos = 10;

   // loop mode
   {
      int segWidth = 110;
      int btnWidth = 94;
      int ypos = 1;

      auto lpl = new CTextLabel(CRect(CPoint(xpos, ypos), CPoint(segWidth, labelHeight)), "Loop Mode");
      lpl->setFont(labelFont);
      lpl->setFontColor(skin->getColor(Colors::MSEGEditor::Text));
      lpl->setTransparency(true);
      lpl->setHoriAlign(kLeftText);
      addView(lpl);

      ypos += margin + labelHeight;

      // button
      auto btnrect = CRect(CPoint(xpos, ypos), CPoint(btnWidth, controlHeight));
      auto lw = new CHSwitch2(btnrect, this, tag_loop_mode, 3, controlHeight, 1, 3,
                        associatedBitmapStore->getBitmap(IDB_MSEG_LOOP_MODES), CPoint(0, 0), true);
      lw->setSkin(skin,associatedBitmapStore);
      addView(lw);
      lw->setValue((ms->loopMode - 1) / 2.f);

      xpos += segWidth;
   }
   
   // movement modes
   {
      int segWidth = 110;
      int marginPos = xpos + margin;
      int btnWidth = 94;
      int ypos = 1;

      // label
      auto mml = new CTextLabel(CRect(CPoint(marginPos, ypos), CPoint(btnWidth, labelHeight)), "Movement Mode");
      mml->setTransparency(true);
      mml->setFont(labelFont);
      mml->setFontColor(skin->getColor(Colors::MSEGEditor::Text));
      mml->setHoriAlign(kLeftText);
      addView(mml);
      
      ypos += margin + labelHeight;

      // button
      auto btnrect = CRect(CPoint(marginPos, ypos), CPoint(btnWidth, controlHeight));
      auto mw = new CHSwitch2(btnrect, this, tag_segment_movement_mode, 3, controlHeight, 1, 3,
                        associatedBitmapStore->getBitmap(IDB_MSEG_MOVEMENT), CPoint(0, 0), true);
      mw->setSkin(skin,associatedBitmapStore);
      addView(mw);
      mw->setValue(canvas->timeEditMode / 2.f);

      xpos += segWidth;
   }

   // Snap Section
   {
      int btnWidth = 49, editWidth = 32;
      int margin = 2;
      int segWidth = btnWidth + editWidth + 10;
      int ypos = 1;
      char svt[256];

      auto snapC = new CTextLabel(CRect(CPoint(xpos, ypos), CPoint(segWidth * 2 - 5, labelHeight)), "Snap To Grid");
      snapC->setTransparency(true);
      snapC->setFont(labelFont);
      snapC->setFontColor(skin->getColor(Colors::MSEGEditor::Text));
      snapC->setHoriAlign(kLeftText);
      addView(snapC);

      ypos += margin + labelHeight;

      auto hbut = new CSwitchControl(CRect(CPoint(xpos, ypos), CPoint(btnWidth, controlHeight)), this, tag_horizontal_snap,
                                     associatedBitmapStore->getBitmap(IDB_MSEG_HORIZONTAL_SNAP));
      hbut->setSkin(skin,associatedBitmapStore);
      addView(hbut);
      hbut->setValue(ms->hSnap < 0.001 ? 0 : 1);

      snprintf(svt, 255, "%d", (int)round(1.f / ms->hSnapDefault));
      auto htxt = new CTextEdit(CRect(CPoint(xpos + 52 + margin, ypos), CPoint(editWidth, controlHeight)),
                                this, tag_horizontal_value, svt);
#if WINDOWS
      htxt->setTextInset(CPoint(3, 1));
#endif
      htxt->setFont(editFont);
      htxt->setFontColor(skin->getColor(Colors::MSEGEditor::NumberField::Text));
      htxt->setFrameColor(skin->getColor(Colors::MSEGEditor::NumberField::Border));
      htxt->setBackColor(skin->getColor(Colors::MSEGEditor::NumberField::Background));
      htxt->setRoundRectRadius(CCoord(3.f));
      addView(htxt);

      xpos += segWidth;

      auto vbut = new CSwitchControl(CRect(CPoint(xpos, ypos), CPoint(btnWidth, controlHeight)), this, tag_vertical_snap,
                                     associatedBitmapStore->getBitmap(IDB_MSEG_VERTICAL_SNAP));
      vbut->setSkin(skin,associatedBitmapStore);
      addView( vbut );
      vbut->setValue( ms->vSnap < 0.001? 0 : 1 );

      snprintf(svt, 255, "%d", (int)round( 1.f / ms->vSnapDefault));
      auto vtxt = new CTextEdit(CRect(CPoint(xpos + 52 + margin , ypos), CPoint(editWidth, controlHeight)), this, tag_vertical_value, svt);
#if WINDOWS
      vtxt->setTextInset(CPoint(3, 1));
#endif
      vtxt->setFont(editFont);
      vtxt->setFontColor(skin->getColor(Colors::MSEGEditor::NumberField::Text));
      vtxt->setFrameColor(skin->getColor(Colors::MSEGEditor::NumberField::Border));
      vtxt->setBackColor(skin->getColor(Colors::MSEGEditor::NumberField::Background));
      vtxt->setRoundRectRadius(CCoord(3.f));
      addView(vtxt);
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

      int controlHeight = 35;

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

MSEGEditor::MSEGEditor(LFOStorage *lfodata, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin, std::shared_ptr<SurgeBitmaps> b) : CViewContainer( CRect( 0, 0, 760, 365) )
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
