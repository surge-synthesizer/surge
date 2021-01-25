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
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
typedef Steinberg::Vst::VSTGUIEditor EditorType;
#define PARENT_PLUGIN_TYPE SurgeVst3Processor
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

#ifndef PARENT_PLUGIN_TYPE
#define PARENT_PLUGIN_TYPE void
#endif

#include "SurgeStorage.h"
#include "SurgeBitmaps.h"

#include "vstcontrols.h"
#include "SurgeSynthesizer.h"


#include "SkinSupport.h"
#include "SkinColors.h"

#include <vector>
#include "MSEGEditor.h"
 
class CSurgeSlider;
class CModulationSourceButton;
class CAboutBox;

#if TARGET_VST3
namespace Steinberg
{
   namespace Vst {
      class IContextMenu;
   }
}
#endif

struct SGEDropAdapter;

class SurgeGUIEditor : public EditorType,
                       public VSTGUI::IControlListener,
                       public VSTGUI::IKeyboardHook
#if TARGET_VST3
                     , public Steinberg::IPlugViewContentScaleSupport
#endif
{
private:
   using super = EditorType;

public:
   SurgeGUIEditor(PARENT_PLUGIN_TYPE* effect, SurgeSynthesizer* synth, void* userdata = nullptr);
   virtual ~SurgeGUIEditor();

   static int start_paramtag_value;
   
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

   void resizeFromIdleSentinel();

   bool initialZoom();
   virtual Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
   virtual Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* newSize) override;
   virtual Steinberg::tresult PLUGIN_API setContentScaleFactor(ScaleFactor factor) override
   {
      // Unused for now. Consider removing this callback since not all hosts use it
      return Steinberg::kResultTrue;
   }

#endif

   bool pause_idle_updates = false;
   int enqueuePatchId = -1;
   void flushEnqueuedPatchId()
   {
      auto t = enqueuePatchId;
      enqueuePatchId = -1;
      if( t >= 0 )
      {
         synth->patchid_queue = t;
         // Looks scary but remember this only runs if audio thread is off
         synth->processThreadunsafeOperations();
         patchCountdown = 200;
      }
   }
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
   void showSettingsMenu(VSTGUI::CRect& menuRect);

   static bool fromSynthGUITag( SurgeSynthesizer *synth, int tag, SurgeSynthesizer::ID &q );
   // If n_scenes > 2, then this initialization and the modsource_editor one below will need to adjust
   int current_scene = 0, current_osc[n_scenes] = {0, 0}, current_fx = 0;

private:
   void openOrRecreateEditor();
   void makeStorePatchDialog();
   void close_editor();
   bool isControlVisible(ControlGroup controlGroup, int controlGroupEntry);
   void repushAutomationFor( Parameter *p );
   SurgeSynthesizer* synth = nullptr;
   bool editor_open = false;
   bool mod_editor = false;
   modsources modsource = ms_lfo1, modsource_editor[n_scenes] = {ms_lfo1, ms_lfo1};
   int fxbypass_tag = 0, f1subtypetag = 0, f2subtypetag = 0,
       filterblock_tag = 0, fmconfig_tag = 0;
   double lastTempo = 0;
   int lastTSNum = 0, lastTSDen = 0;
   void draw_infowindow(int ptag, VSTGUI::CControl* control, bool modulate, bool forceMB = false);
   void adjustSize(float &width, float &height) const;

   struct patchdata
   {
      std::string name;
      std::string category;
      std::string comments;
      std::string author;
   };

   void setBitmapZoomFactor(float zf);
   void showTooLargeZoomError(double width, double height, float zf) const;
   void showMinimumZoomError() const;

   /*
   ** Zoom Implementation 
   ** 
   ** Zoom works by the system maintaining a zoom factor (created by user actions)
   **
   ** All zoom factors are set in units of percentages as ints. So no zoom is "100",
   ** and double size is "200"
   */
   
   float zoomFactor = 100;
   float initialZoomFactor = 100;

   int patchCountdown = -1;
   
public:

   void populateDawExtraState(SurgeSynthesizer *synth) {
      auto des = &(synth->storage.getPatch().dawExtraState);

      des->isPopulated = true;
      des->editor.instanceZoomFactor = zoomFactor;
      des->editor.current_scene = current_scene;
      des->editor.current_fx = current_fx;
      des->editor.modsource = modsource;
      for( int i=0; i<n_scenes; ++i )
      {
         des->editor.current_osc[i] = current_osc[i];
         des->editor.modsource_editor[i] = modsource_editor[i];

         des->editor.msegStateIsPopulated = true;
         for (int lf = 0; lf < n_lfos; ++lf)
         {
            des->editor.msegEditState[i][lf].timeEditMode = msegEditState[i][lf].timeEditMode;
         }
      }
      des->editor.isMSEGOpen = ( editorOverlayTagAtClose == "msegEditor" );
   }

   void loadFromDAWExtraState(SurgeSynthesizer *synth) {
      auto des = &(synth->storage.getPatch().dawExtraState);
      if( des->isPopulated )
      {
         auto sz = des->editor.instanceZoomFactor;
         if (sz > 0)
            setZoomFactor(sz);
         current_scene = des->editor.current_scene;
         current_fx = des->editor.current_fx;
         modsource = des->editor.modsource;

         for (int i = 0; i < n_scenes; ++i)
         {
            current_osc[i] = des->editor.current_osc[i];
            modsource_editor[i] = des->editor.modsource_editor[i];
            if (des->editor.msegStateIsPopulated)
            {
               for (int lf = 0; lf < n_lfos; ++lf)
               {
                  msegEditState[i][lf].timeEditMode = des->editor.msegEditState[i][lf].timeEditMode;
               }
            }
         }
         if (des->editor.isMSEGOpen)
         {
            showMSEGEditorOnNextIdleOrOpen = true;
         }
      }
   }
   
   void setZoomCallback(std::function< void(SurgeGUIEditor *, bool resizeWindow) > f) {
       zoom_callback = f;
       setZoomFactor(getZoomFactor()); // notify the new callback
   }
   float getZoomFactor() const { return zoomFactor; }
   void setZoomFactor(float zf);
   void setZoomFactor(float zf, bool resizeWindow);
   void resizeWindow(float zf);
   bool doesZoomFitToScreen(float zf, float &correctedZf); // returns true if it fits; false if not; sets correctedZF to right size in either case

   void swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m );

   /*
   ** Callbacks from the Status Panel. If this gets to be too many perhaps make these an interface?
   */
   void showMPEMenu(VSTGUI::CPoint& where);
   void showTuningMenu(VSTGUI::CPoint& where);
   void showZoomMenu(VSTGUI::CPoint& where);
   void showLfoMenu( VSTGUI::CPoint &menuRect );

   void toggleMPE();
   void toggleTuning();
   void tuningFileDropped(std::string fn);
   void mappingFileDropped(std::string fn);
   std::string tuningCacheForToggle = "";
   std::string mappingCacheForToggle = "";
   std::string tuningToHtml();

   void swapControllers( int t1, int t2 );
   void openModTypeinOnDrop( int ms, VSTGUI::CControl *sl, int tgt );
   
   void queueRebuildUI() { queue_refresh = true; synth->refresh_editor = true; }

   std::string midiMappingToHtml();


   // These are unused right now
   enum SkinInspectorFlags {
      COLORS = 1 << 0,
      COMPONENTS = 1 << 1,

      ALL = COLORS | COMPONENTS
   };
   std::string skinInspectorHtml( SkinInspectorFlags f = ALL );

   /*
   ** Modulation Hover Support 
   */
   void sliderHoverStart( int tag );
   void sliderHoverEnd( int tag );

   int getWindowSizeX() const { return wsx; }
   int getWindowSizeY() const { return wsy; }

   void setEditorOverlay( VSTGUI::CView *c,
                          std::string editorTitle, // A window display title - whatever you want
                          std::string editorTag, // A tag by editor class. Please unique, no spaces.
                          const VSTGUI::CPoint &topleft = VSTGUI::CPoint( 0, 0 ),
                          bool modalOverlay = true,
                          bool hasCloseButton = true,
                          std::function<void()> onClose = [](){} );
   void dismissEditorOverlay();


   std::string getDisplayForTag( long tag );

   void queuePatchFileLoad( std::string file )
      {
         strncpy( synth->patchid_file, file.c_str(), FILENAME_MAX );
         synth->has_patchid_file = true;
      }

   void closeStorePatchDialog();
   void showStorePatchDialog();

   void lfoShapeChanged(int prior, int curr);
   void showMSEGEditor();
   void closeMSEGEditor();
   void toggleMSEGEditor();
   void broadcastMSEGState();
   int msegIsOpenFor = -1, msegIsOpenInScene = -1;
   bool showMSEGEditorOnNextIdleOrOpen = false;

   MSEGEditor::State msegEditState[n_scenes][n_lfos];
   MSEGEditor::State mostRecentCopiedMSEGState;

   int oscilatorMenuIndex[n_scenes][n_oscs] = {0};

   bool hasIdleRun = false;
   VSTGUI::CPoint resizeToOnIdle = VSTGUI::CPoint(-1,-1);
   /*
    * A countdown which will clearr the bitmap store offscreen caches
    * after N idles if set to a positive N. This is needed as some
    * DAWS (cough cough LIVE) handle draw and resize events in a
    * different order so the offscren cache when going classic->royal
    * at 100% (for instance) is the wrong size.
    */
   int clearOffscreenCachesAtZero = -1;

private:
   SGEDropAdapter *dropAdapter = nullptr;
   friend class SGEDropAdapter;
   bool canDropTarget(const std::string& fname); // these come as const char* from vstgui
   bool onDrop( const std::string& fname);

   VSTGUI::CRect positionForModulationGrid(modsources entry);

   int wsx = BASE_WINDOW_SIZE_X;
   int wsy = BASE_WINDOW_SIZE_Y;


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

public:
   void showAboutBox();
   void hideAboutBox();

private:
#if TARGET_VST3
   Steinberg::Vst::IContextMenu* addVst3MenuForParams(VSTGUI::COptionMenu *c,
                                                      const SurgeSynthesizer::ID &,
                                                      int &eid); // just a noop if you aren't a vst3 of course
#endif
   
   std::function< void(SurgeGUIEditor *, bool resizeWindow) > zoom_callback;
   bool zoomInvalid = false;
   int minimumZoom = 100;

   int selectedFX[8];
   std::string fxPresetName[8];
   
   std::shared_ptr<SurgeBitmaps> bitmapStore = nullptr;

   bool modsource_is_alternate[n_modsources];

public:
   void toggleAlternateFor(VSTGUI::CControl* c);

private:
   VSTGUI::CControl *vu[16];
   VSTGUI::CControl *infowindow, *patchname, *ccfxconf = nullptr;
   VSTGUI::CControl *statusMPE = nullptr, *statusTune = nullptr, *statusZoom = nullptr;
   CAboutBox* aboutbox = nullptr;
   VSTGUI::CTextEdit *patchName = nullptr;
   VSTGUI::CTextEdit *patchCategory = nullptr;
   VSTGUI::CTextEdit *patchCreator = nullptr;
   VSTGUI::CTextEdit *patchComment = nullptr;
   VSTGUI::CCheckBox *patchTuning = nullptr;
   VSTGUI::CTextLabel *patchTuningLabel = nullptr;
#if BUILD_IS_DEBUG
   VSTGUI::CTextLabel *debugLabel = nullptr;
#endif

   VSTGUI::CViewContainer *typeinDialog = nullptr;
   VSTGUI::CTextEdit *typeinValue = nullptr;
   VSTGUI::CTextLabel *typeinLabel = nullptr;
   VSTGUI::CTextLabel *typeinPriorValueLabel = nullptr;
   VSTGUI::CControl *typeinEditControl = nullptr;
   VSTGUI::CControl *msegEditSwitch = nullptr;
   enum TypeInMode {
      Inactive,
      Param,
      Control
   } typeinMode = Inactive;
   std::vector<VSTGUI::CViewContainer*> removeFromFrame;
   int typeinResetCounter = -1;
   std::string typeinResetLabel = "";

   VSTGUI::CViewContainer *editorOverlay = nullptr;
   VSTGUI::CView* editorOverlayContentsWeakReference =
       nullptr; // Use this very very carefully. It may hold a dangling ref until #3223
   std::function<void()> editorOverlayOnClose = [](){};

   VSTGUI::CViewContainer *minieditOverlay = nullptr;
   VSTGUI::CTextEdit *minieditTypein = nullptr;
   std::function<void( const char* )> minieditOverlayDone = [](const char *){};
public:
   std::string editorOverlayTag, editorOverlayTagAtClose;
   void promptForMiniEdit(const std::string& value,
                          const std::string& prompt,
                          const std::string& title,
                          const VSTGUI::CPoint& where,
                          std::function<void(const std::string&)> onOK);
private:
   
   VSTGUI::CTextLabel* lfoNameLabel = nullptr;
   VSTGUI::CTextLabel* fxPresetLabel = nullptr;

   std::string modulatorName(int ms, bool forButton);
   
   Parameter *typeinEditTarget = nullptr;
   int typeinModSource = -1;
   
   VSTGUI::CControl* polydisp = nullptr;
   VSTGUI::CControl* oscdisplay = nullptr;
   VSTGUI::CControl* splitpointControl = nullptr;

   static const int n_paramslots = 1024;
   VSTGUI::CControl* param[n_paramslots] = {};
   VSTGUI::CControl* nonmod_param[n_paramslots] = {}; 
   CModulationSourceButton* gui_modsrc[n_modsources] = {};
   VSTGUI::CControl* metaparam[n_customcontrollers] = {};
   VSTGUI::CControl* lfodisplay = nullptr;
   VSTGUI::CControl* filtersubtype[2] = {};
   VSTGUI::CControl* fxmenu = nullptr;
   int clear_infoview_countdown = 0;
public:
   int clear_infoview_peridle = -1;
   bool useDevMenu = false;
private:
   float blinktimer = 0;
   bool blinkstate = false;
   PARENT_PLUGIN_TYPE* _effect = nullptr;
   void* _userdata = nullptr;
   VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> _idleTimer;
   int firstIdleCountdown = 0;

   /*
   ** Utility Function
   */
   VSTGUI::CCommandMenuItem*
   addCallbackMenu(VSTGUI::COptionMenu* toThis, std::string label, std::function<void()> op);

   VSTGUI::COptionMenu*
   makeSmoothMenu(VSTGUI::CRect& menuRect,
                  const std::string& key,
                  int defaultValue,
                  std::function<void(ControllerModulationSource::SmoothingMode)> setSmooth);

   VSTGUI::COptionMenu* makeMpeMenu(VSTGUI::CRect &rect, bool showhelp);
   VSTGUI::COptionMenu* makeTuningMenu(VSTGUI::CRect& rect, bool showhelp);
   VSTGUI::COptionMenu* makeZoomMenu(VSTGUI::CRect& rect, bool showhelp);
   VSTGUI::COptionMenu* makeSkinMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeUserSettingsMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeDataMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeMidiMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeDevMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeLfoMenu(VSTGUI::CRect &rect);
   VSTGUI::COptionMenu* makeMonoModeOptionsMenu(VSTGUI::CRect &rect, bool updateDefaults );
   bool scannedForMidiPresets = false;

   void resetSmoothing( ControllerModulationSource::SmoothingMode t );
   void resetPitchSmoothing(ControllerModulationSource::SmoothingMode t);

public:
   std::string helpURLFor( Parameter *p ); // this requires internal state so doesn't have statics
   std::string helpURLForSpecial( std::string special ); // these can be either this way or static

   static std::string helpURLForSpecial( SurgeStorage *, std::string special );
   static std::string fullyResolvedHelpURL( std::string helpurl );

private:

#if TARGET_VST3 
OBJ_METHODS(SurgeGUIEditor, EditorType) 
DEFINE_INTERFACES
DEF_INTERFACE(Steinberg::IPlugViewContentScaleSupport) 
END_DEFINE_INTERFACES(EditorType)
REFCOUNT_METHODS(EditorType) 
#endif

   void promptForUserValueEntry(Parameter *p, VSTGUI::CControl *c, int modulationSource = -1);
   
   /*
   ** Skin support
   */
   Surge::UI::Skin::ptr_t currentSkin;
   void setupSkinFromEntry( const Surge::UI::SkinDB::Entry &entry );
   void reloadFromSkin();
   VSTGUI::CControl *layoutComponentForSkin(std::shared_ptr<Surge::UI::Skin::Control> skinCtrl,
                               long tag,
                               int paramIndex = -1,
                               Parameter *p = nullptr,
                               int style = 0
                               );

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

