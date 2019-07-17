//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "COscillatorDisplay.h"
#include "Oscillator.h"
#include <time.h>
#include "unitconversion.h"
#include "UserInteractions.h"
#if MAC
#include "filesystem.h"
#elif LINUX
#include "experimental/filesystem"
#else
#include "filesystem"
#endif

using namespace VSTGUI;

namespace fs = std::experimental::filesystem;

const float disp_pitch = 90.15f - 48.f;
const int wtbheight = 12;

extern CFontRef displayFont;

void COscillatorDisplay::draw(CDrawContext* dc)
{
   pdata tp[n_scene_params];
   tp[oscdata->pitch.param_id_in_scene].f = 0;
   for (int i = 0; i < n_osc_params; i++)
      tp[oscdata->p[i].param_id_in_scene].i = oscdata->p[i].val.i;
   Oscillator* osc = spawn_osc(oscdata->type.val.i, storage, oscdata, tp);

   cdisurf->begin();
   cdisurf->clear(0xffffffff);

   int h2 = cdisurf->getHeight();
   int h = h2;
   assert(h < 512);
   if (uses_wavetabledata(oscdata->type.val.i))
      h -= wtbheight;

   unsigned int column[512];
   int column_d[512];
   float lastval = 0;
   int midline = h >> 1;
   int topline = midline - 0.4f * h;
   int bottomline = midline + 0.4f * h;
   const int aa_bs = 4;
   const int aa_samples = 1 << aa_bs;
   int last_imax = -1, last_imin = -1;
   if (osc)
   {
      // srand(2);
      float disp_pitch_rs = disp_pitch + 12.0 * log2(dsamplerate / 44100.0);
      bool use_display = osc->allow_display();

      // Mis-install check #2
      if (uses_wavetabledata(oscdata->type.val.i) && storage->wt_list.size() == 0)
         use_display = false;

      if (use_display)
         osc->init(disp_pitch_rs, true);
      int block_pos = BLOCK_SIZE_OS;
      for (int y = 0; y < h2; y++)
         column_d[y] = 0;

      for (int x = 0; x < getWidth(); x++)
      {
         for (int y = 0; y < h2; y++)
            column[y] = 0;
         for (int s = 0; s < aa_samples; s++)
         {
            if (use_display && (block_pos >= BLOCK_SIZE_OS))
            {
               if (uses_wavetabledata(oscdata->type.val.i))
               {
                  storage->CS_WaveTableData.enter();
                  osc->process_block(disp_pitch_rs);
                  block_pos = 0;
                  storage->CS_WaveTableData.leave();
               }
               else
               {
                  osc->process_block(disp_pitch_rs);
                  block_pos = 0;
               }
            }

            float val = 0.f;
            if (use_display)
               val = -osc->output[block_pos];

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

            if (imax == imin)
               imax++;

            if (imin < 0)
               imin = 0;
            int dimax = imax, dimin = imin;

            if ((x > 0) || (s > 0))
            {
               if (dimin > last_imax)
                  dimin = last_imax;
               else if (dimax < last_imin)
                  dimax = last_imin;
            }
            dimin = limit_range(dimin, 0, h - 1);
            dimax = limit_range(dimax, 0, h - 1);

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
            block_pos++;
            for (int y = 0; y < h; y++)
            {
               if (column_d[y] > 0)
                  column[y] += 256;
               column_d[y]--;
            }
         }
         column[midline] = max((unsigned int)64 << aa_bs, column[midline]);
         column[topline] = max((unsigned int)32 << aa_bs, column[topline]);
         column[bottomline] = max((unsigned int)32 << aa_bs, column[bottomline]);
         for (int y = 0; y < h2; y++)
         {
            cdisurf->setPixel(x, y, coltable[min((unsigned int)255, (column[y] >> aa_bs))]);
         }
      }
      delete osc;
      // srand( (unsigned)time( NULL ) );
   }
   cdisurf->commit();
   auto size = getViewSize();
   cdisurf->draw(dc, size);

   if (uses_wavetabledata(oscdata->type.val.i))
   {
      CRect wtlbl(size);
      wtlbl.right -= 1;
      wtlbl.top = wtlbl.bottom - wtbheight;
      rmenu = wtlbl;
      rmenu.inset(14, 0);
      char wttxt[256];

      storage->CS_WaveTableData.enter();

      int wtid = oscdata->wt.current_id;
      if ((wtid >= 0) && (wtid < storage->wt_list.size()))
      {
         strcpy(wttxt, storage->wt_list.at(wtid).name.c_str());
      }
      else if (oscdata->wt.flags & wtf_is_sample)
      {
         strcpy(wttxt, "(Patch Sample)");
      }
      else
      {
         strcpy(wttxt, "(Patch Wavetable)");
      }

      storage->CS_WaveTableData.leave();

      char* r = strrchr(wttxt, '.');
      if (r)
         *r = 0;
      // VSTGUI::CColor fgcol = cdisurf->int_to_ccol(coltable[255]);
      VSTGUI::CColor fgcol = {0xff, 0xA0, 0x10, 0xff};
      dc->setFillColor(fgcol);
      dc->drawRect(rmenu, kDrawFilled);
      dc->setFontColor(kBlackCColor);
      dc->setFont(displayFont);
      // strupr(wttxt);
      dc->drawString(wttxt, rmenu, kCenterText, true);

      /*CRect wtlbl_status(size);
      wtlbl_status.bottom = wtlbl_status.top + wtbheight;
      dc->setFontColor(kBlackCColor);
      if(oscdata->wt.flags & wtf_is_sample) dc->drawString("IS
      SAMPLE",wtlbl_status,false,kRightText);*/

      rnext = wtlbl;
      rnext.left = rmenu.right; //+ 1;
      rprev = wtlbl;
      rprev.right = rmenu.left; //- 1;
      dc->setFillColor(fgcol);
      dc->drawRect(rprev, kDrawFilled);
      dc->drawRect(rnext, kDrawFilled);
      dc->setFrameColor(kBlackCColor);

      dc->saveGlobalState();

      dc->setDrawMode(kAntiAliasing);
      dc->setFillColor(kBlackCColor);
      VSTGUI::CDrawContext::PointList trinext;

      trinext.push_back(VSTGUI::CPoint(134, 170));
      trinext.push_back(VSTGUI::CPoint(139, 174));
      trinext.push_back(VSTGUI::CPoint(134, 178));
      dc->drawPolygon(trinext, kDrawFilled);

      VSTGUI::CDrawContext::PointList triprev;

      triprev.push_back(VSTGUI::CPoint(13, 170));
      triprev.push_back(VSTGUI::CPoint(8, 174));
      triprev.push_back(VSTGUI::CPoint(13, 178));

      dc->drawPolygon(triprev, kDrawFilled);

      dc->restoreGlobalState();
   }

   setDirty(false);
}

bool COscillatorDisplay::onDrop(IDataPackage* drag, const CPoint& where)
{
   uint32_t ct = drag->getCount();
   if (ct == 1)
   {
      IDataPackage::Type t = drag->getDataType(0);
      if (t == IDataPackage::kFilePath)
      {
         const void* fn;
         drag->getData(0, fn, t);
         const char* fName = static_cast<const char*>(fn);
         fs::path fPath(fName);
         if (_stricmp(fPath.extension().generic_string().c_str(), ".wt") != 0)
         {
            Surge::UserInteractions::promptError(
                std::string(
                    "Surge only supports drag-and-drop of .wt wavetables onto the oscillator. ") +
                    "You dropped a file with extension " + fPath.extension().generic_string(),
                "Please drag a valid file type");
         }
         else
         {
            strncpy(oscdata->wt.queue_filename, fName, 255);
         }
      }
      else
      {
         Surge::UserInteractions::promptError(
             "Surge only supports drag-and-drop of files onto the oscillator",
             "Please Drag a File");
      }
   }

   return true;
}

CMouseEventResult COscillatorDisplay::onMouseDown(CPoint& where, const CButtonState& button)
{
   if (!((button & kLButton) || (button & kRButton)))
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

   assert(oscdata);

   if (!uses_wavetabledata(oscdata->type.val.i) || (where.y < rmenu.top))
   {
      controlstate = 1;
      lastpos.x = where.x;
      return kMouseEventHandled;
   }
   else if (uses_wavetabledata(oscdata->type.val.i))
   {
      int id = oscdata->wt.current_id;
      if (rprev.pointInside(where))
      {
         id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);
         if (id >= 0)
            oscdata->wt.queue_id = id;
      }
      else if (rnext.pointInside(where))
      {
         id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);
         if (id >= 0)
            oscdata->wt.queue_id = id;
      }
      else if (rmenu.pointInside(where))
      {
         CRect menurect(0, 0, 0, 0);
         menurect.offset(where.x, where.y);
         COptionMenu* contextMenu =
             new COptionMenu(menurect, 0, 0, 0, 0, COptionMenu::kMultipleCheckStyle);

         populateMenu(contextMenu, id);

         getFrame()->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->popup();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
         // getFrame()->looseFocus(pContext);

         getFrame()->removeView(contextMenu, true); // remove from frame and forget
      }
   }
   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

void COscillatorDisplay::populateMenu(COptionMenu* contextMenu, int selectedItem)
{
   int idx = 0;
   bool needToAddSep = false;
   for (auto c : storage->wtCategoryOrdering)
   {
      if (idx == storage->firstThirdPartyWTCategory ||
          (idx == storage->firstUserWTCategory &&
           storage->firstUserWTCategory != storage->wt_category.size()))
      {
         needToAddSep = true; // only add if there's actually data coming so defer the add
      }

      idx++;

      PatchCategory cat = storage->wt_category[c];
      if (cat.numberOfPatchesInCategoryAndChildren == 0)
         continue;

      if (cat.isRoot)
      {
         if (needToAddSep)
         {
            contextMenu->addEntry("-");
            needToAddSep = false;
         }
         populateMenuForCategory(contextMenu, c, selectedItem);
      }
   }
}

bool COscillatorDisplay::populateMenuForCategory(COptionMenu* contextMenu,
                                                 int categoryId,
                                                 int selectedItem)
{
   char name[NAMECHARS];
   COptionMenu* subMenu =
       new COptionMenu(getViewSize(), 0, categoryId, 0, 0, COptionMenu::kMultipleCheckStyle);
   subMenu->setNbItemsPerColumn(32);
   int sub = 0;

   PatchCategory cat = storage->wt_category[categoryId];

   for (auto p : storage->wtOrdering)
   {
      if (storage->wt_list[p].category == categoryId)
      {
         sprintf(name, "%s", storage->wt_list[p].name.c_str());
         auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name));
         auto action = [this, p](CCommandMenuItem* item) { this->loadWavetable(p); };

         if (p == selectedItem)
            actionItem->setChecked(true);
         actionItem->setActions(action, nullptr);
         subMenu->addEntry(actionItem);

         sub++;
      }
   }

   bool selected = false;

   for (auto child : cat.children)
   {
      if (child.numberOfPatchesInCategoryAndChildren > 0)
      {
         // this isn't the best approach but it works
         int cidx = 0;
         for (auto& cc : storage->wt_category)
         {
            if (cc.name == child.name)
               break;
            cidx++;
         }

         bool subSel = populateMenuForCategory(subMenu, cidx, selectedItem);
         selected = selected || subSel;
      }
   }

   if (!cat.isRoot)
   {
#if WINDOWS
      std::string pathSep = "\\";
#else
      std::string pathSep = "/";
#endif
      std::string catName = storage->wt_category[categoryId].name;
      std::size_t sepPos = catName.find_last_of(pathSep);
      if (sepPos != std::string::npos)
      {
         catName = catName.substr(sepPos + 1);
      }
      strncpy(name, catName.c_str(), NAMECHARS);
   }
   else
   {
      strncpy(name, storage->wt_category[categoryId].name.c_str(), NAMECHARS);
   }

   CMenuItem* submenuItem = contextMenu->addEntry(subMenu, name);

   if (selected || (selectedItem >= 0 && storage->wt_list[selectedItem].category == categoryId))
   {
      selected = true;
      submenuItem->setChecked(true);
   }

   subMenu->forget(); // Important, so that the refcounter gets right
   return selected;
}

void COscillatorDisplay::loadWavetable(int id)
{
   if (id >= 0 && (id < storage->wt_list.size()))
   {
      oscdata->wt.queue_id = id;
   }
}

CMouseEventResult COscillatorDisplay::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   if (controlstate)
   {
      controlstate = 0;
   }
   return kMouseEventHandled;
}
CMouseEventResult COscillatorDisplay::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   if (controlstate)
   {
      /*oscdata->startphase.val.f -= 0.005f * (where.x - lastpos.x);
      oscdata->startphase.val.f = limit_range(oscdata->startphase.val.f,0.f,1.f);
      lastpos.x = where.x;
      invalid();*/
   }
   return kMouseEventHandled;
}
