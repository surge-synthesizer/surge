//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "CDIBitmap.h"
#include "DspUtilities.h"

class COscillatorDisplay : public VSTGUI::CControl
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

      /*f_bgcol[0] = powf(f_bgcol[0],2.2f);
      f_bgcol[1] = powf(f_bgcol[1],2.2f);
      f_bgcol[2] = powf(f_bgcol[2],2.2f);*/
      f_fgcol[0] = powf(f_fgcol[0], 2.2f);
      f_fgcol[1] = powf(f_fgcol[1], 2.2f);
      f_fgcol[2] = powf(f_fgcol[2], 2.2f);

      for (int i = 0; i < 256; i++)
      {
         float x = i * sc;
         // unsigned int a = limit_range((unsigned int)((float)255.f*powf(x,1.f/2.2f)),0,255);
         /*unsigned int a = limit_range((int)((float)255.f*powf(x*f_fgcol[0] +
         (1.f-x)*f_bgcol[0],1.f/2.2f)),0,255); unsigned int b =
         limit_range((int)((float)255.f*powf(x*f_fgcol[1] + (1.f-x)*f_bgcol[1],1.f/2.2f)),0,255);
         unsigned int c = limit_range((int)((float)255.f*powf(x*f_fgcol[2] +
         (1.f-x)*f_bgcol[2],1.f/2.2f)),0,255);			*/
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
          coltable[ i ] = ( b << 8 ) | ( g << 16 ) | ( r << 24 ) | a;
#else
          coltable[i] = r | (g << 8) | (b << 16) | (a << 24);
#endif

      }
   }
   virtual ~COscillatorDisplay()
   {
      delete cdisurf;
   }
   virtual void draw(VSTGUI::CDrawContext* dc);
   virtual bool onDrop(VSTGUI::IDataPackage* drag, const VSTGUI::CPoint& where);

   void loadWavetable(int id);

   // virtual void mouse (CDrawContext *pContext, VSTGUI::CPoint &where, long button = -1);
   virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseUp(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual VSTGUI::CMouseEventResult onMouseMoved(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);

protected:
   void populateMenu(VSTGUI::COptionMenu* m, int selectedItem);
   bool populateMenuForCategory(VSTGUI::COptionMenu* parent, int categoryId, int selectedItem);

   OscillatorStorage* oscdata;
   SurgeStorage* storage;
   unsigned int coltable[256], controlstate;
   CDIBitmap* cdisurf;
   VSTGUI::CRect rnext, rprev, rmenu;
   VSTGUI::CPoint lastpos;
   CLASS_METHODS(COscillatorDisplay, VSTGUI::CControl)
};
