// -*-c++-*-

#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <atomic>
#include <iostream>
#include <sstream>
#include "DebugHelpers.h"
#include "globals.h"

#include "efvg/escape_from_vstgui.h"

#include "SkinModel.h"
#include "SkinColors.h"
#include "filesystem/import.h"

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
class TiXmlElement;

#if !ESCAPE_FROM_VSTGUI
namespace VSTGUI
{
class CFrame;
}
#endif

#define FIXMEERROR SkinDB::get().errorStream

namespace Surge
{

template <typename T> class Maybe
{
  public:
    Maybe() : _empty(true){};
    explicit Maybe(const T &value) : _empty(false), _value(value){};

    T fromJust() const
    {
        if (isJust())
        {
            return _value;
        }
        else
        {
            throw "Cannot get value from Nothing";
        }
    }

    bool isJust() const { return !_empty; }
    bool isNothing() const { return _empty; }

    static bool isJust(const Maybe &m) { return m.isJust(); }
    static bool isNothing(const Maybe &m) { return m.isNothing(); }

  private:
    bool _empty;
    T _value;
};

namespace GUI
{

void loadTypefacesFromPath(const fs::path &p,
                           std::unordered_map<std::string, juce::Typeface::Ptr> &result);

extern const std::string NoneClassName;
class SkinDB;

class Skin
{
  public:
    /*
     * Skin Versions
     * 1. The 1.7 and 1.8 skin engine
     * 2. The 1.9 skin engine (adds notch, image labels, more)
     */
    static constexpr int current_format_version = 2;
    int version;
    inline int getVersion() const { return version; }

    typedef std::shared_ptr<Skin> ptr_t;
    typedef std::unordered_map<std::string, std::string> props_t;

    friend class SkinDB;

    Skin(const std::string &root, const std::string &name);
    ~Skin();

    bool reloadSkin(std::shared_ptr<SurgeBitmaps> bitmapStore);

    std::string resourceName(const std::string &relativeName)
    {
#if WINDOWS
        return root + name + "\\" + relativeName;
#else
        return root + name + "/" + relativeName;
#endif
    }

    static bool setAllCapsProperty(std::string propertyValue);
    static juce::Font::FontStyleFlags setFontStyleProperty(std::string propertyValue);
    static VSTGUI::CHoriTxtAlign setTextAlignProperty(std::string propertyValue);
    static juce::Justification setJuceTextAlignProperty(std::string propertyValue);

    std::string root;
    std::string name;
    std::string category;

    std::string displayName;
    std::string author;
    std::string authorURL;

    struct ComponentClass
    {
        typedef std::shared_ptr<ComponentClass> ptr_t;
        std::string name;
        props_t allprops;
    };

    struct Control
    {
        typedef uint64_t sessionid_t;
        typedef std::shared_ptr<Control> ptr_t;
        int x, y, w, h;

        Control();

        sessionid_t sessionid;

        Surge::Skin::Component defaultComponent;

        std::string ui_id;
        typedef enum
        {
            UIID,
            LABEL,
        } Type;
        Type type;
        std::string classname;
        std::string ultimateparentclassname;
        props_t allprops;

        bool parentResolved = false;

        std::string toString() const
        {
            std::ostringstream oss;

            switch (type)
            {
            case UIID:
                oss << "UIID:" << ui_id;
                break;
            case LABEL:
                oss << "LABEL";
                break;
            }
            oss << " (x=" << x << " y=" << y << " w=" << w << " h=" << h << ")";
            return oss.str();
        }

        VSTGUI::CRect getRect() const
        {
            return VSTGUI::CRect(VSTGUI::CPoint(x, y), VSTGUI::CPoint(w, h));
        }
        void copyFromConnector(const Surge::Skin::Connector &c, int skinVersion);
    };

    struct ControlGroup
    {
        typedef std::shared_ptr<ControlGroup> ptr_t;
        std::vector<ControlGroup::ptr_t> childGroups;
        std::vector<Control::ptr_t> childControls;

        int x = 0, y = 0, w = -1, h = -1;

        bool userGroup = false;

        props_t allprops;
    };

    bool hasColor(const std::string &id) const;
    juce::Colour
    getColor(const std::string &id, const juce::Colour &def,
             std::unordered_set<std::string> noLoops = std::unordered_set<std::string>()) const;
    juce::Colour
    getColor(const std::string &id, const Surge::Skin::Color &def,
             std::unordered_set<std::string> noLoops = std::unordered_set<std::string>()) const
    {
        return getColor(id, juce::Colour(def.r, def.g, def.b, def.a), noLoops);
    }

    juce::Colour getColor(const std::string &id)
    {
        return getColor(id, Surge::Skin::Color::colorByName(id));
    }

    juce::Colour colorFromHexString(const std::string &hex) const;

  private:
    juce::Colour
    getColor(const Surge::Skin::Color &id, const juce::Colour &def,
             std::unordered_set<std::string> noLoops = std::unordered_set<std::string>()) const
    {
        return getColor(id.name, def, noLoops);
    }

  public:
    juce::Colour
    getColor(const Surge::Skin::Color &id,
             std::unordered_set<std::string> noLoops = std::unordered_set<std::string>()) const
    {
        return getColor(id, juce::Colour(id.r, id.g, id.b, id.a), noLoops);
    }

    bool hasColor(const Surge::Skin::Color &col) const { return hasColor(col.name); }

    Skin::Control::ptr_t controlForUIID(const std::string &ui_id) const
    {
        // FIXME don't be stupid like this of course
        for (auto ic : controls)
        {
            if (ic->type == Control::Type::UIID && ic->ui_id == ui_id)
            {
                return ic;
            }
        }

        return nullptr;
    }

    Skin::Control::ptr_t getOrCreateControlForConnector(const std::string &s)
    {
        return getOrCreateControlForConnector(Surge::Skin::Connector::connectorByID(s));
    }
    Skin::Control::ptr_t getOrCreateControlForConnector(const Surge::Skin::Connector &c)
    {
        auto res = controlForUIID(c.payload->id);
        if (!res)
        {
            res = std::make_shared<Surge::GUI::Skin::Control>();
            res->copyFromConnector(c, getVersion());
            // resolveBaseParentOffsets( res );
            controls.push_back(res);
        }
        return res;
    }

    void resolveBaseParentOffsets(Skin::Control::ptr_t);

    void addControl(Skin::Control::ptr_t c) { controls.push_back(c); }

    Maybe<std::string> propertyValue(Skin::Control::ptr_t c,
                                     Surge::Skin::Component::Properties pkey)
    {
        if (!c->defaultComponent.hasProperty(pkey))
            return Maybe<std::string>();

        auto stringNames = c->defaultComponent.payload->propertyNamesMap[pkey];

        /*
        ** Traverse class heirarchy looking for value
        */

        for (auto const &key : stringNames)
            if (c->allprops.find(key) != c->allprops.end())
                return Maybe<std::string>(c->allprops[key]);

        auto cl = componentClasses[c->classname];
        if (!cl)
            return Maybe<std::string>();

        do
        {
            for (auto const &key : stringNames)
                if (cl->allprops.find(key) != cl->allprops.end())
                    return Maybe<std::string>(cl->allprops[key]);

            if (cl->allprops.find("parent") != cl->allprops.end() &&
                componentClasses.find(cl->allprops["parent"]) != componentClasses.end())
            {
                cl = componentClasses[cl->allprops["parent"]];
            }
            else
                return Maybe<std::string>();
        } while (cl);

        return Maybe<std::string>();
    }

    std::string propertyValue(Skin::Control::ptr_t c, Surge::Skin::Component::Properties key,
                              const std::string &defaultValue)
    {
        auto pv = propertyValue(c, key);
        if (pv.isJust())
            return pv.fromJust();
        else
            return defaultValue;
    }

    std::string customBackgroundImage() const { return bgimg; }

    int getWindowSizeX() const { return szx; }
    int getWindowSizeY() const { return szy; }

    bool hasFixedZooms() const { return zooms.size() != 0; }
    std::vector<int> getFixedZooms() const { return zooms; }
    CScalableBitmap *backgroundBitmapForControl(Skin::Control::ptr_t c,
                                                std::shared_ptr<SurgeBitmaps> bitmapStore);

    typedef enum
    {
        HOVER,
        HOVER_OVER_ON,
    } HoverType;

    std::string hoverImageIdForResource(const int resource, HoverType t);
    CScalableBitmap *
    hoverBitmapOverlayForBackgroundBitmap(Skin::Control::ptr_t c, CScalableBitmap *b,
                                          std::shared_ptr<SurgeBitmaps> bitmapStore, HoverType t);

    std::array<juce::Drawable *, 3>
    standardHoverAndHoverOnForControl(Skin::Control::ptr_t c, std::shared_ptr<SurgeBitmaps> b);
    std::array<juce::Drawable *, 3> standardHoverAndHoverOnForIDB(int id,
                                                                  std::shared_ptr<SurgeBitmaps> b);
    std::array<juce::Drawable *, 3> standardHoverAndHoverOnForCSB(CScalableBitmap *csb,
                                                                  Skin::Control::ptr_t c,
                                                                  std::shared_ptr<SurgeBitmaps> b);

    std::vector<Skin::Control::ptr_t> getLabels() const
    {
        std::vector<Skin::Control::ptr_t> labels;
        for (auto ic : controls)
        {
            if (ic->type == Control::LABEL)
            {
                labels.push_back(ic);
            }
        }
        return labels;
    }

    static const std::string defaultImageIDPrefix;

    std::unordered_map<std::string, juce::Typeface::Ptr> typeFaces;

  private:
    static std::atomic<int> instances;
    struct GlobalPayload
    {
        GlobalPayload(const props_t &p) : props(p) {}
        props_t props;
        std::vector<std::pair<std::string, props_t>> children;
    };
    std::vector<std::pair<std::string, GlobalPayload>> globals;
    std::string bgimg = "";
    int szx = BASE_WINDOW_SIZE_X, szy = BASE_WINDOW_SIZE_Y;

    struct ColorStore
    {
        juce::Colour color;
        std::string alias;

        typedef enum
        {
            COLOR,
            ALIAS,
            UNRESOLVED_ALIAS
        } Type;

        Type type;
        ColorStore() : type(COLOR), color(juce::Colours::black) {}
        ColorStore(juce::Colour c) : type(COLOR), color(c) {}
        ColorStore(std::string a) : type(ALIAS), alias(a) {}
        ColorStore(std::string a, Type t) : type(t), alias(a) {}
    };
    std::unordered_map<std::string, ColorStore> colors;
    std::unordered_map<std::string, int> imageStringToId;
    std::unordered_set<int> imageAllowedIds;
    ControlGroup::ptr_t rootControl;
    std::vector<Control::ptr_t> controls;
    std::unordered_map<std::string, ComponentClass::ptr_t> componentClasses;
    std::vector<int> zooms;
    bool recursiveGroupParse(ControlGroup::ptr_t parent, TiXmlElement *groupList,
                             bool topLevel = true);
};

class SkinDB
{
  public:
    static SkinDB &get();

    struct Entry
    {

        enum RootType
        {
            UNKNOWN,
            FACTORY,
            USER
        } rootType = UNKNOWN;

        std::string root;
        std::string name;
        std::string displayName;
        std::string category;
        bool parseable;

        // Since we want to use this as a map
        bool operator==(const Entry &o) const { return root == o.root && name == o.name; }

        struct hash
        {
            size_t operator()(const Entry &e) const
            {
                std::hash<std::string> h;
                return h(e.root) ^ h(e.name);
            }
        };

        bool matchesSkin(const Skin::ptr_t s) const
        {
            return s.get() && s->root == root && s->name == name;
        }
    };

    void rescanForSkins(SurgeStorage *);
    const std::vector<Entry> &getAvailableSkins() const { return availableSkins; }
    Maybe<Entry> getEntryByRootAndName(const std::string &r, const std::string &n)
    {
        for (auto &a : availableSkins)
            if (a.root == r && a.name == n)
                return Maybe<Entry>(a);
        return Maybe<Entry>();
    }
    const Entry &getDefaultSkinEntry() const { return defaultSkinEntry; }

    Skin::ptr_t getSkin(const Entry &skinEntry);
    Skin::ptr_t defaultSkin(SurgeStorage *);

    std::string getErrorString() { return errorStream.str(); };
    std::string getAndResetErrorString()
    {
        auto s = errorStream.str();
        errorStream = std::ostringstream();
        return s;
    }

  private:
    SkinDB();
    ~SkinDB();
    SkinDB(const SkinDB &) = delete;
    SkinDB &operator=(const SkinDB &) = delete;

    std::vector<Entry> availableSkins;
    std::unordered_map<Entry, std::shared_ptr<Skin>, Entry::hash> skins;
    Entry defaultSkinEntry;
    bool foundDefaultSkinEntry = false;

    static std::shared_ptr<SkinDB> instance;
    static std::ostringstream errorStream;

    friend class Skin;
};

class SkinConsumingComponent
{
  public:
    virtual ~SkinConsumingComponent() {}
    virtual void setSkin(Skin::ptr_t s) { setSkin(s, nullptr, nullptr); }
    virtual void setSkin(Skin::ptr_t s, std::shared_ptr<SurgeBitmaps> b) { setSkin(s, b, nullptr); }
    virtual void setSkin(Skin::ptr_t s, std::shared_ptr<SurgeBitmaps> b, Skin::Control::ptr_t c)
    {
        bool changed = (skin != s) || (associatedBitmapStore != b) || (skinControl != c);
        skin = s;
        associatedBitmapStore = b;
        skinControl = c;
        if (changed)
        {
            onSkinChanged();
        }
    }

    virtual void onSkinChanged() {}

  protected:
    Skin::ptr_t skin = nullptr;
    Skin::Control::ptr_t skinControl = nullptr;
    std::shared_ptr<SurgeBitmaps> associatedBitmapStore = nullptr;
};
} // namespace GUI
} // namespace Surge
