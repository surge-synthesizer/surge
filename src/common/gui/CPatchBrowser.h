//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"

class CPatchBrowser : public CControl
{
public:
   CPatchBrowser(const CRect& size, IControlListener* listener, long tag, SurgeStorage* storage)
       : CControl(size, listener, tag, 0)
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
   void setLabel(string l)
   {
      pname = l;
      setDirty(true);
   }
   void setCategory(string l)
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
   void setAuthor(string l)
   {
      if (l.length())
      {
         std::string s = l;
         // for (int i=0; i<s.length(); i++) s[i] = toupper(s[i]);
         author = "Creator: " + s;
      }
      else
         author = "";
      setDirty(true);
   }
   virtual void draw(CDrawContext* dc);
   CMouseEventResult onMouseDown(CPoint& where, const CButtonState& button);
   void loadPatch(int id);
   int sel_id = 0;

protected:
   string pname;
   string category;
   string author;
   int current_category = 0, current_patch = 0;
   SurgeStorage* storage = nullptr;

   void populatePatchMenuForCategory (int index, COptionMenu *contextMenu, bool single_category, int &main_e, bool rootCall);
   
   CLASS_METHODS(CPatchBrowser, CControl)
};
