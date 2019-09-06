/*
** This is the surge UI Layout Engine.
*/

#pragma once

#include "SurgeStorage.h"
#include "vstgui/vstgui.h"
#include "SurgeBitmaps.h"
#include "tinyxml.h"
#include <unordered_map>
#include <vector>
#include "LayoutLog.h"

class SurgeGUIEditor;

namespace Surge
{

class LayoutElement;
class LayoutComponent;

class LayoutEngine
{
public:
   LayoutEngine();
   virtual ~LayoutEngine();

   // In the future we may want to change some of these so use a typedef
   typedef VSTGUI::CRect rect_t;
   typedef VSTGUI::CPoint point_t;
   typedef VSTGUI::CCoord coord_t;
   typedef VSTGUI::CControl control_t;
   typedef VSTGUI::CViewContainer container_t;
   typedef VSTGUI::CFrame frame_t;
   typedef VSTGUI::CColor color_t;

   typedef std::string guiid_t;
   typedef std::function<control_t*(
       const guiid_t&, VSTGUI::IControlListener*, long, SurgeGUIEditor*, LayoutElement*)>
       controlGenerator_t;

   static const VSTGUI::CViewAttributeID kSurgeShowPopup = 'snpo';
   
   /*
   ** We have a collection of containers where we can generate a set whenever we want
   */
   container_t* generateLayoutRootControl();
   container_t* getSubContainerByLabel(std::string label);

   /*
   ** One thing we can do is layout the CHSwitch2 types given a name
   */
   virtual control_t* addLayoutControl(const guiid_t& guiid,
                                       VSTGUI::IControlListener* listener,
                                       long tag,
                                       SurgeGUIEditor* editor);

   std::shared_ptr<SurgeBitmaps> bitmapStore = nullptr;
   std::unique_ptr<LayoutElement> rootLayoutElement = nullptr;
   std::unordered_map<std::string, std::shared_ptr<LayoutComponent>> components;
   std::unordered_map<std::string, LayoutElement*> nodeMap;
   std::unordered_map<std::string, controlGenerator_t> controlFactory;
   frame_t* frame = nullptr;

   // Obviously: Fix this
   std::string layoutRoot;
   std::string particularLayout;
   std::string layoutResource(std::string suffix)
   {
      auto res = layoutRoot + particularLayout + suffix;
      return res;
   }

   float getWidth();
   float getHeight();

   bool parseLayout();
   void buildNodeMapFrom(LayoutElement*);
   void setupControlFactory();
   void commonFactoryMethods(control_t *control, std::unordered_map<std::string, std::string> &props);

   int layoutId = 12374; // FIXME - have a unique ID

   color_t colorFromColorMap(std::string name, std::string hexDefault);
   typedef std::unordered_map<std::string, std::string> colormap_t;
   std::unordered_map<std::string, colormap_t> colormaps;
   colormap_t active_colormap;
   void parseColormaps(TiXmlElement* cm);

   std::string stringFromStringMap(std::string name);
   typedef std::unordered_map<std::string, std::string> stringmap_t;
   std::unordered_map<std::string, stringmap_t> strings;
   stringmap_t active_stringmap;
   void parseStringmaps(TiXmlElement* s);

   bool loadSVGToBitmapStore(std::string svg, float w=0, float h=0) {
      return loadSVGToBitmapStore(svg,svg,w,h);
   }
   bool loadSVGToBitmapStore(std::string svg, std::string svgtag, float w=0, float h=0);
};

class LayoutElement
{
public:
   typedef enum ElementType
   {
      Layout,
      Node,
      Label
   } ElementType;
   ElementType type;

   typedef enum LayoutMode
   {
      Free,
      Hlist,
      Vlist,
      Grid
   } LayoutMode;
   LayoutMode mode = Free;

   static const int kAutomaticSize = -100000,
      kOffCenter = kAutomaticSize - 1,
      kOffLeft = kAutomaticSize - 2,
      kOffRight = kAutomaticSize - 3;
   
   LayoutElement()
   {}
   ~LayoutElement()
   {
      for (auto k : children)
      {
         k->parent = nullptr;
         delete k;
      }
   }

   void addChild(LayoutElement* child) // I take ownership
   {
      children.push_back(child);
   }

   std::vector<LayoutElement*> children;

   static LayoutElement* fromXML(TiXmlElement* el, LayoutElement* parent, LayoutEngine* eng);
   std::string toString(bool recurse = true);

   void setupChildSizes();
   void generateLayoutControl(LayoutEngine*, bool recurse = true);

   std::string component = "";
   std::string guiid = "";
   std::string style = "";
   std::string bgcolor = "";
   std::string fgcolor = "";
   std::string label = "";
   float width = 0;
   float height = 0;
   float xoff = 0;
   float yoff = 0;

   float marginx = 0, marginy = 0;
   
   int depth = -1;

   LayoutElement* parent = nullptr;
   LayoutEngine::control_t* associatedControl = nullptr;
   LayoutEngine::container_t* associatedContainer = nullptr;

   std::unordered_map<std::string, std::string> properties;
};

class LayoutComponent
{
public:
   LayoutComponent(std::string _name, std::string _classN) : name(_name), classN(_classN)
   {}
   virtual ~LayoutComponent()
   {}

   std::string name;
   std::string classN;

   int width = 0;
   int height = 0;
   std::unordered_map<std::string, std::string> properties;
   std::unordered_map<std::string, std::vector<std::string>> array_properties;
   
   bool hasResolvedParent = false;
   void resolveParent(LayoutEngine *eng);
   
   static LayoutComponent* fromXML(TiXmlElement* e);
};

class LayoutLibrary
{
public:
   static void initialize(SurgeStorage *);
   static std::vector<std::string> availbleLayouts;
};
   
} // namespace Surge
