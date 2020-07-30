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

#include "vstcontrols.h"
#include "SurgeSynthesizer.h"


#include "SkinSupport.h"

#include <vector>
 
class CSurgeSlider;

#if TARGET_VST3
namespace Steinberg
{
   namespace Vst {
      class IContextMenu;
   }
}
#endif

class SurgeGUIEditor : public EditorType, public VSTGUI::IControlListener, public VSTGUI::IKeyboardHook
{
private:
   using super = EditorType;

public:
   SurgeGUIEditor(void* effect, SurgeSynthesizer* synth, void* userdata = nullptr);
   virtual ~SurgeGUIEditor();
#if TARGET_AUDIOUNIT | TARGET_VST2 | TARGET_LV2
   void idle() override;
#else
   void idle();
#endif   
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
   virtual bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType = VSTGUI::kDefaultNative) override;
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

public:
   void refresh_mod();
   void forceautomationchangefor(Parameter *p);
   
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
   void setDisabledForParameter(Parameter* p, CSurgeSlider* s);
   
private:
   void openOrRecreateEditor();
   VSTGUI::CControl * layoutTagWithSkin( int tag );
   void close_editor();
   bool isControlVisible(ControlGroup controlGroup, int controlGroupEntry);
   SurgeSynthesizer* synth = nullptr;
   int current_scene = 0, current_osc = 0, current_fx = 0;
   bool editor_open = false;
   bool mod_editor = false;
   modsources modsource = ms_original, modsource_editor = ms_original;
   unsigned int idleinc = 0;
   int fxbypass_tag = 0, resolink_tag = 0, f1resotag = 0, f1subtypetag = 0, f2subtypetag = 0,
       filterblock_tag = 0, fmconfig_tag = 0;
   double lastTempo = 0;
   int lastTSNum = 0, lastTSDen = 0;
   void draw_infowindow(int ptag, VSTGUI::CControl* control, bool modulate, bool forceMB = false);

   struct patchdata
   {
      std::string name;
      std::string category;
      std::string comments;
      std::string author;
   };

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
   int zoomFactorRecursionGuard = 0;
   bool doesZoomFitToScreen(int zf, int &correctedZf); // returns true if it fits; false if not; sets correctedZF to right size in either case
   void disableZoom()
   {
      zoomEnabled = false;
      setZoomFactor(100);
   }

   /*
   ** Callbacks from the Status Panel. If this gets to be too many perhaps make these an interface?
   */
   void showMPEMenu(VSTGUI::CPoint& where);
   void showTuningMenu(VSTGUI::CPoint& where);
   void showZoomMenu(VSTGUI::CPoint& where);
   void toggleMPE();
   void toggleTuning();
   void tuningFileDropped(std::string fn);
   void mappingFileDropped(std::string fn);
   std::string tuningCacheForToggle = "";
   std::string mappingCacheForToggle = "";
   std::string tuningToHtml();

   void queueRebuildUI() { queue_refresh = true; synth->refresh_editor = true; }

   std::string midiMappingToHtml();
   
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
#if TARGET_VST3
   Steinberg::Vst::IContextMenu* addVst3MenuForParams(VSTGUI::COptionMenu *c, int ptag, int &eid); // just a noop if you aren't a vst3 of course
#endif
   
   std::function< void(SurgeGUIEditor *) > zoom_callback;
   bool zoomInvalid;
   int minimumZoom;

   int selectedFX[8];
   std::string fxPresetName[8];
   
   std::shared_ptr<SurgeBitmaps> bitmapStore = nullptr;

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

   VSTGUI::CViewContainer* typeinDialog = nullptr;
   VSTGUI::CTextEdit* typeinValue = nullptr;
   VSTGUI::CTextLabel* typeinLabel = nullptr;
   VSTGUI::CTextLabel* typeinPriorValueLabel = nullptr;
   VSTGUI::CControl* typeinEditControl = nullptr;
   enum TypeInMode {
      Inactive,
      Param,
      Control
   } typeinMode = Inactive;
   std::vector<VSTGUI::CViewContainer*> removeFromFrame;
   int typeinResetCounter = -1;
   std::string typeinResetLabel = "";

   VSTGUI::CTextLabel* fxPresetLabel = nullptr;
   
   std::string modulatorName(int ms, bool forButton);
   
   Parameter *typeinEditTarget = nullptr;
   int typeinModSource = -1;
   
   VSTGUI::CControl* polydisp = nullptr;
   VSTGUI::CControl* oscdisplay = nullptr;
   VSTGUI::CControl* splitkeyControl = nullptr;

   static const int n_paramslots = 1024;
   VSTGUI::CControl* param[n_paramslots] = {};
   VSTGUI::CControl* nonmod_param[n_paramslots] = {}; 
   VSTGUI::CControl* gui_modsrc[n_modsources] = {};
   VSTGUI::CControl* metaparam[n_customcontrollers] = {};
   VSTGUI::CControl* lfodisplay = nullptr;
   VSTGUI::CControl* filtersubtype[2] = {};
   VSTGUI::CControl* fxmenu = nullptr;
#if MAC || LINUX
#else
   HWND ToolTipWnd;
#endif
   int clear_infoview_countdown = 0;
public:
   int clear_infoview_peridle = -1;
private:
   float blinktimer = 0;
   bool blinkstate = false;
   bool useDevMenu = false;
   void* _effect = nullptr;
   void* _userdata = nullptr;
   VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> _idleTimer;
   int firstIdleCountdown = 0;

   /*
   ** Utility Function
   */
   VSTGUI::CCommandMenuItem*
   addCallbackMenu(VSTGUI::COptionMenu* toThis, std::string label, std::function<void()> op);
   VSTGUI::COptionMenu* makeMpeMenu(VSTGUI::CRect &rect, bool showhelp);
   VSTGUI::COptionMenu* makeTuningMenu(VSTGUI::CRect& rect, bool showhelp);
   VSTGUI::COptionMenu* makeZoomMenu(VSTGUI::CRect& rect, bool showhelp);
   VSTGUI::COptionMenu* makeSkinMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeUserSettingsMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeDataMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeMidiMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeDevMenu(VSTGUI::CRect &rect);
   bool scannedForMidiPresets = false;

public:
   std::string helpURLFor( Parameter *p );
   std::string helpURLForSpecial( std::string special );
   std::string fullyResolvedHelpURL( std::string helpurl );

private:
   void promptForUserValueEntry(Parameter *p, VSTGUI::CControl *c, int modulationSource = -1);
   
   /*
   ** Skin support
   */
   Surge::UI::Skin::ptr_t currentSkin;
   void setupSkinFromEntry( const Surge::UI::SkinDB::Entry &entry );
   void reloadFromSkin();

   /*
   ** General MIDI CC names
   */
   const char midicc_names[128][42] =
   {
      "Bank Select MSB",
      "Modulation Wheel MSB",
      "Breath Controller MSB",
      "Control 3 MSB",
      "Foot Controller MSB",
      "Portamento Time MSB",
      "Data Entry MSB",
      "Volume MSB",
      "Balance MSB",
      "Control 9 MSB",
      "Pan MSB",
      "Expression MSB",
      "Effect #1 MSB",
      "Effect #2 MSB",
      "Control 14 MSB",
      "Control 15 MSB",
      "General Purpose Controller #1 MSB",
      "General Purpose Controller #2 MSB",
      "General Purpose Controller #3 MSB",
      "General Purpose Controller #4 MSB",
      "Control 20 MSB",
      "Control 21 MSB",
      "Control 22 MSB",
      "Control 23 MSB",
      "Control 24 MSB",
      "Control 25 MSB",
      "Control 26 MSB",
      "Control 27 MSB",
      "Control 28 MSB",
      "Control 29 MSB",
      "Control 30 MSB",
      "Control 31 MSB",
      "Bank Select LSB",
      "Modulation Wheel LSB",
      "Breath Controller LSB",
      "Control 3 LSB",
      "Foot Controller LSB",
      "Portamento Time LSB",
      "Data Entry LSB",
      "Volume LSB",
      "Balance LSB",
      "Control 9 LSB",
      "Pan LSB",
      "Expression LSB",
      "Effect #1 LSB",
      "Effect #2 LSB",
      "Control 14 LSB",
      "Control 15 LSB",
      "General Purpose Controller #1 LSB",
      "General Purpose Controller #2 LSB",
      "General Purpose Controller #3 LSB",
      "General Purpose Controller #4 LSB",
      "Control 20 LSB",
      "Control 21 LSB",
      "Control 22 LSB",
      "Control 23 LSB",
      "Control 24 LSB",
      "Control 25 LSB",
      "Control 26 LSB",
      "Control 27 LSB",
      "Control 28 LSB",
      "Control 29 LSB",
      "Control 30 LSB",
      "Control 31 LSB",
      "Sustain Pedal",
      "Portamento Pedal",
      "Sostenuto Pedal",
      "Soft Pedal",
      "Legato Pedal",
      "Hold Pedal",
      "Sound Control #1 Sound Variation",
      "Sound Control #2 Timbre",
      "Sound Control #3 Release Time",
      "Sound Control #4 Attack Time",
      "Sound Control #5 Brightness / MPE Timbre",
      "Sound Control #6 Decay Time",
      "Sound Control #7 Vibrato Rate",
      "Sound Control #8 Vibrato Depth",
      "Sound Control #9 Vibrato Delay",
      "Sound Control #10 Control 79",
      "General Purpose Controller #5",
      "General Purpose Controller #6",
      "General Purpose Controller #7",
      "General Purpose Controller #8",
      "Portamento Control",
      "Control 85",
      "Control 86",
      "Control 87",
      "High Resolution Velocity Prefix",
      "Control 89",
      "Control 90",
      "Reverb Send Level",
      "Tremolo Depth",
      "Chorus Send Level",
      "Celeste Depth",
      "Phaser Depth",
      "Data Increment",
      "Data Decrement",
      "NRPN LSB",
      "NRPN MSB",
      "RPN LSB",
      "RPN MLSB",
      "Control 102",
      "Control 103",
      "Control 104",
      "Control 105",
      "Control 106",
      "Control 107",
      "Control 108",
      "Control 109",
      "Control 110",
      "Control 111",
      "Control 112",
      "Control 113",
      "Control 114",
      "Control 115",
      "Control 116",
      "Control 117",
      "Control 118",
      "Control 119",
      "Control 120 All Sound Off",
      "Control 121 Reset All Controllers",
      "Control 122 Local Control On/Of",
      "Control 123 All Notes Off",
      "Control 124 Omni Mode Off",
      "Control 125 Omni Mode On",
      "Control 126 Mono Mode Off",
      "Control 127 Mono Mode On"
   };
};

