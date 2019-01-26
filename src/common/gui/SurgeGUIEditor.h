//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "globals.h"

#if TARGET_AUDIOUNIT
//#include "vstkeycode.h"
#include <vstgui/plugin-bindings/plugguieditor.h>
typedef PluginGUIEditor EditorType;
#elif TARGET_VST3
#include "public.sdk/source/vst/vstguieditor.h"
typedef Steinberg::Vst::VSTGUIEditor EditorType;
#elif TARGET_VST2
#include <vstgui/plugin-bindings/aeffguieditor.h>
typedef AEffGUIEditor EditorType;
#else
#include <vstgui/plugin-bindings/plugguieditor.h>
typedef PluginGUIEditor EditorType;
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
   using super = EditorType;

public:
   SurgeGUIEditor(void* effect, SurgeSynthesizer* synth);
   virtual ~SurgeGUIEditor();
   void idle();
   bool queue_refresh;
   virtual void toggle_mod_editing();

   virtual void beginEdit(long index);
   virtual void endEdit(long index);

   virtual long applyParameterOffset(long index);
   
#if !TARGET_VST3
   bool open(void* parent) override;
#else
   virtual bool PLUGIN_API open(void* parent, const PlatformType& platformType = kDefaultNative);
#endif

   virtual void close() override;

protected:
   int32_t onKeyDown(const VstKeyCode& code,
                     CFrame* frame) override; ///< should return 1 if no further key down processing
                                              ///< should apply, otherwise -1
   int32_t onKeyUp(const VstKeyCode& code,
                   CFrame* frame) override; ///< should return 1 if no further key up processing
                                            ///< should apply, otherwise -1

   virtual void setParameter(long index, float value);

   // listener class
   void valueChanged(CControl* control) override;
   int32_t controlModifierClicked(CControl* pControl, CButtonState button) override;
   void controlBeginEdit(CControl* pControl) override;
   void controlEndEdit(CControl* pControl) override;

   void refresh_mod();

   /**
    * getCurrentMouseLocationCorrectedForVSTGUIBugs
    *
    * This function gets the current mouse location for the frame
    * but adds, in necessary cases, workarounds for bugs in the
    * vstgui framework. Use it in place of frame->getCurrentMouseLocation
    */
   CPoint getCurrentMouseLocationCorrectedForVSTGUIBugs();

private:
   void openOrRecreateEditor();
   void close_editor();
   bool isControlVisible(ControlGroup controlGroup, int controlGroupEntry);
   SurgeSynthesizer* synth = nullptr;
   int current_scene = 0, current_osc = 0, current_fx = 0;
   bool editor_open = false;
   bool mod_editor = false;
   modsources modsource = ms_original, modsource_editor = ms_original;
   unsigned int idleinc = 0;
   int fxbypass_tag = 0, resolink_tag = 0, f1resotag = 0, f1subtypetag = 0, f2subtypetag = 0,
       filterblock_tag = 0;
   void draw_infowindow(int ptag, CControl* control, bool modulate, bool forceMB = false);

   bool showPatchStoreDialog(patchdata* p,
                             std::vector<PatchCategory>* patch_category,
                             int startcategory);


   void showSettingsMenu(CRect &menuRect);

   /*
   ** Zoom Implementation 
   ** 
   ** Zoom works by the system maintaining a zoom factor (created by user actions)
   **
   ** All zoom factors are set in units of percentages as ints. So no zoom is "100",
   ** and double size is "200"
   */
   
   int zoomFactor;
 public:
   void setZoomCallback(std::function< void(SurgeGUIEditor *) > f) {
       zoom_callback = f;
       setZoomFactor(getZoomFactor()); // notify the new callback
   }
   int  getZoomFactor() { return zoomFactor; }
   void setZoomFactor(int zf);

private:
   std::function< void(SurgeGUIEditor *) > zoom_callback;
   
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
   CControl* nonmod_param[1024] = {}; 
   CControl* gui_modsrc[n_modsources] = {};
   CControl* metaparam[n_customcontrollers] = {};
   CControl* lfodisplay = nullptr;
   CControl* filtersubtype[2] = {};
#if MAC || __linux__
#else
   HWND ToolTipWnd;
#endif
   int clear_infoview_countdown = 0;
   float blinktimer = 0;
   bool blinkstate = false;
   void* _effect = nullptr;
   CVSTGUITimer* _idleTimer = nullptr;
};

#if ( MAC && ( TARGET_AUDIOUNIT || TARGET_VST2 )  ) || (WINDOWS && TARGET_VST2 )
#define HOST_SUPPORTS_ZOOM 1
#endif
