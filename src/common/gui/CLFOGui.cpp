//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CLFOGui.h"
#include "LfoModulationSource.h"

using namespace VSTGUI;

extern CFontRef surge_minifont;
extern CFontRef surge_patchfont;

// At a later date, fix this along with the other font issues between platforms. Keep this
// in this unit only.
// See github issue #295. The minifont is 1pt too big so shrink it by 1 px.
// Ideally we would shift all fonts to a bundled lato but that requires us
// to resolve github issue #214
#if MAC
SharedPointer<CFontDesc> lfoLabelFont = new CFontDesc("Lucida Grande", 8); // one smaller than minifont
#else
SharedPointer<CFontDesc> lfoLabelFont = new CFontDesc("Microsoft Sans Serif", 8);
#endif

CFontRef surge_lfoLabelFont = lfoLabelFont;

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
       // I know I could do the math to convert these colors but I would rather leave them as literals for the compiler
       // so we don't have to shift them at runtime. See issue #141 in surge github
#if MAC
#define PIX_COL( a, b ) b
#else
#define PIX_COL( a, b ) a
#endif
       // Step Sequencer Colors. Remember mac is 0xRRGGBBAA and mac is 0xAABBGGRR
       int stepMarker = PIX_COL( 0xff000000, 0x000000ff);
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

            if (ss->trigmask & (1 << i))
               cdisurf->fillRect(gstep, stepMarker);
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
           dc->setFont(surge_patchfont);
           dc->drawString("LFO 1",tr,false,kCenterText);
   }*/

   CColor cskugga = {0x5d, 0x5d, 0x5d, 0xff};
   CColor cgray = {0x97, 0x98, 0x9a, 0xff};
   CColor cselected = {0xfe, 0x98, 0x15, 0xff};
   // CColor blackColor (0, 0, 0, 0);
   dc->setFrameColor(cskugga);
   dc->setFont(surge_lfoLabelFont);

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
   cs_trigtray_false,
   cs_trigtray_true,
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
            bool gatestate = false;
            for (int i = 0; i < (n_stepseqsteps); i++)
            {
               if (gaterect[i].pointInside(where))
               {
                  gatestate = ss->trigmask & (1 << i);
               }
            }
            controlstate = gatestate ? cs_trigtray_true : cs_trigtray_false;
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
            ss->trigmask = ((ss->trigmask & 0xfffe) >> 1) | ((ss->trigmask & 1) << 15);
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
            ss->trigmask = ((ss->trigmask & 0x7fff) << 1) | ((ss->trigmask & 0x8000) >> 15);
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
   else if ((controlstate == cs_trigtray_false) || (controlstate == cs_trigtray_true))
   {
      for (int i = 0; i < n_stepseqsteps; i++)
      {
         if ((where.x > gaterect[i].left) && (where.x < gaterect[i].right))
         {
            unsigned int m = 1 << i;
            unsigned int minv = m ^ 0xffffffff;
            ss->trigmask =
                (ss->trigmask & minv) |
                (((controlstate == cs_trigtray_true) || (buttons & (kControl | kRButton))) ? 0 : m);
            invalid();
         }
      }
   }
   return kMouseEventHandled;
}
