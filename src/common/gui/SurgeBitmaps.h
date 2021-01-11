#pragma once

#include "resource.h"
#include "vstgui/vstgui.h"
#include <map>
#include <atomic>
#include <algorithm>

class CScalableBitmap;

class SurgeBitmaps
{
public:
   SurgeBitmaps();
   virtual ~SurgeBitmaps();

   void setupBitmapsForFrame(VSTGUI::CFrame* f);
   void setPhysicalZoomFactor(int pzf);
   void clearAllBitmapOffscreenCaches();

   CScalableBitmap* getBitmap(int id);
   CScalableBitmap* getBitmapByPath(std::string filename);
   CScalableBitmap* getBitmapByStringID(std::string id);
   
   CScalableBitmap* loadBitmapByPath(std::string filename);
   CScalableBitmap* loadBitmapByPathForID(std::string filename, int id);
   CScalableBitmap* loadBitmapByPathForStringID(std::string filename, std::string id);

   enum StringResourceType { PATH, STRINGID } ;
   std::vector<std::string> nonResourceBitmapIDs( StringResourceType t )
   {
      std::vector<std::string> res;
      switch( t )
      {
      case PATH:
         for( auto &c : bitmap_file_registry )
            res.push_back( c.first );
      case STRINGID:
         for( auto &c : bitmap_stringid_registry )
            res.push_back( c.first );
      }
      return res;
   }
protected:
   static std::atomic<int> instances;
   
   void addEntry(int id, VSTGUI::CFrame* f);
   std::map<int, CScalableBitmap*> bitmap_registry;
   std::map<std::string, CScalableBitmap*> bitmap_file_registry;

   struct cicomp {
      bool operator() (const std::string& a, const std::string& b) const {
         std::string al = a; std::transform(al.begin(), al.end(), al.begin(), [](unsigned char c ) { return std::tolower(c); } );
         std::string bl = b; std::transform(bl.begin(), bl.end(), bl.begin(), [](unsigned char c ) { return std::tolower(c); } );
         return al < bl;
      }
   };
   std::map<std::string, CScalableBitmap*, cicomp> bitmap_stringid_registry;
   VSTGUI::CFrame *frame;
};
