//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "globals.h"

#if TARGET_AUDIOUNIT
//#include "vstkeycode.h"
#include <plugin-bindings/plugguieditor.h>
typedef PluginGUIEditor EditorType;
#elif TARGET_VST3
#include "public.sdk/source/vst/vstguieditor.h"
typedef Steinberg::Vst::VSTGUIEditor EditorType;
#else
#include <vstgui/plugin-bindings/aeffguieditor.h>
typedef AEffGUIEditor EditorType;
#endif

#include "SurgeStorage.h"
#include "SurgeBitmaps.h"
#include "PopupEditorSpawner.h"

#include <vstcontrols.h>
#include "SurgeSynthesizer.h"

#include <vector>
using namespace std;

class SurgeGUIEditor : public EditorType, public IControlListener, public IKeyboardHook
{
private:
   typedef EditorType super;

public:
   SurgeGUIEditor(void* effect, SurgeSynthesizer* synth);
   virtual ~SurgeGUIEditor();
   virtual void idle();
   bool queue_refresh;
   virtual void toggle_mod_editing();

   virtual void beginEdit(long index);
   virtual void endEdit(long index);

#if !TARGET_VST3
   virtual bool open(void* parent);
#else
   virtual bool PLUGIN_API open(void* parent, const PlatformType& platformType = kDefaultNative);
#endif

   virtual void close();

protected:
   virtual int32_t onKeyDown(const VstKeyCode& code,
                             CFrame* frame); ///< should return 1 if no further key down processing
                                             ///< should apply, otherwise -1
   virtual int32_t onKeyUp(const VstKeyCode& code,
                           CFrame* frame); ///< should return 1 if no further key up processing
                                           ///< should apply, otherwise -1

   virtual void setParameter(long index, float value);

   // listener class
   virtual void valueChanged(CControl* control);
   virtual int32_t controlModifierClicked(CControl* pControl, CButtonState button);
   virtual void controlBeginEdit(CControl* pControl);
   virtual void controlEndEdit(CControl* pControl);

   void refresh_mod();

private:
   void openOrRecreateEditor();
   void close_editor();
   bool is_visible(int subsec, int subsec_id);
   SurgeSynthesizer* synth = nullptr;
   int current_scene = 0, current_osc = 0, current_fx = 0;
   bool editor_open = false;
   bool mod_editor = false;
   int modsource = 0, modsource_editor = 0;
   unsigned int idleinc = 0;
   int fxbypass_tag = 0, resolink_tag = 0, f1resotag = 0, f1subtypetag = 0, f2subtypetag = 0,
       filterblock_tag = 0;
   void draw_infowindow(int ptag, CControl* control, bool modulate, bool forceMB = false);

   bool showPatchStoreDialog(patchdata* p,
                             vector<patchlist_category>* patch_category,
                             int startcategory);

   SurgeBitmaps bitmap_keeper;

   CControl* vu[16];
   CControl *infowindow, *patchname, *ccfxconf = nullptr;
   CControl* aboutbox = nullptr;
   CViewContainer* saveDialog = nullptr;
   CTextEdit* patchName = nullptr;
   CTextEdit* patchCategory = nullptr;
   CTextEdit* patchCreator = nullptr;
   CTextEdit* patchComment = nullptr;
   CControl* polydisp = nullptr;
   CControl* oscdisplay = nullptr;
   CControl* param[1024] = {};
   CControl* gui_modsrc[n_modsources] = {};
   CControl* metaparam[n_customcontrollers] = {};
   CControl* lfodisplay = nullptr;
   CControl* filtersubtype[2] = {};
#if MAC
#else
   HWND ToolTipWnd;
#endif
   int clear_infoview_countdown = 0;
   float blinktimer = 0;
   bool blinkstate = false;
   void* _effect = 0;
};
