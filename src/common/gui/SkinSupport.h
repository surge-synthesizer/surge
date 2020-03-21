// -*-c++-*-

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "vstgui/lib/ccolor.h"

#define _D(x) " " << (#x) << "=" << x

/*
** Support for rudimentary skinning in Surge
**
** SkinSupport provides a pair of classes, a SkinManager and a SkinDB
** The SkinManager singleton loads and applys the SkinDB to various places as
** appropriate. The SkinDB has all the information you would need 
** to skin yourself, and answers various queries.
*/

class SurgeStorage;
class SurgeBitmaps;
class CScalableBitmap;

namespace VSTGUI {
   class CFrame;
}

#define FIXMEERROR std::cout

namespace Surge
{
namespace UI
{


class SkinDB;
   
class Skin
{
public:
   typedef std::shared_ptr<Skin> ptr_t;
   typedef std::unordered_map<std::string, std::string> props_t;
   
   friend class SkinDB;

   Skin(std::string root, std::string name);
   ~Skin();

   void reloadSkin(std::shared_ptr<SurgeBitmaps> bitmapStore);

   std::string resourceName(std::string relativeName) {
      return root + name + "/" + relativeName;
   }
   
   std::string root;
   std::string name;

   std::string displayName;
   std::string author;
   std::string authorURL;
   
   struct Control
   {
      int x, y, w, h, posx, posy, posy_offset;

      int enum_id;
      std::string ui_id;
      typedef enum {
         ENUM,
         UIID
      } Type;
      Type type;
      props_t instance_info;
   };

   bool hasColor( std::string id );
   VSTGUI::CColor getColor( std::string id, const VSTGUI::CColor &def );
   std::unordered_set<std::string> getQueriedColors() { return queried_colors; }
   
private:
   std::vector<std::pair<std::string, props_t>> globals;

   struct ColorStore {
      VSTGUI::CColor color;
      std::string alias;

      typedef enum {
         COLOR,
         ALIAS
      } Type;

      Type type;
      ColorStore() : type( COLOR ), color( VSTGUI::kBlackCColor ) { }
      ColorStore( VSTGUI::CColor c ) : type( COLOR ), color( c ) { }
      ColorStore( std::string a ) : type( ALIAS ), alias( a ) { }
   };
   std::unordered_map<std::string, ColorStore> colors;
   std::unordered_set<std::string> queried_colors;
   std::unordered_map<std::string, int> imageIds;
   std::vector<Control> controls;
};
   
class SkinDB
{
public:
   static SkinDB& get(SurgeStorage *);

   struct Entry {
      std::string root;
      std::string name;

      // Since we want to use this as a map
      bool operator==(const Entry &o) const {
         return root == o.root && name == o.name;
      }

      struct hash {
         size_t operator()(const Entry & e) const {
            std::hash<std::string> h;
            return h(e.root) ^ h(e.name);
         }
      };
   };
   
   void rescanForSkins(SurgeStorage *);
   std::vector<Entry> getAvailableSkins() { return availableSkins; }
   Skin::ptr_t getSkin( const Entry &skinEntry );
   Skin::ptr_t defaultSkin();

private:
   SkinDB(SurgeStorage *);
   ~SkinDB();
   SkinDB(const SkinDB&) = delete; 
   SkinDB& operator=(const SkinDB&) = delete; 
   SurgeStorage *storage;
   
   std::vector<Entry> availableSkins;
   std::unordered_map<Entry,std::shared_ptr<Skin>, Entry::hash> skins;
   Entry defaultSkinEntry;
   bool foundDefaultSkinEntry = false;
   
   static std::shared_ptr<SkinDB> instance;
};


class SkinConsumingComponnt {
public:
   virtual void setSkin( Skin::ptr_t s ) { skin = s; }
protected:
   Skin::ptr_t skin;
};
}
}
