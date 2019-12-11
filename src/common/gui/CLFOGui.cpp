//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CLFOGui.h"
#include "LfoModulationSource.h"
#include "UserDefaults.h"
#include <chrono>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;
extern CFontRef patchNameFont;
extern CFontRef lfoTypeFont;

void drawtri(CRect r, CDrawContext* context, int orientation)
{
   int m = 2;
   int startx = r.left + m + 1;
   int endx = r.right - m;
   int midy = (r.top + r.bottom) * 0.5;
   int a = 0;
   if (orientation > 0)
      a = (endx - startx) - 1;

   for (int x = startx; x < endx; x++)
   {
      for (int y = (midy - a); y <= (midy + a); y++)
      {
         context->drawPoint(CPoint(x, y), kWhiteCColor);
      }
      a -= orientation;
      a = max(a, 0);
   }
}

void CLFOGui::draw(CDrawContext* dc)
{
#if LINUX
   /*
   ** As of 1.6.2, the linux vectorized drawing is slow and scales imporoperly with zoom, so
   ** return to the original bitmap drawing until we resolve. See issue #1103.
   ** 
   ** Also some older machines report performance problems so make it switchable
   */
   drawBitmap(dc);
#else
   auto useBitmap = Surge::Storage::getUserDefaultValue(storage, "useBitmapLFO", 0 );
   if( ignore_bitmap_pref || useBitmap )
   {
        drawBitmap(dc);
   }
   else
   {
       auto start = std::chrono::high_resolution_clock::now();
       drawVectorized(dc);
       auto end = std::chrono::high_resolution_clock::now();
       std::chrono::duration<double> elapsed_seconds = end-start;

       // If this draw takes more than, say, 1/10th of a second, our GPU is too slow. 
       if( elapsed_seconds.count() > 0.1 )
           ignore_bitmap_pref = true;
   }
#endif
}

void CLFOGui::drawVectorized(CDrawContext* dc)
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
       cdisurf->begin();
#if MAC
       cdisurf->clear(0x0090ffff);
#else
       cdisurf->clear(0xffff9000);
#endif
       int w = cdisurf->getWidth();
       int h = cdisurf->getHeight();

       // I know I could do the math to convert these colors but I would rather leave them as literals for the compiler
       // so we don't have to shift them at runtime. See issue #141 in surge github
#if MAC
#define PIX_COL( a, b ) b
#else
#define PIX_COL( a, b ) a
#endif
       // Step Sequencer Colors. Remember mac is 0xRRGGBBAA and mac is 0xAABBGGRR
       int stepMarker = PIX_COL( 0xff000000, 0x000000ff);
       int disStepMarker = PIX_COL( 0xffaaaaaa, 0xaaaaaaff);
       int loopRegionHi = PIX_COL( 0xffc6e9c4, 0xc4e9c6ff);
       int loopRegionLo = PIX_COL( 0xffb6d9b4, 0xb4d9b6ff );
       int noLoopHi = PIX_COL( 0xffdfdfdf, 0xdfdfdfff );
       int noLoopLo = PIX_COL( 0xffcfcfcf, 0xcfcfcfff );
       int grabMarker = PIX_COL( 0x00087f00, 0x007f08ff ); // Surely you can't mean this to be fully transparent?
       // But leave non-mac unch
       
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         CRect rstep(maindisp), gstep;
         rstep.offset(-size.left - splitpoint, -size.top);
         rstep.left += scale * i;
         rstep.right = rstep.left + scale - 1;
         rstep.bottom -= margin2 + 1;
         CRect shadow(rstep);
         shadow.inset(-1, -1);
         cdisurf->fillRect(shadow, skugga);
         if (edit_trigmask)
         {
            gstep = rstep;
            rstep.top += margin2;
            gstep.bottom = rstep.top - 1;
            gaterect[i] = gstep;
            gaterect[i].offset(size.left + splitpoint, size.top);

            if (ss->trigmask & (1L << i))
               cdisurf->fillRect(gstep, stepMarker);
            else if( ss->trigmask & ( 1L << ( 16 + i ) ) )
            {
               // FIXME - an A or an F would be nice eh?
               cdisurf->fillRect(gstep, disStepMarker);
               auto qrect = gstep;
               qrect.right -= (qrect.getWidth() / 2 );
               cdisurf->fillRect(qrect, stepMarker);
            }
            else if( ss->trigmask & ( 1L << ( 32 + i ) ) )
            {
               cdisurf->fillRect(gstep, disStepMarker);
               auto qrect = gstep;
               qrect.left += (qrect.getWidth() / 2 );
               cdisurf->fillRect(qrect, stepMarker);
            }
            else if ((i >= ss->loop_start) && (i <= ss->loop_end))
               cdisurf->fillRect(gstep, (i & 3) ? loopRegionHi : loopRegionLo);
            else
               cdisurf->fillRect(gstep, (i & 3) ? noLoopHi : noLoopLo);
         }
         if ((i >= ss->loop_start) && (i <= ss->loop_end))
            cdisurf->fillRect(rstep, (i & 3) ? loopRegionHi : loopRegionLo);
         else
            cdisurf->fillRect(rstep, (i & 3) ? noLoopHi : noLoopLo);
         steprect[i] = rstep;
         steprect[i].offset(size.left + splitpoint, size.top);
         CRect v(rstep);
         int p1, p2;
         if (lfodata->unipolar.val.b)
         {
            v.top = v.bottom - (int)(v.getHeight() * ss->steps[i]);
         }
         else
         {
            p1 = v.bottom - (int)((float)0.5f + v.getHeight() * (0.5f + 0.5f * ss->steps[i]));
            p2 = (v.bottom + v.top) * 0.5;
            v.top = min(p1, p2);
            v.bottom = max(p1, p2) + 1;
         }
         // if (p1 == p2) p2++;
         cdisurf->fillRect(v, stepMarker);
      }

      rect_steps = steprect[0];
      rect_steps.right = steprect[n_stepseqsteps - 1].right;
      rect_steps_retrig = gaterect[0];
      rect_steps_retrig.right = gaterect[n_stepseqsteps - 1].right;

      rect_ls = maindisp;
      rect_ls.offset(-size.left - splitpoint, -size.top);
      rect_ls.top = rect_ls.bottom - margin2;
      rect_le = rect_ls;

      rect_ls.left += scale * ss->loop_start - 1;
      rect_ls.right = rect_ls.left + margin2;
      rect_le.right = rect_le.left + scale * (ss->loop_end + 1);
      rect_le.left = rect_le.right - margin2;

      cdisurf->fillRect(rect_ls, grabMarker);
      cdisurf->fillRect(rect_le, grabMarker);
      CRect linerect(rect_ls);
      linerect.top = maindisp.top - size.top;
      linerect.right = linerect.left + 1;
      cdisurf->fillRect(linerect, grabMarker);
      linerect = rect_le;
      linerect.top = maindisp.top - size.top;
      linerect.left = linerect.right - 1;
      cdisurf->fillRect(linerect, grabMarker);

      rect_ls.offset(size.left + splitpoint, size.top);
      rect_le.offset(size.left + splitpoint, size.top);

      
      CPoint sp(0, 0);
      CRect sr(size.left + splitpoint, size.top, size.right, size.bottom);
      cdisurf->commit();
      cdisurf->draw(dc, sr, sp);
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
      
      int minSamples = ( 1 << 3 ) * (int)( boxo.right - boxo.left );
      int totalSamples = std::max( (int)minSamples, (int)(totalEnvTime * samplerate / BLOCK_SIZE) );
      float drawnTime = totalSamples * samplerate_inv * BLOCK_SIZE;

      // OK so lets assume we want about 1000 pixels worth tops in
      int averagingWindow = (int)(totalSamples/1000.0) + 1;

#if LINUX
      float valScale = 10000.0;
#else
      float valScale = 100.0;
#endif
      int susCountdown = -1;
      
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
         }
         else
         {
             if( maxval - minval > 0.2 )
             {
                 minval  = ( ( - minval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
                 maxval  = ( ( - maxval + 1.0f ) * 0.5 * 0.8 + 0.1 ) * valScale;
                 path->addLine(xc, minval );
                 path->addLine(xc, maxval );
             }
             else
             {
                 path->addLine(xc, val );
             }
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

      // find time delta
      int maxNumLabels = 5;
      std::vector<float> timeDeltas = { 0.5, 1.0, 2.5, 5.0, 10.0 };
      auto currDelta = timeDeltas.begin();
      while( currDelta != timeDeltas.end() && ( drawnTime / *currDelta ) > maxNumLabels )
      {
          currDelta ++;
      }
      float delta = timeDeltas.back();
      if( currDelta != timeDeltas.end() )
          delta = *currDelta;

      int nLabels = (int)(drawnTime / delta) + 1;
      float dx = delta / drawnTime * valScale;

      for( int l=0; l<nLabels; ++l )
      {
          float xp = dx * l;
          float yp = valScale * 0.9;
#if LINUX
          yp = valScale * 0.95;
#endif 
          CRect tp(CPoint(xp + 1,yp), CPoint(10,10));
          tf.transform(tp);
          dc->setFontColor(VSTGUI::kBlackCColor);
          dc->setFont(lfoTypeFont);
          char txt[256];
          float tv = delta * l;
          if( fabs(roundf(tv) - tv) < 0.05 )
              snprintf(txt, 256, "%d s", (int)round(delta * l) );
          else
              snprintf(txt, 256, "%.1f s", delta * l );
          dc->drawString(txt, tp, VSTGUI::kLeftText, true );

          CPoint sp(xp, valScale * 0.95), ep(xp, valScale * 0.9 );
          tf.transform(sp);
          tf.transform(ep);
          dc->setLineWidth(1.0);
          dc->setFrameColor(VSTGUI::kBlackCColor);
          dc->drawLine(sp,ep);
      }

      
      if( drawBeats )
      {
         auto bpm = storage->temposyncratio * 120;

         auto denFactor = 4.0 / tsDen;
         auto beatsPerSecond = bpm / 60.0;
         auto secondsPerBeat = 1 / beatsPerSecond;
         auto deltaBeat = secondsPerBeat / drawnTime * valScale * denFactor;

         int nBeats = drawnTime * beatsPerSecond / denFactor;

         auto measureLen = deltaBeat * tsNum;
         int everyMeasure = 1;
         while( measureLen < 5 ) // a hand selected parameter by playing around with it, tbh
         {
            measureLen *= 2;
            everyMeasure *= 2;
         }
         
         for( auto l=0; l<nBeats; ++l )
         {
            auto xp = deltaBeat * l;
            if( l % ( tsNum * everyMeasure ) == 0 )
            {
               auto soff = 0.0;
               if( l > 10 ) soff = 0.0;
               CPoint mp( xp + deltaBeat * ( tsNum - 0.5 ), valScale * 0.01 ),
                  mps( xp +  deltaBeat * soff, valScale * 0.01 ),
                  sp(xp, valScale * (.01)),
                  ep(xp, valScale * (.1) ),
                  vruleS(xp, valScale * .15 ),
                  vruleE(xp, valScale * .85 );
               tf.transform(sp);
               tf.transform(ep);
               tf.transform(mp);
               tf.transform(mps);
               tf.transform(vruleS);
               tf.transform(vruleE);
               dc->setFrameColor(VSTGUI::kBlackCColor);
               dc->setLineWidth(1.0);
               dc->drawLine(sp,ep);
               dc->setLineWidth(1.0);
               dc->setFrameColor(VSTGUI::CColor(0xE0, 0x80, 0x00));
               // dc->drawLine(mps,mp); // this draws the hat on the bar which I decided to skip
               dc->drawLine(vruleS, vruleE );

               auto mnum = l / tsNum;
               char s[256];
               sprintf(s, "%d", l+1 );
               
               CRect tp(CPoint(xp + 1, valScale * 0.0), CPoint(10,10));
               tf.transform(tp);
               dc->setFontColor(VSTGUI::kBlackCColor);
               dc->setFont(lfoTypeFont);
               dc->drawString(s, tp, VSTGUI::kLeftText, true );
            }
            else if( everyMeasure == 1 )
            {
               CPoint sp(xp, valScale * (.06)), ep(xp, valScale * (.1) );
               tf.transform(sp);
               tf.transform(ep);
               dc->setLineWidth(0.5);
               if( l % tsNum == 0 )
                  dc->setFrameColor(VSTGUI::kBlackCColor );
               else
                  dc->setFrameColor(VSTGUI::CColor(0xB0, 0x60, 0x00 ) );
               dc->drawLine(sp,ep);
            }
         }
         
      }
   
      // OK so draw the rules
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
      dc->setFrameColor(VSTGUI::CColor(0xE0, 0x80, 0x00));
      dc->drawLine(mid0, mid1);
      
      dc->setLineWidth(1.0);
      dc->setFrameColor(VSTGUI::CColor(0xE0, 0x80, 0x00));
      dc->drawLine(top0, top1);
      dc->drawLine(bot0, bot1);


#if LINUX
      dc->setLineWidth(100.0);
#else
      dc->setLineWidth(1.0);
#endif
      dc->setFrameColor(VSTGUI::CColor(0xB0, 0x60, 0x00, 0xFF));
      dc->drawGraphicsPath(eupath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );
      dc->drawGraphicsPath(edpath, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );

#if LINUX
      dc->setLineWidth(100.0);
#else
      dc->setLineWidth(1.3);
#endif
      dc->setFrameColor(VSTGUI::CColor(0x00, 0x00, 0, 0xFF));
      dc->drawGraphicsPath(path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tfpath );



      dc->restoreGlobalState();
      path->forget();
      eupath->forget();
      edpath->forget();
   }

   CColor cskugga = {0x5d, 0x5d, 0x5d, 0xff};
   CColor cgray = {0x97, 0x98, 0x9a, 0xff};
   CColor cselected = {0xfe, 0x98, 0x15, 0xff};
   // CColor blackColor (0, 0, 0, 0);
   dc->setFrameColor(cskugga);
   dc->setFont(lfoTypeFont);

   rect_shapes = leftpanel;
   for (int i = 0; i < n_lfoshapes; i++)
   {
      CRect tb(leftpanel);
      tb.top = leftpanel.top + 10 * i;
      tb.bottom = tb.top + 10;
      if (i == lfodata->shape.val.i)
      {
         CRect tb2(tb);
         tb2.left++;
         tb2.top += 0.5;
         tb2.inset(2, 1);
         tb2.offset(0, 1);
         dc->setFillColor(cselected);
         dc->drawRect(tb2, kDrawFilled);
      }
      // else dc->setFillColor(cgray);
      // dc->fillRect(tb);
      shaperect[i] = tb;
      // tb.offset(0,-1);
      dc->setFontColor(kBlackCColor);
      tb.top += 1.6; // now the font is smaller and the box is square, smidge down the text
      dc->drawString(ls_abberations[i], tb);
   }

   if (ss && lfodata->shape.val.i == ls_stepseq)
   {
      ss_shift_left = leftpanel;
      ss_shift_left.offset(53, 23);
      ss_shift_left.right = ss_shift_left.left + 12;
      ss_shift_left.bottom = ss_shift_left.top + 34;

      dc->setFillColor(cskugga);
      dc->drawRect(ss_shift_left, kDrawFilled);
      ss_shift_left.inset(1, 1);
      ss_shift_left.bottom = ss_shift_left.top + 16;

      dc->setFillColor(cgray);
      dc->drawRect(ss_shift_left, kDrawFilled);
      drawtri(ss_shift_left, dc, -1);

      ss_shift_right = ss_shift_left;
      ss_shift_right.offset(0, 16);
      dc->setFillColor(cgray);
      dc->drawRect(ss_shift_right, kDrawFilled);
      drawtri(ss_shift_right, dc, 1);
      // ss_shift_left,ss_shift_right;
   }

   setDirty(false);
}

void CLFOGui::drawBitmap(CDrawContext* dc)
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

   cdisurf->begin();
#if MAC
   cdisurf->clear(0x0090ffff);
#else
   cdisurf->clear(0xffff9000);
#endif
   int w = cdisurf->getWidth();
   int h = cdisurf->getHeight();

   if (ss && lfodata->shape.val.i == ls_stepseq)
   {
      // I know I could do the math to convert these colors but I would rather leave them as
      // literals for the compiler so we don't have to shift them at runtime. See issue #141 in
      // surge github
#if MAC
#define PIX_COL(a, b) b
#else
#define PIX_COL(a, b) a
#endif
      // Step Sequencer Colors. Remember mac is 0xRRGGBBAA and mac is 0xAABBGGRR
      int stepMarker = PIX_COL(0xff000000, 0x000000ff);
      int disStepMarker = PIX_COL( 0xffaaaaaa, 0xaaaaaaff);
      int loopRegionHi = PIX_COL(0xffc6e9c4, 0xc4e9c6ff);
      int loopRegionLo = PIX_COL(0xffb6d9b4, 0xb4d9b6ff);
      int noLoopHi = PIX_COL(0xffdfdfdf, 0xdfdfdfff);
      int noLoopLo = PIX_COL(0xffcfcfcf, 0xcfcfcfff);
      int grabMarker =
          PIX_COL(0x00087f00, 0x007f08ff); // Surely you can't mean this to be fully transparent?
                                           // But leave non-mac unch

      for (int i = 0; i < n_stepseqsteps; i++)
      {
         CRect rstep(maindisp), gstep;
         rstep.offset(-size.left - splitpoint, -size.top);
         rstep.left += scale * i;
         rstep.right = rstep.left + scale - 1;
         rstep.bottom -= margin2 + 1;
         CRect shadow(rstep);
         shadow.inset(-1, -1);
         cdisurf->fillRect(shadow, skugga);
         if (edit_trigmask)
         {
            gstep = rstep;
            rstep.top += margin2;
            gstep.bottom = rstep.top - 1;
            gaterect[i] = gstep;
            gaterect[i].offset(size.left + splitpoint, size.top);

            if (ss->trigmask & (1L << i))
               cdisurf->fillRect(gstep, stepMarker);
            else if( ss->trigmask & ( 1L << ( 16 + i ) ) )
            {
               // FIXME - an A or an F would be nice eh?
               cdisurf->fillRect(gstep, disStepMarker);
               auto qrect = gstep;
               qrect.right -= (qrect.getWidth() / 2 );
               cdisurf->fillRect(qrect, stepMarker);
            }
            else if( ss->trigmask & ( 1L << ( 32 + i ) ) )
            {
               cdisurf->fillRect(gstep, disStepMarker);
               auto qrect = gstep;
               qrect.left += (qrect.getWidth() / 2 );
               cdisurf->fillRect(qrect, stepMarker);
            }

            else if ((i >= ss->loop_start) && (i <= ss->loop_end))
               cdisurf->fillRect(gstep, (i & 3) ? loopRegionHi : loopRegionLo);
            else
               cdisurf->fillRect(gstep, (i & 3) ? noLoopHi : noLoopLo);
         }
         if ((i >= ss->loop_start) && (i <= ss->loop_end))
            cdisurf->fillRect(rstep, (i & 3) ? loopRegionHi : loopRegionLo);
         else
            cdisurf->fillRect(rstep, (i & 3) ? noLoopHi : noLoopLo);
         steprect[i] = rstep;
         steprect[i].offset(size.left + splitpoint, size.top);
         CRect v(rstep);
         int p1, p2;
         if (lfodata->unipolar.val.b)
         {
            v.top = v.bottom - (int)(v.getHeight() * ss->steps[i]);
         }
         else
         {
            p1 = v.bottom - (int)((float)0.5f + v.getHeight() * (0.5f + 0.5f * ss->steps[i]));
            p2 = (v.bottom + v.top) * 0.5;
            v.top = min(p1, p2);
            v.bottom = max(p1, p2) + 1;
         }
         // if (p1 == p2) p2++;
         cdisurf->fillRect(v, stepMarker);
      }

      rect_steps = steprect[0];
      rect_steps.right = steprect[n_stepseqsteps - 1].right;
      rect_steps_retrig = gaterect[0];
      rect_steps_retrig.right = gaterect[n_stepseqsteps - 1].right;

      rect_ls = maindisp;
      rect_ls.offset(-size.left - splitpoint, -size.top);
      rect_ls.top = rect_ls.bottom - margin2;
      rect_le = rect_ls;

      rect_ls.left += scale * ss->loop_start - 1;
      rect_ls.right = rect_ls.left + margin2;
      rect_le.right = rect_le.left + scale * (ss->loop_end + 1);
      rect_le.left = rect_le.right - margin2;

      cdisurf->fillRect(rect_ls, grabMarker);
      cdisurf->fillRect(rect_le, grabMarker);
      CRect linerect(rect_ls);
      linerect.top = maindisp.top - size.top;
      linerect.right = linerect.left + 1;
      cdisurf->fillRect(linerect, grabMarker);
      linerect = rect_le;
      linerect.top = maindisp.top - size.top;
      linerect.left = linerect.right - 1;
      cdisurf->fillRect(linerect, grabMarker);

      rect_ls.offset(size.left + splitpoint, size.top);
      rect_le.offset(size.left + splitpoint, size.top);
   }
   else
   {
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

      LfoModulationSource* tlfo = new LfoModulationSource();
      tlfo->assign(storage, lfodata, tp, 0, ss, true);
      tlfo->attack();
      CRect boxo(maindisp);
      boxo.offset(-size.left - splitpoint, -size.top);

      unsigned int column[512];
      int column_d[512];
      int h2 = cdisurf->getHeight();
      assert(h < 512);
      float lastval = 0;

      int midline = (int)((float)((0.5f + 0.4f * 0.f) * h));
      int topline = (int)((float)((0.5f + 0.4f * 1.f) * h));
      int bottomline = (int)((float)((0.5f + 0.4f * -1.f) * h)) + 1;
      const int aa_bs = 3;
      const int aa_samples = 1 << aa_bs;
      int last_imax = -1, last_imin = -1;

      for (int y = 0; y < h2; y++)
         column_d[y] = 0;

      for (int x = boxo.left; x < boxo.right; x++)
      {
         for (int y = 0; y < h2; y++)
            column[y] = 0;

         /*for(int i=0; i<16; i++) tlfo->process_block();
         int p = h - (int)((float)0.5f + h * (0.5f + 0.5f*tlfo->output));
         cdisurf->setPixel(x,p,0x00000000);*/

         for (int s = 0; s < aa_samples; s++)
         {
            tlfo->process_block();

            float val = -tlfo->output;
            val = (float)((0.5f + 0.4f * val) * h);
            lastval = val;
            float v_min = val;
            float v_max = val;

            v_min = val - 0.5f;
            v_max = val + 0.5f;
            int imin = (int)v_min;
            int imax = (int)v_max;
            float imax_frac = (v_max - imax);
            float imin_frac = 1 - (v_min - imin);

            // envelope outline
            float eval = tlfo->env_val * lfodata->magnitude.val.f;
            unsigned int ieval1 = (int)((float)((0.5f + 0.4f * eval) * h * 128));
            unsigned int ieval2 = (int)((float)((0.5f - 0.4f * eval) * h * 128));

            if ((ieval1 >> 7) < (h - 1))
            {
               column[ieval1 >> 7] += 128 - (ieval1 & 127);
               column[(ieval1 >> 7) + 1] += ieval1 & 127;
            }

            if ((ieval2 >> 7) < (h - 1))
            {
               column[ieval2 >> 7] += 128 - (ieval2 & 127);
               column[(ieval2 >> 7) + 1] += ieval2 & 127;
            }

            if (imax == imin)
               imax++;

            if (imin < 0)
               imin = 0;
            int dimax = imax, dimin = imin;

            if ((x > boxo.left) || (s > 0))
            {
               if (dimin > last_imax)
                  dimin = last_imax;
               else if (dimax < last_imin)
                  dimax = last_imin;
            }
            dimin = limit_range(dimin, 0, h);
            dimax = limit_range(dimax, 0, h);

            // int yp = (int)((float)(size.height() * (-osc->output[block_pos]*0.5+0.5)));
            // yp = limit_range(yp,0,h-1);

            column[dimin] += ((int)((float)imin_frac * 255.f));
            column[dimax + 1] += ((int)((float)imax_frac * 255.f));
            for (int b = (dimin + 1); b < (dimax + 1); b++)
            {
               column_d[b & 511] = aa_samples;
            }
            last_imax = imax;
            last_imin = imin;

            for (int y = 0; y < h; y++)
            {
               if (column_d[y] > 0)
                  column[y] += 256;
               column_d[y]--;
            }
         }
         column[midline] = max((unsigned int)128 << aa_bs, column[midline]);
         column[topline] = max((unsigned int)32 << aa_bs, column[topline]);
         column[bottomline] = max((unsigned int)32 << aa_bs, column[bottomline]);

         for (int y = 0; y < h2; y++)
         {
            cdisurf->setPixel(x, y,
                              coltable[min((unsigned int)255, /*(y>>1) +*/ (column[y] >> aa_bs))]);
         }
      }
      delete tlfo;
   }

   CPoint sp(0, 0);
   CRect sr(size.left + splitpoint, size.top, size.right, size.bottom);
   cdisurf->commit();
   cdisurf->draw(dc, sr, sp);

   /*if(lfodata->shape.val.i != ls_stepseq)
   {
           CRect tr(size);
           tr.x = tr.x2 - 60;
           tr.y2 = tr.y + 50;
           CColor ctext = {224,126,0,0xff};
           dc->setFontColor(ctext);
           dc->setFont(patchNameFont);
           dc->drawString("LFO 1",tr,false,kCenterText);
   }*/

   CColor cskugga = {0x5d, 0x5d, 0x5d, 0xff};
   CColor cgray = {0x97, 0x98, 0x9a, 0xff};
   CColor cselected = {0xfe, 0x98, 0x15, 0xff};
   // CColor blackColor (0, 0, 0, 0);
   dc->setFrameColor(cskugga);
   dc->setFont(lfoTypeFont);

   rect_shapes = leftpanel;
   for (int i = 0; i < n_lfoshapes; i++)
   {
      CRect tb(leftpanel);
      tb.top = leftpanel.top + 10 * i;
      tb.bottom = tb.top + 10;
      if (i == lfodata->shape.val.i)
      {
         CRect tb2(tb);
         tb2.left++;
         tb2.top += 0.5;
         tb2.inset(2, 1);
         tb2.offset(0, 1);
         dc->setFillColor(cselected);
         dc->drawRect(tb2, kDrawFilled);
      }
      // else dc->setFillColor(cgray);
      // dc->fillRect(tb);
      shaperect[i] = tb;
      // tb.offset(0,-1);
      dc->setFontColor(kBlackCColor);
      tb.top += 1.6; // now the font is smaller and the box is square, smidge down the text
      dc->drawString(ls_abberations[i], tb);
   }

   if (ss && lfodata->shape.val.i == ls_stepseq)
   {
      ss_shift_left = leftpanel;
      ss_shift_left.offset(53, 23);
      ss_shift_left.right = ss_shift_left.left + 12;
      ss_shift_left.bottom = ss_shift_left.top + 34;

      dc->setFillColor(cskugga);
      dc->drawRect(ss_shift_left, kDrawFilled);
      ss_shift_left.inset(1, 1);
      ss_shift_left.bottom = ss_shift_left.top + 16;

      dc->setFillColor(cgray);
      dc->drawRect(ss_shift_left, kDrawFilled);
      drawtri(ss_shift_left, dc, -1);

      ss_shift_right = ss_shift_left;
      ss_shift_right.offset(0, 16);
      dc->setFillColor(cgray);
      dc->drawRect(ss_shift_right, kDrawFilled);
      drawtri(ss_shift_right, dc, 1);
      // ss_shift_left,ss_shift_right;
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

};

CMouseEventResult CLFOGui::onMouseDown(CPoint& where, const CButtonState& buttons)
{
   if (1) //(buttons & kLButton))
   {
      if (ss && lfodata->shape.val.i == ls_stepseq)
      {
         if (rect_steps.pointInside(where))
         {
            controlstate = cs_steps;
            onMouseMoved(where, buttons);
            return kMouseEventHandled;
         }
         else if (rect_steps_retrig.pointInside(where))
         {
            controlstate = cs_trigtray_toggle;
            onMouseMoved(where, buttons);
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
            ss->trigmask = ( ((ss->trigmask & 0x000000000000fffe) >> 1) | ((ss->trigmask & 1) << 15) & 0xffff) |
               ( ((ss->trigmask & 0x00000000fffe0000) >> 1) | ((ss->trigmask & 0x10000) << 15) & 0xffff0000 ) |
               ( ((ss->trigmask & 0x0000fffe00000000) >> 1) | ((ss->trigmask & 0x100000000) << 15) & 0xffff00000000 );

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
            ss->trigmask = ( ((ss->trigmask & 0x0000000000007fff) << 1) | ((ss->trigmask & 0x0000000000008000) >> 15) & 0xffff ) |
               ( ((ss->trigmask & 0x000000007fff0000) << 1) | ((ss->trigmask & 0x0000000080000000) >> 15) & 0xffff0000 )|
               ( ((ss->trigmask & 0x00007fff00000000) << 1) | ((ss->trigmask & 0x0000800000000000) >> 15) & 0xffff00000000 );
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
   if (controlstate)
   {
      // onMouseMoved(where,buttons);
      controlstate = cs_null;
   }
   return kMouseEventHandled;
}

CMouseEventResult CLFOGui::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
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
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > steprect[i].left) && (where.x < steprect[i].right))
         {
            float f = (float)(steprect[i].bottom - where.y) / steprect[i].getHeight();
            if (buttons & (kControl | kRButton))
               f = 0;
            else if (lfodata->unipolar.val.b)
               f = limit_range(f, 0.f, 1.f);
            else
               f = limit_range(f * 2.f - 1.f, -1.f, 1.f);
            if (buttons & kShift)
            {
               f *= 12;
               f = floor(f);
               f *= (1.f / 12.f);
            }
            ss->steps[i] = f;
            invalid();
         }
      }
   }
   else if (controlstate == cs_trigtray_toggle)
   {
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > gaterect[i].left) && (where.x < gaterect[i].right))
         {
            bool bothOn = ss->trigmask & ( 1L << i );
            bool filtOn = ss->trigmask & ( 1L << ( 16 + i ) );
            bool ampOn = ss->trigmask & ( 1L << ( 32 + i ) );

            bool anyOn = bothOn || filtOn || ampOn;
            
            uint64_t off = 0, on = 0;
            if( bothOn )
            {
               off = 1L << i;
            }
            else if( filtOn )
            {
               off = 1L << ( 16 + i );
            }
            else if( ampOn )
            {
               off = 1L << ( 32 + i );
            }
            else
            {
               off = 0;
            }

            if( buttons & kShift )
            {
               if( bothOn )
               {
                  on = 1L << ( 16 + i );
               }
               else if( filtOn )
               {
                  on = 1L << ( 32 + i );
               }
               else if( ampOn )
               {
                  on = 0;
               }
               else
               {
                  on = 1L << i;
               }
            }
            else
            {
               if( ! anyOn )
               {
                  on = 1L << i;
               }
            }
            
            ss->trigmask = (ss->trigmask & ~off) | on;
            invalid();
         }
      }
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
