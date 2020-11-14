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


#include "SurgeGUIEditor.h"
#include "resource.h"
#include "CSurgeSlider.h"
#include "CHSwitch2.h"
#include "CSwitchControl.h"
#include "CParameterTooltip.h"
#include "CPatchBrowser.h"
#include "COscillatorDisplay.h"
#include "CVerticalLabel.h"
#include "CModulationSourceButton.h"
#include "CSnapshotMenu.h"
#include "CLFOGui.h"
#include "CEffectSettings.h"
#include "CSurgeVuMeter.h"
#include "CMenuAsSlider.h"
#include "CEffectLabel.h"
#include "CAboutBox.h"
#include "vstcontrols.h"
#include "SurgeBitmaps.h"
#include "CScalableBitmap.h"
#include "CNumberField.h"
#include "UserInteractions.h"
#include "DisplayInfo.h"
#include "UserDefaults.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "UIInstrumentation.h"
#include "guihelpers.h"
#include "DebugHelpers.h"
#include "ModulatorPresetManager.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <codecvt>
#include "MSEGEditor.h"

#if TARGET_VST3
#include "pluginterfaces/vst/ivstcontextmenu.h"
#include "pluginterfaces/base/ustring.h"

#include "vstgui/lib/cvstguitimer.h"

#include "SurgeVst3Processor.h"


template< typename T >
struct RememberForgetGuard {
   RememberForgetGuard( T *tg )
   {
      t = tg;

      int rc = -1;
      if( t )
         rc = t->addRef();
   }
   RememberForgetGuard( const RememberForgetGuard &other )
   {
      int rc = -1;
      if( t )
      {
         rc = t->release();
      }
      t = other.t;
      if( t )
      {
         rc = t->addRef();
      }
   }
   ~RememberForgetGuard() {
      if( t )
      {
         t->release();
      }
   }
   T *t = nullptr;
};

#endif

#if TARGET_AUDIOUNIT
#include "aulayer.h"
#endif

#include "filesystem/import.h"

#if LINUX
#include "vstgui/lib/platform/platform_x11.h"
#include "vstgui/lib/platform/linux/x11platform.h"
#endif

#if LINUX && TARGET_LV2
namespace SurgeLv2
{
    VSTGUI::SharedPointer<VSTGUI::X11::IRunLoop> createRunLoop(void *ui);
}
#endif

#include "RuntimeFont.h"

const int yofs = 10;

using namespace VSTGUI;
using namespace std;

#if LINUX && TARGET_VST3
extern void LinuxVST3Init(Steinberg::Linux::IRunLoop* pf);
extern void LinuxVST3Detatch();
extern void LinuxVST3FrameOpen(CFrame* that, void*, const VSTGUI::PlatformType& pt);
extern void LinuxVST3Idle();
#endif

CFontRef displayFont = NULL;
CFontRef patchNameFont = NULL;
CFontRef lfoTypeFont = NULL;
CFontRef aboutFont = NULL;


enum special_tags
{
   tag_scene_select = 1,
   tag_osc_select,
   tag_osc_menu,
   tag_fx_select,
   tag_fx_menu,
   tag_patchname,
   tag_mp_category,
   tag_mp_patch,
   tag_store,
   tag_store_cancel,
   tag_store_ok,
   tag_store_name,
   tag_store_category,
   tag_store_creator,
   tag_store_comments,
   tag_store_tuning,
   tag_mod_source0,
   tag_mod_source_end = tag_mod_source0 + n_modsources,
   tag_settingsmenu,
   tag_mp_jogfx,
   tag_value_typein,
   tag_editor_overlay_close,
   tag_miniedit_ok,
   tag_miniedit_cancel,

   tag_status_mpe,
   tag_status_zoom,
   tag_status_tune,

   tag_mseg_edit,
   tag_lfo_menu,
   // tag_metaparam,
   // tag_metaparam_end = tag_metaparam+n_customcontrollers,
   start_paramtags,
};

// TODO: CHECK REFERENCE COUNTING
struct SGEDropAdapter : public VSTGUI::IDropTarget, public VSTGUI::ReferenceCounted<int> {
   SurgeGUIEditor *buddy = nullptr;
   SGEDropAdapter( SurgeGUIEditor *buddy ) {
      this->buddy = buddy;
      //std::cout << "Adapter created" << std::endl;
   }
   ~SGEDropAdapter()
   {
      //std::cout << "Adapter Destroyed" << std::endl;
   }

   std::string singleFileFName(VSTGUI::DragEventData data)
   {
      auto drag = data.drag;
      auto where = data.pos;
      uint32_t ct = drag->getCount();
      if (ct == 1)
      {
         IDataPackage::Type t = drag->getDataType(0);
         if (t == IDataPackage::kFilePath)
         {
            const void* fn;
            drag->getData(0, fn, t);
            const char* fName = static_cast<const char*>(fn);
            return fName;
         }
      }
      return "";
   }

   virtual VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override {
      auto fn = singleFileFName( data );

      if( buddy && buddy->canDropTarget(fn) )
         return VSTGUI::DragOperation::Copy;
      // Hand this decision off to SGE
      return VSTGUI::DragOperation::None;
   }
   virtual VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override
   {
      auto fn = singleFileFName( data );

      if( buddy && buddy->canDropTarget(fn) )
         return VSTGUI::DragOperation::Copy;

      return VSTGUI::DragOperation::None;
   }
   virtual void onDragLeave(VSTGUI::DragEventData data) override
   {
   }
   virtual bool onDrop (VSTGUI::DragEventData data) override
   {
      auto fn = singleFileFName(data);
      if( buddy )
         return buddy->onDrop( fn );
      return false;
   }
};

int SurgeGUIEditor::start_paramtag_value = start_paramtags;

bool SurgeGUIEditor::fromSynthGUITag(SurgeSynthesizer *synth, int tag, SurgeSynthesizer::ID& q)
{
   // This is wrong for macros and params but is close
   return synth->fromSynthSideIdWithGuiOffset(tag, start_paramtags, tag_mod_source0 + ms_ctrl1, q );
}

SurgeGUIEditor::SurgeGUIEditor(PARENT_PLUGIN_TYPE* effect, SurgeSynthesizer* synth, void* userdata) : super(effect)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeGUIEditor::SurgeGUIEditor" );
#endif
   frame = 0;

#if TARGET_VST3
   // setIdleRate(25);
   // synth = ((SurgeProcessor*)effect)->getSurge();
#endif


   auto des = &(synth->storage.getPatch().dawExtraState);
   patchname = 0;
   current_scene = des->editor.current_scene;
   current_fx = des->editor.current_fx;
   modsource = des->editor.modsource;
   blinktimer = 0.f;
   blinkstate = false;
   aboutbox = 0;
   patchCountdown = -1;

   for (int i = 0; i < n_scenes; i++)
   {
      current_osc[i] = des->editor.current_osc[i];
      modsource_editor[i] = des->editor.modsource_editor[i];
   }
   
   mod_editor = false;

   // init the size of the plugin
   int userDefaultZoomFactor = Surge::Storage::getUserDefaultValue(&(synth->storage), "defaultZoom", 100);
   float zf = userDefaultZoomFactor / 100.0;

   if( synth->storage.getPatch().dawExtraState.editor.instanceZoomFactor > 0 )
   {
       // If I restore state before I am constructed I need to do this
       zf = synth->storage.getPatch().dawExtraState.editor.instanceZoomFactor / 100.0;
   }

   rect.left = 0;
   rect.top = 0;
#if TARGET_VST2 || TARGET_VST3
   rect.right = getWindowSizeX() * zf;
   rect.bottom = getWindowSizeY() * zf;
#else
   rect.right = getWindowSizeX();
   rect.bottom = getWindowSizeY();
#endif
   editor_open = false;
   queue_refresh = false;
   memset(param, 0, n_paramslots * sizeof(void*));
   polydisp = 0; // FIXME - when changing skins and rebuilding we need to reset these state variables too
   splitpointControl = 0;
   clear_infoview_countdown = -1;
   vu[0] = 0;
   vu[1] = 0;
   vu[2] = 0;
   vu[3] = 0;
   vu[4] = 0;
   vu[5] = 0;
   vu[6] = 0;
   vu[7] = 0;
   vu[8] = 0;
   vu[9] = 0;
   vu[10] = 0;
   vu[11] = 0;
   vu[12] = 0;
   vu[13] = 0;
   vu[14] = 0;
   vu[15] = 0;
   lfodisplay = 0;
   fxmenu = 0;
   for( int i=0; i<8; ++i )
   {
      selectedFX[i] = -1;
      fxPresetName[i] = "";
   }

   _effect = effect;
   _userdata = userdata;
   this->synth = synth;

   minimumZoom = 50;
#if LINUX
   minimumZoom = 100; // See github issue #628
#endif

   zoom_callback = [](SurgeGUIEditor* f) {};
   setZoomFactor(userDefaultZoomFactor);
   zoomInvalid = (userDefaultZoomFactor != 100);

   /*
   ** As documented in RuntimeFonts.h, the contract of this function is to side-effect
   ** onto globals displayFont and patchNameFont with valid fonts from the runtime
   ** distribution
   */
   Surge::GUI::initializeRuntimeFont();

   if (displayFont == NULL)
   {
      /*
      ** OK the runtime load didn't work. Fall back to
      ** the old defaults
      **
      ** FIXME: One day we will be confident enough in
      ** our dyna loader to make this a Surge::UserInteraction::promptError
      ** warning also.
      **
      ** For now, copy the defaults from above. (Don't factor this into
      ** a function since the above defaults are initialized as dll
      ** statics if we are not runtime).
      */
#if MAC
      SharedPointer<CFontDesc> minifont = new CFontDesc("Lucida Grande", 9);
      SharedPointer<CFontDesc> patchfont = new CFontDesc("Lucida Grande", 14);
      SharedPointer<CFontDesc> lfofont = new CFontDesc("Lucida Grande", 8);
      SharedPointer<CFontDesc> aboutfont = new CFontDesc("Lucida Grande", 10);
#elif LINUX
      SharedPointer<CFontDesc> minifont = new CFontDesc("sans-serif", 9);
      SharedPointer<CFontDesc> patchfont = new CFontDesc("sans-serif", 14);
      SharedPointer<CFontDesc> lfofont = new CFontDesc("sans-serif", 8);
      SharedPointer<CFontDesc> aboutfont = new CFontDesc("sans-serif", 10);
#else
      SharedPointer<CFontDesc> minifont = new CFontDesc("Microsoft Sans Serif", 9);
      SharedPointer<CFontDesc> patchfont = new CFontDesc("Arial", 14);
      SharedPointer<CFontDesc> lfofont = new CFontDesc("Microsoft Sans Serif", 8 );
      SharedPointer<CFontDesc> aboutfont = new CFontDesc("Microsoft Sans Serif", 10 );
#endif

      displayFont = minifont;
      patchNameFont = patchfont;
      lfoTypeFont = lfofont;
      aboutFont = aboutfont;

   }

   /*
   ** See the comment in SurgeParamConfig.h
   */
   if((int)Surge::ParamConfig::kHorizontal != (int)VSTGUI::CSlider::kHorizontal ||
      (int)Surge::ParamConfig::kVertical != (int)VSTGUI::CSlider::kVertical
      )
   {
      throw new Surge::Error("Software Error: Param Mismatch" );
   }

   for( int i=0; i<n_modsources; ++i )
      modsource_is_alternate[i] = false;

   currentSkin = Surge::UI::SkinDB::get().defaultSkin(&(this->synth->storage));
   reloadFromSkin();
}

SurgeGUIEditor::~SurgeGUIEditor()
{
   auto isPop = synth->storage.getPatch().dawExtraState.isPopulated;
   populateDawExtraState(synth); // If I must die, leave my state for future generations
   synth->storage.getPatch().dawExtraState.isPopulated = isPop;
   if (frame)
   {
      getFrame()->unregisterKeyboardHook(this);
      frame->close();
   }
   if( dropAdapter )
   {
      dropAdapter->buddy = nullptr;
      dropAdapter->forget();
      dropAdapter = nullptr;
   }

}

void SurgeGUIEditor::idle()
{
#if TARGET_VST2 && LINUX
   if (!super::idle2())
      return;
#endif
#if TARGET_VST3 && LINUX
   LinuxVST3Idle();
#endif
#if TARGET_VST3
   if( _effect )
      _effect->uithreadIdleActivity();
#endif

   if (!synth)
      return;

   if (editor_open && frame && !synth->halt_engine)
   {
      if( firstIdleCountdown )
      {
         // Linux VST3 in JUCE Hosts (maybe others?) sets up the run loop out of order, it seems
         // sometimes missing the very first invalidation. Force a redraw on the first idle
         // isFirstIdle = false;
         firstIdleCountdown --;

         frame->invalid();
      }
      for( auto c : removeFromFrame )
      {
         if( frame->isChild( c ) )
         {
            frame->removeView(c);
         }
      }
      removeFromFrame.clear();


      if( patchCountdown >= 0 )
      {
         patchCountdown --;
         if( patchCountdown < 0 && synth->patchid_queue >= 0 )
         {
            std::ostringstream oss;

            oss << "Loading patch " <<  synth->patchid_queue << " has not occured after 30 idle cycles. "
                << "This usually means that the audio system is not running in your host. Audio system has to be "
                << "running in order to load Surge patches.";
            Surge::UserInteractions::promptError(oss.str(), "Patch Loading Error" );

         }
      }

      if(zoomInvalid)
      {
         setZoomFactor(getZoomFactor());
         zoomInvalid = false;
      }

      /*static CDrawContext drawContext
        (frame, NULL, systemWindow);*/
      // CDrawContext *drawContext = frame->createDrawContext();

      CView* v = frame->getFocusView();
      if (v && dynamic_cast<CControl*>(v) != nullptr)
      {
         int ptag = ((CControl*)v)->getTag() - start_paramtags;
         if (ptag >= 0)
         {
            synth->storage.modRoutingMutex.lock();

            for (int i = 1; i < n_modsources; i++)
            {
               if( gui_modsrc[i] )
                  ((CModulationSourceButton*)gui_modsrc[i])
                     ->update_rt_vals(synth->isActiveModulation(ptag, (modsources)i), 0,
                                      synth->isModsourceUsed((modsources)i));
            }
            synth->storage.modRoutingMutex.unlock();
         }
      }
      else
      {
         synth->storage.modRoutingMutex.lock();
         for (int i = 1; i < n_modsources; i++)
         {
            if( gui_modsrc[i] )
               ((CModulationSourceButton*)gui_modsrc[i])
                  ->update_rt_vals(false, 0, synth->isModsourceUsed((modsources)i));
         }
         synth->storage.modRoutingMutex.unlock();
      }

      if (synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]].wt.refresh_display)
      {
         synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]].wt.refresh_display = false;
         if (oscdisplay)
         {
            oscdisplay->setDirty(true);
            oscdisplay->invalid();
         }
      }

      if( typeinResetCounter > 0 )
      {
         typeinResetCounter--;
         if( typeinResetCounter <= 0 && typeinDialog )
         {
            typeinLabel->setText( typeinResetLabel.c_str() );
            typeinLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Light));
            typeinLabel->invalid();
         }
      }

#if OSC_MOD_ANIMATION
      if (mod_editor && oscdisplay)
      {
         ((COscillatorDisplay*)oscdisplay)->tickModTime();
         oscdisplay->setDirty(true);
         oscdisplay->invalid();
      }
#endif

      if (polydisp)
      {
         CNumberField *cnpd = static_cast< CNumberField* >( polydisp );
         int prior = cnpd->getPoly();
         cnpd->setPoly( synth->polydisplay );
         if( prior != synth->polydisplay )
            cnpd->invalid();
      }

      bool patchChanged = false;
      if (patchname)
      {
         patchChanged = ((CPatchBrowser *)patchname)->sel_id != synth->patchid;
      }

      if( statusMPE )
      {
         auto v = statusMPE->getValue();
         if( ( v < 0.5 && synth->mpeEnabled) ||
             ( v > 0.5 && ! synth->mpeEnabled) )
         {
            statusMPE->setValue( synth->mpeEnabled ? 1 : 0 );
            statusMPE->invalid();
         }
      }

      if( statusTune )
      {
         auto v = statusTune->getValue();
         if( ( v < 0.5 && ! synth->storage.isStandardTuning ) ||
             ( v > 0.5 && synth->storage.isStandardTuning ) )
         {
            statusTune->setValue( ! synth->storage.isStandardTuning );
            statusTune->invalid();
         }
      }

      if( patchChanged )
      {
#if TARGET_AUDIOUNIT
        synth-> getParent()->setPresetByID(synth->patchid);
#endif
        for( int i=0; i<8; ++i )
        {
           fxPresetName[i] = "";
        }
      }


      if (queue_refresh || synth->refresh_editor || patchChanged)
      {
         queue_refresh = false;
         synth->refresh_editor = false;

         if (frame)
         {
            if (synth->patch_loaded)
               mod_editor = false;
            synth->patch_loaded = false;

            openOrRecreateEditor();
         }
         if (patchname)
         {
            ((CPatchBrowser*)patchname)->sel_id = synth->patchid;
            ((CPatchBrowser*)patchname)->setLabel(synth->storage.getPatch().name);
            ((CPatchBrowser*)patchname)->setCategory(synth->storage.getPatch().category);
            ((CPatchBrowser*)patchname)->setAuthor(synth->storage.getPatch().author);
            patchname->invalid();
         }
      }

      bool vuInvalid = false;
      if (synth->vu_peak[0] != vu[0]->getValue())
      {
         vuInvalid = true;
         vu[0]->setValue(synth->vu_peak[0]);
      }
      if (synth->vu_peak[1] != ((CSurgeVuMeter*)vu[0])->getValueR())
      {
         ((CSurgeVuMeter*)vu[0])->setValueR(synth->vu_peak[1]);
         vuInvalid = true;
      }
      if (vuInvalid)
         vu[0]->invalid();

      for (int i = 0; i < 8; i++)
      {
         assert(i + 1 < Effect::KNumVuSlots);
         if (vu[i + 1] && synth->fx[current_fx])
         {
            // there's seems to be a bug here that overwrites either this or the vu-pointer
            // try to catch it earlier to retrieve more info

            // assert(!((int)vu[i+1] & 0xffff0000));

            // check so it doesn't overlap with the infowindow
            CRect iw = ((CParameterTooltip*)infowindow)->getViewSize();
            CRect vur = vu[i + 1]->getViewSize();

            if (!((CParameterTooltip*)infowindow)->isVisible() || !vur.rectOverlap(iw))
            {
               vu[i + 1]->setValue(synth->fx[current_fx]->vu[(i << 1)]);
               ((CSurgeVuMeter*)vu[i + 1])->setValueR(synth->fx[current_fx]->vu[(i << 1) + 1]);
               vu[i + 1]->invalid();
            }
         }
      }

      for (int i = 0; i < 8; i++)
      {
         if (synth->refresh_ctrl_queue[i] >= 0)
         {
            int j = synth->refresh_ctrl_queue[i];
            synth->refresh_ctrl_queue[i] = -1;

            if (param[j])
            {
               char pname[256], pdisp[256];
               SurgeSynthesizer::ID jid;
               if( synth->fromSynthSideId(j, jid) )
               {
                  synth->getParameterName(jid, pname);
                  synth->getParameterDisplay(jid, pdisp);
               }

               /*if(i == 0)
                 {
                 ((gui_pdisplay*)infowindow)->setLabel(pname,pdisp);
                 draw_infowindow(j, param[j], false, true);
                 clear_infoview_countdown = 40;
                 }*/

               param[j]->setValue(synth->refresh_ctrl_queue_value[i]);
               frame->invalidRect(param[j]->getViewSize());
               // oscdisplay->invalid();

               if( oscdisplay )
               {
                  ((COscillatorDisplay*)oscdisplay)->invalidateIfIdIsInRange(j);
               }

               if( lfodisplay )
               {
                  ((CLFOGui*)lfodisplay)->invalidateIfIdIsInRange(j);
               }

            }
         }
      }

      if(lastTempo != synth->time_data.tempo ||
         lastTSNum != synth->time_data.timeSigNumerator ||
         lastTSDen != synth->time_data.timeSigDenominator
         )
      {
         lastTempo = synth->time_data.tempo;
         lastTSNum = synth->time_data.timeSigNumerator;
         lastTSDen = synth->time_data.timeSigDenominator;
         if( lfodisplay )
         {
            ((CLFOGui*)lfodisplay)->setTimeSignature(synth->time_data.timeSigNumerator,
                                                     synth->time_data.timeSigDenominator);
            ((CLFOGui*)lfodisplay)->invalidateIfAnythingIsTemposynced();
         }
      }

      if( synth->refresh_overflow )
      {
         // Basicall yreset everything and repaint.
         synth->refresh_overflow = false;
         for( int i=0; i<8; ++i )
            synth->refresh_parameter_queue[i] = -1;
         for( int i=0; i<n_total_params; ++i )
         {
            auto p = param[i];
            if( ! p ) p = nonmod_param[i];
            if( p )
            {
               SurgeSynthesizer::ID jid;
               if( synth->fromSynthSideId(i, jid ))
                  if( synth->getParameter01(jid) != p->getValue() )
                     p->setValue(synth->getParameter01(jid));
            }

         }
         for( int i=0; i<n_customcontrollers; ++i )
         {
            gui_modsrc[ms_ctrl1 + i]->setValue(
                ((ControllerModulationSource*)synth->storage.getPatch()
                    .scene[current_scene].modsources[ms_ctrl1 + i])->get_target01());

         }
         frame->invalid();
      }
      else for (int i = 0; i < 8; i++)
      {
         if (synth->refresh_parameter_queue[i] >= 0)
         {
            int j = synth->refresh_parameter_queue[i];
            synth->refresh_parameter_queue[i] = -1;
            if ((j < n_total_params) && param[j])
            {
               SurgeSynthesizer::ID jid;
               if( synth->fromSynthSideId(j, jid ))
                  param[j]->setValue(synth->getParameter01(jid));
               frame->invalidRect(param[j]->getViewSize());

               if( oscdisplay )
               {
                  ((COscillatorDisplay*)oscdisplay)->invalidateIfIdIsInRange(j);
               }

               if( lfodisplay )
               {
                  ((CLFOGui*)lfodisplay)->invalidateIfIdIsInRange(j);
               }

            }
            else if ((j >= metaparam_offset) && (j < (metaparam_offset + n_customcontrollers)))
            {
               int cc = j - metaparam_offset;
               gui_modsrc[ms_ctrl1 + cc]->setValue(
                  ((ControllerModulationSource*)synth->storage.getPatch()
                   .scene[current_scene].modsources[ms_ctrl1 + i])->get_target01());
            }
            else if((j>=0) && (j < n_total_params) && nonmod_param[j])
            {
               /*
               ** What the heck is this NONMOD_PARAM thing?
               **
               ** There are a set of params - like discrete things like
               ** octave and filter type - which are not LFO modulatable
               ** and aren't in the params[] array. But they are exposed
               ** properties, so you can control them from a DAW. The
               ** DAW control works - everything up to this path (as described
               ** in #160) works fine and sets the value but since there's
               ** no CControl in param the above bails out. But ading
               ** all these controls to param[] would have the unintended
               ** side effect of giving them all the other param[] behaviours.
               ** So have a second array and drop select items in here so we
               ** can actually get them redrawing when an external param set occurs.
               */
               CControl *cc = nonmod_param[ j ];

               /*
               ** Some state changes enable and disable sliders. If this is one of those state changes and a value has changed, then
               ** we need to invalidate them. See #2056.
               */
               auto tag = cc->getTag();
               SurgeSynthesizer::ID jid;

               auto sv = 0.f;
               if( synth->fromSynthSideId(j, jid ))
                  sv = synth->getParameter01(jid);
               auto cv = cc->getValue();

               if ((sv != cv) && ((tag == fmconfig_tag || tag == filterblock_tag)))
               {
                  std::unordered_map<int, bool> resetMap;

                  if (tag == fmconfig_tag)
                  {
                     auto targetTag = synth->storage.getPatch().scene[current_scene].fm_depth.id + start_paramtags;
                     auto targetState = (round(sv * n_fm_configuration) == fm_off);
                     resetMap[targetTag] = targetState;
                  }

                  if (tag == filterblock_tag)
                  {
                     auto pval = round(sv * n_fb_configuration);

                     auto targetTag = synth->storage.getPatch().scene[current_scene].feedback.id + start_paramtags;
                     auto targetState = (pval == fb_serial);
                     resetMap[targetTag] = targetState;

                     targetTag = synth->storage.getPatch().scene[current_scene].width.id + start_paramtags;
                     targetState = (pval != fb_stereo && pval != fb_wide);
                     resetMap[targetTag] = targetState;
                  }

                  for (int i = 0; i < n_paramslots; i++)
                  {
                     if (param[i] && (resetMap.find(param[i]->getTag()) != resetMap.end()))
                     {
                        auto css = dynamic_cast<CSurgeSlider*>(param[i]);

                        if (css)
                        {
                           css->disabled = resetMap[param[i]->getTag()];
                           css->invalid();
                        }
                     }
                  }
               }

#if TARGET_VST2
               /*
               ** This is a gross hack. The right thing is to have a remapper lambda on the control.
               ** But for now we have this. The VST2 calls back into here when you setvalue to (basically)
               ** double set value. But for the scenemod this means that the transformation doesn't occur
               ** so you get a dance. Since we don't really care if scenemode is automatable for now we just do
               */
               if( synth->storage.getPatch().param_ptr[j]->ctrltype != ct_scenemode )
                  cc->setValue(synth->getParameter01(jid));
#else
               cc->setValue(synth->getParameter01(jid));
#endif

               // Integer switches also work differently
               auto assw = dynamic_cast<CSwitchControl *>(cc);
               if( assw )
               {
                  if( assw->is_itype )
                  {
                     assw->ivalue = synth->storage.getPatch().param_ptr[j]->val.i + 1;
                  }
               }

               cc->setDirty();
               cc->invalid();
            }
#if 0
            /*
            ** A set of special things which invalidate the entire UI.
            ** See #2226 for why this is currently off.
            */
            else if( j == synth->storage.getPatch().scene_active.id )
            {
               if( current_scene != synth->storage.getPatch().scene_active.val.i )
               {
                  synth->refresh_editor = true;
                  current_scene = synth->storage.getPatch().scene_active.val.i;
               }

            }
#endif
            else
            {
               // printf( "Bailing out of all possible refreshes on %d\n", j );
               // This is not really a problem
            }
         }
      }
      for (int i = 0; i < n_customcontrollers; i++)
      {
         if (((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + i])->has_changed(true))
         {
            gui_modsrc[ms_ctrl1 + i]->setValue(
               ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + i])->get_target01());
         }
      }
      clear_infoview_countdown += clear_infoview_peridle;
      if (clear_infoview_countdown == 0)
      {
         ((CParameterTooltip*)infowindow)->Hide();
         // infowindow->getViewSize();
         // ctnvg       frame->redrawRect(drawContext,r);
      }

      // frame->update(&drawContext);
      // frame->idle();
  }

#if BUILD_IS_DEBUG
  if( debugLabel )
  {
     /*
      * We can do debuggy stuff here to get an indea about internal state on the screen
      */

   /*
     auto t = std::to_string( synth->storage.songpos );
     debugLabel->setText(t.c_str());
     debugLabel->invalid();
     */


  }
#endif
}

void SurgeGUIEditor::toggle_mod_editing()
{
   mod_editor = !mod_editor;
   refresh_mod();
}

void SurgeGUIEditor::refresh_mod()
{
   CModulationSourceButton *cms = gui_modsrc[modsource];

   modsources thisms = modsource;
   if( cms->hasAlternate && cms->useAlternate )
      thisms = (modsources)cms->alternateId;

   synth->storage.modRoutingMutex.lock();
   for (int i = 0; i < n_paramslots; i++)
   {
      if (param[i])
      {
         auto *s = dynamic_cast<CSurgeSlider*>(param[i]);
         if( s )
         {
            if (s->is_mod)
            {
               s->setModMode(mod_editor ? 1 : 0);
               s->setModPresent(synth->isModDestUsed(i));
               s->setModCurrent(synth->isActiveModulation(i, thisms), synth->isBipolarModulation(thisms));
            }
            // s->setDirty();
            s->setModValue(synth->getModulation(i, thisms));
            s->invalid();
         }
      }
   }
#if OSC_MOD_ANIMATION
   if (oscdisplay)
   {
      ((COscillatorDisplay*)oscdisplay)->setIsMod(mod_editor);
      ((COscillatorDisplay*)oscdisplay)->setModSource(thisms);
      oscdisplay->invalid();
      oscdisplay->setDirty(true);
   }
#endif

   synth->storage.modRoutingMutex.unlock();

   for (int i = 1; i < n_modsources; i++)
   {
      int state = 0;
      
      if (i == modsource)
         state = mod_editor ? 2 : 1;
      
      if (i == modsource_editor[current_scene])
      {
         state |= 4;

         // update the LFO title label
         std::string modname = modulatorName(modsource_editor[current_scene], true);

         lfoNameLabel->setText(modname.c_str());
      }

      if (gui_modsrc[i])
      {
         // this could change if I cleared the last one
         gui_modsrc[i]->used = synth->isModsourceUsed( (modsources)i );
         gui_modsrc[i]->state = state;

         if( i < ms_ctrl1 || i > ms_ctrl8 )
         {
            auto mn = modulatorName(i, true);
            gui_modsrc[i]->setlabel(mn.c_str() );
         }
         gui_modsrc[i]->invalid();
      }
   }
   // ctnvg frame->redraw();
   // frame->setDirty();
   #if LINUX
      frame->invalid();
      // Turns out linux is very bad with lots of little invalid rects in vstgui
      // see github issue 1103
   #endif
}

int32_t SurgeGUIEditor::onKeyDown(const VstKeyCode& code, CFrame* frame)
{
   if(code.virt != 0 )
   {
      switch (code.virt)
      {
      case VKEY_TAB:
         if ( (editorOverlay && editorOverlayTag == "storePatch" ) || (typeinDialog && typeinDialog->isVisible() ) )
         {
            /*
            ** SaveDialog gets access to the tab key to switch between fields if it is open
            */
            return -1;
         }
         toggle_mod_editing();
         return 1;
         break;
      case VKEY_ESCAPE:
         if( typeinDialog && typeinDialog->isVisible() )
         {
            // important to do this first since it basically stops us listening to changes
            typeinEditTarget = nullptr;

            typeinDialog->setVisible(false);
            removeFromFrame.push_back( typeinDialog );
            typeinDialog = nullptr;
            typeinMode = Inactive;
            typeinResetCounter = -1;


            return 1;
         }
         break;
      case VKEY_RETURN:
         if( typeinDialog && typeinDialog->isVisible() )
         {
            typeinDialog->setVisible(false);
         }
         break;
      }
   }
   return -1;
}

int32_t SurgeGUIEditor::onKeyUp(const VstKeyCode& keyCode, CFrame* frame)
{
   return -1;
}

bool SurgeGUIEditor::isControlVisible(ControlGroup controlGroup, int controlGroupEntry)
{
   switch (controlGroup)
   {
   case cg_OSC:
      return (controlGroupEntry == current_osc[current_scene]);
   case cg_LFO:
      return (controlGroupEntry == modsource_editor[current_scene]);
   case cg_FX:
      return (controlGroupEntry == current_fx);
   default:
      return true;
   }
}

CRect SurgeGUIEditor::positionForModulationGrid(modsources entry)
{
   bool isMacro = isCustomController(entry);
   int gridX = modsource_grid_xy[entry][0];
   int gridY = modsource_grid_xy[entry][1];
   int width = isMacro ? 93 : 74;

   // to ensure the same gap between the modbuttons,
   // make the first and last non-macro button wider 2px
   if ((!isMacro) && ((gridX == 0) || (gridX == 9)))
      width += 2;

   auto skinCtrl = currentSkin->controlForUIID("controls.modulation.panel");
   if( ! skinCtrl )
   {
      skinCtrl = currentSkin->getOrCreateControlForConnector(Surge::Skin::Connector::connectorByID("controls.modulation.panel"));
   }
   CRect r(CPoint(skinCtrl->x, skinCtrl->y), CPoint( width-1, 14) );

   if (isMacro)
      r.bottom += 8;

   int offsetX = 1;

   for (int i = 0; i < gridX; i++)
   {
      if ((!isMacro) && (i == 0))
         offsetX += 2;

      // gross hack for accumulated 2 px horizontal offsets from the previous if clause
      // needed to align the last column nicely
      if ((!isMacro) && (i == 8))
         offsetX -= 18;

      offsetX += width;
   }

   r.offset(offsetX,  (8 * gridY));

   return r;
}

void SurgeGUIEditor::setDisabledForParameter(Parameter* p, CSurgeSlider* s)
{
   if (p->id == synth->storage.getPatch().scene[current_scene].fm_depth.id)
   {
      s->disabled = !(synth->storage.getPatch().scene[current_scene].fm_switch.val.i);
   }
}

class LastChanceEventCapture : public CControl {
public:
   LastChanceEventCapture(const CPoint &sz, SurgeGUIEditor *ed ) : CControl( CRect( CPoint( 0, 0 ), sz )), editor(ed) {}
   void draw(CDrawContext* pContext) override
   {
      return;
   }

   CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons) override
   {
      if (buttons & (kMButton | kButton4 | kButton5))
      {
         if( editor) editor->toggle_mod_editing();
         return kMouseEventHandled;
      }
      return kMouseEventNotHandled;
   }

private:
   SurgeGUIEditor *editor = nullptr;
   CLASS_METHODS( LastChanceEventCapture, CControl );
};

void SurgeGUIEditor::openOrRecreateEditor()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeGUIEditor::openOrRecreateEditor" );
#endif
   if (!synth)
      return;
   assert(frame);

   removeFromFrame.clear();

   editorOverlayTagAtClose = "";
   if (editor_open)
   {
      if( editorOverlay != nullptr )
      {
         editorOverlay->remember();
         frame->removeView( editorOverlay );
         editorOverlayTagAtClose = editorOverlayTag;
      }
   
      close_editor();
   }
   CPoint nopoint(0, 0);
   CPoint sz( getWindowSizeX(), getWindowSizeY() );
   auto lcb = new LastChanceEventCapture(sz, this);
   frame->addView( lcb );

   clear_infoview_peridle = -1;

   /*
   ** There are a collection of member states we need to reset
   */
   polydisp = 0;
   lfodisplay = 0;
   fxmenu = 0;
   typeinDialog = nullptr;
   minieditOverlay = nullptr;
   msegEditSwitch = nullptr;
   
   for( int i=0; i<16; ++i ) vu[i] = 0;

   current_scene = synth->storage.getPatch().scene_active.val.i;



   /*
    * In Surge 1.8, the skin engine can change the position of this panel as a whole
    * but not anything else about it. The skin query happens inside positionForModulationGrid
    */
   for (int k = 1; k < n_modsources; k++)
   {
      if ((k != ms_random_unipolar) && (k != ms_alternate_unipolar))
      {
         modsources ms = (modsources)k;

         CRect r = positionForModulationGrid(ms);

         int state = 0;
         if (ms == modsource)
            state = mod_editor ? 2 : 1;
         if (ms == modsource_editor[current_scene])
            state |= 4;

         gui_modsrc[ms] = new CModulationSourceButton(r, this, tag_mod_source0 + ms, state, ms, bitmapStore, &(synth->storage));

         gui_modsrc[ms]->setSkin(currentSkin);
         gui_modsrc[ms]->update_rt_vals(false, 0, synth->isModsourceUsed(ms));
      
         if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
         {
            gui_modsrc[ms]->setlabel(synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
            gui_modsrc[ms]->set_ismeta(true);
            gui_modsrc[ms]->setBipolar(synth->storage.getPatch().scene[current_scene].modsources[ms]->is_bipolar());
            gui_modsrc[ms]->setValue(((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms])->get_target01());
         }
         else
         {
            gui_modsrc[ms]->setlabel(modulatorName(ms, true).c_str() );

            if (ms == ms_random_bipolar)
            {
               gui_modsrc[ms]->setAlternate(ms_random_unipolar, modsource_names_button[ms_random_unipolar]);
               gui_modsrc[ms]->setUseAlternate(modsource_is_alternate[ms]);
            }

            if (ms == ms_alternate_bipolar)
            {
               gui_modsrc[ms]->setAlternate(ms_alternate_unipolar, modsource_names_button[ms_alternate_unipolar]);
               gui_modsrc[ms]->setUseAlternate(modsource_is_alternate[ms]);
            }
         }

         frame->addView(gui_modsrc[ms]);
      }
   }

   /*// Comments
     {
     CRect CommentsRect(6 + 150*4,528, getWindowSizeX(), getWindowSizeY());
     CTextLabel *Comments = new
     CTextLabel(CommentsRect,synth->storage.getPatch().comment.c_str());
     Comments->setTransparency(true);
     Comments->setFont(displayFont);
     Comments->setHoriAlign(kMultiLineCenterText);
     frame->addView(Comments);
     }*/


   // fx vu-meters & labels. This is all a bit hacky still
   if (synth->fx[current_fx])
   {
      auto fxpp = currentSkin->getOrCreateControlForConnector("fx.param.panel");
      CRect fxRect = CRect( CPoint( fxpp->x, fxpp->y ), CPoint( 123, 13 ) );
      for (int i = 0; i < 8; i++)
      {
         int t = synth->fx[current_fx]->vu_type(i);
         if (t)
         {
            CRect vr(fxRect); // FIXME (vurect);
            vr.offset(6, yofs * synth->fx[current_fx]->vu_ypos(i));
            vr.offset(0, -14);
            vu[i + 1] = new CSurgeVuMeter(vr, this);
            ((CSurgeVuMeter*)vu[i + 1])->setSkin(currentSkin,bitmapStore);
            ((CSurgeVuMeter*)vu[i + 1])->setType(t);
            frame->addView(vu[i + 1]);
         }
         else
            vu[i + 1] = 0;

         const char* label = synth->fx[current_fx]->group_label(i);

         if (label)
         {
            CRect vr(fxRect); // (vurect);
            vr.top += 1;
            vr.right += 5;
            vr.offset(5, -12);
            vr.offset(0, yofs * synth->fx[current_fx]->group_label_ypos(i));
            CEffectLabel* lb = new CEffectLabel(vr);
            lb->setLabel(label);
            lb->setSkin(currentSkin,bitmapStore);
            frame->addView(lb);
         }
      }
   }

   /*
    * Loop over the non-associated controls
    */
   for( auto i=Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED + 1;
        i < Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED;
        ++i )
   {
      Surge::Skin::Connector::NonParameterConnection npc = (Surge::Skin::Connector::NonParameterConnection)i;
      auto conn = Surge::Skin::Connector::connectorByNonParameterConnection(npc);
      auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
      currentSkin->resolveBaseParentOffsets(skinCtrl);


      if( ! skinCtrl )
      {
         std::cout << "Unable to find SkinCtrl" << std::endl;
         continue;
      }
      if( skinCtrl->classname == Surge::UI::NoneClassName )
         continue;

      /*
       * Many of the controls are special and so require non-generalizable constructors
       * handled here. Some are standard and so once we know the tag we can use layoutComponentForSkin
       * but it's not worth generalizing the OSCILLATOR_DISPLAY beyond this, say.
       */
      switch( npc )
      {
      case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_DISPLAY: {
         auto od = new COscillatorDisplay(
             CRect(CPoint(skinCtrl->x, skinCtrl->y), CPoint(skinCtrl->w, skinCtrl->h)), this,
             &synth->storage.getPatch()
                  .scene[synth->storage.getPatch().scene_active.val.i]
                  .osc[current_osc[current_scene]],
             &synth->storage);
         od->setSkin(currentSkin, bitmapStore);
         frame->addView(od);
         oscdisplay = od;
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::SURGE_MENU: {
         auto q = layoutComponentForSkin(skinCtrl, tag_settingsmenu);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::OSCILLATOR_SELECT: {
         auto oscswitch = layoutComponentForSkin(skinCtrl, tag_osc_select );
         oscswitch->setValue((float)current_osc[current_scene] / 2.0f);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::JOG_PATCHCATEGORY: {
         layoutComponentForSkin(skinCtrl, tag_mp_category);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::JOG_PATCH: {
         layoutComponentForSkin(skinCtrl, tag_mp_patch);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::JOG_FX: {
         layoutComponentForSkin(skinCtrl, tag_mp_jogfx);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::STATUS_MPE: {
         statusMPE  = layoutComponentForSkin(skinCtrl, tag_status_mpe);
         statusMPE->setValue( synth->mpeEnabled ? 1 : 0 );
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::STATUS_TUNE: {
         // FIXME - drag and drop onto this?
         statusTune  = layoutComponentForSkin(skinCtrl, tag_status_tune);
         statusTune->setValue( synth->storage.isStandardTuning ? 0 : 1 );

         auto csc = dynamic_cast<CSwitchControl *>(statusTune);
         if( csc && synth->storage.isStandardTuning )
         {
            csc->unValueClickable = ( tuningCacheForToggle.size() == 0 && mappingCacheForToggle.size() == 0 );
         }
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::STATUS_ZOOM: {
         statusZoom  = layoutComponentForSkin(skinCtrl, tag_status_zoom);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::STORE_PATCH: {
         layoutComponentForSkin( skinCtrl, tag_store );
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::MSEG_EDIT: {
         msegEditSwitch = layoutComponentForSkin( skinCtrl, tag_mseg_edit );
         msegEditSwitch->setVisible( false );
         msegEditSwitch->setValue( editorOverlay != nullptr && editorOverlayTag == "msegEditor" );
         auto q = modsource_editor[current_scene];
         if( ( q >= ms_lfo1 && q <= ms_lfo6  ) || ( q >= ms_slfo1 && q <= ms_slfo6 ) )
         {
            auto *lfodata = &( synth->storage.getPatch().scene[current_scene].lfo[ q - ms_lfo1 ] );
            if( lfodata->shape.val.i == lt_mseg )
               msegEditSwitch->setVisible( true );
         }
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::LFO_MENU: {
         layoutComponentForSkin( skinCtrl, tag_lfo_menu );

         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::LFO_LABEL: {
         // Room for improvement, obviously
         lfoNameLabel = new CVerticalLabel(skinCtrl->getRect(), "" );
         lfoNameLabel->setTransparency(true);
         VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnt = new VSTGUI::CFontDesc("Lato", 10, kBoldFace);
         lfoNameLabel->setFont(fnt);
         lfoNameLabel->setFontColor(currentSkin->getColor(Colors::LFO::Title::Text));
         lfoNameLabel->setHoriAlign(kCenterText);
         frame->addView(lfoNameLabel);

         break;
      }

      case Surge::Skin::Connector::NonParameterConnection::FXPRESET_LABEL: {
         // Room for improvement, obviously
         fxPresetLabel = new CTextLabel(skinCtrl->getRect(), "Preset");
         fxPresetLabel->setFontColor(currentSkin->getColor(Colors::Effect::Preset::Name));
         fxPresetLabel->setTransparency(true);
         fxPresetLabel->setFont(displayFont);
         fxPresetLabel->setHoriAlign(kRightText);
         fxPresetLabel->setTextTruncateMode(CTextLabel::TextTruncateMode::kTruncateTail);
         frame->addView(fxPresetLabel);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::PATCH_BROWSER: {
         patchname =
             new CPatchBrowser(skinCtrl->getRect(), this, tag_patchname, &synth->storage);
         ((CPatchBrowser*)patchname)->setSkin(currentSkin, bitmapStore);
         ((CPatchBrowser*)patchname)->setLabel(synth->storage.getPatch().name);
         ((CPatchBrowser*)patchname)->setCategory(synth->storage.getPatch().category);
         ((CPatchBrowser*)patchname)->setIDs(synth->current_category_id, synth->patchid);
         ((CPatchBrowser*)patchname)->setAuthor(synth->storage.getPatch().author);
         frame->addView(patchname);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::FX_SELECTOR: {
         CEffectSettings* fc = new CEffectSettings(skinCtrl->getRect(), this, tag_fx_select, current_fx, bitmapStore);
         fc->setSkin(currentSkin, bitmapStore);
         ccfxconf = fc;
         for (int i = 0; i < 8; i++)
         {
            fc->set_type(i, synth->storage.getPatch().fx[i].type.val.i);
         }
         fc->set_bypass(synth->storage.getPatch().fx_bypass.val.i);
         fc->set_disable(synth->storage.getPatch().fx_disable.val.i);
         frame->addView(fc);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::MAIN_VU_METER: { // main vu-meter
         vu[0] = new CSurgeVuMeter(skinCtrl->getRect(), this);
         ((CSurgeVuMeter*)vu[0])->setSkin(currentSkin,bitmapStore);
         ((CSurgeVuMeter*)vu[0])->setType(vut_vu_stereo);
         frame->addView(vu[0]);
         break;
      }
      case Surge::Skin::Connector::NonParameterConnection::N_NONCONNECTED:
      case Surge::Skin::Connector::NonParameterConnection::PARAMETER_CONNECTED:
         break;
      }

   }

   memset(param, 0, n_paramslots * sizeof(void*));
   memset(nonmod_param, 0, n_paramslots * sizeof(void*));
   int i = 0;
   vector<Parameter*>::iterator iter;
   for (iter = synth->storage.getPatch().param_ptr.begin();
        iter != synth->storage.getPatch().param_ptr.end(); iter++)
   {
      if (i == n_paramslots)
      {
         // This would only happen if a dev added params.
         Surge::UserInteractions::promptError(
             "INTERNAL ERROR: List of parameters is larger than maximum number of parameter slots. Increase n_paramslots in SurgeGUIEditor.h!",
             "Error");
      }
      Parameter* p = *iter;
      bool paramIsVisible = ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
                            isControlVisible(p->ctrlgroup, p->ctrlgroup_entry) && (p->ctrltype != ct_none);

      auto conn = Surge::Skin::Connector::connectorByID(p->ui_identifier);
      std::string uiid = p->ui_identifier;

      long style = p->ctrlstyle;
      switch (p->ctrltype)
      {
      case ct_decibel:
      case ct_decibel_narrow:
      case ct_decibel_narrow_extendable:
      case ct_decibel_narrow_short_extendable:
      case ct_decibel_extra_narrow:
      case ct_decibel_extendable:
      case ct_freq_mod:
      case ct_percent_bidirectional:
      case ct_percent_bidirectional_stereo:
      case ct_freq_shift:
      case ct_osc_feedback_negative:
      case ct_lfodeform:
      case ct_airwindow_param_bipolar:
         style |= kBipolar;
         break;
      case ct_lfoamplitude:
         if (p->extend_range)
            style |= kBipolar;
         break;
      case ct_fmratio:
         if (p->extend_range)
            style |= kBipolar;
         break;
      };

      if (p->hasSkinConnector && conn.payload->defaultComponent != Surge::Skin::Connector::NONE && paramIsVisible )
      {
         /*
          * Some special cases where we don't add a control
          */
         bool addControl = true;

         // Case: Analog envelopes have no shapers
         if( p->ctrltype == ct_envshape || p->ctrltype == ct_envshape_attack )
         {
            addControl = (synth->storage.getPatch().scene[current_scene].adsr[p->ctrlgroup_entry].mode.val.i == emt_digital);
         }

         if( addControl )
         {
            auto skinCtrl = currentSkin->getOrCreateControlForConnector(conn);
            currentSkin->resolveBaseParentOffsets(skinCtrl);
            layoutComponentForSkin(skinCtrl, p->id + start_paramtags, i, p, style | conn.payload->controlStyleFlags);
         }
      }
      i++;
   }

   // resonance link mode
   if (synth->storage.getPatch().scene[current_scene].f2_link_resonance.val.b)
   {
      i = synth->storage.getPatch().scene[current_scene].filterunit[1].resonance.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
         ((CSurgeSlider*)param[i])->disabled = true;
   }

   // feedback control
   if (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i == fb_serial)
   {
      i = synth->storage.getPatch().scene[current_scene].feedback.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
      {
         bool curr = ((CSurgeSlider*)param[i])->disabled;
         ((CSurgeSlider*)param[i])->disabled = true;
         if (!curr)
         {
            param[i]->setDirty();
            param[i]->invalid();
         }
      }
   }

   // pan2 control
   if ((synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
        fb_stereo) &&
       (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i != fb_wide))
   {
      i = synth->storage.getPatch().scene[current_scene].width.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
      {
         bool curr = ((CSurgeSlider*)param[i])->disabled;
         ((CSurgeSlider*)param[i])->disabled = true;
         if( ! curr )
         {
            param[i]->setDirty();
            param[i]->invalid();
         }
      }
   }

   int menuX = 844, menuY = 550, menuW = 50, menuH = 15;
   CRect menurect(menuX, menuY, menuX + menuW, menuY + menuH);

   infowindow = new CParameterTooltip(CRect(0, 0, 0, 0));
   ((CParameterTooltip *)infowindow)->setSkin( currentSkin );
   frame->addView(infowindow);

   // Mouse behavior
   if (CSurgeSlider::sliderMoveRateState == CSurgeSlider::kUnInitialized)
      CSurgeSlider::sliderMoveRateState = (CSurgeSlider::MoveRateState)Surge::Storage::getUserDefaultValue(&(synth->storage), "sliderMoveRateState", (int)CSurgeSlider::kClassic);

   /*
   ** Skin Labels
   */
   auto labels = currentSkin->getLabels();

   for (auto &l : labels)
   {
      auto mtext = currentSkin->propertyValue(l, "text");

      if (mtext.isJust())
      {
         auto ta = currentSkin->propertyValue(l, "text_align", "left");
         // make text align value not case sensitive
         std::transform(ta.begin(), ta.end(), ta.begin(),[](unsigned char c) { return std::tolower(c); });

         VSTGUI::CHoriTxtAlign txtalign;

         if (ta == "left")
            txtalign = kLeftText;
         else if (ta == "center")
            txtalign = kCenterText;
         else if (ta == "right")
            txtalign = kRightText;

         auto fs = currentSkin->propertyValue(l, "font_size", "12");
         auto fsize = std::atof(fs.c_str());

         auto fst = currentSkin->propertyValue(l, "font_style", "normal");
         // make font style value not case sensitive
         std::transform(fst.begin(), fst.end(), fst.begin(),[](unsigned char c) { return std::tolower(c); });
         
         VSTGUI::CTxtFace fstyle;

         if (fst == "normal")
            fstyle = kNormalFace;
         else if (fst == "bold")
            fstyle = kBoldFace;
         else if (fst == "italic")
            fstyle = kItalicFace;
         else if (fst == "underline")
            fstyle = kUnderlineFace;
         else if (fst == "strikethrough")
            fstyle = kStrikethroughFace;

         auto coln = currentSkin->propertyValue(l, "color", "#FF0000");
         auto col = currentSkin->getColor(coln, kRedCColor);

         auto bgcoln = currentSkin->propertyValue(l, "bg_color", "#FFFFFF00");
         auto bgcol = currentSkin->getColor(bgcoln, kTransparentCColor);

         auto frcoln = currentSkin->propertyValue(l, "frame_color", "#FFFFFF00");
         auto frcol = currentSkin->getColor(frcoln, kTransparentCColor);

         VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnt = new VSTGUI::CFontDesc("Lato", fsize, fstyle);

         auto lb = new CTextLabel(CRect(CPoint(l->x, l->y), CPoint(l->w, l->h)), mtext.fromJust().c_str());
         lb->setTransparency((bgcol == kTransparentCColor && frcol == kTransparentCColor));
         lb->setAntialias(true);
         lb->setHoriAlign(txtalign);
         lb->setFont(fnt);
         lb->setFontColor(col);
         lb->setBackColor(bgcol);
         lb->setFrameColor(frcol);

         frame->addView(lb);
      }
   }
#if BUILD_IS_DEBUG
   /*
    * This code is here JUST because baconpaul keeps developing surge and then swapping
    * to make music and wondering why LPX is stuttering. Please don't remove it!
    *
    * UPDATE: Might as well keep a reference to the object though so we can touch it in idle
    */
   auto lb = new CTextLabel( CRect( CPoint( 310, 39 ), CPoint( 195, 15 ) ), "DEBUG BUILD" );
   lb->setTransparency( false );
   lb->setBackColor( kRedCColor );
   lb->setFontColor( kWhiteCColor );
   lb->setFont( patchNameFont );
   lb->setHoriAlign(VSTGUI::kCenterText);
   lb->setAntialias(true);
   frame->addView(lb);
   debugLabel = lb;
#endif
   if( editorOverlay )
   {
      frame->addView( editorOverlay );
   }
   
   refresh_mod();

   editor_open = true;
   queue_refresh = false;

   /* When we rebuild, the onMouseEntered event is not re-sent to the new component by VSTGui;
    * Maybe this is a bug in VSTG? But it screws up our hover states so force an onMouseEntered
    * if we are over a coponent
    */
   CPoint tr;
   frame->getCurrentMouseLocation(tr);

   // OK to *find* the component we need to look in transformed coordinates
   CPoint ttr = tr;
   frame->getTransform().transform( ttr );
   auto v = frame->getViewAt(ttr);

   if( v )
   {
      // but to *hover* the component we need to do it in 100-scale coordinates
      v->onMouseEntered(tr, 0);
   }

   frame->invalid();

}

void SurgeGUIEditor::close_editor()
{
   editor_open = false;
   lfodisplay = 0;
   frame->removeAll(true);
   setzero(param);
}


#if !TARGET_VST3
bool SurgeGUIEditor::open(void* parent)
#else
bool PLUGIN_API SurgeGUIEditor::open(void* parent, const PlatformType& platformType)
#endif
{
   if (samplerate == 0)
   {
      std::cout << "Sample rate was not set when editor opened. Defaulting to 44.1k" << std::endl;

      /*
      ** The oscillator displays need a sample rate; some test hosts don't call
      ** setSampleRate so if we are in this state make the bad but reasonable
      ** default choice
      */
      synth->setSamplerate(44100);
   }
#if !TARGET_VST3
   // !!! always call this !!!
   super::open(parent);

   PlatformType platformType = kDefaultNative;
#endif

#if TARGET_VST3
#if LINUX
   Steinberg::Linux::IRunLoop* l = nullptr;
   if (getIPlugFrame()->queryInterface(Steinberg::Linux::IRunLoop::iid, (void**)&l) ==
       Steinberg::kResultOk)
   {
      std::cout << "IPlugFrame is a runloop " << l << std::endl;
   }
   else
   {
      std::cout << "IPlogFrame is not a runloop " << l << std::endl;
   }
   if (l == nullptr)
   {
      Surge::UserInteractions::promptError("Starting VST3 with null runloop", "Software Error");
   }
   LinuxVST3Init(l);
#endif
#endif

   float fzf = getZoomFactor() / 100.0;
#if TARGET_VST2
   CRect wsize(0, 0, currentSkin->getWindowSizeX() * fzf, currentSkin->getWindowSizeY() * fzf);
#else
   CRect wsize( 0, 0, currentSkin->getWindowSizeX(), currentSkin->getWindowSizeY() );
#endif

   CFrame *nframe = new CFrame(wsize, this);

   bitmapStore.reset(new SurgeBitmaps());
   bitmapStore->setupBitmapsForFrame(nframe);
   currentSkin->reloadSkin(bitmapStore);
   nframe->setZoom(fzf);
   /*
    * OK so WTF is this? Well:
    * CFrame is a CView so it can be a drop thing
    * but CFrame is final so you can't subclass it
    * You could call setDropTarget on it but that's not what CViewContainers use
    * They instead use this attribute, kCViewContainerDropTargetAttribute
    * but that's not public so copy its value here
    * and add a comment
    * and think - sigh. VSTGUI.
    */

   dropAdapter = new SGEDropAdapter(this);
   dropAdapter->remember();

   VSTGUI::IDropTarget *dt = nullptr;
   nframe->getAttribute( 'vcdt', dt );
   if( dt ) dt->forget();
   nframe->setAttribute('vcdt', dropAdapter );

   frame = nframe;

#if LINUX && TARGET_VST3
   LinuxVST3FrameOpen(frame, parent, platformType);
#elif LINUX && TARGET_LV2
   VSTGUI::X11::FrameConfig frameConfig;
   frameConfig.runLoop = SurgeLv2::createRunLoop(_userdata);
   frame->open(parent, platformType, &frameConfig);
#else
   frame->open(parent, platformType);
#endif

#if TARGET_VST3 || TARGET_LV2
   _idleTimer = VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer>( new CVSTGUITimer([this](CVSTGUITimer* timer) { idle(); }, 50, false), false );
   _idleTimer->start();
#endif

#if TARGET_VST3 && LINUX
   firstIdleCountdown = 2;
#endif

   /*#if TARGET_AUDIOUNIT
     synth = (sub3_synth*)_effect;
     #elif TARGET_VST3
     //synth = (sub3_synth*)_effect; ??
     #else
     vstlayer *plug = (vstlayer*)_effect;
     if(!plug->initialized) plug->init();
     synth = (sub3_synth*)plug->plugin_instance;
     #endif*/

   /*
   ** Register only once (when we open)
   */
   frame->registerKeyboardHook(this);
   reloadFromSkin();
   openOrRecreateEditor();

   if(getZoomFactor() != 100)
   {
      zoom_callback(this);
      zoomInvalid = true;
   }

   //HEREHERE
   auto *des = &(synth->storage.getPatch().dawExtraState);

   if( des->isPopulated && des->editor.isMSEGOpen )
      showMSEGEditor();

   return true;
}

void SurgeGUIEditor::close()
{
   if( editorOverlay )
   {
      frame->removeView( editorOverlay );
      editorOverlayOnClose();
      editorOverlayTagAtClose = editorOverlayTag;
      editorOverlayTag = "";
      editorOverlay = nullptr;
   }
#if TARGET_VST2 // && WINDOWS
   // We may need this in other hosts also; but for now
   if (frame)
   {
      getFrame()->unregisterKeyboardHook(this);
      frame->close();
      frame = nullptr;
   }

   if( dropAdapter )
   {
      dropAdapter->buddy = nullptr;
      dropAdapter->forget();
      dropAdapter = nullptr;
   }

#endif

#if !TARGET_VST3
   super::close();
#endif

#if TARGET_VST3 || TARGET_LV2
   _idleTimer->stop();
   _idleTimer = nullptr;
#endif
   firstIdleCountdown = 0;

#if TARGET_VST3
#if LINUX
   LinuxVST3Detatch();
#endif
#endif
}

void SurgeGUIEditor::setParameter(long index, float value)
{
   if (!frame)
      return;
   if (!editor_open)
      return;
   if (index > synth->storage.getPatch().param_ptr.size())
      return;

   // if(param[index])
   {
      // param[index]->setValue(value);
      int j = 0;
      while (j < 7)
      {
         if ((synth->refresh_ctrl_queue[j] > -1) && (synth->refresh_ctrl_queue[j] != index))
            j++;
         else
            break;
      }
      synth->refresh_ctrl_queue[j] = index;
      synth->refresh_ctrl_queue_value[j] = value;
   }
}

void decode_controllerid(char* txt, int id)
{
   int type = (id & 0xffff0000) >> 16;
   int num = (id & 0xffff);
   switch (type)
   {
   case 1:
      sprintf(txt, "NRPN %i", num);
      break;
   case 2:
      sprintf(txt, "RPN %i", num);
      break;
   default:
      sprintf(txt, "CC %i", num);
      break;
   };
}

int32_t SurgeGUIEditor::controlModifierClicked(CControl* control, CButtonState button)
{
   if (!synth)
      return 0;
   if (!frame)
      return 0;
   if (!editor_open)
      return 0;

   if (button & (kMButton | kButton4 | kButton5))
   {
      toggle_mod_editing();
      return 1;
   }

   long tag = control->getTag();

   // In these cases just move along with success. RMB does nothing on these switches
   if( tag == tag_mp_jogfx || tag == tag_mp_category || tag == tag_mp_patch || tag == tag_store || tag == tag_store_cancel || tag == tag_store_ok )
      return 1;

   if( tag == tag_lfo_menu )
   {
      control->setValue(0);
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showLfoMenu(where);
      return 1;
   }

   if( tag == tag_status_zoom )
   {
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showZoomMenu(where);
      return 1;
   }
   if( tag == tag_status_mpe )
   {
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showMPEMenu(where);
      return 1;
   }
   if( tag == tag_status_tune )
   {
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showTuningMenu(where);
      return 1;
   }

   std::vector< std::string > clearControlTargetNames;

   auto cmensl = dynamic_cast<CMenuAsSlider*>(control);

   if( cmensl && cmensl->deactivated )
      return 0;
   
   if (button & kRButton)
   {
      if (tag == tag_settingsmenu)
      {
         CRect r = control->getViewSize();
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);

         menuRect.offset(where.x, where.y);

         useDevMenu = true;
         showSettingsMenu(menuRect);
         return 1;
      }

      if (tag == tag_osc_select)
      {
         CRect r = control->getViewSize();
         CRect menuRect;

         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);

         int a = limit_range((int)((3 * (where.x - r.left)) / r.getWidth()), 0, 2);
         menuRect.offset(where.x, where.y);

         COptionMenu* contextMenu = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
         int eid = 0;

         char txt[256];
         auto hu = helpURLForSpecial( "osc-select" );
         if( hu != "" )
         {
            sprintf(txt, "[?] Osc %i", a + 1);
            auto lurl = fullyResolvedHelpURL(hu);
            addCallbackMenu(contextMenu, txt, [lurl]() {
                                                 Surge::UserInteractions::openURL( lurl );
                                              } );
            eid++;
         }
         else
         {
            sprintf(txt, "Osc %i", a + 1);
            contextMenu->addEntry(txt, eid++);
         }

         contextMenu->addSeparator(eid++);
         addCallbackMenu(contextMenu, "Copy",
                         [this, a]() { synth->storage.clipboard_copy(cp_osc, current_scene, a); });
         eid++;

         addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Copy With Modulation"), [this, a]() {
                                                                   synth->storage.clipboard_copy(cp_oscmod, current_scene, a);
                                                                });
         eid++;

         if (synth->storage.get_clipboard_type() == cp_osc)
         {
            addCallbackMenu(contextMenu, "Paste", [this, a]() {
                                                     synth->clear_osc_modulation(current_scene, a);
                                                     synth->storage.clipboard_paste(cp_osc, current_scene, a);
                                                     queue_refresh = true;
                                                  });
            eid++;
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget

         return 1;
      }

      if (tag == tag_scene_select)
      {
         CRect r = control->getViewSize();
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);


         int a = limit_range((int)((2 * (where.x - r.left)) / r.getWidth()), 0, 2);
         menuRect.offset(where.x, where.y);

         COptionMenu* contextMenu = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
         int eid = 0;

         char txt[256];
         auto hu = helpURLForSpecial( "scene-select" );
         if( hu != "" )
         {
            sprintf(txt, "[?] Scene %c", 'A' + a);
            auto lurl = fullyResolvedHelpURL(hu);
            addCallbackMenu(contextMenu, txt, [lurl]() {
                                                 Surge::UserInteractions::openURL( lurl );
                                              } );
            eid++;
         }
         else
         {
            sprintf(txt, "Scene %c", 'A' + a);
            contextMenu->addEntry(txt, eid++);
         }

         contextMenu->addSeparator(eid++);

         addCallbackMenu(contextMenu, "Copy",
                         [this, a]() { synth->storage.clipboard_copy(cp_scene, a, -1); });
         eid++;
         if (synth->storage.get_clipboard_type() == cp_scene)
         {
            addCallbackMenu(contextMenu, "Paste", [this, a]() {
                                                     synth->storage.clipboard_paste(cp_scene, a, -1);
                                                     queue_refresh = true;
                                                  });
            eid++;
         }

         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget

         return 1;
      }
   }

   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      modsources modsource = (modsources)(tag - tag_mod_source0);

      if (button & kRButton)
      {
         CModulationSourceButton *cms = (CModulationSourceButton *)control;
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);


         menuRect.offset(where.x, where.y);
         COptionMenu* contextMenu =
            new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
         int eid = 0;

         std::string hu;
         if( modsource >= ms_ctrl1 && modsource <= ms_ctrl8 )
         {
            hu = helpURLForSpecial( "macro-modbutton" );
         }
         else if( modsource >= ms_lfo1 && modsource <= ms_slfo6 )
         {
            hu = helpURLForSpecial( "lfo-modbutton" );
         }
         else
         {
            hu = helpURLForSpecial( "other-modbutton" );
         }

         if( cms->hasAlternate )
         {
            int idOn = modsource;
            int idOff = cms->alternateId;
            if( cms->useAlternate )
            {
               auto t = idOn;
               idOn = idOff;
               idOff = t;
            }

            if( hu != "" )
            {
               auto lurl = fullyResolvedHelpURL(hu);
               std::string hs = std::string( "[?] " ) + (char*)modsource_names[idOn];
               addCallbackMenu( contextMenu, hs, [lurl]()
                                                    {
                                                       Surge::UserInteractions::openURL(lurl);
                                                    }
                  );
               eid++;
            }
            else
            {
               contextMenu->addEntry((char*)modsource_names[idOn], eid++);
            }
            std::string offLab = "Switch to ";
            offLab += modsource_names[idOff];
            bool activeMod = (cms->state & 3) == 2;

            auto *mi = addCallbackMenu(
               contextMenu, offLab, [this, modsource, cms]() {
                                       cms->setUseAlternate( ! cms->useAlternate );
                                       modsource_is_alternate[modsource] = cms->useAlternate;
                                    }
               );
            if( activeMod )
               mi->setEnabled(false);
            eid++;
         }
         else
         {
            if( hu != "" )
            {
               auto lurl = fullyResolvedHelpURL(hu);
               std::string hs = std::string( "[?] " ) + modulatorName(modsource, false);
               addCallbackMenu( contextMenu, hs, [lurl]()
                                                    {
                                                       Surge::UserInteractions::openURL(lurl);
                                                    }
                  );
               eid++;
            }
            else
            {
               contextMenu->addEntry((char*)modulatorName( modsource, false ).c_str(), eid++);
            }
         }

         int n_total_md = synth->storage.getPatch().param_ptr.size();

         const int max_md = 4096;
         assert(max_md >= n_total_md);

         bool cancellearn = false;
         int ccid = 0;

         int detailedMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);

         // should start at 0, but started at 1 before.. might be a reason but don't remember why...
         std::vector<modsources> possibleSources;
         possibleSources.push_back(modsource);
         if( cms->hasAlternate )
         {
            possibleSources.push_back((modsources)(cms->alternateId));
         }

         for( auto thisms : possibleSources )
         {
            bool first_destination = true;
            int n_md = 0;

            for (int md = 0; md < n_total_md; md++)
            {
               auto activeScene = synth->storage.getPatch().scene_active.val.i;
               Parameter* parameter = synth->storage.getPatch().param_ptr[md];

               if (((md < n_global_params) || ((parameter->scene - 1) == activeScene)) &&
                   synth->isActiveModulation(md, thisms))
               {
                  char modtxt[256];
                  auto pmd = synth->storage.getPatch().param_ptr[md];
                  pmd->get_display_of_modulation_depth( modtxt, synth->getModDepth(md, thisms), synth->isBipolarModulation(thisms), Parameter::Menu);
                  char tmptxt[1024]; // leave room for that ubuntu 20.0 error
                  if( pmd->ctrlgroup == cg_LFO )
                  {
                     char pname[256];
                     pmd->create_fullname( pmd->get_name(), pname, pmd->ctrlgroup, pmd->ctrlgroup_entry, modulatorName(pmd->ctrlgroup_entry, true ).c_str() );
                     sprintf(tmptxt, "Edit %s -> %s: %s", (char*)modulatorName(thisms, true).c_str(),
                             pname,
                             modtxt);
                  }
                  else
                  {
                     sprintf(tmptxt, "Edit %s -> %s: %s", (char*)modulatorName(thisms, true).c_str(),
                             pmd->get_full_name(),
                             modtxt);
                  }

                  auto clearOp = [this, parameter, control, thisms]() {
                                    this->promptForUserValueEntry( parameter, control, thisms );
                                 };

                  if (first_destination)
                  {
                     contextMenu->addSeparator(eid++);
                     first_destination = false;
                  }

                  addCallbackMenu(contextMenu, tmptxt, clearOp);
                  eid++;
               }
            }

            first_destination = true;
            for (int md = 0; md < n_total_md; md++)
            {
               auto activeScene = synth->storage.getPatch().scene_active.val.i;
               Parameter* parameter = synth->storage.getPatch().param_ptr[md];

               if (((md < n_global_params) || ((parameter->scene - 1) == activeScene)) &&
                   synth->isActiveModulation(md, thisms))
               {
                  char tmptxt[1024];
                  if( parameter->ctrlgroup == cg_LFO )
                  {
                     char pname[512];
                     parameter->create_fullname( parameter->get_name(), pname, parameter->ctrlgroup,
                                                 parameter->ctrlgroup_entry, modulatorName(parameter->ctrlgroup_entry, true ).c_str() );
                     sprintf(tmptxt, "Clear %s -> %s", (char*)modulatorName(thisms, true).c_str(),
                             pname);
                  }
                  else
                  {
                     sprintf(tmptxt, "Clear %s -> %s", (char*)modulatorName(thisms, true).c_str(),
                             parameter->get_full_name() );
                  }

                  auto clearOp = [this, first_destination, md, n_total_md, thisms, control]() {
                                    bool resetName = false;   // Should I reset the name?
                                    std::string newName = ""; // And to what?
                                    int ccid = thisms - ms_ctrl1;

                                    if (first_destination)
                                    {
                                       if (strncmp(synth->storage.getPatch().CustomControllerLabel[ccid],
                                                   synth->storage.getPatch().param_ptr[md]->get_name(), 15) == 0)
                                       {
                                          // So my modulator is named after my short name. I haven't been renamed. So
                                          // I want to reset at least to "-" unless someone is after me
                                          resetName = true;
                                          newName = "-";

                                          // Now we have to find if there's another modulation below me
                                          int nextmd = md + 1;
                                          while (nextmd < n_total_md && !synth->isActiveModulation(nextmd, thisms))
                                             nextmd++;
                                          if (nextmd < n_total_md &&
                                              strlen(synth->storage.getPatch().param_ptr[nextmd]->get_name()) > 1)
                                             newName = synth->storage.getPatch().param_ptr[nextmd]->get_name();
                                       }
                                    }

                                    synth->clearModulation(md, thisms);
                                    refresh_mod();
                                    control->setDirty();
                                    control->invalid();

                                    if (resetName)
                                    {
                                       // And this is where we apply the name refresh, of course.
                                       strncpy(synth->storage.getPatch().CustomControllerLabel[ccid], newName.c_str(),
                                               15);
                                       synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                                       ((CModulationSourceButton*)control)
                                          ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
                                       control->setDirty();
                                       control->invalid();
                                       synth->updateDisplay();
                                    }
                                 };

                  if (first_destination)
                  {
                     contextMenu->addSeparator(eid++);
                     first_destination = false;
                  }

                  addCallbackMenu(contextMenu, tmptxt, clearOp);
                  eid++;

                  n_md++;
               }
            }
            if (n_md)
            {
               std::string clearLab;
               std::string modName;

               // hacky, but works - we want to retain the capitalization for modulator names regardless of OS!
               modName = modulatorName( thisms, false );
               clearLab = Surge::UI::toOSCaseForMenu("Clear All ") + modName +
                       Surge::UI::toOSCaseForMenu(" Routings");

               addCallbackMenu(
                  contextMenu, clearLab, [this, n_total_md, thisms, control]() {
                                            for (int md = 1; md < n_total_md; md++)
                                               synth->clearModulation(md, thisms);
                                            refresh_mod();

                                            // Also blank out the name and rebuild the UI
                                            if (within_range(ms_ctrl1, thisms, ms_ctrl1 + n_customcontrollers - 1))
                                            {
                                               int ccid = thisms - ms_ctrl1;

                                               synth->storage.getPatch().CustomControllerLabel[ccid][0] = '-';
                                               synth->storage.getPatch().CustomControllerLabel[ccid][1] = 0;
                                               ((CModulationSourceButton*)control)
                                                  ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
                                            }
                                            control->setDirty(true);
                                            control->invalid();
                                            synth->updateDisplay();

                                         });
               eid++;
            }
         }
         int sc = limit_range(synth->storage.getPatch().scene_active.val.i, 0, n_scenes - 1);
#if TARGET_VST3
         Steinberg::Vst::IContextMenu *hostMenu = nullptr;
#endif

         if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
         {
            /*
            ** This is the menu for the controls
            */

            ccid = modsource - ms_ctrl1;

            auto cms = ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[modsource]);

            contextMenu->addSeparator(eid++);
            char vtxt[1024];
            sprintf( vtxt, "Edit Value: %.*f %%", (detailedMode ? 6 : 2 ), 100 * cms->get_output() );
            addCallbackMenu( contextMenu, vtxt, [this, control, modsource]()
                                                   {
                                                      promptForUserValueEntry( nullptr, control, modsource );
                                                   }
               );
            eid++;

            contextMenu->addSeparator(eid++);

            char txt[256];

            // Construct submenus for explicit controller mapping
            COptionMenu* midiSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                   VSTGUI::COptionMenu::kNoDrawStyle |
                                                       VSTGUI::COptionMenu::kMultipleCheckStyle);
            COptionMenu* currentSub = nullptr;

            for (int subs = 0; subs < 7; ++subs)
            {
               if (currentSub)
               {
                  currentSub->forget();
                  currentSub = nullptr;
               }
               currentSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                            VSTGUI::COptionMenu::kNoDrawStyle |
                                                VSTGUI::COptionMenu::kMultipleCheckStyle);

               char name[16];

               sprintf(name, "CC %d ... %d", subs * 20, min((subs * 20) + 20, 128) - 1);

               auto added_to_menu = midiSub->addEntry(currentSub, name);

               for (int item = 0; item < 20; ++item)
               {
                  int mc = (subs * 20) + item;
                  int disabled = 0;

                  // these CCs cannot be used for MIDI learn (see
                  // SurgeSynthesizer::channelController)
                  if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 ||
                      (mc == 74 && synth->mpeEnabled) || (mc >= 98 && mc <= 101) || mc == 120 ||
                      mc == 123)
                     disabled = 1;

                  // we don't have any more CCs to cover, so break the loop
                  if (mc > 127)
                     break;

                  char name[128];

                  sprintf(name, "CC %d (%s) %s", mc, midicc_names[mc],
                          (disabled == 1 ? "- RESERVED" : ""));

                  CCommandMenuItem* cmd = new CCommandMenuItem(CCommandMenuItem::Desc(name));

                  cmd->setActions([this, ccid, mc](CCommandMenuItem* men) {
                     synth->storage.controllers[ccid] = mc;
                     synth->storage.save_midi_controllers();
                  });
                  cmd->setEnabled(!disabled);

                  auto added = currentSub->addEntry(cmd);

                  if (synth->storage.controllers[ccid] == mc)
                  {
                     added->setChecked();
                     added_to_menu->setChecked();
                  }
               }
            }

            if (currentSub)
            {
               currentSub->forget();
               currentSub = nullptr;
            }

            contextMenu->addEntry(midiSub, Surge::UI::toOSCaseForMenu("Assign Macro To..."));

            eid++;

            if (synth->learn_custom > -1)
               cancellearn = true;

            std::string learnTag =
               cancellearn ? "Abort Macro MIDI Learn" : "MIDI Learn Macro...";
            addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu(learnTag), [this, cancellearn, ccid] {
                                                      if (cancellearn)
                                                         synth->learn_custom = -1;
                                                      else
                                                         synth->learn_custom = ccid;
                                                   });
            eid++;

            if (synth->storage.controllers[ccid] >= 0)
            {
               char txt4[16];
               decode_controllerid(txt4, synth->storage.controllers[ccid]);
               sprintf(txt, "Clear Learned MIDI (%s ", txt4);
               addCallbackMenu(contextMenu,
                               Surge::UI::toOSCaseForMenu(txt) + midicc_names[synth->storage.controllers[ccid]] + ")",
                               [this, ccid]() {
                                                    synth->storage.controllers[ccid] = -1;
                                                    synth->storage.save_midi_controllers();
                                                 });
               eid++;
            }

            contextMenu->addSeparator(eid++);

            addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Bipolar Mode"), [this, control, ccid]() {
                                                       bool bp =
                                                          !synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + ccid]->is_bipolar();
                                                       synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + ccid]->set_bipolar(bp);

                                                       float f =
                                                          synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + ccid]->get_output01();
                                                       control->setValue(f);
                                                       ((CModulationSourceButton*)control)->setBipolar(bp);
                                                       refresh_mod();
                                                    });
            contextMenu->checkEntry(
               eid, synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 + ccid]->is_bipolar());
            eid++;

            addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Rename Macro..."), [this, control, ccid, menuRect]() {
                   std::string pval = synth->storage.getPatch().CustomControllerLabel[ccid];
                   if( pval == "-" ) pval = "";
                   promptForMiniEdit(
                       pval,
                       "Enter a new name for the macro:", "Rename Macro", menuRect.getTopLeft(),
                       [this, control, ccid](const std::string& s) {
                          auto useS = s;
                          if( useS == "" ) useS = "-";
                          strncpy(synth->storage.getPatch().CustomControllerLabel[ccid], useS.c_str(),
                                  16);
                          ((CModulationSourceButton*)control)
                              ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);

                          control->setDirty();
                          control->invalid();
                          synth->refresh_editor = true;
                          // synth->updateDisplay();
                       });
                                                                                        });
            eid++;

#if TARGET_VST3
            SurgeSynthesizer::ID mid;
            if( synth->fromSynthSideId(modsource - ms_ctrl1 + metaparam_offset, mid ) )
               hostMenu = addVst3MenuForParams( contextMenu, mid, eid);
#endif

            midiSub->forget();
         }

         int lfo_id = isLFO(modsource) ? modsource - ms_lfo1 : -1;

         if (lfo_id >= 0)
         {
            contextMenu->addSeparator(eid++);
            addCallbackMenu(contextMenu, "Copy", [this, sc, lfo_id]() {
                                                    if (lfo_id >= 0)
                                                       synth->storage.clipboard_copy(cp_lfo, sc, lfo_id);
                                                 });
            eid++;

            if (synth->storage.get_clipboard_type() == cp_lfo)
            {
               addCallbackMenu(contextMenu, "Paste", [this, sc, lfo_id]() {
                                                        if (lfo_id >= 0)
                                                           synth->storage.clipboard_paste(cp_lfo, sc, lfo_id);
                                                        queue_refresh = true;
                                                     });
               eid++;
            }
         }
         frame->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget

#if TARGET_VST3
         if( hostMenu ) hostMenu->release();
#endif

         return 1;
      }
      return 0;
   }

   if (tag < start_paramtags)
      return 0;

   if (!(button & (kDoubleClick | kRButton | kControl)))
      return 0;

   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      Parameter* p = synth->storage.getPatch().param_ptr[ptag];
      
      // don't show RMB context menu for filter subtype if it's hidden/not applicable
      auto f1type = synth->storage.getPatch().scene[current_scene].filterunit[0].type.val.i;
      auto f2type = synth->storage.getPatch().scene[current_scene].filterunit[1].type.val.i;

      if (tag == f1subtypetag && (f1type == fut_none || f1type == fut_SNH))
         return 1;
      if (tag == f2subtypetag && (f2type == fut_none || f2type == fut_SNH))
         return 1;

      bool blockForLFO = false;
      if( p->ctrltype == ct_lfotype )
      {
         blockForLFO = true;
         auto *clfo = dynamic_cast<CLFOGui*>(control);
         if( clfo )
         {
            CPoint where;
            frame->getCurrentMouseLocation(where);
            frame->localToFrame(where);

            blockForLFO = !clfo->insideTypeSelector(where);
         }
      }

      // all the RMB context menus
      if ((button & kRButton) && !blockForLFO )
      {
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);


         menuRect.offset(where.x, where.y);

         COptionMenu* contextMenu =
            new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
         int eid = 0;

         // FIXME - only fail at this once
         /* if( bitmapStore->getBitmapByPath( synth->storage.datapath + "skins/shared/help-24.svg" ) == nullptr )
         {
            std::cout << "LOADING BMP" << std::endl;
            bitmapStore->loadBitmapByPath( synth->storage.datapath + "skins/shared/help-24.svg" );
         }
         auto helpbmp = bitmapStore->getBitmapByPath( synth->storage.datapath + "skins/shared/help-24.svg" );
         std::cout << "HELPBMP is " << helpbmp << std::endl; */
         //auto pp = IPlatformBitmap::createFromPath( (synth->storage.datapath + "skins/shared/help-14.png" ).c_str() );
         //auto helpbmp = new CBitmap( pp );

         std::string helpurl = helpURLFor(p);
         if( helpurl == "" )
         {
            contextMenu->addEntry((char*)p->get_full_name(), eid++);
         }
         else
         {
            std::string helpstr = "[?] ";
            auto lurl = fullyResolvedHelpURL(helpurl);
            addCallbackMenu(contextMenu, std::string(helpstr + p->get_full_name()).c_str(),
                            [lurl]() { Surge::UserInteractions::openURL(lurl); });
            eid++;
         }
         contextMenu->addSeparator(eid++);
         char txt[256], txt2[512];
         p->get_display(txt);
         sprintf(txt2, "Edit Value: %s", txt);
         if( p->valtype == vt_float )
         {
            if( p->can_temposync() && p->temposync )
            {
               COptionMenu *tsMenuR = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
               COptionMenu *tsMenuD = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
               COptionMenu *tsMenuT = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);

               // have 0 1 2 then 0 + log2 1.5 1 + log2 1.5 then 0 + log2 1.33 and so on
               for( int i=p->val_min.f; i <= p->val_max.f; ++i )
               {
                  float mul = 1.0;
                  float triplaboff = log2(1.33333333);
                  float tripoff = triplaboff;
                  float dotlaboff = log2(1.5);
                  float dotoff = dotlaboff;
                  if( p->ctrltype == ct_lforate || p->ctrltype == ct_lforate_deactivatable )
                  {
                     mul = -1.0;
                     triplaboff = log2(1.5);
                     tripoff = triplaboff;
                     dotlaboff = log2(1.3333333333);
                     dotoff = dotlaboff;
                  }
                  addCallbackMenu( tsMenuR, p->tempoSyncNotationValue( mul * (  (float) i  ) ),
                                   [p, i, this]() {
                                      p->val.f = (float)i;
                                      p->bound_value();
                                      this->synth->refresh_editor = true;
                                   } );
                  addCallbackMenu( tsMenuD, p->tempoSyncNotationValue( mul * (  (float) i  + dotlaboff ) ),
                                   [p, i, dotoff, this]() {
                                      p->val.f = (float)i + dotoff;
                                      p->bound_value();
                                      this->synth->refresh_editor = true;
                                   } );
                  addCallbackMenu( tsMenuT, p->tempoSyncNotationValue( mul * (  (float) i  + triplaboff ) ),
                                   [p, i, tripoff, this]() {
                                      p->val.f = (float)i + tripoff;
                                      p->bound_value();
                                      this->synth->refresh_editor = true;
                                   } );
               }
               contextMenu->addEntry( txt2 ); eid++;
               contextMenu->addSeparator(); eid++;
               contextMenu->addEntry( tsMenuR, "Straight" ); tsMenuR->forget(); eid++;
               contextMenu->addEntry( tsMenuD, "Dotted" ); tsMenuD->forget(); eid++;
               contextMenu->addEntry( tsMenuT, "Triplet" ); tsMenuT->forget(); eid++;
               contextMenu->addSeparator(); eid++;
            }
            else
            {
               addCallbackMenu(contextMenu, txt2,
                               [this,p,control]() {
                                  this->promptForUserValueEntry( p, control );
                               }
                  );
               eid++;
            }
         }
         else if( p->valtype == vt_bool )
         {
            // FIXME - make this a checked toggle
            auto b = addCallbackMenu(contextMenu, txt2,
                            [this,p,control]() {
                                        SurgeSynthesizer::ID pid;
                                        if( synth->fromSynthSideId(p->id, pid ) )
                                        {
                                           synth->setParameter01(pid, !p->val.b, false, false);
                                           repushAutomationFor(p);
                                           synth->refresh_editor = true;
                                        }
                            }
               );
            eid++;
            if( p->val.b )
               b->setChecked(true);

         }
         else if( p->valtype == vt_int )
         {
            if( p->can_setvalue_from_string() )
            {
               addCallbackMenu(contextMenu, txt2,
                               [this,p,control]() {
                                  this->promptForUserValueEntry( p, control );
                               }
                  );
               eid++;
            }
            else
            {
               int incr = 1;

               if( p->ctrltype == ct_vocoder_bandcount )
                  incr = 4;

               // we have a case where the number of menu entries to be generated depends on another parameter
               // so instead of using val_max.i directly, store it to local var and modify its value
               // when required
               int max = p->val_max.i;

               // currently we only have this case with filter subtypes - different filter types have a different number of them
               // so let's do this!
               if (p->ctrltype == ct_filtersubtype)
                  max = fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[p->ctrlgroup_entry].type.val.i] - 1;

               auto dm = dynamic_cast<ParameterDiscreteIndexRemapper*>( p->user_data );

               if( dm )
               {
                  std::map<std::string, std::map<int,int>> reorderMap;
                  for( int i=p->val_min.i; i<= max; i += incr )
                  {
                     int idx = dm->remapStreamedIndexToDisplayIndex( i );
                     auto gn = dm->groupNameAtStreamedIndex(i);
                     
                     reorderMap[gn][idx] = i;
                  }
                  bool useSubMenus = dm->hasGroupNames();
                  COptionMenu *sub = nullptr;
                  for( auto grp : reorderMap )
                  {
                     bool checkSub = false;
                     if( useSubMenus )
                     {
                        sub = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
                     }
                     for( auto rp : grp.second )
                     {
                        int i = rp.second;
                        std::string displaytxt = dm->nameAtStreamedIndex(i);
                     
#if WINDOWS
                           Surge::Storage::findReplaceSubstring(displaytxt, std::string("&"), std::string("&&"));
#endif
                        
                        auto b = addCallbackMenu( useSubMenus ? sub : contextMenu, displaytxt.c_str(),
                                                  [this,p,i, tag]() {
                                                     float ef = (1.0f * i - p->val_min.i) / (p->val_max.i - p->val_min.i);
                                                     synth->setParameter01(synth->idForParameter(p), ef, false,
                                                                              false);
                                                     repushAutomationFor(p);
                                                     synth->refresh_editor = true;
                                                    }
                           );
                        eid++;
                        if( i == p->val.i ) {
                           b->setChecked(true);
                           checkSub = true;
                        }
                     }
                     if( useSubMenus )
                     {
                        auto e = contextMenu->addEntry( sub, grp.first.c_str() ); sub->forget(); sub = nullptr;
                        e->setChecked( checkSub );
                     }
                  }
               }
               else
               {
                  for( int i=p->val_min.i; i<= max; i += incr )
                  {
                     char txt[256];
                     float ef = (1.0f * i - p->val_min.i) / (p->val_max.i - p->val_min.i);
                     p->get_display(txt, true, ef );
                     
                     std::string displaytxt = txt;
                     
#if WINDOWS
                        Surge::Storage::findReplaceSubstring(displaytxt, std::string("&"), std::string("&&"));
#endif
                     
                     auto b = addCallbackMenu(contextMenu, displaytxt.c_str(),
                                              [this,ef,p,i]() {
                                                 synth->setParameter01( synth->idForParameter(p), ef, false, false );
                                                 repushAutomationFor(p);
                                                 synth->refresh_editor=true;
                                              }
                        );
                     eid++;
                     if( i == p->val.i )
                        b->setChecked(true);
                  }
               }
            }
         }
         bool cancellearn = false;

         // Modulation and Learn semantics only apply to vt_float types in surge right now
         if( p->valtype == vt_float )
         {
            // if(p->can_temposync() || p->can_extend_range()) contextMenu->addSeparator(eid++);
            if (p->can_temposync())
            {
               auto r = addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Tempo Sync"),
                               [this, p, control]() {
                                  p->temposync = !p->temposync;
                                  if( p->temposync )
                                     p->bound_value();
                                  else if( control )
                                     p->set_value_f01( control->getValue() );

                                  if( this->lfodisplay )
                                     this->lfodisplay->invalid();
                                  auto *css = dynamic_cast<CSurgeSlider *>(control);
                                  if( css )
                                  {
                                     css->setTempoSync(p->temposync);
                                     css->invalid();
                                  }
                               });
               if( p->temposync )
                  r->setChecked( true );
               eid++;

               if (p->ctrlgroup == cg_ENV || p->ctrlgroup == cg_LFO)
               {
                  char label[256];
                  char prefix[128];
                  char pars[32];

                  int a = p->ctrlgroup_entry;

                  if (p->ctrlgroup == cg_ENV)
                  {
                     if (a == 0)
                        sprintf(prefix, "Amp EG");
                     else
                        sprintf(prefix, "Filter EG");
                  }
                  else if (p->ctrlgroup == cg_LFO)
                  {
                     if (a >= ms_lfo1 && a <= ms_lfo1 + n_lfos_voice)
                        sprintf(prefix, "Voice LFO %i", a - ms_lfo1 + 1);
                     else if (a >= ms_slfo1 && a <= ms_slfo1 + n_lfos_scene)
                        sprintf(prefix, "Scene LFO %i", a - ms_slfo1 + 1);
                  }

                  bool setTSTo;

#if WINDOWS
                     snprintf(pars, 32, "parameters");
#else
                     snprintf(pars, 32, "Parameters");
#endif

                  if( p->temposync )
                  {
                     snprintf(label, 256, "%s %s %s", Surge::UI::toOSCaseForMenu("Disable Tempo Sync for All").c_str(), prefix, pars);
                     setTSTo = false;
                  }
                  else
                  {
                     snprintf(label, 256, "%s %s %s", Surge::UI::toOSCaseForMenu("Enable Tempo Sync for All").c_str(), prefix, pars);
                     setTSTo = true;
                  }

                  addCallbackMenu( contextMenu, label,
                                   [this, p, setTSTo](){
                                      // There is surely a more efficient way but this is fine
                                      for (auto iter = this->synth->storage.getPatch().param_ptr.begin();
                                           iter != this->synth->storage.getPatch().param_ptr.end(); iter++)
                                      {
                                         Parameter* pl = *iter;
                                         if( pl->ctrlgroup_entry == p->ctrlgroup_entry &&
                                             pl->ctrlgroup == p->ctrlgroup &&
                                             pl->can_temposync()
                                            )
                                         {
                                            pl->temposync = setTSTo;
                                            if( setTSTo )
                                               pl->bound_value();
                                         }
                                      }
                                      this->synth->refresh_editor = true;
                                   } );
                  eid++;
               }
            }
            if (p->has_portaoptions())
            {
               contextMenu->addSeparator(eid++);

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Constant Rate"),
                               [this, p]() { p->porta_constrate = !p->porta_constrate; });
               contextMenu->checkEntry(eid, p->porta_constrate);
               eid++;

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Glissando"),
                               [this, p]() { p->porta_gliss = !p->porta_gliss; });
               contextMenu->checkEntry(eid, p->porta_gliss);
               eid++;

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Retrigger at Scale Degrees"),
                               [this, p]() { p->porta_retrigger = !p->porta_retrigger; });
               contextMenu->checkEntry(eid, p->porta_retrigger);
               eid++;

               contextMenu->addSeparator(eid++);

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Logarithmic Curve"),
                               [this, p]() { p->porta_curve = -1; });
               contextMenu->checkEntry(eid, (p->porta_curve == -1));
               eid++;

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Linear Curve"),
                               [this, p]() { p->porta_curve = 0; });
               contextMenu->checkEntry(eid, (p->porta_curve == 0));
               eid++;

               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Exponential Curve"),
                               [this, p]() { p->porta_curve = 1; });
               contextMenu->checkEntry(eid, (p->porta_curve == 1));
               eid++;

            }
            
            if (p->has_deformoptions())
            {                
               
               auto q = modsource_editor[current_scene];
               auto* lfodata = &(synth->storage.getPatch().scene[current_scene].lfo[q - ms_lfo1]);

               switch (lfodata->shape.val.i) {
               case lt_sine:
               case lt_tri:
               case lt_ramp:
                   contextMenu->addSeparator(eid++);

                   addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Deform Type 1"), [this, p]() {
                       p->deform_type = 0;
                       if (frame)
                           frame->invalid();
                   });
                   contextMenu->checkEntry(eid, (p->deform_type == 0));
                   eid++;
               


                   addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Deform Type 2"), [this, p]() {
                    
                       p->deform_type = 1;
                       if (frame)
                           frame->invalid();
                   });
                   contextMenu->checkEntry(eid, (p->deform_type == 1));
                   eid++;


                   addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Deform Type 3"), [this, p]() { 
                       p->deform_type = 2; 
                       if (frame)
                           frame->invalid();
                       });

                   contextMenu->checkEntry(eid, (p->deform_type == 2));
                   eid++;
               }
            }


            if (p->can_extend_range())
            {
               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Extend Range"),
                               [this, p]() { p->extend_range = !p->extend_range; this->synth->refresh_editor=true;});
               contextMenu->checkEntry(eid, p->extend_range);
               eid++;
            }
            if (p->can_be_absolute())
            {
               addCallbackMenu(contextMenu, "Absolute", [this, p]() { p->absolute = !p->absolute; });
               contextMenu->checkEntry(eid, p->absolute);
               eid++;
            }
            if (p->can_deactivate())
            {
               if( p->deactivated )
               {
                  addCallbackMenu(contextMenu, "Activate", [this, p]() { p->deactivated = false; this->synth->refresh_editor = true; } );
               }
               else
               {
                  addCallbackMenu(contextMenu, "Deactivate", [this, p]() { p->deactivated = true; this->synth->refresh_editor = true; } );

               }
               eid++;
            }

            contextMenu->addSeparator(eid++);

            // Construct submenus for explicit controller mapping
            COptionMenu* midiSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                   VSTGUI::COptionMenu::kNoDrawStyle |
                                                       VSTGUI::COptionMenu::kMultipleCheckStyle);
            COptionMenu* currentSub = nullptr;

            for (int subs = 0; subs < 7; ++subs)
            {
               if (currentSub)
               {
                  currentSub->forget();
                  currentSub = nullptr;
               }
               currentSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                            VSTGUI::COptionMenu::kNoDrawStyle |
                                                VSTGUI::COptionMenu::kMultipleCheckStyle);

               char name[16];

               sprintf(name, "CC %d ... %d", subs * 20, min((subs * 20) + 20, 128) - 1);

               auto added_to_menu = midiSub->addEntry(currentSub, name);

               for (int item = 0; item < 20; ++item)
               {
                  int mc = (subs * 20) + item;
                  int disabled = 0;

                  // these CCs cannot be used for MIDI learn (see
                  // SurgeSynthesizer::channelController)
                  if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 ||
                      (mc == 74 && synth->mpeEnabled) || (mc >= 98 && mc <= 101) || mc == 120 ||
                      mc == 123)
                     disabled = 1;

                  // we don't have any more CCs to cover, so break the loop
                  if (mc > 127)
                     break;

                  char name[128];

                  sprintf(name, "CC %d (%s) %s", mc, midicc_names[mc],
                          (disabled == 1 ? "- RESERVED" : ""));

                  CCommandMenuItem* cmd = new CCommandMenuItem(CCommandMenuItem::Desc(name));

                  cmd->setActions([this, p, ptag, mc](CCommandMenuItem* men) {
                     if (ptag < n_global_params)
                        p->midictrl = mc;
                     else
                     {
                        int a = ptag;
                        if (ptag >= (n_global_params + n_scene_params))
                           a -= ptag;

                        synth->storage.getPatch().param_ptr[a]->midictrl = mc;
                        synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl = mc;
                     }

                     synth->storage.save_midi_controllers();
                  });
                  cmd->setEnabled(!disabled);

                  auto added = currentSub->addEntry(cmd);

                  if ((ptag < n_global_params && p->midictrl == mc) ||
                      (ptag > n_global_params &&
                       synth->storage.getPatch().param_ptr[ptag]->midictrl == mc))
                  {
                     added->setChecked();
                     added_to_menu->setChecked();
                  }
               }
            }

            if (currentSub)
            {
               currentSub->forget();
               currentSub = nullptr;
            }

            contextMenu->addEntry(midiSub, Surge::UI::toOSCaseForMenu("Assign Parameter To..."));

            eid++;

            if (synth->learn_param > -1)
                cancellearn = true;

            std::string learnTag = cancellearn ? "Abort Parameter MIDI Learn" : "MIDI Learn Parameter...";
            addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu(learnTag), [this, cancellearn, p] {
                                                        if (cancellearn)
                                                        synth->learn_param = -1;
                                                        else
                                                        synth->learn_param = p->id;
                                                    });
            eid++;

            if (p->midictrl >= 0)
            {
               char txt4[16];
               decode_controllerid(txt4, p->midictrl);
               sprintf(txt, "Clear Learned MIDI (%s ", txt4);
               addCallbackMenu(contextMenu,
                               Surge::UI::toOSCaseForMenu(txt) + midicc_names[p->midictrl] + ")",
                               [this, p, ptag]() {
                                  if (ptag < n_global_params)
                                  {
                                     p->midictrl = -1;
                                  }
                                  else
                                  {
                                     int a = ptag;
                                     if (ptag >= (n_global_params + n_scene_params))
                                        a -= n_scene_params;

                                     synth->storage.getPatch().param_ptr[a]->midictrl = -1;
                                     synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl = -1;
                                  }

                                  synth->storage.save_midi_controllers();
                               });
               eid++;
            }

            int n_ms = 0;

            for (int ms = 1; ms < n_modsources; ms++)
               if (synth->isActiveModulation(ptag, (modsources)ms))
                  n_ms++;

            // see if we have any modulators that are unassigned, then create "Add Modulation from..." menu
            if (n_ms != n_modsources)
            {
               COptionMenu* addModSub = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                      VSTGUI::COptionMenu::kNoDrawStyle |
                                                          VSTGUI::COptionMenu::kMultipleCheckStyle);
               contextMenu->addSeparator(eid++);

               for (int k = 1; k < n_modsources; k++)
               {
                  modsources ms = (modsources)modsource_display_order[k];
                  if (!synth->isActiveModulation(ptag, (modsources)ms))
                  {
                     char tmptxt[512];
                     sprintf(tmptxt, "%s", modulatorName(ms, false).c_str());
                     addCallbackMenu(addModSub, tmptxt, [this, p, control, ms]() {
                        this->promptForUserValueEntry(p, control, ms);
                     });
                  }
               }

               contextMenu->addEntry(addModSub, Surge::UI::toOSCaseForMenu("Add Modulation from..."));

               if (addModSub)
               {
                  addModSub->forget();
                  addModSub = nullptr;
               }

               eid++;
            }

            if (n_ms)
            {
               contextMenu->addSeparator(eid++);

               for (int k = 1; k < n_modsources; k++)
               {
                  modsources ms = (modsources)k;
                  if (synth->isActiveModulation(ptag, ms))
                  {
                     char modtxt[256];
                     p->get_display_of_modulation_depth( modtxt, synth->getModDepth(ptag, ms), synth->isBipolarModulation(ms), Parameter::Menu);

                     char tmptxt[512];
                     sprintf(tmptxt, "Edit %s -> %s: %s", (char*)modulatorName(ms, true).c_str(),
                             p->get_name(), modtxt );
                     addCallbackMenu(contextMenu, tmptxt, [this, p, control, ms ]() {
                                                             this->promptForUserValueEntry( p, control, ms );
                                                          });
                     eid++;
                  }
               }
               contextMenu->addSeparator(eid++);
               for (int k = 1; k < n_modsources; k++)
               {
                  modsources ms = (modsources)k;
                  if (synth->isActiveModulation(ptag, ms))
                  {
                     char tmptxt[256];
                     snprintf(tmptxt, 256, "Clear %s -> %s", (char*)modulatorName(ms, true).c_str(),
                             p->get_name() );
                     // clear_ms[ms] = eid;
                     // contextMenu->addEntry(tmptxt, eid++);
                     addCallbackMenu(contextMenu, tmptxt, [this, ms, ptag]() {
                                                             synth->clearModulation(ptag, (modsources)ms);
                                                             refresh_mod();
                                                             /*
                                                             ** FIXME - this is a pretty big hammer to deal with
                                                             ** #1477 - can we be more parsimonious?
                                                             */
                                                             synth->refresh_editor = true;
                                                          });
                     eid++;
                  }
               }
               if (n_ms > 1)
               {
                  addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Clear All"), [this, ptag]() {
                                                               for (int ms = 1; ms < n_modsources; ms++)
                                                               {
                                                                  synth->clearModulation(ptag, (modsources)ms);
                                                               }
                                                               refresh_mod();
                                                               synth->refresh_editor = true;
                                                            });
                  eid++;
               }
            }
         } // end vt_float if statement

         if (p->ctrltype == ct_amplitude_clipper){
             contextMenu->addSeparator(eid++);
             addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu("Hard Clip Signals Over 0 dBFS"), [this]() {
                     synth->hardclipEnabled = !synth->hardclipEnabled;
                     });
             contextMenu->checkEntry(eid, synth->hardclipEnabled);
             eid++;
         }

#if TARGET_VST3
         auto hostMenu = addVst3MenuForParams(contextMenu, synth->idForParameter(p), eid );
#endif

         frame->addView(contextMenu); // add to frame
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget

#if TARGET_VST3
            if( hostMenu ) hostMenu->release();
#endif

         return 1;
      }
      // reset to default value
      else if (button & kDoubleClick)
      {
         if (synth->isValidModulation(ptag, modsource) && mod_editor)
         {
            CModulationSourceButton *cms = (CModulationSourceButton *)gui_modsrc[modsource];
            auto thisms = modsource;
            if( cms && cms->hasAlternate && cms->useAlternate )
               thisms = (modsources)cms->alternateId;

            synth->clearModulation(ptag, thisms);
            ((CSurgeSlider*)control)->setModValue(synth->getModulation(p->id, thisms));
            ((CSurgeSlider*)control)->setModPresent(synth->isModDestUsed(p->id));
            ((CSurgeSlider*)control)->setModCurrent(synth->isActiveModulation(p->id, thisms), synth->isBipolarModulation(thisms));
            // control->setGhostValue(p->get_value_f01());
            oscdisplay->invalid();
            return 0;
         }
         else
         {
            /*
            ** This code resets you to default if you double click on control
            ** but on the lfoshape UI this is undesirable; it means if you accidentally
            ** control click on step sequencer, say, you go back to sin and lose your
            ** edits. So supress
            */
            if (p->ctrltype != ct_lfotype)
            {
               p->set_value_f01(p->get_default_value_f01());
               control->setValue(p->get_value_f01());
               if (oscdisplay && (p->ctrlgroup == cg_OSC))
                  oscdisplay->invalid();
               if (lfodisplay && (p->ctrlgroup == cg_LFO))
                  lfodisplay->invalid();
               control->invalid();
               return 0;
            }
         }
      } 
      // exclusive mute/solo in the mixer
      else if (button & kControl)
      {
         if (p->ctrltype == ct_bool_mute)
         {
            Parameter* o1 = &(synth->storage.getPatch().scene[current_scene].mute_o1);
            Parameter* r23 = &(synth->storage.getPatch().scene[current_scene].mute_ring_23);

            auto curr = o1;

            while (curr <= r23)
            {
               if (curr->id == p->id)
                  curr->val.b = true;
               else
                  curr->val.b = false;

               curr++;
            }

            synth->refresh_editor = true;
         }
         else if (p->ctrltype == ct_bool_solo)
         {
            Parameter* o1 = &(synth->storage.getPatch().scene[current_scene].solo_o1);
            Parameter* r23 = &(synth->storage.getPatch().scene[current_scene].solo_ring_23);

            auto curr = o1;

            while (curr <= r23)
            {
               if (curr->id == p->id)
                  curr->val.b = true;
               else
                  curr->val.b = false;

               curr++;
            }

            synth->refresh_editor = true;
         }
         else
            p->bound_value();
      }
   }
   return 0;
}

void SurgeGUIEditor::valueChanged(CControl* control)
{
   if (!frame)
      return;
   if (!editor_open)
      return;
   long tag = control->getTag();

   if( typeinDialog != nullptr && tag != tag_value_typein )
   {
      typeinDialog->setVisible(false);
      removeFromFrame.push_back(typeinDialog);
      typeinDialog = nullptr;
      typeinResetCounter = -1;
      typeinEditTarget = nullptr;
      typeinMode = Inactive;
   }

   if( tag == tag_status_zoom )
   {
      control->setValue(0);
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showZoomMenu(where);
      return;
   }

   if( tag == tag_lfo_menu )
   {
      control->setValue(0);
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      showLfoMenu(where);
      return;
   }

   if( tag == tag_status_mpe )
   {
      toggleMPE();
      return;
   }
   if( tag == tag_status_tune )
   {
      toggleTuning();
      return;
   }

   if( tag == tag_mseg_edit )
   {
      if( control->getValue() > 0.5 )
      {
         showMSEGEditor();
      } else {
         closeMSEGEditor();
      }
      return;
   }

   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      if (((CModulationSourceButton*)control)->event_is_drag)
      {
         int t = (tag - tag_mod_source0);
         ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[t])
            ->set_target01(control->getValue(), false);

         SurgeSynthesizer::ID tid;
         if( synth->fromSynthSideId(t + metaparam_offset - ms_ctrl1, tid ) )
         {
            synth->sendParameterAutomation(tid, control->getValue());
         }
         return;
      }
      else
      {
         CModulationSourceButton *cms = (CModulationSourceButton *)control;
         int state = cms->get_state();
         modsources newsource = (modsources)(tag - tag_mod_source0);
         long buttons = 0; // context->getMouseButtons(); // temp fix vstgui 3.5
         bool ciep =
            ((CModulationSourceButton*)control)->click_is_editpart && (newsource >= ms_lfo1);
         if (!ciep)
         {
            switch (state & 3)
            {
            case 0:
               modsource = newsource;
               if (mod_editor)
                  mod_editor = true;
               else
                  mod_editor = false;
               queue_refresh = true;
               refresh_mod();
               break;
            case 1:
               modsource = newsource;
               mod_editor = true;
               refresh_mod();
               break;
            case 2:
               modsource = newsource;
               mod_editor = false;
               refresh_mod();
               break;
            };
         }

         if (isLFO(newsource) && !(buttons & kShift))
         {
            if (modsource_editor[current_scene] != newsource)
            {
               auto tabPosMem = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "rememberTabPositionsPerScene", 0);
               
               if (tabPosMem)
                  modsource_editor[current_scene] = newsource;
               else
               {
                  for (int i = 0; i < n_scenes; i++)
                  {
                     modsource_editor[i] = newsource;
                  }
               }

               if(editorOverlay && editorOverlayTag == "msegEditor" )
               {
                  auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[newsource-ms_lfo1]);
                  if( ld->shape.val.i == lt_mseg )
                  {
                     showMSEGEditor();
                  }
                  else
                  {
                     closeMSEGEditor();
                  }
               }


               queue_refresh = true;
            }
         }
      }

      return;
   }

   if ((tag == f1subtypetag) || (tag == f2subtypetag))
   {
      int idx = (tag == f2subtypetag) ? 1 : 0;
      int a = synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i + 1;
      int nn =
         fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
      if (a >= nn)
         a = 0;
      synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
      if (!nn)
         ((CSwitchControl*)control)->ivalue = 0;
      else
         ((CSwitchControl*)control)->ivalue = a + 1;
      control->invalid();
      synth->switch_toggled_queued = true;
      return;
   }

   switch (tag)
   {
   case tag_scene_select:
   {
      current_scene = (int)(control->getValue() * 1.f) + 0.5f;
      synth->release_if_latched[synth->storage.getPatch().scene_active.val.i] = true;
      synth->storage.getPatch().scene_active.val.i = current_scene;
      // synth->storage.getPatch().param_ptr[scene_select_pid]->set_value_f01(control->getValue());

      if (editorOverlay && editorOverlayTag == "msegEditor")
      {
         auto ld = &(synth->storage.getPatch().scene[current_scene].lfo[modsource_editor[current_scene] - ms_lfo1]);
         if (ld->shape.val.i == lt_mseg)
         {
            showMSEGEditor();
         }
         else
         {
            closeMSEGEditor();
         }
      }

      queue_refresh = true;
      return;
   }
   break;
   case tag_patchname:
   {
      int id = ((CPatchBrowser*)control)->sel_id;
      // synth->load_patch(id);
      synth->patchid_queue = id;
      synth->processThreadunsafeOperations();
      patchCountdown = 30;
      return;
   }
   break;
   case tag_mp_category:
   {
      if( editorOverlay && editorOverlayTag == "storePatch" )
      {
         closeStorePatchDialog();
      }

      if (control->getValue() > 0.5f)
         synth->incrementCategory(true);
      else
         synth->incrementCategory(false);
      return;
   }
   break;
   case tag_mp_patch:
   {
      if( editorOverlay && editorOverlayTag == "storePatch" )
      {
         closeStorePatchDialog();
      }

      if (control->getValue() > 0.5f)
         synth->incrementPatch(true);
      else
         synth->incrementPatch(false);
      return;
   }
   break;
   case tag_mp_jogfx:
   {
      CFxMenu *fxm = dynamic_cast<CFxMenu*>(fxmenu);
      auto jog = [this, fxm](int byThis ) {
                    this->selectedFX[this->current_fx] = std::max( this->selectedFX[this->current_fx] + byThis, -1 );
                    if( ! fxm->loadSnapshotByIndex( this->selectedFX[this->current_fx] ) )
                    {
                       // Try and go back to 0. This is the wrong behavior for negative jog
                       this->selectedFX[this->current_fx] = 0;
                       fxm->loadSnapshotByIndex( 0 );
                    }
                 };

      if( fxm )
      {
         if( fxm->selectedIdx >= 0 && fxm->selectedIdx != selectedFX[current_fx] )
            selectedFX[current_fx] = fxm->selectedIdx;

         if (control->getValue() > 0.5f)
         {
            jog( +1 );
         }
         else
         {
            jog( -1 );
         }

         if( fxPresetLabel )
         {
            fxPresetLabel->setText( fxm->selectedName.c_str() );
            fxPresetName[this->current_fx] = fxm->selectedName;
         }
      }
      else
      {
      }
      return;
   }
   break;
   case tag_settingsmenu:
   {
      CRect r = control->getViewSize();
      CRect menuRect;
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);

      menuRect.offset(where.x, where.y);

      useDevMenu = false;
      showSettingsMenu(menuRect);
   }
   break;
   case tag_osc_select:
   {
      auto tabPosMem = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "rememberTabPositionsPerScene", 0);

      if (tabPosMem)
         current_osc[current_scene] = (int)(control->getValue() * 2.f + 0.5f);
      else
      {
         for (int i = 0; i < n_scenes; i++)
         {
            current_osc[i] = (int)(control->getValue() * 2.f + 0.5f);
         }
      }

      queue_refresh = true;
      return;
   }
   break;
   case tag_fx_select:
   {
      auto fxc = ((CEffectSettings*)control);
      int d = fxc->get_disable();
      synth->fx_suspend_bitmask = synth->storage.getPatch().fx_disable.val.i ^ d;
      synth->storage.getPatch().fx_disable.val.i = d;
      fxc->set_disable(d);

      int nfx = fxc->get_current();
      if (current_fx != nfx)
      {
         current_fx = nfx;
         queue_refresh = true;
      }

      return;
   }
   break;
   case tag_osc_menu:
   {
      synth->switch_toggled_queued = true;
      queue_refresh = true;
      synth->processThreadunsafeOperations();
      return;
   }
   break;
   case tag_fx_menu:
   {
      synth->load_fx_needed = true;
      // queue_refresh = true;
      synth->fx_reload[current_fx & 7] = true;
      synth->processThreadunsafeOperations();

      CFxMenu *fxm = dynamic_cast<CFxMenu*>(fxmenu);
      if( fxm && fxm->selectedIdx >= 0 )
      {
         selectedFX[current_fx] = fxm->selectedIdx;
         fxPresetName[current_fx] = fxm->selectedName;
      }
      else if( fxPresetLabel )
      {
         fxPresetLabel->setText( fxm->selectedName.c_str() );
         fxPresetName[this->current_fx] = fxm->selectedName;
      }

      return;
   }
   break;
   case tag_store:
   {
      patchdata p;

      p.name = synth->storage.getPatch().name;
      p.category = synth->storage.getPatch().category;
      p.author = synth->storage.getPatch().author;
      p.comments = synth->storage.getPatch().comment;

      string defaultAuthor = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchAuthor", "");
      string defaultComment = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchComment", "");
      string oldAuthor = "";

      if( ! Surge::Storage::isValidUTF8( defaultAuthor ) )
      {
         defaultAuthor = "";
      }
      if( ! Surge::Storage::isValidUTF8( defaultComment ) )
      {
         defaultComment = "";
      }

      if (p.author == "" && defaultAuthor != "")
      {
         p.author = defaultAuthor;
      }

      if (p.author != "" && defaultAuthor != "")
      {
         if (_stricmp(p.author.c_str(), defaultAuthor.c_str()))
         {
            oldAuthor = p.author;
            p.author = defaultAuthor;
         }
      }

      if (p.comments == "" && defaultComment != "")
      {
         p.comments = defaultComment;
      }

      if (oldAuthor != "")
      {
         if (p.comments == "")
            p.comments += "Original patch by " + oldAuthor;
         else
            p.comments += " (Original patch by " + oldAuthor + ")";
      }

      showStorePatchDialog();

      patchName->setText(p.name.c_str());
      patchCategory->setText(p.category.c_str());
      patchCreator->setText(p.author.c_str());
      patchComment->setText(p.comments.c_str());
   }
   break;
   case tag_store_ok:
   {
      // prevent duplicate execution of savePatch() by detecting if the Store Patch dialog is displayed or not
      // FIXME: baconpaul will know a better and more correct way to fix this
      if (editorOverlay && editorOverlayTag == "storePatch")
      {
         /*
         ** Don't allow a blank patch
         */
         std::string whatIsBlank = "";
         bool haveBlanks = false;
       
         if (!Surge::Storage::isValidName(patchName->getText().getString()))
         {
            whatIsBlank = "name";
            haveBlanks = true;
         }
         if (!Surge::Storage::isValidName(patchCategory->getText().getString()))
         {
            whatIsBlank = whatIsBlank + (haveBlanks ? " and category" : "category");
            haveBlanks = true;
         }
         if (haveBlanks)
         {
            Surge::UserInteractions::promptError(
                std::string("Unable to store a patch due to invalid ") + whatIsBlank +
                            ". Please save again and provide a complete " + whatIsBlank + ".",
                            "Patch Saving Error");
         }
         else
         {
            synth->storage.getPatch().name = patchName->getText();
            synth->storage.getPatch().author = patchCreator->getText();
            synth->storage.getPatch().category = patchCategory->getText();
            synth->storage.getPatch().comment = patchComment->getText();
       
            synth->storage.getPatch().patchTuning.tuningStoredInPatch = patchTuning->getValue() > 0.5;
            if (synth->storage.getPatch().patchTuning.tuningStoredInPatch)
            {
               synth->storage.getPatch().patchTuning.tuningContents = synth->storage.currentScale.rawText;
               if (synth->storage.isStandardMapping)
               {
                  synth->storage.getPatch().patchTuning.mappingContents = "";
               }
               else
               {
                  synth->storage.getPatch().patchTuning.mappingContents = synth->storage.currentMapping.rawText;
               }
            }
       
            synth->storage.getPatch().dawExtraState.isPopulated = false; // Ignore whatever comes from the DAW
       
            synth->savePatch();
         }

         closeStorePatchDialog();
         frame->setDirty();
      }
   }
   break;
   case tag_store_cancel:
   {
      closeStorePatchDialog();
      frame->setDirty();
   }
   break;
   case tag_editor_overlay_close:
   {
      if( editorOverlay != nullptr )
      {
         editorOverlay->setVisible(false);
         removeFromFrame.push_back(editorOverlay);
         editorOverlayOnClose();
         editorOverlay = nullptr;
         editorOverlayTag = "";
         editorOverlayTagAtClose = "";
      }
   }
   break;
   case tag_miniedit_ok:
   case tag_miniedit_cancel:
   {
      if( minieditOverlay != nullptr )
      {
         if( minieditTypein )
         {
            auto q = minieditTypein->getText().getString();
            if( tag == tag_miniedit_ok )
            {
               minieditOverlayDone( q.c_str() );
            }
            minieditTypein->looseFocus();
         }
         minieditOverlay->setVisible( false );
         removeFromFrame.push_back( minieditOverlay );
         minieditOverlay = nullptr;
      }
   }
   break;
   case tag_value_typein:
   {
      if( typeinDialog && typeinMode != Inactive )
      {
         std::string t = typeinValue->getText().getString();
         bool isInvalid = false;
         if( typeinMode == Param && typeinEditTarget && typeinModSource > 0 )
         {
            bool valid = false;
            auto mv = typeinEditTarget->calculate_modulation_value_from_string( t, valid );

            if( ! valid )
            {
               isInvalid = true;
            }
            else
            {
               synth->setModulation(typeinEditTarget->id, (modsources)typeinModSource, mv );
               synth->refresh_editor = true;

               typeinDialog->setVisible(false);
               removeFromFrame.push_back(typeinDialog);
               typeinDialog = nullptr;
               typeinResetCounter = -1;
               typeinEditTarget = nullptr;
               typeinMode = Inactive;
            }
         }
         else if( typeinMode == Param && typeinEditTarget && typeinEditTarget->set_value_from_string( t ) )
         {
            repushAutomationFor(typeinEditTarget);
            isInvalid = false;
            synth->refresh_editor = true;
            typeinDialog->setVisible(false);
            removeFromFrame.push_back(typeinDialog);
            typeinDialog = nullptr;
            typeinResetCounter = -1;
            typeinEditTarget = nullptr;
            typeinMode = Inactive;
         }
         else if( typeinMode == Control )
         {
            auto cms = ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[typeinModSource]);
            bool bp = cms->is_bipolar();
            float val = std::atof( t.c_str() ) / 100.0;
            if( ( bp && val >= -1 && val <= 1 ) || ( val >= 0 && val <= 1 ) )
            {
               cms->output = val;
               cms->target = val;
               // This doesn't seem to work so hammer away
               if( typeinEditControl )
               {
                  typeinEditControl->invalid();
               }
               synth->refresh_editor = true;
            }
            else
            {
               isInvalid = true;
            }
         }
         else
         {
            isInvalid = true;
         }

         if( isInvalid ) {
            auto l = typeinLabel->getText().getString();
            promptForUserValueEntry( typeinEditTarget, typeinEditControl, typeinModSource );
            typeinResetCounter = 20;
            typeinResetLabel = l;
            typeinLabel->setText( "Invalid Entry" );
            typeinValue->setText(t.c_str());
            typeinLabel->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Error));
         }

      }
   }
   break;
   default:
   {
      int ptag = tag - start_paramtags;

      if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
      {
         Parameter* p = synth->storage.getPatch().param_ptr[ptag];

         char pname[256], pdisp[128], txt[128];
         bool modulate = false;

         // This allows us to turn on and off the editor. FIXME mseg check it
         if( p->ctrltype == ct_lfotype )
            synth->refresh_editor = true;

         if (modsource && mod_editor && synth->isValidModulation(p->id, modsource) &&
             dynamic_cast<CSurgeSlider*>(control) != nullptr)
         {
            modsources thisms = modsource;
            if( gui_modsrc[modsource] )
            {
               CModulationSourceButton *cms = (CModulationSourceButton *)gui_modsrc[modsource];
               if( cms->hasAlternate && cms->useAlternate )
                  thisms = (modsources) cms->alternateId;
            }
            bool quantize_mod = frame->getCurrentMouseButtons() & kControl;
            float mv = ((CSurgeSlider*)control)->getModValue();
            if( quantize_mod )
            {
               mv = p->quantize_modulation(mv);
               // maybe setModValue here
            }

            synth->setModulation(ptag, thisms, mv );
            ((CSurgeSlider*)control)->setModPresent(synth->isModDestUsed(p->id));
            ((CSurgeSlider*)control)->setModCurrent(synth->isActiveModulation(p->id, thisms), synth->isBipolarModulation(thisms));

            SurgeSynthesizer::ID ptagid;
            if( synth->fromSynthSideId(ptag, ptagid ))
               synth->getParameterName(ptagid, txt);
            sprintf(pname, "%s -> %s", modulatorName(thisms,true).c_str(), txt);
            ModulationDisplayInfoWindowStrings mss;
            p->get_display_of_modulation_depth(pdisp, synth->getModDepth(ptag, thisms), synth->isBipolarModulation(thisms), Parameter::InfoWindow, &mss);
            if( mss.val != "" )
            {
               ((CParameterTooltip*)infowindow)->setLabel(pname, pdisp);
               ((CParameterTooltip*)infowindow)->setMDIWS(mss);

            }
            else
            {
               ((CParameterTooltip*)infowindow)->setLabel(pname, pdisp);
               ((CParameterTooltip*)infowindow)->clearMDIWS();
            }
            modulate = true;

            if (isCustomController(modsource))
            {
               int ccid = modsource - ms_ctrl1;
               char* lbl = synth->storage.getPatch().CustomControllerLabel[ccid];

               if ((lbl[0] == '-') && !lbl[1])
               {
                  strncpy(lbl, p->get_name(), 15);
                  synth->storage.getPatch().CustomControllerLabel[ccid][15] = 0;
                  ((CModulationSourceButton*)gui_modsrc[modsource])->setlabel(lbl);
                  ((CModulationSourceButton*)gui_modsrc[modsource])->invalid();
               }
            }
         }
         else
         {
            auto val = control->getValue();
            if( p->ctrltype == ct_scenemode )
            {

               /*
               ** See the comment in the constructor of ct_scenemode above
               */
               auto cs2 = dynamic_cast<CHSwitch2 *>(control);
               auto im = 0;
               if( cs2 )
               {
                  im = cs2->getIValue();
                  if( im == 3 ) im = 2;
                  else if( im == 2 ) im = 3;
                  val = 0.005 + 0.99 * ( (float)( im ) / (n_scenemodes-1) );
               }

               /*
               ** Now I also need to toggle the split key state
               */
               auto nf = dynamic_cast<CNumberField *>(splitpointControl);
               if( nf )
               {
                  int cm = nf->getControlMode();
                  if( im == sm_chsplit && cm != cm_midichannel_from_127 )
                  {
                     nf->setControlMode(cm_midichannel_from_127);
                     nf->invalid();
                  }
                  else if( im == sm_split && cm != cm_notename ) {
                     nf->setControlMode(cm_notename);
                     nf->invalid();
                  }
                  else if( (im == sm_single || im == sm_dual ) && cm != cm_none ) {
                     nf->setControlMode(cm_none);
                     nf->invalid();
                  }

               }
            }

            bool force_integer = frame->getCurrentMouseButtons() & kControl;
            SurgeSynthesizer::ID ptagid;
            synth->fromSynthSideId(ptag, ptagid);
            if (synth->setParameter01(ptagid, val, false, force_integer))
            {
               queue_refresh = true;
               return;
            }
            else
            {
               synth->sendParameterAutomation(ptagid, synth->getParameter01(ptagid));

               if (dynamic_cast<CSurgeSlider*>(control) != nullptr)
                  ((CSurgeSlider*)control)->SetQuantitizedDispValue(p->get_value_f01());
               else
                  control->invalid();
               synth->getParameterName(ptagid, pname);
               synth->getParameterDisplay(ptagid, pdisp);
               char pdispalt[256];
               synth->getParameterDisplayAlt(ptagid, pdispalt);
               ((CParameterTooltip*)infowindow)->setLabel(0, pdisp, pdispalt);
               ((CParameterTooltip*)infowindow)->clearMDIWS();
               if (p->ctrltype == ct_polymode)
                  modulate = true;
            }

            if( p->ctrltype == ct_bool_unipolar || p->ctrltype == ct_lfotype )
            {
               // The green line might change so...
               refresh_mod();
            }
         }
         if (!queue_refresh)
         {
            if (!(p->ctrlstyle & kNoPopup))
            {
               draw_infowindow(ptag, control, modulate);
            }

            if (oscdisplay && ((p->ctrlgroup == cg_OSC) || (p->ctrltype == ct_character)))
            {
               oscdisplay->setDirty();
               oscdisplay->invalid();
            }
            if (lfodisplay && (p->ctrlgroup == cg_LFO))
            {
               lfodisplay->setDirty();
               lfodisplay->invalid();
            }
            if( editorOverlay )
            {
               editorOverlay->invalid();
            }
         }
      }
      break;
   }
   }

   if ((tag == (f1subtypetag - 1)) || (tag == (f2subtypetag - 1)))
   {
      int idx = (tag == (f2subtypetag - 1)) ? 1 : 0;

      int a = synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i;
      int nn =
         fut_subcount[synth->storage.getPatch().scene[current_scene].filterunit[idx].type.val.i];
      if (a >= nn)
         a = 0;
      synth->storage.getPatch().scene[current_scene].filterunit[idx].subtype.val.i = a;
      if (!nn)
         ((CSwitchControl*)filtersubtype[idx])->ivalue = 0;
      else
         ((CSwitchControl*)filtersubtype[idx])->ivalue = a + 1;

      filtersubtype[idx]->setDirty();
      filtersubtype[idx]->invalid();
   }

   if (tag == fmconfig_tag)
   {
      // FM depth control
      int i = synth->storage.getPatch().scene[current_scene].fm_depth.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
         ((CSurgeSlider*)param[i])->disabled =
             (synth->storage.getPatch().scene[current_scene].fm_switch.val.i ==
              fm_off);

      param[i]->setDirty();
      param[i]->invalid();
   }

   if (tag == filterblock_tag)
   {
      // pan2 control
      int i = synth->storage.getPatch().scene[current_scene].width.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
         ((CSurgeSlider*)param[i])->disabled =
            (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
             fb_stereo) &&
            (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i !=
             fb_wide);

      param[i]->setDirty();
      param[i]->invalid();

      // feedback control
      i = synth->storage.getPatch().scene[current_scene].feedback.id;
      if (param[i] && dynamic_cast<CSurgeSlider*>(param[i]) != nullptr)
         ((CSurgeSlider*)param[i])->disabled =
            (synth->storage.getPatch().scene[current_scene].filterblock_configuration.val.i ==
             fb_serial);

      param[i]->setDirty();
      param[i]->invalid();
   }

   if (tag == fxbypass_tag) // still do the normal operation, that's why it's outside the
      // switch-statement
   {
      if (ccfxconf)
      {
         ((CEffectSettings*)ccfxconf)->set_bypass(synth->storage.getPatch().fx_bypass.val.i);
         ccfxconf->invalid();
      }

      switch (synth->storage.getPatch().fx_bypass.val.i)
      {
      case fxb_no_fx:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xff;
         break;
      case fxb_scene_fx_only:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0xf0;
         break;
      case fxb_no_sends:
         synth->fx_suspend_bitmask = synth->fx_suspend_bitmask | 0x30;
         break;
      case fxb_all_fx:
      default:
         break;
      }
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::beginEdit(long tag)
{
   if (tag < start_paramtags)
   {
      return;
   }

   int synthidx = tag - start_paramtags;
   SurgeSynthesizer::ID did;

   if ( synth->fromSynthSideId( synthidx, did ) )
   {
      super::beginEdit(did.getDawSideId());
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::endEdit(long tag)
{
   if (tag < start_paramtags)
   {
      return;
   }

   int synthidx = tag - start_paramtags;
   SurgeSynthesizer::ID did;

   if ( synth->fromSynthSideId( synthidx, did ) )
   {
      super::endEdit(did.getDawSideId());
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlBeginEdit(VSTGUI::CControl* control)
{
#if TARGET_AUDIOUNIT
   long tag = control->getTag();
   int ptag = tag - start_paramtags;

   if( tag >= tag_mod_source0 + ms_ctrl1 && tag <= tag_mod_source0 + ms_ctrl8 )
   {
      ptag = metaparam_offset + tag - tag_mod_source0 - ms_ctrl1;
      SurgeSynthesizer::ID did;
      if( synth->fromSynthSideId(ptag, did ))
      {
         synth->getParent()->ParameterBeginEdit(did.getDawSideIndex());
      }
   }
   else if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      SurgeSynthesizer::ID did;

      if ( synth->fromSynthSideId( ptag, did ) )
      {
         synth->getParent()->ParameterBeginEdit(did.getDawSideIndex());
      }
   }

#endif
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::controlEndEdit(VSTGUI::CControl* control)
{
#if TARGET_AUDIOUNIT
   long tag = control->getTag();
   int ptag = tag - start_paramtags;
   if( tag >= tag_mod_source0 + ms_ctrl1 && tag <= tag_mod_source0 + ms_ctrl8 )
   {
      ptag = metaparam_offset + tag - tag_mod_source0 - ms_ctrl1;
      SurgeSynthesizer::ID did;
      if( synth->fromSynthSideId(ptag, did ))
         synth->getParent()->ParameterEndEdit(did.getDawSideIndex());
   }
   else if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      SurgeSynthesizer::ID did;

      if ( synth->fromSynthSideId( ptag, did ) )
      {
         synth->getParent()->ParameterEndEdit(did.getDawSideIndex());
      }
   }
#endif
   if (((CParameterTooltip*)infowindow)->isVisible())
   {
      auto cs = dynamic_cast<CSurgeSlider *>(control);
      if( cs == nullptr || cs->hasBeenDraggedDuringMouseGesture )
         ((CParameterTooltip*)infowindow)->Hide();
      else
      {
#if LINUX
         clear_infoview_countdown = 15;
#else
         clear_infoview_countdown = 15;
#endif
      }
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::draw_infowindow(int ptag, CControl* control, bool modulate, bool forceMB)
{
   long buttons = 1; // context?context->getMouseButtons():1;

   if (buttons && forceMB)
      return; // don't draw via CC if MB is down

   // A heuristic
   auto ml = ((CParameterTooltip*)infowindow)->getMaxLabelLen();
   auto iff = 148;
   // This is just empirical. It would be lovely to use the actual string width but that needs a draw context
   // of for these to be TextLabels so we can call sizeToFit
   if( ml > 24 )
      iff += ( ml - 24 ) * 5;

   auto modValues = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "modWindowShowsValues", 0);

   ((CParameterTooltip*)infowindow)->setExtendedMDIWS(modValues);
   CRect r(0, 0, iff, 18);
   if (modulate)
   {
      r.bottom += ( ((CParameterTooltip*)infowindow)->hasMDIWS() & modValues ? 36 : 18 );
      if( modValues ) r.right += 20;
   }

   CRect r2 = control->getViewSize();

   // OK this is a heuristic to stop deform overpainting and stuff
   if( r2.bottom > getWindowSizeY() - r.getHeight() - 2 )
   {
      // stick myself on top please
      r.offset((r2.left / 150) * 150, r2.top - r.getHeight() - 2);

   }
   else
   {
      r.offset((r2.left / 150) * 150, r2.bottom);
   }

   if (r.bottom > getWindowSizeY() - 2)
   {
      int ao = (getWindowSizeY() - 2 - r.bottom);
      r.offset(0, ao);
   }

   if (r.right > getWindowSizeX() - 2)
   {
      int ao = (getWindowSizeX() - 2 - r.right);
      r.offset(ao, 0);
   }

   if( r.left < 0 ) {
      int ao = 2 - r.left;
      r.offset( ao, 0 );
   }

   if( r.top < 0 ) {
      int ao = 2 - r.top;
      r.offset( 0, ao );
   }

   if (buttons || forceMB)
   {
      // make sure an infowindow doesn't appear twice
      if (((CParameterTooltip*)infowindow)->isVisible())
      {
         ((CParameterTooltip*)infowindow)->Hide();
         ((CParameterTooltip*)infowindow)->invalid();
      }

      ((CParameterTooltip*)infowindow)->setViewSize(r);
      ((CParameterTooltip*)infowindow)->Show();
      infowindow->invalid();
      // on Linux the infoview closes too soon
#if LINUX
      clear_infoview_countdown = 100;
#else
      clear_infoview_countdown = 40;
#endif
   }
   else
   {
      ((CParameterTooltip*)infowindow)->Hide();
      frame->invalidRect(r);
   }
}

long SurgeGUIEditor::applyParameterOffset(long id)
{
   return id-start_paramtags;
}

long SurgeGUIEditor::unapplyParameterOffset(long id)
{
   return id + start_paramtags;
}

// Status Panel Callbacks
void SurgeGUIEditor::toggleMPE()
{
   this->synth->mpeEnabled = ! this->synth->mpeEnabled;
   if( statusMPE )
   {
      statusMPE->setValue(this->synth->mpeEnabled ? 1 : 0 );
      statusMPE->invalid();
   }
}
void SurgeGUIEditor::showZoomMenu(VSTGUI::CPoint& where)
{
   CRect menuRect;
   menuRect.offset(where.x, where.y);
   auto m = makeZoomMenu(menuRect, true);

   frame->addView(m);
   m->setDirty();
   m->popup();
   frame->removeView(m, true);
}

void SurgeGUIEditor::showMPEMenu(VSTGUI::CPoint& where)
{
   CRect menuRect;
   menuRect.offset(where.x, where.y);
   auto m = makeMpeMenu(menuRect, true);

   frame->addView(m);
   m->setDirty();
   m->popup();
   frame->removeView(m, true);
}
void SurgeGUIEditor::showLfoMenu(VSTGUI::CPoint& where)
{
   CRect menuRect;
   menuRect.offset(where.x, where.y);
   auto m = makeLfoMenu(menuRect);

   frame->addView(m);
   m->setDirty();
   m->popup();
   frame->removeView(m, true);
}


void SurgeGUIEditor::toggleTuning()
{
   if( this->synth->storage.isStandardTuning && tuningCacheForToggle.size() > 0 )
   {
      try
      {
         this->synth->storage.retuneToScale(Tunings::parseSCLData(tuningCacheForToggle));
         if( mappingCacheForToggle.size() > 0 )
            this->synth->storage.remapToKeyboard(Tunings::parseKBMData(mappingCacheForToggle));
      } catch( Tunings::TuningError &e )
      {
         Surge::UserInteractions::promptError( e.what(), "Microtuning Error" );
      }

   }
   else if( ! this->synth->storage.isStandardTuning )
   {
      tuningCacheForToggle = this->synth->storage.currentScale.rawText;
      if( ! this->synth->storage.isStandardMapping )
      {
         mappingCacheForToggle = this->synth->storage.currentMapping.rawText;
      }
      this->synth->storage.remapToStandardKeyboard();
      this->synth->storage.init_tables();
   }

   if( statusTune )
      statusTune->setValue( this->synth->storage.isStandardTuning ? 0 : 1 );

   this->synth->refresh_editor = true;
}
void SurgeGUIEditor::showTuningMenu(VSTGUI::CPoint& where)
{
   CRect menuRect;
   menuRect.offset(where.x, where.y);
   auto m = makeTuningMenu(menuRect, true);

   frame->addView(m);
   m->setDirty();
   m->popup();
   frame->removeView(m, true);
}


void SurgeGUIEditor::tuningFileDropped(std::string fn)
{
   try
   {
      this->synth->storage.retuneToScale(Tunings::readSCLFile(fn));
      this->synth->refresh_editor = true;
   }
   catch( Tunings::TuningError &e )
   {
      Surge::UserInteractions::promptError( e.what(), "SCL Error" );
   }
}

void SurgeGUIEditor::mappingFileDropped(std::string fn)
{
   try {
      this->synth->storage.remapToKeyboard(Tunings::readKBMFile(fn));
      this->synth->refresh_editor = true;
   }
   catch( Tunings::TuningError &e )
   {
      Surge::UserInteractions::promptError( e.what(), "KBM Error" );
   }
}

bool SurgeGUIEditor::doesZoomFitToScreen(int zf, int &correctedZf)
{
   CRect screenDim = Surge::GUI::getScreenDimensions(getFrame());

   float baseW = getWindowSizeX();
   float baseH = getWindowSizeY();

   /*
   ** Window decoration takes up some of the screen so don't zoom to full screen dimensions.
   ** This heuristic seems to work on windows 10 and macos 10.14 weel enough.
   ** Keep these as integers to be consistent wiht the other zoom factors, and to make
   ** the error message cleaner.
   */
   int maxScreenUsage = 90;

   /*
   ** In the startup path we may not have a clean window yet to give us a trustworthy
   ** screen dimension; so allow callers to supress this check with an optional
   ** variable and set it only in the constructor of SurgeGUIEditor
   */
   if (zf != 100.0 && zf > 100 &&
       screenDim.getHeight() > 0 && screenDim.getWidth() > 0 && (
           (baseW * zf / 100.0) > maxScreenUsage * screenDim.getWidth() / 100.0 ||
           (baseH * zf / 100.0) > maxScreenUsage * screenDim.getHeight() / 100.0
           )
       )
   {
      correctedZf = findLargestFittingZoomBetween(100.0 , zf, 5, maxScreenUsage, baseW, baseH);
      return false;
   }
   else
   {
      correctedZf = zf;
      return true;
   }
}

void SurgeGUIEditor::setZoomFactor(int zf)
{
   zoomFactorRecursionGuard ++;

   if( zoomFactorRecursionGuard > 3 )
   {
      std::ostringstream oss;
      oss << "Surge has recursed into setZoomFactor too many times. This indicates an error in the interaction between "
          << "Surge, your host's zoom implementation, and your screen size. Please report this error to the "
          << "Surge Synth Team on GitHub, since we think it should never happen. But it seems it has!";
      // Choose to not show this error.  It only ever happens in Studio one. See issue 2397.
      // Surge::UserInteractions::promptError( oss.str(), "Surge Software Zoom Error" );
      zoomFactorRecursionGuard--;
      return;
   }

   if (!zoomEnabled)
      zf = 100.0;

   if (zf < minimumZoom)
   {
      std::ostringstream oss;
      oss << "The smallest zoom level possible on your platform is " << minimumZoom
          << "%. Sorry, you cannot make Surge any smaller!";
      Surge::UserInteractions::promptError(oss.str(), "Zoom Level Error");
      zf = minimumZoom;
   }


   CRect screenDim = Surge::GUI::getScreenDimensions(getFrame());

   /*
   ** If getScreenDimensions() can't determine a size on all paltforms it now
   ** returns a size 0 screen. In that case we will skip the min check but
   ** need to remember the zoom is invalid
   */
   if (screenDim.getWidth() == 0 || screenDim.getHeight() == 0)
   {
      zoomInvalid = true;
   }

   int newZf;
   if( doesZoomFitToScreen(zf, newZf) )
   {
      zoomFactor = zf;
   }
   else
   {
      zoomFactor = newZf;

      std::ostringstream msg;
      msg << "Surge adjusts the maximum zoom level in order to prevent the interface becoming larger than available screen area. "
          << "Your screen resolution is " << screenDim.getWidth() << "x" << screenDim.getHeight() << " "
          << "for which the target zoom level of " << zf << "% would be too large."
          << std::endl << std::endl;
      if( currentSkin && currentSkin->hasFixedZooms() )
      {
         msg << "Surge chose the largest fitting fixed zoom which is provided by this skin.";
      }
      else
      {
         msg << "Surge chose the largest fitting zoom level of " << zoomFactor << "%.";
      }
      Surge::UserInteractions::promptError(msg.str(),
                                           "Zoom Level Adjusted");
   }

   /*
    * Fixed zoom zoom factor constraint
    */
   if( currentSkin && currentSkin->hasFixedZooms() )
   {
      int nzf = 100;
      for( auto z : currentSkin->getFixedZooms() )
      {
         if( z <= zoomFactor )
            nzf = z;
      }
      zoomFactor = nzf;
   }

   zoom_callback(this);

   float dbs = Surge::GUI::getDisplayBackingScaleFactor(getFrame());
   int fullPhysicalZoomFactor = (int)(zf * dbs);
   if (bitmapStore != nullptr)
      bitmapStore->setPhysicalZoomFactor(fullPhysicalZoomFactor);

   zoomFactorRecursionGuard--;
}

void SurgeGUIEditor::showSettingsMenu(CRect &menuRect)
{
    COptionMenu* settingsMenu =
    new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);

    int eid = 0;

    auto mpeSubMenu = makeMpeMenu(menuRect, false);
    settingsMenu->addEntry(mpeSubMenu, Surge::UI::toOSCaseForMenu("MPE Options"));
    eid++;
    mpeSubMenu->forget();

    auto tuningSubMenu = makeTuningMenu(menuRect, false);
    settingsMenu->addEntry(tuningSubMenu, "Tuning");
    eid++;
    tuningSubMenu->forget();

    if (zoomEnabled)
    {
        auto zoomMenu = makeZoomMenu(menuRect, false);
        settingsMenu->addEntry(zoomMenu, "Zoom");
        eid++;
        zoomMenu->forget();
    }

    auto skinSubMenu = makeSkinMenu(menuRect);
    settingsMenu->addEntry(skinSubMenu, "Skins");
    eid++;
    skinSubMenu->forget();

    auto uiOptionsMenu = makeUserSettingsMenu(menuRect);
    settingsMenu->addEntry(uiOptionsMenu, Surge::UI::toOSCaseForMenu("User Settings"));
    uiOptionsMenu->forget();
    eid++;

    auto dataSubMenu = makeDataMenu(menuRect);
    settingsMenu->addEntry(dataSubMenu, Surge::UI::toOSCaseForMenu("Data Folders"));
    eid++;
    dataSubMenu->forget();

    auto midiSubMenu = makeMidiMenu(menuRect);
    settingsMenu->addEntry(midiSubMenu, Surge::UI::toOSCaseForMenu("MIDI Settings"));
    eid++;
    midiSubMenu->forget();

    if (useDevMenu)
    {
        auto devSubMenu = makeDevMenu(menuRect);
        settingsMenu->addEntry(devSubMenu, Surge::UI::toOSCaseForMenu("Developer Options"));
        eid++;
        devSubMenu->forget();
    }

    settingsMenu->addSeparator(eid++);

    addCallbackMenu(settingsMenu, Surge::UI::toOSCaseForMenu("Reach the Developers..."), []() {
            Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/feedback");
        });
    eid++;

    addCallbackMenu(settingsMenu, Surge::UI::toOSCaseForMenu("Read the Code..."), []() {
            Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge/");
        });
    eid++;

    addCallbackMenu(settingsMenu, Surge::UI::toOSCaseForMenu("Download Additional Content..."), []() {
            Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge-synthesizer.github.io/wiki/Additional-Content");
    });
    eid++;

    addCallbackMenu(settingsMenu, Surge::UI::toOSCaseForMenu("Surge Website..."), []() {
            Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/");
        });
    eid++;

    addCallbackMenu(settingsMenu, Surge::UI::toOSCaseForMenu("Surge Manual..."), []() {
       Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/manual/");
    });
    eid++;

    settingsMenu->addSeparator(eid++);

    addCallbackMenu(settingsMenu, "About Surge", [this]() {
       this->showAboutBox();
    });
    eid++;

    frame->addView(settingsMenu);
    settingsMenu->setDirty();
    settingsMenu->popup();
    frame->removeView(settingsMenu, true);
}

VSTGUI::COptionMenu *SurgeGUIEditor::makeLfoMenu(VSTGUI::CRect &menuRect)
{
   int currentLfoId = modsource_editor[current_scene] - ms_lfo1;

   int shapev = synth->storage.getPatch().scene[current_scene].lfo[currentLfoId].shape.val.i;
   std::string what = "LFO";
   if( lt_mseg == shapev )
      what = "MSEG";
   if( lt_stepseq == shapev)
      what = "Step Seq";
   if (lt_envelope == shapev)
      what = "Env";
   if (lt_function == shapev) 
      what = "Env"; //Change this later

   COptionMenu* lfoSubMenu =
       new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
   addCallbackMenu(lfoSubMenu, "[?] Modulator Presets", [](){} ); // FIXME - help needed
   lfoSubMenu->addSeparator();
   addCallbackMenu( lfoSubMenu,
                   Surge::UI::toOSCaseForMenu("Save " + what + " As..."),
                   [this, currentLfoId, what](){
                      // Prompt for a name
                      promptForMiniEdit("preset", "Enter " + what + " Preset Name", "Please Name Preset",
                                        CPoint( -1, -1 ),
                                        [this, currentLfoId](const std::string &s ) {
                                           Surge::ModulatorPreset::savePresetToUser(string_to_path(s),
                                                                                  &(this->synth->storage),
                                                                                  current_scene,
                                                                                  currentLfoId );
                                        });
                      // and save
                   }
                   );
   
   auto presetCategories = Surge::ModulatorPreset::getPresets(&(synth->storage));

   if (!presetCategories.empty())
      lfoSubMenu->addSeparator();

   for( auto const &cat : presetCategories )
   {
      COptionMenu *catSubMenu = new COptionMenu( menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
      for( auto const &p : cat.presets )
         addCallbackMenu(catSubMenu,
                         Surge::UI::toOSCaseForMenu(p.name),
                         [this, p, currentLfoId](){
                            Surge::ModulatorPreset::loadPresetFrom(p.path, &(this->synth->storage), current_scene, currentLfoId );
                            auto newshape = this->synth->storage.getPatch().scene[current_scene].lfo[currentLfoId].shape.val.i;

                            if( editorOverlay && editorOverlayTag == "msegEditor" )
                            {
                               closeMSEGEditor();
                               if( newshape == lt_mseg )
                                  showMSEGEditor();
                            }

                            this->synth->refresh_editor = true;
                         });
      lfoSubMenu->addEntry(catSubMenu, cat.name.c_str());
      catSubMenu->forget();
   }
   return lfoSubMenu;
}


VSTGUI::COptionMenu *SurgeGUIEditor::makeMpeMenu(VSTGUI::CRect &menuRect, bool showhelp)
{

    COptionMenu* mpeSubMenu =
    new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

    auto hu = helpURLForSpecial("mpe-menu");
    if (hu != "" && showhelp)
    {
        auto lurl = fullyResolvedHelpURL(hu);
        addCallbackMenu(mpeSubMenu, "[?] MPE",
                      [lurl]() { Surge::UserInteractions::openURL(lurl); });
        mpeSubMenu->addSeparator();
    }

    std::string endis = "Enable MPE";
    if (synth->mpeEnabled)
       endis = "Disable MPE";
    addCallbackMenu(mpeSubMenu, endis.c_str(),
                    [this]() { this->synth->mpeEnabled = !this->synth->mpeEnabled; });
    mpeSubMenu->addSeparator();

    std::ostringstream oss;
    oss << "Change MPE Pitch Bend Range (Current: " << synth->storage.mpePitchBendRange << " Semitones)";
    addCallbackMenu(mpeSubMenu, Surge::UI::toOSCaseForMenu(oss.str().c_str()), [this,menuRect]() {
       // FIXME! This won't work on linux
       const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
       promptForMiniEdit(c, "Enter new MPE pitch bend range:", "MPE Pitch Bend Range",
                         menuRect.getTopLeft(), [this](const std::string& c) {
                            int newVal = ::atoi(c.c_str());
                            this->synth->storage.mpePitchBendRange = newVal;
                         });
    });

    std::ostringstream oss2;
    int def = Surge::Storage::getUserDefaultValue( &(synth->storage), "mpePitchBendRange", 48 );
    oss2 << "Change Default MPE Pitch Bend Range (Current: " << def << " Semitones)";
    addCallbackMenu(mpeSubMenu, Surge::UI::toOSCaseForMenu(oss2.str().c_str()), [this, menuRect]() {
       // FIXME! This won't work on linux
       const auto c{std::to_string(int(synth->storage.mpePitchBendRange))};
       promptForMiniEdit(c, "Enter default MPE pitch bend range:", "Default MPE Pitch Bend Range",
                         menuRect.getTopLeft(), [this](const std::string& s) {
                            int newVal = ::atoi(s.c_str());
                            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                                   "mpePitchBendRange", newVal);
                            this->synth->storage.mpePitchBendRange = newVal;
                         });
    });

    mpeSubMenu->addEntry(makeSmoothMenu(menuRect, "pitchSmoothingMode",
                                        (int)ControllerModulationSource::SmoothingMode::DIRECT,
                                        [this](auto md) { this->resetPitchSmoothing(md); }),
                         Surge::UI::toOSCaseForMenu("MPE Pitch Bend Smoothing"));

    return mpeSubMenu;
}

VSTGUI::COptionMenu* SurgeGUIEditor::makeTuningMenu(VSTGUI::CRect& menuRect, bool showhelp)
{
    int tid=0;
    COptionMenu *tuningSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

    auto hu = helpURLForSpecial("tun-menu");
    if (hu != "" && showhelp)
    {
       auto lurl = fullyResolvedHelpURL(hu);
       addCallbackMenu(tuningSubMenu, "[?] Tuning",
                       [lurl]() { Surge::UserInteractions::openURL(lurl); });
       tid++;
       tuningSubMenu->addSeparator();
    }

    auto *st = addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Set to Standard Tuning"),
                    [this]()
                    {
                        this->synth->storage.init_tables();
                        tuningCacheForToggle = "";
                        this->synth->refresh_editor = true;
                    }
        );
    st->setEnabled(!this->synth->storage.isStandardTuning || tuningCacheForToggle.size() > 0);
    tid++;

    auto *kst = addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Set to Standard Keyboard Mapping"),
                    [this]()
                    {
                        this->synth->storage.remapToStandardKeyboard();
                        mappingCacheForToggle = "";
                        this->synth->refresh_editor = true;
                    }
        );
    kst->setEnabled(!this->synth->storage.isStandardMapping || mappingCacheForToggle.size() > 0);
    tid++;

    tuningSubMenu->addSeparator();
    tid++;

    addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Apply .scl Tuning..."),
                    [this]()
                    {
                        auto cb = [this](std::string sf)
                        {
                            std::string sfx = ".scl";
                            if( sf.length() >= sfx.length())
                            {
                                if( sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0 )
                                {
                                    Surge::UserInteractions::promptError( "Please select only .scl files!", "Invalid Choice" );
                                    std::cout << "FILE is [" << sf << "]" << std::endl;
                                    return;
                                }
                            }
                            try
                            {
                               auto sc = Tunings::readSCLFile(sf);

                               if (!this->synth->storage.retuneToScale(sc))
                               {
                                  Surge::UserInteractions::promptError( "This .scl file is not valid!", "File Format Error" );
                                  return;
                               }
                               this->synth->refresh_editor = true;
                            }
                            catch( Tunings::TuningError &e )
                            {
                               Surge::UserInteractions::promptError( e.what(), "Loading Error" );
                            }
                        };

                        auto scl_path = Surge::Storage::appendDirectory(this->synth->storage.datapath, "tuning-library", "SCL");

                        Surge::UserInteractions::promptFileOpenDialog(scl_path, ".scl", "Scala microtuning files (*.scl)", cb);
                    }
        );
    tid++;

    addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Apply .kbm Keyboard Mapping..."),
                    [this]()
                    {
                        auto cb = [this](std::string sf)
                        {
                            std::string sfx = ".kbm";
                            if( sf.length() >= sfx.length())
                            {
                                if( sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0 )
                                {
                                    Surge::UserInteractions::promptError( "Please select only .kbm files!", "Invalid Choice" );
                                    std::cout << "FILE is [" << sf << "]" << std::endl;
                                    return;
                                }
                            }
                            try
                            {
                               auto kb = Tunings::readKBMFile(sf);

                               if (!this->synth->storage.remapToKeyboard(kb) )
                               {
                                  Surge::UserInteractions::promptError( "This .kbm file is not valid!", "File Format Error" );
                                  return;
                               }

                               this->synth->refresh_editor = true;
                            }
                            catch( Tunings::TuningError &e )
                            {
                               Surge::UserInteractions::promptError( e.what(), "Loading Error" );
                            }

                        };

                        auto kbm_path = Surge::Storage::appendDirectory(this->synth->storage.datapath, "tuning-library", "KBM Concert Pitch");

                        Surge::UserInteractions::promptFileOpenDialog(kbm_path, ".kbm", "Scala keyboard mapping files (*.kbm)", cb);
                    }
        );
    tid++;

    int oct = 5 - Surge::Storage::getUserDefaultValue(&(this->synth->storage), "middleC", 1);
    string middle_A = "A" + to_string(oct);

    addCallbackMenu( tuningSubMenu, Surge::UI::toOSCaseForMenu("Remap " + middle_A + " (MIDI note 69) directly to..."),
                     [this, middle_A, menuRect]()
                        {
                           char ma[256];
                           sprintf(ma, "Remap %s Frequency", middle_A.c_str());

                           char c[256];
                           snprintf(c, 256, "440.0");
                           promptForMiniEdit(c, "Remap MIDI note 69 frequency to: ", ma,
                                             menuRect.getTopLeft(), [this](const std::string& s) {
                                                float freq = ::atof(s.c_str());
                                                auto kb = Tunings::tuneA69To(freq);
                                                if (!this->synth->storage.remapToKeyboard(kb))
                                                {
                                                   Surge::UserInteractions::promptError(
                                                       "This .kbm file is not valid!",
                                                       "File Format Error");
                                                   return;
                                                }
                                             });
                        }
       );

    tuningSubMenu->addSeparator();
    tid++;
    auto *sct = addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Show Current Tuning..."),
                    [this]()
                    {
                       Surge::UserInteractions::showHTML( this->tuningToHtml() );
                    }
        );
    sct->setEnabled(! this->synth->storage.isStandardTuning );

    addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Factory Tuning Library..."),
                    [this]()
                    {
                       auto dpath = Surge::Storage::appendDirectory(this->synth->storage.datapath, "tuning-library");

                       Surge::UserInteractions::openFolderInFileBrowser(dpath);
                    }
        );

    return tuningSubMenu;
}

VSTGUI::COptionMenu* SurgeGUIEditor::makeZoomMenu(VSTGUI::CRect& menuRect, bool showhelp)
{
    COptionMenu* zoomSubMenu =
        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

    int zid = 0;

    auto hu = helpURLForSpecial("zoom-menu");
    if (hu != "" && showhelp)
    {
       auto lurl = fullyResolvedHelpURL(hu);
       addCallbackMenu(zoomSubMenu, "[?] Zoom",
                       [lurl]() { Surge::UserInteractions::openURL(lurl); });
       zid++;

       zoomSubMenu->addSeparator(zid++);
    }

    std::vector<int> zoomTos = {{100, 125, 150, 175, 200, 300, 400}};
    bool isFixed = false;
    if( currentSkin->hasFixedZooms() )
    {
       isFixed = true;
       zoomTos = currentSkin->getFixedZooms();
    }

    for (auto s : zoomTos ) // These are somewhat arbitrary reasonable defaults
    {
        std::ostringstream lab;
        lab << "Zoom to " << s << "%";
        CCommandMenuItem* zcmd = new CCommandMenuItem(CCommandMenuItem::Desc(lab.str()));
        zcmd->setActions([this, s](CCommandMenuItem* m) { setZoomFactor(s); });
        zoomSubMenu->addEntry(zcmd);
        if( s == zoomFactor )
           zcmd->setChecked( true );
        zid++;
    }
    zoomSubMenu->addSeparator(zid++);

    if( isFixed )
    {
       /*
        * DO WE WANT SOMETHING LIKE THIS?
       addCallbackMenu( zoomSubMenu, Surge::UI::toOSCaseForMenu( "About fixed zoom skins..." ),
                        [](){});
                        */
    }
    else
    {
       for (auto jog : {-25, -10, 10, 25}) // These are somewhat arbitrary reasonable defaults also
       {
          std::ostringstream lab;
          if (jog > 0)
             lab << "Grow by " << jog << "%";
          else
             lab << "Shrink by " << -jog << "%";

          CCommandMenuItem* zcmd = new CCommandMenuItem(CCommandMenuItem::Desc(lab.str()));
          zcmd->setActions(
              [this, jog](CCommandMenuItem* m) { setZoomFactor(getZoomFactor() + jog); });
          zoomSubMenu->addEntry(zcmd);
          zid++;
       }

       zoomSubMenu->addSeparator(zid++);

       CCommandMenuItem* biggestZ = new CCommandMenuItem(
           CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Zoom to Largest")));
       biggestZ->setActions([this](CCommandMenuItem* m) {
          int newZF = findLargestFittingZoomBetween(100.0, 500.0, 5,
                                                    90, // See comment in setZoomFactor
                                                    getWindowSizeX(), getWindowSizeY());
          setZoomFactor(newZF);
       });
       zoomSubMenu->addEntry(biggestZ);
       zid++;

       CCommandMenuItem* smallestZ = new CCommandMenuItem(
           CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Zoom to Smallest")));
       smallestZ->setActions([this](CCommandMenuItem* m) { setZoomFactor(minimumZoom); });
       zoomSubMenu->addEntry(smallestZ);
       zid++;

       zoomSubMenu->addSeparator(zid++);

       auto dzf = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultZoom",
                                                      this->zoomFactor);
       std::ostringstream dss;
       dss << "Zoom to Default (" << dzf << "%)";
       CCommandMenuItem* todefaultZ = new CCommandMenuItem(
           CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu(dss.str().c_str())));
       todefaultZ->setActions([this, dzf](CCommandMenuItem* m) { this->setZoomFactor(dzf); });
       zoomSubMenu->addEntry(todefaultZ);
       zid++;
    }

    std::ostringstream zss;
    zss << "Set " << zoomFactor << "% as Default";
    CCommandMenuItem* defaultZ = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu(zss.str().c_str())));
    defaultZ->setActions([this](CCommandMenuItem* m) {
        Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultZoom",
                                            this->zoomFactor);
    });
    zoomSubMenu->addEntry(defaultZ);
    zid++;

    if( ! isFixed )
    {
       addCallbackMenu(zoomSubMenu, Surge::UI::toOSCaseForMenu("Set Default Zoom Level to..."),
                       [this, menuRect]() {
                          // FIXME! This won't work on linux
                          char c[256];
                          snprintf(c, 256, "%d", this->zoomFactor);
                          promptForMiniEdit(
                              c, "Enter a default zoom level value:", "Set Default Zoom Level",
                              menuRect.getTopLeft(), [this](const std::string& s) {
                                 int newVal = ::atoi(s.c_str());
                                 Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                                        "defaultZoom", newVal);
                                 this->setZoomFactor(newVal);
                              });
                       });
       zid++;
    }

    return zoomSubMenu;
}

VSTGUI::COptionMenu* SurgeGUIEditor::makeUserSettingsMenu(VSTGUI::CRect& menuRect)
{
   COptionMenu* uiOptionsMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                VSTGUI::COptionMenu::kNoDrawStyle |
                                                    VSTGUI::COptionMenu::kMultipleCheckStyle);

   // Mouse behavior submenu
   int mid = 0;
   COptionMenu* mouseSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                               VSTGUI::COptionMenu::kNoDrawStyle |
                                                   VSTGUI::COptionMenu::kMultipleCheckStyle);

   std::string mouseClassic = "Classic";
   std::string mouseSlow = "Slow";
   std::string mouseMedium = "Medium";
   std::string mouseExact = "Exact";

   VSTGUI::CCommandMenuItem* menuItem = nullptr;

   menuItem = addCallbackMenu(mouseSubMenu, mouseClassic.c_str(), [this]() {
      CSurgeSlider::sliderMoveRateState = CSurgeSlider::kClassic;
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                             CSurgeSlider::sliderMoveRateState);
   });
   menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kClassic));
   mid++;

   menuItem = addCallbackMenu(mouseSubMenu, mouseSlow.c_str(), [this]() {
      CSurgeSlider::sliderMoveRateState = CSurgeSlider::kSlow;
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                             CSurgeSlider::sliderMoveRateState);
   });
   menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kSlow));
   mid++;

   menuItem = addCallbackMenu(mouseSubMenu, mouseMedium.c_str(), [this]() {
      CSurgeSlider::sliderMoveRateState = CSurgeSlider::kMedium;
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                             CSurgeSlider::sliderMoveRateState);
   });
   menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kMedium));
   mid++;

   menuItem = addCallbackMenu(mouseSubMenu, mouseExact.c_str(), [this]() {
      CSurgeSlider::sliderMoveRateState = CSurgeSlider::kExact;
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                             CSurgeSlider::sliderMoveRateState);
   });
   menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kExact));
   mid++;

   mouseSubMenu->addSeparator(mid++);

   auto tsMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "showCursorWhileEditing", 0);

   menuItem = addCallbackMenu(mouseSubMenu, Surge::UI::toOSCaseForMenu("Show Cursor While Editing"), [this, tsMode]() {
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "showCursorWhileEditing", 1 - tsMode);
   });
   menuItem->setChecked(tsMode);

   std::string mouseMenuName = Surge::UI::toOSCaseForMenu("Mouse Behavior");

   uiOptionsMenu->addEntry(mouseSubMenu, mouseMenuName.c_str());
   mouseSubMenu->forget();

   // Middle C submenu
   COptionMenu* middleCSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                               VSTGUI::COptionMenu::kNoDrawStyle |
                                                   VSTGUI::COptionMenu::kMultipleCheckStyle);

   VSTGUI::CCommandMenuItem* mcItem = nullptr;

   auto mcValue = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "middleC", 1);

   mcItem = addCallbackMenu(middleCSubMenu, "C3", [this]() {
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "middleC", 2);
      synth->refresh_editor = true;
   });
   mcItem->setChecked(mcValue == 2);

   mcItem = addCallbackMenu(middleCSubMenu, "C4", [this]() {
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "middleC", 1);
      synth->refresh_editor = true;
   });
   mcItem->setChecked(mcValue == 1);

   mcItem = addCallbackMenu(middleCSubMenu, "C5", [this]() {
      Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "middleC", 0);
      synth->refresh_editor = true;
   });
   mcItem->setChecked(mcValue == 0);

   uiOptionsMenu->addEntry(middleCSubMenu, "Middle C");
   middleCSubMenu->forget();

   // patch defaults
   COptionMenu* patchDefMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                     VSTGUI::COptionMenu::kMultipleCheckStyle);

   VSTGUI::CCommandMenuItem* pdItem = nullptr;

   pdItem = addCallbackMenu(patchDefMenu, Surge::UI::toOSCaseForMenu("Set Default Patch Author..."), [this, menuRect]() {
            string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchAuthor", "");
            char txt[256];
            txt[0] = 0;
            if( Surge::Storage::isValidUTF8( s ) )
               strncpy(txt, s.c_str(), 256);
            promptForMiniEdit(txt, "Enter default patch author name:", "Set Default Patch Author",
                              menuRect.getTopLeft(), [this](const std::string& s) {
                                 Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                                        "defaultPatchAuthor", s);
                              });
            });

   pdItem = addCallbackMenu(patchDefMenu, Surge::UI::toOSCaseForMenu("Set Default Patch Comment..."), [this, menuRect]() {
            string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchComment", "");
            char txt[256];
            txt[0] = 0;
            if( Surge::Storage::isValidUTF8( s ) )
               strncpy(txt, s.c_str(), 256);
            promptForMiniEdit(txt, "Enter default patch comment text:", "Set Default Patch Comment",
                              menuRect.getTopLeft(), [this](const std::string& s) {
                                 Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                                        "defaultPatchComment", s);
                              });
            });

   uiOptionsMenu->addEntry(patchDefMenu, Surge::UI::toOSCaseForMenu("Patch Defaults"));
   patchDefMenu->forget();

   // high precision value readouts
   auto precReadout = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);

   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("High Precision Value Readouts"),
       [this, precReadout]()
       {
          Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", precReadout ? 0 : 1);
       });
   menuItem->setChecked(precReadout);

   // modulation value readout shows bounds
   auto modValues = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "modWindowShowsValues", 0);

   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("Modulation Value Readout Shows Bounds"),
       [this, modValues]()
       {
          Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "modWindowShowsValues", modValues ? 0 : 1);
       });
   menuItem->setChecked(modValues);

   // remember tab positions per scene
   auto tabPosMem = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "rememberTabPositionsPerScene", 0);

   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("Remember Tab Positions Per Scene"),
       [this, tabPosMem]()
       {
          Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "rememberTabPositionsPerScene", tabPosMem ? 0 : 1);
       });
   menuItem->setChecked(tabPosMem);

   // activate individual scene outputs
   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("Activate Individual Scene Outputs"),
       [this]()
       {
          this->synth->activateExtraOutputs = ! this->synth->activateExtraOutputs;
          Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "activateExtraOutputs", this->synth->activateExtraOutputs ? 1 : 0 );
       }
      );
   menuItem->setChecked(synth->activateExtraOutputs);

   return uiOptionsMenu;
}

VSTGUI::COptionMenu *SurgeGUIEditor::makeSkinMenu(VSTGUI::CRect &menuRect)
{
    int tid=0;
    COptionMenu *skinSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

    auto &db = Surge::UI::SkinDB::get();
    bool hasTests = false;

    // TODO: Later allow nesting
    std::map<std::string, std::vector<Surge::UI::SkinDB::Entry>> entryByCategory;
    for( auto &entry : db.getAvailableSkins() )
    {
       entryByCategory[entry.category].push_back( entry );
    }

    for( auto pr : entryByCategory )
    {
       auto addToThis = skinSubMenu;
       auto cat = pr.first;
       if( cat != "" )
       {
          addToThis = new COptionMenu( menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle );
       }
       for (auto& entry : pr.second)
       {
          auto dname = entry.displayName;
          if (useDevMenu)
          {
             dname += " (";
             if (entry.root.find(synth->storage.datapath) != std::string::npos)
             {
                dname += "factory";
             }
             else if (entry.root.find(synth->storage.userDataPath) != std::string::npos)
             {
                dname += "user";
             }
             else
             {
                dname += "other";
             }

             dname += PATH_SEPARATOR + entry.name + ")";
          }

          auto cb = addCallbackMenu(addToThis, dname, [this, entry]() {
             setupSkinFromEntry(entry);
             this->synth->refresh_editor = true;
             Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultSkin",
                                                    entry.name);
          });
          cb->setChecked(entry.matchesSkin(currentSkin));
          tid++;
       }
       if( cat != "" )
       {
          skinSubMenu->addEntry( addToThis, cat.c_str());
          addToThis->forget();
       }
    }

    skinSubMenu->addSeparator();

    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Reload Current Skin"),
                    [this]() {
                       this->bitmapStore.reset(new SurgeBitmaps());
                       this->bitmapStore->setupBitmapsForFrame(frame);
                       if( ! this->currentSkin->reloadSkin(this->bitmapStore) )
                       {
                          auto &db = Surge::UI::SkinDB::get();
                          auto msg = std::string( "Unable to load skin! Reverting the skin to Surge Classic.\n\nSkin error:\n" )
                             + db.getAndResetErrorString();
                          this->currentSkin = db.defaultSkin( &( this->synth->storage ) );
                          this->currentSkin->reloadSkin(this->bitmapStore);
                          Surge::UserInteractions::promptError( msg, "Skin Loading Error" );
                       }

                       reloadFromSkin();
                       this->synth->refresh_editor = true;
                    } );
    tid++;
    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Rescan Skins"),
                    [this]() {
                       auto r = this->currentSkin->root;
                       auto n = this->currentSkin->name;

                       auto &db = Surge::UI::SkinDB::get();
                       db.rescanForSkins( &(this->synth->storage) );

                       // So go find the skin
                       auto e = db.getEntryByRootAndName( r, n );
                       if( e.isJust() )
                       {
                          setupSkinFromEntry(e.fromJust());
                       }
                       else
                       {
                          setupSkinFromEntry(db.getDefaultSkinEntry());
                       }
                       this->synth->refresh_editor = true;
                    }
       );
    tid++;
    skinSubMenu->addSeparator(tid++);

    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Open Current Skin Folder..."),
                    [this]() {
                       Surge::UserInteractions::openFolderInFileBrowser(this->currentSkin->root +
                                                                        this->currentSkin->name);
                    });
    tid++;

    skinSubMenu->addSeparator();

    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Show Skin Inspector..."),
                   [this]()
                   {
                     Surge::UserInteractions::showHTML( skinInspectorHtml() );
                   });

    tid++;
    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Skin Development Guide..."),
                   []() {
                      Surge::UserInteractions::openURL( "https://surge-synthesizer.github.io/skin-manual.html" );
                   });

    tid++;

    return skinSubMenu;
}

VSTGUI::COptionMenu* SurgeGUIEditor::makeDataMenu(VSTGUI::CRect& menuRect)
{
   int did = 0;
   COptionMenu* dataSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                              VSTGUI::COptionMenu::kNoDrawStyle |
                                                  VSTGUI::COptionMenu::kMultipleCheckStyle);

   addCallbackMenu(
       dataSubMenu, Surge::UI::toOSCaseForMenu("Open Factory Data Folder..."), [this]() {
          Surge::UserInteractions::openFolderInFileBrowser(this->synth->storage.datapath);
       });
   did++;

   addCallbackMenu(dataSubMenu, Surge::UI::toOSCaseForMenu("Open User Data Folder..."), [this]() {
      // make it if it isn't there
      fs::create_directories(string_to_path(this->synth->storage.userDataPath));
      Surge::UserInteractions::openFolderInFileBrowser(this->synth->storage.userDataPath);
   });
   did++;

   addCallbackMenu(
       dataSubMenu, Surge::UI::toOSCaseForMenu("Set Custom User Data Folder..."), [this]() {
          auto cb = [this](std::string f) {
             // FIXME - check if f is a path
             this->synth->storage.userDataPath = f;
             Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "userDataPath", f);
             this->synth->storage.refresh_wtlist();
             this->synth->storage.refresh_patchlist();
          };
          Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.userDataPath, "", "",
                                                        cb, true, true);
       });
   did++;

   dataSubMenu->addSeparator(did++);


   addCallbackMenu(dataSubMenu, Surge::UI::toOSCaseForMenu("Rescan All Data Folders"), [this]() {
      this->synth->storage.refresh_wtlist();
      this->synth->storage.refresh_patchlist();
      this->scannedForMidiPresets = false;
      CFxMenu::scanForUserPresets = true; // that's annoying now I see it side by side. But you know.

      // Rescan for skins
      auto r = this->currentSkin->root;
      auto n = this->currentSkin->name;

      auto &db = Surge::UI::SkinDB::get();
      db.rescanForSkins( &(this->synth->storage) );

      // So go find the skin
      auto e = db.getEntryByRootAndName( r, n );
      if( e.isJust() )
      {
         setupSkinFromEntry(e.fromJust());
      }
      else
      {
         setupSkinFromEntry(db.getDefaultSkinEntry());
      }

      // Will need to rebuild the FX menu also so...
      this->synth->refresh_editor = true;
   });

   return dataSubMenu;
}

// builds a menu for setting controller smoothing, used in ::makeMidiMenu and ::makeMpeMenu
// key is the key given to getUserDefaultValue,
// default is a value to default to,
// setSmooth is a function called to set the smoothing value
VSTGUI::COptionMenu* SurgeGUIEditor::makeSmoothMenu(
    VSTGUI::CRect& menuRect,
    const std::string& key,
    int defaultValue,
    std::function<void(ControllerModulationSource::SmoothingMode)> setSmooth)
{
   COptionMenu* smoothMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                             VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

   int smoothing = Surge::Storage::getUserDefaultValue(&(synth->storage), key, defaultValue);

   auto asmt = [this, smoothMenu, smoothing,
                setSmooth](const char* label, ControllerModulationSource::SmoothingMode md) {
      auto me = addCallbackMenu(smoothMenu, label, [setSmooth, md]() { setSmooth(md); });
      me->setChecked(smoothing == md);
   };
   asmt("Legacy", ControllerModulationSource::SmoothingMode::LEGACY);
   asmt("Slow Exponential", ControllerModulationSource::SmoothingMode::SLOW_EXP);
   asmt("Fast Exponential", ControllerModulationSource::SmoothingMode::FAST_EXP);
   asmt("Fast Linear", ControllerModulationSource::SmoothingMode::FAST_LINE);
   asmt("No Smoothing", ControllerModulationSource::SmoothingMode::DIRECT);
   return smoothMenu;
};

VSTGUI::COptionMenu* SurgeGUIEditor::makeMidiMenu(VSTGUI::CRect& menuRect)
{
   COptionMenu* midiSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                              VSTGUI::COptionMenu::kNoDrawStyle |
                                              VSTGUI::COptionMenu::kMultipleCheckStyle);

   midiSubMenu->addEntry(makeSmoothMenu(menuRect, "smoothingMode",
                                        (int)ControllerModulationSource::SmoothingMode::LEGACY,
                                        [this](auto md) { this->resetSmoothing(md); }),
                         Surge::UI::toOSCaseForMenu("Controller Smoothing"));

   midiSubMenu->addSeparator();

   addCallbackMenu(
       midiSubMenu, Surge::UI::toOSCaseForMenu("Save MIDI Mapping As..."),
       [this,menuRect]() {
          this->scannedForMidiPresets = false; // force a rescan
          char msn[256];
          msn[0] = 0;
          promptForMiniEdit(
              msn, "MIDI Mapping Name", "Save MIDI Mapping", menuRect.getTopLeft(),
              [this](const std::string& s) { this->synth->storage.storeMidiMappingToName(s); });
       });

   addCallbackMenu(
      midiSubMenu, Surge::UI::toOSCaseForMenu("Show Current MIDI Mapping..."),
      [this]() {
            Surge::UserInteractions::showHTML( this->midiMappingToHtml() );
         }
      );

   addCallbackMenu(
      midiSubMenu, Surge::UI::toOSCaseForMenu("Clear Current MIDI Mapping"),
      [this]() {
          int n = n_global_params + n_scene_params;

          for (int i = 0; i < n; i++)
          {
             this->synth->storage.getPatch().param_ptr[i]->midictrl = -1;
             if (i > n_global_params)
                this->synth->storage.getPatch().param_ptr[i + n_scene_params]->midictrl = -1;
          }
      }
      );

   if( ! scannedForMidiPresets )
   {
      scannedForMidiPresets = true;
      synth->storage.rescanUserMidiMappings();
   }

   bool gotOne = false;
   for( const auto & p : synth->storage.userMidiMappingsXMLByName )
   {
      if( ! gotOne )
      {
         gotOne = true;
         midiSubMenu->addSeparator();
      }
      addCallbackMenu( midiSubMenu, p.first,
                       [this, p] {
                          this->synth->storage.loadMidiMappingByName(p.first);
                       }
         );
   }


   return midiSubMenu;
}

void SurgeGUIEditor::reloadFromSkin()
{
   if( ! frame || ! bitmapStore.get() )
      return;

   float dbs = Surge::GUI::getDisplayBackingScaleFactor(getFrame());
   bitmapStore->setPhysicalZoomFactor(getZoomFactor() * dbs);

   auto bg = currentSkin->customBackgroundImage();

   if( bg != "" )
   {
      CScalableBitmap *cbm = bitmapStore->getBitmapByStringID( bg );
      if( cbm )
      {
         cbm->setExtraScaleFactor(getZoomFactor());
         frame->setBackground(cbm);
      }
   }
   else
   {
      CScalableBitmap *cbm = bitmapStore->getBitmap( IDB_BG );
      cbm->setExtraScaleFactor(getZoomFactor());
      frame->setBackground( cbm );
   }

   auto c = currentSkin->getColor(Colors::Dialog::Entry::Focus);
   frame->setFocusColor( c );

   wsx = currentSkin->getWindowSizeX();
   wsy = currentSkin->getWindowSizeY();

   float sf = 1;
#if TARGET_VST2
   sf = getZoomFactor() / 100.0;
#endif
   frame->setSize( wsx * sf, wsy * sf );

#if TARGET_VST3
   sf = getZoomFactor() / 100.0;
#endif

   rect.right = wsx * sf;
   rect.bottom = wsy * sf;


   setZoomFactor( getZoomFactor() );

   // update MSEG editor if opened
   if (editorOverlay && editorOverlayTag == "msegEditor")
   {
      showMSEGEditor();
   }

   // update Store Patch dialog if opened
   if (editorOverlay && editorOverlayTag == "storePatch")
   {
      auto pname = patchName->getText();
      auto pcat = patchCategory->getText();
      auto pauth = patchCreator->getText();
      auto pcom = patchComment->getText();

      showStorePatchDialog();

      patchName->setText(pname);
      patchCategory->setText(pcat);
      patchCreator->setText(pauth);
      patchComment->setText(pcom);
   }
}

VSTGUI::COptionMenu *SurgeGUIEditor::makeDevMenu(VSTGUI::CRect &menuRect)
{
    int tid = 0;

    COptionMenu *devSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

#if WINDOWS
    VSTGUI::CCommandMenuItem* conItem = nullptr;

    static bool consoleState;

    conItem = addCallbackMenu(devSubMenu, Surge::UI::toOSCaseForMenu("Show Debug Console..."),
        []() {
                consoleState = Surge::Debug::toggleConsole();
             });
        conItem->setChecked(consoleState);
    tid++;
#endif

#ifdef INSTRUMENT_UI
    addCallbackMenu(devSubMenu, Surge::UI::toOSCaseForMenu("Show UI Instrumentation..."),
                    []() {
                       Surge::Debug::report();
                    }
       );
    tid++;
#endif

    return devSubMenu;
}


int SurgeGUIEditor::findLargestFittingZoomBetween(int zoomLow, // bottom of range
                                                  int zoomHigh, // top of range
                                                  int zoomQuanta, // step size
                                                  int percentageOfScreenAvailable, // How much to shrink actual screen
                                                  float baseW,
                                                  float baseH
    )
{
    // Here is a very crude implementation
    int result = zoomHigh;
    CRect screenDim = Surge::GUI::getScreenDimensions(getFrame());
    float sx = screenDim.getWidth() * percentageOfScreenAvailable / 100.0;
    float sy = screenDim.getHeight() * percentageOfScreenAvailable / 100.0;

    while(result > zoomLow)
    {
        if(result * baseW / 100.0 <= sx &&
            result * baseH / 100.0 <= sy)
            break;
        result -= zoomQuanta;
    }
    if(result < zoomLow)
        result = zoomLow;

    return result;
}

VSTGUI::CCommandMenuItem* SurgeGUIEditor::addCallbackMenu(VSTGUI::COptionMenu* toThis,
                                                          std::string label,
                                                          std::function<void()> op)
{
   CCommandMenuItem* menu = new CCommandMenuItem(CCommandMenuItem::Desc(label.c_str()));
   menu->setActions([op](CCommandMenuItem* m) { op(); });
   toThis->addEntry(menu);
   return menu;
}

#if TARGET_VST3
Steinberg::tresult PLUGIN_API SurgeGUIEditor::onSize(Steinberg::ViewRect* newSize)
{
   float izfx = newSize->getWidth() * 1.0 / getWindowSizeX() * 100.0;
   float izfy = newSize->getHeight() * 1.0 / getWindowSizeY() * 100.0;
   float izf = std::min(izfx, izfy);
   izf = std::max(izf, 1.0f*minimumZoom);

   int fzf = (int)izf;
   // Don't allow me to set a zoom which will pop a dialog from this drag event. See #1212
   if( ! doesZoomFitToScreen((int)izf, fzf) )
   {
      izf = fzf;
   }

   /*
   ** If the implied zoom factor changes my size by more than a pixel, resize
   */
   auto zfdx = std::fabs( (izf - zoomFactor) * getWindowSizeX() ) / 100.0;
   auto zfdy = std::fabs( (izf - zoomFactor) * getWindowSizeY() ) / 100.0;
   auto zfd = std::max( zfdx, zfdy );

   if( zfd > 1 )
   {
      setZoomFactor( izf );
   }

   return Steinberg::Vst::VSTGUIEditor::onSize(newSize);
}
Steinberg::tresult PLUGIN_API SurgeGUIEditor::checkSizeConstraint(Steinberg::ViewRect* newSize)
{
   // float tratio = 1.0 * getWindowSizeX() / getWindowSizeY();
   // float cratio = 1.0 * newSize->getWidth() / newSize->getHeight();

   // we want cratio == tration by adjusting height so
   // WSX / WSY = gW / gH
   // gH = gW * WSY / WSX

   float newHeight = 1.0 * newSize->getWidth() * getWindowSizeY() / getWindowSizeX();
   newSize->bottom = newSize->top + std::ceil(newHeight);
   return Steinberg::kResultTrue;
}

#endif

void SurgeGUIEditor::forceautomationchangefor(Parameter *p)
{
#if TARGET_LV2
   // revisit this for non-LV2 outside 1.6.6
   synth->sendParameterAutomation(synth->idForParameter(p), synth->getParameter01(synth->idForParameter(p)));
#endif
}
//------------------------------------------------------------------------------------------------


void SurgeGUIEditor::promptForUserValueEntry( Parameter *p, CControl *c, int ms )
{
   if( typeinDialog )
   {
      typeinDialog->setVisible(false);
      removeFromFrame.push_back(typeinDialog);
      typeinDialog = nullptr;
      typeinResetCounter = -1;
   }

   if( p )
   {
      typeinMode = Param;
   }
   else
   {
      typeinMode = Control;
   }


   bool ismod = p && ms > 0;
   int boxht = 56;
   auto cp = c->getViewSize();

   if( ismod )
      boxht += 22;

   CRect typeinSize( cp.left, cp.top - boxht, cp.left + 120, cp.top - boxht + boxht  );

   if( cp.top - boxht < 0 )
   {
      typeinSize = CRect( cp.left, cp.top + c->getHeight(),
                          cp.left + 120, cp.top + c->getHeight() + boxht );
   }

   typeinModSource = ms;
   typeinEditControl = c;
   typeinResetCounter = -1;

   typeinDialog = new CViewContainer(typeinSize);
   typeinDialog->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
   typeinDialog->setVisible(false);
   frame->addView(typeinDialog);

   CRect innerSize( 0, 0, typeinSize.getWidth(), typeinSize.getHeight() );
   innerSize.inset( 1, 1 );
   auto inner = new CViewContainer(innerSize);
   CColor bggr = currentSkin->getColor(Colors::Dialog::Background);
   inner->setBackgroundColor( bggr );
   typeinDialog->addView(inner);

   std::string lab = "";
   if( p )
   {
      if( p->ctrlgroup == cg_LFO )
      {
         char pname[1024];
         p->create_fullname( p->get_name(), pname, p->ctrlgroup, p->ctrlgroup_entry, modulatorName(p->ctrlgroup_entry, true ).c_str() );
         lab = pname;
      }
      else
      {
         lab = p->get_full_name();
      }
   }
   else
   {
      lab = modulatorName( ms, false );
   }

   typeinLabel = new CTextLabel( CRect( 2, 2, 114, 14 ), lab.c_str() );
   typeinLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
   typeinLabel->setTransparency(true);
   typeinLabel->setFont( displayFont );
   inner->addView(typeinLabel);

   char txt[256];
   char ptext[1024], ptext2[1024];
   ptext2[0] = 0;
   if( p )
   {
      if( ismod )
      {
         char txt2[256];
         p->get_display_of_modulation_depth(txt, synth->getModDepth(p->id, (modsources)ms), synth->isBipolarModulation((modsources)ms), Parameter::TypeIn);
         p->get_display(txt2);
         sprintf( ptext, "mod: %s", txt );
         sprintf( ptext2, "current: %s", txt2 );
      }
      else
      {
         p->get_display(txt);
         sprintf( ptext, "current: %s", txt );
      }
   }
   else
   {
      int detailedMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);
      auto cms = ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms]);
      sprintf( txt, "%.*f %%", (detailedMode ? 6 : 2 ), 100.0 * cms->get_output() );
      sprintf( ptext, "current: %s", txt );
   }

   if( ismod )
   {
      std::string mls = std::string( "by " ) + (char*)modulatorName(ms, true).c_str();
      auto ml = new CTextLabel( CRect( 2, 10, 114, 27 ), mls.c_str() );
      ml->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
      ml->setTransparency(true);
      ml->setFont( displayFont );
      inner->addView(ml);
   }


   typeinPriorValueLabel = new CTextLabel(CRect(2, 29 - (ismod ? 0 : 23), 116, 36 + ismod), ptext);
   typeinPriorValueLabel->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
   typeinPriorValueLabel->setTransparency(true);
   typeinPriorValueLabel->setFont( displayFont );
   inner->addView(typeinPriorValueLabel);

   if( ismod )
   {
      auto sl = new CTextLabel( CRect( 2, 29 + 11, 116, 36 + 11 ), ptext2 );
      sl->setFontColor(currentSkin->getColor(Colors::Slider::Label::Dark));
      sl->setTransparency(true);
      sl->setFont( displayFont );
      inner->addView( sl );
   }


   typeinValue = new CTextEdit( CRect( 4, 31 + ( ismod ? 22 : 0 ), 114, 50 + ( ismod ? 22 : 0 ) ), this, tag_value_typein, txt );
   typeinValue->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
   typeinValue->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));

   // fix the text selection rectangle background overhanging the borders on Windows
#if WINDOWS
   typeinValue->setTextInset(CPoint(3, 0));
#endif

   if( p )
   {
      if( ! p->can_setvalue_from_string() )
      {
         typeinValue->setFontColor( VSTGUI::kRedCColor );
         typeinValue->setText( "Edit coming soon!" );
      }
   }

   inner->addView(typeinValue);
   typeinEditTarget = p;
   typeinDialog->setVisible(true);
   typeinValue->takeFocus();
}

std::string SurgeGUIEditor::modulatorName( int i, bool button )
{
   if( ( i >= ms_lfo1 && i <= ms_slfo6 ) )
   {
      int idx = i-ms_lfo1;
      bool isS = idx >= 6;
      int fnum = idx % 6;
      auto *lfodata = &( synth->storage.getPatch().scene[current_scene].lfo[ i - ms_lfo1 ] );

      if( lfodata->shape.val.i == lt_envelope )
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sENV %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s Envelope %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
      else if( lfodata->shape.val.i == lt_stepseq )
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sSEQ %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s Step Sequencer %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
      else if( lfodata->shape.val.i == lt_mseg )
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sMSEG %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s MSEG %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
      else if( lfodata->shape.val.i == lt_function)
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sENV %d", (isS ? "S-" : "" ), fnum + 1 );
            //sprintf( txt, "%sFUN %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s Envelope %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
            //sprintf( txt, "%s Function %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
   }

   if( i >= ms_ctrl1 && i <= ms_ctrl8 )
   {
      std::string ccl = std::string(synth->storage.getPatch().CustomControllerLabel[i - ms_ctrl1]);
      if( ccl == "-" )
      {
         return std::string( modsource_names[i] );
      }
      else
      {
         return ccl + " (" + modsource_names[i] + ")";
      }
   }
   if( button )
      return std::string( modsource_names_button[i] );
   else
      return std::string( modsource_names[i] );

}

#if TARGET_VST3
Steinberg::Vst::IContextMenu* SurgeGUIEditor::addVst3MenuForParams(VSTGUI::COptionMenu *contextMenu,
                                                                   const SurgeSynthesizer::ID &pid, int &eid)
{
   CRect menuRect;
   Steinberg::Vst::IComponentHandler* componentHandler =
      getController()->getComponentHandler();
   Steinberg::FUnknownPtr<Steinberg::Vst::IComponentHandler3> componentHandler3(
      componentHandler);
   Steinberg::Vst::IContextMenu* hostMenu = nullptr;
   if (componentHandler3)
   {
      std::stack<COptionMenu *> menuStack;
      menuStack.push(contextMenu);
      std::stack<int> eidStack;
      eidStack.push(eid);

      Steinberg::Vst::ParamID param = pid.getDawSideId();
      hostMenu = componentHandler3->createContextMenu(this, &param);

      int N = hostMenu ?  hostMenu->getItemCount() : 0;
      if( N > 0 )
      {
         contextMenu->addSeparator(); eid++;
      }

      std::deque<COptionMenu*> parentMenus;
      for (int i = 0; i < N; i++)
      {
         Steinberg::Vst::IContextMenu::Item item = {0};
         Steinberg::Vst::IContextMenuTarget *target = {0};

         hostMenu->getItem(i, item, &target );

         //char nm[1024];
         //Steinberg::UString128(item.name, 128).toAscii(nm, 1024);
#if WINDOWS
         // https://stackoverflow.com/questions/32055357/visual-studio-c-2015-stdcodecvt-with-char16-t-or-char32-t
         std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>,wchar_t> conversion;
         std::string nm = conversion.to_bytes( (wchar_t*)( item.name ) );
#else
         std::wstring_convert<std::codecvt_utf8_utf16<char16_t>,char16_t> conversion;
         std::string nm = conversion.to_bytes( (char16_t*)( item.name ) );
#endif

         if( nm[0] == '-' ) // FL sends us this as a separator with no VST indication so just strip the '-'
         {
            int pos = 1;
            while( nm[pos] == ' ' && nm[pos] != 0 )
               pos++;
            nm = nm.substr(pos);
         }

         auto itag = item.tag;
         /*
         ** Leave this here so we can debug if another vst3 problem comes up
         std::cout << nm << " FL=" << item.flags << " jGS=" << Steinberg::Vst::IContextMenuItem::kIsGroupStart
         << " and=" << ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart )
         << " IGS="
         << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart ) == Steinberg::Vst::IContextMenuItem::kIsGroupStart ) << " IGE="
         << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) == Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) << " "
         << std::endl;
         */
         if( item.flags & Steinberg::Vst::IContextMenuItem::kIsSeparator &&
             ! item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart )
         {
            menuStack.top()->addSeparator(eidStack.top()++);
         }
         else if( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart ) == Steinberg::Vst::IContextMenuItem::kIsGroupStart )
         {
            COptionMenu *subMenu = new COptionMenu( menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle |
                                                    VSTGUI::COptionMenu::kMultipleCheckStyle );
            menuStack.top()->addEntry(subMenu, nm.c_str() );
            menuStack.push(subMenu);
            subMenu->forget();
            eidStack.push(0);

            /*
              VSTGUI doesn't seem to allow a disabled or checked grouping menu.
              if( item.flags & Steinberg::Vst::IContextMenuItem::kIsDisabled )
              {
              subMenu->setEnabled(false);
              }
              if( item.flags & Steinberg::Vst::IContextMenuItem::kIsChecked )
              {
              subMenu->setChecked(true);
              }
            */

         }
         else
         {
            RememberForgetGuard<Steinberg::Vst::IContextMenuTarget> tg(target);
            RememberForgetGuard<Steinberg::Vst::IContextMenu> hm(hostMenu);

            auto menu = addCallbackMenu(menuStack.top(), nm, [this, hm, tg, itag]() {
                                                                tg.t->executeMenuItem(itag);
                                                             });
            eidStack.top()++;
            if( item.flags & Steinberg::Vst::IContextMenuItem::kIsDisabled )
            {
               menu->setEnabled(false);
            }
            if( item.flags & Steinberg::Vst::IContextMenuItem::kIsChecked )
            {
               menu->setChecked(true);
            }

            if( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) == Steinberg::Vst::IContextMenuItem::kIsGroupEnd )
            {
               menuStack.pop();
               eidStack.pop();
            }
         }
         // hostMenu->addItem(item, &target);
      }
      eid = eidStack.top();

   }
   return hostMenu;
}
#endif

std::string SurgeGUIEditor::helpURLFor( Parameter *p )
{
   auto storage = &(synth->storage);
#if 0 // useful debug
   static bool once = false;
   if( ! once )
   {
      once = true;
      for( auto hp : storage->helpURL_paramidentifier )
      {
         auto k = hp.first;
         bool found = false;
         for (auto iter = synth->storage.getPatch().param_ptr.begin();
              iter != synth->storage.getPatch().param_ptr.end(); iter++)
         {
            Parameter* q = *iter;
            if( ! q ) break;
            if( q->ui_identifier == k )
            {
               found = true;
               break;
            }
         }
         if( ! found )
            std::cout << "UNFOUND : " << k << std::endl;
      }
   }
#endif
   std::string id = p->ui_identifier;
   int type = -1;
   if( p->ctrlgroup == cg_OSC )
   {
      type = storage->getPatch().scene[current_scene].osc[current_osc[current_scene]].type.val.i;
   }
   if( p->ctrlgroup == cg_FX )
   {
      type = storage->getPatch().fx[current_fx].type.val.i;
   }
   if( type >= 0 )
   {
      auto key = std::make_pair( id, type );
      if( storage->helpURL_paramidentifier_typespecialized.find(key) != storage->helpURL_paramidentifier_typespecialized.end() )
      {
         auto r = storage->helpURL_paramidentifier_typespecialized[key];
         if( r != "" )
            return r;
      }
   }
   if( storage->helpURL_paramidentifier.find(id) != storage->helpURL_paramidentifier.end() )
   {
      auto r = storage->helpURL_paramidentifier[id];
      if( r != "" )
         return r;
   }
   if( storage->helpURL_controlgroup.find(p->ctrlgroup) != storage->helpURL_controlgroup.end() )
   {
      auto r = storage->helpURL_controlgroup[p->ctrlgroup];
      if( r != "" )
         return r;
   }
   return "";
}

std::string SurgeGUIEditor::helpURLForSpecial( std::string key )
{
   auto storage = &(synth->storage);
   if( storage->helpURL_specials.find(key) != storage->helpURL_specials.end() )
   {
      auto r = storage->helpURL_specials[key];
      if( r != "" )
         return r;
   }
   return "";
}

std::string SurgeGUIEditor::fullyResolvedHelpURL( std::string helpurl )
{
   std::string lurl = helpurl;
   if( helpurl[0] == '#' )
   {
      lurl = "https://surge-synthesizer.github.io/manual/" + helpurl;
   }
   return lurl;
}

void SurgeGUIEditor::setupSkinFromEntry( const Surge::UI::SkinDB::Entry &entry )
{
   auto &db = Surge::UI::SkinDB::get();
   auto s = db.getSkin(entry);
   this->currentSkin = s;
   this->bitmapStore.reset(new SurgeBitmaps());
   this->bitmapStore->setupBitmapsForFrame(frame);
   if( ! this->currentSkin->reloadSkin(this->bitmapStore) )
   {
      std::ostringstream oss;
      oss << "Unable to load " << entry.root << entry.name << " skin! Reverting the skin to Surge Classic.\n\nSkin Error:\n"
          << db.getAndResetErrorString();

      auto msg = std::string( oss.str() );
      this->currentSkin = db.defaultSkin( &( this->synth->storage ) );
      this->currentSkin->reloadSkin(this->bitmapStore);
      Surge::UserInteractions::promptError( msg, "Skin Loading Error" );
   }
   reloadFromSkin();
}

void SurgeGUIEditor::sliderHoverStart( int tag )
{
   int ptag =  tag - start_paramtags;
   for (int k = 1; k < n_modsources; k++)
   {
      modsources ms = (modsources)k;
      if (synth->isActiveModulation(ptag, ms))
      {
         auto gms = dynamic_cast<CModulationSourceButton  *>(gui_modsrc[k]);
         if( gms )
         {
            gms->setSecondaryHover(true);
         }

      }
   };
}
void SurgeGUIEditor::sliderHoverEnd( int tag )
{
   for (int k = 1; k < n_modsources; k++)
   {
      auto gms = dynamic_cast<CModulationSourceButton *>(gui_modsrc[k]);
      if( gms )
      {
         gms->setSecondaryHover(false);
      }
   }

}

void SurgeGUIEditor::dismissEditorOverlay()
{
   if( editorOverlay != nullptr )
   {
      editorOverlay->setVisible(false);
      editorOverlayOnClose();
      removeFromFrame.push_back( editorOverlay );
      editorOverlay = nullptr;
      editorOverlayTag = "";
      editorOverlayTagAtClose = "";
   }
}

void SurgeGUIEditor::setEditorOverlay(VSTGUI::CView *c, std::string editorTitle, std::string editorTag,
                                      const VSTGUI::CPoint &topLeft, bool modalOverlay, bool hasCloseButton, std::function<void ()> onClose)
{
   dismissEditorOverlay();

   const int header = 18;
   const int buttonwidth = 40;
   
   if (!c)
      return;

   auto vs = c->getViewSize();
   
   // add a solid outline thing which is bigger than the control with the title to that
   auto containerSize = vs;
   containerSize.top -= header;

   if (!modalOverlay)
      containerSize.offset(-containerSize.left + topLeft.x, -containerSize.top + topLeft.y);
   
   auto fs = CRect(0, 0, getWindowSizeX(), getWindowSizeY());

   if (!modalOverlay)
      fs = containerSize;

   // add a screen size transparent thing into the editorOverlay
   editorOverlay = new CViewContainer(fs);
   editorOverlay->setBackgroundColor(currentSkin->getColor(Colors::Overlay::Background));
   editorOverlay->setVisible(true);
   frame->addView(editorOverlay);

   if (modalOverlay)
      containerSize = containerSize.centerInside(fs);
   else
      containerSize.moveTo(CPoint(0, 0));

   VSTGUI::SharedPointer<VSTGUI::CFontDesc> headerFont = new VSTGUI::CFontDesc("Lato", 9, kBoldFace);
   VSTGUI::SharedPointer<VSTGUI::CFontDesc> btnFont = new VSTGUI::CFontDesc("Lato", 9);
   
   auto outerc = new CViewContainer(containerSize);
   outerc->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
   editorOverlay->addView(outerc);

   auto csz = containerSize;
   csz.bottom = csz.top + header;
   auto innerc = new CViewContainer(csz);
   innerc->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
   editorOverlay->addView(innerc);

   auto tl = new CTextLabel(csz, editorTitle.c_str());
   tl->setBackColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
   tl->setFrameColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
   tl->setFontColor(currentSkin->getColor(Colors::Dialog::Titlebar::Text));
   tl->setFont(headerFont);
   innerc->addView(tl);

   auto iconrect = CRect(CPoint(3, 2), CPoint(15, 15));
   auto icon = new CViewContainer(iconrect);
   icon->setBackground(bitmapStore->getBitmap(IDB_MINIEDIT_ICON));
   icon->setVisible(true);
   innerc->addView(icon);

   auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
   auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
   auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

   auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
   auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
   auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

   VSTGUI::CGradient::ColorStopMap csm;
   VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);
   cg->addColorStop(0, btnbg);

   VSTGUI::CGradient::ColorStopMap hovcsm;
   VSTGUI::CGradient* hovcg = VSTGUI::CGradient::create(hovcsm);
   hovcg->addColorStop(0, hovbtnbg);

   if (hasCloseButton)
   {
      csz.left = csz.right - buttonwidth;
      csz.inset(3, 3);
      csz.bottom++;
      auto b = new CTextButton(csz, this, tag_editor_overlay_close, "Close");
      b->setVisible(true);
      b->setFont(btnFont);
      b->setGradient(cg);
      b->setFrameColor(btnborder);
      b->setTextColor(btntext);
      b->setGradientHighlighted(hovcg);
      b->setFrameColorHighlighted(hovbtnborder);
      b->setTextColorHighlighted(hovbtntext);
      b->setRoundRadius(CCoord(3.f));
      innerc->addView(b);
   }
 
   // add the control inside that in an outline
   if (modalOverlay)
   {
      containerSize = vs;
      containerSize.top -= header;
      containerSize = containerSize.centerInside(fs);
      containerSize.top += header;
      containerSize.inset(3, 3);
   }
   else
   {
      containerSize = vs.moveTo(0, header).inset(2, 0);
      containerSize.bottom -= 2;
   }

   c->setViewSize(containerSize);
   c->setMouseableArea(containerSize); // sigh
   editorOverlay->addView(c);

   // save the onClose function
   editorOverlayOnClose = onClose;
   editorOverlayTag = editorTag;
   editorOverlayTagAtClose = editorTag;
}

std::string SurgeGUIEditor::getDisplayForTag( long tag )
{
   if( tag < start_paramtags )
      return "Non-param tag";
   
   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()) )
   {
      Parameter* p = synth->storage.getPatch().param_ptr[ptag];
      if( p )
      {
         char txt[1024];
         p->get_display( txt );
         return txt;
      }
   }

   return "Unknown";
}

void SurgeGUIEditor::promptForMiniEdit(const std::string& value,
                                       const std::string& prompt,
                                       const std::string& title,
                                       const VSTGUI::CPoint& iwhere,
                                       std::function<void(const std::string&)> onOK)
{
   auto fs = CRect( 0, 0, getWindowSizeX(), getWindowSizeY() );
   minieditOverlay = new CViewContainer( fs );
   minieditOverlay->setBackgroundColor(currentSkin->getColor(Colors::Overlay::Background));
   minieditOverlay->setVisible(true);
   frame->addView( minieditOverlay );

   auto where = iwhere;
   if( where.x < 0 || where.y < 0 )
   {
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);
   }

   int wd = 160;
   int ht = 80;

   // We want to center the text on where. The 0.4 just 'feels better' than 0.5
   where = where.offset( -wd * 0.4, -ht * 0.5 );

   auto rr = CRect( CPoint( where.x - fs.left, where.y - fs.top ), CPoint( wd, ht ) );

   if( rr.top < 0 )
      rr = rr.offset( 0, 10 - rr.top );
   if( rr.bottom > fs.getHeight() )

      rr.offset( 0, - rr.bottom + fs.getHeight() - 10 );
   if( rr.left < 0 )
      rr = rr.offset( 10 - rr.left, 0 );
   if( rr.right > fs.getWidth() )

      rr.offset( - rr.right + fs.getWidth() - 10, 0 );

   VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnt = new VSTGUI::CFontDesc("Lato", 11);
   VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnts = new VSTGUI::CFontDesc("Lato", 9);
   VSTGUI::SharedPointer<VSTGUI::CFontDesc> fntt = new VSTGUI::CFontDesc("Lato", 9, kBoldFace);

   auto window = new CViewContainer(rr);
   window->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Border));
   window->setVisible(true);
   minieditOverlay->addView(window);
   
   auto titlebar = new CViewContainer(CRect(0, 0, wd, 18));
   titlebar->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Titlebar::Background));
   titlebar->setVisible(true);
   window->addView(titlebar);

   auto titlerect = CRect(CPoint(0, 0), CPoint(wd, 16));
   auto titletxt = new CTextLabel(titlerect, title.c_str());
   titletxt->setTransparency(true);
   titletxt->setFontColor(currentSkin->getColor(Colors::Dialog::Titlebar::Text));
   titletxt->setFont(fntt);
   titlebar->addView(titletxt);
   
   auto iconrect = CRect(CPoint(3, 2), CPoint(15, 15));
   auto icon = new CViewContainer(iconrect);
   icon->setBackground(bitmapStore->getBitmap(IDB_MINIEDIT_ICON));
   icon->setVisible(true);
   titlebar->addView(icon);

   auto bgrect = CRect(CPoint(1, 18), CPoint(wd - 2, ht - 19));
   auto bg = new CViewContainer(bgrect);
   bg->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Background));
   bg->setVisible(true);
   window->addView(bg);

   auto msgrect = CRect(CPoint(0, 2), CPoint(wd, 14));
   msgrect.inset(5, 0);
   auto msgtxt = new CTextLabel(msgrect, prompt.c_str());
   msgtxt->setTransparency(true);
   msgtxt->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   msgtxt->setFont(fnts);
   msgtxt->setHoriAlign(kLeftText);
   bg->addView(msgtxt);

   auto mer = CRect(CPoint(0, 18), CPoint(wd - 2, 22));
   mer.inset(4, 2);
   minieditTypein = new CTextEdit(mer, this, tag_miniedit_ok, value.c_str());
   minieditTypein->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
   minieditTypein->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
   minieditTypein->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
   minieditTypein->setFont(fnt);
#if WINDOWS
   minieditTypein->setTextInset(CPoint(3, 0));
#endif
   bg->addView(minieditTypein);
   minieditTypein->takeFocus();

   minieditOverlayDone = onOK;

   int bw = 44;
   auto b1r = CRect(CPoint(wd - (bw * 2) - 9, bgrect.bottom - 36), CPoint(bw, 13));
   auto b2r = CRect(CPoint(wd - bw - 6, bgrect.bottom - 36), CPoint(bw, 13));

   auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
   auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
   auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

   auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
   auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
   auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

   VSTGUI::CGradient::ColorStopMap csm;
   VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);
   cg->addColorStop(0, btnbg);

   VSTGUI::CGradient::ColorStopMap hovcsm;
   VSTGUI::CGradient* hovcg = VSTGUI::CGradient::create(hovcsm);
   hovcg->addColorStop(0, hovbtnbg);

   auto cb = new CTextButton(b1r, this, tag_miniedit_cancel, "Cancel");
   cb->setVisible(true);
   cb->setFont(fnts);
   cb->setGradient(cg);
   cb->setFrameColor(btnborder);
   cb->setTextColor(btntext);
   cb->setGradientHighlighted(hovcg);
   cb->setFrameColorHighlighted(hovbtnborder);
   cb->setTextColorHighlighted(hovbtntext);
   cb->setRoundRadius(CCoord(3.f));
   bg->addView(cb);

   auto kb = new CTextButton(b2r, this, tag_miniedit_ok, "OK");
   kb->setVisible(true);
   kb->setFont(fnts);
   kb->setGradient(cg);
   kb->setFrameColor(btnborder);
   kb->setTextColor(btntext);
   kb->setGradientHighlighted(cg);
   kb->setFrameColorHighlighted(btnborder);
   kb->setTextColorHighlighted(btntext);
   kb->setRoundRadius(CCoord(3.f));
   bg->addView(kb);
}

void SurgeGUIEditor::swapControllers( int t1, int t2 )
{
   synth->swapMetaControllers( t1 - tag_mod_source0 - ms_ctrl1,
                               t2 - tag_mod_source0 - ms_ctrl1 );
}

void SurgeGUIEditor::openModTypeinOnDrop( int modt, CControl *sl, int slidertag )
{
   auto p =  synth->storage.getPatch().param_ptr[slidertag-start_paramtags];
   int ms = modt - tag_mod_source0;

   if( synth->isValidModulation( p->id, (modsources)ms ) )
       promptForUserValueEntry( p, sl, ms );
}

void SurgeGUIEditor::resetSmoothing( ControllerModulationSource::SmoothingMode t )
{
   // Reset the default value and tell the synth it is updated
   Surge::Storage::updateUserDefaultValue(&(synth->storage), "smoothingMode", (int)t );
   synth->changeModulatorSmoothing( t );
}

void SurgeGUIEditor::resetPitchSmoothing(ControllerModulationSource::SmoothingMode t)
{
   // Reset the default value and update it in storage for newly created voices to use
   Surge::Storage::updateUserDefaultValue(&(synth->storage), "pitchSmoothingMode", (int)t);
   synth->storage.pitchSmoothingMode = t;
}

void SurgeGUIEditor::makeStorePatchDialog()
{
   CRect dialogSize(CPoint(0, 0), CPoint(390, 143));

   auto saveDialog = new CViewContainer(dialogSize);
   saveDialog->setBackgroundColor(currentSkin->getColor(Colors::Dialog::Background));

   auto btnbg = currentSkin->getColor(Colors::Dialog::Button::Background);
   auto btnborder = currentSkin->getColor(Colors::Dialog::Button::Border);
   auto btntext = currentSkin->getColor(Colors::Dialog::Button::Text);

   auto hovbtnbg = currentSkin->getColor(Colors::Dialog::Button::BackgroundHover);
   auto hovbtnborder = currentSkin->getColor(Colors::Dialog::Button::BorderHover);
   auto hovbtntext = currentSkin->getColor(Colors::Dialog::Button::TextHover);

   VSTGUI::CGradient::ColorStopMap csm;
   VSTGUI::CGradient* cg = VSTGUI::CGradient::create(csm);
   cg->addColorStop(0, btnbg);

   VSTGUI::CGradient::ColorStopMap hovcsm;
   VSTGUI::CGradient* hovcg = VSTGUI::CGradient::create(hovcsm);
   hovcg->addColorStop(0, hovbtnbg);

   VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnt = new VSTGUI::CFontDesc("Lato", 11);

   auto label = CRect(CPoint(10, 10), CPoint(47, 19));
   auto pnamelbl = new CTextLabel(label, "Name");
   pnamelbl->setTransparency(true);
   pnamelbl->setFont(fnt);
   pnamelbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   pnamelbl->setHoriAlign(kRightText);

   label.offset(0, 24);
   auto pcatlbl = new CTextLabel(label, "Category");
   pcatlbl->setTransparency(true);
   pcatlbl->setFont(fnt);
   pcatlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   pcatlbl->setHoriAlign(kRightText);

   label.offset(0, 24);
   auto pauthlbl = new CTextLabel(label, "Author");
   pauthlbl->setTransparency(true);
   pauthlbl->setFont(fnt);
   pauthlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   pauthlbl->setHoriAlign(kRightText);

   label.offset(0, 24);
   auto pcomlbl = new CTextLabel(label, "Comment");
   pcomlbl->setTransparency(true);
   pcomlbl->setFont(fnt);
   pcomlbl->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   pcomlbl->setHoriAlign(kRightText);

   patchName = new CTextEdit(CRect(CPoint(67, 10), CPoint(309, 19)), this, tag_store_name);
   patchCategory = new CTextEdit(CRect(CPoint(67, 34), CPoint(309, 19)), this, tag_store_category);
   patchCreator = new CTextEdit(CRect(CPoint(67, 58), CPoint(309, 19)), this, tag_store_creator);
   patchComment = new CTextEdit(CRect(CPoint(67, 82), CPoint(309, 19)), this, tag_store_comments);
   patchTuning = new CCheckBox(CRect(CPoint(67, 111), CPoint(200, 20)), this, tag_store_tuning, "Save With Tuning");
   patchTuning->setFont(fnt);
   patchTuning->setFontColor(currentSkin->getColor(Colors::Dialog::Label::Text));
   patchTuning->setBoxFrameColor(currentSkin->getColor(Colors::Dialog::Checkbox::Border));
   patchTuning->setBoxFillColor(currentSkin->getColor(Colors::Dialog::Checkbox::Background));
   patchTuning->setCheckMarkColor(currentSkin->getColor(Colors::Dialog::Checkbox::Tick));
   patchTuning->sizeToFit();
   patchTuning->setValue(0);
   patchTuning->setMouseEnabled(true);
   patchTuning->setVisible(!synth->storage.isStandardTuning);

   // fix the text selection rectangle background overhanging the borders on Windows
#if WINDOWS
   patchName->setTextInset(CPoint(3, 0));
   patchCategory->setTextInset(CPoint(3, 0));
   patchCreator->setTextInset(CPoint(3, 0));
   patchComment->setTextInset(CPoint(3, 0));
#endif

   /*
    * There is, apparently, a bug in VSTGui that focus events don't fire reliably on some mac hosts.
    * This leads to the odd behaviour when you click out of a box that in some hosts - Logic Pro for
    * instance - there is no looseFocus event and so the value doesn't update. We could fix that
    * a variety of ways I imagine, but since we don't really mind the value being updated as we
    * go, we can just set the editors to immediate and correct the problem.
    *
    * See GitHub Issue #231 for an explanation of the behaviour without these changes as of Jan 2019.
    */
   patchName->setImmediateTextChange(true);
   patchCategory->setImmediateTextChange(true);
   patchCreator->setImmediateTextChange(true);
   patchComment->setImmediateTextChange(true);

   patchName->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
   patchCategory->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
   patchCreator->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));
   patchComment->setBackColor(currentSkin->getColor(Colors::Dialog::Entry::Background));

   patchName->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
   patchCategory->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
   patchCreator->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));
   patchComment->setFontColor(currentSkin->getColor(Colors::Dialog::Entry::Text));

   patchName->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
   patchCategory->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
   patchCreator->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));
   patchComment->setFrameColor(currentSkin->getColor(Colors::Dialog::Entry::Border));

   auto b1r = CRect(CPoint(266, 111), CPoint(50, 20));
   auto cb = new CTextButton(b1r, this, tag_store_cancel, "Cancel");
   cb->setFont(aboutFont);
   cb->setGradient(cg);
   cb->setFrameColor(btnborder);
   cb->setTextColor(btntext);
   cb->setGradientHighlighted(hovcg);
   cb->setFrameColorHighlighted(hovbtnborder);
   cb->setTextColorHighlighted(hovbtntext);
   cb->setRoundRadius(CCoord(3.f));

   auto b2r = CRect(CPoint(326, 111), CPoint(50, 20));
   auto kb = new CTextButton(b2r, this, tag_store_ok, "OK");
   kb->setFont(aboutFont);
   kb->setGradient(cg);
   kb->setFrameColor(btnborder);
   kb->setTextColor(btntext);
   kb->setGradientHighlighted(cg);
   kb->setFrameColorHighlighted(btnborder);
   kb->setTextColorHighlighted(btntext);
   kb->setRoundRadius(CCoord(3.f));

   saveDialog->addView(pnamelbl);
   saveDialog->addView(pcatlbl);
   saveDialog->addView(pauthlbl);
   saveDialog->addView(pcomlbl);
   saveDialog->addView(patchName);
   saveDialog->addView(patchCategory);
   saveDialog->addView(patchCreator);
   saveDialog->addView(patchComment);
   saveDialog->addView(patchTuning);
   saveDialog->addView(patchTuningLabel);
   saveDialog->addView(cb);
   saveDialog->addView(kb);

   setEditorOverlay(saveDialog, "Store Patch", "storePatch", CPoint(157, 57), false, false, [this]() {});
}

VSTGUI::CControl *SurgeGUIEditor::layoutComponentForSkin( std::shared_ptr<Surge::UI::Skin::Control> skinCtrl,
                                            long tag,
                                            int paramIndex,
                                            Parameter *p,
                                            int style)
{
   // Special cases to preserve things
   if( p && p->ctrltype == ct_fmconfig )
   {
      fmconfig_tag = tag;
   }
   if( p && p->ctrltype == ct_fbconfig )
   {
      filterblock_tag = tag;
   }
   if( p && p->ctrltype == ct_fxbypass )
   {
      fxbypass_tag = tag;
   }
   // Basically put this in a function
   if (skinCtrl->ultimateparentclassname == "CSurgeSlider")
   {
      if (!p)
      {
         // FIXME ERROR
         return nullptr;
      }

      CPoint loc(skinCtrl->x, skinCtrl->y + p->posy_offset * yofs );

      // This is still a bit of a special case
      if( p->ctrltype == ct_airwindow_fx ||
          p->ctrltype == ct_flangermode ||
          p->ctrltype == ct_flangerwave ||
          p->ctrltype == ct_distortion_waveshape )
      {
         loc.offset( 2, 2 );
         auto hs = new CMenuAsSlider(loc,
                                     this,
                                     p->id + start_paramtags,
                                     bitmapStore,
                                     &(synth->storage));
         hs->setSkin( currentSkin, bitmapStore );
         hs->setValue( p->get_value_f01() );
         hs->setMinMax( p->val_min.i, p->val_max.i );
         hs->setLabel(p->get_name());
         p->ctrlstyle = p->ctrlstyle | kNoPopup;
         frame->addView( hs );
         if( p->can_deactivate() )
            hs->setDeactivated( p->deactivated );

         param[paramIndex] = hs;
         return hs;
      }
      bool is_mod = p && synth->isValidModulation(p->id, modsource);

      auto hs = new CSurgeSlider(loc,
                                 style, this, tag, is_mod,
                                 bitmapStore, &(synth->storage));
      hs->setSkin(currentSkin, bitmapStore, skinCtrl);
      hs->setMoveRate(p->moverate);

      if (p->can_temposync())
         hs->setTempoSync(p->temposync);
      hs->setValue(p->get_value_f01());
      hs->setLabel(p->get_name());
      hs->setModPresent(synth->isModDestUsed(p->id));
      hs->setDefaultValue(p->get_default_value_f01());

      if (p->can_deactivate())
         hs->deactivated = p->deactivated;
      else
         hs->deactivated = false;

      auto ff = currentSkin->propertyValue(skinCtrl, "font-family", "" );
      if( ff.size() > 0 )
      {
         hs->setFont(new CFontDesc(ff.c_str(), 9 ));
      }

      if (p->valtype == vt_int || p->valtype == vt_bool)
      {
         hs->isStepped = true;
         hs->intRange = p->val_max.i - p->val_min.i;
      }
      else
      {
         hs->isStepped = false;
      }

      setDisabledForParameter(p, hs);

      if (synth->isValidModulation(p->id, modsource))
      {
         hs->setModMode(mod_editor ? 1 : 0);
         hs->setModValue(synth->getModulation(p->id, modsource));
         hs->setModCurrent(synth->isActiveModulation(p->id, modsource),
                           synth->isBipolarModulation(modsource));
      }
      else
      {
         // Even if current modsource isn't modulating me, something else may be
      }
      frame->addView(hs);
      if( paramIndex >= 0 ) param[paramIndex] = hs;
      return hs;
   }
   if (skinCtrl->ultimateparentclassname == "CHSwitch2")
   {
      CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
      rect.offset(skinCtrl->x, skinCtrl->y);

      // Make this a function on skin
      auto bmp = currentSkin->backgroundBitmapForControl(skinCtrl, bitmapStore);

      if (bmp)
      {
         // Special case that scene select parameter is "odd"
         if( p && p->ctrltype == ct_scenesel )
            tag = tag_scene_select;

         auto frames = currentSkin->propertyValue(skinCtrl, "frames", "1");
         auto rows = currentSkin->propertyValue(skinCtrl, "rows", "1");
         auto cols = currentSkin->propertyValue(skinCtrl, "columns", "1");
         auto frameoffset = currentSkin->propertyValue(skinCtrl, "frame_offset", "0" );
         auto drgb = currentSkin->propertyValue( skinCtrl, "draggable", "1" );
         auto hsw = new CHSwitch2(rect, this, tag, std::atoi(frames.c_str()), skinCtrl->h,
                                  std::atoi(rows.c_str()), std::atoi(cols.c_str()), bmp, CPoint(0,0),
                                  std::atoi( drgb.c_str() ) );
         if( p )
         {
            auto fval = p->get_value_f01();

            if( p->ctrltype == ct_scenemode )
            {
               /*
               ** SceneMode is special now because we have a streaming vs UI difference.
               ** The streamed integer value is 0, 1, 2, 3 which matches the scene_mode
               ** SurgeStorage enum. But our display would look gross in that order, so
               ** our display order is single, split, channel split, dual which is 0, 1, 3, 2.
               ** Fine. So just deal with that here.
               */
               auto guiscenemode = p->val.i;
               if (guiscenemode == 3)
                  guiscenemode = 2;
               else if (guiscenemode == 2)
                  guiscenemode = 3;
               fval = 0.005 + 0.99 * ((float)(guiscenemode) / (n_scenemodes - 1));
            }
            hsw->setValue( fval );
         }
         hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
         hsw->setMouseableArea(rect);
         hsw->frameOffset = std::atoi(frameoffset.c_str());

         frame->addView(hsw);
         if( paramIndex >= 0 ) nonmod_param[paramIndex] = hsw;

         return hsw;
      }
   }
   if( skinCtrl->ultimateparentclassname == "CSwitchControl" )
   {
      CRect rect(CPoint( skinCtrl->x, skinCtrl->y), CPoint( skinCtrl->w, skinCtrl->h ) );
      auto bmp = currentSkin->backgroundBitmapForControl(skinCtrl, bitmapStore);
      if (bmp)
      {
         CSwitchControl* hsw =
             new CSwitchControl(rect, this, tag, bmp);
         hsw->setSkin(currentSkin, bitmapStore, skinCtrl);
         hsw->setMouseableArea(rect);
         frame->addView(hsw);
         if( paramIndex >= 0 ) nonmod_param[paramIndex] = hsw;
         if( p )
         {
            hsw->setValue(p->get_value_f01());

            // Carry over this filter type special case from the default control path
            if (p->ctrltype == ct_filtersubtype)
            {
               hsw->is_itype = true;
               hsw->imax = 3;
               hsw->ivalue = p->val.i + 1;
               if (fut_subcount[synth->storage.getPatch()
                                    .scene[current_scene]
                                    .filterunit[p->ctrlgroup_entry]
                                    .type.val.i] == 0)
                  hsw->ivalue = 0;

               if (p->ctrlgroup_entry == 1)
               {
                  f2subtypetag = p->id + start_paramtags;
                  filtersubtype[1] = hsw;
               }
               else
               {
                  f1subtypetag = p->id + start_paramtags;
                  filtersubtype[0] = hsw;
               }
            }
         }
         return hsw;
      }
   }
   if( skinCtrl->ultimateparentclassname == "CLFOGui" )
   {
      if( ! p )
         return nullptr;
      if (p->ctrltype != ct_lfotype)
      {
         // FIXME - warning?
      }
      CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
      rect.offset(skinCtrl->x, skinCtrl->y);
      int lfo_id = p->ctrlgroup_entry - ms_lfo1;
      if ((lfo_id >= 0) && (lfo_id < n_lfos))
      {
         CLFOGui* slfo = new CLFOGui(
             rect, lfo_id >= 0 && lfo_id <= (ms_lfo6 - ms_lfo1), this, p->id + start_paramtags,
             &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
             &synth->storage.getPatch().stepsequences[current_scene][lfo_id],
             &synth->storage.getPatch().msegs[current_scene][lfo_id],
             &synth->storage.getPatch().formulamods[current_scene][lfo_id], bitmapStore);
         slfo->setSkin(currentSkin, bitmapStore, skinCtrl);
         lfodisplay = slfo;
         frame->addView(slfo);
         nonmod_param[paramIndex] = slfo;
         return slfo;
      }
   }
   if( skinCtrl->ultimateparentclassname == "COSCMenu" )
   {
      CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
      rect.offset(skinCtrl->x, skinCtrl->y);
      auto hsw = new COscMenu(
          rect, this, tag_osc_menu, &synth->storage,
          &synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]], bitmapStore);
      hsw->setSkin(currentSkin,bitmapStore);
      hsw->setMouseableArea( rect );
      if( p ) hsw->setValue(p->get_value_f01());
      // TODO: This was not on before skinnification. Why?
      // if( paramIndex >= 0 ) nonmod_param[paramIndex] = hsw;

      frame->addView(hsw);
      return hsw;
   }
   if( skinCtrl->ultimateparentclassname == "CFXMenu" )
   {
      CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
      rect.offset(skinCtrl->x, skinCtrl->y);
      // CControl *m = new
      // CFxMenu(rect,this,tag_fx_menu,&synth->storage,&synth->storage.getPatch().fx[current_fx],current_fx);
      CControl* m = new CFxMenu(rect, this, tag_fx_menu, &synth->storage,
                                &synth->storage.getPatch().fx[current_fx],
                                &synth->fxsync[current_fx], current_fx);
      m->setMouseableArea(rect);
      ((CFxMenu*)m)->setSkin(currentSkin,bitmapStore);
      ((CFxMenu*)m)->selectedIdx = this->selectedFX[current_fx];
      fxPresetLabel->setText( this->fxPresetName[current_fx].c_str() );
      m->setValue(p->get_value_f01());
      frame->addView(m);
      fxmenu = m;
      return m;
   }
   if( skinCtrl->ultimateparentclassname == "CNumberField" )
   {
      CRect rect(0, 0, skinCtrl->w, skinCtrl->h);
      rect.offset(skinCtrl->x, skinCtrl->y);
      CNumberField* pbd = new CNumberField(rect, this, tag, nullptr, &(synth->storage));
      pbd->setSkin(currentSkin,bitmapStore, skinCtrl);
      pbd->setMouseableArea(rect);

      // TODO extra from properties
      auto nfcm = currentSkin->propertyValue(skinCtrl, "numberfield_controlmode", std::to_string( cm_none ));
      pbd->setControlMode(std::atoi(nfcm.c_str()));

      pbd->setValue(p->get_value_f01());
      frame->addView(pbd);
      nonmod_param[paramIndex] = pbd;

      if( p && p->ctrltype == ct_midikey_or_channel )
      {
         auto sm = this->synth->storage.getPatch().scenemode.val.i;

         switch (sm)
         {
         case sm_single:
         case sm_dual:
            pbd->setControlMode(cm_none);
            break;
         case sm_split:
            pbd->setControlMode(cm_notename);
            break;
         case sm_chsplit:
            pbd->setControlMode(cm_midichannel_from_127);
            break;
         }
      }

      // Save some of these for later reference
      switch( p->ctrltype )
      {
      case ct_polylimit:
         polydisp = pbd;
         break;
      case ct_midikey_or_channel:
         splitpointControl = pbd;
         break;
      default:
         break;
      }
      return pbd;
   }
   if( skinCtrl->ultimateparentclassname == "FilterSelector" )
   {
      // Obviously exposing this widget as a controllable widget would be better
      if( ! p ) return nullptr;

      auto rect = skinCtrl->getRect();
      auto hsw = new CMenuAsSlider(rect.getTopLeft(), CPoint(124, 21), this,
                                   p->id + start_paramtags, bitmapStore, &(synth->storage));
      hsw->setMinMax(0, n_fu_type - 1);
      hsw->setMouseableArea(rect);
      hsw->setLabel(p->get_name());
      hsw->setDeactivated(false);
      hsw->setBackgroundID(IDB_MENU_IN_FILTER_BG);
      hsw->setFilterMode(true);
      p->ctrlstyle = p->ctrlstyle | kNoPopup;

      auto drr = rect;
      drr.right = drr.left + 18;
      hsw->setDragRegion(drr);
      hsw->setDragGlyph(IDB_FILTER_GLYPHS, 18);

      hsw->setValue(p->get_value_f01());
      hsw->setSkin(currentSkin, bitmapStore);

      frame->addView(hsw);
      nonmod_param[paramIndex] = hsw;
      return hsw;
   }
   if( skinCtrl->ultimateparentclassname != Surge::UI::NoneClassName )
      std::cout << "Unable to make control with upc " << skinCtrl->ultimateparentclassname << std::endl;
   return nullptr;
}

bool SurgeGUIEditor::canDropTarget(const std::string& fname)
{
   static std::unordered_set<std::string> extensions;
   if( extensions.empty() )
   {
      extensions.insert( ".scl" );
      extensions.insert( ".kbm" );
      extensions.insert( ".wav" );
      extensions.insert( ".wt" );
      extensions.insert( ".fxp" );
   }

   fs::path fPath(fname);
   std::string fExt(path_to_string(fPath.extension()));
   std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                  [](unsigned char c){ return std::tolower(c); });
   if( extensions.find(fExt) != extensions.end() )
      return true;
   return false;
}
bool SurgeGUIEditor::onDrop( const std::string& fname)
{
   std::cout << "Dropped " << fname << std::endl;
   fs::path fPath(fname);
   std::string fExt(path_to_string(fPath.extension()));
   std::transform(fExt.begin(), fExt.end(), fExt.begin(),
                  [](unsigned char c){ return std::tolower(c); });
   if( fExt == ".wav" || fExt == ".wt" )
   {
      strncpy(synth->storage.getPatch().scene[current_scene].osc[current_osc[current_scene]].wt.queue_filename,
              fname.c_str(), 255);
   }
   else if( fExt == ".scl" )
   {
      tuningFileDropped( fname );
   }
   else if( fExt == ".kbm" )
   {
      mappingFileDropped( fname );
   }
   else if( fExt == ".fxp" )
   {
      queuePatchFileLoad(fname);
   }

   return true;
}

void SurgeGUIEditor::swapFX(int source, int target, SurgeSynthesizer::FXReorderMode m)
{
   auto t = fxPresetName[target];
   fxPresetName[target] = fxPresetName[source];
   if( m == SurgeSynthesizer::SWAP )
      fxPresetName[source] = t;
   if( m == SurgeSynthesizer::MOVE )
      fxPresetName[source] = "";

   synth->reorderFx(source, target, m );
}

void SurgeGUIEditor::lfoShapeChanged(int prior, int curr)
{
   if( prior != curr || prior == lt_mseg || curr == lt_mseg )
   {
      if( msegEditSwitch )
      {
         msegEditSwitch->setVisible( curr == lt_mseg );
      }
   }

   if( curr == lt_mseg && editorOverlay && editorOverlayTag == "msegEditor" )
   {
      // We have the MSEGEditor open and have swapped to the MSEG here
      showMSEGEditor();
   }
   else if( prior == lt_mseg && curr != lt_mseg && editorOverlay && editorOverlayTag == "msegEditor" )
   {
      // We can choose to not do this too; if we do we are editing an MSEG which isn't used though
      closeMSEGEditor();
   }

   // update the LFO title label
   std::string modname = modulatorName(modsource_editor[current_scene], true);
   lfoNameLabel->setText(modname.c_str());
   lfoNameLabel->invalid();
}

void SurgeGUIEditor::closeStorePatchDialog()
{
   if (editorOverlayTag == "storePatch")
   {
      dismissEditorOverlay();
   }
   // Have to update all that state too for the newly orphaned items
   patchName = nullptr;
   patchCategory = nullptr;
   patchCreator = nullptr;
   patchComment = nullptr;

}

void SurgeGUIEditor::showStorePatchDialog()
{
   if (editorOverlayTag == "msegEditor")
   {
      dismissEditorOverlay();
   }

   makeStorePatchDialog();
}

void SurgeGUIEditor::closeMSEGEditor()
{
   if( editorOverlayTag == "msegEditor" ) {
      dismissEditorOverlay( );
   }
}
void SurgeGUIEditor::toggleMSEGEditor()
{
   if( editorOverlayTag == "msegEditor" )
   {
      closeMSEGEditor();
   }
   else
   {
      showMSEGEditor();
   }
}

void SurgeGUIEditor::showMSEGEditor()
{
   auto lfo_id = modsource_editor[current_scene] - ms_lfo1;
   auto lfodata = &synth->storage.getPatch().scene[current_scene].lfo[lfo_id];
   auto ms = &synth->storage.getPatch().msegs[current_scene][lfo_id];
   auto mse = new MSEGEditor(lfodata, ms, &msegEditState[lfo_id], currentSkin, bitmapStore);
   auto vs = mse->getViewSize().getWidth();
   float xp = (currentSkin->getWindowSizeX() - (vs + 8)) * 0.5;

   std::string title = modsource_names[modsource_editor[current_scene]];
   title += " Editor";
   Surge::Storage::findReplaceSubstring(title, std::string("LFO"), std::string("MSEG"));

   setEditorOverlay(mse, title, "msegEditor", CPoint(0, 57), false, true, [this]() {
      if (msegEditSwitch)
      {
         msegEditSwitch->setValue(0.0);
         msegEditSwitch->invalid();
      }
   });

   if( msegEditSwitch )
   {
      msegEditSwitch->setValue( 1.0 );
      msegEditSwitch->invalid();
   }
}

void SurgeGUIEditor::repushAutomationFor(Parameter* p)
{
   auto id = synth->idForParameter(p);
   synth->sendParameterAutomation(id, synth->getParameter01(id));

#if TARGET_AUDIOUNIT
   synth->getParent()->ParameterBeginEdit( id.getDawSideIndex());
   synth->getParent()->ParameterUpdate( id.getDawSideIndex());
   synth->getParent()->ParameterEndEdit(id.getDawSideIndex());
#endif

}

void SurgeGUIEditor::showAboutBox()
{
   std::cout << "Show About Box" << std::endl;

   CRect wsize(0, 0, getWindowSizeX(), getWindowSizeY());
   aboutbox = new CAboutBox(wsize, this, &(synth->storage), synth->hostProgram, currentSkin, bitmapStore );
   aboutbox->setVisible( true );
   getFrame()->addView( aboutbox );
}

void SurgeGUIEditor::hideAboutBox()
{
   std::cout << "Hide About box" << std::endl;
   if( aboutbox )
   {
      aboutbox->setVisible( false );
      removeFromFrame.push_back( aboutbox );
      aboutbox = nullptr;
   }
}
