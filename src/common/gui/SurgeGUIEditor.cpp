//-------------------------------------------------------------------------------------------------------
//	Copyright 2005 Claes Johanson & Vember Audio
//-------------------------------------------------------------------------------------------------------
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

#include <iostream>
#include <iomanip>
#include <strstream>

#if TARGET_AUDIOUNIT
#include "aulayer.h"
#endif

#include "vstgui/lib/platform/platform_x11.h"
#include "vstgui/lib/platform/linux/x11platform.h"

#if LINUX
#include <experimental/filesystem>
#elif MAC || TARGET_RACK
#include <filesystem.h>
#else
#include <filesystem>
#endif

namespace fs = std::experimental::filesystem;

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
   //	tag_metaparam,
   // tag_metaparam_end = tag_metaparam+n_customcontrollers,
   start_paramtags,
};

SurgeGUIEditor::SurgeGUIEditor(void* effect, SurgeSynthesizer* synth, void* userdata) : super(effect)
{
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
   memset(param, 0, 1024 * sizeof(void*));
   polydisp = 0;
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
   idleinc = 0;

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
   if(Surge::ParamConfig::kHorizontal != VSTGUI::CSlider::kHorizontal ||
      Surge::ParamConfig::kVertical != VSTGUI::CSlider::kVertical
       )
   {
       throw new Surge::Error("Software Error: Param MisMaptch" );
   }

   for( int i=0; i<n_modsources; ++i )
      modsource_is_alternate[i] = false;
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
   if (!synth)
      return;
   if (editor_open && frame && !synth->halt_engine)
   {
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
               char pname[256], pdisp[256], txt[256];
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
                   .scene[0]
                   .modsources[ms_ctrl1 + i])
                  ->get_target01());
            }
            else if((j < n_total_params) && nonmod_param[j])
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
                cc->setValue(synth->getParameter01(j));
                cc->setDirty();
                cc->invalid();
            }
            else
            {
                // printf( "Bailing out of all possible refreshes on %d\n", j );
                // This is not really a problem
            }
         }
      }
      for (int i = 0; i < n_customcontrollers; i++)
      {
         if (((ControllerModulationSource*)synth->storage.getPatch().scene[0].modsources[ms_ctrl1 +
                                                                                         i])
                 ->has_changed(true))
         {
            gui_modsrc[ms_ctrl1 + i]->setValue(
                ((ControllerModulationSource*)synth->storage.getPatch()
                     .scene[0]
                     .modsources[ms_ctrl1 + i])
                    ->get_target01());
         }
      }
      clear_infoview_countdown--;
      if (clear_infoview_countdown == 0)
      {
         ((CParameterTooltip*)infowindow)->Hide();
         // infowindow->getViewSize();
         // ctnvg			frame->redrawRect(drawContext,r);
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
   for (int i = 0; i < 512; i++)
   {
      if (param[i])
      {
         CSurgeSlider* s = (CSurgeSlider*)param[i];
         if (s->is_mod)
         {
            s->setModMode(mod_editor ? 1 : 0);
            s->setModPresent(synth->isModDestUsed(i));
            s->setModCurrent(synth->isActiveModulation(i, thisms));
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
         ((CModulationSourceButton*)gui_modsrc[i])->invalid();
      }
   }
   // ctnvg	frame->redraw();
   // frame->setDirty();
}

int32_t SurgeGUIEditor::onKeyDown(const VstKeyCode& code, CFrame* frame)
{
    if(code.virt != 0 )
    {
        switch (code.virt)
        {
        case VKEY_TAB:
            if (saveDialog && saveDialog->isVisible())
            {
               /* 
               ** SaveDialog gets access to the tab key to switch between fields if it is open
               */
               return -1;
            }
            toggle_mod_editing();
            return 1;
        }
    }
    else
    {
        switch(code.character)
        {
        case '+':
            setZoomFactor(getZoomFactor()+10);
            return 1;
        case '=':
           /*
           ** This is a bit unsatisfying. The '+' key on linux with a US standard
           ** keyboard delivers as = with a shift modifier. I dislike hardcoding keyboard
           ** layouts but I don't see an API and this is a commonly used feature
           */
           if (code.modifier == VstModifierKey::MODIFIER_SHIFT)
           {
              setZoomFactor(getZoomFactor()+10);
              return 1;
           }
           break;
        case '-':
            setZoomFactor(getZoomFactor()-10);
            return 1;
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
   }

   return true; // visible by default
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

void SurgeGUIEditor::openOrRecreateEditor()
{
   if (!synth)
      return;
   assert(frame);

   if (editor_open)
      close_editor();

   CPoint nopoint(0, 0);

   current_scene = synth->storage.getPatch().scene_active.val.i;

   {
      CRect rect(0, 0, 75, 13);
      rect.offset(104 - 36, 69);
      CControl* oscswitch = new CHSwitch2(rect, this, tag_osc_select, 3, 13, 1, 3,
                                          bitmapStore->getBitmap(IDB_OSCSELECT), nopoint);
      oscswitch->setValue((float)current_osc / 2.0f);
      frame->addView(oscswitch);
   }

   {
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

   int rws = 15;
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
          new CModulationSourceButton(r, this, tag_mod_source0 + ms, state, ms, bitmapStore);
      ((CModulationSourceButton*)gui_modsrc[ms])
          ->update_rt_vals(false, 0, synth->isModsourceUsed(ms));
      if ((ms >= ms_ctrl1) && (ms <= ms_ctrl8))
      {
         ((CModulationSourceButton*)gui_modsrc[ms])
             ->setlabel(synth->storage.getPatch().CustomControllerLabel[ms - ms_ctrl1]);
         ((CModulationSourceButton*)gui_modsrc[ms])->set_ismeta(true);
         ((CModulationSourceButton*)gui_modsrc[ms])
             ->setBipolar(synth->storage.getPatch().scene[0].modsources[ms]->is_bipolar());
         gui_modsrc[ms]->setValue(
             ((ControllerModulationSource*)synth->storage.getPatch().scene[0].modsources[ms])
                 ->get_target01());
      }
      else
      {
         ((CModulationSourceButton*)gui_modsrc[ms])->setlabel(modsource_abberations_button[ms]);
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
   frame->addView(oscdisplay);

   // 150*b - 16 = 434 (b=3)
   patchname =
       new CPatchBrowser(CRect(156, 11, 547, 11 + 28), this, tag_patchname, &synth->storage);
   ((CPatchBrowser*)patchname)->setLabel(synth->storage.getPatch().name);
   ((CPatchBrowser*)patchname)->setCategory(synth->storage.getPatch().category);
   ((CPatchBrowser*)patchname)->setIDs(synth->current_category_id, synth->patchid);
   ((CPatchBrowser*)patchname)->setAuthor(synth->storage.getPatch().author);
   frame->addView(patchname);

   statuspanel = new CStatusPanel(CRect( 560, 1, 595, 54 ), this, tag_statuspanel, &synth->storage);
   {
       CStatusPanel *pb = (CStatusPanel *)statuspanel;
       pb->setDisplayFeature(CStatusPanel::mpeMode, synth->mpeEnabled);
       pb->setDisplayFeature(CStatusPanel::tuningMode, ! synth->storage.isStandardTuning);
       pb->setEditor(this);
   }

   frame->addView(statuspanel);
   
   CHSwitch2* mp_cat =
       new CHSwitch2(CRect(157, 41, 157 + 37, 41 + 12), this, tag_mp_category, 2, 12, 1, 2,
                     bitmapStore->getBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   mp_cat->setUsesMouseWheel(false); // mousewheel on category and patch buttons is undesirable     
   frame->addView(mp_cat);

   CHSwitch2* mp_patch =
       new CHSwitch2(CRect(242, 41, 242 + 37, 41 + 12), this, tag_mp_patch, 2, 12, 1, 2,
                     bitmapStore->getBitmap(IDB_BUTTON_MINUSPLUS), nopoint, false);
   mp_patch->setUsesMouseWheel(false);// mousewheel on category and patch buttons is undesirable                                
   frame->addView(mp_patch);

   CHSwitch2* b_store = new CHSwitch2(CRect(547 - 37, 41, 547, 41 + 12), this, tag_store, 1, 12, 1,
                                      1, bitmapStore->getBitmap(IDB_BUTTON_STORE), nopoint, false);
   frame->addView(b_store);

   memset(param, 0, 1024 * sizeof(void*)); // see the correct size in SurgeGUIEditor.h
   memset(nonmod_param, 0, 1024 * sizeof(void*));
   int i = 0;
   vector<Parameter*>::iterator iter;
   for (iter = synth->storage.getPatch().param_ptr.begin();
        iter != synth->storage.getPatch().param_ptr.end(); iter++)
   {
      Parameter* p = *iter;

      if ((p->posx != 0) && ((p->scene == (current_scene + 1)) || (p->scene == 0)) &&
          isControlVisible(p->ctrlgroup, p->ctrlgroup_entry) && (p->ctrltype != ct_none))
      {
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
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 10, 18, 1, 10,
                                          bitmapStore->getBitmap(IDB_FILTERBUTTONS), nopoint, true);
            rect(3, 0, 124, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_filtersubtype:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 1);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
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
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_KTRK));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_retrigger:
         {
            CRect rect(0, 0, 43, 7);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_RETRIGGER));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_oscroute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 15, 1, 3,
                                          bitmapStore->getBitmap(IDB_OSCROUTE), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_envshape:
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

            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_lfotrigmode:
         {
            CRect rect(0, 0, 51, 39);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 39, 3, 1,
                                          bitmapStore->getBitmap(IDB_LFOTRIGGER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_mute:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_MUTE));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_solo:
         {
            CRect rect(0, 0, 22, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_SOLO));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_unipolar:
         {
            CRect rect(0, 0, 51, 15);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_UNIPOLAR));
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_bool_relative_switch:
         {
            CRect rect(0, 0, 12, 18);
            rect.offset(p->posx + 129, p->posy + 5);
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_RELATIVE_TOGGLE));
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
            CControl* hsw = new CSwitchControl(rect, this, p->id + start_paramtags,
                                               bitmapStore->getBitmap(IDB_SWITCH_LINK));
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
            m->setValue(p->get_value_f01());
            frame->addView(m);
         }
         break;
         case ct_wstype:
         {
            CRect rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                          bitmapStore->getBitmap(IDB_WAVESHAPER), nopoint, true);
            rect(0, 0, 28, 47);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_polymode:
         {
            CRect rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 6, 47, 6, 1,
                                          bitmapStore->getBitmap(IDB_POLYMODE), nopoint, true);
            rect(0, 0, 50, 47);
            rect.offset(p->posx, p->posy);
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
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 4, 27, 1, 4,
                                          bitmapStore->getBitmap(IDB_FXBYPASS), nopoint, true);
            fxbypass_tag = p->id + start_paramtags;
            rect(2, 2, 133, 25);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_pitch_octave:
         {
            CRect rect(0, 0, 96, 18);
            rect.offset(p->posx, p->posy + 1);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 7, 18, 1, 7,
                                          bitmapStore->getBitmap(IDB_OCTAVES), nopoint, true);
            rect(1, 0, 91, 14);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_fbconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 8, 52, 1, 8,
                                          bitmapStore->getBitmap(IDB_FBCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
            filterblock_tag = p->id + start_paramtags;
         }
         break;
         case ct_fmconfig:
         {
            CRect rect(0, 0, 134, 52);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 4, 52, 1, 4,
                                          bitmapStore->getBitmap(IDB_FMCONFIG), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_scenemode:
         {
            CRect rect(0, 0, 36, 27);
            rect.offset(p->posx, p->posy);
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 27, 3, 1,
                                          bitmapStore->getBitmap(IDB_SCENEMODE), nopoint, true);
            rect(1, 1, 35, 27);
            rect.offset(p->posx, p->posy);
            hsw->setMouseableArea(rect);
            hsw->setValue(p->get_value_f01());
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
               CControl* slfo = new CLFOGui(
                   rect, lfo_id == 0, this, p->id + start_paramtags,
                   &synth->storage.getPatch().scene[current_scene].lfo[lfo_id], &synth->storage,
                   &synth->storage.getPatch().stepsequences[current_scene][lfo_id]);
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
            CControl* sceneswitch = new CHSwitch2(rect, this, tag_scene_select, 2, 27, 1, 2,
                                                  bitmapStore->getBitmap(IDB_SCENESWITCH), nopoint);
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
            CControl* hsw = new CHSwitch2(rect, this, p->id + start_paramtags, 3, 12, 1, 3,
                                          bitmapStore->getBitmap(IDB_CHARACTER), nopoint, true);
            hsw->setValue(p->get_value_f01());
            frame->addView(hsw);
            nonmod_param[i] = hsw;
         }
         break;
         case ct_midikey:
         {
            CRect rect(0, 0, 43, 14);
            rect.offset(p->posx, p->posy);
            CNumberField* key = new CNumberField(rect, this, p->id + start_paramtags);
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
            CNumberField* pbd = new CNumberField(rect, this, p->id + start_paramtags);
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
            CNumberField* key = new CNumberField(rect, this, p->id + start_paramtags);
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
                                    p->id + start_paramtags, true, bitmapStore);
               hs->setModMode(mod_editor ? 1 : 0);
               hs->setModValue(synth->getModulation(p->id, modsource));
               hs->setModPresent(synth->isModDestUsed(p->id));
               hs->setModCurrent(synth->isActiveModulation(p->id, modsource));
               hs->setValue(p->get_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               if( p->can_temposync() )
                  hs->setTempoSync(p->temposync);
               frame->addView(hs);
               param[i] = hs;
            }
            else
            {
               CSurgeSlider* hs =
                   new CSurgeSlider(CPoint(p->posx, p->posy + p->posy_offset * yofs), style, this,
                                    p->id + start_paramtags, false, bitmapStore);
               hs->setValue(p->get_value_f01());
               hs->setDefaultValue(p->get_default_value_f01());
               hs->setLabel(p->get_name());
               hs->setMoveRate(p->moverate);
               if( p->can_temposync() )
                  hs->setTempoSync(p->temposync);
               frame->addView(hs);
               param[i] = hs;

               /*						if(p->can_temposync() && (style &
               kHorizontal))
               {
               CRect rect(0,0,14,18);
               rect.offset(p->posx+134,p->posy+5 + p->posy_offset*yofs);
               CControl *hsw = new
               gui_switch(rect,this,p->id+start_paramtags+tag_temposyncoffset,bmp_temposync);
               rect(1,1,11,14);
               rect.offset(p->posx+134,p->posy+5 + p->posy_offset*yofs);
               hsw->setMouseableArea(rect);
               hsw->setValue(p->temposync?1.f:0.f);
               frame->addView(hsw);
               }		*/
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
   frame->addView(b_settingsMenu);

   infowindow = new CParameterTooltip(CRect(0, 0, 0, 0));
   frame->addView(infowindow);

   CRect wsize(0, 0, WINDOW_SIZE_X, WINDOW_SIZE_Y);
   aboutbox =
       new CAboutBox(aboutbrect, this, 0, 0, wsize, nopoint, bitmapStore->getBitmap(IDB_ABOUT));
   frame->addView(aboutbox);

   CRect dialogSize(148, 8, 598, 8 + 182);
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
   patchTuningLabel->setText( "Save Tuning in Patch" );
   patchTuningLabel->sizeToFit();
   
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
   
   patchName->setBackColor(kWhiteCColor);
   patchCategory->setBackColor(kWhiteCColor);
   patchCreator->setBackColor(kWhiteCColor);
   patchComment->setBackColor(kWhiteCColor);
   
   patchName->setFontColor(kBlackCColor);
   patchCategory->setFontColor(kBlackCColor);
   patchCreator->setFontColor(kBlackCColor);
   patchComment->setFontColor(kBlackCColor);

   patchName->setFrameColor(kGreyCColor);
   patchCategory->setFrameColor(kGreyCColor);
   patchCreator->setFrameColor(kGreyCColor);
   patchComment->setFrameColor(kGreyCColor);

   CColor bggr(205,206,212);
   patchTuningLabel->setBackColor(bggr);
   patchTuningLabel->setFrameColor(bggr);
   patchTuningLabel->setFontColor(kBlackCColor);
   
   saveDialog->addView(patchName);
   saveDialog->addView(patchCategory);
   saveDialog->addView(patchCreator);
   saveDialog->addView(patchComment);
   saveDialog->addView(patchTuning);
   saveDialog->addView(patchTuningLabel);
                               
                               
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

#if LINUX && TARGET_VST3
extern void LinuxVST3Init(Steinberg::Linux::IRunLoop* pf);
extern void LinuxVST3Detatch();
extern void LinuxVST3FrameOpen(CFrame* that, void*, const VSTGUI::PlatformType& pt);
#endif

#if !TARGET_VST3
bool SurgeGUIEditor::open(void* parent)
#else
bool PLUGIN_API SurgeGUIEditor::open(void* parent, const PlatformType& platformType)
#endif
{
   if (samplerate == 0)
   {
      std::cout << "SampleRate never set when editor opened. Setting to 44.1k" << std::endl;

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

   nframe->setZoom(fzf);
   CScalableBitmap *cbm = bitmapStore->getBitmap(IDB_BG);
   cbm->setExtraScaleFactor(getZoomFactor());
   nframe->setBackground(cbm);

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
   _idleTimer = new CVSTGUITimer([this](CVSTGUITimer* timer) { idle(); }, 50, false);
   _idleTimer->start();
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
#if !TARGET_VST3
   super::close();
#endif

#if TARGET_VST3 || TARGET_LV2
   _idleTimer->stop();
   _idleTimer = nullptr;
#endif

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

   std::vector< std::string > clearControlTargetNames;
   
   if (button & kDoubleClick)
      button |= kControl;

   if (button & kRButton)
   {
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
         sprintf(txt, "Osc %i", a + 1);
         contextMenu->addEntry(txt, eid++);
         contextMenu->addEntry("-", eid++);
         addCallbackMenu(contextMenu, "Copy",
                         [this, a]() { synth->storage.clipboard_copy(cp_osc, current_scene, a); });
         eid++;

         addCallbackMenu(contextMenu, "Copy (with modulation)", [this, a]() {
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
         sprintf(txt, "Scene %c", 'A' + a);
         contextMenu->addEntry(txt, eid++);
         contextMenu->addEntry("-", eid++);

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
         int id_clearallmr = -1, id_learnctrl = -1, id_clearctrl = -1, id_bipolar = -1,
             id_copy = -1, id_paste = -1, id_rename = -1;

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

            contextMenu->addEntry((char*)modsource_abberations[idOn], eid++);
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
            contextMenu->addEntry((char*)modsource_abberations[modsource], eid++);
         }
         
         int n_total_md = synth->storage.getPatch().param_ptr.size();

         const int max_md = 4096;
         assert(max_md >= n_total_md);

         bool cancellearn = false;
         int ccid = 0;

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
                  char tmptxt[256];
                  sprintf(tmptxt, "Clear %s -> %s [%.2f]", (char*)modsource_abberations[thisms],
                          synth->storage.getPatch().param_ptr[md]->get_full_name(),
                          synth->getModDepth(md, thisms));
                  
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
                     contextMenu->addEntry("-", eid++);
                     first_destination = false;
                  }
                  
                  addCallbackMenu(contextMenu, tmptxt, clearOp);
                  eid++;

                  n_md++;
               }
            }
            if (n_md)
            {
               char clearLab[256];
               sprintf( clearLab, "Clear all %s routings", modsource_abberations[thisms] );
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
         if (within_range(ms_ctrl1, modsource, ms_ctrl1 + n_customcontrollers - 1))
         {
            ccid = modsource - ms_ctrl1;
            contextMenu->addEntry("-", eid++);
            char txt[256];

            if (synth->learn_custom > -1)
               cancellearn = true;

            std::string learnTag =
                cancellearn ? "Abort learn controller" : "Learn controller [MIDI]";
            addCallbackMenu(contextMenu, learnTag, [this, cancellearn, ccid] {
               if (cancellearn)
                  synth->learn_param = -1;
               else
                  synth->learn_param = ccid;
            });
            eid++;

            if (synth->storage.controllers[ccid] >= 0)
            {
               char txt4[128];
               decode_controllerid(txt4, synth->storage.controllers[ccid]);
               sprintf(txt, "Clear controller [currently %s]", txt4);
               addCallbackMenu(contextMenu, txt, [this, ccid]() {
                  synth->storage.controllers[ccid] = -1;
                  synth->storage.save_midi_controllers();
               });
               eid++;
            }

         
            contextMenu->addEntry("-", eid++);

            addCallbackMenu(contextMenu, "Bipolar", [this, control, ccid]() {
               bool bp =
                   !synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->is_bipolar();
               synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->set_bipolar(bp);

               float f =
                   synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->get_output01();
               control->setValue(f);
               ((CModulationSourceButton*)control)->setBipolar(bp);
            });
            contextMenu->checkEntry(
                eid, synth->storage.getPatch().scene[0].modsources[ms_ctrl1 + ccid]->is_bipolar());
            eid++;

            addCallbackMenu(contextMenu, "Rename", [this, control, ccid]() {
               spawn_miniedit_text(synth->storage.getPatch().CustomControllerLabel[ccid], 16);
               ((CModulationSourceButton*)control)
                   ->setlabel(synth->storage.getPatch().CustomControllerLabel[ccid]);
               control->setDirty();
               control->invalid();
               synth->updateDisplay();
            });
            eid++;

            contextMenu->addEntry("-", eid++);

            // Construct submenus for expicit controller mapping
            COptionMenu* midiSub =
                new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
            COptionMenu* currentSub;
            for (int mc = 0; mc < 128; ++mc)
            {
               if (mc % 20 == 0)
               {
                  currentSub =
                      new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
                  char name[256];
                  sprintf(name, "CC %d -> %d", mc, min(mc + 20, 128) - 1);
                  midiSub->addEntry(currentSub, name);
               }

               char name[256];
               sprintf(name, "CC # %d", mc);
               CCommandMenuItem* cmd = new CCommandMenuItem(CCommandMenuItem::Desc(name));
               cmd->setActions([this, ccid, mc](CCommandMenuItem* men) {
                  synth->storage.controllers[ccid] = mc;
                  synth->storage.save_midi_controllers();
               });
               currentSub->addEntry(cmd);
            }
            contextMenu->addEntry(midiSub, "Set Controller To...");
         }

         int lfo_id = isLFO(modsource) ? modsource - ms_lfo1 : -1;

         if (lfo_id >= 0)
         {
            contextMenu->addEntry("-", eid++);
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

         return 1;
      }
      return 0;
   }

   if (tag < start_paramtags)
      return 0;
   if (!(button & (kControl | kRButton)))
      return 0;

   int ptag = tag - start_paramtags;
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
   {
      Parameter* p = synth->storage.getPatch().param_ptr[ptag];

      if ((button & kRButton))
      {
         CRect menuRect;
         CPoint where;
         frame->getCurrentMouseLocation(where);
         frame->localToFrame(where);
         

         menuRect.offset(where.x, where.y);

         COptionMenu* contextMenu =
             new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
         int eid = 0;

         contextMenu->addEntry((char*)p->get_name(), eid++);
         contextMenu->addEntry("-", eid++);
         char txt[256], txt2[512];
         p->get_display(txt);
         sprintf(txt2, "Value: %s", txt);
         contextMenu->addEntry(txt2, eid++);
         bool cancellearn = false;

         // Modulation and Learn semantics only apply to vt_float types in surge right now
         if( p->valtype == vt_float )
         {
            // if(p->can_temposync() || p->can_extend_range())	contextMenu->addEntry("-",eid++);
            if (p->can_temposync())
            {
               addCallbackMenu(contextMenu, "Temposync",
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
               contextMenu->checkEntry(eid, p->temposync);
               eid++;

               if( p->ctrlgroup == cg_LFO )
               {
                  char lab[256];
                  char prefix[256];
                  char un[5];

                  // WARNING - this won't work with Surge++
                  int a = p->ctrlgroup_entry + 1 - ms_lfo1;
                  if (a > 6)
                     sprintf(prefix, "SLFO%i", a - 6);
                  else
                     sprintf(prefix, "LFO%i", a);

                  bool setTSTo;
                  if( p->temposync )
                  {
                     un[0] = 'U'; un[1] = 'n'; un[2] = '-', un[3] = 0;;
                     setTSTo = false;
                  }
                  else
                  {
                     un[0] = 0;
                     setTSTo = true;
                  }

                  snprintf(lab, 256, "%sTempoSync all %s Params", un, prefix );
                  addCallbackMenu( contextMenu, lab,
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
            if (p->can_extend_range())
            {
               addCallbackMenu(contextMenu, "Extend range",
                               [this, p]() { p->extend_range = !p->extend_range; });
               contextMenu->checkEntry(eid, p->extend_range);
               eid++;
            }
            if (p->can_be_absolute())
            {
               addCallbackMenu(contextMenu, "Absolute", [this, p]() { p->absolute = !p->absolute; });
               contextMenu->checkEntry(eid, p->absolute);
               eid++;
            }
            if (p->can_snap())
            {
               addCallbackMenu(contextMenu, "Snap", [this, p]() { p->snap = !p->snap; });
               contextMenu->checkEntry(eid, p->snap);
               eid++;
            }
            
            {
               if (synth->learn_param > -1)
                  cancellearn = true;
               std::string learnTag =
                  cancellearn ? "Abort learn controller" : "Learn controller [MIDI]";
               addCallbackMenu(contextMenu, learnTag, [this, cancellearn, p] {
                                                         if (cancellearn)
                                                            synth->learn_param = -1;
                                                         else
                                                            synth->learn_param = p->id;
                                                      });
               eid++;
            }
            
            if (p->midictrl >= 0)
            {
               char txt4[128];
               decode_controllerid(txt4, p->midictrl);
               sprintf(txt, "Clear controller [currently %s]", txt4);
               addCallbackMenu(contextMenu, txt,
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
            }
            
            int n_ms = 0;
            // int clear_ms[n_modsources];
            bool is_modulated = false;
            for (int ms = 1; ms < n_modsources; ms++)
               if (synth->isActiveModulation(ptag, (modsources)ms))
                  n_ms++;

            if (n_ms)
            {
               contextMenu->addEntry("-", eid++);
               for (int k = 1; k < n_modsources; k++)
               {
                  modsources ms = (modsources)k;
                  if (synth->isActiveModulation(ptag, ms))
                  {
                     char tmptxt[256];
                     sprintf(tmptxt, "Clear %s -> %s [%.2f]", (char*)modsource_abberations[ms],
                             p->get_name(), synth->getModDepth(ptag, (modsources)ms));
                     // clear_ms[ms] = eid;
                     // contextMenu->addEntry(tmptxt, eid++);
                     addCallbackMenu(contextMenu, tmptxt, [this, ms, ptag]() {
                                                             synth->clearModulation(ptag, (modsources)ms);
                                                             refresh_mod();
                                                          });
                     eid++;
                  }
               }
               if (n_ms > 1)
               {
                  addCallbackMenu(contextMenu, "Clear All", [this, ptag]() {
                                                               for (int ms = 1; ms < n_modsources; ms++)
                                                               {
                                                                  synth->clearModulation(ptag, (modsources)ms);
                                                               }
                                                               refresh_mod();
                                                            });
               }
            }
         } // end vt_float if statement
         
         frame->addView(contextMenu); // add to frame
         contextMenu->popup();
         frame->removeView(contextMenu, true); // remove from frame and forget
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
            ((CSurgeSlider*)control)->setModCurrent(synth->isActiveModulation(p->id, thisms));
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

   if ((tag >= tag_mod_source0) && (tag < tag_mod_source_end))
   {
      if (((CModulationSourceButton*)control)->event_is_drag)
      {
         int t = (tag - tag_mod_source0);
         ((ControllerModulationSource*)synth->storage.getPatch().scene[0].modsources[t])
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
   case tag_settingsmenu:
   {
      CRect r = control->getViewSize();
      CRect menuRect;
      CPoint where;
      frame->getCurrentMouseLocation(where);
      frame->localToFrame(where);
         
      menuRect.offset(where.x, where.y);

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
      return;
   }
   case tag_store:
   {
      patchdata p;
      p.name = synth->storage.getPatch().name;
      p.category = synth->storage.getPatch().category;
      p.comments = synth->storage.getPatch().comment;
      p.author = synth->storage.getPatch().author;
      if (p.author.empty())
         p.author = synth->storage.defaultname;
      if (p.comments.empty())
         p.comments = synth->storage.defaultsig;

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
                                               "Error saving patch");
      }
      else
      {
          synth->storage.getPatch().name = patchName->getText();
          synth->storage.getPatch().author = patchCreator->getText();
          synth->storage.getPatch().category = patchCategory->getText();
          synth->storage.getPatch().comment = patchComment->getText();

          synth->storage.getPatch().patchTuning.tuningStoredInPatch = patchTuning->getValue() > 0.5;
          if( synth->storage.getPatch().patchTuning.tuningStoredInPatch )
              synth->storage.getPatch().patchTuning.tuningContents = synth->storage.currentScale.rawText;

          synth->storage.getPatch().dawExtraState.isPopulated = false; // Ignore whatever comes from the DAW
          
          synth->savePatch();
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
            synth->setModulation(ptag, thisms, ((CSurgeSlider*)control)->getModValue());
            ((CSurgeSlider*)control)->setModPresent(synth->isModDestUsed(p->id));
            ((CSurgeSlider*)control)->setModCurrent(synth->isActiveModulation(p->id, thisms));

            synth->getParameterName(ptag, txt);
            sprintf(pname, "%s -> %s", modsource_abberations_short[thisms], txt);
            sprintf(pdisp, "%f", synth->getModDepth(ptag, thisms));
            ((CParameterTooltip*)infowindow)->setLabel(pname, pdisp);
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
                  synth->updateDisplay();
               }
            }
         }
         else
         {
            // synth->storage.getPatch().param_ptr[ptag]->set_value_f01(control->getValue());
            bool force_integer = frame->getCurrentMouseButtons() & kControl;
            if (synth->setParameter01(ptag, control->getValue(), false, force_integer))
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
               ((CParameterTooltip*)infowindow)->setLabel(0, pdisp);
               if (p->ctrltype == ct_polymode)
                  modulate = true;
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
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
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
   if ((ptag >= 0) && (ptag < synth->storage.getPatch().param_ptr.size()))
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
      ((CParameterTooltip*)infowindow)->Hide();
   }
}

//------------------------------------------------------------------------------------------------

void SurgeGUIEditor::draw_infowindow(int ptag, CControl* control, bool modulate, bool forceMB)
{
   long buttons = 1; // context?context->getMouseButtons():1;

   if (buttons && forceMB)
      return; // don't draw via CC if MB is down

   CRect r(0, 0, 148, 18);
   if (modulate)
      r.bottom += 18;
   CRect r2 = control->getViewSize();

   r.offset((r2.left / 150) * 150, r2.bottom);

   int ao = 0;
   if (r.bottom > WINDOW_SIZE_Y)
      ao = (WINDOW_SIZE_Y - r.bottom);
   r.offset(0, ao);

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
       patchTuningLabel->setFontColor(kGreyCColor);
       patchTuning->setMouseEnabled(false);
       patchTuning->setBoxFrameColor(kGreyCColor);
       patchTuning->setValue(0);
   }
   else
   {
       patchTuningLabel->setFontColor(kBlackCColor);
       patchTuning->setMouseEnabled(true);
       patchTuning->setBoxFrameColor(kBlackCColor);
       patchTuning->setValue(0);
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
void SurgeGUIEditor::showMPEMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeMpeMenu(menuRect);
    
    frame->addView(m);
    m->setDirty();
    m->popup();
    frame->removeView(m, true);
}

void SurgeGUIEditor::toggleTuning()
{
    if( this->synth->storage.isStandardTuning && tuningCacheForToggle.size() > 0 )
    {
        this->synth->storage.retuneToScale(Surge::Storage::parseSCLData(tuningCacheForToggle));
    }
    else if( ! this->synth->storage.isStandardTuning )
    {
        tuningCacheForToggle = this->synth->storage.currentScale.rawText;
        this->synth->storage.init_tables();
    }

    if( statuspanel )
        ((CStatusPanel *)statuspanel)->setDisplayFeature(CStatusPanel::tuningMode, !this->synth->storage.isStandardTuning );
}
void SurgeGUIEditor::showTuningMenu(VSTGUI::CPoint &where)
{
    CRect menuRect;
    menuRect.offset(where.x, where.y);
    auto m = makeTuningMenu(menuRect);
    
    frame->addView(m);
    m->setDirty();
    m->popup();
    frame->removeView(m, true);
}

void SurgeGUIEditor::tuningFileDropped(std::string fn)
{
    this->synth->storage.retuneToScale(Surge::Storage::readSCLFile(fn));
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
   if (!zoomEnabled)
      zf = 100.0;

   if (zf < minimumZoom)
   {
      std::ostringstream oss;
      oss << "The smallest zoom on your platform is " << minimumZoom
          << "%. Sorry, you can't make surge any smaller.";
      Surge::UserInteractions::promptError(oss.str(), "No Teensy Surge for You");
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
      msg << "Surge limits zoom levels so as not to grow Surge larger than your available screen. "
          << "Your screen size is " << screenDim.getWidth() << "x" << screenDim.getHeight() << " "
          << "and your target zoom of " << zf << "% would be too large."
          << std::endl << std::endl
          << "Surge is choosing the largest fitting zoom " << zoomFactor << "%.";
      Surge::UserInteractions::promptError(msg.str(),
                                           "Limiting Zoom by Screen Size");
   }

   zoom_callback(this);

   float dbs = Surge::GUI::getDisplayBackingScaleFactor(getFrame());
   int fullPhysicalZoomFactor = (int)(zf * dbs);
   if (bitmapStore != nullptr)
      bitmapStore->setPhysicalZoomFactor(fullPhysicalZoomFactor);
}

void SurgeGUIEditor::showSettingsMenu(CRect &menuRect)
{
    COptionMenu* settingsMenu =
        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle | VSTGUI::COptionMenu::kMultipleCheckStyle);
    int eid = 0;

    // Zoom submenus
    if (zoomEnabled)
    {
       COptionMenu* zoomSubMenu =
           new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);

       int zid = 0;
       for (auto s : {100, 125, 150, 200, 300}) // These are somewhat arbitrary reasonable defaults
       {
          std::ostringstream lab;
          lab << "Zoom to " << s << "%";
          CCommandMenuItem* zcmd = new CCommandMenuItem(CCommandMenuItem::Desc(lab.str()));
          zcmd->setActions([this, s](CCommandMenuItem* m) { setZoomFactor(s); });
          zoomSubMenu->addEntry(zcmd);
          zid++;
       }

       zoomSubMenu->addEntry("-", zid++);

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

       zoomSubMenu->addEntry("-", zid++);
       CCommandMenuItem* biggestZ = new CCommandMenuItem(CCommandMenuItem::Desc("Zoom to Largest"));
       biggestZ->setActions([this](CCommandMenuItem* m) {
          int newZF = findLargestFittingZoomBetween(100.0, 500.0, 5,
                                                    90, // See comment in setZoomFactor
                                                    WINDOW_SIZE_X, WINDOW_SIZE_Y);
          setZoomFactor(newZF);
       });
       zoomSubMenu->addEntry(biggestZ);
       zid++;

       CCommandMenuItem* smallestZ =
           new CCommandMenuItem(CCommandMenuItem::Desc("Zoom to Smallest"));
       smallestZ->setActions([this](CCommandMenuItem* m) { setZoomFactor(minimumZoom); });
       zoomSubMenu->addEntry(smallestZ);
       zid++;

       zoomSubMenu->addEntry("-", zid++);
       std::ostringstream zss;
       zss << "Set " << zoomFactor << "% as default";
       CCommandMenuItem* defaultZ = new CCommandMenuItem(CCommandMenuItem::Desc(zss.str().c_str()));
       defaultZ->setActions([this](CCommandMenuItem* m) {
          Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultZoom",
                                                 this->zoomFactor);
       });
       zoomSubMenu->addEntry(defaultZ);
       zid++;

       addCallbackMenu(zoomSubMenu, "Set default zoom to ...", [this]() {
               // FIXME! This won't work on linux
               char c[256];
               snprintf(c, 256, "%d", this->zoomFactor );
               spawn_miniedit_text(c, 16);
               int newVal = ::atoi(c);
               Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "defaultZoom", newVal);
           });
       zid++;

       settingsMenu->addEntry(zoomSubMenu, "Zoom" );
       eid++;
    }

    auto mpeSubMenu = makeMpeMenu(menuRect);
    std::string mpeMenuName = "MPE (disabled)";
    if (synth->mpeEnabled)
       mpeMenuName = "MPE (enabled)";
    settingsMenu->addEntry(mpeSubMenu, mpeMenuName.c_str());
    eid++;
    mpeSubMenu->forget();

    COptionMenu *uiOptionsMenu = new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle );
    
    // Mouse behavior
    // Mouse behavior
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
    if (menuItem)
       menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kClassic));
    mid++;

    menuItem = addCallbackMenu(mouseSubMenu, mouseSlow.c_str(), [this]() {
       CSurgeSlider::sliderMoveRateState = CSurgeSlider::kSlow;
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                              CSurgeSlider::sliderMoveRateState);
    });
    if (menuItem)
       menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kSlow));
    mid++;

    menuItem = addCallbackMenu(mouseSubMenu, mouseMedium.c_str(), [this]() {
       CSurgeSlider::sliderMoveRateState = CSurgeSlider::kMedium;
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                              CSurgeSlider::sliderMoveRateState);
    });
    if (menuItem)
       menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kMedium));
    mid++;

    menuItem = addCallbackMenu(mouseSubMenu, mouseExact.c_str(), [this]() {
       CSurgeSlider::sliderMoveRateState = CSurgeSlider::kExact;
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "sliderMoveRateState",
                                              CSurgeSlider::sliderMoveRateState);
    });
    if (menuItem)
       menuItem->setChecked((CSurgeSlider::sliderMoveRateState == CSurgeSlider::kExact));
    mid++;

    std::string mouseMenuName = "Mouse Behavior";

    uiOptionsMenu->addEntry(mouseSubMenu, mouseMenuName.c_str() );
    mouseSubMenu->forget();

#if !LINUX
    auto useBitmap = Surge::Storage::getUserDefaultValue(&(this->synth->storage), "useBitmapLFO",
                                                         0
        );
    auto bitmapMenu = useBitmap ? "Use Vector LFO Display" : "Use Bitmap LFO Display";
    addCallbackMenu( uiOptionsMenu, bitmapMenu, [this, useBitmap]() {
            Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                   "useBitmapLFO",
                                                   useBitmap ? 0 : 1 );
            this->synth->refresh_editor = true;
        });
#endif
    
    settingsMenu->addEntry(uiOptionsMenu, "UI Options" );
    uiOptionsMenu->forget();
    eid++;

    auto tuningSubMenu = makeTuningMenu(menuRect);
    settingsMenu->addEntry(tuningSubMenu, "Tuning" );
    eid++;
    tuningSubMenu->forget();

    int did=0;
    COptionMenu *dataSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                               VSTGUI::COptionMenu::kNoDrawStyle |
                                               VSTGUI::COptionMenu::kMultipleCheckStyle);


    addCallbackMenu(dataSubMenu, "Open User Data Folder", [this]() {
       // make it if it isn't there
       fs::create_directories(this->synth->storage.userDataPath);
       Surge::UserInteractions::openFolderInFileBrowser(this->synth->storage.userDataPath);
    });
    did++;
    
    addCallbackMenu(dataSubMenu, "Open Factory Data Folder", [this]() {
       Surge::UserInteractions::openFolderInFileBrowser(this->synth->storage.datapath);
    });
    did++;

    addCallbackMenu(dataSubMenu, "Rescan All Data Folders", [this]() {
       this->synth->storage.refresh_wtlist();
       this->synth->storage.refresh_patchlist();
    });
    did++;

    addCallbackMenu( dataSubMenu, "Set Custom User Data Folder", [this]() {
            auto cb = [this](std::string f) {
                // FIXME - check if f is a path
                this->synth->storage.userDataPath = f;
                Surge::Storage::updateUserDefaultValue(&(this->synth->storage),
                                                       "userDataPath",
                                                       f);
                this->synth->storage.refresh_wtlist();
                this->synth->storage.refresh_patchlist();
            };
            Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.userDataPath, "", cb, true, true);
                                                          
        });
    did++;
    
    settingsMenu->addEntry( dataSubMenu, "Data and Patches");
    eid++;
    dataSubMenu->forget();

    settingsMenu->addSeparator(eid++);


    addCallbackMenu(settingsMenu, "Reach the Developers", []() {
            Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/feedback");
        });
    eid++;
    
    addCallbackMenu(settingsMenu, "Read the Code", []() {
            Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge/");
        });
    eid++;

    addCallbackMenu(settingsMenu, "Download Additional Content", []() {
            Surge::UserInteractions::openURL("https://github.com/surge-synthesizer/surge-synthesizer.github.io/wiki/Additional-Content");
    });
    eid++;

    addCallbackMenu(settingsMenu, "Surge Website", []() {
            Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/");
        });
    eid++;
    
    addCallbackMenu(settingsMenu, "Surge Manual", []() {
       Surge::UserInteractions::openURL("https://surge-synthesizer.github.io/manual/");
    });
    eid++;

    settingsMenu->addSeparator(eid++);

    addCallbackMenu(settingsMenu, "About", [this]() {
       if (aboutbox)
          ((CAboutBox*)aboutbox)->boxShow();
    });
    eid++;

    frame->addView(settingsMenu);
    settingsMenu->setDirty();
    settingsMenu->popup();
    frame->removeView(settingsMenu, true);
}


VSTGUI::COptionMenu *SurgeGUIEditor::makeMpeMenu(VSTGUI::CRect &menuRect)
{
    COptionMenu* mpeSubMenu =
        new COptionMenu(menuRect, 0, 0, 0, 0, VSTGUI::COptionMenu::kNoDrawStyle);
    std::string endis = "Enable MPE";
    if (synth->mpeEnabled)
       endis = "Disable MPE";
    addCallbackMenu(mpeSubMenu, endis.c_str(),
                    [this]() { this->synth->mpeEnabled = !this->synth->mpeEnabled; });

    std::ostringstream oss;
    oss << "Change default pitch bend range (" << synth->mpePitchBendRange << ")";
    addCallbackMenu(mpeSubMenu, oss.str().c_str(), [this]() {
       // FIXME! This won't work on linux
       char c[256];
       snprintf(c, 256, "%d", synth->mpePitchBendRange);
       spawn_miniedit_text(c, 16);
       int newVal = ::atoi(c);
       Surge::Storage::updateUserDefaultValue(&(this->synth->storage), "mpePitchBendRange", newVal);
       this->synth->mpePitchBendRange = newVal;
    });
    return mpeSubMenu;

}

VSTGUI::COptionMenu *SurgeGUIEditor::makeTuningMenu(VSTGUI::CRect &menuRect)
{
    int tid=0;
    COptionMenu *tuningSubMenu = new COptionMenu(menuRect, 0, 0, 0, 0,
                                                 VSTGUI::COptionMenu::kNoDrawStyle |
                                                 VSTGUI::COptionMenu::kMultipleCheckStyle);

    auto *st = addCallbackMenu(tuningSubMenu, "Set to Standard Tuning",
                    [this]()
                    {
                        this->synth->storage.init_tables();
                    }
        );
    st->setEnabled(! this->synth->storage.isStandardTuning);
    tid++;

    addCallbackMenu(tuningSubMenu, "Apply .scl file tuning",
                    [this]()
                    {
                        auto cb = [this](std::string sf)
                        {
                            std::string sfx = ".scl";
                            if( sf.length() >= sfx.length())
                            {
                                if( sf.compare(sf.length() - sfx.length(), sfx.length(), sfx) != 0 )
                                {
                                    Surge::UserInteractions::promptError( "Please only select .scl files", "Invalid Choice" );
                                    std::cout << "FILE is [" << sf << "]" << std::endl;
                                    return;
                                }
                            }
                            auto sc = Surge::Storage::readSCLFile(sf);

                            if (!this->synth->storage.retuneToScale(sc))
                            {
                               Surge::UserInteractions::promptError( "This .scl file is not valid", "File format error" );
                               return;
                            }
                        };
                        Surge::UserInteractions::promptFileOpenDialog(this->synth->storage.userDataPath,
                                                                      ".scl",
                                                                      cb);
                    }
        );
    tid++;

    auto *sct = addCallbackMenu(tuningSubMenu, "Show current tuning",
                    [this]()
                    {
                        // Surge::UserInteractions::promptOKCancel( "Surge tuning is NONstandard tuning", "Tuning Info" );
                        Surge::UserInteractions::showHTML( this->synth->storage.currentScale.toHtml() );
                    }
        );
    sct->setEnabled(! this->synth->storage.isStandardTuning );

    /*
    tuningSubMenu->addSeparator(tid++);
    
    addCallbackMenu(tuningSubMenu, "Apply .kbm file mapping",
                    [this]()
                    {
                        Surge::UserInteractions::promptError("KBM File Mappings are not yet supported in Surge",
                                                             "Unsupported",
                                                             this);
                    }
        );
    */
    return tuningSubMenu;
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

//------------------------------------------------------------------------------------------------
