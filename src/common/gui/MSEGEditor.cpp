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

using namespace VSTGUI;

struct MSEGCanvas;

struct MSEGSegmentPanel : public CViewContainer, public Surge::UI::SkinConsumingComponent, public VSTGUI::IControlListener {
   MSEGSegmentPanel(const CRect &size, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin ): CViewContainer( size ) {
      setSkin( skin );
      this->ms = ms;
      rebuild();
      std::cout << Colors::VuMeter::Level.name << std::endl;
   };

   void segmentChanged( int i ) {
      currSeg = i;
      rebuild();
   }

   enum tags {
      seg_type_0 = 0,
      add_before = 10000,
      add_after,
      deletenode
   };
   
   void rebuild() {
      removeAll();

      std::string s = "Segment " + std::to_string(currSeg);

      auto l = new CTextLabel( CRect( CPoint( 3, 2 ), CPoint( getViewSize().getWidth()-6, 18 ) ), s.c_str() );
      addView( l );
      
      int pos = 24;
      auto addb = [&pos, this](std::string s, int t ) {
                     auto b = new CTextButton( CRect( CPoint( 3, pos ), CPoint( getViewSize().getWidth() - 6, 18 ) ), this, t, s.c_str() );
                     this->addView( b );
                     pos += 20;
                  };
      addb( "Constant", seg_type_0 );
      addb( "Line", seg_type_0 + 1);
      addb( "Bezier", seg_type_0 + 2);
      addb( "Add Before", add_before );
      addb( "Add After", add_after );
      addb( "Delete", deletenode );
   }

   virtual void valueChanged( VSTGUI::CControl *control ) override;
   virtual int32_t controlModifierClicked(CControl *p, CButtonState b ) override { return 0; }
   
   int currSeg = -1;
   MSEGStorage *ms;
   MSEGCanvas *canvas = nullptr;
};


struct MSEGControlPanel : public CViewContainer, public Surge::UI::SkinConsumingComponent, public VSTGUI::IControlListener {
   MSEGControlPanel(const CRect &size, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin ): CViewContainer( size ) {
      setSkin( skin );
      this->ms = ms;
      rebuild();
   };

   enum button_tag {
      mag_only,
      time_only,
      mag_and_time,
   };
   
   void rebuild() {
      removeAll();
      std::string s = "UNDER CONSTRUCTION IN NIGHTLIES COMING SOON";
      auto cslab = new CTextLabel( CRect( CPoint( 0, 0 ), getViewSize().getSize() ), s.c_str() );
      cslab->setBackColor( kBlueCColor );
      cslab->setFontColor( kWhiteCColor );
      addView( cslab );

      int pos = 2;
      auto addb = [&pos, this](std::string s, int t ) {
                     auto b = new CTextButton( CRect( CPoint( 3, pos ), CPoint( 100, 18 ) ), this, t, s.c_str() );
                     this->addView( b );
                     pos += 19;
                  };
      addb( "Edit Amp Only", mag_only );
      addb( "Edit Dur Only", time_only );
      addb( "Edit Both", mag_and_time );

   }

   int editmode = mag_only;
   virtual void valueChanged( VSTGUI::CControl *control ) override;
      

   MSEGCanvas *canvas = nullptr;
   MSEGStorage *ms;
};

struct MSEGCanvas : public CControl, public Surge::UI::SkinConsumingComponent {
   MSEGCanvas(const CRect &size, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin ): CControl( size ) {
      setSkin( skin );
      this->ms = ms;
      MSEGModulationHelper::rebuildCache( ms );
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
      return [tscale](float t) { return t / tscale; };
   }
   
   void recalcHotZones( const CPoint &where ) {
      hotzones.clear();

      auto drawArea = getDrawArea();
      
      int handleRadius = 5;

      int editmode = 0; // mag, time, mag + time
      if( controlpanel )
         editmode = controlpanel->editmode;
      
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

         auto rectForPoint = [&]( float t, float v, float xmul1, float xmul2, std::function<void(float,float)> onDrag ){
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
                                hotzones.push_back(h);
                             };
           
         switch( s.type )
         {
         case MSEGStorage::segment::CONSTANT:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, 0, 2,
                          [i,this,editmode,vscale,tscale](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v0 -=  2 * dy / vscale;
                                this->ms->segments[i].v1 = this->ms->segments[i].v0;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                if( i != 0 )
                                {
                                   this->ms->segments[i-1].duration += dx / tscale;
                                   this->ms->segments[i].duration -= dx / tscale;
                                }
                             }
                          } );
            rectForPoint( t1, s.v0, -2, 0,
                          [i,this,editmode,vscale,tscale](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v0 -=  2 * dy / vscale;
                                this->ms->segments[i].v1 = this->ms->segments[i].v0;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                this->ms->segments[i].duration += dx / tscale;
                             }

                          } );
            break;
         }
         case MSEGStorage::segment::LINEAR:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, 0, 2,  
                          [i,this,vscale,tscale,editmode](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v0 -= 2 * dy / vscale;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                if( i != 0 )
                                {
                                   this->ms->segments[i-1].duration += dx / tscale;
                                   this->ms->segments[i].duration -= dx / tscale;
                                }
                             }

                          } );
            rectForPoint( t1, s.v1, -2, 0,
                          [i,this,vscale,tscale,editmode](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v1 -= 2 * dy / vscale;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                this->ms->segments[i].duration += dx / tscale;
                             }

                          } );
                          
            break;
         }
         case MSEGStorage::segment::QUADBEZ:
         {
            // We get a mousable point at the start of the line
            rectForPoint( t0, s.v0, 0, 2,
                          [i,this,vscale,tscale,editmode](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v0 -= 2 * dy / vscale;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                if( i != 0 )
                                {
                                   this->ms->segments[i-1].duration += dx / tscale;
                                   this->ms->segments[i].duration -= dx / tscale;
                                }
                             }
                          } );
                          
            rectForPoint( t1, s.v1, -2, 0,
                          [i,this,vscale,tscale,editmode](float dx, float dy) {
                             if( editmode == 0 || editmode == 2 )
                             {
                                this->ms->segments[i].v1 -= 2 * dy / vscale;
                             }
                             if( editmode == 1 || editmode == 2 )
                             {
                                this->ms->segments[i].duration += dx / tscale;
                             }
                          } );

            float tn = tpx(ms->segmentStart[i] + ms->segments[i].cpduration );
            rectForPoint( tn, s.cpv, -1, 1,
                          [i, this, tscale, vscale]( float dx, float dy ) {
                             this->ms->segments[i].cpv -= 2 * dy / vscale;
                             this->ms->segments[i].cpduration += dx / tscale;
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
         float up = pxt( i );
         float v = MSEGModulationHelper::valueAt( up, 0, ms );
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

      dc->setLineWidth( 3 );
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
            if( h.active )
            {
               dc->setFillColor( kYellowCColor );
               dc->drawRect( h.rect, kDrawFilled );
            }
            if( h.dragging )
            {
               dc->setFillColor( kGreenCColor );
               dc->drawRect( h.rect, kDrawFilled );
            }
            dc->setFrameColor( kRedCColor );
            dc->drawRect( h.rect, kDrawStroked );
         }
      }
   }

   CPoint mouseDownOrigin;
   bool inDrag = false;
   virtual CMouseEventResult onMouseDown(CPoint &where, const CButtonState &buttons ) override {
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
            if( h.selected && segmentpanel )
               segmentpanel->segmentChanged( h.associatedSegment );
               
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

   void modelChanged() {
      MSEGModulationHelper::rebuildCache( ms );
      recalcHotZones(mouseDownOrigin); // FIXME
      invalid();
   }
   
   MSEGStorage *ms;
   MSEGSegmentPanel *segmentpanel = nullptr;
   MSEGControlPanel *controlpanel = nullptr;
   
   CLASS_METHODS( MSEGCanvas, CControl );
};

void MSEGSegmentPanel::valueChanged(CControl *c) {
   auto tag = c->getTag();

   int addAfterSeg = currSeg;
   
   switch(tag )
   {
   case seg_type_0:
   case seg_type_0+1:
   case seg_type_0+2:
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

struct MSEGMainEd : public CViewContainer {

   int controlHeight = 60;
   int segmentWidth = 100;
   
   MSEGMainEd(const CRect &size, MSEGStorage *ms, Surge::UI::Skin::ptr_t skin) : CViewContainer(size) {
      this->ms = ms;
      this->skin = skin;

      auto controlArea = new MSEGControlPanel( CRect( CPoint( 0, 0 ), CPoint( size.getWidth(), controlHeight ) ), ms, skin );
      auto segmentArea = new MSEGSegmentPanel(CRect( CPoint( size.getWidth() - segmentWidth, controlHeight ), CPoint( segmentWidth, size.getHeight() - controlHeight ) ),
                                                     ms, skin );

      auto msegCanv = new MSEGCanvas( CRect( CPoint( 0, controlHeight ), CPoint( size.getWidth() - segmentWidth, size.getHeight() - controlHeight ) ), ms, skin );
      msegCanv->segmentpanel = segmentArea;
      msegCanv->controlpanel = controlArea;
      segmentArea->canvas = msegCanv;
      controlArea->canvas = msegCanv;
      
      addView( msegCanv );
      addView( segmentArea );
      addView( controlArea );
   }

   Surge::UI::Skin::ptr_t skin;
   MSEGStorage *ms;

};

MSEGEditor::MSEGEditor(MSEGStorage *ms, Surge::UI::Skin::ptr_t skin) : CViewContainer( CRect( 0, 0, 720, 475 ) )
{
   setSkin( skin );
   setBackgroundColor( kRedCColor );
   addView( new MSEGMainEd( getViewSize(), ms, skin ) );
}
