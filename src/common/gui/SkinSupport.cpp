/*
** Various support of skin operations for the rudimentary skinning engine
*/

#include "SkinSupport.h"
#include "SurgeStorage.h"
#include "SurgeBitmaps.h"
#include "UserInteractions.h"
#include "UserDefaults.h"
#include "UIInstrumentation.h"
#include "CScalableBitmap.h"

#include "ImportFilesystem.h"

#include <iostream>
#include <iomanip>

namespace Surge
{
namespace UI
{

const std::string Skin::defaultImageIDPrefix = "DEFAULT/";
std::ostringstream SkinDB::errorStream;
   
SkinDB& Surge::UI::SkinDB::get()
{
   static SkinDB instance;
   return instance;
}

SkinDB::SkinDB()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SkinDB::SkinDB" );
#endif   
   // std::cout << "Constructing SkinDB" << std::endl;
}

SkinDB::~SkinDB()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SkinDB::~SkinDB" );
#endif   
   skins.clear(); // Not really necessary but means the skins are destroyed before the rest of the
                  // dtor runs
   // std::cout << "Destroying SkinDB" << std::endl;
}

std::shared_ptr<Skin> SkinDB::defaultSkin(SurgeStorage *storage)
{
   rescanForSkins(storage);
   auto uds = Surge::Storage::getUserDefaultValue( storage, "defaultSkin", "" );
   if( uds == "" )
      return getSkin(defaultSkinEntry);
   else
   {
      for( auto e : availableSkins )
      {
         if( e.name == uds )
            return getSkin(e);
      }
      return getSkin(defaultSkinEntry);
   }
}

std::shared_ptr<Skin> SkinDB::getSkin(const Entry& skinEntry)
{
   if (skins.find(skinEntry) == skins.end())
   {
      skins[skinEntry] = std::make_shared<Skin>(skinEntry.root, skinEntry.name);
   }
   return skins[skinEntry];
}

void SkinDB::rescanForSkins(SurgeStorage* storage)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "SkinDB::rescanForSkins" );
#endif   
   availableSkins.clear();

   std::vector<std::string> paths = {storage->datapath, storage->userDataPath};

   for (auto sourceS : paths)
   {
      fs::path source(sourceS);
      std::vector<fs::path> candidates;

      std::vector<fs::path> alldirs;
      std::deque<fs::path> workStack;
      workStack.push_back(source);
      while (!workStack.empty())
      {
         auto top = workStack.front();
         workStack.pop_front();
         if( fs::is_directory(top) )
         {
            for (auto& d : fs::directory_iterator(top))
            {
               if (fs::is_directory(d))
               {
                  alldirs.push_back(d);
                  workStack.push_back(d);
               }
            }
         }
      }

      for (auto& p : alldirs)
      {
         std::string name;
#if WINDOWS && !TARGET_RACK
         /*
         ** Windows filesystem names are properly wstrings which, if we want them to
         ** display properly in vstgui, need to be converted to UTF8 using the
         ** windows widechar API. Linux and Mac do not require this.
         */
         std::wstring str = p.wstring();
         name = Surge::Storage::wstringToUTF8(str);
#else
         name = p.generic_string();
#endif

         std::string ending = ".surge-skin";
         if (name.length() >= ending.length() &&
             0 == name.compare(name.length() - ending.length(), ending.length(), ending))
         {
#if WINDOWS
            char sep = '\\';
#else
            char sep = '/';
#endif
            auto sp = name.rfind(sep);
            if (sp == std::string::npos)
            {
            }
            else
            {
               auto path = name.substr(0, sp + 1);
               auto lo = name.substr(sp + 1);
               Entry e;
               e.root = path;
               e.name = lo + sep;
               if (e.name.find("default.surge-skin") != std::string::npos &&
                   defaultSkinEntry.name == "")
               {
                  defaultSkinEntry = e;
                  foundDefaultSkinEntry = true;
               }
               availableSkins.push_back(e);
            }
         }
      }
   }
   if( ! foundDefaultSkinEntry )
   {
      std::ostringstream oss;
      oss << "Surge Default Skin was not located. This usually means Surge is mis-installed or using an incompatible "
          << "set of assets. Surge looked in '" << storage->datapath << "' and '" << storage->userDataPath << "'. "
          << "You can fix this by re-installing or removing the offending incompatible resources.";
      Surge::UserInteractions::promptError( oss.str(),
                                            "Skin Support Error" );
   }

   // Run over the skins parsing the name
   for( auto &e : availableSkins )
   {
      auto x = e.root + e.name + "/skin.xml";

      TiXmlDocument doc;
      // Obviously fix this
      doc.SetTabSize(4);
      
      if (!doc.LoadFile(x) || doc.Error())
      {
         e.displayName = e.name + " (parse error)";
         continue;
      }
      TiXmlElement* surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
      if (!surgeskin)
      {
         e.displayName = e.name + " (no skin element)";
         continue;
      }

      const char* a;
      if( ( a = surgeskin->Attribute("name") ) )
      {
         e.displayName = a;
      }
      else
      {
         e.displayName = e.name + " (no name att)";
      }
   }

   std::sort( availableSkins.begin(), availableSkins.end(),
              [](const SkinDB::Entry &a, const SkinDB::Entry &b) {
                 return _stricmp( a.displayName.c_str(), b.displayName.c_str() ) < 0;
              } );
}

// see scripts/misc/idmap.pl if you want to regen this
std::unordered_map<std::string, int> createIdNameMap()
{
   std::unordered_map<std::string, int> res;
   res["BG"] = 102;
   res["FADERV_BG"] = 105;
   res["FILTERBUTTONS"] = 108;
   res["OSCSWITCH"] = 110;
   res["FBCONFIG"] = 112;
   res["SCENESWITCH"] = 113;
   res["SCENEMODE"] = 114;
   res["OCTAVES_OSC"] = 117;
   res["OCTAVES"] = 118;
   res["OSCMENU"] = 119;
   res["WAVESHAPER"] = 120;
   res["RELATIVE_TOGGLE"] = 121;
   res["OSCSELECT"] = 122;
   res["POLYMODE"] = 123;
   res["MODSRC_BG"] = 124;
   res["SWITCH_KTRK"] = 125;
   res["SWITCH_RETRIGGER"] = 126;
   res["NUMBERS"] = 127;
   res["MODSRC_SYMBOL"] = 128;
   res["FADERH_LABELS"] = 131;
   res["SWITCH_SOLO"] = 132;
   res["SWITCH_FM"] = 133;
   res["SWITCH_MUTE"] = 134;
   res["CONF"] = 135;
   res["FXCONF_SYMBOLS"] = 136;
   res["FXCONF"] = 137;
   res["SWITCH_TEMPOSYNC"] = 140;
   res["SWITCH_LINK"] = 140;
   res["VFADER_MINI_BG_BLACK"] = 141;
   res["OSCROUTE"] = 143;
   res["FXBYPASS"] = 144;
   res["ENVSHAPE"] = 145;
   res["LFOTRIGGER"] = 146;
   res["BUTTON_STORE"] = 148;
   res["BUTTON_MINUSPLUS"] = 149;
   res["BUTTON_CHECK"] = 150;
   res["FMCONFIG"] = 151;
   res["UNIPOLAR"] = 152;
   res["FADERH_HANDLE"] = 153;
   res["FADERH_BG"] = 154;
   res["FADERV_HANDLE"] = 157;
   res["ABOUT"] = 158;
   res["BUTTON_ABOUT"] = 159;
   res["FILTERSUBTYPE"] = 160;
   res["CHARACTER"] = 161;
   res["ENVMODE"] = 162;
   res["STOREPATCH"] = 163;
   res["BUTTON_MENU"] = 164;
   return res;
}

std::atomic<int> Skin::instances( 0 );

Skin::Skin(std::string root, std::string name) : root(root), name(name)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "Skin::Skin" );
#endif   
   instances++;
   // std::cout << "Constructing a skin " << _D(root) << _D(name) << _D(instances) << std::endl;
   imageIds = createIdNameMap();
}

Skin::~Skin()
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "Skin::~Skin" );
#endif   
   instances--;
   // std::cout << "Destroying a skin " << _D(instances) << std::endl;
}

#if !defined(TINYXML_SAFE_TO_ELEMENT)
#define TINYXML_SAFE_TO_ELEMENT(expr) ((expr) ? (expr)->ToElement() : NULL)
#endif

bool Skin::reloadSkin(std::shared_ptr<SurgeBitmaps> bitmapStore)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "Skin::reloadSkin" );
   Surge::Debug::TimeThisBlock _trs_( "Skin::reloadSkin" );
#endif   
   // std::cout << "Reloading skin " << _D(name) << std::endl;
   TiXmlDocument doc;
   // Obviously fix this
   doc.SetTabSize(4);

   if (!doc.LoadFile(resourceName("skin.xml")) || doc.Error())
   {
      FIXMEERROR << "Unable to load skin.xml resource '" << resourceName("skin.xml") << "'"
                 << std::endl;
      FIXMEERROR << "Unable to parse bskin.xml\nError is:\n"
                 << doc.ErrorDesc() << " at row=" << doc.ErrorRow() << " col=" << doc.ErrorCol()
                 << std::endl;
      return false;
   }

   TiXmlElement* surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
   if (!surgeskin)
   {
      FIXMEERROR << "There is no top level suge-skin node in skin.xml" << std::endl;
      return true;
   }
   const char* a;
   displayName = name;
   author = "";
   authorURL = "";
   if ( (a = surgeskin->Attribute("name") ) )
      displayName = a;
   if ( (a = surgeskin->Attribute("author") ) )
      author = a;
   if ( ( a = surgeskin->Attribute("authorURL") ) )
      authorURL = a;

   int version = -1;
   if( ! ( surgeskin->QueryIntAttribute( "version", &version ) == TIXML_SUCCESS ) )
   {
      FIXMEERROR << "Skin file does not contain a 'version' attribute. You must contain a version at most " << Skin::current_format_version << std::endl;
      return false;
   }
   if( version > Skin::current_format_version )
   {
      FIXMEERROR << "Skin version '" << version << "' is greater than the max version '"
                 << Skin::current_format_version << "' supported by this binary" << std::endl;
      return false;
   }
   
   auto globalsxml = TINYXML_SAFE_TO_ELEMENT(surgeskin->FirstChild("globals"));
   auto componentclassesxml = TINYXML_SAFE_TO_ELEMENT(surgeskin->FirstChild("component-classes"));
   auto controlsxml = TINYXML_SAFE_TO_ELEMENT(surgeskin->FirstChild("controls"));
   if (!globalsxml)
   {
      FIXMEERROR << "surge-skin contains no globals element" << std::endl;
      return false;
   }
   if(!componentclassesxml)
   {
      FIXMEERROR << "surge-skin contains no component-classes element" << std::endl;
      return false;
   }
   if (!controlsxml)
   {
      FIXMEERROR << "surge-skin contains no controls element" << std::endl;
      return false;
   }

   /*
   ** Parse the globals section
   */
   globals.clear();
   for (auto gchild = globalsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (lkid)
      {
         std::string key = lkid->Value();
         props_t amap;
         for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            amap[a->Name()] = a->Value();
         globals.push_back(std::make_pair(key, amap));
      }
   }

   /*
   ** Pasrse the controls section
   */
   auto attrint = [](TiXmlElement* e, const char* a) {
      const char* av = e->Attribute(a);
      if (!av)
         return -1;
      return std::atoi(av);
   };

   auto attrstr = [](TiXmlElement *e, const char* a ) {
                     const char* av = e->Attribute(a);
                     if( !av )
                        return std::string();
                     return std::string( av );
                  };

   controls.clear();
   rootControl = std::make_shared<ControlGroup>();
   bool rgp = recursiveGroupParse( rootControl, controlsxml );
   if( ! rgp )
   {
      FIXMEERROR << "Recursive Group Parse failed" << std::endl;
      return false;
   }
    

   componentClasses.clear();
   for (auto gchild = componentclassesxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (!lkid)
         continue;

      if (std::string(lkid->Value()) != "class")
      {
         FIXMEERROR << "INVALID CLASS" << std::endl;
         continue;
      }

      auto c = std::make_shared<Skin::ComponentClass>();

      c->name = attrstr( lkid, "name" );
      if( c->name == "" )
      {
         FIXMEERROR << "INVALUD NAME" << std::endl;
         return false;
      }

      if( componentClasses.find( c->name ) != componentClasses.end() )
      {
         FIXMEERROR << "Double Definition" << std::endl;
         return false;
      }
      
      for (auto a = lkid->FirstAttribute(); a; a = a->Next())
         c->allprops[a->Name()] = a->Value();

      componentClasses[c->name] = c;

   };

   // process the images and colors
   for (auto g : globals)
   {
      if (g.first == "defaultimage")
      {
         auto path = resourceName(g.second["directory"]);
         fs::path source(path);
         for (auto& d : fs::directory_iterator(source))
         {
            auto pos = d.path().generic_string().find("bmp001");
            if (pos != std::string::npos)
            {
               int idx = std::atoi(d.path().generic_string().c_str() + pos + 3);
               bitmapStore->loadBitmapByPathForID(d.path().generic_string(), idx);
            }
            else
            {
               std::string id = defaultImageIDPrefix + d.path().filename().generic_string();
               bitmapStore->loadBitmapByPathForStringID(d.path().generic_string(), id);
            }
         }
      }
   }

   colors.clear();
   queried_colors.clear();
   for (auto g : globals)
   {
      if (g.first == "image")
      {
         auto p = g.second;
         auto id = p["id"];
         auto res = p["resource"];
         // FIXME = error handling
         if (res.size() > 0)
         {
            if (id.size() > 0)
            {
               if( imageIds.find(id) != imageIds.end() )
                  bitmapStore->loadBitmapByPathForID(resourceName(res), imageIds[id]);
               else
               {
                  bitmapStore->loadBitmapByPathForStringID(resourceName(res), id );
               }
            }
            else
            {
               bitmapStore->loadBitmapByPath(resourceName(res));
            }
         }
      }
      if (g.first == "color")
      {
         auto p = g.second;
         auto id = p["id"];
         auto val = p["value"];
         auto r = VSTGUI::CColor();
         if (val[0] == '#')
         {
            uint32_t rgb;
            sscanf(val.c_str() + 1, "%x", &rgb);

            int b = rgb % 256;
            rgb = rgb >> 8;

            int g = rgb % 256;
            rgb = rgb >> 8;

            int r = rgb % 256;
            colors[id] = ColorStore( VSTGUI::CColor(r, g, b) );
         }
         else if( val[0] == '$' )
         {
            colors[id] = ColorStore( val.c_str() + 1 );
         }
         else {
            colors[id] = ColorStore( VSTGUI::CColor(255, 0, 0) );
         }
      }
   }

   // Resolve the ultimate parent classes
   for( auto &c : controls )
   {
      c->ultimateparentclassname = c->classname;
      while( componentClasses.find( c->ultimateparentclassname ) != componentClasses.end() )
      {
         auto comp = componentClasses[c->ultimateparentclassname];
         if( comp->allprops.find( "parent" ) != comp->allprops.end() )
         {
            c->ultimateparentclassname = componentClasses[c->ultimateparentclassname]->allprops["parent"];
         }
         else
         {
            FIXMEERROR << "PARENT CLASS DOESN'T RESOLVE FOR " << c->ultimateparentclassname << std::endl;
            return false;
            break;
         }
      }
   }
   return true;
}

bool Skin::recursiveGroupParse( ControlGroup::ptr_t parent, TiXmlElement *controlsxml, std::string pfx )
{
   // I know I am gross for copying these
   auto attrint = [](TiXmlElement* e, const char* a) {
                     const char* av = e->Attribute(a);
                     if (!av)
                        return -1;
                     return std::atoi(av);
                  };
   
   auto attrstr = [](TiXmlElement *e, const char* a ) {
                     const char* av = e->Attribute(a);
                     if( !av )
                        return std::string();
                     return std::string( av );
                  };

   for (auto gchild = controlsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (!lkid)
         continue;

      if( std::string( lkid->Value()) == "group" )
      {
         auto g = std::make_shared<Skin::ControlGroup>();

         g->x = attrint( lkid, "x" ); if( g->x < 0 ) g->x = 0; g->x += parent->x;
         g->y = attrint( lkid, "y" ); if( g->y < 0 ) g->y = 0; g->y += parent->y;
         g->w = attrint( lkid, "w" );
         g->h = attrint( lkid, "h" );
         
         parent->childGroups.push_back(g);
         if (! recursiveGroupParse( g, lkid, pfx + "|--"  ) )
         {
            return false;
         }
      }
      else if ( (std::string(lkid->Value()) == "control") || (std::string(lkid->Value()) == "label" ))
      {
         auto c = std::make_shared<Skin::Control>();
         c->x = attrint(lkid, "x") + parent->x;
         c->y = attrint(lkid, "y") + parent->y;
         c->w = attrint(lkid, "w");
         c->h = attrint(lkid, "h");

         c->classname  = attrstr(lkid, "class" );
         if( std::string( lkid->Value() ) == "label" )
         {
            c->type = Control::Type::LABEL;
         }
         else if( lkid->Attribute( "tag_value" ) )
         {
            c->type = Control::Type::ENUM;
            c->enum_id = attrint( lkid, "tag_value" );
            c->enum_name = attrstr( lkid, "tag_name" );
         }
         else
         {
            c->type = Control::Type::UIID;
            c->ui_id = attrstr( lkid, "ui_identifier" );
         }
         
         for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            c->allprops[a->Name()] = a->Value();

         controls.push_back(c);
         parent->childControls.push_back(c);
      }
      else
      {
         FIXMEERROR << "INVALID CONTROL" << std::endl;
         return false;
      }
   }
   return true;
}

bool Skin::hasColor(std::string id)
{
   if( id[0] == '$' ) // when used as a value in a control you can have the $ or not, even though internally we strip it
      id = std::string( id.c_str() + 1 );
   queried_colors.insert(id);
   return colors.find(id) != colors.end();
}
VSTGUI::CColor Skin::getColor(std::string id, const VSTGUI::CColor& def, std::unordered_set<std::string> noLoops)
{
   if( id[0] == '$' )
      id = std::string( id.c_str() + 1 );
   if( noLoops.find( id ) != noLoops.end() )
   {
      std::ostringstream oss;
      oss << "Resoving color '" << id << "' resulted in a loop. Please check your XML. Colors which were visited during traversal are: ";
      for( auto l : noLoops )
         oss << "'" << l << "' ";
      // FIXME ERROR
      Surge::UserInteractions::promptError( oss.str(), "Skin Configuration Error" );
      return def;
   }
   queried_colors.insert(id);
   noLoops.insert(id);
   if (colors.find(id) != colors.end())
   {
      auto c = colors[id];
      switch( c.type )
      {
      case ColorStore::COLOR:
         return c.color;
      case ColorStore::ALIAS:
         return getColor( c.alias, def, noLoops );
      }
   }
   return def;
}


CScalableBitmap *Skin::backgroundBitmapForControl( Skin::Control::ptr_t c, std::shared_ptr<SurgeBitmaps> bitmapStore )
{
   CScalableBitmap *bmp = nullptr;
   auto ms = propertyValue( c, "bg_id" );
   if( ms.isJust() )
   {
      bmp = bitmapStore->getBitmap(std::atoi(ms.fromJust().c_str()));
   }
   else
   {
      auto mr = propertyValue( c, "bg_resource" );
      if( mr.isJust() )
      {
         bmp = bitmapStore->getBitmapByStringID( mr.fromJust() );
         if( ! bmp )
            bmp = bitmapStore->loadBitmapByPathForStringID( resourceName( mr.fromJust() ), mr.fromJust() );
      }
   }
   return bmp;
}

CScalableBitmap *Skin::hoverBitmapOverlayForBackgroundBitmap( Skin::Control::ptr_t c, CScalableBitmap *b, std::shared_ptr<SurgeBitmaps> bitmapStore, HoverType t )
{
   if( ! bitmapStore.get() )
   {
      return nullptr;
   }
   if( c.get() )
   {
      std::cout << "TODO: The component may have a name for a hover asset type=" << t << " component=" << c->toString() << std::endl;
   }
   if( ! b )
   {
      return nullptr;
   }
   std::ostringstream sid;
   if( b->resourceID < 0 )
   {
      // we got a skin
      auto pos = b->fname.find( "bmp00" );
      if( pos != std::string::npos )
      {
         auto b4 = b->fname.substr( 0, pos );
         auto ftr = b->fname.substr( pos + 3 );

         switch(t)
         {
         case HOVER:
            sid << defaultImageIDPrefix << "hover" << ftr;
            break;
         case HOVER_OVER_ON:
            sid << defaultImageIDPrefix << "hoverOn" << ftr;
            break;
         }
         auto bmp = bitmapStore->getBitmapByStringID( sid.str() );
         if( bmp )
            return bmp;
      }
   }
   else
   {
      switch(t)
      {
      case HOVER:
         sid << defaultImageIDPrefix << "hover" << std::setw(5) << std::setfill('0') << b->resourceID << ".svg";
         break;
      case HOVER_OVER_ON:
         sid << defaultImageIDPrefix << "hoverOn" << std::setw(5) << std::setfill('0') << b->resourceID << ".svg";
         break;
      }
      auto bmp = bitmapStore->getBitmapByStringID( sid.str() );
      if( bmp )
         return bmp;
   }

   return nullptr;
}

} // namespace UI
} // namespace Surge
