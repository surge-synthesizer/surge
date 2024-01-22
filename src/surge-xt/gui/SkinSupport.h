/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#ifndef SURGE_SRC_SURGE_XT_GUI_SKINSUPPORT_H
#define SURGE_SRC_SURGE_XT_GUI_SKINSUPPORT_H

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

#include "juce_graphics/juce_graphics.h"

#include "SkinModel.h"
#include "SkinColors.h"
#include "filesystem/import.h"
#include "SkinFonts.h"

#include <optional>

/*
** Support for rudimentary skinning in Surge
**
** SkinSupport provides a pair of classes, a SkinManager and a SkinDB
** The SkinManager singleton loads and applies the SkinDB to various places as
** appropriate. The SkinDB has all the information you would need
** to skin yourself, and answers various queries.
*/

class SurgeStorage;
class SurgeImageStore;
class SurgeImage;
class TiXmlElement;

#define FIXMEERROR SkinDB::get()->errorStream

namespace Surge
{

namespace GUI
{

struct FontManager;

void loadTypefacesFromPath(const fs::path &p,
                           std::unordered_map<std::string, juce::Typeface::Ptr> &result);

extern const std::string NoneClassName;

static constexpr char MemorySkinName[] = "Surge Classic";

class SkinDB;

enum RootType
{
    UNKNOWN,
    FACTORY,
    USER,
    MEMORY
};

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
    Skin(bool inMemory);
    ~Skin();

    bool useInMemorySkin{false};

    std::unique_ptr<FontManager> fontManager;
    bool reloadSkin(std::shared_ptr<SurgeImageStore> bitmapStore);

    std::string resourceName(const std::string &relativeName)
    {
#if WINDOWS
        return root + name + "\\" + relativeName;
#else
        return root + name + "/" + relativeName;
#endif
    }

    static bool setAllCapsProperty(const std::string &propertyValue);
    static juce::Font::FontStyleFlags setFontStyleProperty(const std::string &propertyValue);
    static juce::Justification setJuceTextAlignProperty(const std::string &propertyValue);

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

        juce::Rectangle<int> getRect() const { return juce::Rectangle<int>(x, y, w, h); }
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

    juce::Font getFont(const Surge::Skin::FontDesc &d);

  private:
    struct FontOverride
    {
        std::string family{"Comic Sans"};
        int size{9};
    };
    std::unordered_map<std::string, FontOverride> fontOverrides;

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

    std::unordered_set<Control::sessionid_t> sessionIds;
    bool containsControlWithSessionId(const Control::sessionid_t &sid)
    {
        return (sessionIds.find(sid) != sessionIds.end());
    }

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
            sessionIds.insert(res->sessionid);
        }
        return res;
    }

    void resolveBaseParentOffsets(Skin::Control::ptr_t);

    void addControl(Skin::Control::ptr_t c) { controls.push_back(c); }

    std::optional<std::string> propertyValue(Skin::Control::ptr_t c,
                                             Surge::Skin::Component::Properties pkey)
    {
        if (!c->defaultComponent.hasProperty(pkey))
            return {};

        auto stringNames = c->defaultComponent.payload->propertyNamesMap[pkey];

        /*
        ** Traverse class hierarchy looking for value
        */

        for (auto const &key : stringNames)
            if (c->allprops.find(key) != c->allprops.end())
                return c->allprops[key];

        auto cl = componentClasses[c->classname];
        if (!cl)
            return {};

        do
        {
            for (auto const &key : stringNames)
                if (cl->allprops.find(key) != cl->allprops.end())
                    return cl->allprops[key];

            if (cl->allprops.find("parent") != cl->allprops.end() &&
                componentClasses.find(cl->allprops["parent"]) != componentClasses.end())
            {
                cl = componentClasses[cl->allprops["parent"]];
            }
            else
                return {};
        } while (cl);

        return {};
    }

    std::string propertyValue(Skin::Control::ptr_t c, Surge::Skin::Component::Properties key,
                              const std::string &defaultValue)
    {
        return propertyValue(c, key).value_or(defaultValue);
    }

    std::string customBackgroundImage() const { return bgimg; }

    int getWindowSizeX() const { return szx; }
    int getWindowSizeY() const { return szy; }

    bool hasFixedZooms() const { return zooms.size() != 0; }
    std::vector<int> getFixedZooms() const { return zooms; }
    SurgeImage *backgroundBitmapForControl(Skin::Control::ptr_t c,
                                           std::shared_ptr<SurgeImageStore> bitmapStore);

    typedef enum
    {
        HOVER,
        HOVER_OVER_ON,
        TEMPOSYNC,
        HOVER_TEMPOSYNC
    } HoverType;

    std::string hoverImageIdForResource(const int resource, HoverType t);
    SurgeImage *hoverBitmapOverlayForBackgroundBitmap(Skin::Control::ptr_t c, SurgeImage *b,
                                                      std::shared_ptr<SurgeImageStore> bitmapStore,
                                                      HoverType t);

    std::array<SurgeImage *, 3>
    standardHoverAndHoverOnForControl(Skin::Control::ptr_t c, std::shared_ptr<SurgeImageStore> b);
    std::array<SurgeImage *, 3> standardHoverAndHoverOnForIDB(int id,
                                                              std::shared_ptr<SurgeImageStore> b);
    std::array<SurgeImage *, 3> standardHoverAndHoverOnForCSB(SurgeImage *csb,
                                                              Skin::Control::ptr_t c,
                                                              std::shared_ptr<SurgeImageStore> b);

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
        ColorStore(const juce::Colour &c) : type(COLOR), color(c) {}
        ColorStore(const std::string &a) : type(ALIAS), alias(a) {}
        ColorStore(const std::string &a, Type t) : type(t), alias(a) {}
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

class SkinDB : public juce::DeletedAtShutdown
{
  public:
    static SkinDB *instance;
    static SkinDB *get();

    struct Entry
    {
        using RootType = Surge::GUI::RootType;

        RootType rootType = UNKNOWN;

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
            return s.get() && ((s->root == root && s->name == name) ||
                               (rootType == MEMORY && s->displayName == MemorySkinName));
        }
    };

    void rescanForSkins(SurgeStorage *);
    const std::vector<Entry> &getAvailableSkins() const { return availableSkins; }
    std::optional<Entry> getEntryByRootAndName(const std::string &r, const std::string &n)
    {
        for (auto &a : availableSkins)
            if (a.root == r && a.name == n)
                return a;
        return {};
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

    std::optional<Entry> installSkinFromPathToUserDirectory(SurgeStorage *, const fs::path &from);

  private:
    SkinDB();
    ~SkinDB();
    SkinDB(const SkinDB &) = delete;
    SkinDB &operator=(const SkinDB &) = delete;

    std::vector<Entry> availableSkins;
    std::unordered_map<Entry, std::shared_ptr<Skin>, Entry::hash> skins;
    Entry defaultSkinEntry;
    bool foundDefaultSkinEntry = false;

    static std::ostringstream errorStream;

    friend class Skin;
};

class SkinConsumingComponent
{
  public:
    virtual ~SkinConsumingComponent() {}
    virtual void setSkin(Skin::ptr_t s) { setSkin(s, nullptr, nullptr); }
    virtual void setSkin(Skin::ptr_t s, std::shared_ptr<SurgeImageStore> b)
    {
        setSkin(s, b, nullptr);
    }
    virtual void setSkin(Skin::ptr_t s, std::shared_ptr<SurgeImageStore> b, Skin::Control::ptr_t c)
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
    std::shared_ptr<SurgeImageStore> associatedBitmapStore = nullptr;
};

} // namespace GUI
} // namespace Surge

#endif // SURGE_SRC_SURGE_XT_GUI_SKINSUPPORT_H
