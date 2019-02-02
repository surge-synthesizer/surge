//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "globals.h"

#if TARGET_AUDIOUNIT
//#include "vstkeycode.h"
#include <vstgui/plugin-bindings/plugguieditor.h>
typedef VSTGUI::PluginGUIEditor EditorType;
#elif TARGET_VST3
#include "public.sdk/source/vst/vstguieditor.h"
typedef Steinberg::Vst::VSTGUIEditor EditorType;
#elif TARGET_VST2
#if LINUX
#include "../linux/linux-aeffguieditor.h"
typedef VSTGUI::LinuxAEffGUIEditor EditorType;
#else
#include <vstgui/plugin-bindings/aeffguieditor.h>
typedef VSTGUI::AEffGUIEditor EditorType;
#endif
#else
#include <vstgui/plugin-bindings/plugguieditor.h>
typedef VSTGUI::PluginGUIEditor EditorType;
#endif

#include "SurgeStorage.h"
#include "SurgeBitmaps.h"
#include "PopupEditorSpawner.h"

#include <vstcontrols.h>
#include "SurgeSynthesizer.h"

#include <vector>

class SurgeGUIEditor : public EditorType, public VSTGUI::IControlListener, public VSTGUI::IKeyboardHook
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
   void close() override;
#else
   virtual bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType = VSTGUI::kDefaultNative);
   virtual void PLUGIN_API close() override;
#endif


protected:
   int32_t onKeyDown(const VstKeyCode& code,
                     VSTGUI::CFrame* frame) override; ///< should return 1 if no further key down processing
                                              ///< should apply, otherwise -1
   int32_t onKeyUp(const VstKeyCode& code,
                   VSTGUI::CFrame* frame) override; ///< should return 1 if no further key up processing
                                            ///< should apply, otherwise -1

   virtual void setParameter(long index, float value);

   // listener class
   void valueChanged(VSTGUI::CControl* control) override;
   int32_t controlModifierClicked(VSTGUI::CControl* pControl, VSTGUI::CButtonState button) override;
   void controlBeginEdit(VSTGUI::CControl* pControl) override;
   void controlEndEdit(VSTGUI::CControl* pControl) override;

   void refresh_mod();

   /**
    * getCurrentMouseLocationCorrectedForVSTGUIBugs
    *
    * This function gets the current mouse location for the frame
    * but adds, in necessary cases, workarounds for bugs in the
    * vstgui framework. Use it in place of frame->getCurrentMouseLocation
    */
   VSTGUI::CPoint getCurrentMouseLocationCorrectedForVSTGUIBugs();

#if TARGET_VST3
public:
   /**
    * getIPlugFrame
    *
    * Amazingly, IPlugView has a setFrame( IPlugFrame ) method but
    * no getter. It does, however, store the value as a protected
    * variable. To collaborate for zoom, we need this reference
    * in the VST Processor, so expose this function
    */
   Steinberg::IPlugFrame *getIPlugFrame() { return plugFrame; }
#endif
   
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
   void draw_infowindow(int ptag, VSTGUI::CControl* control, bool modulate, bool forceMB = false);

   bool showPatchStoreDialog(patchdata* p,
                             std::vector<PatchCategory>* patch_category,
                             int startcategory);


   void showSettingsMenu(VSTGUI::CRect &menuRect);

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

   VSTGUI::CControl* vu[16];
   VSTGUI::CControl *infowindow, *patchname, *ccfxconf = nullptr;
   VSTGUI::CControl* aboutbox = nullptr;
   VSTGUI::CViewContainer* saveDialog = nullptr;
   VSTGUI::CTextEdit* patchName = nullptr;
   VSTGUI::CTextEdit* patchCategory = nullptr;
   VSTGUI::CTextEdit* patchCreator = nullptr;
   VSTGUI::CTextEdit* patchComment = nullptr;
   VSTGUI::CControl* polydisp = nullptr;
   VSTGUI::CControl* oscdisplay = nullptr;
   VSTGUI::CControl* param[1024] = {};
   VSTGUI::CControl* nonmod_param[1024] = {}; 
   VSTGUI::CControl* gui_modsrc[n_modsources] = {};
   VSTGUI::CControl* metaparam[n_customcontrollers] = {};
   VSTGUI::CControl* lfodisplay = nullptr;
   VSTGUI::CControl* filtersubtype[2] = {};
#if MAC || LINUX
#else
   HWND ToolTipWnd;
#endif
   int clear_infoview_countdown = 0;
   float blinktimer = 0;
   bool blinkstate = false;
   void* _effect = nullptr;
   VSTGUI::CVSTGUITimer* _idleTimer = nullptr;
};

#if MAC || WINDOWS
#define HOST_SUPPORTS_ZOOM 1
#endif
