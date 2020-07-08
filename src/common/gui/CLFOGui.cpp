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

#include "SurgeGUIEditor.h"
#include "CLFOGui.h"
#include "LfoModulationSource.h"
#include "UserDefaults.h"
#include <chrono>
#include "DebugHelpers.h"
#include "guihelpers.h"
#include <cstdint>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;
extern CFontRef patchNameFont;
extern CFontRef lfoTypeFont;

void CLFOGui::drawtri(CRect r, CDrawContext* dc, int orientation)
{
   int m = 2;
   int startx = r.left + m + 1;
   int endx = r.right - m;

   int starty = r.top + m;
   int midy = (r.top + r.bottom) * 0.5;
   int endy = r.bottom - m;

   if( orientation < 0 )
   {
      std::swap( startx, endx );
   }

   std::vector<CPoint> pl;
   pl.push_back( CPoint( startx, starty ) );
   pl.push_back( CPoint( endx, midy ) );
   pl.push_back( CPoint( startx, endy ) );
   dc->setFillColor( skin->getColor( "lfo.stepseq.button.arrow.fill", kWhiteCColor ) );
   dc->drawPolygon( pl, kDrawFilled );
}


void CLFOGui::draw(CDrawContext* dc)
{
   assert(lfodata);
   assert(storage);
   assert(ss);

   auto size = getViewSize();
   CRect outer(size);
   outer.inset(margin, margin);
   CRect leftpanel(outer);
   CRect maindisp(outer);
   leftpanel.right = lpsize + leftpanel.left;
   maindisp.left = leftpanel.right + 4 + 15;
   maindisp.top += 1;
   maindisp.bottom -= 1;

   if (ss && lfodata->shape.val.i == ls_stepseq)
   {
      drawStepSeq(dc, maindisp, leftpanel);
   }
   else
   {
      bool drawBeats = false;
      CGraphicsPath *path = dc->createGraphicsPath();
      CGraphicsPath *eupath = dc->createGraphicsPath();
      CGraphicsPath *edpath = dc->createGraphicsPath();

      pdata tp[n_scene_params];
      {
         tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
         tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
         tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
         tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
         tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
         tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;

         tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;
         tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
         tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
         tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
         tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
         tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;
      }

      {
         auto *c = &lfodata->rate;
         auto *e = &lfodata->release;
         while( c <= e && ! drawBeats )
         {
            if( c->temposync )
               drawBeats = true;
            ++c;
         }
      }


      float susTime = 0.5;
      float totalEnvTime =  pow(2.0f,lfodata->delay.val.f) +
          pow(2.0f,lfodata->attack.val.f) +
          pow(2.0f,lfodata->hold.val.f) +
          pow(2.0f,lfodata->decay.val.f) +
          std::min(pow(2.0f,lfodata->release.val.f), 4.f) +
          susTime;

      LfoModulationSource* tlfo = new LfoModulationSource();
      tlfo->assign(storage, lfodata, tp, 0, ss, true);
      tlfo->attack();
      CRect boxo(maindisp);
      boxo.offset(-size.left - splitpoint, -size.top);

      if( skin->hasColor( "lfo.waveform.background" ) )
      {
         CRect boxI(size);
         boxI.left += lpsize + 4 + 15;
         // LFO waveform bg area
         dc->setFillColor( skin->getColor( "lfo.waveform.background", CColor( 0xFF, 0x90, 0x00 ) ) );
         dc->drawRect(boxI, CDrawStyle::kDrawFilled);
      }
      auto lfoBgGlyph = bitmapStore->getBitmapByStringID( "LFO_WAVE_BACKGROUND" );
      if( lfoBgGlyph != nullptr )
      {
         CRect boxI(size);
         boxI.left += lpsize + 4 + 15;
         lfoBgGlyph->draw( dc, boxI, CPoint( 0, 0 ), 0xff );
      }

      int minSamples = ( 1 << 3 ) * (int)( boxo.right - boxo.left );
      int totalSamples = std::max( (int)minSamples, (int)(totalEnvTime * samplerate / BLOCK_SIZE) );
      float drawnTime = totalSamples * samplerate_inv * BLOCK_SIZE;

      // OK so let's assume we want about 1000 pixels worth tops in
      int averagingWindow = (int)(totalSamples/1000.0) + 1;

#if LINUX
      float valScale = 10000.0;
#else
      float valScale = 100.0;
#endif
      int susCountdown = -1;

      float priorval = 0.f;
      for (int i=0; i<totalSamples; i += averagingWindow )
      {
         float val = 0;
         float eval = 0;
         float minval = 1000000;
         float maxval = -1000000;
         float firstval;
         float lastval;
         for (int s = 0; s < averagingWindow; s++)
         {
            tlfo->process_block();
            if( susCountdown < 0 && tlfo->env_state == lenv_stuck )
            {
                susCountdown = susTime * samplerate / BLOCK_SIZE;
            }
            else if( susCountdown == 0 && tlfo->env_state == lenv_stuck ) {
                tlfo->release();
            }
            else if( susCountdown > 0 )
            {
                susCountdown --;
            }

            val  += tlfo->output;
            if( s == 0 ) firstval = tlfo->output;
            if( s == averagingWindow - 1 ) lastval = tlfo->output;
            minval = std::min(tlfo->output, minval);
            maxval = std::max(tlfo->output, maxval);
            eval += tlfo->env_val * lfodata->magnitude.val.f;
         }
         val = val / averagingWindow;
         eval = eval / averagingWindow;
         val  = ( ( - val + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
         float euval = ( ( - eval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
         float edval = ( ( eval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;

         float xc = valScale * i / totalSamples;

         if( i == 0 )
         {
             path->beginSubpath(xc, val );
             eupath->beginSubpath(xc, euval);
             if( ! lfodata->unipolar.val.b )
                edpath->beginSubpath(xc, edval);
             priorval = val;
         }
         else
         {
             if( maxval - minval > 0.2 )
             {
                 minval  = ( ( - minval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
                 maxval  = ( ( - maxval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
                 // Windows is sensitive to out-of-order line draws in a way which causes spikes.
                 // Make sure we draw one closest to prior first. See #1438
                 float firstval = minval;
                 float secondval = maxval;
                 if( priorval - minval < maxval - priorval )
                 {
                    firstval = maxval;
                    secondval = minval;
                 }
                 path->addLine(xc - 0.1 * valScale / totalSamples, firstval );
                 path->addLine(xc + 0.1 * valScale / totalSamples, secondval );
             }
             else
             {
                 path->addLine(xc, val );
             }
             priorval = val;
             eupath->addLine(xc, euval);
             edpath->addLine(xc, edval);
         }
      }
      delete tlfo;

      VSTGUI::CGraphicsTransform tf = VSTGUI::CGraphicsTransform()
          .scale(boxo.getWidth()/valScale, boxo.getHeight() / valScale )
          .translate(boxo.getTopLeft().x, boxo.getTopLeft().y )
          .translate(maindisp.getTopLeft().x, maindisp.getTopLeft().y );

#if LINUX
      auto xdisp = maindisp;
      dc->getCurrentTransform().transform(xdisp);
      VSTGUI::CGraphicsTransform tfpath = VSTGUI::CGraphicsTransform()
          .scale(boxo.getWidth()/valScale, boxo.getHeight() / valScale )
          .translate(boxo.getTopLeft().x, boxo.getTopLeft().y )
          .translate(xdisp.getTopLeft().x, xdisp.getTopLeft().y );
#else
      auto tfpath = tf;
#endif

      dc->saveGlobalState();


      // OK so draw the rulers
      CPoint mid0(0, valScale/2.f), mid1(valScale,valScale/2.f);
      CPoint top0(0, valScale * 0.9), top1(valScale,valScale * 0.9);
      CPoint bot0(0, valScale * 0.1), bot1(valScale,valScale * 0.1);
      tf.transform(mid0);
      tf.transform(mid1);
      tf.transform(top0);
      tf.transform(top1);
      tf.transform(bot0);
      tf.transform(bot1);
      dc->setDrawMode(VSTGUI::kAntiAliasing);

      dc->setLineWidth(1.0);
      // LFO bg center line
      dc->setFrameColor(skin->getColor( "lfo.waveform.centerline", VSTGUI::CColor(0xE0, 0x80, 0x00)) );
      dc->drawLine(mid0, mid1);

      dc->setLineWidth(1.0);
      // LFO ruler bounds AKA the upper and lower horizontal lines that define the bounds that the waveform draws in
      dc->setFrameColor(skin->getColor( "lfo.waveform.bounds", VSTGUI::CColor(0xE0, 0x80, 0x00)) );
      dc->drawLine(top0, top1);
      dc->drawLine(bot0, bot1);


#if LINUX
      dc->setLineWidth(40.0);
#else
      dc->setLineWidth(1.0);
#endif
      // LFO ruler bounds AKA the upper and lower horizontal lines that draw the envelope if enabled
      dc->setFrameColor(skin->getColor("lfo.waveform.envelope", VSTGUI::CColor(0xB0, 0x60, 0x00, 0xFF)));
      dc->drawGraphicsPath(eupath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
      dc->drawGraphicsPath(edpath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

      // calculate beat grid related data
      auto bpm = storage->temposyncratio * 120;

      auto denFactor = 4.0 / tsDen;
      auto beatsPerSecond = bpm / 60.0;
      auto secondsPerBeat = 1 / beatsPerSecond;
      auto deltaBeat = secondsPerBeat / drawnTime * valScale * denFactor;

      int nBeats = drawnTime * beatsPerSecond / denFactor;

      auto measureLen = deltaBeat * tsNum;
      int everyMeasure = 1;

      while (measureLen < 4) // a hand selected parameter by playing around with it, tbh
      {
         measureLen *= 2;
         everyMeasure *= 2;
      }

      if (drawBeats)
      {
         for (auto l = 0; l < nBeats; ++l)
         {
            auto xp = deltaBeat * l;

            if (l % (tsNum * everyMeasure) == 0)
            {
               auto soff = 0.0;
               if (l > 10)
                  soff = 0.0;
               CPoint vruleS(xp, valScale * .15), vruleE(xp, valScale * .85);
               tf.transform(vruleS);
               tf.transform(vruleE);
               // major beat divisions on the LFO waveform bg
               dc->setFrameColor(
                   skin->getColor("lfo.waveform.majordivisions", VSTGUI::CColor(0xE0, 0x80, 0x00)));
               // dc->drawLine(mps,mp); // this draws the hat on the bar which I decided to skip
               dc->drawLine(vruleS, vruleE);
            }
         }
      }

#if LINUX
      dc->setLineWidth(50.0);
#elif WINDOWS
      dc->setLineWidth(1.0);
#else
      dc->setLineWidth(1.3);
#endif
      // LFO waveform itself
      dc->setFrameColor(skin->getColor("lfo.waveform.wave", VSTGUI::CColor( 0x00, 0x00, 0x00, 0xFF )));
      dc->drawGraphicsPath(path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

      // top ruler
      if (drawBeats)
      {
         for (auto l = 0; l < nBeats; ++l)
         {
            auto xp = deltaBeat * l;

            if (l % (tsNum * everyMeasure) == 0)
            {
               auto soff = 0.0;
               if (l > 10)
                  soff = 0.0;
               CPoint mp(xp + deltaBeat * (tsNum - 0.5), valScale * 0.01),
                   mps(xp + deltaBeat * soff, valScale * 0.01), sp(xp, valScale * (.01)),
                   ep(xp, valScale * (.1));
               tf.transform(sp);
               tf.transform(ep);
               tf.transform(mp);
               tf.transform(mps);
               // ticks for major beats
               dc->setFrameColor(skin->getColor("lfo.waveform.ruler.ticks", VSTGUI::kBlackCColor));
               dc->setLineWidth(1.0);
               dc->drawLine(sp, ep);
               dc->setLineWidth(1.0);

               char s[256];
               sprintf(s, "%d", l + 1);

               CRect tp(CPoint(xp + 1, valScale * 0.0), CPoint(10, 10));
               tf.transform(tp);
               dc->setFontColor(skin->getColor("lfo.waveform.ruler.font", VSTGUI::kBlackCColor));
               dc->setFont(lfoTypeFont);
               dc->drawString(s, tp, VSTGUI::kLeftText, true);
            }
            else if (everyMeasure == 1)
            {
               CPoint sp(xp, valScale * (.06)), ep(xp, valScale * (.1));
               tf.transform(sp);
               tf.transform(ep);
               dc->setLineWidth(0.5);
               if (l % tsNum == 0)
                  dc->setFrameColor(VSTGUI::kBlackCColor);
               else
                  // small ticks for the ruler
                  dc->setFrameColor(
                      skin->getColor("lfo.waveform.ruler.ticks", VSTGUI::CColor(0xB0, 0x60, 0x00)));
               dc->drawLine(sp, ep);
            }
         }
      }

      // lower ruler calculation
      // find time delta
      int maxNumLabels = 5;
      std::vector<float> timeDeltas = {0.5, 1.0, 2.5, 5.0, 10.0};
      auto currDelta = timeDeltas.begin();
      while (currDelta != timeDeltas.end() && (drawnTime / *currDelta) > maxNumLabels)
      {
         currDelta++;
      }
      float delta = timeDeltas.back();
      if (currDelta != timeDeltas.end())
         delta = *currDelta;

      int nLabels = (int)(drawnTime / delta) + 1;
      float dx = delta / drawnTime * valScale;

      for (int l = 0; l < nLabels; ++l)
      {
         float xp = dx * l;
         float yp = valScale * 0.9;
         float typ = yp;
#if LINUX
         typ = valScale * 0.95;
#endif
         CRect tp(CPoint(xp + 0.5, typ + 0.5), CPoint(10, 10));
         tf.transform(tp);
         dc->setFontColor(skin->getColor("lfo.waveform.ruler.font", VSTGUI::kBlackCColor));
         dc->setFont(lfoTypeFont);
         char txt[256];
         float tv = delta * l;
         if (fabs(roundf(tv) - tv) < 0.05)
            snprintf(txt, 256, "%d s", (int)round(delta * l));
         else
            snprintf(txt, 256, "%.1f s", delta * l);
         dc->drawString(txt, tp, VSTGUI::kLeftText, true);

         CPoint sp(xp, valScale * 0.95), ep(xp, valScale * 0.9);
         tf.transform(sp);
         tf.transform(ep);
         dc->setLineWidth(1.0);
         // lower ruler time ticks
         dc->setFrameColor(skin->getColor("lfo.waveform.ruler.ticks", VSTGUI::kBlackCColor));
         dc->drawLine(sp, ep);
      }

      dc->restoreGlobalState();
      path->forget();
      eupath->forget();
      edpath->forget();
   }

   CColor cshadow = {0x5d, 0x5d, 0x5d, 0xff};
   CColor cselected = skin->getColor( "lfo.type.selected.background", CColor( 0xfe, 0x98, 0x15, 0xff ) );

   dc->setFrameColor(cshadow);
   dc->setFont(lfoTypeFont);

   rect_shapes = leftpanel;
   for (int i = 0; i < n_lfoshapes; i++)
   {
      CRect tb(leftpanel);
      tb.top = leftpanel.top + 10 * i;
      tb.bottom = tb.top + 10;
      //std::cout << std::hex << this << std::dec << " CHECK " << i << " " << lfodata->shape.val.i;
      if (i == lfodata->shape.val.i)
      {
         //std::cout << " ON" << std::endl;
         CRect tb2(tb);
         tb2.left++;
         tb2.top += 0.5;
         tb2.inset(2, 1);
         tb2.offset(0, 1);
         dc->setFillColor(cselected);
         dc->drawRect(tb2, kDrawFilled);
         dc->setFontColor(skin->getColor( "lfo.type.selected.foreground", kBlackCColor) );
      }
      else
      {
         //std::cout << " OFF" << std::endl;
         dc->setFontColor(skin->getColor( "lfo.type.unselected.foreground", kBlackCColor) );
      }
      // dc->fillRect(tb);
      shaperect[i] = tb;
      // tb.offset(0,-1);
      tb.top += 1.6; // now the font is smaller and the box is square, smidge down the text
      dc->drawString(ls_abberations[i], tb);
   }

   setDirty(false);
}

enum
{
   cs_null = 0,
   cs_shape,
   cs_steps,
   cs_trigtray_toggle,
   cs_loopstart,
   cs_loopend,
   cs_linedrag,
};

void CLFOGui::drawStepSeq(VSTGUI::CDrawContext *dc, VSTGUI::CRect &maindisp, VSTGUI::CRect &leftpanel)
{
   dc->setDrawMode(VSTGUI::kAntiAliasing);

   auto size = getViewSize();

   int w = size.getWidth() - splitpoint;
   int h = size.getHeight();

   auto ssbg = skin->getColor( "lfo.stepseq.background", CColor( 0xFF, 0x90, 0x00 ) );
   dc->setFillColor( ssbg );
   dc->drawRect( CRect( 0, 0, w, h ), kDrawFilled );


   // This spec is legacy code from the bitmap days but for now keep it here so we can make sure our default
   // colors and color names are unch in the port.
   auto skd = [this](std::string n, int def) -> CColor {
                 int b = ((def & 0xFF000000) >> 24);
                 int g = ((def & 0x00FF0000) >> 16);
                 int r = ((def & 0x0000FF00) >>  8);
                 return this->skin->getColor( n, CColor( r, g, b ) );
              };


   auto shadowcol = skd( "lfo.stepseq.column.shadow", 0x7d6d6dff );
   auto stepMarker = skd( "lfo.stepseq.step.fill", 0x633412FF);
   auto disStepMarker = skd( "lfo.stepseq.step.fill.disabled", 0xffccbbff);
   auto noLoopHi = skd( "lfo.stepseq.loop.outside.majorstep", 0xdfdfdfff );
   auto noLoopLo = skd( "lfo.stepseq.loop.outside.minorstep", 0xcfcfcfff );
   auto loopRegionLo = skd( "lfo.stepseq.loop.majorstep", 0xe0bf9aff);
   auto loopRegionHi = skd( "lfo.stepseq.loop.minorstep", 0xefd0a9ff );
   auto grabMarker = skd( "lfo.stepseq.loop.markers", 0x633412ff );
   auto grabMarkerHi = skd( "lfo.stepseq.trigger.click", 0x835432ff );

   auto fillr = [dc](CRect r,CColor c) {
                   dc->setFillColor(c);
                   dc->drawRect(r,kDrawFilled);
                };

   // set my coordinate system so that 0,0 is at splitpoint
   auto shiftTranslate = CGraphicsTransform().translate( size.left, size.top ).translate( splitpoint, 0 );
   {
      CDrawContext::Transform shiftTranslatetransform( *dc, shiftTranslate );
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         CRect rstep( 1.f * i * scale, 1.f * margin, 1.f * ( i + 1 ) * scale - 1, 1.f * h - margin2 + 1 ), gstep;


         // Draw an outline in the shadow color
         CRect outline(rstep);
         outline.inset( -1.f, -1.f );
         fillr( outline, shadowcol );

         // Now draw the actual step
         int sp = std::min( ss->loop_start, ss->loop_end );
         int ep = std::max( ss->loop_start, ss->loop_end );

         CColor stepcolor, valuecolor, knobcolor;

         if ((i >= sp) && (i <= ep))
         {
            stepcolor = (i & 3) ? loopRegionHi : loopRegionLo;
            knobcolor = stepMarker;
            valuecolor = stepMarker;
         }
         else
         {
            stepcolor = (i & 3 ) ? noLoopHi : noLoopLo;
            knobcolor = disStepMarker;
            valuecolor = disStepMarker;
         }

         // Code to put in place an editor for the envelope retrigger
         if( edit_trigmask )
         {
            gstep = rstep;
            rstep.top += margin2;
            gstep.bottom = rstep.top - 1;
            gaterect[i] = gstep;
            auto gatecolor = knobcolor;
            auto gatebgcolor = stepcolor;

            uint64_t maski = ss->trigmask & (UINT64_C(1) << i);
            uint64_t maski16 = ss->trigmask & (UINT64_C(1) << (i + 16));
            uint64_t maski32 = ss->trigmask & (UINT64_C(1) << (i + 32));
            
            if( controlstate == cs_trigtray_toggle && ( i == selectedSSrow || draggedIntoTrigTray[i] ) )
            {
               auto bs = trigTrayButtonState;
               maski = ss->trigmask & (UINT64_C(1) << mouseDownTrigTray);
               maski16 = ss->trigmask & (UINT64_C(1) << (mouseDownTrigTray + 16));
               maski32 = ss->trigmask & (UINT64_C(1) << (mouseDownTrigTray + 32));
               if( ( bs & kLButton ) )
               {
                  if( maski | maski16 | maski32 ) {
                     maski = 0;
                     maski16 = 0;
                     maski32 = 0;
                  }
                  else maski = 1;
               }
               else
               {
                  if( maski ) { maski = 0; maski16 = 1; }
                  else if( maski16 ) { maski16 = 0; maski32 = 1; }
                  else if( maski32 ) { maski32 = 0; maski16 = 1; }
                  else { maski16 = 1; }
               }
            }

            if (maski)
            {
               fillr(gstep, gatecolor);
            }
            else if( maski16 )
            {
               // FIXME - an A or an F would be nice eh?
               fillr(gstep, gatebgcolor);
               auto qrect = gstep;
               qrect.right -= (qrect.getWidth() / 2 );
               fillr(qrect, gatecolor);
            }
            else if( maski32 )
            {
               fillr(gstep, gatebgcolor);
               auto qrect = gstep;
               qrect.left += (qrect.getWidth() / 2 );
               fillr(qrect, gatecolor);
            }
            else
            {
               fillr(gstep, gatebgcolor );
            }
         }

         fillr(rstep, stepcolor);
         steprect[i] = rstep;

         // draw the midline when not in unipolar mode
         CRect mid(rstep);
         int p;

         if (!lfodata->unipolar.val.b)
         {
            p = (mid.bottom + mid.top) * 0.5;
            mid.top = p;
            mid.bottom = p + 1;

            fillr(mid, shadowcol);
         }

         // Now draw the value
         CRect v(rstep);
         int p1, p2;
         if (lfodata->unipolar.val.b)
         {
            auto sv = std::max( ss->steps[i], 0.f );
            v.top = v.bottom - (int)(v.getHeight() * sv );
         }
         else
         {
            p1 = v.bottom - (int)((float)0.5f + v.getHeight() * (0.50f + 0.5f * ss->steps[i]));
            p2 = (v.bottom + v.top) * 0.5;
            v.top = min(p1, p2);
            v.bottom = max(p1, p2);
         }

         fillr( v, valuecolor );
      }

      // Draw the loop markers
      rect_ls = steprect[0];
      rect_ls.bottom += margin2 - 1.f;
      rect_ls.top = rect_ls.bottom - margin2 + 2.f;
      rect_ls.right = rect_ls.left + margin2;

      rect_le = rect_ls;

      rect_ls.left  += scale * ss->loop_start - 1.f;
      rect_ls.right += rect_ls.left;

      rect_le.left += scale * (ss->loop_end + 1.f) - margin2;
      rect_le.right = rect_le.left + margin2;

      fillr( rect_ls, grabMarker );
      fillr( rect_le, grabMarker );

      // and lines on either side of the loop
      dc->setFrameColor( grabMarker );
      dc->drawLine( CPoint( rect_ls.left, 0.f), CPoint( rect_ls.left, h-3.f ) );
      dc->drawLine( CPoint( rect_le.right - 1.f, 0.f), CPoint( rect_le.right - 1.f, h-3.f) );
   }

   // These data structures are used for mouse hit detection so have to translate them back to screen
   for( auto i=0; i<n_stepseqsteps; ++i )
   {
      shiftTranslate.transform( steprect[i] );
      shiftTranslate.transform( gaterect[i] );
   }

   shiftTranslate.transform( rect_le );
   shiftTranslate.transform( rect_ls );

   rect_steps = steprect[0];
   rect_steps.right = steprect[n_stepseqsteps - 1].right;

   rect_steps_retrig = gaterect[0];
   rect_steps_retrig.right = gaterect[n_stepseqsteps - 1].right;




   CPoint sp(0, 0);
   CRect sr(size.left + splitpoint, size.top, size.right, size.bottom);

   ss_shift_left = leftpanel;
   ss_shift_left.offset(53, 23);
   ss_shift_left.right = ss_shift_left.left + 12;
   ss_shift_left.bottom = ss_shift_left.top + 34;

   dc->setFillColor(skin->getColor( "lfo.stepseq.button.border", VSTGUI::CColor( 0x5d, 0x5d, 0x5d, 0xff ) ) );
   dc->drawRect(ss_shift_left, kDrawFilled);
   ss_shift_left.inset(1, 1);
   ss_shift_left.bottom = ss_shift_left.top + 16;

   if( ss_shift_hover == 1 )
      dc->setFillColor(skin->getColor( "lfo.stepseq.button.hover", VSTGUI::CColor( 0xAE, 0xAF, 0xBE, 0xff ) ) );
   else
      dc->setFillColor(skin->getColor( "lfo.stepseq.button.fill", VSTGUI::CColor( 0x97, 0x98, 0x9a, 0xff ) ) );
   dc->drawRect(ss_shift_left, kDrawFilled);
   drawtri(ss_shift_left, dc, -1);

   ss_shift_right = ss_shift_left;
   ss_shift_right.offset(0, 16);
   if( ss_shift_hover == 2 )
      dc->setFillColor(skin->getColor( "lfo.stepseq.button.hover", VSTGUI::CColor( 0xAE, 0xAF, 0xBE, 0xff ) ) );
   else
      dc->setFillColor(skin->getColor( "lfo.stepseq.button.fill", VSTGUI::CColor( 0x97, 0x98, 0x9a, 0xff ) ) );
   dc->drawRect(ss_shift_right, kDrawFilled);
   drawtri(ss_shift_right, dc, 1);

   // OK now we have everything drawn we want to draw the LFO. This is similar to the LFO
   // code above but with very different scaling in time since we need to match the steps no
   // matter the rate

   pdata tp[n_scene_params];
   tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
   tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
   tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
   tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
   tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
   tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;
   
   tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;

   // Min out the rate
   float displayRate = lfodata->rate.val.f;
   if( lfodata->rate.val.f < -3.5 )
   {
      tp[lfodata->rate.param_id_in_scene].f = - 3.5;
      displayRate = -3.5;
   }
   else
   {
      tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
   }
   
   tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
   tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
   tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
   tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;


   /*
   ** First big difference - the total env time is basically 16 * the time of the rate
   */
   float freq = pow( 2.0, displayRate ); // frequency in hz
   if( lfodata->rate.temposync )
   {
      freq *= storage->temposyncratio;
   }
   
   float cyclesec = 1.0 / freq;
   float totalSampleTime = cyclesec * n_stepseqsteps;
   float susTime = 4.0 * cyclesec;

   LfoModulationSource* tlfo = new LfoModulationSource();
   tlfo->assign(storage, lfodata, tp, 0, ss, true);
   tlfo->attack();
   CRect boxo(rect_steps);

   int minSamples = ( 1 << 3 ) * (int)( boxo.right - boxo.left );
   int totalSamples = std::max( (int)minSamples, (int)(totalSampleTime * samplerate / BLOCK_SIZE) );
   float drawnTime = totalSamples * samplerate_inv * BLOCK_SIZE;
   float cycleSamples = cyclesec * samplerate / BLOCK_SIZE;

   
   // OK so lets assume we want about 1000 pixels worth tops in
   int averagingWindow = (int)(totalSamples/1000.0) + 1;

#if LINUX
   float valScale = 10000.0;
#else
   float valScale = 100.0;
#endif
   int susCountdown = -1;

   CGraphicsPath *path = dc->createGraphicsPath();
   CGraphicsPath *eupath = dc->createGraphicsPath();
   CGraphicsPath *edpath = dc->createGraphicsPath();

   for (int i=0; i<totalSamples; i += averagingWindow )
   {
      float val = 0;
      float eval = 0;
      float minval = 1000000;
      float maxval = -1000000;
      float firstval;
      float lastval;
      for (int s = 0; s < averagingWindow; s++)
      {
         tlfo->process_block();
         if( susCountdown < 0 && tlfo->env_state == lenv_stuck )
         {
            susCountdown = susTime * samplerate / BLOCK_SIZE;
         }
         else if( susCountdown == 0 && tlfo->env_state == lenv_stuck ) {
            tlfo->release();
         }
         else if( susCountdown > 0 ) {
            susCountdown --;
         }
         
         val  += tlfo->output;
         if( s == 0 ) firstval = tlfo->output;
         if( s == averagingWindow - 1 ) lastval = tlfo->output;
         minval = std::min(tlfo->output, minval);
         maxval = std::max(tlfo->output, maxval);
         eval += tlfo->env_val * lfodata->magnitude.val.f;
      }
      val = val / averagingWindow;
      eval = eval / averagingWindow;

      if( lfodata->unipolar.val.b )
         val = val * 2.0 - 1.0;

      val  = ( ( - val + 1.0f ) * 0.5 ) * valScale;
      float euval = ( ( - eval + 1.0f ) * 0.5 ) * valScale;
      float edval = ( ( eval + 1.0f ) * 0.5  ) * valScale;

      float xc = valScale * i / ( cycleSamples * n_stepseqsteps);
      if( i == 0 )
      {
         path->beginSubpath(xc, val );
         eupath->beginSubpath(xc, euval);
         if( ! lfodata->unipolar.val.b )
            edpath->beginSubpath(xc, edval);
      }
      else
      {
         path->addLine(xc, val );
         eupath->addLine(xc, euval);
         edpath->addLine(xc, edval);
      }
   }
   delete tlfo;

   auto q = boxo;
#if LINUX   
   dc->getCurrentTransform().transform(q);
#endif
   
   VSTGUI::CGraphicsTransform tf = VSTGUI::CGraphicsTransform()
      .scale(boxo.getWidth()/valScale, boxo.getHeight() / valScale )
      .translate(q.getTopLeft().x, q.getTopLeft().y );

   auto tfpath = tf;
   
#if LINUX
   dc->setLineWidth(50.0);
#else
   dc->setLineWidth(1.0);
#endif

   dc->setFrameColor( skin->getColor( "lfo.stepseq.envelope", shadowcol ) );
   dc->drawGraphicsPath( eupath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
   dc->drawGraphicsPath( edpath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

#if LINUX
   dc->setLineWidth(60.0);
#else
   dc->setLineWidth(1.0);
#endif
   dc->setFrameColor( skin->getColor( "lfo.stepseq.wave", CColor( 0xFF, 0xFF, 0xFF) ) );
   dc->drawGraphicsPath( path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
   
   path->forget();
   eupath->forget();
   edpath->forget();

   // draw the RMB drag line
   if (controlstate == cs_linedrag)
   {
      dc->setLineWidth( 1.0 );
      dc->setFrameColor(skin->getColor("lfo.stepseq.drag.line", VSTGUI::CColor(0xdd, 0xdd, 0xff)));
      dc->setFillColor(skin->getColor("lfo.stepseq.drag.line", VSTGUI::CColor(0xdd, 0xdd, 0xff)));
      dc->drawLine(rmStepStart, rmStepCurr);

      CRect eStart(rmStepStart, VSTGUI::CPoint(3, 3));
      eStart.offset(-1, -1);
      dc->drawEllipse(eStart, kDrawFilled);

      // Draw an arrow. Thanks
      // https://stackoverflow.com/questions/3010803/draw-arrow-on-line-algorithm
      float dx = rmStepCurr.x - rmStepStart.x;
      float dy = rmStepCurr.y - rmStepStart.y;
      float dd = dx * dx + dy * dy;
      if (dd > 0)
      {
         float ds = sqrt(dd);
         dx /= ds;
         dy /= ds;
         dx *= 6;
         dy *= 6;

         const double cosv = 0.866;
         const double sinv = 0.500;
         auto e1 = rmStepCurr;
         auto e2 = rmStepCurr;
         e1.offset(-(dx * cosv + dy * -sinv), -(dx * sinv + dy * cosv));
         e2.offset(-(dx * cosv + dy * sinv), -(dx * -sinv + dy * cosv));

         std::vector<CPoint> pl;
         pl.push_back(rmStepCurr);
         pl.push_back(e1);
         pl.push_back(e2);

         dc->drawPolygon(pl, kDrawFilled);
         dc->drawLine( rmStepCurr, e1 );
         dc->drawLine( e1, e2 );
         dc->drawLine( e2, rmStepCurr );
      }
   }

   // Finally draw the drag label
   if (controlstate == cs_steps && draggedStep >= 0 && draggedStep < n_stepseqsteps)
   {
      int prec = 2;

      if (storage)
      {
         int detailedMode =
             Surge::Storage::getUserDefaultValue(storage, "highPrecisionReadouts", 0);
         if (detailedMode)
         {
            prec = 6;
         }
      }

      float dragX, dragY;
      float dragW = (prec > 4 ? 60 : 40), dragH = (keyModMult ? 22 : 12);

      auto sr = steprect[draggedStep];

      // Draw to the right in the second half of the seq table
      if (draggedStep < n_stepseqsteps / 2)
      {
         dragX = sr.right;
      }
      else
      {
         dragX = sr.left - dragW;
      }

      float yTop;
      if (lfodata->unipolar.val.b)
      {
         auto sv = std::max(ss->steps[draggedStep], 0.f);
         yTop = sr.bottom - (sr.getHeight() * sv);
      }
      else
      {
         yTop = sr.bottom -
                ((float)0.5f + sr.getHeight() * (0.5f + 0.5f * ss->steps[draggedStep]));
      }

      if (yTop > sr.getHeight() / 2)
      {
         dragY = yTop - dragH;
      }
      else
      {
         dragY = yTop;
      }

      if( dragY < size.top + 2 ) dragY = size.top + 2;
      
      CRect labelR(dragX, dragY, dragX + dragW, dragY + dragH);

      auto iwbrc = skin->getColor( "infowindow.border", kBlackCColor );
      auto iwbgc = skin->getColor( "infowindow.background", kWhiteCColor );
      auto iwfgc = skin->getColor( "infowindow.foreground", kBlackCColor );
      
      fillr(labelR, skin->getColor("lfo.stepseq.popup.border", iwbrc));
      labelR.inset(1, 1);
      fillr(labelR, skin->getColor("lfo.stepseq.popup.background", iwbgc ));

      labelR.left += 1;
      labelR.top -= (keyModMult > 0 ? 9 : 0);

      char txt[256];
      sprintf(txt, "%.*f %%", prec, ss->steps[draggedStep] * 100.f);

      dc->setFontColor(skin->getColor("lfo.stepseq.popup.foreground", iwfgc ) );
      dc->setFont(lfoTypeFont);
      dc->drawString(txt, labelR, VSTGUI::kLeftText, true);

      if (keyModMult > 0)
      {
         labelR.bottom += 18;

         sprintf(txt, "%d/%d", (int)(floor(ss->steps[draggedStep] * keyModMult + 0.5)), keyModMult);
         dc->drawString(txt, labelR, VSTGUI::kLeftText, true);
      }
   }
}


CMouseEventResult CLFOGui::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (1) //(buttons & kLButton))
   {
      if (ss && lfodata->shape.val.i == ls_stepseq)
      {
         if (rect_steps.pointInside(where))
         {
            if( buttons.isRightButton() )
            {
               rmStepStart = where;
               controlstate = cs_linedrag;
            }
            else
            {
               controlstate = cs_steps;
            }
            onMouseMoved(where, buttons);
            return kMouseEventHandled;
         }
         else if (rect_steps_retrig.pointInside(where))
         {
            controlstate = cs_trigtray_toggle;

            for (int i = 0; i < n_stepseqsteps; i++)
            {
               draggedIntoTrigTray[i] = false;
               if ((where.x > gaterect[i].left) && (where.x < gaterect[i].right))
               {
                  selectedSSrow = i;
                  mouseDownTrigTray = i;
                  trigTrayButtonState = buttons;
                  draggedIntoTrigTray[i] = true;
               }
            }
            invalid();
            return kMouseEventHandled;
         }
         else if (ss_shift_left.pointInside(where))
         {
            float t = ss->steps[0];
            for (int i = 0; i < (n_stepseqsteps - 1); i++)
            {
               ss->steps[i] = ss->steps[i + 1];
               assert((i >= 0) && (i < n_stepseqsteps));
            }
            ss->steps[n_stepseqsteps - 1] = t;
            ss->trigmask = ( ((ss->trigmask & 0x000000000000fffe) >> 1) | (((ss->trigmask & 1) << 15) & 0xffff)) |
               ( ((ss->trigmask & 0x00000000fffe0000) >> 1) | (((ss->trigmask & 0x10000) << 15) & 0xffff0000 )) |
               ( ((ss->trigmask & 0x0000fffe00000000) >> 1) | (((ss->trigmask & 0x100000000) << 15) & 0xffff00000000 ));

            invalid();
            return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
         }
         else if (ss_shift_right.pointInside(where))
         {
            float t = ss->steps[n_stepseqsteps - 1];
            for (int i = (n_stepseqsteps - 2); i >= 0; i--)
            {
               ss->steps[i + 1] = ss->steps[i];
               assert((i >= 0) && (i < n_stepseqsteps));
            }
            ss->steps[0] = t;
            ss->trigmask = ( ((ss->trigmask & 0x0000000000007fff) << 1) | (((ss->trigmask & 0x0000000000008000) >> 15) & 0xffff )) |
               ( ((ss->trigmask & 0x000000007fff0000) << 1) | (((ss->trigmask & 0x0000000080000000) >> 15) & 0xffff0000 ))|
               ( ((ss->trigmask & 0x00007fff00000000) << 1) | (((ss->trigmask & 0x0000800000000000) >> 15) & 0xffff00000000 ));
            invalid();
            return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
         }
      }
      if (rect_ls.pointInside(where))
      {
         controlstate = cs_loopstart;
         return kMouseEventHandled;
      }
      else if (rect_le.pointInside(where))
      {
         controlstate = cs_loopend;
         return kMouseEventHandled;
      }
      else if (rect_shapes.pointInside(where))
      {
         controlstate = cs_shape;
         onMouseMoved(where, buttons);
         return kMouseEventHandled;
      }
   }
   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}
CMouseEventResult CLFOGui::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   if (controlstate == cs_trigtray_toggle)
   {
      selectedSSrow = -1;

      bool bothOn = ss->trigmask & ( UINT64_C(1) << mouseDownTrigTray );
      bool filtOn = ss->trigmask & ( UINT64_C(1) << ( 16 + mouseDownTrigTray ) );
      bool ampOn = ss->trigmask & ( UINT64_C(1) << ( 32 + mouseDownTrigTray ) );
      bool anyOn = bothOn || filtOn || ampOn;

      for (int i = 0; i < n_stepseqsteps; i++)
      {

         if (draggedIntoTrigTray[i])
         {
            uint64_t off = 0, on = 0;
            off = UINT64_C(1) << i |
               UINT64_C(1) << ( i + 16 ) |
               UINT64_C(1) << ( i + 32 );

            if( ( buttons & kShift ) | ( buttons & kRButton ) )
            {
               if( bothOn || ( !(filtOn || ampOn) ) )
               {
                  on = UINT64_C(1) << ( 16 + i );
               }
               else if( filtOn )
               {
                  on = UINT64_C(1) << ( 32 + i );
               }
               else if( ampOn )
               {
                  on = UINT64_C(1) << ( 16 + i );
               }
            }
            else
            {
               if( ! anyOn )
               {
                  on = UINT64_C(1) << i;
               }
            }

            ss->trigmask = (ss->trigmask & ~off) | on;
            invalid();
         }
      }
   }

   if( controlstate == cs_linedrag )
   {
      int startStep = -1;
      int endStep = -1;

      // find out if a microtuning is loaded and store the scale length for Alt-drag
      // quantization to scale degrees
      keyModMult = 0;
      int quantStep = 12;

      if (!storage->isStandardTuning && storage->currentScale.count > 1)
         quantStep = storage->currentScale.count;

      for (int i = 0; i < 16; ++i)
      {
         if( steprect[i].pointInside(rmStepStart) ) startStep = i;
         if( steprect[i].pointInside(rmStepCurr ) ) endStep = i;
      };

      int s = startStep, e = endStep;

      if (s >= 0 && e >= 0 && s != e) // s == e is the abort gesture
      {
         float fs = (float)(steprect[s].bottom - rmStepStart.y) / steprect[s].getHeight();
         float fe = (float)(steprect[e].bottom - rmStepCurr.y) / steprect[e].getHeight();

         if( e < s )
         {
            std::swap( e, s );
            std::swap( fe, fs );
         }

         if (lfodata->unipolar.val.b)
         {
            fs = limit_range(fs, 0.f, 1.f);
            fe = limit_range(fe, 0.f, 1.f);
         }
         else
         {
            fs = limit_range(fs * 2.f - 1.f, -1.f, 1.f);
            fe = limit_range(fe * 2.f - 1.f, -1.f, 1.f);
         }

         ss->steps[s] = fs;

         if (s != e)
         {
            float ds = ( fs - fe ) / ( s - e );
            for (int q = s; q <= e; q ++)
            {
               float f = ss->steps[s] + (q - s) * ds;
               
               if (buttons & kShift)
               {
                  keyModMult = quantStep; // only shift pressed

                  if (buttons & kAlt)
                  {
                     keyModMult = quantStep * 2; // shift+alt pressed
                     f *= quantStep * 2;
                     f = floor(f + 0.5);
                     f *= (1.f / (quantStep * 2));
                  }
                  else
                  {
                     f *= quantStep;
                     f = floor(f + 0.5);
                     f *= (1.f / quantStep);
                  }
               }

               ss->steps[q] = f;
            }
         }

         invalid();
      }
   }

   if (controlstate)
   {
      // onMouseMoved(where,buttons);
      controlstate = cs_null;
      if( lfodata->shape.val.i == ls_stepseq )
         invalid();
   }
   return kMouseEventHandled;
}

CMouseEventResult CLFOGui::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   int pss = ss_shift_hover;
   ss_shift_hover = 0;
   if (rect_shapes.pointInside(where))
   {
      // getFrame()->setCursor( VSTGUI::kCursorHand );
   }
   else if (ss && lfodata->shape.val.i == ls_stepseq && (
               ss_shift_left.pointInside(where) ||
               ss_shift_right.pointInside(where)
               ))
   {
      
      ss_shift_hover = ss_shift_left.pointInside(where) ? 1 : 2;
      // getFrame()->setCursor( VSTGUI::kCursorHand );
   }
   else
   {
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
   }
   if( ss_shift_hover != pss )
      invalid();
   
   if (controlstate == cs_shape)
   {
      for (int i = 0; i < n_lfoshapes; i++)
      {
         if (shaperect[i].pointInside(where))
         {
            if (lfodata->shape.val.i != i)
            {
               lfodata->shape.val.i = i;
               invalid();

               // This is such a hack
               auto sge = dynamic_cast<SurgeGUIEditor *>(listener);
               if( sge )
               {
                  sge->refresh_mod();
                  sge->forceautomationchangefor(&(lfodata->shape));
               }

            }
         }
      }
   }
   else if (controlstate == cs_loopstart)
   {
      ss->loop_start = limit_range((int)(where.x - steprect[0].left + (scale >> 1)) / scale, 0,
                                   n_stepseqsteps - 1);
      invalid();
   }
   else if (controlstate == cs_loopend)
   {
      ss->loop_end = limit_range((int)(where.x - steprect[0].left - (scale >> 1)) / scale, 0,
                                 n_stepseqsteps - 1);
      invalid();
   }
   else if (controlstate == cs_steps)
   {
      // find out if a microtuning is loaded and store the scale length for Alt-drag
      // quantization to scale degrees
      keyModMult = 0;
      int quantStep = 12;

      if (!storage->isStandardTuning && storage->currentScale.count > 1)
          quantStep = storage->currentScale.count;

      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > steprect[i].left) && (where.x < steprect[i].right))
         {
            draggedStep = i;

            float f = (float)(steprect[i].bottom - where.y) / steprect[i].getHeight();

            if (( buttons & kControl ) || ( buttons & kDoubleClick ))
            {
               f = 0;
            }
            else if (lfodata->unipolar.val.b)
            {
               f = limit_range(f, 0.f, 1.f);
            }
            else
            {
               f = limit_range(f * 2.f - 1.f, -1.f, 1.f);
            }

            if (buttons & kShift)
            {
               keyModMult = quantStep; // only shift pressed

               if (buttons & kAlt)
               {
                  keyModMult = quantStep * 2; // shift+alt pressed
                  f *= quantStep * 2;
                  f = floor(f + 0.5);
                  f *= (1.f / (quantStep * 2));
               }
               else
               {
                  f *= quantStep;
                  f = floor(f + 0.5);
                  f *= (1.f / quantStep);
               }
            }

            ss->steps[i] = f;
            invalid();
         }
      }
   }
   else if( controlstate == cs_trigtray_toggle )
   {
      bool change = false;
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > gaterect[i].left) && (where.x < gaterect[i].right))
         {
            if( ! draggedIntoTrigTray[i] )
               change = true;
            draggedIntoTrigTray[i] = true;
         }
      }
      if( change )
         invalid();
   }
   else if( controlstate == cs_linedrag )
   {
      // ED: this is gross, but works!
      if (rect_steps.pointInside(where))
      {
         rmStepCurr = where;
      }
      else
      {
         // this is supposed to be better but doesn't quite work yet!
         float rx, ry;
         
         if (Surge::UI::get_line_intersection(rmStepStart.x, rmStepStart.y, where.x, where.y,
                                              rect_steps.left, rect_steps.top, rect_steps.right, rect_steps.top,
                                              &rx, &ry))
         {
            rmStepCurr.x = rx;
            rmStepCurr.y = ry+1;
         }
         else if (Surge::UI::get_line_intersection(rmStepStart.x, rmStepStart.y, where.x, where.y,
                                                   rect_steps.left, rect_steps.top, rect_steps.left, rect_steps.bottom,
                                                   &rx, &ry))
         {
            rmStepCurr.x = rx+1;
            rmStepCurr.y = ry;
         }
         else if (Surge::UI::get_line_intersection(rmStepStart.x, rmStepStart.y, where.x, where.y,
                                                   rect_steps.right, rect_steps.top, rect_steps.right, rect_steps.bottom,
                                                   &rx, &ry))
         {
            rmStepCurr.x = rx-1;
            rmStepCurr.y = ry;
         }
         else if (Surge::UI::get_line_intersection(rmStepStart.x, rmStepStart.y, where.x, where.y,
                                                   rect_steps.left, rect_steps.bottom, rect_steps.right, rect_steps.bottom,
                                                   &rx, &ry))
         {
            rmStepCurr.x = rx;
            rmStepCurr.y = ry-1;
         }
         else if (rect_steps.pointInside(where))
            rmStepCurr = where;
      }
      
      invalid();
   }
      
   return kMouseEventHandled;
}

void CLFOGui::invalidateIfIdIsInRange(int id)
{
   bool inRange = false;
   if( ! lfodata ) return;

   auto *c = &lfodata->rate;
   auto *e = &lfodata->release;
   while( c <= e && ! inRange )
   {
      if( id == c->id )
         inRange = true;
      ++c;
   }

   if (inRange)
   {
      invalid();
   }
}

void CLFOGui::invalidateIfAnythingIsTemposynced()
{
   bool isSynced = false;
   if( ! lfodata ) return;

   auto *c = &lfodata->rate;
   auto *e = &lfodata->release;
   while( c <= e && ! isSynced )
   {
      if( c->temposync )
         isSynced = true;
      ++c;
   }

   if (isSynced)
   {
      invalid();
   }
}

bool CLFOGui::onWheel( const VSTGUI::CPoint &where, const float &distance, const CButtonState &buttons )
{
   if (ss && lfodata->shape.val.i == ls_stepseq && rect_steps.pointInside(where) ) {
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > steprect[i].left) && (where.x < steprect[i].right))
         {
            // I am sure we will need to callibrate this and add scale gestures and stuff
            if (buttons & kShift)
            {
               ss->steps[i] = limit_range(ss->steps[i] + (distance / 30.f), -1.f, 1.f);
            }
            else
            {
               ss->steps[i] = limit_range(ss->steps[i] + (distance / 10.f), -1.f, 1.f);
            }
            invalid();
         }
      }
   }
   else if (rect_shapes.pointInside(where))
   {
      static float accumDist = 0;
      accumDist += distance;

      auto jog = [this](int d) {
                    auto ps = this->lfodata->shape.val.i;
                    auto ns = ps + d;
                    if( ns < 0 )
                       ns = 0;
                    if( ns >= n_lfoshapes )
                       ns = n_lfoshapes - 1;

                    if( ns != this->lfodata->shape.val.i )
                    {
                       this->lfodata->shape.val.i = ns;
                       this->invalid();
                       // This is such a hack
                       auto sge = dynamic_cast<SurgeGUIEditor *>(this->listener);
                       if( sge )
                       {
                          sge->refresh_mod();
                          sge->forceautomationchangefor(&(this->lfodata->shape));
                       }
                    }

                 };
      if( accumDist > 0.5 )
      {
         jog(-1);
         accumDist = 0;
      }
      else if( accumDist < -0.5 )
      {
         jog(+1);
         accumDist = 0;
      }
   }

   return false;
}
