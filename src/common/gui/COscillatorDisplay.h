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

#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"
#include "SkinSupport.h"

#define OSC_MOD_ANIMATION 0

class COscillatorDisplay : public VSTGUI::CControl, public VSTGUI::IDropTarget, public Surge::UI::SkinConsumingComponnt
{
public:
   COscillatorDisplay(const VSTGUI::CRect& size, OscillatorStorage* oscdata, SurgeStorage* storage)
       : VSTGUI::CControl(size, 0, 0, 0)
   {
      this->oscdata = oscdata;
      this->storage = storage;
      controlstate = 0;

      cdisurf = new CDIBitmap(getWidth(), getHeight());

      int bgcol = 0xff161616;
      int fgcol = 0x00ff9000;
      float f_bgcol[4], f_fgcol[4];
      const float sc = (1.f / 255.f);
      f_bgcol[0] = (bgcol & 0xff) * sc;
      f_fgcol[0] = (fgcol & 0xff) * sc;
      f_bgcol[1] = ((bgcol >> 8) & 0xff) * sc;
      f_fgcol[1] = ((fgcol >> 8) & 0xff) * sc;
      f_bgcol[2] = ((bgcol >> 16) & 0xff) * sc;
      f_fgcol[2] = ((fgcol >> 16) & 0xff) * sc;

      f_fgcol[0] = powf(f_fgcol[0], 2.2f);
      f_fgcol[1] = powf(f_fgcol[1], 2.2f);
      f_fgcol[2] = powf(f_fgcol[2], 2.2f);

      for (int i = 0; i < 256; i++)
      {
         float x = i * sc;
         unsigned int r =
             limit_range((int)((float)255.f * (1.f - (1.f - powf(x * f_fgcol[0], 1.f / 2.2f)) *
                                                         (1.f - f_bgcol[0]))),
                         0, 255);
         unsigned int g =
             limit_range((int)((float)255.f * (1.f - (1.f - powf(x * f_fgcol[1], 1.f / 2.2f)) *
                                                         (1.f - f_bgcol[1]))),
                         0, 255);
         unsigned int b =
             limit_range((int)((float)255.f * (1.f - (1.f - powf(x * f_fgcol[2], 1.f / 2.2f)) *
                                                         (1.f - f_bgcol[2]))),
                         0, 255);
         unsigned int a = 0xff;

#if MAC
         // MAC uses a different raw pixel byte order than windows
         coltable[i] = (b << 8) | (g << 16) | (r << 24) | a;
#else
         coltable[i] = r | (g << 8) | (b << 16) | (a << 24);
#endif
      }
   }
   virtual ~COscillatorDisplay()
   {
      delete cdisurf;
   }
   virtual void draw(VSTGUI::CDrawContext* dc) override;

   virtual VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override
   {
       doingDrag = true;
       /* invalid();
          setDirty(true); */

       return VSTGUI::DragOperation::Copy;
   }
   virtual VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override
   {
       return VSTGUI::DragOperation::Copy;
   }
   virtual void onDragLeave(VSTGUI::DragEventData data) override
   {
       doingDrag = false;
       /* invalid();
          setDirty(true); */
   }
   virtual bool onDrop(VSTGUI::DragEventData data) override;
   
   virtual VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget () override { return this; }

   void loadWavetable(int id);
   void loadWavetableFromFile();

   // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override;

   void invalidateIfIdIsInRange(int id);
   
#if OSC_MOD_ANIMATION
   void setIsMod(bool b)
   {
      is_mod = b;
      mod_time = 0;
   }
   void setModSource(modsources m)
   {
       modsource = m;
   }
   void tickModTime()
   {
      mod_time += 1.0 / 30.0;
   }
#endif

protected:
   void populateMenu(VSTGUI::COptionMenu* m, int selectedItem);
   bool populateMenuForCategory(VSTGUI::COptionMenu* parent, int categoryId, int selectedItem);

   OscillatorStorage* oscdata;
   SurgeStorage* storage;
   unsigned int controlstate;
   unsigned int coltable[256];
   CDIBitmap* cdisurf;

   bool doingDrag = false;

#if OSC_MOD_ANIMATION
   bool is_mod = false;
   modsources modsource = ms_original;
   float mod_time = 0;
#endif
   VSTGUI::CRect rnext, rprev, rmenu;
   VSTGUI::CPoint lastpos;
   CLASS_METHODS(COscillatorDisplay, VSTGUI::CControl)
};
