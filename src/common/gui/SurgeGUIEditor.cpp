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
#include "CStatusPanel.h"
#include "COscillatorDisplay.h"
#include "CModulationSourceButton.h"
#include "CSnapshotMenu.h"
#include "CLFOGui.h"
#include "CEffectSettings.h"
#include "CSurgeVuMeter.h"
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
#include "UIInstrumentation.h"
#include "guihelpers.h"
#include "DebugHelpers.h"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stack>
#include <unordered_map>

#if TARGET_VST3
#include "pluginterfaces/vst/ivstcontextmenu.h"
#include "pluginterfaces/base/ustring.h"

#include "vstgui/lib/cvstguitimer.h"


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
         auto rc = t->release();
      }
   }
   T *t = nullptr;
};

#endif

#if TARGET_AUDIOUNIT
#include "aulayer.h"
#endif

#include "ImportFilesystem.h"

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
   tag_statuspanel,
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
   // tag_metaparam,
   // tag_metaparam_end = tag_metaparam+n_customcontrollers,
   start_paramtags,
};

std::string specialTagToString( special_tags t )
{
   if( t >= tag_mod_source0 && t < tag_mod_source_end )
   {
      modsources modsource = (modsources)(t - tag_mod_source0);
      std::string s = std::string( "tag_modsource" ) + modsource_abberations_short[modsource];
      return s;
   }

   switch( t ) {
   case tag_scene_select:
      return "tag_scene_select";
   case tag_osc_select:
      return "tag_osc_select";
   case tag_osc_menu:
      return "tag_osc_menu";
   case tag_fx_select:
      return "tag_fx_select";
   case tag_fx_menu:
      return "tag_fx_menu";
   case tag_patchname:
      return "tag_patchname";
   case tag_statuspanel:
      return "tag_statuspanel";
   case tag_mp_category:
      return "tag_mp_category";
   case tag_mp_jogfx:
      return "tag_mp_jogfx";
   case tag_mp_patch:
      return "tag_mp_patch";
   case tag_store:
      return "tag_store";
   case tag_store_cancel:
      return "tag_store_cancel";
   case tag_store_ok:
      return "tag_store_ok";
   case tag_store_name:
      return "tag_store_name";
   case tag_store_category:
      return "tag_store_category";
   case tag_store_creator:
      return "tag_store_creator";
   case tag_store_comments:
      return "tag_store_comments";
   case tag_store_tuning:
      return "tag_store_tuning";
   case tag_settingsmenu:
      return "tag_store_settingsmenu";
   default:
      return "ERROR";
   }
   return "ERROR";

}

SurgeGUIEditor::SurgeGUIEditor(void* effect, SurgeSynthesizer* synth, void* userdata) : super(effect)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeGUIEditor::SurgeGUIEditor" );
#endif
   frame = 0;

#if TARGET_VST3
   // setIdleRate(25);
   // synth = ((SurgeProcessor*)effect)->getSurge();
#endif


   patchname = 0;
   statuspanel = nullptr;
   current_scene = 1;
   current_osc = 0;
   current_fx = 0;
   modsource = ms_lfo1;
   modsource_editor = ms_lfo1;
   blinktimer = 0.f;
   blinkstate = false;
   aboutbox = 0;

   mod_editor = false;

   // init the size of the plugin
   int userDefaultZoomFactor = Surge::Storage::getUserDefaultValue(&(synth->storage), "defaultZoom", 100);
   float zf = userDefaultZoomFactor / 100.0;

   if( synth->storage.getPatch().dawExtraState.isPopulated &&
       synth->storage.getPatch().dawExtraState.instanceZoomFactor > 0
       )
   {
       // If I restore state before I am constructed I need to do this
       zf = synth->storage.getPatch().dawExtraState.instanceZoomFactor / 100.0;
   }

   rect.left = 0;
   rect.top = 0;
#if TARGET_VST2 || TARGET_VST3
   rect.right = WINDOW_SIZE_X * zf;
   rect.bottom = WINDOW_SIZE_Y * zf;
#else
   rect.right = WINDOW_SIZE_X;
   rect.bottom = WINDOW_SIZE_Y;
#endif
   editor_open = false;
   queue_refresh = false;
   memset(param, 0, n_paramslots * sizeof(void*));
   polydisp = 0; // FIXME - when changing skins and rebuilding we need to reset these state variables too
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
   idleinc = 0;
   for( int i=0; i<8; ++i )
   {
      selectedFX[i] = -1;
      fxPresetName[i] = "Off";
   }

   _effect = effect;
   _userdata = userdata;
   this->synth = synth;

   // ToolTipWnd = 0;

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
}

SurgeGUIEditor::~SurgeGUIEditor()
{
   if (frame)
   {
      getFrame()->unregisterKeyboardHook(this);
      frame->close();
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
         frame->removeView(c);
      }

      removeFromFrame.clear();

      if(zoomInvalid)
      {
         setZoomFactor(getZoomFactor());
         zoomInvalid = false;
      }

      if (aboutbox && (aboutbox->getValue() > 0.5f))
         return;
      /*static CDrawContext drawContext
        (frame, NULL, systemWindow);*/
      // CDrawContext *drawContext = frame->createDrawContext();

      CView* v = frame->getFocusView();
      if (v && dynamic_cast<CControl*>(v) != nullptr)
      {
         int ptag = ((CControl*)v)->getTag() - start_paramtags;
         if (ptag >= 0)
         {
            synth->storage.CS_ModRouting.enter();

            for (int i = 1; i < n_modsources; i++)
            {
               if( gui_modsrc[i] )
                  ((CModulationSourceButton*)gui_modsrc[i])
                     ->update_rt_vals(synth->isActiveModulation(ptag, (modsources)i), 0,
                                      synth->isModsourceUsed((modsources)i));
            }
            synth->storage.CS_ModRouting.leave();
         }
      }
      else
      {
         synth->storage.CS_ModRouting.enter();
         for (int i = 1; i < n_modsources; i++)
         {
            if( gui_modsrc[i] )
               ((CModulationSourceButton*)gui_modsrc[i])
                  ->update_rt_vals(false, 0, synth->isModsourceUsed((modsources)i));
         }
         synth->storage.CS_ModRouting.leave();
      }
#if MAC
      idleinc++;
      if (idleinc > 15)
      {
         idleinc = 0;
#elif LINUX // slow down the blinking on linux a bit
      idleinc++;
      if (idleinc > 50)
      {
         idleinc = 0;
#else
      SYSTEMTIME st;
      GetSystemTime(&st);

      if (((st.wMilliseconds > 500) && blinkstate) || ((st.wMilliseconds <= 500) && !blinkstate))
      {
#endif
         for (int i = 1; i < n_modsources; i++)
         {
            if( gui_modsrc[i] )
               ((CModulationSourceButton*)gui_modsrc[i])->setblink(blinkstate);
         }
         blinkstate = !blinkstate;
      }

      if (synth->storage.getPatch().scene[current_scene].osc[current_osc].wt.refresh_display)
      {
         synth->storage.getPatch().scene[current_scene].osc[current_osc].wt.refresh_display = false;
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
            typeinLabel->setFontColor(currentSkin->getColor( "slider.light.label", kBlackCColor ));
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

      if( statuspanel )
      {
         CStatusPanel *pb = (CStatusPanel *)statuspanel;
         pb->setDisplayFeature(CStatusPanel::mpeMode, synth->mpeEnabled);
         pb->setDisplayFeature(CStatusPanel::tuningMode, ! synth->storage.isStandardTuning);
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
               synth->getParameterName(j, pname);
               synth->getParameterDisplay(j, pdisp);

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

      for (int i = 0; i < 8; i++)
      {
         if (synth->refresh_parameter_queue[i] >= 0)
         {
            int j = synth->refresh_parameter_queue[i];
            synth->refresh_parameter_queue[i] = -1;
            if ((j < n_total_params) && param[j])
            {
               param[j]->setValue(synth->getParameter01(j));
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
                   .scene[current_scene]
                   .modsources[ms_ctrl1 + i])
                  ->get_target01());
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
               auto sv = synth->getParameter01(j);
               auto cv = cc->getValue();

               if ((sv != cv) && ((tag == fmconfig_tag || tag == filterblock_tag)))
               {
                  std::unordered_map<int, bool> resetMap;

                  if (tag == fmconfig_tag)
                  {
                     auto targetTag = synth->storage.getPatch().scene[current_scene].fm_depth.id + start_paramtags;
                     auto targetState = ((int)(sv * n_fm_configuration) == fm_off);
                     resetMap[targetTag] = targetState;
                  }

                  if (tag == filterblock_tag)
                  {
                     auto pval = (int)(sv * n_fb_configuration);
                     
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
                  cc->setValue(synth->getParameter01(j));
#else
               cc->setValue(synth->getParameter01(j));
#endif
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
         if (((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms_ctrl1 +
                                                                                                     i])
             ->has_changed(true))
         {
            gui_modsrc[ms_ctrl1 + i]->setValue(
               ((ControllerModulationSource*)synth->storage.getPatch()
                .scene[current_scene]
                .modsources[ms_ctrl1 + i])
               ->get_target01());
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
}

void SurgeGUIEditor::toggle_mod_editing()
{
   mod_editor = !mod_editor;
   refresh_mod();
}

void SurgeGUIEditor::refresh_mod()
{
   CModulationSourceButton *cms = (CModulationSourceButton *)gui_modsrc[modsource];

   modsources thisms = modsource;
   if( cms->hasAlternate && cms->useAlternate )
      thisms = (modsources)cms->alternateId;

   synth->storage.CS_ModRouting.enter();
   for (int i = 0; i < n_paramslots; i++)
   {
      if (param[i])
      {
         CSurgeSlider* s = (CSurgeSlider*)param[i];
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
#if OSC_MOD_ANIMATION
   if (oscdisplay)
   {
      ((COscillatorDisplay*)oscdisplay)->setIsMod(mod_editor);
      ((COscillatorDisplay*)oscdisplay)->setModSource(thisms);
      oscdisplay->invalid();
      oscdisplay->setDirty(true);
   }
#endif

   synth->storage.CS_ModRouting.leave();
   for (int i = 1; i < n_modsources; i++)
   {
      int state = 0;
      if (i == modsource)
         state = mod_editor ? 2 : 1;
      if (i == modsource_editor)
         state |= 4;

      if( gui_modsrc[i] )
      {
         // this could change if I cleared the last one
         ((CModulationSourceButton*)gui_modsrc[i])->used = synth->isModsourceUsed( (modsources)i );
         ((CModulationSourceButton*)gui_modsrc[i])->state = state;

         if( i < ms_ctrl1 || i > ms_ctrl8 )
         {
            auto mn = modulatorName(i, true);
            ((CModulationSourceButton*)gui_modsrc[i])->setlabel(mn.c_str() );
         }
         ((CModulationSourceButton*)gui_modsrc[i])->invalid();
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
         if ( (saveDialog && saveDialog->isVisible()) || (typeinDialog && typeinDialog->isVisible() ) )
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
      return (controlGroupEntry == current_osc);
   case cg_LFO:
      return (controlGroupEntry == modsource_editor);
   case cg_FX:
      return (controlGroupEntry == current_fx);
   default:
      return true;
   }
}

CRect positionForModulationGrid(modsources entry)
{
   const int width = isCustomController(entry) ? 75 : 64;
   CRect r(2, 1, width, 14 + 1);

   if (isCustomController(entry))
      r.bottom += 8;
   int gridX = modsource_grid_xy[entry][0];
   int gridY = modsource_grid_xy[entry][1];
   r.offset(0, 399 + 8 * gridY);
   for (int i = 0; i < gridX; i++)
   {
      r.offset((i >= 7) ? 75 : 64, 0);
   }

   return r;
}

void SurgeGUIEditor::setDisabledForParameter(Parameter* p, CSurgeSlider* s)
{
   if (p->id == synth->storage.getPatch().scene[current_scene].fm_depth.id)
   {
      s->disabled = !(synth->storage.getPatch().scene[current_scene].fm_switch.val.i);
   }
}

void SurgeGUIEditor::openOrRecreateEditor()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SurgeGUIEditor::openOrRecreateEditor" );
#endif
   if (!synth)
      return;
   assert(frame);

   if (editor_open)
      close_editor();

   CPoint nopoint(0, 0);

   clear_infoview_peridle = -1;
   
   /*
   ** There are a collection of member states we need to reset
   */
   polydisp = 0;
   lfodisplay = 0;
   fxmenu = 0;
   typeinDialog = nullptr;
   removeFromFrame.clear();
   for( int i=0; i<16; ++i ) vu[i] = 0;

   current_scene = synth->storage.getPatch().scene_active.val.i;

   CControl *skinTagControl;
   if( ( skinTagControl = layoutTagWithSkin( tag_osc_select ) ) == nullptr ) {
      CRect rect(0, 0, 75, 13);
      rect.offset(104 - 36, 69);
      auto oscswitch = new CHSwitch2(rect, this, tag_osc_select, 3, 13, 1, 3,
                                          bitmapStore->getBitmap(IDB_OSCSELECT), nopoint);
      oscswitch->setValue((float)current_osc / 2.0f);
      frame->addView(oscswitch);
      oscswitch->setSkin(currentSkin,bitmapStore);
   }

   if( ( skinTagControl = layoutTagWithSkin( tag_fx_select ) ) == nullptr ) {
      CRect rect(0, 0, 119, 51);
      rect.offset(764 + 3, 71);
      CEffectSettings* fc = new CEffectSettings(rect, this, tag_fx_select, current_fx, bitmapStore);
      ccfxconf = fc;
      for (int i = 0; i < 8; i++)
      {
         fc->set_type(i, synth->storage.getPatch().fx[i].type.val.i);
      }
      fc->set_bypass(synth->storage.getPatch().fx_bypass.val.i);
      fc->set_disable(synth->storage.getPatch().fx_disable.val.i);
      frame->addView(fc);
   }

   /* This loop bound is 1.6.* valid ONLY */
   for (int k = 1; k < /* n_modsources */ ms_releasevelocity; k++)
   {
      modsources ms = (modsources)k;

      CRect r = positionForModulationGrid(ms);

      int state = 0;
      if (ms == modsource)
         state = mod_editor ? 2 : 1;
      if (ms == modsource_editor)
         state |= 4;

      gui_modsrc[ms] =
         new CModulationSourceButton(r, this, tag_mod_source0 + ms, state, ms, bitmapStore, &(synth->storage));
      ((CModulationSourceButton*)gui_modsrc[ms])->setSkin(currentSkin);
      ((CModulationSourceButton*)gui_modsrc[ms])
         ->update_rt_vals(false, 0, synth->isModsourceUsed(ms));
      if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
      {
         ((CModulationSourceButton*)gui_modsrc[ms])
            ->setlabel(synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
         ((CModulationSourceButton*)gui_modsrc[ms])->set_ismeta(true);
         ((CModulationSourceButton*)gui_modsrc[ms])
            ->setBipolar(synth->storage.getPatch().scene[current_scene].modsources[ms]->is_bipolar());
         gui_modsrc[ms]->setValue(
            ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[ms])
            ->get_target01());
      }
      else
      {
         ((CModulationSourceButton*)gui_modsrc[ms])->setlabel(modulatorName(ms, true).c_str() );
         /*
         ** Velocity special case for 1.6.* vintage
         */
         if( ms == ms_velocity )
         {
            ((CModulationSourceButton*)gui_modsrc[ms])->setAlternate(ms_releasevelocity,
                                                                     modsource_abberations_button[ms_releasevelocity]);
            ((CModulationSourceButton*)gui_modsrc[ms])->setUseAlternate(modsource_is_alternate[ms]);
         }
      }
      frame->addView(gui_modsrc[ms]);
   }

   /*// Comments
     {
     CRect CommentsRect(6 + 150*4,528, WINDOW_SIZE_X, WINDOW_SIZE_Y);
     CTextLabel *Comments = new
     CTextLabel(CommentsRect,synth->storage.getPatch().comment.c_str());
     Comments->setTransparency(true);
     Comments->setFont(displayFont);
     Comments->setHoriAlign(kMultiLineCenterText);
     frame->addView(Comments);
     }*/

   // main vu-meter
   CRect vurect(763, 0, 763 + 123, 13);
   vurect.offset(0, 14);
   vu[0] = new CSurgeVuMeter(vurect);
   ((CSurgeVuMeter*)vu[0])->setSkin(currentSkin,bitmapStore);
   ((CSurgeVuMeter*)vu[0])->setType(vut_vu_stereo);
   frame->addView(vu[0]);

   // fx vu-meters & labels

   vurect.offset(0, 162 + 8);
   if (synth->fx[current_fx])
   {
      for (int i = 0; i < 8; i++)
      {
         int t = synth->fx[current_fx]->vu_type(i);
         if (t)
         {
            CRect vr(vurect);
            vr.offset(0, yofs * synth->fx[current_fx]->vu_ypos(i));
            vr.offset(0, 7);
            vu[i + 1] = new CSurgeVuMeter(vr);
            ((CSurgeVuMeter*)vu[i + 1])->setSkin(currentSkin,bitmapStore);
            ((CSurgeVuMeter*)vu[i + 1])->setType(t);
            frame->addView(vu[i + 1]);
         }
         else
            vu[i + 1] = 0;

         const char* label = synth->fx[current_fx]->group_label(i);

         if (label)
         {
            CRect vr(vurect);
            vr.top += 1;
            vr.offset(0, 9);
            vr.offset(0, yofs * synth->fx[current_fx]->group_label_ypos(i));
            CEffectLabel* lb = new CEffectLabel(vr);
            lb->setLabel(label);
            lb->setSkin(currentSkin,bitmapStore);
            frame->addView(lb);
         }
      }
   }

   // CRect(12,62,140,159)
   oscdisplay = new COscillatorDisplay(CRect(6, 81, 142, 180),
                                       &synth->storage.getPatch()
                                       .scene[synth->storage.getPatch().scene_active.val.i]
                                       .osc[current_osc],
                                       &synth->storage);
   ((COscillatorDisplay*)oscdisplay)->setSkin( currentSkin, bitmapStore );
   frame->addView(oscdisplay);

   // 150*b - 16 = 434 (b=3)
   patchname =
      new CPatchBrowser(CRect(156, 11, 547, 11 + 28), this, tag_patchname, &synth->storage);
   ((CPatchBrowser*)patchname)->setSkin( currentSkin, bitmapStore );
   ((CPatchBrowser*)patchname)->setLabel(synth->storage.getPatch().name);
   ((CPatchBrowser*)patchname)->setCategory(synth->storage.getPatch().category);
   ((CPatchBrowser*)patchname)->setIDs(synth->current_category_id, synth->patchid);
   ((CPatchBrowser*)patchname)->setAuthor(synth->storage.getPatch().author);
   frame->addView(patchname);

   statuspanel = new CStatusPanel(CRect( 560, 1, 595, 54 ), this, tag_statuspanel, &synth->storage, bitmapStore);
   {
      CStatusPanel *pb = (CStatusPanel *)statuspanel;
      pb->setSkin( currentSkin, bitmapStore );
      pb->setDisplayFeature(CStatusPanel::mpeMode, synth->mpeEnabled);
      pb->setDisplayFeature(CStatusPanel::tuningMode, ! synth->storage.isStandardTuning);
      pb->setEditor(this);
   }

   frame->addView(statuspanel);

   int catx = 157, caty = 41;
   auto catctrl = currentSkin->controlForEnumID( tag_mp_category );
   if( catctrl )
   {
      catx = catctrl->x;
      caty = catctrl->y;
   }
   
   CHSwitch2* mp_cat =
      new CHSwitch2(CRect(catx, caty, 157 + 37, 41 + 12), this, tag_mp_category, 2, 12, 1, 2,
                    bitmapStore->getBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   mp_cat->setUsesMouseWheel(false); // mousewheel on category and patch buttons is undesirable
   mp_cat->setSkin( currentSkin, bitmapStore );
   frame->addView(mp_cat);

   int patchx = 242, patchy = 41;
   auto patchctrl = currentSkin->controlForEnumID( tag_mp_patch );
   if( patchctrl )
   {
      patchx = patchctrl->x;
      patchy = patchctrl->y;
   }
   CHSwitch2* mp_patch =
      new CHSwitch2(CRect(patchx, patchy, 242 + 37, 41 + 12), this, tag_mp_patch, 2, 12, 1, 2,
                    bitmapStore->getBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   mp_patch->setUsesMouseWheel(false);// mousewheel on category and patch buttons is undesirable
   mp_patch->setSkin( currentSkin, bitmapStore );
   frame->addView(mp_patch);

   int jogx = 759 + 131 - 39, jogy = 182 + 15;
   auto jogctrl = currentSkin->controlForEnumID( tag_mp_jogfx );
   if( jogctrl )
   {
      jogx = jogctrl->x;
      jogy = jogctrl->y;
   }

   CHSwitch2* mp_jogfx =
      new CHSwitch2(CRect(jogx, jogy, jogx + 37, jogy + 12), this, tag_mp_jogfx, 2, 12, 1, 2,
                    bitmapStore->getBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   mp_jogfx->setUsesMouseWheel(false); // mousewheel on category and patch buttons is undesirable
   mp_jogfx->setSkin( currentSkin, bitmapStore );
   frame->addView(mp_jogfx);

   fxPresetLabel = new CTextLabel( CRect( 759, 197, jogx - 2, 207 ), "Preset" );
   fxPresetLabel->setFontColor(currentSkin->getColor( "effect.preset.name", CColor( 0, 0, 0, 255 ) ));
   fxPresetLabel->setTransparency(true);
   fxPresetLabel->setFont( displayFont );
   fxPresetLabel->setHoriAlign( kRightText );
   fxPresetLabel->setTextTruncateMode( CTextLabel::TextTruncateMode::kTruncateTail );
   frame->addView(fxPresetLabel);

   CHSwitch2* b_store = new CHSwitch2(CRect(547 - 37, 41, 547, 41 + 12), this, tag_store, 1, 12, 1,
                                      1, bitmapStore->getBitmap(IDB_BUTTON_STORE), nopoint, false);
   b_store->setSkin( currentSkin, bitmapStore );
   frame->addView(b_store);

   memset(param, 0, n_paramslots * sizeof(void*));
   memset(nonmod_param, 0, n_paramslots * sizeof(void*));
   int i = 0;
   vector<Parameter*>::iterator iter;
   for (iter = synth->storage.getPatch().param_ptr.begin();
        iter != synth->storage.getPatch().param_ptr.end(); iter++)
   {
      if( i == n_paramslots )
      {
         // This would only happen if a dev added params. But with 1.7 a dev might so lets at least put this here.
         Surge::UserInteractions::promptError( "INTERNAL ERROR - params larger than paramslots.  Up n_paramslots in SGE.h", "SOFTWARE ERROR" );
      }
      Parameter* p = *iter;

      bool paramIsVisible = ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
         isControlVisible(p->ctrlgroup, p->ctrlgroup_entry) && (p->ctrltype != ct_none);

      std::string uiid = p->ui_identifier;

      bool handledBySkins = false;

      /*
      ** OK so we basically have two paths here. One which is skin specified, and one which is
      ** default specified. The skin specified one delegates almost all the decisions to the skin
      ** engine data structure if it can find it
      */
      auto c = currentSkin->controlForUIID(uiid);
      if( c.get() && paramIsVisible )
      {
         // borrow from 1.6.6
         long style = p->ctrlstyle;

         switch (p->ctrltype)
         {
         case ct_decibel:
         case ct_decibel_narrow:
         case ct_decibel_narrow_extendable:
         case ct_decibel_extra_narrow:
         case ct_decibel_extendable:
         case ct_freq_mod:
         case ct_percent_bidirectional:
         case ct_percent_bidirectional_stereo:
         case ct_freq_shift:
         case ct_osc_feedback_negative:
            style |= kBipolar;
            break;
         };

         auto parentClassName = c->ultimateparentclassname;
         handledBySkins = true; // by default assume we are handled

         // Would be great to change this to a switch by making it an enum. FIXME
         if( parentClassName == "CSurgeSlider" )
         {
            // If we don't specify a positoin use the default
            int px = ( c->x >= 0 ? c->x : p->posx );
            int py = ( c->y >= 0 ? c->y : p->posy );
            bool is_mod = synth->isValidModulation(p->id, modsource);
            CSurgeSlider* hs =
               new CSurgeSlider(CPoint(px, py), style, this, p->id + start_paramtags, is_mod, bitmapStore, &(synth->storage));
            hs->setSkin(currentSkin,bitmapStore,c);
            hs->setMoveRate(p->moverate);
            if( p->can_temposync() )
               hs->setTempoSync(p->temposync);
            hs->setValue(p->get_value_f01());
            hs->setLabel(p->get_name());
            hs->setModPresent(synth->isModDestUsed(p->id));
            hs->setDefaultValue(p->get_default_value_f01());

            if( p->can_deactivate() )
               hs->deactivated = p->deactivated;
            else
               hs->deactivated = false;
         
         
            if( p->valtype  == vt_int || p->valtype  == vt_bool) 
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
               hs->setModCurrent(synth->isActiveModulation(p->id, modsource), synth->isBipolarModulation(modsource));
            }
            else
            {
               // Even if current modsource isn't modulating me, something else may be
            }
            frame->addView(hs);
            param[i] = hs;

         }
         else if( parentClassName == "CLFOGui" )
         {
            if( p->ctrltype != ct_lfoshape )
            {
               // FIXME - warning?
            }
            CRect rect(0, 0, c->w, c->h);
            rect.offset(c->x, c->y);
            int lfo_id = p->ctrlgroup_entry - ms_lfo1;
            if ((lfo_id >= 0) && (lfo_id < n_lfos))
            {
               CLFOGui* slfo = new CLFOGui(
                  rect, lfo_id >= 0 && lfo_id <= ( ms_lfo6 - ms_lfo1 ), this, p->id + start_paramtags,
                  &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
                  &synth->storage.getPatch().stepsequences[current_scene][lfo_id],
                  bitmapStore);
               slfo->setSkin( currentSkin, bitmapStore, c );
               lfodisplay = slfo;
               frame->addView(slfo);
               nonmod_param[i] = slfo;
            }
         }
         else if( parentClassName == "CSwitchControl" )
         {
            CRect rect(0, 0, c->w, c->h);
            rect.offset(c->x, c->y);

            // Make this a function on skin
            auto bmp = currentSkin->backgroundBitmapForControl( c, bitmapStore );
            if( bmp )
            {
               CSwitchControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags, bmp );
               hsw->setValue(p->get_value_f01());
               hsw->setSkin(currentSkin,bitmapStore,c);
               frame->addView(hsw);
               nonmod_param[i] = hsw;

               // Carry over this filter type special case from the default control path
               if( p->ctrltype == ct_filtertype )
               {
                  ((CSwitchControl*)hsw)->is_itype = true;
                  ((CSwitchControl*)hsw)->imax = 3;
                  ((CSwitchControl*)hsw)->ivalue = p->val.i + 1;
                  if (fut_subcount[synth->storage.getPatch()
                                   .scene[current_scene]
                                   .filterunit[p->ctrlgroup_entry]
                                   .type.val.i] == 0)
                     ((CSwitchControl*)hsw)->ivalue = 0;

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
            else
            {
               std::cout << "Couldn't make CSwitch Control for ID '" << uiid << "'" << std::endl;
            }

         }
         else if( parentClassName == "CHSwitch2" )
         {
            CRect rect(0, 0, c->w, c->h);
            rect.offset(c->x, c->y);

            // Make this a function on skin
            auto bmp = currentSkin->backgroundBitmapForControl( c, bitmapStore );

            auto defaultTag = p->id + start_paramtags;
            switch( p->ctrltype )
            {
            case ct_scenesel:
               defaultTag = tag_scene_select;
               break;
            default:
               break;
               // FIXME - we need to fill in the rest of these
            }
            
            if( bmp )
            {
               auto subpixmaps = currentSkin->propertyValue( c, "subpixmaps", "1" );
               auto rows = currentSkin->propertyValue( c, "rows", "1" );
               auto cols = currentSkin->propertyValue( c, "columns", "1" );
               auto hsw = new CHSwitch2(rect, this, defaultTag,
                                        std::atoi(subpixmaps.c_str()), c->h,
                                        std::atoi(rows.c_str()), std::atoi(cols.c_str()),
                                        bmp, nopoint,
                                        true); // FIXME - this needs parameterization
               hsw->setValue(p->get_value_f01());
               frame->addView(hsw);
               nonmod_param[i] = hsw;
               hsw->setSkin( currentSkin, bitmapStore, c );
            }

         }
         else
         {
            handledBySkins = false; /* FIXME: currentSkin->fallbackToDefaultSkin() */
            std::cout << "Was able to get a control for the uiid '" << c->ui_id  << "' class is '" << c->classname << "' upc='" << parentClassName << "'" << std::endl;
         }


         /*
           switch( parentClass )
           {
           case Surge::UI::Skin::Control::CSurgeSlider:
           case Surge::UI::t lSkin::Control::CHSwitch2:
           default:
           std::cout << "UNKNOWN PARENT CLASS " << parentClass <<  std::endl;
           break;
           }
         */
      }

      if (! handledBySkins && p->posx != 0 && paramIsVisible)
      {
         /*
         **  This is the legacy/1.6.6 codepath
         */
         long style = p->ctrlstyle;
         /*if(p->ctrlstyle == cs_hori) style = kHorizontal;
           else if(p->ctrlstyle == cs_vert) style = kVertical | kBottom;*/
         switch (p->ctrltype)
         {
         case ct_decibel:
         case ct_decibel_narrow:
         case ct_decibel_narrow_extendable:
         case ct_decibel_extra_narrow:
         case ct_decibel_extendable:
         case ct_freq_mod:
         case ct_percent_bidirectional:
         case ct_percent_bidirectional_stereo:
         case ct_osc_feedback_negative:
         case ct_freq_shift:
            style |= kBipolar;
            break;
         };

         switch (p->ctrltype)
         {
         case ct_filtertype:
         {
            CRect rect(0, 0, 129, 18);
            rect.offset(p->posx - 2, p->posy + 1);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 10, 18, 1, 10,
                                          bitmapStore->getBitmap(IDB_FILTERBUTTONS), nopoint, true);
            rect(3, 0, 124, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );

            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_filtersubtype:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 1);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_FILTERSUBTYPE));
            rect(1, 1, 9, 14);
            ((CSwitchControl*)hsw)->is_itype = true;
            ((CSwitchControl*)hsw)->imax = 3;
            ((CSwitchControl*)hsw)->ivalue = p->val.i + 1;
            if (fut_subcount[synth->storage.getPatch()
                             .scene[current_scene]
                             .filterunit[p->ctrlgroup_entry]
                             .type.val.i] == 0)
               ((CSwitchControl*)hsw)->ivalue = 0;
            rect.offset(p->posx + 129, p->posy + 1);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin(currentSkin,bitmapStore);
            frame->addView(hsw);

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
         break;
         case ct_bool_keytrack:
         {
            CRect rect(0, 0, 43, 7);
            rect.offset(p->posx, p->posy);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_KTRK));
            hsw->setSkin(currentSkin,bitmapStore);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_retrigger:
         {
            CRect rect(0, 0, 43, 7);
            rect.offset(p->posx, p->posy);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_RETRIGGER));
            hsw->setSkin(currentSkin,bitmapStore);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_oscroute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 15, 1, 3,
                                          bitmapStore->getBitmap(IDB_OSCROUTE), nopoint, true);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_envshape:
         case ct_envshape_attack:
         {
            bool hasShape = synth->storage.getPatch()
               .scene[current_scene]
               .adsr[p->ctrlgroup_entry]
               .mode.val.i == emt_digital;

            if (hasShape)
            {
               CRect rect(0, 0, 20, 14);
               rect.offset(p->posx, p->posy);
               CHSwitch2* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 14, 1, 3,
                                              bitmapStore->getBitmap(IDB_ENVSHAPE), nopoint, true);
               hsw->setValue(p->get_value_f01());
               hsw->setSkin( currentSkin, bitmapStore );
               if (p->name[0] == 'd')
                  hsw->imgoffset = 3;
               else if (p->name[0] == 'r')
                  hsw->imgoffset = 6;

               frame->addView(hsw);
               nonmod_param[i] = hsw;
            }
         }
         break;
         case ct_envmode:
         {
            CRect rect(0, 0, 34, 15);
            rect.offset(p->posx, p->posy);
            CHSwitch2* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 2, 15, 2, 1,
                                           bitmapStore->getBitmap(IDB_ENVMODE), nopoint, false);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );

            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_lfotrigmode:
         {
            CRect rect(0, 0, 51, 39);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 39, 3, 1,
                                          bitmapStore->getBitmap(IDB_LFOTRIGGER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_mute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_MUTE));
            hsw->setValue(p->get_value_f01());
            hsw->setSkin(currentSkin,bitmapStore);
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_solo:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_SOLO));
            hsw->setValue(p->get_value_f01());
            hsw->setSkin(currentSkin,bitmapStore);
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_unipolar:
         {
            CRect rect(0, 0, 51, 15);
            rect.offset(p->posx, p->posy);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_UNIPOLAR));
            hsw->setValue(p->get_value_f01());
            hsw->setSkin(currentSkin,bitmapStore);
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_relative_switch:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 5);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_RELATIVE_TOGGLE));
            hsw->setSkin(currentSkin,bitmapStore);
            rect(1, 1, 9, 14);
            rect.offset(p->posx + 129, p->posy + 5);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_bool_link_switch:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 5);
            auto hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_LINK));
            hsw->setSkin(currentSkin,bitmapStore);
            rect(1, 1, 9, 14);
            rect.offset(p->posx + 129, p->posy + 5);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_osctype:
         {
            CRect rect(0, 0, 41, 18);
            rect.offset(p->posx + 96, p->posy + 1);
            CControl* hsw = new COscMenu(
               rect, this, tag_osc_menu, &synth->storage,
               &synth->storage.getPatch().scene[current_scene].osc[current_osc], bitmapStore);
            ((COscMenu*)hsw)->setSkin(currentSkin,bitmapStore);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
         }
         break;
         case ct_fxtype:
         {
            CRect rect(6, 0, 131, 15);
            rect.offset(p->posx, p->posy);
            // CControl *m = new
            // CFxMenu(rect,this,tag_fx_menu,&synth->storage,&synth->storage.getPatch().fx[current_fx],current_fx);
            CControl* m = new CFxMenu(rect, this, tag_fx_menu, &synth->storage,
                                      &synth->storage.getPatch().fx[current_fx],
                                      &synth->fxsync[current_fx], current_fx);
            ((CFxMenu*)m)->setSkin(currentSkin,bitmapStore);
            ((CFxMenu*)m)->selectedIdx = this->selectedFX[current_fx];
            fxPresetLabel->setText( this->fxPresetName[current_fx].c_str() );
            m->setValue(p->get_value_f01());
            frame->addView(m);
            fxmenu = m;
         }
         break;
         case ct_wstype:
         {
            CRect rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                          bitmapStore->getBitmap(IDB_WAVESHAPER), nopoint, true);
            rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setSkin( currentSkin, bitmapStore );
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_polymode:
         {
            CRect rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                          bitmapStore->getBitmap(IDB_POLYMODE), nopoint, true);
            rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
            hsw->setSkin( currentSkin, bitmapStore );
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_fxbypass:
         {
            CRect rect(0, 0, 135, 27);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 4, 27, 1, 4,
                                          bitmapStore->getBitmap(IDB_FXBYPASS), nopoint, true);
            fxbypass_tag = p->id + start_paramtags;
            rect(2, 2, 133, 25);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setSkin( currentSkin, bitmapStore );
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_pitch_octave:
         {
            int oid = IDB_OCTAVES;
            if( p->ctrlgroup == cg_OSC )
               oid = IDB_OCTAVES_OSC;
            CRect rect(0, 0, 96, 18);
            rect.offset(p->posx, p->posy + 1);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 7, 18, 1, 7,
                                          bitmapStore->getBitmap(oid), nopoint, true);
            rect(1, 0, 91, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setSkin( currentSkin, bitmapStore );
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_fbconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 8, 52, 1, 8,
                                          bitmapStore->getBitmap(IDB_FBCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );
            frame->addView(hsw);
            nonmod_param[i] = hsw;
            filterblock_tag = p->id + start_paramtags;
         }
         break;
         case ct_fmconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 4, 52, 1, 4,
                                          bitmapStore->getBitmap(IDB_FMCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );
            frame->addView(hsw);
            nonmod_param[i] = hsw;
            fmconfig_tag = p->id + start_paramtags;
         }
         break;
         case ct_scenemode:
         {
            CRect rect(0, 0, 36, 33);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 4, 33, 4, 1,
                                          bitmapStore->getBitmap(IDB_SCENEMODE), nopoint, true);
            rect(1, 1, 35, 32);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setSkin( currentSkin, bitmapStore );

            /*
            ** SceneMode is special now because we have a streaming vs UI difference.
            ** The streamed integer value is 0, 1, 2, 3 which matches the sub3_scenemode
            ** SurgeStorage enum. But our display would look gross in that order, so
            ** our display order is singple, split, chsplit, dual which is 0, 1, 3, 2.
            ** Fine. So just deal with that here.
            */
            auto guiscenemode = p->val.i;
            if( guiscenemode == 3 ) guiscenemode = 2;
            else if( guiscenemode == 2 ) guiscenemode = 3;
            auto fscenemode = 0.005 + 0.99 * ( (float)( guiscenemode ) / (n_scenemodes-1) );
            hsw->setValue(fscenemode);
            /*
            ** end special case
            */

            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_lfoshape:
         {
            CRect rect(0, 0, 359, 85);
            rect.offset(p->posx, p->posy - 2);
            int lfo_id = p->ctrlgroup_entry - ms_lfo1;
            if ((lfo_id >= 0) && (lfo_id < n_lfos))
            {
               CLFOGui* slfo = new CLFOGui(
                  rect, lfo_id >= 0 && lfo_id <= ( ms_lfo6 - ms_lfo1 ) , this, p->id + start_paramtags,
                  &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
                  &synth->storage.getPatch().stepsequences[current_scene][lfo_id],
                  bitmapStore);
               slfo->setSkin( currentSkin, bitmapStore );
               lfodisplay = slfo;
               frame->addView(slfo);
               nonmod_param[i] = slfo;
            }
         }
         break;
         case ct_scenesel:
         {
            CRect rect(0, 0, 51, 27);
            rect.offset(p->posx, p->posy);
            auto sceneswitch = new CHSwitch2(rect, this, tag_scene_select, 2, 27, 1, 2,
                                                  bitmapStore->getBitmap(IDB_SCENESWITCH), nopoint);
            sceneswitch->setSkin( currentSkin, bitmapStore );
            sceneswitch->setValue(p->get_value_f01());
            rect(1, 1, 50, 26);
            rect.offset(p->posx, p->posy);
            sceneswitch->setMouseableArea(rect);
            frame->addView(sceneswitch);
         }
         break;
         case ct_character:
         {
            CRect rect(0, 0, 135, 12);
            rect.offset(p->posx, p->posy);
            auto hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 12, 1, 3,
                                          bitmapStore->getBitmap(IDB_CHARACTER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            hsw->setSkin( currentSkin, bitmapStore );
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_midikey_or_channel:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CNumberField* key = new CNumberField(rect, this, p->id + start_paramtags, nullptr, &(synth->storage));
            key->setSkin(currentSkin,bitmapStore);
            auto sm = this->synth->storage.getPatch().scenemode.val.i;

            switch(sm)
            {
            case sm_single:
            case sm_dual:
               key->setControlMode(cm_none);
               break;
            case sm_split:
               key->setControlMode(cm_notename);
               break;
            case sm_chsplit:
               key->setControlMode(cm_midichannel_from_127);
               break;
            }

            // key->altlook = true;
            key->setValue(p->get_value_f01());
            splitkeyControl = key;
            frame->addView(key);
            nonmod_param[i] = key;
         }
         break;
         case ct_midikey:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CNumberField* key = new CNumberField(rect, this, p->id + start_paramtags, nullptr, &(synth->storage));
            key->setSkin(currentSkin,bitmapStore);
            key->setControlMode(cm_notename);
            // key->altlook = true;
            key->setValue(p->get_value_f01());
            frame->addView(key);
            nonmod_param[i] = key;
         }
         break;
         case ct_pbdepth:
         {
            CRect rect(0, 0, 24, 10);
            rect.offset(p->posx, p->posy);
            CNumberField* pbd = new CNumberField(rect, this, p->id + start_paramtags, nullptr, &(synth->storage));
            pbd->setSkin(currentSkin,bitmapStore);
            pbd->altlook = true;
            pbd->setControlMode(cm_pbdepth);
            pbd->setValue(p->get_value_f01());
            frame->addView(pbd);
         }
         break;
         case ct_polylimit:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CNumberField* key = new CNumberField(rect, this, p->id + start_paramtags, nullptr, &(synth->storage));
            key->setSkin(currentSkin,bitmapStore);
            key->setControlMode(cm_polyphony);
            // key->setLabel("POLY");
            // key->setLabelPlacement(lp_below);
            key->setValue(p->get_value_f01());
            frame->addView(key);
            polydisp = key;
         }
         break;
         default:
         {
            if (synth->isValidModulation(p->id, modsource))
            {
               CSurgeSlider* hs =
                  new CSurgeSlider(CPoint(p->posx, p->posy + p->posy_offset * yofs), style, this,
                                    p->id + start_paramtags, true, bitmapStore, &(synth->storage));
               hs->setSkin(currentSkin,bitmapStore);
               hs->setModMode(mod_editor ? 1 : 0);
               hs->setModValue(synth->getModulation(p->id, modsource));
               hs->setModPresent(synth->isModDestUsed(p->id));
               hs->setModCurrent(synth->isActiveModulation(p->id, modsource), synth->isBipolarModulation(modsource));
               hs->setValue(p->get_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               if( p->can_temposync() )
                  hs->setTempoSync(p->temposync);

               if( p->can_deactivate() )
                  hs->deactivated = p->deactivated;
               else
                  hs->deactivated = false;
           
            
            if( p->valtype  == vt_int || p->valtype  == vt_bool) 
            { 
               hs->isStepped = true; 
               hs->intRange = p->val_max.i - p->val_min.i;
            }
            else
            {
               hs->isStepped = false;
            }
            
            setDisabledForParameter(p, hs);

               frame->addView(hs);
               param[i] = hs;
            }
            else
            {
               CSurgeSlider* hs =
                  new CSurgeSlider(CPoint(p->posx, p->posy + p->posy_offset * yofs), style, this,
                                    p->id + start_paramtags, false, bitmapStore, &(synth->storage));

               // Even if current modsource isn't modulating me, something else may be
               hs->setModPresent(synth->isModDestUsed(p->id));

               hs->setSkin(currentSkin,bitmapStore);
               hs->setValue(p->get_value_f01());
               hs->setDefaultValue(p->get_default_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               if( p->can_temposync() )
                  hs->setTempoSync(p->temposync);

               if( p->can_deactivate() )
                  hs->deactivated = p->deactivated;
               else
                  hs->deactivated = false;
           
           
            if( p->valtype  == vt_int || p->valtype  == vt_bool) 
            { 
               hs->isStepped = true; 
               hs->intRange = p->val_max.i - p->val_min.i;
            }
            else
            {
               hs->isStepped = false;
            }


               setDisabledForParameter(p, hs);

               frame->addView(hs);
               param[i] = hs;
            }
         }
         break;
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

   CRect aboutbrect(892 - 37, 526, 892, 526 + 12);

   CHSwitch2* b_settingsMenu =
      new CHSwitch2(aboutbrect, this, tag_settingsmenu, 1, 27, 1, 1,
                    bitmapStore->getBitmap(IDB_BUTTON_MENU), nopoint, false);
   b_settingsMenu->setSkin( currentSkin, bitmapStore );
   frame->addView(b_settingsMenu);

   infowindow = new CParameterTooltip(CRect(0, 0, 0, 0));
   ((CParameterTooltip *)infowindow)->setSkin( currentSkin );
   frame->addView(infowindow);

   CRect wsize(0, 0, WINDOW_SIZE_X, WINDOW_SIZE_Y);
   aboutbox =
      new CAboutBox(aboutbrect, this, 0, 0, wsize, nopoint, bitmapStore->getBitmap(IDB_ABOUT));
   ((CAboutBox *)aboutbox)->setSkin(currentSkin,bitmapStore);

   frame->addView(aboutbox);


   CRect dialogSize(148, 53, 598, 53 + 182);
   saveDialog = new CViewContainer(dialogSize);
   saveDialog->setBackground(bitmapStore->getBitmap(IDB_STOREPATCH));
   saveDialog->setVisible(false);
   frame->addView(saveDialog);

   CHSwitch2* cancelButton = new CHSwitch2(CRect(CPoint(305, 147), CPoint(62, 25)), this,
                                           tag_store_cancel, 1, 25, 1, 1, nullptr, nopoint, false);
   saveDialog->addView(cancelButton);
   CHSwitch2* okButton = new CHSwitch2(CRect(CPoint(373, 147), CPoint(62, 25)), this, tag_store_ok,
                                       1, 25, 1, 1, nullptr, nopoint, false);
   saveDialog->addView(okButton);

   patchName = new CTextEdit(CRect(CPoint(96, 31), CPoint(340, 21)), this, tag_store_name);
   patchCategory = new CTextEdit(CRect(CPoint(96, 58), CPoint(340, 21)), this, tag_store_category);
   patchCreator = new CTextEdit(CRect(CPoint(96, 85), CPoint(340, 21)), this, tag_store_creator);
   patchComment = new CTextEdit(CRect(CPoint(96, 112), CPoint(340, 21)), this, tag_store_comments);
   patchTuning = new CCheckBox(CRect(CPoint(96, 112 + (112-85) ), CPoint( 21, 21 )), this, tag_store_tuning);
   patchTuningLabel = new CTextLabel(CRect(CPoint(96 + 22, 112 + (112-85) ), CPoint( 200, 21 )));
   patchTuningLabel->setText( "Save With Tuning" );
   patchTuningLabel->sizeToFit();

   // fix the text selection rectangle background overhanging the borders on Windows
   #if WINDOWS
      patchName->setTextInset(CPoint(3, 0));
      patchCategory->setTextInset(CPoint(3, 0));
      patchCreator->setTextInset(CPoint(3, 0));
      patchComment->setTextInset(CPoint(3, 0));
   #endif   

   // Mouse behavior
   if (CSurgeSlider::sliderMoveRateState == CSurgeSlider::kUnInitialized)
      CSurgeSlider::sliderMoveRateState =
         (CSurgeSlider::MoveRateState)Surge::Storage::getUserDefaultValue(
            &(synth->storage), "sliderMoveRateState", (int)CSurgeSlider::kClassic);

   /*
    * There is, apparently, a bug in VSTGui that focus events don't fire reliably on some mac hosts.
    * This leads to the odd behaviour when you click out of a box that in some hosts - Logic Pro for
    * instance - there is no looseFocus event and so the value doesn't update. We could fix that
    * a variety of ways I imagine, but since we don't really mind the value being updated as we
    * go, we can just set the editors to immediate and correct the problem.
    *
    * See GitHub Issue #231 for an explanation of the behaviour without these changes as of Jan 2019.
    */
   patchName->setImmediateTextChange( true );
   patchCategory->setImmediateTextChange( true );
   patchCreator->setImmediateTextChange( true );
   patchComment->setImmediateTextChange( true );

   patchName->setBackColor(currentSkin->getColor( "savedialog.textfield.background", kWhiteCColor ));
   patchCategory->setBackColor(currentSkin->getColor( "savedialog.textfield.background", kWhiteCColor ));
   patchCreator->setBackColor(currentSkin->getColor( "savedialog.textfield.background", kWhiteCColor ));
   patchComment->setBackColor(currentSkin->getColor( "savedialog.textfield.background", kWhiteCColor ));

   patchName->setFontColor(currentSkin->getColor( "savedialog.textfield.foreground", kBlackCColor ));
   patchCategory->setFontColor(currentSkin->getColor( "savedialog.textfield.foreground", kBlackCColor ));
   patchCreator->setFontColor(currentSkin->getColor( "savedialog.textfield.foreground", kBlackCColor ));
   patchComment->setFontColor(currentSkin->getColor( "savedialog.textfield.foreground", kBlackCColor ));

   patchName->setFrameColor(currentSkin->getColor( "savedialog.textfield.border", kGreyCColor ));
   patchCategory->setFrameColor(currentSkin->getColor( "savedialog.textfield.border", kGreyCColor ));
   patchCreator->setFrameColor(currentSkin->getColor( "savedialog.textfield.border", kGreyCColor ));
   patchComment->setFrameColor(currentSkin->getColor( "savedialog.textfield.border", kGreyCColor ));

   CColor bggr = currentSkin->getColor( "savedialog.background", CColor(205,206,212) );
   patchTuningLabel->setBackColor(bggr);
   patchTuningLabel->setFrameColor(bggr);

   saveDialog->addView(patchName);
   saveDialog->addView(patchCategory);
   saveDialog->addView(patchCreator);
   saveDialog->addView(patchComment);
   saveDialog->addView(patchTuning);
   saveDialog->addView(patchTuningLabel);


   /*
   ** Skin Labels
   */
   auto labels = currentSkin->getLabels();
   for( auto &l : labels )
   {
      auto mtext = currentSkin->propertyValue( l, "text" );
      if( mtext.isJust() )
      {
         auto fss = currentSkin->propertyValue( l, "font-size", "12" );
         auto fs = std::atof( fss.c_str() );

         auto coln = currentSkin->propertyValue( l, "color", "#00FF00" );
         auto col = currentSkin->getColor( coln, kBlackCColor );
         
         auto lb = new CTextLabel( CRect( l->x, l->y, l->x + 100, l->y + 20 ), mtext.fromJust().c_str() );
         lb->setHoriAlign(VSTGUI::kLeftText);
         lb->setTransparency(true);
         lb->setFontColor(col);
         VSTGUI::SharedPointer<VSTGUI::CFontDesc> fnt = new VSTGUI::CFontDesc("Lato", fs);
         lb->setFont( fnt );
         lb->setAntialias( true );
         frame->addView(lb);
      }
   }
   
   refresh_mod();

   editor_open = true;
   queue_refresh = false;
   frame->setDirty();
   frame->invalid();
}

void SurgeGUIEditor::close_editor()
{
   editor_open = false;
   lfodisplay = 0;
   frame->removeAll(true);
   setzero(param);
}

VSTGUI::CControl *SurgeGUIEditor::layoutTagWithSkin( int tag )
{
   return nullptr;
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
   CRect wsize(0, 0, WINDOW_SIZE_X * fzf, WINDOW_SIZE_Y * fzf);
#else
   CRect wsize( 0, 0, WINDOW_SIZE_X, WINDOW_SIZE_Y );
#endif

   CFrame *nframe = new CFrame(wsize, this);

   bitmapStore.reset(new SurgeBitmaps());
   bitmapStore->setupBitmapsForFrame(nframe);
   currentSkin->reloadSkin(bitmapStore);
   nframe->setZoom(fzf);
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

   return true;
}

void SurgeGUIEditor::close()
{
#if TARGET_VST2 && WINDOWS
   // We may need this in other hosts also; but for now
   if (frame);
   {
      getFrame()->unregisterKeyboardHook(this);
      frame->close();
      frame = nullptr;
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
   Parameter* p = synth->storage.getPatch().param_ptr[index];

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
   /*if((button&kRButton)&&modsource)
     {
     modsource = 0;
     queue_refresh = true;
     return 1;
     }*/
   if (button & (kMButton | kButton4 | kButton5))
   {
      toggle_mod_editing();

      /*mod_editor = !mod_editor;
        blinktimer = 0.f;
        blinkstate = false;
        refresh_mod();*/
      return 1;
   }
   long tag = control->getTag();

   // In these cases just move along with success. RMB does nothing on these switches
   if( tag == tag_mp_jogfx || tag == tag_mp_category || tag == tag_mp_patch || tag == tag_store || tag == tag_store_cancel || tag == tag_store_ok )
   {
      return 1;
   }
   
   std::vector< std::string > clearControlTargetNames;

   if (button & kDoubleClick)
      button |= kControl;


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
               std::string hs = std::string( "[?] " ) + (char*)modsource_abberations[idOn];
               addCallbackMenu( contextMenu, hs, [lurl]()
                                                    {
                                                       Surge::UserInteractions::openURL(lurl);
                                                    }
                  );
               eid++;
            }
            else
            {
               contextMenu->addEntry((char*)modsource_abberations[idOn], eid++);
            }
            std::string offLab = "Switch to ";
            offLab += modsource_abberations[idOff];
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
         int sc = limit_range(synth->storage.getPatch().scene_active.val.i, 0, 1);
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
               sprintf(txt, "Clear Macro (%s ", txt4);
               addCallbackMenu(contextMenu,
                               Surge::UI::toOSCaseForMenu(txt) + midicc_names[synth->storage.controllers[ccid]] + ")",
                               [this, ccid]() {
                                                    synth->storage.controllers[ccid] = -1;
                                                    synth->storage.save_midi_controllers();
                                                 });
               eid++;
            }


            contextMenu->addSeparator(eid++);

            addCallbackMenu(contextMenu, "Bipolar", [this, control, ccid]() {
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

            addCallbackMenu(contextMenu, "Rename", [this, control, ccid]() {
                                                      VSTGUI::Call::later( [this, control, ccid]() {
                                                                              spawn_miniedit_text(synth->storage.getPatch().CustomControllerLabel[ccid] , 16,
                                                                                                  "Enter a new name for macro controller:", "Rename Macro");
                                                                              
                                                                              ((CModulationSourceButton*)control)
                                                                                 ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
                                                                              
                                                                              control->setDirty();
                                                                              control->invalid();
                                                                              synth->refresh_editor = true;
                                                                              synth->updateDisplay();
                                                                           }, 1 );
                                                   });
            eid++;

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

                  // these CCs cannot be used for MIDI learn (see SurgeSynthesizer::channelController)
                  if (mc == 0 || mc == 6 || mc == 32 || mc == 38 || mc == 64 || (mc == 74 && synth->mpeEnabled) ||(mc >= 98 && mc <= 101) || mc == 120 || mc == 123)
                     disabled = 1;

                  // we don't have any more CCs to cover, so break the loop
                  if (mc > 127)
                     break;

                  char name[128];

                  sprintf(name, "CC %d (%s) %s", mc, midicc_names[mc], (disabled == 1 ? "- RESERVED" : ""));

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

            contextMenu->addEntry(midiSub, Surge::UI::toOSCaseForMenu("Set Macro To..."));

#if TARGET_VST3            
            hostMenu = addVst3MenuForParams( contextMenu, modsource - ms_ctrl1 + metaparam_offset, eid);
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
   if (!(button & (kDoubleClick | kRButton)))
      return 0;

   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()) )
   {
      Parameter* p = synth->storage.getPatch().param_ptr[ptag];

      auto ctrl = dynamic_cast<CSurgeSlider*>(control);

      // don't show RMB context menu for filter subtype if it's hidden/not applicable
      auto f1type = synth->storage.getPatch().scene[current_scene].filterunit[0].type.val.i;
      auto f2type = synth->storage.getPatch().scene[current_scene].filterunit[1].type.val.i;

      if (tag == f1subtypetag && (f1type == fut_none || f1type == fut_SNH))
         return 1;
      if (tag == f2subtypetag && (f2type == fut_none || f2type == fut_SNH))
         return 1;

      if ((button & kRButton) && !(p->ctrltype == ct_lfoshape)) // for LFO let CLFOGui handle it
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
            auto fnmi = contextMenu->addEntry((char *)p->get_full_name(), eid++);
         }
         else
         {
            std::string helpstr = "[?] ";
            auto lurl = fullyResolvedHelpURL(helpurl);
            auto fnmi = addCallbackMenu(contextMenu, std::string(helpstr + p->get_full_name()).c_str(),
                                        [lurl]()
                                           {
                                              Surge::UserInteractions::openURL( lurl );
                                           }
               );
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
                               synth->setParameter01( p->id, !p->val.b, false, false );
                               synth->refresh_editor=true;
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
                                             synth->setParameter01( p->id, ef, false, false );
                                             synth->refresh_editor=true;
                                          }
                     );
                  eid++;
                  if( i == p->val.i )
                     b->setChecked(true);
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

               if( p->ctrlgroup == cg_LFO )
               {
                  char lab[256];
                  char prefix[32];

                  // WARNING - this won't work with Surge++
                  int a = p->ctrlgroup_entry + 1 - ms_lfo1;
                  if (a > 6)
                     sprintf(prefix, "SLFO %i", a - 6);
                  else
                     sprintf(prefix, "LFO %i", a);

                  bool setTSTo;
                  if( p->temposync )
                  {
                     snprintf(lab, 256, "Disable Tempo Sync for All %s Parameters", prefix);
                     setTSTo = false;
                  }
                  else
                  {
                     snprintf(lab, 256, "Enable Tempo Sync for All %s Parameters", prefix);
                     setTSTo = true;
                  }

                  addCallbackMenu( contextMenu, Surge::UI::toOSCaseForMenu(lab),
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

            {
               if (synth->learn_param > -1)
                  cancellearn = true;
               std::string learnTag =
                  cancellearn ? "Abort Parameter MIDI Learn" : "MIDI Learn Parameter...";
               addCallbackMenu(contextMenu, Surge::UI::toOSCaseForMenu(learnTag), [this, cancellearn, p] {
                                                         if (cancellearn)
                                                            synth->learn_param = -1;
                                                         else
                                                            synth->learn_param = p->id;
                                                      });
               eid++;
            }

            if (p->midictrl >= 0)
            {
               char txt4[16];
               decode_controllerid(txt4, p->midictrl);
               sprintf(txt, "Clear Learned MIDI (%s ", txt4);
               addCallbackMenu(contextMenu,
                               Surge::UI::toOSCaseForMenu(txt) + midicc_names[p->midictrl] + ")",
                               [this, p, ptag]() {
                                  // p->midictrl = -1;
                                  // TODO add so parameter for both scenes are cleared!
                                  if (ptag < n_global_params)
                                  {
                                     p->midictrl = -1;
                                     synth->storage.save_midi_controllers();
                                  }
                                  else
                                  {
                                     int a = ptag;
                                     if (ptag >= (n_global_params + n_scene_params))
                                        a -= ptag;

                                     synth->storage.getPatch().param_ptr[a]->midictrl = -1;
                                     synth->storage.getPatch().param_ptr[a + n_scene_params]->midictrl = -1;
                                     synth->storage.save_midi_controllers();
                                  }
                               });
               eid++;
            }

            int n_ms = 0;

            for (int ms = 1; ms < n_modsources; ms++)
               if (synth->isActiveModulation(ptag, (modsources)ms))
                  n_ms++;

            if (n_ms)
            {
               contextMenu->addSeparator(eid++);

               int detailedMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);

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
                     sprintf(tmptxt, "Clear %s -> %s", (char*)modulatorName(ms, true).c_str(),
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

#if TARGET_VST3
         auto hostMenu = addVst3MenuForParams(contextMenu, ptag, eid );
#endif         

         frame->addView(contextMenu); // add to frame
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget

#if TARGET_VST3
         if( hostMenu ) hostMenu->release();
#endif
         return 1;
      }
      else if (button & kControl)
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
            return 1;
         }
         else
         {
            /*
            ** This code resets you to default if you double click or ctrl click
            ** but on the lfoshape UI this is undesirable; it means if you accidentally
            ** control click on step sequencer, say, you go back to sin and lose your
            ** edits. So supress
            */
            if (p->ctrltype != ct_lfoshape)
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
      else
         return 0;
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

   
   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      if (((CModulationSourceButton*)control)->event_is_drag)
      {
         int t = (tag - tag_mod_source0);
         ((ControllerModulationSource*)synth->storage.getPatch().scene[current_scene].modsources[t])
            ->set_target01(control->getValue(), false);

         synth->sendParameterAutomation(t + metaparam_offset - ms_ctrl1, control->getValue());

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

               mod_editor = false;
               // mod_editor = true;
               queue_refresh = true;
               refresh_mod();
               break;
            case 1:
               modsource = newsource;
               mod_editor = true;
               refresh_mod();
               // queue_refresh = true;
               break;
            case 2:
               modsource = newsource;
               // modsource = 0;
               mod_editor = false;
               refresh_mod();
               // queue_refresh = true;
               break;
            };
         }
         //((gui_modsrcbutton*)control)->

         if (isLFO(newsource) && !(buttons & kShift))
         {
            if (modsource_editor != newsource)
            {
               modsource_editor = newsource;
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
      synth->release_if_latched[synth->storage.getPatch().scene_active.val.i] = true;
      synth->storage.getPatch().scene_active.val.i = (int)(control->getValue() * 1.f + 0.5f);
      // synth->storage.getPatch().param_ptr[scene_select_pid]->set_value_f01(control->getValue());
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
      return;
   }
   break;
   case tag_mp_category:
   {
      if (control->getValue() > 0.5f)
         synth->incrementCategory(true);
      else
         synth->incrementCategory(false);
      return;
   }
   break;
   case tag_mp_patch:
   {
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
      current_osc = (int)(control->getValue() * 2.f + 0.5f);
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

      patchName->setText(p.name.c_str());
      patchCategory->setText(p.category.c_str());
      patchCreator->setText(p.author.c_str());
      patchComment->setText(p.comments.c_str());

      showPatchStoreDialog(&p, &synth->storage.patch_category,
                           synth->storage.firstUserCategory);
   }
   break;
   case tag_store_cancel:
   {
      saveDialog->setVisible(false);
      // frame->setModalView(nullptr);
      frame->setDirty();
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
            typeinLabel->setFontColor( currentSkin->getColor( "savedialog.textlabel.error", kRedCColor ) );
         }

      }
   }
   break;
   case tag_store_ok:
   {
      saveDialog->setVisible(false);
      // frame->setModalView(nullptr);
      frame->setDirty();

      /*
      ** Don't allow a blank patch
      */
      std::string whatIsBlank = "";
      bool haveBlanks = false;

      if (! Surge::Storage::isValidName(patchName->getText().getString()))
      {
         whatIsBlank = "name"; haveBlanks = true;
      }
      if (! Surge::Storage::isValidName(patchCategory->getText().getString()))
      {
         whatIsBlank = whatIsBlank + (haveBlanks? " and category" : "category"); haveBlanks = true;
      }
      if (haveBlanks)
      {
         Surge::UserInteractions::promptError(std::string("Unable to store a patch due to invalid ") +
                                              whatIsBlank + ". Please save again and provide a complete " +
                                              whatIsBlank + ".",
                                              "Error Saving Patch");
      }
      else
      {
         synth->storage.getPatch().name = patchName->getText();
         synth->storage.getPatch().author = patchCreator->getText();
         synth->storage.getPatch().category = patchCategory->getText();
         synth->storage.getPatch().comment = patchComment->getText();

         synth->storage.getPatch().patchTuning.tuningStoredInPatch = patchTuning->getValue() > 0.5;
         if( synth->storage.getPatch().patchTuning.tuningStoredInPatch )
         {
            synth->storage.getPatch().patchTuning.tuningContents = synth->storage.currentScale.rawText;
            if( synth->storage.isStandardMapping )
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
   }
   break;
   default:
   {
      int ptag = tag - start_paramtags;

      int detailedMode = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);

      if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
      {
         Parameter* p = synth->storage.getPatch().param_ptr[ptag];

         char pname[256], pdisp[128], txt[128];
         bool modulate = false;

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

            synth->getParameterName(ptag, txt);
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
               auto nf = dynamic_cast<CNumberField *>(splitkeyControl);
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
            if (synth->setParameter01(ptag, val, false, force_integer))
            {
               queue_refresh = true;
               return;
            }
            else
            {
               synth->sendParameterAutomation(ptag, synth->getParameter01(ptag));

               if (dynamic_cast<CSurgeSlider*>(control) != nullptr)
                  ((CSurgeSlider*)control)->SetQuantitizedDispValue(p->get_value_f01());
               else
                  control->invalid();
               synth->getParameterName(ptag, pname);
               synth->getParameterDisplay(ptag, pdisp);
               char pdispalt[256];
               synth->getParameterDisplayAlt(ptag, pdispalt);
               ((CParameterTooltip*)infowindow)->setLabel(0, pdisp, pdispalt);
               ((CParameterTooltip*)infowindow)->clearMDIWS();
               if (p->ctrltype == ct_polymode)
                  modulate = true;
            }

            if( p->ctrltype == ct_bool_unipolar || p->ctrltype == ct_lfoshape )
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
         ((CEffectSettings*)ccfxconf)->set_bypass(synth->storage.getPatch().fx_bypass.val.i);

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

void SurgeGUIEditor::beginEdit(long index)
{
   if (index < start_paramtags)
   {
      return;
   }

   int externalparam = synth->remapInternalToExternalApiId(index - start_paramtags);

   if (externalparam >= 0)
   {
      super::beginEdit(externalparam);
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::endEdit(long index)
{
   if (index < start_paramtags)
   {
      return;
   }

   int externalparam = synth->remapInternalToExternalApiId(index - start_paramtags);

   if (externalparam >= 0)
   {
      super::endEdit(externalparam);
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
      synth->getParent()->ParameterBeginEdit(ptag);
   }
   else if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      int externalparam = synth->remapInternalToExternalApiId(ptag);
      if (externalparam >= 0)
      {
         synth->getParent()->ParameterBeginEdit(externalparam);
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
      synth->getParent()->ParameterEndEdit(ptag);
   }
   else if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      int externalparam = synth->remapInternalToExternalApiId(ptag);
      if (externalparam >= 0)
      {
         synth->getParent()->ParameterEndEdit(externalparam);
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
   if( r2.bottom > WINDOW_SIZE_Y - r.getHeight() - 2 )
   {
      // stick myself on top please
      r.offset((r2.left / 150) * 150, r2.top - r.getHeight() - 2);
      
   }
   else
   {
      r.offset((r2.left / 150) * 150, r2.bottom);
   }

   if (r.bottom > WINDOW_SIZE_Y - 2)
   {
      int ao = (WINDOW_SIZE_Y - 2 - r.bottom);
      r.offset(0, ao);
   }

   if (r.right > WINDOW_SIZE_X - 2)
   {
      int ao = (WINDOW_SIZE_X - 2 - r.right);
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

bool SurgeGUIEditor::showPatchStoreDialog(patchdata* p,
                                          std::vector<PatchCategory>* patch_category,
                                          int startcategory)
{
   if( synth->storage.isStandardTuning )
   {
      patchTuningLabel->setVisible(false);
      patchTuning->setVisible(false);
   }
   else
   {
      patchTuningLabel->setFontColor(currentSkin->getColor( "savedialog.textlabel", kBlackCColor ));
      patchTuningLabel->setVisible(true);
      patchTuning->setBoxFrameColor(currentSkin->getColor( "savedialog.checkbox.border", kBlackCColor ));
      patchTuning->setBoxFillColor(currentSkin->getColor( "savedialog.checkbox.fill", kWhiteCColor ));
      patchTuning->setCheckMarkColor(currentSkin->getColor( "savedialog.checkbox.tick", kBlackCColor));
      patchTuning->setValue(0);
      patchTuning->setMouseEnabled(true);
      patchTuning->setVisible(true);
   }

   saveDialog->setVisible(true);
   // frame->setModalView(saveDialog);

   return false;
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
   if( statuspanel )
      ((CStatusPanel *)statuspanel)->setDisplayFeature(CStatusPanel::mpeMode, this->synth->mpeEnabled );
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
         Surge::UserInteractions::promptError( e.what(), "Error When Toggling Tuning" );
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

   if( statuspanel )
      ((CStatusPanel *)statuspanel)->setDisplayFeature(CStatusPanel::tuningMode, !this->synth->storage.isStandardTuning );

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
      Surge::UserInteractions::promptError( e.what(), "Tuning Error Applying Scale" );
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
      Surge::UserInteractions::promptError( e.what(), "Tuning Error Applying Mapping" );
   }
}

bool SurgeGUIEditor::doesZoomFitToScreen(int zf, int &correctedZf)
{
   CRect screenDim = Surge::GUI::getScreenDimensions(getFrame());

   float baseW = WINDOW_SIZE_X;
   float baseH = WINDOW_SIZE_Y;

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
      oss << "Surge has recursed into setZoomFactor too many times. This indicates an error in the interaction "
          << "Surge, your Host's Zoom implementation, and your screen size. Please report this error to the "
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
      oss << "The smallest zoom on your platform is " << minimumZoom
          << "%. Sorry, you can't make Surge any smaller.";
      Surge::UserInteractions::promptError(oss.str(), "No Teensy Surge For You!");
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
      msg << "Surge limits the maximum zoom level in order to prevent the interface becoming larger than available screen area. "
          << "Your screen resolution is " << screenDim.getWidth() << "x" << screenDim.getHeight() << " "
          << "for which the target zoom level of " << zf << "% would be too large."
          << std::endl << std::endl
          << "Surge chose the largest fitting zoom level of " << zoomFactor << "%.";
      Surge::UserInteractions::promptError(msg.str(),
                                           "Zoom Level Limited");
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
       if (aboutbox)
          ((CAboutBox*)aboutbox)->boxShow(this->synth->storage.datapath, this->synth->storage.userDataPath);
    });
    eid++;

    frame->addView(settingsMenu);
    settingsMenu->setDirty();
    settingsMenu->popup();
    frame->removeView(settingsMenu, true);
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

    std::ostringstream oss;
    oss << "Change MPE Pitch Bend Range (Current: " << synth->mpePitchBendRange << " Semitones)";
    addCallbackMenu(mpeSubMenu, Surge::UI::toOSCaseForMenu(oss.str().c_str()), [this]() {
       // FIXME! This won't work on linux
       char c[256];
       snprintf(c, 256, "%d", synth->mpePitchBendRange);
       spawn_miniedit_text(c, 16, "Enter new MPE pitch bend range:", "MPE Pitch Bend Range");
       int newVal = ::atoi(c);
       this->synth->mpePitchBendRange = newVal;
    });

    std::ostringstream oss2;
    int def = Surge::Storage::getUserDefaultValue( &(synth->storage), "mpePitchBendRange", 48 );
    oss2 << "Change Default MPE Pitch Bend Range (Current: " << def << " Semitones)";
    addCallbackMenu(mpeSubMenu, Surge::UI::toOSCaseForMenu(oss2.str().c_str()), [this]() {
       // FIXME! This won't work on linux
       char c[256];
       snprintf(c, 256, "%d", synth->mpePitchBendRange);
       spawn_miniedit_text(c, 16, "Enter default MPE pitch bend range:", "Default MPE Pitch Bend Range");
       int newVal = ::atoi(c);
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "mpePitchBendRange", newVal);
       this->synth->mpePitchBendRange = newVal;
    });   
    
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
                    }
        );
    st->setEnabled(!this->synth->storage.isStandardTuning || tuningCacheForToggle.size() > 0);
    tid++;

    auto *kst = addCallbackMenu(tuningSubMenu, Surge::UI::toOSCaseForMenu("Set to Standard Keyboard Mapping"),
                    [this]()
                    {
                        this->synth->storage.remapToStandardKeyboard();
                        mappingCacheForToggle = "";
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
                            }
                            catch( Tunings::TuningError &e )
                            {
                               Surge::UserInteractions::promptError( e.what(), "Error Loading .scl" );
                            }
                        };

                        std::string scl_folder = "tuning-library";
                        #if WINDOWS
                           scl_folder += "\\SCL";
                        #else
                           scl_folder += "/SCL";
                        #endif

                        Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.datapath + scl_folder, ".scl", "Scala microtuning files (*.scl)", cb);
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
                            }
                            catch( Tunings::TuningError &e )
                            {
                               Surge::UserInteractions::promptError( e.what(), "Error Loading .kbm" );
                            }

                        };

                        std::string kbm_folder = "tuning-library";
                        #if WINDOWS
                           kbm_folder += "\\KBM Concert Pitch";
                        #else
                           kbm_folder += "/KBM Concert Pitch";
                        #endif

                        Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.datapath + kbm_folder, ".kbm", "Scala keyboard mapping files (*.kbm)", cb);
                    }
        );
    tid++;

    int oct = 5 - Surge::Storage::getUserDefaultValue(&(this->synth->storage), "middleC", 1);
    string middle_A = "A" + to_string(oct);
    
    addCallbackMenu( tuningSubMenu, Surge::UI::toOSCaseForMenu("Remap " + middle_A + " (MIDI note 69) directly to..."),
                     [this, middle_A]()
                        {
                           char ma[256];
                           sprintf(ma, "Remap %s Frequency", middle_A.c_str());

                           char c[256];
                           snprintf(c, 256, "440.0");
                           spawn_miniedit_text(c, 16, "Remap MIDI note 69 frequency to: ", ma);
                           float freq = ::atof(c);
                           auto kb = Tunings::tuneA69To(freq);
                           if( ! this->synth->storage.remapToKeyboard(kb) )
                           {
                              Surge::UserInteractions::promptError( "This .kbm file is not valid!", "File Format Error" );
                              return;
                           }
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
                       auto dpath = this->synth->storage.datapath;
                       std::string sep = "/";
#if WINDOWS
                       sep = "\\";
                       if( dpath.back() == '\\' || dpath.back() == '/' )
                         sep = "";
#endif
                       Surge::UserInteractions::openFolderInFileBrowser(
                          dpath + sep + "tuning-library");
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

    for (auto s : {100, 125, 150, 175, 200, 300, 400}) // These are somewhat arbitrary reasonable defaults
    {
        std::ostringstream lab;
        lab << "Zoom to " << s << "%";
        CCommandMenuItem* zcmd = new CCommandMenuItem(CCommandMenuItem::Desc(lab.str()));
        zcmd->setActions([this, s](CCommandMenuItem* m) { setZoomFactor(s); });
        zoomSubMenu->addEntry(zcmd);
        zid++;
    }

    zoomSubMenu->addSeparator(zid++);

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
                                                WINDOW_SIZE_X, WINDOW_SIZE_Y);
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

    auto dzf = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultZoom", this->zoomFactor );
    std::ostringstream dss;
    dss << "Zoom to Default (" << dzf << "%)";
    CCommandMenuItem* todefaultZ = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu(dss.str().c_str())));
    todefaultZ->setActions([this, dzf](CCommandMenuItem* m) { this->setZoomFactor(dzf); });
    zoomSubMenu->addEntry(todefaultZ);
    zid++;


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

    addCallbackMenu(
        zoomSubMenu, Surge::UI::toOSCaseForMenu("Set Default Zoom Level to..."), [this]() {
            // FIXME! This won't work on linux
            char c[256];
            snprintf(c, 256, "%d", this->zoomFactor);
            spawn_miniedit_text(c, 16, "Enter a default zoom level value:", "Set Default Zoom Level");
            int newVal = ::atoi(c);
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultZoom", newVal);
            this->setZoomFactor(newVal);
        });
    zid++;

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
   int pdid = 0;
   COptionMenu* patchDefMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                     VSTGUI::COptionMenu::kMultipleCheckStyle);

   VSTGUI::CCommandMenuItem* pdItem = nullptr;

   pdItem = addCallbackMenu(patchDefMenu, Surge::UI::toOSCaseForMenu("Set Default Patch Author..."), [this]() {
            string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchAuthor", "");
            char txt[256];
            txt[0] = 0;
            if( Surge::Storage::isValidUTF8( s ) )
               strncpy(txt, s.c_str(), 256);
            spawn_miniedit_text(txt, 256, "Enter default patch author name:", "Set Default Patch Author");
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultPatchAuthor", txt);
            });

   pdItem = addCallbackMenu(patchDefMenu, Surge::UI::toOSCaseForMenu("Set Default Patch Comment..."), [this]() {
            string s = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "defaultPatchComment", "");
            char txt[256];
            txt[0] = 0;
            if( Surge::Storage::isValidUTF8( s ) )
               strncpy(txt, s.c_str(), 256);
            spawn_miniedit_text(txt, 256, "Enter default patch comment text:", "Set Default Patch Comment");
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultPatchComment", txt);
            });

   uiOptionsMenu->addEntry(patchDefMenu, Surge::UI::toOSCaseForMenu("Patch Defaults"));
   patchDefMenu->forget();

   // high precision value readouts
   auto precReadout = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", 0);

   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("High Precision Value Readouts"), [this, precReadout]() {
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "highPrecisionReadouts", precReadout ? 0 : 1);
   });
   menuItem->setChecked(precReadout);

   auto modValues = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "modWindowShowsValues", 0);

   menuItem = addCallbackMenu(uiOptionsMenu, Surge::UI::toOSCaseForMenu("Modulation Popup Shows Bounds"), [this, modValues]() {
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "modWindowShowsValues", modValues? 0 : 1);
   });
   menuItem->setChecked(modValues);

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
    for( auto &entry : db.getAvailableSkins() )
    {
       if( entry.name.rfind( "test", 0 ) == 0 )
       {
          hasTests = true;
       }
       else
       {
          auto dname = entry.displayName;
          if( useDevMenu )
          {
             dname += "(";
             if( entry.root.find( synth->storage.datapath ) != std::string::npos )
             {
                dname += "factory/";
             }
             else if( entry.root.find( synth->storage.userDataPath ) != std::string::npos )
             {
                dname += "user/";
             }
             else
             {
                dname += "other/";
             }
             dname += entry.name;
             
             dname += ")";
          }

          auto cb = addCallbackMenu(skinSubMenu, dname,
                                    [this, entry]() {
                                       setupSkinFromEntry(entry);
                                       this->synth->refresh_editor = true;
                                       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultSkin", entry.name );
                                    });
          cb->setChecked( entry.matchesSkin( currentSkin ) );
          tid++;
       }
    }


    if( hasTests && useDevMenu )
    {
       COptionMenu *testSM = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);
       for( auto &entry : db.getAvailableSkins() )
       {
          if( entry.name.rfind( "test", 0 ) == 0 )
             addCallbackMenu(testSM, entry.displayName,
                             [this, entry]() {
                                setupSkinFromEntry(entry);
                                this->synth->refresh_editor = true;
                                Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultSkin", entry.name );
                             });

       }
       skinSubMenu->addEntry( testSM, Surge::UI::toOSCaseForMenu("Test Skins"));
       testSM->forget();
    }

    skinSubMenu->addSeparator(tid++);

    addCallbackMenu(skinSubMenu, Surge::UI::toOSCaseForMenu("Reload Current Skin"),
                    [this]() {
                       this->bitmapStore.reset(new SurgeBitmaps());
                       this->bitmapStore->setupBitmapsForFrame(frame);
                       if( ! this->currentSkin->reloadSkin(this->bitmapStore) )
                       {
                          auto &db = Surge::UI::SkinDB::get();
                          auto msg = std::string( "Unable to load skin. Returning to default. Skin error was '" )
                             + db.getAndResetErrorString() + "'";
                          this->currentSkin = db.defaultSkin( &( this->synth->storage ) );
                          this->currentSkin->reloadSkin(this->bitmapStore);
                          Surge::UserInteractions::promptError( msg, "Skin Error" );
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
      fs::create_directories(this->synth->storage.userDataPath);
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

VSTGUI::COptionMenu* SurgeGUIEditor::makeMidiMenu(VSTGUI::CRect& menuRect)
{
   int did = 0;
   COptionMenu* midiSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                              VSTGUI::COptionMenu::kNoDrawStyle |
                                                  VSTGUI::COptionMenu::kMultipleCheckStyle);

   addCallbackMenu(
       midiSubMenu, Surge::UI::toOSCaseForMenu("Save MIDI Mapping As..."),
       [this]() {
          VSTGUI::Call::later(
             [this]() {
                this->scannedForMidiPresets = false; // force a rescan
                char msn[256];
                msn[0] = 0;
                spawn_miniedit_text(msn, 128, "MIDI Mapping Name", "Save MIDI Mapping");
                if( strlen( msn ) > 0 )
                   this->synth->storage.storeMidiMappingToName( msn );
             }, 1 );
       });
   did++;

   addCallbackMenu(
      midiSubMenu, Surge::UI::toOSCaseForMenu( "Show Current MIDI Mapping..." ),
      [this]() {
         Surge::UserInteractions::showHTML( this->midiMappingToHtml() );
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
         did++;
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

   CScalableBitmap *cbm = bitmapStore->getBitmap( IDB_BG );
   cbm->setExtraScaleFactor(getZoomFactor());
   frame->setBackground( cbm );

   auto c = currentSkin->getColor( "textfield.focuscolor", VSTGUI::CColor( 170, 170, 230 ) );
   frame->setFocusColor( c );

}

/*
** The DEV menu is almost always off. @baconpaul just activates it above when he wants
** to do stuff in the gui. We keep this code kicking around though in case we need it
** even though it isn't callable in the production UI
*/
VSTGUI::COptionMenu *SurgeGUIEditor::makeDevMenu(VSTGUI::CRect &menuRect)
{
    int tid = 0;

    COptionMenu *devSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

#if WINDOWS
    VSTGUI::CCommandMenuItem* conItem = nullptr;

    static bool consoleState;

    conItem = addCallbackMenu(devSubMenu, "Debug console",
        []() {
                consoleState = Surge::Debug::toggleConsole();
             });
        conItem->setChecked(consoleState);
    tid++;
#endif

#ifdef INSTRUMENT_UI
    addCallbackMenu(devSubMenu, Surge::UI::toOSCaseForMenu("Show UI Instrumentation"),
                    []() {
                       Surge::Debug::report();
                    }
       );
    tid++;
#endif

    addCallbackMenu(devSubMenu, Surge::UI::toOSCaseForMenu("Show Queried Colors"),
                    [this]()
                       {
                          auto qc = this->currentSkin->getQueriedColors();
                          std::set<std::string> ss( qc.begin(), qc.end() );
                          std::ostringstream htmls;
                          htmls << "<html><body><h1>Color Tags</h1>\n<table border=1><tr><th>tag</th><th>value in skin</th></tr>\n";
                          for( auto s : ss )
                          {
                             htmls << "<tr><td>" << s << "</td><td>";
                             if( this->currentSkin->hasColor(s) )
                             {
                                auto c = this->currentSkin->getColor(s,kBlackCColor);
                                htmls << "#" << std::hex << (int)c.red << (int)c.green << (int)c.blue << std::dec << std::endl;
                             }
                             else
                             {
                                htmls << "default" << std::endl;
                             }
                             htmls << "</td></tr>\n";
                          }
                          htmls << "</pre></body></html>\n";
                          Surge::UserInteractions::showHTML( htmls.str() );
                       }
       );
    tid++;

    /*
    ** This code takes a running surge and makes an XML file we can use to bootstrap the layout manager.
    ** It as used as we transitioned from 1.6.5 to 1.7 to do the first layout file which matched
    ** surge exactly
    */
    addCallbackMenu(devSubMenu, Surge::UI::toOSCaseForMenu("Dump Components to stdout and runtime.xml"),
                    [this]()
                    {
                       std::ostringstream oss;
                       auto leafOp =
                          [this, &oss](CView *v) {
                             CControl *c = dynamic_cast<CControl *>(v);
                             if( c )
                             {
                                auto tag = c->getTag();

                                // Some cases where we don't want to eject the control
                                if( tag >= tag_store && tag <= tag_store_tuning )
                                   return;
                                if( dynamic_cast<CAboutBox*>( c ) )
                                   return;
                                if( dynamic_cast<COptionMenu*>( c ) )
                                   return;
                                if( dynamic_cast<CParameterTooltip*>( c ) )
                                   return;
                                if( dynamic_cast<CEffectLabel*>( c ) )
                                   return;
                                if( c == this->patchTuningLabel )
                                   return;

                                // OK so we want to eject this one. Lets get to work
                                Parameter *p = nullptr;
                                if( tag >= start_paramtags )
                                {
                                   p = this->synth->storage.getPatch().param_ptr[tag - start_paramtags];
                                }
                                oss << "    <control " ;
                                if( p )
                                {
                                   oss << "ui_identifier=\""  <<  p->ui_identifier << "\"";
                                }
                                else if( c->getTag() >= tag_scene_select && c->getTag() < start_paramtags )
                                {
                                   oss << "tag_value=\"" << c->getTag() << "\" tag_name=\"" << specialTagToString( (special_tags)(c->getTag()) ) << "\"";
                                }
                                else
                                {
                                   oss << "special=\"true\" ";
                                }
                                auto vs = c->getViewSize();
                                oss << " x=\"" << vs.left << "\" y=\"" << vs.top
                                          << "\" w=\"" << vs.getWidth() << "\" h=\"" << vs.getHeight() << "\"";

                                /*
                                if( p )
                                   oss << " posx=\"" << p->posx << "\" "
                                       << " posy=\"" << p->posy << "\" "
                                       << " posy_offset=\"" << p->posy_offset << "\" ";
                                */

                                auto dumpBG = [&oss](CControl *a) {
                                                 auto bg = a->getBackground();
                                                 auto rd = bg->getResourceDescription();
                                                 if( rd.type == CResourceDescription::kIntegerType )
                                                 {
                                                    ios::fmtflags f(oss.flags());
                                                    oss << " bg_resource=\"SVG/bmp" << std::setw(5) << std::setfill('0') << rd.u.id << ".svg\" ";
                                                    oss << " bg_id=\""  << rd.u.id << "\" ";
                                                    oss.flags(f);
                                                 }
                                              };

                                if( auto a = dynamic_cast<CHSwitch2 *>(c) )
                                {
                                   oss << " class=\"CHSwitch2\" ";
                                   dumpBG(a);
                                   oss << " subpixmaps=\"" << a->getNumSubPixmaps()
                                       << "\" rows=\"" << a->rows
                                       << "\" columns=\"" << a->columns << "\"";
                                }
                                else if( auto a = dynamic_cast<CSurgeSlider *>(c) )
                                {
                                   if( p->ctrlstyle & Surge::ParamConfig::kHorizontal )
                                   {
                                      if( p->ctrlstyle & kWhite )
                                         oss << " class=\"light-hslider\"";
                                      else
                                         oss << " class=\"dark-hslider\"";
                                   }
                                   else
                                   {
                                      if( p->ctrlstyle & kWhite )
                                         oss << " class=\"light-vslider\"";
                                      else
                                         oss << " class=\"dark-vslider\"";
                                   }

                                }
                                else if( auto a = dynamic_cast<CSwitchControl *>(c) )
                                {
                                   oss << " class=\"CSwitchControl\" ";
                                   dumpBG(a);
                                }
                                else if( auto a = dynamic_cast<CModulationSourceButton *>(c) )
                                {
                                   oss << " class=\"CModulationSourceButton\" ";
                                }
                                else if( auto a = dynamic_cast<CNumberField *>(c) )
                                {
                                   oss << " class=\"CNumberField\" ";
                                   // want to collect controlmode and altlook
                                }
                                else if( auto a = dynamic_cast<COscillatorDisplay *>(c) )
                                {
                                   oss << " class=\"COscillatorDisplay\" ";
                                }
                                else if( auto a = dynamic_cast<CStatusPanel *>(c) )
                                {
                                   oss << " class=\"CStatusPanel\" ";
                                }
                                else if( auto a = dynamic_cast<CPatchBrowser *>(c) )
                                {
                                   oss << " class=\"CPatchBrowser\" ";
                                }
                                else if( auto a = dynamic_cast<CSurgeVuMeter *>(c) )
                                {
                                   oss << " class=\"CSurgeVuMeter\" ";
                                }
                                else if( auto a = dynamic_cast<CLFOGui *>(c) )
                                {
                                   oss << " class=\"CLFOGui\" ";
                                }
                                else if( auto a = dynamic_cast<CEffectSettings *>(c) )
                                {
                                   oss << " class=\"CEffectSettings\" ";
                                }
                                else
                                {
                                   oss << " rtti_class=\"" << typeid(*c).name() << "\" tag=\"" << tag << "\" ";
                                }
                                oss << "/>\n";
                             }
                             else
                             {
                                // oss << "Something Else " << typeid(*v).name() << std::endl;
                                // this is always a container and it is recurse by the stack thingy below
                             }
                          };

                       std::stack<CView*> stk;
                       stk.push( this->frame );
                       oss << "<surge-layout>\n";
                       oss << "  <globals>\n";
                       oss << "  </globals\n";

                       oss << "  <component-classes>\n";
                       oss << "    <class name=\"dark-vslider\" parent=\"CSurgeSlider\" orientation=\"vertical\" white=\"false\"/>\n";
                       oss << "    <class name=\"light-vslider\" parent=\"CSurgeSlider\" orientation=\"vertical\" white=\"true\"/>\n";
                       oss << "    <class name=\"dark-hslider\" parent=\"CSurgeSlider\" orientation=\"horizontal\" white=\"false\"/>\n";
                       oss << "    <class name=\"light-hslider\" parent=\"CSurgeSlider\" orientation=\"horizontal\" white=\"true\"/>\n";
                       oss << "  </component-classes>\n\n";

                       oss << "  <controls>\n";
                       while( ! stk.empty() )
                       {
                          auto e = stk.top();
                          stk.pop();
                          leafOp(e);
                          auto c = dynamic_cast<CViewContainer *>(e);
                          if( c )
                          {
                             c->forEachChild( [&stk](CView *cc) { stk.push(cc); } );
                             // oss << "Added " << stk.size() - b << " elements" << std::endl;
                          }
                       }
                       oss << "  </controls>" << std::endl;
                       oss << "</surge-layout>\n";

                       std::cout << oss.str();
                       std::ofstream of( "runtime.xml" );
                       of << oss.str();
                       of.close();
                    }
        );
    tid++;

    addCallbackMenu(
        devSubMenu,
        Surge::UI::toOSCaseForMenu("Dump Documentation XML to stdout and docstrings.xml"),
                    [this]()
                       {
                          std::ostringstream oss;

                          oss << "<param-doc>\n";

#define OSSCG(x) oss << "  <ctrl_group group=\"" << x << "\" group_name=\"" << (#x) << "\" help_url=\"\"/>\n";
                          OSSCG( cg_GLOBAL );
                          OSSCG( cg_OSC );
                          OSSCG( cg_MIX );
                          OSSCG( cg_FILTER );
                          OSSCG( cg_ENV );
                          OSSCG( cg_LFO );
                          OSSCG( cg_FX );

                          for( auto cg = (int)cg_GLOBAL; cg < (int)endCG; ++cg )
                          {
                             std::set<std::string> pn;
                             for( auto p : this->synth->storage.getPatch().param_ptr )
                             {
                                if( p->ctrlgroup == cg )
                                {
                                   pn.insert( p->ui_identifier );
                                }
                             }
                             if( pn.size() )
                             {
                                oss << "\n  <!-- Params for CG " << (int)cg << " -->\n";
                                for( auto el : pn )
                                {
                                   oss << "  <param id=\"" << el << "\" help_url=\"\"/>\n";
                                }

                                if( cg == cg_OSC )
                                {
                                   for( int i=ot_classic; i<ot_WT2; ++i )
                                   {
                                      switch(i)
                                      {
                                      case ot_classic:
                                         oss << "\n  <!-- special case oscillator params for Classic ";
                                         break;
                                      case ot_sinus:
                                         oss << "\n  <!-- special case oscillator params for Sine ";
                                         break;
                                      case ot_wavetable:
                                         oss << "\n  <!-- special case oscillator params for Wavetable ";
                                         break;
                                      case ot_shnoise:
                                         oss << "\n  <!-- special case oscillator params for S&H Noise ";
                                         break;
                                      case ot_audioinput:
                                         oss << "\n  <!-- special case oscillator params for Audio Input ";
                                         break;
                                      case ot_FM:
                                         oss << "\n  <!-- special case oscillator params for FM3 ";
                                         break;
                                      case ot_FM2:
                                         oss << "\n  <!-- special case oscillator params for FM2 ";
                                         break;
                                      case ot_WT2:
                                         oss << "\n  <!-- special case oscillator params for Window ";
                                         break;
                                      default:
                                         oss << "\n  <!-- ERROR ";
                                         break;
                                      }
                                      oss << "(" << i << ") -->\n";
                                      for( auto n : pn )
                                      {
                                         if( n.find( "param" ) != string::npos )
                                         {
                                            oss << "  <param id=\"" << n << "\" type=\"" << i << "\" help_url=\"\"/>\n";
                                         }
                                      }
                                   }
                                }
                                if( cg == cg_FX )
                                {
                                   for( int i=fxt_delay; i<num_fxtypes; ++i )
                                   {
#define CFX(x) case x: { oss << "\n  <!-- special case FX Param for " << (#x); break; }
                                      switch(i)
                                      {
                                         CFX(fxt_delay)
                                            CFX(fxt_reverb)
                                            CFX(fxt_phaser)
                                            CFX(fxt_rotaryspeaker)
                                            CFX(fxt_distortion)
                                            CFX(fxt_eq)
                                            CFX(fxt_freqshift)
                                            CFX(fxt_conditioner)
                                            CFX(fxt_chorus4)
                                            CFX(fxt_vocoder)
                                            CFX(fxt_reverb2)
                                            CFX(fxt_flanger)
                                            CFX(fxt_ringmod)

                                      default:
                                         oss << "\n  <!-- ERROR ";
                                         break;
                                      }
                                      oss << "(" << i << ") -->\n";
                                      for( auto n : pn )
                                      {
                                         if( n.find( "param" ) != string::npos )
                                         {
                                            oss << "  <param id=\"" << n << "\" type=\"" << i << "\" help_url=\"\"/>\n";
                                         }
                                      }
                                   }
                                }

                             }
                          }
                          
                          oss << "</param-doc>\n";
                          
                          std::cout << oss.str();
                          std::ofstream of( "docstrings.xml" );
                          of << oss.str();
                          of.close();
                       }
       );
    tid++;
    
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
   float izfx = newSize->getWidth() * 1.0 / WINDOW_SIZE_X * 100.0;
   float izfy = newSize->getHeight() * 1.0 / WINDOW_SIZE_Y * 100.0;
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
   auto zfdx = std::fabs( (izf - zoomFactor) * WINDOW_SIZE_X ) / 100.0;
   auto zfdy = std::fabs( (izf - zoomFactor) * WINDOW_SIZE_Y ) / 100.0;
   auto zfd = std::max( zfdx, zfdy );

   if( zfd > 1 )
   {
      setZoomFactor( izf );
   }

   return Steinberg::Vst::VSTGUIEditor::onSize(newSize);
}
Steinberg::tresult PLUGIN_API SurgeGUIEditor::checkSizeConstraint(Steinberg::ViewRect* newSize)
{
   // float tratio = 1.0 * WINDOW_SIZE_X / WINDOW_SIZE_Y;
   // float cratio = 1.0 * newSize->getWidth() / newSize->getHeight();

   // we want cratio == tration by adjusting height so
   // WSX / WSY = gW / gH
   // gH = gW * WSY / WSX

   float newHeight = 1.0 * newSize->getWidth() * WINDOW_SIZE_Y / WINDOW_SIZE_X;
   newSize->bottom = newSize->top + std::ceil(newHeight);
   return Steinberg::kResultTrue;
}

#endif

void SurgeGUIEditor::forceautomationchangefor(Parameter *p)
{
#if TARGET_LV2
   // revisit this for non-LV2 outside 1.6.6
   synth->sendParameterAutomation(p->id, synth->getParameter01(p->id));
#endif
}
//------------------------------------------------------------------------------------------------

std::string SurgeGUIEditor::tuningToHtml()
{
   std::ostringstream htmls;

   htmls <<
      R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="https://fonts.googleapis.com/css?family=Lato" />
    <style>
table {
  border-collapse: collapse;
}

td {
  border: 1px solid #CDCED4;
  padding: 2pt;
}

th {
  padding: 4pt;
  color: #123463;
  background: #CDCED4;
  border: 1px solid #123463;
}
</style>
  </head>
  <body style="margin: 0pt; background: #CDCED4;">
    <div style="border-bottom: 1px solid #123463; background: #ff9000; padding: 2pt;">
      <div style="font-size: 20pt; font-family: Lato; padding: 2pt; color:#123463;">
        Surge Tuning
      </div>
      <div style="font-size: 12pt; font-family: Lato; padding: 2pt;">
    )HTML"
       << synth->storage.currentScale.description <<
    R"HTML(
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
        Tuning Information
     )HTML";

    if( ! synth->storage.isStandardMapping )
    {
       htmls << "<ul>\n"
             << "<li><a href=\"#rawscl\">Jump to raw SCL</a>.\n"
             << "<li><a href=\"#rawkbm\">Jump to raw KBM</a>.\n"
             << "<li>Scale position 0 maps to key "
             << synth->storage.currentMapping.middleNote << "\n<li> MIDI note " << synth->storage.currentMapping.tuningConstantNote << " is set to frequency "
             << synth->storage.currentMapping.tuningFrequency << "Hz.\n</ul> ";
    }
    else
    {
       htmls << "\n<div>Tuning uses standard keyboard mapping.\n"
             << "<ul>\n"
             << "<li><a href=\"#rawscl\">Jump to raw SCL</a>.\n"
             << "</ul>\n";
    }

    htmls << R"HTML(
      </div>
      <div style="font-size: 12pt; font-family: Lato;">
        <div style="padding-bottom: 10pt;">
        )HTML" << synth->storage.currentScale.count << " tones" <<
R"HTML(
    </div>
        <table>
          <tr>
            <th>#</th><th>Datum</th><th>Cents</th><th>Float</th>
          </tr>
          <tr>
            <td>0</td><td>1</td><td>0</td><td>1</td>
          </tr>
    )HTML";

    int ct = 1;
    for( auto & t : synth->storage.currentScale.tones )
    {
       htmls << "<tr><td> " << ct++ << "</td><td>";
       if (t.type == Tunings::Tone::kToneCents)
          htmls << t.cents;
       else
          htmls << t.ratio_n << " / " << t.ratio_d;

       htmls << "</td><td>" << t.cents << "</td><td>" << t.floatValue << "</td></tr>\n";
    };

    htmls << R"HTML(
        </table>

        <p>
)HTML";



    htmls << R"HTML(
<p>
        <table>
          <tr>
            <th colspan=2>MIDI Note</th><th>Scale Position</th><th>Frequency</th>
          </tr>

)HTML";

       for( int i=0; i<128; ++i )
       {
          int oct_offset = 1;
             oct_offset = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "middleC", 1);
          char notename[16];

          std::string rowstyle="";
          std::string tdopen="<td colspan=2>";
          int np = i%12;
          if( np == 1 || np == 3 || np == 6 || np ==8 || np == 10 )
          {
             rowstyle = "style=\"background-color: #dddddd;\"";
             tdopen="<td style=\"background-color: #ffffff;\">&nbsp;</td><td>";
          }
          htmls << "<tr " << rowstyle << ">" << tdopen << i << " (" << get_notename(notename, i, oct_offset) << ")</td>\n";

          auto tn = i - synth->storage.scaleConstantNote();
          if( ! synth->storage.isStandardMapping )
          {
             tn = i - synth->storage.currentMapping.middleNote;
          }
          while( tn < 0 ) tn += synth->storage.currentScale.count;

          auto p = synth->storage.note_to_pitch(i);
          htmls << "<td>" << (tn % synth->storage.currentScale.count + 1) << "</td><td>" << 8.175798915 * p << " Hz</td>";
          htmls << "</tr>\n";
       }

       htmls << R"HTML(
        </table>
      </div>

    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; font-family: Lato; color: #123463;">
        <a name="rawscl">Tuning Raw File</a>:
           )HTML" << synth->storage.currentScale.name << "</div>\n<pre>\n" << synth->storage.currentScale.rawText << R"HTML(
      </pre>
    </div>
)HTML";

       if( ! synth->storage.isStandardMapping )
       {
          htmls << R"HTML(
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; font-family: Lato; color: #123463;">
        <a name="rawkbm">Keyboard Mapping Raw File</a>:
           )HTML" << synth->storage.currentMapping.name << "</div>\n<pre>\n" << synth->storage.currentMapping.rawText << R"HTML(
      </pre>
    </div>
)HTML";
       }
       htmls << R"HTML(
  </body>
</html>
      )HTML";

  return htmls.str();
}

std::string SurgeGUIEditor::midiMappingToHtml()
{
   std::ostringstream htmls;

   htmls <<
      R"HTML(
<html>
  <head>
    <link rel="stylesheet" type="text/css" href="https://fonts.googleapis.com/css?family=Lato" />
    <style>
table {
  border-collapse: collapse;
}

td {
  border: 1px solid #CDCED4;
  padding: 2pt 4px;
}

.center {
  text-align: center;
}

th {
  padding: 4pt;
  color: #123463;
  background: #CDCED4;
  border: 1px solid #123463;
}
</style>
  </head>
  <body style="margin: 0pt; background: #CDCED4;">
    <div style="border-bottom: 1px solid #123463; background: #ff9000; padding: 2pt;">
      <div style="font-size: 20pt; font-family: Lato; padding: 2pt; color:#123463;">
        Surge MIDI Mapping
      </div>
    </div>

    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">

     )HTML";
   // TODO - if there are none print differently
   bool foundOne = false;
   int n = n_global_params + n_scene_params;
   for (int i = 0; i < n; i++)
   {
      if (synth->storage.getPatch().param_ptr[i]->midictrl >= 0)
      {
         if( ! foundOne )
         {
            foundOne = true;
            htmls << "Individual parameter MIDI mappings<p>\n"
                  << "<table><tr><th>CC#</th><th>Parameter</th></tr>\n";
         }
         htmls << "<tr><td class=\"center\">" << synth->storage.getPatch().param_ptr[i]->midictrl << "</td><td> "
               << synth->storage.getPatch().param_ptr[i]->get_full_name() << "</td></tr>\n";
      }
   }
   if( foundOne )
   {
      htmls << "</table>\n";
   }
   else
   {
      htmls << "No parameter MIDI mappings present!";
   }
   
   
   htmls << R"HTML(

      </div>
    </div>
    <div style="margin:10pt; padding: 5pt; border: 1px solid #123463; background: #fafbff;">
      <div style="font-size: 12pt; margin-bottom: 10pt; font-family: Lato; color: #123463;">
         Macro Assignments<p>
         <table><tr><th>CC#</th><th>Macro</th><th>Custom Name</th></tr>
     )HTML";
   for( int i=0; i<n_customcontrollers; ++i )
   {
      string name = synth->storage.getPatch().CustomControllerLabel[i];
      auto ccval = synth->storage.controllers[i];

      htmls << "<tr><td class=\"center\">" << (ccval == -1 ? "N/A" : to_string(ccval)) << "</td><td class=\"center\">" << i + 1 << "</td><td>" << name << "</td></tr>" << std::endl;
   }
   htmls << R"HTML(
         </table>
      </div>
    </div>
  </body>
</html>
      )HTML";

  return htmls.str();
}

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
      boxht += 12;

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
   typeinDialog->setBackgroundColor(currentSkin->getColor( "savedialog.border", VSTGUI::kBlackCColor) );
   typeinDialog->setVisible(false);
   frame->addView(typeinDialog);

   CRect innerSize( 0, 0, typeinSize.getWidth(), typeinSize.getHeight() );
   innerSize.inset( 1, 1 );
   auto inner = new CViewContainer(innerSize);
   CColor bggr = currentSkin->getColor( "savedialog.background", CColor(205,206,212) );
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
   typeinLabel->setFontColor(currentSkin->getColor( "slider.dark.label", kBlackCColor ));
   typeinLabel->setTransparency(true);
   typeinLabel->setFont( displayFont );
   inner->addView(typeinLabel);

   char txt[256];
   char ptext[1024];
   if( p )
   {
      if( ismod )
      {
         char txt2[256];
         p->get_display_of_modulation_depth(txt, synth->getModDepth(p->id, (modsources)ms), synth->isBipolarModulation((modsources)ms), Parameter::TypeIn);
         p->get_display(txt2);
         sprintf( ptext, "%s from %s", txt, txt2 );
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
      ml->setFontColor(currentSkin->getColor( "slider.light.label", kBlackCColor ));
      ml->setTransparency(true);
      ml->setFont( displayFont );
      inner->addView(ml);
   }

   
   typeinPriorValueLabel = new CTextLabel(CRect(2, 29 - (ismod ? 0 : 23), 116, 36 + ismod), ptext);
   typeinPriorValueLabel->setFontColor(currentSkin->getColor( "slider.light.label", kBlackCColor ));
   typeinPriorValueLabel->setTransparency(true);
   typeinPriorValueLabel->setFont( displayFont );
   inner->addView(typeinPriorValueLabel);

   typeinValue = new CTextEdit( CRect( 4, 31 + ( ismod ? 12 : 0 ), 114, 50 + ( ismod ? 12 : 0 ) ), this, tag_value_typein, txt );
   typeinValue->setBackColor(currentSkin->getColor( "savedialog.textfield.background", kWhiteCColor ));
   typeinValue->setFontColor(currentSkin->getColor( "savedialog.textfield.foreground", kBlackCColor ));

   // fix the text selection rectangle background overhanging the borders on Windows
   #if WINDOWS
   typeinValue->setTextInset(CPoint(3, 0));
   #endif   

   if( p )
   {
      if( ! p->can_setvalue_from_string() )
      {
         typeinValue->setFontColor( VSTGUI::kRedCColor );
         typeinValue->setText( "edit coming soon" );
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

      if( lfodata->shape.val.i == ls_constant1 )
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sENV %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s Envelope %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
      else if( lfodata->shape.val.i == ls_stepseq )
      {
         char txt[64];
         if( button )
            sprintf( txt, "%sSEQ %d", (isS ? "S-" : "" ), fnum + 1 );
         else
            sprintf( txt, "%s Step Sequencer %d", (isS ? "Scene" : "Voice" ), fnum + 1 );
         return std::string( txt );
      }
   }

   if( i >= ms_ctrl1 && i <= ms_ctrl8 )
   {
      std::string ccl = std::string(synth->storage.getPatch().CustomControllerLabel[i - ms_ctrl1]);
      if( ccl == "-" )
      {
         return std::string( modsource_abberations[i] );
      }
      else
      {
         return ccl + " (" + modsource_abberations[i] + ")";
      }
   }
   if( button )
      return std::string( modsource_abberations_button[i] );
   else
      return std::string( modsource_abberations[i] );

}

#ifdef TARGET_VST3
Steinberg::Vst::IContextMenu* SurgeGUIEditor::addVst3MenuForParams(VSTGUI::COptionMenu *contextMenu, int ptag, int &eid)
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
      
      Steinberg::Vst::ParamID param = ptag;
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
         
         char nm[1024];
         Steinberg::UString128(item.name, 128).toAscii(nm, 1024);
         if( nm[0] == '-' ) // FL sends us this as a separator with no VST indication so just strip the '-'
         {
            int pos = 1;
            while( nm[pos] == ' ' && nm[pos] != 0 )
               pos++;
            std::string truncName( nm + pos );
            strcpy( nm, truncName.c_str() );
         }
         
         int itag = item.tag;
         /*
         ** Leave this here so we can debug if another vst3 problem comes up
         std::cout << nm << " FL=" << item.flags << " jGS=" << Steinberg::Vst::IContextMenuItem::kIsGroupStart
         << " and=" << ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart )
         << " IGS="
         << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart ) == Steinberg::Vst::IContextMenuItem::kIsGroupStart ) << " IGE="
         << ( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) == Steinberg::Vst::IContextMenuItem::kIsGroupEnd ) << " "
         << std::endl;
         */
         if( item.flags & Steinberg::Vst::IContextMenuItem::kIsSeparator )
         {
            menuStack.top()->addSeparator(eidStack.top()++);
         }
         else if( ( item.flags & Steinberg::Vst::IContextMenuItem::kIsGroupStart ) == Steinberg::Vst::IContextMenuItem::kIsGroupStart )
         {
            COptionMenu *subMenu = new COptionMenu( menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle |
                                                    VSTGUI::COptionMenu::kMultipleCheckStyle );
            menuStack.top()->addEntry(subMenu, nm );
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
      type = storage->getPatch().scene[current_scene].osc[current_osc].type.val.i;
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
      auto msg = std::string( "Unable to load skin. Returning to default. Skin error was '" )
         + db.getAndResetErrorString() + "'";
      this->currentSkin = db.defaultSkin( &( this->synth->storage ) );
      this->currentSkin->reloadSkin(this->bitmapStore);
      Surge::UserInteractions::promptError( msg, "Skin Error" );
   }
   reloadFromSkin();
}
