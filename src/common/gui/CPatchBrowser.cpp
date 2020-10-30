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
#include "CPatchBrowser.h"
#include "UserInteractions.h"
#include "SkinColors.h"
#include "guihelpers.h"

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
   dc->setFillColor(skin->getColor(Colors::PatchBrowser::Background));
   // dc->fillRect(ar);
   // ar.top += 2;
   CRect al(ar);
   // ar.left += 68;
   ar.left += 2;
   al.right = al.left + 150;
   al.left += 3;
   // al.top += 2;
   al.bottom = al.top + 12;
   dc->setFontColor(skin->getColor(Colors::PatchBrowser::Text));
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
   if (listener && (button & (kMButton | kButton4 | kButton5)))
   {
      listener->controlModifierClicked(this, button);
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

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
              if (_stricmp(storage->patch_category[c].name.c_str(),"Init") == 0)
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
       auto factory_add = contextMenu->addEntry("FACTORY PATCHES");
       factory_add->setEnabled(0);

       for (int i = 0; i < storage->patch_category.size(); i++)
       {
           if ((!single_category) || (i == last_category))
           {
               if (!single_category && (i == storage->firstThirdPartyCategory || i == storage->firstUserCategory))
               {
                   string txt;

                   if (i == storage->firstThirdPartyCategory)
                      txt = "THIRD PARTY PATCHES";
                   else
                      txt = "USER PATCHES";

                   auto add = contextMenu->addEntry(txt.c_str());
                   add->setEnabled(0);
               }

               // Remap index to the corresponding category in alphabetical order.
               int c = storage->patchCategoryOrdering[i];

               populatePatchMenuForCategory( c, contextMenu, single_category, main_e, true );
           }
       }
   }
   
   contextMenu->addSeparator();

   auto loadF = new CCommandMenuItem( CCommandMenuItem::Desc( Surge::UI::toOSCaseForMenu( "Load Patch from File..." ) ) );
   loadF->setActions( [this](CCommandMenuItem *item) {
                         Surge::UserInteractions::promptFileOpenDialog( "", "fxp", "Surge FXP Files",
                                                                        [this](std::string fn) {
                                                                           auto sge = dynamic_cast<SurgeGUIEditor*>(listener);
                                                                           if( ! sge ) return;
                                                                           sge->queuePatchFileLoad( fn );
                                                                        });
                      }
      );
   contextMenu->addEntry(loadF);
   
   auto refreshItem = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh Patch List")));
   auto refreshAction = [this](CCommandMenuItem *item)
                           {
                              this->storage->refresh_patchlist();
                           };
   refreshItem->setActions(refreshAction,nullptr);
   contextMenu->addEntry(refreshItem);

   contextMenu->addSeparator();

   auto showU = new CCommandMenuItem( CCommandMenuItem::Desc( Surge::UI::toOSCaseForMenu( "Open User Patches Folder..." )));
   showU->setActions( [this](CCommandMenuItem *item) {
                         Surge::UserInteractions::openFolderInFileBrowser( this->storage->userDataPath );
                      }
      );
   contextMenu->addEntry(showU);
   
   auto showF = new CCommandMenuItem( CCommandMenuItem::Desc( Surge::UI::toOSCaseForMenu( "Open Factory Patches Folder..." )));
   showF->setActions( [this](CCommandMenuItem *item) {
                         Surge::UserInteractions::openFolderInFileBrowser(Surge::Storage::appendDirectory(this->storage->datapath, "patches_factory"));
                      }
      );
   contextMenu->addEntry(showF);

   auto show3 = new CCommandMenuItem( CCommandMenuItem::Desc( Surge::UI::toOSCaseForMenu( "Open Third Party Patches Folder..." )));
   show3->setActions( [this](CCommandMenuItem *item) {
                         Surge::UserInteractions::openFolderInFileBrowser(Surge::Storage::appendDirectory(this->storage->datapath, "patches_3rdparty"));
                      }
      );
   contextMenu->addEntry(show3);


   contextMenu->addSeparator();

   auto *sge = dynamic_cast<SurgeGUIEditor*>(listener);
   if( sge )
   {
      auto hu = sge->helpURLForSpecial( "patch-browser" );
      if( hu != "" )
      {
         auto lurl = sge->fullyResolvedHelpURL(hu);
         auto hi = new CCommandMenuItem( CCommandMenuItem::Desc("[?] Patch Browser"));
         auto ca = [lurl](CCommandMenuItem *i)
                      {
                         Surge::UserInteractions::openURL(lurl);
                      };
         hi->setActions( ca, nullptr );
         contextMenu->addEntry(hi);
      }
   }
   
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
        string name;
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

            name = storage->patch_list[p].name;

            #if WINDOWS
               Surge::Storage::findReplaceSubstring(name, string("&"), string("&&"));
            #endif
            
            auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name.c_str()));
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
            if (checkedKid)
            {
                amIChecked=true;
            }
        }
        
        string menuName = storage->patch_category[c].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != string::npos)
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);
        
        if (n_subc > 1)
           name = menuName.c_str() + (subc + 1);
        else
           name = menuName.c_str();

        // tuck in the category name by 4 spaces, but only the root categories
        if (rootCall)
           name = "    " + name;

        #if WINDOWS
           Surge::Storage::findReplaceSubstring(name, string("&"), string("&&"));
        #endif

        if (!single_category)
        {
            CMenuItem *entry = contextMenu->addEntry(subMenu, name.c_str());
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
