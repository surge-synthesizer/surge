//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once
#include "vstcontrols.h"
#include "SurgeStorage.h"
#include "PopupEditorSpawner.h"
#include "SurgeBitmaps.h"
#include "SkinSupport.h"

class CSnapshotMenu : public VSTGUI::COptionMenu, public Surge::UI::SkinConsumingComponnt
{
public:
   CSnapshotMenu(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, long tag, SurgeStorage* storage);
   virtual ~CSnapshotMenu();
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   // virtual VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons);
   virtual void populate();
   virtual void loadSnapshot(int type, TiXmlElement* e, int idx){};
   virtual bool loadSnapshotByIndex(int idx);

   virtual void saveSnapshot(TiXmlElement* e, const char* name){};
   virtual bool canSave();

   virtual VSTGUI::CMouseEventResult onMouseEntered (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // getFrame()->setCursor( VSTGUI::kCursorHand );
      return VSTGUI::kMouseEventHandled;
   }
   virtual VSTGUI::CMouseEventResult onMouseExited (VSTGUI::CPoint& where, const VSTGUI::CButtonState& buttons) override {
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
      return VSTGUI::kMouseEventHandled;
   }

   int selectedIdx = -1;
   std::string selectedName = "";
protected:
   void populateSubmenuFromTypeElement(TiXmlElement *typeElement, VSTGUI::COptionMenu *parent, int &main, int &sub, const long &max_sub, int &idx);
   SurgeStorage* storage = nullptr;
   char mtype[16] = {0};
};

class COscMenu : public CSnapshotMenu
{
public:
   COscMenu(const VSTGUI::CRect& size,
            VSTGUI::IControlListener* listener,
            long tag,
            SurgeStorage* storage,
            OscillatorStorage* osc,
            std::shared_ptr<SurgeBitmaps>);
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual void loadSnapshot(int type, TiXmlElement* e, int idx) override;

protected:
   OscillatorStorage* osc = nullptr;
   VSTGUI::CBitmap* bmp = nullptr;

   CLASS_METHODS(COscMenu, VSTGUI::CControl)
};

class CFxMenu : public CSnapshotMenu
{
public:
   CFxMenu(const VSTGUI::CRect& size,
           VSTGUI::IControlListener* listener,
           long tag,
           SurgeStorage* storage,
           FxStorage* fx,
           FxStorage* fxbuffer,
           int slot);
   virtual void draw(VSTGUI::CDrawContext* dc) override;
   virtual bool canSave() override
   {
      return true;
   }
   virtual void loadSnapshot(int type, TiXmlElement* e, int idx) override;
   virtual void saveSnapshot(TiXmlElement* e, const char* name) override;
   virtual void populate() override;
   
protected:
   FxStorage *fx = nullptr, *fxbuffer = nullptr;
   static std::vector<float> fxCopyPaste; // OK this is a crap data structure for now. See the code.
   int slot = 0;

   void copyFX();
   void pasteFX();

   CLASS_METHODS(CFxMenu, VSTGUI::CControl)
};
