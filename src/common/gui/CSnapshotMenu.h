//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "PopupEditorSpawner.h"

class CSnapshotMenu : public COptionMenu
{
public:
   CSnapshotMenu(const CRect& size, IControlListener* listener, long tag, SurgeStorage* storage);
   virtual ~CSnapshotMenu();
   virtual void draw(CDrawContext* dc);
   // virtual CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons);
   void populate();
   virtual void loadSnapshot(int type, TiXmlElement* e){};
   virtual void saveSnapshot(TiXmlElement* e, const char* name){};
   virtual bool canSave();

protected:
   SurgeStorage* storage = nullptr;
   char mtype[16] = {0};
};

class COscMenu : public CSnapshotMenu
{
public:
   COscMenu(const CRect& size,
            IControlListener* listener,
            long tag,
            SurgeStorage* storage,
            OscillatorStorage* osc);
   virtual void draw(CDrawContext* dc);
   virtual void loadSnapshot(int type, TiXmlElement* e);

protected:
   OscillatorStorage* osc = nullptr;

   CLASS_METHODS(COscMenu, CControl)
};

class CFxMenu : public CSnapshotMenu
{
public:
   CFxMenu(const CRect& size,
           IControlListener* listener,
           long tag,
           SurgeStorage* storage,
           FxStorage* fx,
           FxStorage* fxbuffer,
           int slot);
   virtual void draw(CDrawContext* dc);
   virtual bool canSave()
   {
      return true;
   }
   virtual void loadSnapshot(int type, TiXmlElement* e);
   virtual void saveSnapshot(TiXmlElement* e, const char* name);

protected:
   FxStorage *fx = nullptr, *fxbuffer = nullptr;
   int slot = 0;

   CLASS_METHODS(CFxMenu, CControl)
};