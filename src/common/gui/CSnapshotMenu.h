//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "storage.h"
#include "miniedit_spawners.h"

class CSnapshotMenu : public COptionMenu
{
public:
   CSnapshotMenu(const CRect& size, IControlListener* listener, long tag, sub3_storage* storage);
   virtual ~CSnapshotMenu();
   virtual void draw(CDrawContext* dc);
   // virtual CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons);
   void populate();
   virtual void loadSnapshot(int type, TiXmlElement* e){};
   virtual void saveSnapshot(TiXmlElement* e, const char* name){};
   virtual bool canSave();

protected:
   sub3_storage* storage = nullptr;
   char mtype[16] = {0};
};

class COscMenu : public CSnapshotMenu
{
public:
   COscMenu(const CRect& size,
            IControlListener* listener,
            long tag,
            sub3_storage* storage,
            sub3_osc* osc);
   virtual void draw(CDrawContext* dc);
   virtual void loadSnapshot(int type, TiXmlElement* e);

protected:
   sub3_osc* osc = nullptr;

   CLASS_METHODS(COscMenu, CControl)
};

class CFxMenu : public CSnapshotMenu
{
public:
   CFxMenu(const CRect& size,
           IControlListener* listener,
           long tag,
           sub3_storage* storage,
           sub3_fx* fx,
           sub3_fx* fxbuffer,
           int slot);
   virtual void draw(CDrawContext* dc);
   virtual bool canSave()
   {
      return true;
   }
   virtual void loadSnapshot(int type, TiXmlElement* e);
   virtual void saveSnapshot(TiXmlElement* e, const char* name);

protected:
   sub3_fx *fx = nullptr, *fxbuffer = nullptr;
   int slot = 0;

   CLASS_METHODS(CFxMenu, CControl)
};