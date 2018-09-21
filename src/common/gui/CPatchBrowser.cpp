//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CPatchBrowser.h"

#include <vector>

extern CFontRef surge_minifont;
extern CFontRef surge_patchfont;

void CPatchBrowser::draw(CDrawContext* dc)
{
   dc->setFillColor(kBlackCColor);
   CRect size = getViewSize();
   CRect ar(size);
   ar.inset(1, 0);
   // dc->fillRect(ar);
   ar = size;
   ar.inset(0, 1);
   // dc->fillRect(ar);
   ar = size;
   ar.inset(2, 2);
   dc->setFillColor(kWhiteCColor);
   // dc->fillRect(ar);
   // ar.top += 2;
   CRect al(ar);
   // ar.left += 68;
   ar.left += 2;
   al.right = al.left + 150;
   al.left += 3;
   // al.top += 2;
   al.bottom = al.top + 12;
   dc->setFontColor(kBlackCColor);
   dc->setFont(surge_patchfont);
   dc->drawString(pname.c_str(), ar, kCenterText, true);

   dc->setFont(surge_minifont);
   dc->drawString(category.c_str(), al, kLeftText, true);
   al.offset(0, 12);
   dc->drawString(author.c_str(), al, kLeftText, true);

   setDirty(false);
}

CMouseEventResult CPatchBrowser::onMouseDown(CPoint& where, const CButtonState& button)
{
   if (!(button & kLButton || button & kRButton))
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;

   char txt[256];
   int splitcount = 256;

   CRect menurect(0, 0, 0, 0);
   menurect.offset(where.x, where.y);
   COptionMenu* contextMenu = new COptionMenu(menurect, 0, 0, 0, 0, kNoDrawStyle);

   int main_e = 0;
   // if RMB is down, only show the current category
   bool single_category = button & (kRButton | kControl);
   int last_category = current_category;
   if (single_category)
      contextMenu->setNbItemsPerColumn(32);

   for (int c = 0; c < storage->patch_category.size(); c++)
   {
      if ((!single_category) || (c == last_category))
      {
         if (!single_category &&
             ((c == storage->patch_category_split[0]) || (c == storage->patch_category_split[1])))
            contextMenu->addEntry("-");

         vector<int> ctge;
         for (int p = 0; p < storage->patch_list.size(); p++)
         {
            if (storage->patch_list[p].category == c)
            {
               ctge.push_back(p);
            }
         }

         // dela in kategorier med fler entries än splitcount i subkategorier ex. bass (1,2) etc etc
         int n_subc = 1 + (max(2, (int)ctge.size()) - 1) / splitcount;
         for (int subc = 0; subc < n_subc; subc++)
         {
            char name[256];
            COptionMenu* subMenu;
            if (single_category)
               subMenu = contextMenu;
            else
            {
               subMenu = new COptionMenu(getViewSize(), nullptr, main_e, 0, 0, kNoDrawStyle);
               subMenu->setNbItemsPerColumn(32);
            }

            int sub = 0;

            for (int i = subc * splitcount; i < min((subc + 1) * splitcount, (int)ctge.size()); i++)
            {
               int p = ctge[i];
               // sprintf(name,"%i. %s",p,storage->patch_list[p].name.c_str());
               sprintf(name, "%s", storage->patch_list[p].name.c_str());

               auto actionItem = new CCommandMenuItem(name);
               auto action = [this, p](CCommandMenuItem* item) { this->loadPatch(p); };

               actionItem->setActions(action, nullptr);
               subMenu->addEntry(actionItem);
               sub++;
            }

            if (n_subc > 1)
               sprintf(name, "%s - %i", storage->patch_category[c].name.c_str(), subc + 1);
            else
               strncpy(name, storage->patch_category[c].name.c_str(), namechars);

            if (!single_category)
            {
               contextMenu->addEntry(subMenu, name);
               subMenu->forget(); // viktigt, så att refcounter blir rätt
            }
            main_e++;
         }
      }
   }
   // contextMenu->addEntry("refresh list");

   getFrame()->addView(contextMenu); // add to frame
   contextMenu->setDirty();
   contextMenu->onMouseDown(where, kLButton);
}

void CPatchBrowser::loadPatch(int id)
{
   if (listener && (id >= 0))
   {
      sel_id = id;
      listener->valueChanged(this);
   }
}
