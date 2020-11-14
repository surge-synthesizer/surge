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

#include "filesystem/import.h"

#include <array>
#include <iostream>
#include <iomanip>

namespace Surge
{
namespace UI
{


const std::string NoneClassName = "none";
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
   skins.clear(); // Not really necessary but means the skins are destroyed before the rest of the dtor runs
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

   std::array<fs::path, 2> paths = {string_to_path(storage->datapath),
                                    string_to_path(storage->userDataPath)};

   for (auto& source : paths)
   {
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
         name = path_to_string(p);

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
      oss << "Surge Classic skin was not located. This usually means Surge is incorrectly installed or uses an incompatible "
          << "set of resources. Surge looked in '" << storage->datapath << "' and '" << storage->userDataPath << "'. "
          << "Please reinstall Surge or remove incompatible resources.";
      Surge::UserInteractions::promptError( oss.str(),
                                            "Skin Loading Error" );
   }

   // Run over the skins parsing the name
   for( auto &e : availableSkins )
   {
      auto x = e.root + e.name + "/skin.xml";

      TiXmlDocument doc;
      // Obviously fix this
      doc.SetTabSize(4);
      
      if (!doc.LoadFile(string_to_path(x)))
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

      e.category = "";
      if( ( a = surgeskin->Attribute( "category" ) ) )
      {
         e.category = a;
      }
   }

   std::sort( availableSkins.begin(), availableSkins.end(),
              [](const SkinDB::Entry &a, const SkinDB::Entry &b) {
                 return _stricmp( a.displayName.c_str(), b.displayName.c_str() ) < 0;
              } );
}

// Define the inverse maps
#include "SkinImageMaps.h"

std::atomic<int> Skin::instances( 0 );

Skin::Skin(const std::string &root, const std::string &name) : root(root), name(name)
{
#ifdef INSTRUMENT_UI
   Surge::Debug::record( "Skin::Skin" );
#endif   
   instances++;
   // std::cout << "Constructing a skin " << _D(root) << _D(name) << _D(instances) << std::endl;
   imageStringToId = createIdNameMap();
   imageAllowedIds = allowedImageIds();
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

   if (!doc.LoadFile(string_to_path(resourceName("skin.xml"))))
   {
      FIXMEERROR << "Unable to load skin.xml resource '" << resourceName("skin.xml") << "'"
                 << std::endl;
      FIXMEERROR << "Unable to parse skin.xml\nError is:\n"
                 << doc.ErrorDesc() << " at row " << doc.ErrorRow() << ", column " << doc.ErrorCol()
                 << std::endl;
      return false;
   }

   TiXmlElement* surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
   if (!surgeskin)
   {
      FIXMEERROR << "There is no top level surge-skin node in skin.xml" << std::endl;
      return true;
   }
   const char* a;
   displayName = name;
   author = "";
   authorURL = "";
   category = "";
   if ( (a = surgeskin->Attribute("name") ) )
      displayName = a;
   if ( (a = surgeskin->Attribute("author") ) )
      author = a;
   if ( ( a = surgeskin->Attribute("authorURL") ) )
      authorURL = a;
   if( ( a = surgeskin->Attribute( "category" ) ) )
      category = a;

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
    * We have a reasonably valid skin so try and add fonts
    */
   auto fontPath = string_to_path( resourceName("fonts"));
   if( fs::is_directory(fontPath ))
   {
      addFontSearchPathToSystem(fontPath);
   }

   /*
   ** Parse the globals section
   */
   globals.clear();
   zooms.clear();
   for (auto gchild = globalsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
   {
      auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
      if (lkid)
      {
         std::string key = lkid->Value();
         props_t amap;
         for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            amap[a->Name()] = a->Value();

         auto r = GlobalPayload(amap);
         for( auto qchild = lkid->FirstChild(); qchild; qchild = qchild->NextSibling())
         {
            auto qkid = TINYXML_SAFE_TO_ELEMENT(qchild);
            if( qkid )
            {
               std::string k = qkid->Value();
               props_t kmap;
               for( auto a = qkid->FirstAttribute(); a; a = a->Next() )
                  kmap[a->Name()] = a->Value();
               r.children.push_back(std::make_pair(k,kmap));
            }
         }

         globals.push_back(std::make_pair(key, r));
      }
   }

   /*
   ** Parse the controls section
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
         FIXMEERROR << "INVALID NAME" << std::endl;
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
         const auto path = resourceName(g.second.props["directory"]);
         fs::path source(string_to_path(path));
         for (const fs::path& d : fs::directory_iterator(source))
         {
            const auto pathStr = path_to_string(d);
            const auto pos = pathStr.find("bmp");
            if (pos != std::string::npos)
            {
               auto postbmp = pathStr.substr(pos+3);
               int idx = std::atoi(pathStr.c_str() + pos + 3);
               // We epxpect 5 digits and a .svg or .png
               auto xtn = pathStr.substr( pos + 8 );

               if( ( xtn == ".svg" || xtn == ".png" ) &&
                   ( imageAllowedIds.find(idx) != imageAllowedIds.end()))
               {
                  bitmapStore->loadBitmapByPathForID(pathStr, idx);
               }
            }
            else
            {
               std::string id = defaultImageIDPrefix + path_to_string(d.filename());
               bitmapStore->loadBitmapByPathForStringID(pathStr, id);
            }
         }
      }
   }

   colors.clear();
   bgimg = "";
   szx = BASE_WINDOW_SIZE_X;
   szy = BASE_WINDOW_SIZE_Y;
   for (auto g : globals)
   {
      if (g.first == "image")
      {
         auto p = g.second.props;
         auto id = p["id"];
         auto res = p["resource"];
         // FIXME = error handling
         if (res.size() > 0)
         {
            if (id.size() > 0)
            {
               if( imageStringToId.find(id) != imageStringToId.end() )
                  bitmapStore->loadBitmapByPathForID(resourceName(res), imageStringToId[id]);
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
      if( g.first == "multi-image" )
      {
         // FIXME error checking
         auto props = g.second.props;
         if( props.find( "id" ) == props.end() )
         {
            FIXMEERROR << "multi-image must contain an id";
         }
         auto id = props["id"];

         if( id.size() > 0 )
         {
            bool validKids = true;
            auto kids = g.second.children;
            // Go find the 100 one first
            CScalableBitmap *bm = nullptr;
            for( auto k : kids ) {
               if( k.second.find( "zoom-level" ) == k.second.end() ||
                   k.second.find("resource") == k.second.end()
                   )
               {
                  validKids = false;
                  //for( auto kk : k.second )
                     //std::cout << _D(kk.first) << _D(kk.second) << std::endl;
                  FIXMEERROR << "Each subchild of a multi-image must ontain a zoom-level and resource";
                  break;
               }
               else if( k.second["zoom-level"] == "100" )
               {
                  auto res = k.second["resource"];
                  if( imageStringToId.find(id) != imageStringToId.end() )
                     bm = bitmapStore->loadBitmapByPathForID(resourceName(res), imageStringToId[id]);
                  else
                  {
                     bm = bitmapStore->loadBitmapByPathForStringID(resourceName(res), id );
                  }
               }
            }
            if( bm && validKids )
            {
               for (auto k : kids)
               {
                  auto zl = k.second["zoom-level"];
                  auto zli = std::atoi( zl.c_str() );
                  auto r = k.second["resource"];
                  if( zli != 100 )
                  {
                     bm->addPNGForZoomLevel(resourceName(r), zli );
                  }
               }
            } else {
               FIXMEERROR << "invalid multi-image for some reason";
            }
         }
      }
      if( g.first == "zoom-levels" )
      {
         for( auto k : g.second.children )
         {
            if( k.second.find("zoom") != k.second.end() )
            {
               zooms.push_back( std::atoi( k.second["zoom"].c_str()));
            }
         }
         // Store the zooms in sorted order
         std::sort( zooms.begin(), zooms.end() );
      }
      if (g.first == "color")
      {
         auto p = g.second.props;
         auto id = p["id"];
         auto val = p["value"];
         auto r = VSTGUI::CColor();
         if (val[0] == '#')
         {
            colors[id] = ColorStore( colorFromHexString(val));
         }
         else if( val[0] == '$' )
         {
            colors[id] = ColorStore( val.c_str() + 1, ColorStore::UNRESOLVED_ALIAS );
         }
         else {
            // Look me up later
            colors[id] = ColorStore( val, ColorStore::UNRESOLVED_ALIAS );
         }
      }

      if( g.first == "background" && g.second.props.find( "image" ) != g.second.props.end() )
      {
         bgimg = g.second.props["image" ];
      }

      if( g.first == "window-size" )
      {
         if( g.second.props.find( "x" ) != g.second.props.end() )
         {
            szx = std::atoi( g.second.props["x"].c_str() );
            if( szx < 1 ) szx = BASE_WINDOW_SIZE_X;
         }
         if( g.second.props.find( "y" ) != g.second.props.end() )
         {
            szy = std::atoi( g.second.props["y" ].c_str() );
            if( szy < 1 ) szy = BASE_WINDOW_SIZE_Y;
         }
      }
   }


   // Resolve color aliases.
   for( auto &c : colors )
   {
      if( c.second.type == ColorStore::UNRESOLVED_ALIAS )
      {
         if( colors.find( c.second.alias ) != colors.end() )
         {
            c.second.type = ColorStore::ALIAS;
         }
         else
         {
            c.second.type = ColorStore::COLOR;
            c.second.color = VSTGUI::kRedCColor;
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

   // Modifies an int value if found (with offset)
   auto attrintif = [](TiXmlElement* e, const char* a, int &target, const int& offset = 0) {
     const char* av = e->Attribute(a);
     if (av)
     {
        target = std::atoi(av) + offset;
        return true;
     }
     return false;
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
         auto control = std::make_shared<Skin::Control>();

         if( std::string( lkid->Value() ) == "label" )
         {
            control->type = Control::Type::LABEL;
         }
         else
         {
            // allow using case insensitive UI identifiers
            auto str = attrstr(lkid, "ui_identifier");
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) { return std::tolower(c); });

            auto uid = str;
            auto conn = Surge::Skin::Connector::connectorByID(uid);
            if( !conn.payload || conn.payload->defaultComponent == Surge::Skin::Connector::NONE )
            {
               FIXMEERROR << "Got a default component of NONE for uiid '" << uid << "'" << std::endl;
            }
            else
            {
               control->copyFromConnector(conn);
               control->type = Control::Type::UIID;
               control->ui_id = uid;
            }
         }

         // Basically conditionalize each of these
         attrintif( lkid, "x", control->x, parent->x );
         attrintif( lkid, "y", control->y, parent->y );
         attrintif( lkid, "w", control->w );
         attrintif( lkid, "h", control->h );

         // Candidate class name
         auto ccn  = attrstr(lkid, "class" );
         if( ccn != "" )
            control->classname = ccn;

         std::vector<std::string> removeThese;
         for (auto a = lkid->FirstAttribute(); a; a = a->Next())
         {
            /*
             * A special case: bg_id and bg_resource are paired and in your skin
             * you only say one, but if you say bg_resource you may have picked up
             * a bg_id from the default. So queue up some removals.
             */
            if( std::string( a->Name() ) == "bg_resource" )
               removeThese.push_back( "bg_id" );
            control->allprops[a->Name()] = a->Value();
         }
         for( auto &c : removeThese )
         {
            control->allprops.erase(c);
         }

         controls.push_back(control);
         parent->childControls.push_back(control);
      }
      else
      {
         FIXMEERROR << "INVALID CONTROL" << std::endl;
         return false;
      }
   }

   /*
    * We now need to create all the base parent objects before we go
    * and resolve them, since that resolutino can be recursive when
    * looping over controls, and we don't want to actually modify controls
    * while resolving.
    */
   std::set<std::string> baseParents;
   for( auto &c : Surge::Skin::Connector::connectorsByComponentType(Surge::Skin::Connector::GROUP))
      baseParents.insert( c.payload->id );
   do {
      for( auto &bp : baseParents )
      {
         getOrCreateControlForConnector(bp);
      }
      baseParents.clear();
      for( auto c : controls )
      {
         if( c->allprops.find("base_parent") != c->allprops.end() )
         {
            auto bp = c->allprops["base_parent"];
            auto pc = controlForUIID(bp);

            if (!pc)
            {
               baseParents.insert(bp);
            }
         }
      }
   } while( ! baseParents.empty() );

   controls.shrink_to_fit();
   for( auto &c : controls )
   {
      resolveBaseParentOffsets(c);
   }
   return true;
}

VSTGUI::CColor Skin::colorFromHexString(const std::string& val) const
{
   uint32_t rgb;
   sscanf(val.c_str() + 1, "%x", &rgb);

   auto l = strlen( val.c_str() + 1 );
   int a = 255;
   if( l > 6 )
   {
      a = rgb % 256;
      rgb = rgb >> 8;
   }

   int b = rgb % 256;
   rgb = rgb >> 8;

   int g = rgb % 256;
   rgb = rgb >> 8;

   int r = rgb % 256;

   return VSTGUI::CColor(r, g, b, a);
}

bool Skin::hasColor(const std::string &iid) const
{
   auto id = iid;
   if( id[0] == '$' ) // when used as a value in a control you can have the $ or not, even though internally we strip it
      id = std::string( id.c_str() + 1 );
   return colors.find(id) != colors.end();
}

VSTGUI::CColor Skin::getColor(const std::string &iid, const VSTGUI::CColor& def, std::unordered_set<std::string> noLoops) const
{
   auto id = iid;
   if( id[0] == '$' )
      id = std::string( id.c_str() + 1 );
   if( noLoops.find( id ) != noLoops.end() )
   {
      std::ostringstream oss;
      oss << "Resoving color '" << id << "' resulted in a loop. Please check the skin definition XML file. Colors which were visited during traversal are: ";
      for( auto l : noLoops )
         oss << "'" << l << "' ";
      // FIXME ERROR
      Surge::UserInteractions::promptError( oss.str(), "Skin Configuration Error" );
      return def;
   }
   noLoops.insert(id);
   auto q = colors.find(id);
   if (q != colors.end())
   {
      auto c = q->second;
      switch( c.type )
      {
      case ColorStore::COLOR:
         return c.color;
      case ColorStore::ALIAS:
         return getColor( c.alias, def, noLoops );
      case ColorStore::UNRESOLVED_ALIAS: // This should never occur
         return VSTGUI::kRedCColor;
      }
   }

   if( id[0] == '#' )
   {
      return colorFromHexString(id);
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
      // YES it might. Show and document this
      // std::cout << "TODO: The component may have a name for a hover asset type=" << t << " component=" << c->toString() << std::endl;
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

void Surge::UI::Skin::Control::copyFromConnector(const Surge::Skin::Connector& c)
{
   x = c.payload->posx;
   y = c.payload->posy;
   w = c.payload->w;
   h = c.payload->h;
   ui_id = c.payload->id;
   type = Control::UIID;

   auto transferPropertyIf = [this,c](Surge::Skin::Connector::Properties p,  std::string target)
   {
      if( c.payload->properties.find(p) != c.payload->properties.end() )
      {
         this->allprops[target] = c.payload->properties[p];
      }
   };

   if( c.payload->parentId != "" )
   {
      this->allprops["base_parent"] = c.payload->parentId; // bit of a hack
   }

   switch( c.payload->defaultComponent )
   {
      // remember to do this null case and the non-null case above also
   case Surge::Skin::Connector::SLIDER: {
      classname = "CSurgeSlider";
      ultimateparentclassname = "CSurgeSlider";
      break;
   }
   case Surge::Skin::Connector::HSWITCH2: {
      classname = "CHSwitch2";
      ultimateparentclassname = "CHSwitch2";
      transferPropertyIf(Surge::Skin::Connector::BACKGROUND, "bg_id" );
      transferPropertyIf(Surge::Skin::Connector::ROWS, "rows" );
      transferPropertyIf( Surge::Skin::Connector::COLUMNS, "columns" );
      transferPropertyIf( Surge::Skin::Connector::FRAMES, "frames" );
      transferPropertyIf( Surge::Skin::Connector::FRAME_OFFSET, "frame_offset" );
      transferPropertyIf( Surge::Skin::Connector::DRAGGABLE_HSWITCH, "draggable" );
      break;
   }
   case Surge::Skin::Connector::SWITCH: {
      classname = "CSwitchControl";
      ultimateparentclassname = "CSwitchControl";
      transferPropertyIf( Surge::Skin::Connector::BACKGROUND, "bg_id" );
      break;
   }
   case Surge::Skin::Connector::LFO: {
      classname = "CLFOGui";
      ultimateparentclassname = "CLFOGui";
      break;
   }
   case Surge::Skin::Connector::OSCMENU: {
      classname = "COSCMenu";
      ultimateparentclassname = "COSCMenu";
      break;
   }
   case Surge::Skin::Connector::FXMENU: {
      classname = "CFXMenu";
      ultimateparentclassname = "CFXMenu";
      break;
   }
   case Surge::Skin::Connector::NUMBERFIELD: {
      classname = "CNumberField";
      ultimateparentclassname = "CNumberField";
      transferPropertyIf( Surge::Skin::Connector::NUMBERFIELD_CONTROLMODE, "numberfield_controlmode" );
      transferPropertyIf( Surge::Skin::Connector::BACKGROUND, "bg_id" );
      transferPropertyIf( Surge::Skin::Connector::TEXT_COLOR, "text_color" );
      transferPropertyIf( Surge::Skin::Connector::TEXT_HOVER_COLOR, "text_color.hover" );
      break;
   }
   case Surge::Skin::Connector::VU_METER: {
      classname = "CVUMeter";
      ultimateparentclassname = "CVUMeter";
      break;
   }
   case Surge::Skin::Connector::GROUP: {
      classname = "--GROUP--";
      ultimateparentclassname = "--GROUP--";
      break;
   }

   case Surge::Skin::Connector::CUSTOM: {
      classname = "--CUSTOM--";
      ultimateparentclassname = "--CUSTOM--";
      break;
   }

   case Surge::Skin::Connector::FILTERSELECTOR: {
      classname = "FilterSelector";
      ultimateparentclassname = "FilterSelector";
      break;
   }
   default: {
      classname = "UNKNOWN";
      ultimateparentclassname = "UNKNOWN";
      //std::cout << "SOFTWARE ERROR " << __LINE__ << " " << __FILE__ << " '" << ui_id << "' " << c.payload->defaultComponent << std::endl;
      break;
   }
   }
}

void Surge::UI::Skin::resolveBaseParentOffsets(Skin::Control::ptr_t c)
{
   if( c->parentResolved ) return;
   // When we enter here, all the objects will ahve been created by the loop above
   if( c->allprops.find("base_parent") != c->allprops.end() )
   {
      // std::cout << "Control Parent " << _D(c->x) << _D(c->y) << std::endl;
      auto bp = c->allprops["base_parent"];
      auto pc = controlForUIID(bp);
      while( pc )
      {
         /*
          * A special case: If a group has control type 'none' then my control adopts it
          * This is the case only for none.
          */
         if( pc->classname == NoneClassName )
         {
            c->classname = NoneClassName;
            c->ultimateparentclassname = NoneClassName;
         }
         c->x += pc->x;
         c->y += pc->y;
         if( pc->allprops.find( "base_parent" ) != pc->allprops.end() )
         {
            pc = controlForUIID(pc->allprops["base_parent"]);
         }
         else
         {
            pc = nullptr;
         }
      }
      // std::cout << "Control POSTPar " << _D(c->x) << _D(c->y) << std::endl;
   }
   c->parentResolved = true;
}

} // namespace UI
} // namespace Surge

