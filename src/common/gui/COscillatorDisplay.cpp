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
#include "COscillatorDisplay.h"
#include "Oscillator.h"
#include <time.h>
#include "unitconversion.h"
#include "UserInteractions.h"
#include "guihelpers.h"
#include "SkinColors.h"

#include "filesystem/import.h"

using namespace VSTGUI;

const float disp_pitch = 90.15f - 48.f;
const int wtbheight = 12;

extern CFontRef displayFont;

void COscillatorDisplay::draw(CDrawContext* dc)
{
   pdata tp[2][n_scene_params]; // 0 is orange, 1 is blue
   Oscillator* osces[2];
   std::string olabel;
   for (int c = 0; c < 2; ++c)
   {
      osces[c] = nullptr;

#if OSC_MOD_ANIMATION
      if (!is_mod && c > 0 )
         continue;
#else
      if (c > 0)
         continue;
#endif

      tp[c][oscdata->pitch.param_id_in_scene].f = 0;
      for (int i = 0; i < n_osc_params; i++)
         tp[c][oscdata->p[i].param_id_in_scene].i = oscdata->p[i].val.i;

#if OSC_MOD_ANIMATION
      if (c == 1 )
      {
         auto modOut = 1.0;
         auto activeScene = storage->getPatch().scene_active.val.i;
         auto* scene = &(storage->getPatch().scene[activeScene]);
         auto iter = scene->modulation_voice.begin();

         // You'd think you could use this right? But you can't. These are NULL if you don't have a voice except scene ones
         // ModulationSource *ms = scene->modsources[modsource];

         bool isUnipolar = true;
         double smt = 1.0 - ( mod_time - (int)mod_time ); // 1hz ramp

         std::ostringstream oss;
         oss << modsource_names[modsource];
         olabel = oss.str();

         if( modsource >= ms_lfo1 && modsource <= ms_slfo6 )
         {
             auto *lfo = &(scene->lfo[modsource-ms_lfo1]);

             float frate = lfo->rate.val.f;
             if (lfo->rate.temposync)
                 frate *= storage->temposyncratio;

             auto freq = powf(2.0f, frate );

             smt = sin(M_PI * freq * mod_time);
             if( lfo->shape.val.i == lt_square )
                 smt = ( smt > 0 ? 1 : -1 );

             if( lfo->unipolar.val.i )
             {
                 smt = 0.5 * ( smt + 1 );
             }
         }

         while (iter != scene->modulation_voice.end())
         {
            int src_id = iter->source_id;
            if( src_id == modsource )
            {
                int dst_id = iter->destination_id;
                float depth = iter->depth;
                tp[c][dst_id].f += depth * modOut * smt;
            }
            iter++;
         }

         iter = scene->modulation_scene.begin();

         while (iter != scene->modulation_scene.end())
         {
            int src_id = iter->source_id;
            if( src_id == modsource )
            {
                int dst_id = iter->destination_id;
                float depth = iter->depth;
                tp[c][dst_id].f += depth * modOut * smt;
            }
            iter++;
         }
      }
#endif

      osces[c] = spawn_osc(oscdata->type.val.i, storage, oscdata, tp[c]);
   }

   int h = getHeight();
   if (uses_wavetabledata(oscdata->type.val.i))
      h -= wtbheight;

   int totalSamples = ( 1 << 4 ) * (int)getWidth();
   int averagingWindow = 4; // this must be both less than BLOCK_SIZE_OS and BLOCK_SIZE_OS must be an integer multiple of it

#if LINUX
   float valScale = 10000.0f;
#else
   float valScale = 100.0f;
#endif

   auto size = getViewSize();

   for (int c = 1; c >= 0; --c ) // backwards so we draw blue first
   {
      bool use_display = false;
      Oscillator* osc = osces[c];
      CGraphicsPath* path = dc->createGraphicsPath();
      if (osc)
      {
         float disp_pitch_rs = disp_pitch + 12.0 * log2(dsamplerate / 44100.0);
         if(! storage->isStandardTuning )
         {
            // OK so in this case we need to find a better version of the note which gets us
            // that pitch. Sigh.
            auto pit = storage->note_to_pitch_ignoring_tuning(disp_pitch_rs);
            int bracket = -1;
            for( int i=0; i<128; ++i )
            {
               if( storage->note_to_pitch(i) < pit && storage->note_to_pitch(i+1) > pit )
               {
                  bracket = i;
                  break;
               }
            }
            if( bracket >= 0 )
            {
               float f1 = storage->note_to_pitch(bracket);
               float f2 = storage->note_to_pitch(bracket+1);
               float frac = (pit - f1)/ (f2-f1);
               disp_pitch_rs = bracket + frac;
            }
            else
            {
               // What the hell type of scale is this folks! But this is just UI code so just punt
            }
         }
         use_display = osc->allow_display();

         // Mis-install check #2
         if (uses_wavetabledata(oscdata->type.val.i) && storage->wt_list.size() == 0)
            use_display = false;

         if (use_display)
            osc->init(disp_pitch_rs, true);

         int block_pos = BLOCK_SIZE_OS;
         for (int i = 0; i < totalSamples; i += averagingWindow)
         {
            if (use_display && (block_pos >= BLOCK_SIZE_OS))
            {
               if (uses_wavetabledata(oscdata->type.val.i))
               {
                  storage->waveTableDataMutex.lock();
                  osc->process_block(disp_pitch_rs);
                  block_pos = 0;
                  storage->waveTableDataMutex.unlock();
               }
               else
               {
                  osc->process_block(disp_pitch_rs);
                  block_pos = 0;
               }
            }

            float val = 0.f;
            if (use_display)
            {
               for (int j = 0; j < averagingWindow; ++j)
               {
                  val += osc->output[block_pos];
                  block_pos++;
               }
               val = val / averagingWindow;
               val = ((-val + 1.0f) * 0.5f * 0.8 + 0.1) * valScale;
            }
            block_pos++;
            float xc = valScale * i / totalSamples;

            // OK so val is now a value between 0 and valScale, and xc is a value between 0 and
            // valScale
            if (i == 0)
            {
               path->beginSubpath(xc, val);
            }
            else
            {
               path->addLine(xc, val);
            }
         }
         // srand( (unsigned)time( NULL ) );
      }
      // OK so now we need to figure out how to transfer the box with is [0,valscale] x [0,valscale]
      // to our coords. So scale then position
      VSTGUI::CGraphicsTransform tf = VSTGUI::CGraphicsTransform()
                                          .scale(getWidth() / valScale, h / valScale)
                                          .translate(size.getTopLeft().x, size.getTopLeft().y);
#if LINUX
      auto tmps = size;
      dc->getCurrentTransform().transform(tmps);
      VSTGUI::CGraphicsTransform tpath = VSTGUI::CGraphicsTransform()
                                             .scale(getWidth() / valScale, h / valScale)
                                             .translate(tmps.getTopLeft().x, tmps.getTopLeft().y);
#else
      auto tpath = tf;
#endif

      dc->saveGlobalState();

      if (c == 0)
      {
         // OK so draw the rules
         CPoint mid0(0, valScale / 2.f), mid1(valScale, valScale / 2.f);
         CPoint top0(0, valScale * 0.9), top1(valScale, valScale * 0.9);
         CPoint bot0(0, valScale * 0.1), bot1(valScale, valScale * 0.1);
         tf.transform(mid0);
         tf.transform(mid1);
         tf.transform(top0);
         tf.transform(top1);
         tf.transform(bot0);
         tf.transform(bot1);
         dc->setDrawMode(VSTGUI::kAntiAliasing);

         dc->setLineWidth(1.0);
         dc->setFrameColor(skin->getColor(Colors::Osc::Display::Center));
         dc->drawLine(mid0, mid1);

         dc->setLineWidth(1.0);
         dc->setFrameColor(skin->getColor(Colors::Osc::Display::Bounds));
         dc->drawLine(top0, top1);
         dc->drawLine(bot0, bot1);

         // OK so now the label
         if( osces[1] )
         {
            dc->setFontColor(skin->getColor(Colors::Osc::Display::AnimatedWave));
            dc->setFont(displayFont);
            CPoint lab0(0, valScale * 0.1 - 10);
            tf.transform(lab0);
            CRect rlab(lab0, CPoint(10,10) );
            dc->drawString(olabel.c_str(), rlab, kLeftText, true);
         }
      }

#if LINUX
      dc->setLineWidth(130);
#else
      dc->setLineWidth(1.3);
#endif
      dc->setDrawMode(VSTGUI::kAntiAliasing);
      if (c == 1)
         dc->setFrameColor(VSTGUI::CColor(100, 100, 180, 0xFF));
      else
         dc->setFrameColor(skin->getColor(Colors::Osc::Display::Wave));

      if( use_display )
         dc->drawGraphicsPath(path, VSTGUI::CDrawContext::PathDrawMode::kPathStroked, &tpath);
      dc->restoreGlobalState();

      path->forget();

      delete osces[c];
   }

   if (uses_wavetabledata(oscdata->type.val.i))
   {
      CRect wtlbl(size);
      wtlbl.right -= 1;
      wtlbl.top = wtlbl.bottom - wtbheight;
      rmenu = wtlbl;
      rmenu.inset(14, 0);
      char wttxt[256];

      storage->waveTableDataMutex.lock();

      int wtid = oscdata->wt.current_id;
      if( oscdata->wavetable_display_name[0] != '\0' )
      {
         strcpy(wttxt, oscdata->wavetable_display_name );
      }
      else if ((wtid >= 0) && (wtid < storage->wt_list.size()))
      {
         strcpy(wttxt, storage->wt_list.at(wtid).name.c_str());
      }
      else if (oscdata->wt.flags & wtf_is_sample)
      {
         strcpy(wttxt, "(Patch Sample)");
      }
      else
      {
         strcpy(wttxt, "(Patch Wavetable)");
      }

      storage->waveTableDataMutex.unlock();

      char* r = strrchr(wttxt, '.');
      if (r)
         *r = 0;
      // VSTGUI::CColor fgcol = cdisurf->int_to_ccol(coltable[255]);
      VSTGUI::CColor fgcol = skin->getColor(Colors::Osc::Filename::Background);
      dc->setFillColor(fgcol);
      dc->drawRect(rmenu, kDrawFilled);
      dc->setFontColor(skin->getColor(Colors::Osc::Filename::Text));
      dc->setFont(displayFont);
      // strupr(wttxt);
      dc->drawString(wttxt, rmenu, kCenterText, true);

      /*CRect wtlbl_status(size);
      wtlbl_status.bottom = wtlbl_status.top + wtbheight;
      dc->setFontColor(kBlackCColor);
      if(oscdata->wt.flags & wtf_is_sample) dc->drawString("IS
      SAMPLE",wtlbl_status,false,kRightText);*/

      rnext = wtlbl;
      rnext.left = rmenu.right; //+ 1;
      rprev = wtlbl;
      rprev.right = rmenu.left; //- 1;
      dc->setFillColor(fgcol);
      dc->drawRect(rprev, kDrawFilled);
      dc->drawRect(rnext, kDrawFilled);
      dc->setFrameColor(kBlackCColor);

      dc->saveGlobalState();

      dc->setDrawMode(kAntiAliasing);
      dc->setFillColor(kBlackCColor);
      VSTGUI::CDrawContext::PointList trinext;

      trinext.push_back(VSTGUI::CPoint(134, 170));
      trinext.push_back(VSTGUI::CPoint(139, 174));
      trinext.push_back(VSTGUI::CPoint(134, 178));
      dc->drawPolygon(trinext, kDrawFilled);

      VSTGUI::CDrawContext::PointList triprev;

      triprev.push_back(VSTGUI::CPoint(13, 170));
      triprev.push_back(VSTGUI::CPoint(8, 174));
      triprev.push_back(VSTGUI::CPoint(13, 178));

      dc->drawPolygon(triprev, kDrawFilled);

      dc->restoreGlobalState();
   }

   setDirty(false);
}

CMouseEventResult COscillatorDisplay::onMouseDown(CPoint& where, const CButtonState& button)
{
   if (listener && (button & (kMButton | kButton4 | kButton5)))
   {
      listener->controlModifierClicked(this, button);
      return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
   }

   assert(oscdata);

   if (!uses_wavetabledata(oscdata->type.val.i) || (where.y < rmenu.top))
   {
      controlstate = 1;
      lastpos.x = where.x;
      return kMouseEventHandled;
   }
   else if (uses_wavetabledata(oscdata->type.val.i))
   {
      int id = oscdata->wt.current_id;
      if (rprev.pointInside(where))
      {
         id = storage->getAdjacentWaveTable(oscdata->wt.current_id, false);
         if (id >= 0)
            oscdata->wt.queue_id = id;
      }
      else if (rnext.pointInside(where))
      {
         id = storage->getAdjacentWaveTable(oscdata->wt.current_id, true);
         if (id >= 0)
            oscdata->wt.queue_id = id;
      }
      else if (rmenu.pointInside(where))
      {
         CRect menurect(0, 0, 0, 0);
         menurect.offset(where.x, where.y);
         COptionMenu* contextMenu =
             new COptionMenu(menurect, 0, 0, 0, 0, COptionMenu::kMultipleCheckStyle);

         populateMenu(contextMenu, id);

         getFrame()->addView(contextMenu); // add to frame
         contextMenu->setDirty();
         contextMenu->popup();
         contextMenu->onMouseDown(where, kLButton); // <-- modal menu loop is here
         // getFrame()->looseFocus(pContext);

         getFrame()->removeView(contextMenu, true); // remove from frame and forget
      }
   }
   return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

void COscillatorDisplay::populateMenu(COptionMenu* contextMenu, int selectedItem)
{   
   int idx = 0;
   bool needToAddSep = false;
   for (auto c : storage->wtCategoryOrdering)
   {
      if (idx == storage->firstThirdPartyWTCategory ||
          (idx == storage->firstUserWTCategory &&
           storage->firstUserWTCategory != storage->wt_category.size()))
      {
         needToAddSep = true; // only add if there's actually data coming so defer the add
      }

      idx++;

      PatchCategory cat = storage->wt_category[c];
      if (cat.numberOfPatchesInCategoryAndChildren == 0)
         continue;

      if (cat.isRoot)
      {
         if (needToAddSep)
         {
            contextMenu->addEntry("-");
            needToAddSep = false;
         }
         populateMenuForCategory(contextMenu, c, selectedItem);
      }
   }

   // Add direct open here
   contextMenu->addSeparator();

   auto renameItem = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Change Wavetable Display Name..." )));
   auto rnaction = [this](CCommandMenuItem *item)
                      {
                         char c[256];
                         strncpy( c, this->oscdata->wavetable_display_name, 256 );
                         auto *sge = dynamic_cast<SurgeGUIEditor*>(listener);
                         if( sge )
                         {
                            sge->promptForMiniEdit(
                                c,
                                "Enter a custom wavetable display name:", "Wavetable Display Name",
                                CPoint( -1, -1 ), [this](const std::string& s) {
                                   strncpy(this->oscdata->wavetable_display_name, s.c_str(), 256);
                                   this->invalid();
                                });
                         }
                      };
   renameItem->setActions(rnaction, nullptr);
   contextMenu->addEntry(renameItem);

   auto refreshItem = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh Wavetable List")));
   auto refresh = [this](CCommandMenuItem* item) { this->storage->refresh_wtlist(); };
   refreshItem->setActions(refresh, nullptr);
   contextMenu->addEntry(refreshItem);

   contextMenu->addSeparator();

   auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Load Wavetable from File...")));
   auto action = [this](CCommandMenuItem* item) { this->loadWavetableFromFile(); };
   actionItem->setActions(action, nullptr);
   contextMenu->addEntry(actionItem);

   auto exportItem = new CCommandMenuItem(CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Save Wavetable to File...")));
   auto exportAction = [this](CCommandMenuItem *item)
       {
          // FIXME - we need to find the scene and osc by iterating (gross).
          int oscNum = -1;
          int scene = -1;
          for (int sc = 0; sc < n_scenes; sc++)
          {
             auto s = &(storage->getPatch().scene[sc]);
             for( int o=0;o<n_oscs; ++o )
             {
                if( &( s->osc[o] ) == this->oscdata )
                {
                   oscNum = o;
                   scene = sc;
                }
             }
          }
          if( scene == -1 || oscNum == -1 )
          {
             Surge::UserInteractions::promptError( "Unable to determine which oscillator has wavetable data ready for export!", "Export Error" );
          }
          else
          {
             std::string baseName = storage->getPatch().name + "_osc" + std::to_string(oscNum + 1) + "_scene" + (scene == 0 ? "A" : "B" );
             storage->export_wt_wav_portable( baseName, &( oscdata->wt ) );
          }
       };
   exportItem->setActions(exportAction,nullptr);
   contextMenu->addEntry(exportItem);

   auto omi = new CCommandMenuItem( CCommandMenuItem::Desc( Surge::UI::toOSCaseForMenu("Open Exported Wavetables Folder..." ) ) );
   omi->setActions([this](CCommandMenuItem *i) {
                      Surge::UserInteractions::openFolderInFileBrowser( Surge::Storage::appendDirectory( this->storage->userDataPath, "Exported Wavetables" ) );
                   }
      );
   contextMenu->addEntry( omi );

   auto *sge = dynamic_cast<SurgeGUIEditor*>(listener);
   if( sge )
   {
      auto hu = sge->helpURLForSpecial( "wavetables" );
      if( hu != "" )
      {
         auto lurl = sge->fullyResolvedHelpURL(hu);
         auto hi = new CCommandMenuItem( CCommandMenuItem::Desc("[?] Wavetables"));
         auto ca = [lurl](CCommandMenuItem *i)
                      {
                         Surge::UserInteractions::openURL(lurl);
                      };
         hi->setActions( ca, nullptr );
         contextMenu->addSeparator();
         contextMenu->addEntry(hi);
      }
   }

}

bool COscillatorDisplay::populateMenuForCategory(COptionMenu* contextMenu,
                                                 int categoryId,
                                                 int selectedItem)
{
   char name[NAMECHARS];
   COptionMenu* subMenu =
       new COptionMenu(getViewSize(), 0, categoryId, 0, 0, COptionMenu::kMultipleCheckStyle);
   subMenu->setNbItemsPerColumn(32);
   int sub = 0;

   PatchCategory cat = storage->wt_category[categoryId];

   for (auto p : storage->wtOrdering)
   {
      if (storage->wt_list[p].category == categoryId)
      {
         sprintf(name, "%s", storage->wt_list[p].name.c_str());
         auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name));
         auto action = [this, p](CCommandMenuItem* item) { this->loadWavetable(p); };

         if (p == selectedItem)
            actionItem->setChecked(true);
         actionItem->setActions(action, nullptr);
         subMenu->addEntry(actionItem);

         sub++;
      }
   }

   bool selected = false;

   for (auto child : cat.children)
   {
      if (child.numberOfPatchesInCategoryAndChildren > 0)
      {
         // this isn't the best approach but it works
         int cidx = 0;
         for (auto& cc : storage->wt_category)
         {
            if (cc.name == child.name)
               break;
            cidx++;
         }

         bool subSel = populateMenuForCategory(subMenu, cidx, selectedItem);
         selected = selected || subSel;
      }
   }

   if (!cat.isRoot)
   {
      std::string catName = storage->wt_category[categoryId].name;
      std::size_t sepPos = catName.find_last_of(PATH_SEPARATOR);
      if (sepPos != std::string::npos)
      {
         catName = catName.substr(sepPos + 1);
      }
      strncpy(name, catName.c_str(), NAMECHARS);
   }
   else
   {
      strncpy(name, storage->wt_category[categoryId].name.c_str(), NAMECHARS);
   }

   CMenuItem* submenuItem = contextMenu->addEntry(subMenu, name);

   if (selected || (selectedItem >= 0 && storage->wt_list[selectedItem].category == categoryId))
   {
      selected = true;
      submenuItem->setChecked(true);
   }

   subMenu->forget(); // Important, so that the refcounter gets right
   return selected;
}

void COscillatorDisplay::loadWavetable(int id)
{
   if (id >= 0 && (id < storage->wt_list.size()))
   {
      oscdata->wt.queue_id = id;
   }
}

void COscillatorDisplay::loadWavetableFromFile()
{
    Surge::UserInteractions::promptFileOpenDialog( "", "", "", [this](std::string s) {
            strncpy(this->oscdata->wt.queue_filename, s.c_str(), 255);

        } );
}

CMouseEventResult COscillatorDisplay::onMouseUp(CPoint& where, const CButtonState& buttons)
{
   if (controlstate)
   {
      controlstate = 0;
   }
   return kMouseEventHandled;
}
CMouseEventResult COscillatorDisplay::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
   if ( uses_wavetabledata(oscdata->type.val.i) )
   {
      if (rprev.pointInside(where) || rnext.pointInside(where) || rmenu.pointInside(where) )
      {
         // getFrame()->setCursor( VSTGUI::kCursorHand );
      }
      else
      {
         // getFrame()->setCursor( VSTGUI::kCursorDefault );
      }
   }
   else
   {
      // getFrame()->setCursor( VSTGUI::kCursorDefault );
   }

   if (controlstate)
   {
      /*oscdata->startphase.val.f -= 0.005f * (where.x - lastpos.x);
      oscdata->startphase.val.f = limit_range(oscdata->startphase.val.f,0.f,1.f);
      lastpos.x = where.x;
      invalid();*/
   }
   return kMouseEventHandled;
}

void COscillatorDisplay::invalidateIfIdIsInRange(int id)
{
   auto *currOsc = &oscdata->type;
   auto *endOsc = &oscdata->retrigger;
   bool oscInvalid = false;
   while( currOsc <= endOsc && ! oscInvalid )
   {
      if( currOsc->id == id )
         oscInvalid = true;
      currOsc++;
   }

   if( oscInvalid  )
   {
      invalid();
   }
}
