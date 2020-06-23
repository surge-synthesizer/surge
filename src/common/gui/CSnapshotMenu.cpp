#include "globals.h"
#include "SurgeGUIEditor.h"
#include "DspUtilities.h"
#include "CSnapshotMenu.h"
#include "effect/Effect.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeStorage.h" // for TINYXML macro
#include "PopupEditorSpawner.h"
#include "ImportFilesystem.h"
#include "guihelpers.h"

#include <iostream>
#include <iomanip>
#include <queue>

using namespace VSTGUI;

extern CFontRef displayFont;

// CSnapshotMenu

CSnapshotMenu::CSnapshotMenu(const CRect& size,
                             IControlListener* listener,
                             long tag,
                             SurgeStorage* storage)
    : COptionMenu(size, nullptr, tag, 0)
{
   this->storage = storage;
   this->listenerNotForParent = listener;
}
CSnapshotMenu::~CSnapshotMenu()
{}

void CSnapshotMenu::draw(CDrawContext* dc)
{
   setDirty(false);
}

bool CSnapshotMenu::canSave()
{
   return false;
}

void CSnapshotMenu::populate()
{
   int main = 0, sub = 0;
   bool do_nothing = false;
   const long max_main = 128, max_sub = 256;

   int idx = 0;
   TiXmlElement* sect = storage->getSnapshotSection(mtype);
   if (sect)
   {
      TiXmlElement* type = sect->FirstChildElement();

      while (type)
      {
         if( type->Value() && strcmp( type->Value(), "type" ) == 0  )
         {
            auto sm = populateSubmenuFromTypeElement(type, this, main, sub, max_sub, idx);
            if( sm )
               addToTopLevelTypeMenu(type, sm, idx);
         }
         else if( type->Value() && strcmp( type->Value(), "separator" ) == 0 )
            addSeparator();
         type = type->NextSiblingElement();
         main++;
         if (main >= max_main)
            break;
      }
   }
}

bool CSnapshotMenu::loadSnapshotByIndex( int idx )
{
   int cidx = 0;
   // This isn't that efficient but you know
   TiXmlElement *sect = storage->getSnapshotSection(mtype);
   if( sect )
   {
      std::queue<TiXmlElement *> typeD;
      typeD.push( TINYXML_SAFE_TO_ELEMENT(sect->FirstChild("type")) );
      while( ! typeD.empty() )
      {
         auto type = typeD.front();
         typeD.pop();
         auto tn = type->Attribute( "name" );
         int type_id = 0;
         type->Attribute("i", &type_id);
         TiXmlElement* snapshot = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("snapshot"));
         while (snapshot)
         {
            auto n = snapshot->Attribute("name");
            int snapshotTypeID = type_id, tmpI = 0;
            if (snapshot->Attribute("i", &tmpI) != nullptr)
            {
               snapshotTypeID = tmpI;
            }

            if( cidx == idx )
            {
               selectedIdx = cidx;
               loadSnapshot( snapshotTypeID, snapshot, cidx);
               if( listenerNotForParent )
                  listenerNotForParent->valueChanged( this );
               return true;
            }
            snapshot = TINYXML_SAFE_TO_ELEMENT(snapshot->NextSibling("snapshot"));
            cidx++;
         }

         auto subType = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("type"));
         while( subType )
         {
            typeD.push(subType);
            subType = TINYXML_SAFE_TO_ELEMENT(subType->NextSibling("type"));
         }

         auto next = TINYXML_SAFE_TO_ELEMENT(type->NextSibling("type"));
         if( next )
            typeD.push( next );
      }
   }
   if( idx < 0 && cidx + idx > 0 ) {
      // I've gone off the end
      return( loadSnapshotByIndex( cidx + idx ) );
   }
   return false;
}

VSTGUI::COptionMenu* CSnapshotMenu::populateSubmenuFromTypeElement(TiXmlElement *type, VSTGUI::COptionMenu *parent, int &main, int &sub, const long &max_sub, int &idx)
{
    /*
    ** Begin by grabbing all the snapshot elements
    */
    std::string txt;
    int type_id = 0;
    type->Attribute("i", &type_id);
    sub = 0;
    COptionMenu* subMenu = new COptionMenu(getViewSize(), 0, main, 0, 0, kNoDrawStyle);
    TiXmlElement* snapshot = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("snapshot"));
    while (snapshot)
    {
        txt = snapshot->Attribute("name");

        #if WINDOWS
            Surge::Storage::findReplaceSubstring(txt, std::string("&"), std::string("&&"));
        #endif

        int snapshotTypeID = type_id;
        int tmpI;
        if (snapshot->Attribute("i", &tmpI) != nullptr)
        {
           snapshotTypeID = tmpI;
        }

        auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(txt.c_str()));
        auto action = [this, snapshot, snapshotTypeID, idx](CCommandMenuItem* item) {
                         this->selectedIdx = idx;
                         this->loadSnapshot(snapshotTypeID, snapshot, idx);
                         if( this->listenerNotForParent )
                            this->listenerNotForParent->valueChanged( this );
        };
        idx++;
        
        actionItem->setActions(action, nullptr);
        subMenu->addEntry(actionItem);

        snapshot = TINYXML_SAFE_TO_ELEMENT(snapshot->NextSibling("snapshot"));
        sub++;
        if (sub >= max_sub)
            break;
    }


    /*
    ** Next see if we have any subordinate types
    */
    TiXmlElement* subType = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("type"));
    if (subType)
    {
        populateSubmenuFromTypeElement(subType, subMenu, main, sub, max_sub, idx);
        subType = TINYXML_SAFE_TO_ELEMENT(subType->NextSibling("type"));
    }
        

    /*
    ** Then add myself to parent
    */
    txt = type->Attribute("name");

    #if WINDOWS
        Surge::Storage::findReplaceSubstring(txt, std::string("&"), std::string("&&"));
    #endif

    if (sub)
    {
        parent->addEntry(subMenu, txt.c_str());
    }
    else
    {
        auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(txt.c_str()));
        auto action = [this, type_id](CCommandMenuItem* item) {
                         this->selectedIdx = 0;
                         this->loadSnapshot(type_id, nullptr, 0);
                         if( this->listenerNotForParent )
                            this->listenerNotForParent->valueChanged(this);
        };

        actionItem->setActions(action, nullptr);
        parent->addEntry(actionItem);
    }

    subMenu->forget();
    if( sub )
       return subMenu; // OK to return forgoten since it has lifetime of parent
    else
       return nullptr;
}

// COscMenu

COscMenu::COscMenu(const CRect& size,
                   IControlListener* listener,
                   long tag,
                   SurgeStorage* storage,
                   OscillatorStorage* osc,
                   std::shared_ptr<SurgeBitmaps> bitmapStore)
    : CSnapshotMenu(size, listener, tag, storage)
{
   strcpy(mtype, "osc");
   this->osc = osc;
   bmp = bitmapStore->getBitmap(IDB_OSCMENU);
   populate();
}

void COscMenu::draw(CDrawContext* dc)
{
   CRect size = getViewSize();
   int i = osc->type.val.i;
   int y = i * size.getHeight();
   if (bmp)
      bmp->draw(dc, size, CPoint(0, y), 0xff);

   setDirty(false);
}

void COscMenu::loadSnapshot(int type, TiXmlElement* e, int idx)
{
   assert(within_range(0, type, num_osctypes));
   osc->queue_type = type;
   osc->queue_xmldata = e;
}

/*void COscMenu::load_snapshot(int type, TiXmlElement *e)
{
        assert(within_range(0,type,num_osctypes));
        osc->type.val.i = type;
        //osc->retrigger.val.i =
        storage->patch.update_controls(false, osc);
        if(e)
        {
                for(int i=0; i<n_osc_params; i++)
                {
                        double d; int j;
                        char lbl[256];
                        sprintf(lbl,"p%i",i);
                        if (osc->p[i].valtype == vt_float)
                        {
                                if(e->QueryDoubleAttribute(lbl,&d) == TIXML_SUCCESS) osc->p[i].val.f
= (float)d;
                        }
                        else
                        {
                                if(e->QueryIntAttribute(lbl,&j) == TIXML_SUCCESS) osc->p[i].val.i =
j;
                        }
                }
        }
}*/

// CFxMenu

const char fxslot_names[8][NAMECHARS] = {"A Insert FX 1", "A Insert FX 2", "B Insert FX 1", "B Insert FX 2",
                                         "Send FX 1",  "Send FX 2",  "Master FX 1",   "Master FX 2"};

std::vector<float> CFxMenu::fxCopyPaste;

CFxMenu::CFxMenu(const CRect& size,
                 IControlListener* listener,
                 long tag,
                 SurgeStorage* storage,
                 FxStorage* fx,
                 FxStorage* fxbuffer,
                 int slot)
    : CSnapshotMenu(size, listener, tag, storage)
{
   strcpy(mtype, "fx");
   this->fx = fx;
   this->fxbuffer = fxbuffer;
   this->slot = slot;
   selectedName = "";
   populate();
}

void CFxMenu::draw(CDrawContext* dc)
{
   CRect lbox = getViewSize();
   lbox.right--;
   lbox.bottom--;

   auto fgc = skin->getColor( "fxmenu.foreground", kBlackCColor );
   dc->setFontColor(fgc);
   dc->setFont(displayFont);
   CRect txtbox(lbox);
   txtbox.inset(2, 2);
   txtbox.inset(3, 0);
   txtbox.right -= 6;
   txtbox.top--;
   txtbox.bottom += 2;
   dc->drawString(fxslot_names[slot], txtbox, kLeftText, true);
   char fxname[NAMECHARS];
   sprintf(fxname, "%s", fxtype_abberations[fx->type.val.i]);
   dc->drawString(fxname, txtbox, kRightText, true);

   CPoint d(txtbox.right + 2, txtbox.top + 5);
   dc->drawPoint(d, fgc);
   d.x++;
   dc->drawPoint(d, fgc);
   d.y++;
   dc->drawPoint(d, fgc);
   d.y--;
   d.x++;
   dc->drawPoint(d, fgc);

   setDirty(false);
}

void CFxMenu::loadSnapshot(int type, TiXmlElement* e, int idx)
{
   if (!type)
      fxbuffer->type.val.i = type;
   if (e)
   {
      fxbuffer->type.val.i = type;
      // storage->patch.update_controls();
      selectedName = e->Attribute( "name" );
      
      Effect* t_fx = spawn_effect(type, storage, fxbuffer, 0);
      if (t_fx)
      {
         t_fx->init_ctrltypes();
         t_fx->init_default_values();
         delete t_fx;
      }

      for (int i = 0; i < n_fx_params; i++)
      {
         double d;
         int j;
         char lbl[256], sublbl[256];
         sprintf(lbl, "p%i", i);
         if (fxbuffer->p[i].valtype == vt_float)
         {
            if (e->QueryDoubleAttribute(lbl, &d) == TIXML_SUCCESS)
               fxbuffer->p[i].set_storage_value((float)d);
         }
         else
         {
            if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
               fxbuffer->p[i].set_storage_value(j);
         }

         sprintf(sublbl, "p%i_temposync", i);
         fxbuffer->p[i].temposync =
             ((e->QueryIntAttribute(sublbl, &j) == TIXML_SUCCESS) && (j == 1));
         sprintf(sublbl, "p%i_extend_range", i);
         fxbuffer->p[i].extend_range =
             ((e->QueryIntAttribute(sublbl, &j) == TIXML_SUCCESS) && (j == 1));
      }
   }
#if TARGET_LV2
   auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
   if( sge )
   {
      sge->forceautomationchangefor( &(fxbuffer->type) );
      for( int i=0; i<n_fx_params; ++i )
         sge->forceautomationchangefor( &(fxbuffer->p[i] ) );
   }
#endif
   
}
void CFxMenu::saveSnapshot(TiXmlElement* e, const char* name)
{
   if (fx->type.val.i == 0)
      return;
   TiXmlElement* t = TINYXML_SAFE_TO_ELEMENT(e->FirstChild("type"));
   while (t)
   {
      int ii;
      if ((t->QueryIntAttribute("i", &ii) == TIXML_SUCCESS) && (ii == fx->type.val.i))
      {
         // if name already exists, delete old entry
         TiXmlElement* sn = TINYXML_SAFE_TO_ELEMENT(t->FirstChild("snapshot"));
         while (sn)
         {
            if (sn->Attribute("name") && !strcmp(sn->Attribute("name"), name))
            {
               t->RemoveChild(sn);
               break;
            }
            sn = TINYXML_SAFE_TO_ELEMENT(sn->NextSibling("snapshot"));
         }

         TiXmlElement neu("snapshot");

         for (int p = 0; p < n_fx_params; p++)
         {
            char lbl[256], txt[256], sublbl[256];
            sprintf(lbl, "p%i", p);
            if (fx->p[p].ctrltype != ct_none)
            {
               neu.SetAttribute(lbl, fx->p[p].get_storage_value(txt));

               if (fx->p[p].temposync)
               {
                  sprintf(sublbl, "p%i_temposync", p);
                  neu.SetAttribute(sublbl, "1");
               }
               if (fx->p[p].extend_range)
               {
                  sprintf(sublbl, "p%i_extend_range", p);
                  neu.SetAttribute(sublbl, "1");
               }
            }
         }
         neu.SetAttribute("name", name);
         t->InsertEndChild(neu);
         return;
      }
      t = TINYXML_SAFE_TO_ELEMENT(t->NextSibling("type"));
   }
}

bool CFxMenu::scanForUserPresets = true;
std::unordered_map<int,std::vector<CFxMenu::UserPreset>> CFxMenu::userPresets;

void CFxMenu::rescanUserPresets()
{
   userPresets.clear();
   scanForUserPresets = false;

   auto ud = storage->userFXPath;

   if( fs::is_directory( fs::path( ud ) ) ) {
      for( auto &d : fs::directory_iterator( fs::path( ud ) ) )
      {
         auto fn = d.path().generic_string();
         std::string ending = ".srgfx";
         if( fn.length() >= ending.length() && ( 0 == fn.compare( fn.length() - ending.length(), ending.length(), ending ) ) )
         {
            UserPreset preset;
            preset.file = fn;
            TiXmlDocument d;
            int t;
            
            if( ! d.LoadFile( fn ) ) goto badPreset;
            
            auto r = TINYXML_SAFE_TO_ELEMENT(d.FirstChild( "single-fx" ) );
            if( ! r ) goto badPreset;
            
            auto s = TINYXML_SAFE_TO_ELEMENT(r->FirstChild( "snapshot" ) );
            if( ! s ) goto badPreset;
            
            if( ! s->Attribute( "name" ) ) goto badPreset;
            preset.name = s->Attribute( "name" );
            
            if( s->QueryIntAttribute( "type", &t ) != TIXML_SUCCESS ) goto badPreset;
            preset.type = t;
            
            for( int i=0; i<n_fx_params; ++i )
            {
               double fl;
               std::string p = "p";
               if( s->QueryDoubleAttribute( (p + std::to_string(i)).c_str(), &fl ) == TIXML_SUCCESS )
               {
                  preset.p[i] = fl;
               }
               if( s->QueryDoubleAttribute( (p + std::to_string(i) + "_temposync" ).c_str(), &fl ) == TIXML_SUCCESS && fl != 0 )
               {
                  preset.ts[i] = true;
               }
               if( s->QueryDoubleAttribute( (p + std::to_string(i) + "_extend_range").c_str(), &fl ) == TIXML_SUCCESS && fl != 0 )
               {
                  preset.er[i] = fl;
               }
            }
            if( userPresets.find(preset.type) == userPresets.end() )
               userPresets[preset.type] = std::vector<UserPreset>();
            userPresets[preset.type].push_back(preset);
         }
      badPreset:
         ;
      }
   }

   for( auto &a : userPresets )
      std::sort( a.second.begin(), a.second.end(),
                 [](UserPreset a, UserPreset b)
                    {
                       if( a.type == b.type )
                       {
                          return _stricmp( a.name.c_str(), b.name.c_str() ) < 0;
                       }
                       else
                       {
                          return a.type < b.type;
                       }
                    }
         );
}

void CFxMenu::populate()
{
    /*
    ** Are there user presets
    */
    if( scanForUserPresets || true ) // for now
    {
       rescanUserPresets();
    }

    CSnapshotMenu::populate();

    /*
    ** Add copy/paste/save
    */
    
    this->addSeparator();

    auto copyItem = new CCommandMenuItem(CCommandMenuItem::Desc("Copy"));
    auto copy = [this](CCommandMenuItem* item) {
        this->copyFX();
    };
    copyItem->setActions(copy, nullptr);
    this->addEntry(copyItem);

    auto pasteItem = new CCommandMenuItem(CCommandMenuItem::Desc("Paste"));
    auto paste = [this](CCommandMenuItem* item) {
        this->pasteFX();
    };
    pasteItem->setActions(paste, nullptr);
    this->addEntry(pasteItem);

    this->addSeparator();

    if( fx->type.val.i != fxt_off )
    {
       auto saveItem = new CCommandMenuItem(
           CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Save FX Preset")));
       saveItem->setActions([this](CCommandMenuItem *item) {
                               this->saveFX();
                            });
       this->addEntry(saveItem);
    }

    
    auto rescanItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh FX Preset List")));
    rescanItem->setActions([this](CCommandMenuItem *item) {
                              scanForUserPresets = true;
                              auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
                              if( sge )
                                 sge->queueRebuildUI();
                           });
    this->addEntry(rescanItem);

}


void CFxMenu::copyFX()
{
    /*
    ** This is a junky implementation until I figure out save and load which will require me to stream this
    */
    if( fxCopyPaste.size() == 0 )
    {
        fxCopyPaste.resize( n_fx_params * 4 + 1 ); // type then (val; ts; extend; deact)
    }

    fxCopyPaste[0] = fx->type.val.i;
    for( int i=0; i<n_fx_params; ++i )
    {
        int vp = i * 4 + 1;
        int tp = i * 4 + 2;
        int xp = i * 4 + 3;
        int dp = i * 4 + 4;

        switch( fx->p[i].valtype )
        {
        case vt_float:
            fxCopyPaste[vp] = fx->p[i].val.f;
            break;
        case vt_int:
            fxCopyPaste[vp] = fx->p[i].val.i;
            break;
        }

        fxCopyPaste[tp] = fx->p[i].temposync;
        fxCopyPaste[xp] = fx->p[i].extend_range;
        fxCopyPaste[dp] = fx->p[i].deactivated;
    }
    memcpy((void*)fxbuffer,(void*)fx,sizeof(FxStorage));
    
}

void CFxMenu::pasteFX()
{
    if( fxCopyPaste.size() == 0 )
    {
        return;
    }

    fxbuffer->type.val.i = (int)fxCopyPaste[0];

    Effect* t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);
    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
        delete t_fx;
    }

    for (int i = 0; i < n_fx_params; i++)
    {
        int vp = i * 4 + 1;
        int tp = i * 4 + 2;
        int xp = i * 4 + 3;
        int dp = i * 4 + 4;
        
        switch( fxbuffer->p[i].valtype )
        {
        case vt_float:
        {
            fxbuffer->p[i].val.f = fxCopyPaste[vp];
            if( fxbuffer->p[i].val.f < fxbuffer->p[i].val_min.f )
            {
               fxbuffer->p[i].val.f = fxbuffer->p[i].val_min.f;
            }
            if( fxbuffer->p[i].val.f > fxbuffer->p[i].val_max.f )
            {
               fxbuffer->p[i].val.f = fxbuffer->p[i].val_max.f;
            }
        }
        break;
        case vt_int:
            fxbuffer->p[i].val.i = (int)fxCopyPaste[vp];
            break;
        default:
            break;
        }
        fxbuffer->p[i].temposync = (int)fxCopyPaste[tp];
        fxbuffer->p[i].extend_range = (int)fxCopyPaste[xp];
        fxbuffer->p[i].deactivated = (int)fxCopyPaste[dp];
    }

    selectedName = std::string( "Copied " ) + fxtype_abberations[ fxbuffer->type.val.i ];
    
    if( listenerNotForParent )
       listenerNotForParent->valueChanged( this );

}

void CFxMenu::saveFX()
{
   // Rough implementation
   char fxName[256];
   fxName[0] = 0;
   spawn_miniedit_text(fxName, 256, "Enter a name for the FX preset:", "Save FX Preset" );

   if( strlen( fxName ) == 0 )
   {
      return;
   }

   if( ! Surge::Storage::isValidName( fxName ) )
   {
      return;
   }
   
   std::string pn = storage->userFXPath;
   fs::create_directories( pn );

   std::ostringstream oss;
   oss << pn
#if WINDOWS
        << "\\"
#else
        << "/"
#endif
       << fxName << ".srgfx";
   auto fn = oss.str();
   std::ofstream pfile( fn, std::ios::out );
   if( ! pfile.is_open() )
   {
      Surge::UserInteractions::promptError( std::string( "Unable to open FX preset file '" ) + fn + "' for writing!",
                                            "Surge IO Error" );
      return;
   }

   pfile << "<single-fx streaming_versio=\"" << ff_revision << "\">\n";

   // take care of 5 special XML characters
   std::string fxNameSub(fxName);
   Surge::Storage::findReplaceSubstring(fxNameSub, std::string("&"), std::string("&amp;"));
   Surge::Storage::findReplaceSubstring(fxNameSub, std::string("<"), std::string("&lt;"));
   Surge::Storage::findReplaceSubstring(fxNameSub, std::string(">"), std::string("&gt;"));
   Surge::Storage::findReplaceSubstring(fxNameSub, std::string("\""), std::string("&quot;"));
   Surge::Storage::findReplaceSubstring(fxNameSub, std::string("'"), std::string("&apos;"));
   
   pfile << "  <snapshot name=\"" << fxNameSub.c_str() << "\" \n";

   pfile << "     type=\"" <<  fx->type.val.i << "\"\n";
   for( int i=0; i<n_fx_params; ++i )
   {
      if( fx->p[i].ctrltype != ct_none )
      {
         switch( fx->p[i].valtype )
         {
         case vt_float:
            pfile << "     p" << i << "=\"" << fx->p[i].val.f << "\"\n";
            break;
         case vt_int:
            pfile << "     p" << i << "=\"" << fx->p[i].val.i << "\"\n";
            break;
         }
         
         if( fx->p[i].can_temposync() && fx->p[i].temposync )
         {
            pfile << "     p" << i << "_temposync=\"1\"\n";
         }
         if( fx->p[i].can_extend_range() && fx->p[i].extend_range )
         {
            pfile << "     p" << i << "_extend_range=\"1\"\n";
         }
      }
   }

   pfile << "  />\n";
   pfile << "</single-fx>\n";
   pfile.close();

   scanForUserPresets = true;
   auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
   if( sge )
      sge->queueRebuildUI();
}

void CFxMenu::loadUserPreset( const UserPreset &p )
{
   fxbuffer->type.val.i = p.type;

   Effect* t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);
   if (t_fx)
   {
      t_fx->init_ctrltypes();
      t_fx->init_default_values();
      delete t_fx;
   }

   for (int i = 0; i < n_fx_params; i++)
   {
      switch( fxbuffer->p[i].valtype )
      {
      case vt_float:
      {
         fxbuffer->p[i].val.f = p.p[i];
      }
      break;
      case vt_int:
         fxbuffer->p[i].val.i = (int)p.p[i];
         break;
      default:
         break;
      }
      fxbuffer->p[i].temposync = (int)p.ts[i];;
      fxbuffer->p[i].extend_range = (int)p.er[i];
   }

   selectedIdx = -1;
   selectedName = p.name;
   
   if( listenerNotForParent )
      listenerNotForParent->valueChanged( this );

}

void CFxMenu::addToTopLevelTypeMenu(TiXmlElement *type, VSTGUI::COptionMenu *subMenu, int &idx)
{
   if( ! type || ! subMenu ) return;

   int type_id = 0;
   type->Attribute("i", &type_id);

   if( userPresets.find(type_id) == userPresets.end() || userPresets[type_id].size() == 0 )
      return;

   auto factory_add = subMenu->addEntry("FACTORY PRESETS", 0);
   factory_add->setEnabled(0);

   auto user_add = subMenu->addEntry("USER PRESETS");
   user_add->setEnabled(0);

   int fidx = idx + 10000;
   for( auto &ps : userPresets[type_id] )
   {
      auto fxName = ps.name;

#if WINDOWS
      Surge::Storage::findReplaceSubstring(fxName, std::string("&"), std::string("&&"));
#endif

      auto i = new CCommandMenuItem(CCommandMenuItem::Desc(fxName.c_str()));
      i->setActions([this,ps](CCommandMenuItem *item)
                       {
                          this->loadUserPreset(ps);
                       } );
      subMenu->addEntry(i);
   }
}
