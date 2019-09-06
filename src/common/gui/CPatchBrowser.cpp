//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#include "CPatchBrowser.h"

#include "UserInteractions.h"
#include <vector>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;
extern CFontRef patchNameFont;

void CPatchBrowser::draw(CDrawContext* dc)
{
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
   dc->setFontColor(kRedCColor);
   dc->setFont(patchNameFont);
   dc->drawString(pname.c_str(), ar, kCenterText, true);

   dc->setFont(displayFont);
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

   CRect menurect(0, 0, 0, 0);
   menurect.offset(where.x, where.y);
   COptionMenu* contextMenu = new COptionMenu(menurect, 0, 0, 0, 0, COptionMenu::kMultipleCheckStyle);

   int main_e = 0;
   // if RMB is down, only show the current category
   bool single_category = button & (kRButton | kControl);
   int last_category = current_category;

   if (single_category)
   {
      contextMenu->setNbItemsPerColumn(32);

      /*
      ** in the init scenario we don't have a category yet. Our two choices are 
      ** don't pop up the menu or pick one. I choose to pick one. If I can
      ** find the one called "Init" use that. Otherwise pick 0.
      */
      int rightMouseCategory = current_category;
      if (current_category < 0)
      {
          if (storage->patchCategoryOrdering.size() == 0)
              return kMouseEventHandled;
          for (auto c : storage->patchCategoryOrdering)
          {
              if (_stricmp(storage->patch_category[c].name.c_str(),"init")==0)
              {
                  rightMouseCategory = c;;
              }

          }
          if (rightMouseCategory<0)
          {
              rightMouseCategory = storage->patchCategoryOrdering[0];
          }
      }

      populatePatchMenuForCategory(rightMouseCategory,contextMenu,single_category,main_e,false);
   }
   else
   {
       for (int i = 0; i < storage->patch_category.size(); i++)
       {
           if ((!single_category) || (i == last_category))
           {
               if (!single_category &&
                   ((i == storage->firstThirdPartyCategory) ||
                    (i == storage->firstUserCategory)))
                   contextMenu->addEntry("-");

               // Remap index to the corresponding category in alphabetical order.
               int c = storage->patchCategoryOrdering[i];

               populatePatchMenuForCategory( c, contextMenu, single_category, main_e, true );
           }
       }
   }
   // contextMenu->addEntry("refresh list");

   contextMenu->addSeparator();
   auto contentItem = new CCommandMenuItem(CCommandMenuItem::Desc("Download Additional Content..."));
   auto contentAction = [](CCommandMenuItem *item)
       {
           Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge-synthesizer.github.io/wiki/Additional-Content");
       };
   contentItem->setActions(contentAction,nullptr);
   contextMenu->addEntry(contentItem);
   
   getFrame()->addView(contextMenu); // add to frame
   contextMenu->setDirty();
   contextMenu->popup();
   getFrame()->removeView(contextMenu, true); // remove from frame and forget

   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

bool CPatchBrowser::populatePatchMenuForCategory( int c, COptionMenu *contextMenu, bool single_category, int &main_e, bool rootCall )
{
    bool amIChecked = false;
    PatchCategory cat = storage->patch_category[ c ];
    if (rootCall && ! cat.isRoot) return false; // stop it going in the top menu which is a straight iteration
    if (cat.numberOfPatchesInCategoryAndChildren == 0)
       return false; // Don't do empty categories

    int splitcount = 256;
    // Go through the whole patch list in alphabetical order and filter
    // out only the patches that belong to the current category.
    vector<int> ctge;
    for (auto p : storage->patchOrdering)
    {
        if (storage->patch_list[p].category == c)
        {
            ctge.push_back(p);
        }
    }
    
    // Divide categories with more entries than splitcount into subcategories f.ex. bass (1,2) etc etc
    int n_subc = 1 + (max(2, (int)ctge.size()) - 1) / splitcount;
    for (int subc = 0; subc < n_subc; subc++)
    {
        char name[256];
        COptionMenu* subMenu;
        if (single_category)
            subMenu = contextMenu;
        else
        {
            subMenu = new COptionMenu(getViewSize(), nullptr, main_e, 0, 0, COptionMenu::kMultipleCheckStyle);
            subMenu->setNbItemsPerColumn(32);
        }
        
        int sub = 0;
        
        for (int i = subc * splitcount; i < min((subc + 1) * splitcount, (int)ctge.size()); i++)
        {
            int p = ctge[i];
            // sprintf(name,"%i. %s",p,storage->patch_list[p].name.c_str());
            sprintf(name, "%s", storage->patch_list[p].name.c_str());
            
            auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name));
            auto action = [this, p](CCommandMenuItem* item) { this->loadPatch(p); };
            
            if (p == current_patch)
            {
                actionItem->setChecked(true);
                amIChecked = true;
            }
            actionItem->setActions(action, nullptr);
            subMenu->addEntry(actionItem);
            sub++;
        }

        for (auto &childcat : cat.children)
        {
            // this isn't the best approach but it works 
            int idx = 0;
            for (auto &cc : storage->patch_category)
            {
                if (cc.name == childcat.name && cc.internalid == childcat.internalid) break;
                idx++;
            }
            bool checkedKid = populatePatchMenuForCategory( idx, subMenu, false, main_e, false );
            if(checkedKid)
            {
                amIChecked=true;
            }
        }
        
        std::string menuName = storage->patch_category[c].name;
        std::string pathSep = "/";
#if WINDOWS
        pathSep = "\\";
#endif
        if (menuName.find_last_of(pathSep) != string::npos)
            menuName = menuName.substr(menuName.find_last_of(pathSep) + 1);
        
        if (n_subc > 1)
            sprintf(name, "%s - %i", menuName.c_str(), subc + 1);
        else
        {
           strncpy(name, menuName.c_str(), NAMECHARS);
        }

        if (!single_category)
        {
            CMenuItem *entry = contextMenu->addEntry(subMenu, name);
            if (c == current_category || amIChecked)
                entry->setChecked(true);
            subMenu->forget(); // Important, so that the refcounter gets it right
        }
        main_e++;
    }
    return amIChecked;
}

void CPatchBrowser::loadPatch(int id)
{
   if (listener && (id >= 0))
   {
      sel_id = id;
      listener->valueChanged(this);
   }
}
