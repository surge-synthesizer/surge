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
#include "SkinSupport.h"

class CPatchBrowser : public VSTGUI::CControl, public Surge::UI::SkinConsumingComponent
{
public:

   CPatchBrowser(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, SurgeStorage* storage)
       : VSTGUI::CControl(size, listener, tag, 0)
   {
      setLabel("Init");
      setCategory("Init");
      this->storage = storage;
      sel_id = -1;
      current_patch = -1;
      current_category = -1;
   }
   void setIDs(int category, int patch)
   {
      current_category = category;
      current_patch = patch;
   }
   void setLabel(std::string l)
   {
      pname = l;
      setDirty(true);
   }
   void setCategory(std::string l)
   {
      if (l.length())
      {
         std::string s = l;
         // for (int i=0; i<s.length(); i++) s[i] = toupper(s[i]);
         category = "Category: " + s;
      }
      else
         category = "";
      setDirty(true);
   }
   void setAuthor(std::string l)
   {
      if (l.length())
      {
         std::string s = l;
         // for (int i=0; i<s.length(); i++) s[i] = toupper(s[i]);
         author = "Author: " + s;
      }
      else
         author = "";
      setDirty(true);
   }

   virtual void draw(VSTGUI::CDrawContext* dc) override;
   VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& button) override;
   void loadPatch(int id);
   int sel_id = 0;

protected:
   std::string pname;
   std::string category;
   std::string author;
   int current_category = 0, current_patch = 0;
   SurgeStorage* storage = nullptr;

   /**
    * populatePatchMenuForCategory
    *
    * recursively builds the nested patch menu. In the event that one of my children is checked, return true so I too can be checked.
    * otherwise, return false.
    */
   bool populatePatchMenuForCategory (int index, VSTGUI::COptionMenu *contextMenu, bool single_category, int &main_e, bool rootCall);
   
   CLASS_METHODS(CPatchBrowser, VSTGUI::CControl)
};
