//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "globals.h"

#if TARGET_AUDIOUNIT
//#include "vstkeycode.h"
#include "vstgui/plugin-bindings/plugguieditor.h"
typedef VSTGUI::PluginGUIEditor EditorType;
#elif TARGET_VST3
#include "public.sdk/source/vst/vstguieditor.h"
typedef Steinberg::Vst::VSTGUIEditor EditorType;
#elif TARGET_VST2
#if LINUX
#include "../linux/linux-aeffguieditor.h"
typedef VSTGUI::LinuxAEffGUIEditor EditorType;
#else
#include "vstgui/plugin-bindings/aeffguieditor.h"
typedef VSTGUI::AEffGUIEditor EditorType;
#endif
#else
#include "vstgui/plugin-bindings/plugguieditor.h"
typedef VSTGUI::PluginGUIEditor EditorType;
#endif

#include "SurgeStorage.h"
#include "SurgeBitmaps.h"
#include "PopupEditorSpawner.h"
#include "../layoutengine/LayoutEngine.h"

#include <vstcontrols.h>
#include "SurgeSynthesizer.h"

#include <vector>

namespace Surge
{
class LayoutEngine;
};

class SurgeGUIEditor : public EditorType, public VSTGUI::IControlListener, public VSTGUI::IKeyboardHook
{
   friend class Surge::LayoutEngine;

private:
   using super = EditorType;

public:
   int WINDOW_SIZE_X = 904;
   int WINDOW_SIZE_Y = 542;

   SurgeGUIEditor(void* effect, SurgeSynthesizer* synth, void* userdata = nullptr);
   virtual ~SurgeGUIEditor();
   void idle();
   bool queue_refresh;
   virtual void toggle_mod_editing();

   virtual void beginEdit(long index);
   virtual void endEdit(long index);

   static long applyParameterOffset(long index);
   static long unapplyParameterOffset(long index);

#if !TARGET_VST3
   bool open(void* parent) override;
   void close() override;
#else
   virtual bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType = VSTGUI::kDefaultNative);
   virtual void PLUGIN_API close() override;

   virtual Steinberg::tresult PLUGIN_API onWheel( float distance ) override
   {
     /*
     ** in VST3 the VstGuiEditorBase we have - even if the OS has handled it
     ** a call to the VSTGUIEditor::onWheel (trust me; I put in stack traces
     ** and prints). That's probably wrong. But when you use VSTGUI Zoom 
     ** and frame->getCurrentPosition gets screwed up because VSTGUI has transform
     ** bugs it is definitely wrong. So the mouse wheel event gets mistakenly
     ** delivered twice (OK) but to the wrong spot (not OK!).
     ** 
     ** So stop the superclass from doing that by just making this do nothing.
     */
     return Steinberg::kResultFalse;
   }

   virtual Steinberg::tresult PLUGIN_API canResize() override
   {
      return Steinberg::kResultTrue;
   }

   virtual Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
   virtual Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* newSize) override;
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
   bool zoomEnabled = true;

public:

   void populateDawExtraState(SurgeSynthesizer *synth) {
       synth->storage.getPatch().dawExtraState.isPopulated = true;
       synth->storage.getPatch().dawExtraState.instanceZoomFactor = zoomFactor;
   }
   void loadFromDAWExtraState(SurgeSynthesizer *synth) {
       if( synth->storage.getPatch().dawExtraState.isPopulated )
       {
           auto sz = synth->storage.getPatch().dawExtraState.instanceZoomFactor;
           if( sz > 0 )
               setZoomFactor(sz);
       }
   }
   
   void setZoomCallback(std::function< void(SurgeGUIEditor *) > f) {
       zoom_callback = f;
       setZoomFactor(getZoomFactor()); // notify the new callback
   }
   int  getZoomFactor() { return zoomFactor; }
   void setZoomFactor(int zf);
   bool doesZoomFitToScreen(int zf, int &correctedZf); // returns true if it fits; false if not; sets correctedZF to right size in either case
   void disableZoom()
   {
      zoomEnabled = false;
      setZoomFactor(100);
   }

   /*
   ** Callbacks from the Status Panel. If this gets to be too many perhaps make these an interface?
   */
   void toggleMPE();
   void showMPEMenu(VSTGUI::CPoint &where);
   void toggleTuning();
   void showTuningMenu(VSTGUI::CPoint &where);
   void tuningFileDropped(std::string fn);
   std::string tuningCacheForToggle = "";
   
private:
   /**
    * findLargestFittingZoomBetween
    *
    * Finds the largest zoom which will fit your current screen between a lower and upper bound.
    * Will never return something smaller than lower or larger than upper. Default is as large as
    * possible, quantized in units of zoomQuanta, with the maximum screen usage percentages
    * protecting for screen real estate. The function also needs to know the 100% size of the UI
    * the baseW and baseH)
    *
    * for instance findLargestFittingZoomBetween( 100, 200, 5, 90, bw, bh )
    *
    * would find the largest possible zoom which uses at most 90% of your screen real estate but
    * would also guarantee that the result % 5 == 0.
    */
   int findLargestFittingZoomBetween(int zoomLow, int zoomHigh, int zoomQuanta, int percentageOfScreenAvailable,
                                     float baseW, float baseH);
   
private:
   std::function< void(SurgeGUIEditor *) > zoom_callback;
   bool zoomInvalid;
   int minimumZoom;

   std::shared_ptr<SurgeBitmaps> bitmapStore = nullptr;
   std::unique_ptr<Surge::LayoutEngine> layout = nullptr;

   bool modsource_is_alternate[n_modsources];

   VSTGUI::CControl* vu[16];
   VSTGUI::CControl *infowindow, *patchname, *ccfxconf = nullptr, *statuspanel = nullptr;
   VSTGUI::CControl* aboutbox = nullptr;
   VSTGUI::CViewContainer* saveDialog = nullptr;
   VSTGUI::CTextEdit* patchName = nullptr;
   VSTGUI::CTextEdit* patchCategory = nullptr;
   VSTGUI::CTextEdit* patchCreator = nullptr;
   VSTGUI::CTextEdit* patchComment = nullptr;
   VSTGUI::CCheckBox* patchTuning = nullptr;
   VSTGUI::CTextLabel* patchTuningLabel = nullptr;
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
   void* _userdata = nullptr;
   VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> _idleTimer;

   /*
   ** Utility Function
   */
   VSTGUI::CCommandMenuItem*
   addCallbackMenu(VSTGUI::COptionMenu* toThis, std::string label, std::function<void()> op);
   VSTGUI::COptionMenu* makeMpeMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeTuningMenu(VSTGUI::CRect &rect);
};

