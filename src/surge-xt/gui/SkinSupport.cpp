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

#include "SkinSupport.h"
#include "SurgeStorage.h"
#include "SurgeImageStore.h"
#include "UserDefaults.h"
#include "SurgeImage.h"

#include "filesystem/import.h"

#include <array>
#include <set>
#include <iostream>
#include <iomanip>
#include "SurgeXTBinary.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace GUI
{

const std::string NoneClassName = "none";
const std::string Skin::defaultImageIDPrefix = "DEFAULT/";
std::ostringstream SkinDB::errorStream;

SkinDB *SkinDB::instance{nullptr};

SkinDB *Surge::GUI::SkinDB::get()
{
    if (!instance)
        instance = new SkinDB();

    return instance;
}

SkinDB::SkinDB()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SkinDB::SkinDB");
#endif
    // std::cout << "Constructing SkinDB" << std::endl;
}

SkinDB::~SkinDB()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SkinDB::~SkinDB");
#endif
    skins.clear(); // Not really necessary but means the skins are destroyed before the rest of the
                   // dtor runs
                   // std::cout << "Destroying SkinDB" << std::endl;
    instance = nullptr;
}

std::shared_ptr<Skin> SkinDB::defaultSkin(SurgeStorage *storage)
{
    rescanForSkins(storage);
    auto uds = Surge::Storage::getUserDefaultValue(storage, Surge::Storage::DefaultSkin, "");
    if (uds == "")
        return getSkin(defaultSkinEntry);
    else
    {
        auto st = (Entry::RootType)(Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::DefaultSkinRootType, UNKNOWN));

        for (auto e : availableSkins)
        {
            if (e.name == uds && (e.rootType == st || st == UNKNOWN) && e.parseable)
                return getSkin(e);
        }
        return getSkin(defaultSkinEntry);
    }
}

std::shared_ptr<Skin> SkinDB::getSkin(const Entry &skinEntry)
{
    if (skins.find(skinEntry) == skins.end())
    {
        if (skinEntry.rootType == MEMORY)
        {
            skins[skinEntry] = std::make_shared<Skin>(true);
        }
        else
        {
            skins[skinEntry] = std::make_shared<Skin>(skinEntry.root, skinEntry.name);
        }
    }
    return skins[skinEntry];
}

void SkinDB::rescanForSkins(SurgeStorage *storage)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("SkinDB::rescanForSkins");
#endif
    availableSkins.clear();

    std::array<fs::path, 2> paths = {storage->datapath, storage->userSkinsPath};

    for (auto &source : paths)
    {
        Entry::RootType rt = UNKNOWN;
        if (source == paths[0])
            rt = FACTORY;
        if (source == paths[1])
            rt = USER;

        std::vector<fs::path> alldirs;
        std::deque<fs::path> workStack;
        workStack.push_back(source);
        try
        {
            while (!workStack.empty())
            {
                auto top = workStack.front();
                workStack.pop_front();
                if (fs::is_directory(top))
                {
                    for (auto &d : fs::directory_iterator(top))
                    {
                        if (fs::is_directory(d))
                        {
                            if (d.path().filename().u8string() == "__MACOSX")
                            {
                                // skip this mis-zipped dir. See #7249
                            }
                            else
                            {
                                alldirs.push_back(d);
                                workStack.push_back(d);
                            }
                        }
                    }
                }
            }
        }
        catch (const fs::filesystem_error &e)
        {
            // This will give us a broken skin but no need to tell the users
            FIXMEERROR << "Unable to traverse for skins in user directory: " << e.what();
        }

        for (auto &p : alldirs)
        {
            std::string name;
            name = path_to_string(p);

            std::string ending = ".surge-skin";
            if (name.length() >= ending.length() &&
                0 == name.compare(name.length() - ending.length(), ending.length(), ending))
            {
                auto sp = name.rfind(PATH_SEPARATOR);

                if (sp == std::string::npos)
                {
                }
                else
                {
                    auto path = name.substr(0, sp + 1);
                    auto lo = name.substr(sp + 1);
                    Entry e;
                    e.rootType = rt;
                    e.root = path;
                    e.name = lo + PATH_SEPARATOR;
                    if (e.name.find("default.surge-skin") != std::string::npos && rt == FACTORY &&
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
    if (!foundDefaultSkinEntry)
    {
        auto memSkin = Entry();
        memSkin.rootType = MEMORY;
        memSkin.name = MemorySkinName;
        memSkin.displayName = MemorySkinName;
        memSkin.category = "";
        memSkin.root = "";
        availableSkins.push_back(memSkin);
        defaultSkinEntry = memSkin;
    }

    // Run over the skins parsing the name
    for (auto &e : availableSkins)
    {
        auto x = e.root + e.name + PATH_SEPARATOR + "skin.xml";

        TiXmlDocument doc;
        // Obviously fix this
        doc.SetTabSize(4);

        if (!doc.LoadFile(string_to_path(x)))
        {
            if (e.rootType == MEMORY)
            {
                // e.displayName += " (in memory)";
            }
            else
            {
                e.displayName = e.name + " (parse error)";
                e.parseable = false;
            }
            continue;
        }
        e.parseable = true;
        TiXmlElement *surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
        if (!surgeskin)
        {
            e.displayName = e.name + " (no skin element)";
            continue;
        }

        const char *a;
        if ((a = surgeskin->Attribute("name")))
        {
            e.displayName = a;
        }
        else
        {
            e.displayName = e.name + " (no name att)";
        }

        e.category = "";
        if ((a = surgeskin->Attribute("category")))
        {
            e.category = a;
        }
    }

    std::sort(availableSkins.begin(), availableSkins.end(),
              [](const SkinDB::Entry &a, const SkinDB::Entry &b) {
                  return _stricmp(a.displayName.c_str(), b.displayName.c_str()) < 0;
              });
}

// Define the inverse maps
#include "SkinImageMaps.h"

std::atomic<int> Skin::instances(0);

Skin::Skin(const std::string &root, const std::string &name) : root(root), name(name)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("Skin::Skin");
#endif
    instances++;
    // std::cout << "Constructing a skin " << _D(root) << _D(name) << _D(instances) << std::endl;
    imageStringToId = createIdNameMap();
    imageAllowedIds = allowedImageIds();
    fontManager = std::make_unique<FontManager>();
}

Skin::Skin(bool inMemory)
{
    if (!inMemory)
        std::cout << "SOFTWARE ERROR" << std::endl;
    instances++;
    // std::cout << "Constructing a skin " << _D(root) << _D(name) << _D(instances) << std::endl;
    imageStringToId = createIdNameMap();
    imageAllowedIds = allowedImageIds();
    useInMemorySkin = true;
    fontManager = std::make_unique<FontManager>();
}

Skin::~Skin()
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("Skin::~Skin");
#endif
    instances--;
}

#if !defined(TINYXML_SAFE_TO_ELEMENT)
#define TINYXML_SAFE_TO_ELEMENT(expr) ((expr) ? (expr)->ToElement() : NULL)
#endif

bool Skin::reloadSkin(std::shared_ptr<SurgeImageStore> bitmapStore)
{
#ifdef INSTRUMENT_UI
    Surge::Debug::record("Skin::reloadSkin");
    Surge::Debug::TimeThisBlock _trs_("Skin::reloadSkin");
#endif
    // std::cout << "Reloading skin " << _D(name) << std::endl;
    TiXmlDocument doc;
    if (useInMemorySkin)
    {
        auto memSkin =
            std::string(SurgeXTBinary::memoryskin_xml, SurgeXTBinary::memoryskin_xmlSize);
        doc.Parse(memSkin.c_str());
    }
    else
    {
        // Obviously fix this
        doc.SetTabSize(4);

        if (!doc.LoadFile(string_to_path(resourceName("skin.xml"))))
        {
            FIXMEERROR << "Unable to load skin.xml resource '" << resourceName("skin.xml") << "'"
                       << std::endl;
            FIXMEERROR << "Unable to parse skin.xml\nError is:\n"
                       << doc.ErrorDesc() << " at row " << doc.ErrorRow() << ", column "
                       << doc.ErrorCol() << std::endl;
            return false;
        }
    }

    TiXmlElement *surgeskin = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("surge-skin"));
    if (!surgeskin)
    {
        FIXMEERROR << "There is no top level surge-skin node in skin.xml" << std::endl;
        return true;
    }
    const char *a;
    displayName = name;
    author = "";
    authorURL = "";
    category = "";
    if ((a = surgeskin->Attribute("name")))
        displayName = a;
    if ((a = surgeskin->Attribute("author")))
        author = a;
    if ((a = surgeskin->Attribute("authorURL")))
        authorURL = a;
    if ((a = surgeskin->Attribute("category")))
        category = a;

    version = -1;
    if (!(surgeskin->QueryIntAttribute("version", &version) == TIXML_SUCCESS))
    {
        FIXMEERROR << "Skin file does not contain a 'version' attribute. You must contain a "
                      "version at most "
                   << Skin::current_format_version << std::endl;
        return false;
    }
    if (version > Skin::current_format_version)
    {
        FIXMEERROR << "Skin version '" << version << "' is greater than the max version '"
                   << Skin::current_format_version << "' supported by this binary" << std::endl;
        return false;
    }
    if (version < 1)
    {
        FIXMEERROR << "Skin version '" << version << "' is lower than 1. Why?" << std::endl;
        return false;
    }
    if (version == 1)
    {
        FIXMEERROR << "Surge XT no longer supports Version 1 skins. Please make the "
                   << "(relatively minor) set of changes to bring this skin to version 2"
                   << std::endl;
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
    if (!componentclassesxml)
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
    auto fontPath = string_to_path(resourceName("fonts"));
    if (fs::is_directory(fontPath))
    {
        typeFaces.clear();
        loadTypefacesFromPath(fontPath, typeFaces);
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
            for (auto qchild = lkid->FirstChild(); qchild; qchild = qchild->NextSibling())
            {
                auto qkid = TINYXML_SAFE_TO_ELEMENT(qchild);
                if (qkid)
                {
                    std::string k = qkid->Value();
                    props_t kmap;
                    for (auto a = qkid->FirstAttribute(); a; a = a->Next())
                        kmap[a->Name()] = a->Value();
                    r.children.push_back(std::make_pair(k, kmap));
                }
            }

            globals.push_back(std::make_pair(key, r));
        }
    }

    /*
    ** Parse the controls section
    */
    auto attrint = [](TiXmlElement *e, const char *a) {
        const char *av = e->Attribute(a);
        if (!av)
            return -1;
        return std::atoi(av);
    };

    auto attrstr = [](TiXmlElement *e, const char *a) {
        const char *av = e->Attribute(a);
        if (!av)
            return std::string();
        return std::string(av);
    };

    controls.clear();
    rootControl = std::make_shared<ControlGroup>();
    bool rgp = recursiveGroupParse(rootControl, controlsxml);
    if (!rgp)
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

        c->name = attrstr(lkid, "name");
        if (c->name == "")
        {
            FIXMEERROR << "INVALID NAME" << std::endl;
            return false;
        }

        if (componentClasses.find(c->name) != componentClasses.end())
        {
            FIXMEERROR << "Double Definition" << std::endl;
            return false;
        }

        for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            c->allprops[a->Name()] = a->Value();

        componentClasses[c->name] = c;
    };

    // process the images and colors
    fontManager->restoreLatoAsDefault();
    for (auto g : globals)
    {
        if (g.first == "defaultimage")
        {
            const auto path = resourceName(g.second.props["directory"]);
            try
            {
                fs::path source(string_to_path(path));
                for (const fs::path &d : fs::directory_iterator(source))
                {
                    const auto pathStr = path_to_string(d);
                    const auto pos = pathStr.find("bmp");
                    if (pos != std::string::npos)
                    {
                        auto postbmp = pathStr.substr(pos + 3);
                        int idx = std::atoi(pathStr.c_str() + pos + 3);
                        // We epxpect 5 digits and a .svg or .png
                        auto xtn = pathStr.substr(pos + 8);

                        if ((xtn == ".svg" || xtn == ".png") &&
                            (imageAllowedIds.find(idx) != imageAllowedIds.end()))
                        {
                            bitmapStore->loadImageByPathForID(pathStr, idx);
                        }
                    }
                    else
                    {
                        std::string id = defaultImageIDPrefix + path_to_string(d.filename());
                        bitmapStore->loadImageByPathForStringID(pathStr, id);
                    }
                }
            }
            catch (const fs::filesystem_error &e)
            {
                // This will give us a broken skin but no need to tell the users
                FIXMEERROR << "Unable to load image directory: " << e.what();
            }
        }

        if (g.first == "default-font")
        {
            auto family = g.second.props["family"];
            auto f = typeFaces[family];
            if (f)
                fontManager->overrideLatoWith(f);
            else
                FIXMEERROR << "COULD NOT LOAD FONT " << family << std::endl;
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
                    if (imageStringToId.find(id) != imageStringToId.end())
                        bitmapStore->loadImageByPathForID(resourceName(res), imageStringToId[id]);
                    else
                    {
                        bitmapStore->loadImageByPathForStringID(resourceName(res), id);
                    }
                }
                else
                {
                    bitmapStore->loadImageByPath(resourceName(res));
                }
            }
        }
        if (g.first == "multi-image")
        {
            // FIXME error checking
            auto props = g.second.props;
            if (props.find("id") == props.end())
            {
                FIXMEERROR << "multi-image must contain an id";
            }
            auto id = props["id"];

            if (id.size() > 0)
            {
                bool validKids = true;
                auto kids = g.second.children;
                // Go find the 100 one first
                SurgeImage *bm = nullptr;
                for (auto k : kids)
                {
                    if (k.second.find("zoom-level") == k.second.end() ||
                        k.second.find("resource") == k.second.end())
                    {
                        validKids = false;
                        // for( auto kk : k.second )
                        // std::cout << _D(kk.first) << _D(kk.second) << std::endl;
                        FIXMEERROR
                            << "Each subchild of a multi-image must contain a zoom-level and "
                               "resource";
                        break;
                    }
                    else if (k.second["zoom-level"] == "100")
                    {
                        auto res = k.second["resource"];
                        if (imageStringToId.find(id) != imageStringToId.end())
                            bm = bitmapStore->loadImageByPathForID(resourceName(res),
                                                                   imageStringToId[id]);
                        else
                        {
                            bm = bitmapStore->loadImageByPathForStringID(resourceName(res), id);
                        }
                    }
                }
                if (bm && validKids)
                {
                    for (auto k : kids)
                    {
                        auto zl = k.second["zoom-level"];
                        auto zli = std::atoi(zl.c_str());
                        auto r = k.second["resource"];
                        if (zli != 100)
                        {
                            bm->addPNGForZoomLevel(resourceName(r), zli);
                        }
                    }
                }
                else
                {
                    FIXMEERROR << "invalid multi-image for some reason";
                }
            }
        }
        if (g.first == "zoom-levels")
        {
            for (auto k : g.second.children)
            {
                if (k.second.find("zoom") != k.second.end())
                {
                    zooms.push_back(std::atoi(k.second["zoom"].c_str()));
                }
            }
            // Store the zooms in sorted order
            std::sort(zooms.begin(), zooms.end());
        }
        if (g.first == "color")
        {
            auto p = g.second.props;
            auto id = p["id"];
            auto val = p["value"];
            auto r = juce::Colour();
            if (val[0] == '#')
            {
                colors[id] = ColorStore(colorFromHexString(val));
            }
            else if (val[0] == '$')
            {
                colors[id] = ColorStore(val.c_str() + 1, ColorStore::UNRESOLVED_ALIAS);
            }
            else
            {
                // Look me up later
                colors[id] = ColorStore(val, ColorStore::UNRESOLVED_ALIAS);
            }
        }
        if (g.first == "font")
        {
            auto p = g.second.props;
            auto fo = FontOverride();
            if (p.find("family") != p.end())
                fo.family = p["family"];
            if (p.find("size") != p.end())
                fo.size = std::atoi(p["size"].c_str());
            if (p.find("id") != p.end())
                fontOverrides[p["id"]] = fo;
            else
            {
                FIXMEERROR << "Font without ID ignored ";
            }
        }

        if (g.first == "background" && g.second.props.find("image") != g.second.props.end())
        {
            bgimg = g.second.props["image"];
        }

        if (g.first == "window-size")
        {
            if (g.second.props.find("x") != g.second.props.end())
            {
                szx = std::atoi(g.second.props["x"].c_str());
                if (szx < 1)
                    szx = BASE_WINDOW_SIZE_X;
            }
            if (g.second.props.find("y") != g.second.props.end())
            {
                szy = std::atoi(g.second.props["y"].c_str());
                if (szy < 1)
                    szy = BASE_WINDOW_SIZE_Y;
            }
        }
    }

    // Resolve color aliases.
    for (auto &c : colors)
    {
        if (c.second.type == ColorStore::UNRESOLVED_ALIAS)
        {
            if (colors.find(c.second.alias) != colors.end())
            {
                c.second.type = ColorStore::ALIAS;
            }
            else
            {
                c.second.type = ColorStore::COLOR;
                c.second.color = juce::Colours::red;
            }
        }
    }

    // Resolve the ultimate parent classes
    for (auto &c : controls)
    {
        c->ultimateparentclassname = c->classname;
        if (c->defaultComponent == Surge::Skin::Components::Label)
            continue;

        while (componentClasses.find(c->ultimateparentclassname) != componentClasses.end())
        {
            auto comp = componentClasses[c->ultimateparentclassname];

            for (auto p : comp->allprops)
            {
                if (p.first != "parent")
                {
                    c->allprops[p.first] = p.second;
                }
            }
            if (comp->allprops.find("parent") != comp->allprops.end())
            {
                c->ultimateparentclassname =
                    componentClasses[c->ultimateparentclassname]->allprops["parent"];
            }
            else
            {
                FIXMEERROR << "PARENT CLASS DOESN'T RESOLVE FOR " << c->ultimateparentclassname
                           << std::endl;
                return false;
                break;
            }
        }
        if (c->defaultComponent.payload->internalClassname != c->ultimateparentclassname)
        {
            if (c->defaultComponent.hasAlias(c->ultimateparentclassname))
            {
                c->ultimateparentclassname = c->defaultComponent.payload->internalClassname;
            }
            else
            {
                // I must have changed my class. Can I find the class I became?
                bool found = false;
                for (auto id : Surge::Skin::Component::allComponentIds())
                {
                    auto comp = Surge::Skin::Component::componentById(id);
                    if (comp.payload->internalClassname == c->ultimateparentclassname ||
                        comp.hasAlias(c->ultimateparentclassname))
                    {
                        found = true;
                        c->defaultComponent = comp;
                    }
                }

                if (!found)
                {
                    FIXMEERROR << "Ultimate Parent Class '" << c->ultimateparentclassname
                               << "' does not resolve to an internal class and thinks it is '"
                               << c->defaultComponent.payload->internalClassname << "'"
                               << std::endl;
                    return false;
                }
            }
        }

        if (getVersion() >= 2)
        {
            /*
             * OK so we need to resolve the image names. There's a hierarchy of
             * 'image/resource/bg_id' in that order and we want to choose the 'lowest' constant one.
             * So if image != resource, set resource to image, bit if all three are the same, erase
             * image and resource
             */
            if (c->defaultComponent.hasProperty(Surge::Skin::Component::Properties::BACKGROUND))
            {
                auto hasImage = c->allprops.find("image") != c->allprops.end();
                auto hasResource = c->allprops.find("bg_resource") != c->allprops.end();
                auto hasId = c->allprops.find("bg_id") != c->allprops.end();

                if (hasImage && hasResource)
                    c->allprops["bg_resource"] = c->allprops["image"];
                if (hasResource && hasId && c->allprops["bg_resource"] == c->allprops["bg_id"])
                {
                    c->allprops.erase("bg_resource");
                    c->allprops.erase("image");
                }
            }
        }
        else
        {
            /*
             * This is a super-late-in-1.8 semi-hack I need to unwind in 1.9. It is unwound above
             */
            if (c->ultimateparentclassname == "CHSwitch2" &&
                c->allprops.find("image") != c->allprops.end() &&
                c->allprops.find("bg_resource") == c->allprops.end())
            {
                c->allprops["bg_resource"] = c->allprops["image"];
            }

            /*
             * Also handle the bg_id / bg_resource special case here
             */
            if (c->allprops.find("bg_resource") != c->allprops.end() &&
                c->allprops.find("bg_id") != c->allprops.end())
            {
                c->allprops.erase("bg_id");
            }
        }
    }
    return true;
}

bool Skin::setAllCapsProperty(const std::string &propertyValueA)
{
    // make the property value not case sensitive
    auto propertyValue = propertyValueA;
    std::transform(propertyValue.begin(), propertyValue.end(), propertyValue.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (propertyValue == "true")
    {
        return true;
    }
    else
    {
        return false;
    }
}

juce::Font::FontStyleFlags Skin::setFontStyleProperty(const std::string &propertyValueA)
{
    // make the property value not case sensitive
    auto propertyValue = propertyValueA;
    std::transform(propertyValue.begin(), propertyValue.end(), propertyValue.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (propertyValue == "bold")
    {
        return juce::Font::FontStyleFlags::bold;
    }
    else if (propertyValue == "italic")
    {
        return juce::Font::FontStyleFlags::italic;
    }
    else if (propertyValue == "underline")
    {
        return juce::Font::FontStyleFlags::underlined;
    }
    else
    {
        return juce::Font::FontStyleFlags::plain;
    }
}

juce::Justification Skin::setJuceTextAlignProperty(const std::string &propertyValueA)
{
    // make the property value not case sensitive
    auto propertyValue = propertyValueA;
    std::transform(propertyValue.begin(), propertyValue.end(), propertyValue.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (propertyValue == "center")
    {
        return juce::Justification::centred;
    }
    else if (propertyValue == "right")
    {
        return juce::Justification::centredRight;
    }
    else
    {
        return juce::Justification::centredLeft;
    }
}

bool Skin::recursiveGroupParse(ControlGroup::ptr_t parent, TiXmlElement *controlsxml, bool toplevel)
{
    // I know I am gross for copying these
    auto attrint = [](TiXmlElement *e, const char *a) {
        const char *av = e->Attribute(a);
        if (!av)
            return -1;
        return std::atoi(av);
    };

    // Modifies an int value if found (with offset)
    auto attrintif = [](TiXmlElement *e, const char *a, int &target, const int &offset = 0) {
        const char *av = e->Attribute(a);
        if (av)
        {
            target = std::atoi(av) + offset;
            return true;
        }
        return false;
    };

    auto attrstr = [](TiXmlElement *e, const char *a) {
        const char *av = e->Attribute(a);
        if (!av)
            return std::string();
        return std::string(av);
    };

    for (auto gchild = controlsxml->FirstChild(); gchild; gchild = gchild->NextSibling())
    {
        auto lkid = TINYXML_SAFE_TO_ELEMENT(gchild);
        if (!lkid)
            continue;

        if (std::string(lkid->Value()) == "group")
        {
            auto g = std::make_shared<Skin::ControlGroup>();
            g->userGroup = true;
            g->x = attrint(lkid, "x");
            if (g->x < 0)
                g->x = 0;
            g->x += parent->x;
            g->y = attrint(lkid, "y");
            if (g->y < 0)
                g->y = 0;
            g->y += parent->y;
            g->w = attrint(lkid, "w");
            g->h = attrint(lkid, "h");

            parent->childGroups.push_back(g);
            if (!recursiveGroupParse(g, lkid, false))
            {
                return false;
            }
        }
        else if ((std::string(lkid->Value()) == "control") ||
                 (std::string(lkid->Value()) == "label"))
        {
            auto control = std::make_shared<Skin::Control>();

            /*
             * This is a bit of a mess; v 1.0 had label as special and 2 and higher introduced
             * components so we could have this be cleaner by making uiid and label symmetric. Later
             * later...
             */
            if (std::string(lkid->Value()) == "label")
            {
                control->type = Control::Type::LABEL;
                control->defaultComponent = Surge::Skin::Components::Label;
            }
            else
            {
                // allow using case insensitive UI identifiers
                auto str = attrstr(lkid, "ui_identifier");
                std::transform(str.begin(), str.end(), str.begin(),
                               [](unsigned char c) { return std::tolower(c); });

                auto uid = str;
                auto conn = Surge::Skin::Connector::connectorByID(uid);
                if (!conn.payload ||
                    conn.payload->defaultComponent == Surge::Skin::Components::None)
                {
                    FIXMEERROR << "Got a default component of NONE for uiid '" << uid << "'"
                               << std::endl;
                }
                else
                {
                    control->copyFromConnector(conn, getVersion());
                    control->type = Control::Type::UIID;
                    control->ui_id = uid;
                }
            }

            /*
             * Per long discussion Feb 20 on discord, the expectation in the skin engine
             * is that you are overriding defaults *except* parent groups in the XML will reset
             * your default position to the origin. So if we *don't* specify an x/y and
             * *are* in a user specified group, reset the control default before we apply
             * the XML and offsets
             */
            if (parent->userGroup && getVersion() >= 2)
            {
                if (lkid->Attribute("x") == nullptr)
                {
                    control->x = 0;
                }
                if (lkid->Attribute("y") == nullptr)
                {
                    control->y = 0;
                }
            }

            attrintif(lkid, "x", control->x, parent->x);
            attrintif(lkid, "y", control->y, parent->y);

            attrintif(lkid, "w", control->w);
            attrintif(lkid, "h", control->h);

            // Candidate class name
            auto ccn = attrstr(lkid, "class");
            if (ccn != "")
                control->classname = ccn;

            std::vector<std::string> removeThese;
            for (auto a = lkid->FirstAttribute(); a; a = a->Next())
            {
                if (getVersion() == 1)
                {
                    /*
                     * A special case: bg_id and bg_resource are paired and in your skin
                     * you only say one, but if you say bg_resource you may have picked up
                     * a bg_id from the default. So queue up some removals.
                     */
                    if (std::string(a->Name()) == "bg_resource")
                        removeThese.push_back("bg_id");
                }
                control->allprops[a->Name()] = a->Value();
            }
            for (auto &c : removeThese)
            {
                control->allprops.erase(c);
            }

            controls.push_back(control);
            sessionIds.insert(control->sessionid);
            parent->childControls.push_back(control);
        }
        else
        {
            FIXMEERROR << "INVALID CONTROL" << std::endl;
            return false;
        }
    }

    if (toplevel)
    {
        /*
         * We now need to create all the base parent objects before we go
         * and resolve them, since that resolution can be recursive when
         * looping over controls, and we don't want to actually modify controls
         * while resolving.
         */
        std::set<std::string> baseParents;
        for (auto &c :
             Surge::Skin::Connector::connectorsByComponentType(Surge::Skin::Components::Group))
            baseParents.insert(c.payload->id);
        do
        {
            for (auto &bp : baseParents)
            {
                getOrCreateControlForConnector(bp);
            }
            baseParents.clear();
            for (auto c : controls)
            {
                if (c->allprops.find("base_parent") != c->allprops.end())
                {
                    auto bp = c->allprops["base_parent"];
                    auto pc = controlForUIID(bp);

                    if (!pc)
                    {
                        baseParents.insert(bp);
                    }
                }
            }
        } while (!baseParents.empty());

        controls.shrink_to_fit();
        for (auto &c : controls)
        {
            resolveBaseParentOffsets(c);
        }
    }
    return true;
}

juce::Colour Skin::colorFromHexString(const std::string &val) const
{
    uint32_t rgb;
    sscanf(val.c_str() + 1, "%x", &rgb);

    auto l = strlen(val.c_str() + 1);
    uint8_t a = 255;
    if (l > 6)
    {
        a = rgb % 256;
        rgb = rgb >> 8;
    }

    uint8_t b = rgb % 256;
    rgb = rgb >> 8;

    uint8_t g = rgb % 256;
    rgb = rgb >> 8;

    uint8_t r = rgb % 256;

    return juce::Colour(r, g, b, a);
}

bool Skin::hasColor(const std::string &iid) const
{
    auto id = iid;
    if (id[0] == '$') // when used as a value in a control you can have the $ or not, even though
                      // internally we strip it
        id = std::string(id.c_str() + 1);
    return colors.find(id) != colors.end();
}

juce::Font Skin::getFont(const Surge::Skin::FontDesc &d)
{
    jassert((int)Surge::Skin::FontDesc::plain == juce::Font::FontStyleFlags::plain);
    jassert((int)Surge::Skin::FontDesc::italic == juce::Font::FontStyleFlags::italic);
    jassert((int)Surge::Skin::FontDesc::bold == juce::Font::FontStyleFlags::bold);

    if (fontOverrides.find(d.id) != fontOverrides.end())
    {
        auto fo = fontOverrides[d.id];
        if (typeFaces.find(fo.family) != typeFaces.end())
            return juce::Font(juce::FontOptions(typeFaces[fo.family])).withPointHeight(fo.size);
        return juce::Font(juce::FontOptions(fo.family, fo.size, juce::Font::FontStyleFlags::plain));
    }

    if (d.hasParent)
    {
        return getFont(d.parent);
    }
    if (d.defaultFamily == Surge::Skin::FontDesc::SANS)
    {
        return fontManager->getLatoAtSize(d.size, (juce::Font::FontStyleFlags)d.style);
    }
    if (d.defaultFamily == Surge::Skin::FontDesc::MONO)
    {
        return fontManager->getFiraMonoAtSize(d.size);
    }

    static bool IHaveImplementedThis = false;
    jassert(IHaveImplementedThis);
    return fontManager->getLatoAtSize(d.size, (juce::Font::FontStyleFlags)d.style);
}

juce::Colour Skin::getColor(const std::string &iid, const juce::Colour &def,
                            std::unordered_set<std::string> noLoops) const
{
    auto id = iid;
    if (id[0] == '$')
        id = std::string(id.c_str() + 1);
    if (noLoops.find(id) != noLoops.end())
    {
        std::ostringstream oss;
        oss << "Resolving color '" << id
            << "' resulted in a loop. Please check the skin definition XML file. Colors which were "
               "visited during traversal are: ";
        for (auto l : noLoops)
            oss << "'" << l << "' ";
        // FIXME thread a storage here
        std::cout << oss.str() << std::endl;
        // storage->reportError(oss.str(), "Skin Configuration Error");
        return def;
    }
    noLoops.insert(id);
    auto q = colors.find(id);
    if (q != colors.end())
    {
        auto c = q->second;
        switch (c.type)
        {
        case ColorStore::COLOR:
            return c.color;
        case ColorStore::ALIAS:
            return getColor(c.alias, def, noLoops);
        case ColorStore::UNRESOLVED_ALIAS: // This should never occur
            return juce::Colours::red;
        }
    }

    if (id[0] == '#')
    {
        return colorFromHexString(id);
    }
    return def;
}

SurgeImage *Skin::backgroundBitmapForControl(Skin::Control::ptr_t c,
                                             std::shared_ptr<SurgeImageStore> bitmapStore)
{
    SurgeImage *bmp = nullptr;

    auto ms = propertyValue(c, Surge::Skin::Component::Properties::BACKGROUND);
    if (ms.has_value())
    {
        auto msv = *ms;
        auto msAsInt = std::atoi(msv.c_str());
        if (imageAllowedIds.find(msAsInt) != imageAllowedIds.end())
        {
            bmp = bitmapStore->getImage(std::atoi(msv.c_str()));
        }
        else
        {
            bmp = bitmapStore->getImageByStringID(msv);
            if (!bmp)
                bmp = bitmapStore->loadImageByPathForStringID(resourceName(msv), msv);
        }
    }
    return bmp;
}

SurgeImage *
Skin::hoverBitmapOverlayForBackgroundBitmap(Skin::Control::ptr_t c, SurgeImage *b,
                                            std::shared_ptr<SurgeImageStore> bitmapStore,
                                            HoverType t)
{
    if (!bitmapStore.get())
    {
        return nullptr;
    }

    if (c.get())
    {
        // YES it might. Show and document this
        // std::cout << "TODO: The component may have a name for a hover asset type=" << t << "
        // component=" << c->toString() << std::endl;
    }

    if (!b)
    {
        return nullptr;
    }

    std::ostringstream sid;

    if (b->resourceID < 0)
    {
        // we got a skin
        auto pos = b->fname.find("bmp00");
        if (pos != std::string::npos)
        {
            auto ftr = b->fname.substr(pos + 3);

            switch (t)
            {
            case HOVER:
                sid << defaultImageIDPrefix << "hover" << ftr;
                break;
            case HOVER_OVER_ON:
                sid << defaultImageIDPrefix << "hoverOn" << ftr;
                break;
            case TEMPOSYNC:
                sid << defaultImageIDPrefix << "bmpTS" << ftr;
                break;
            case HOVER_TEMPOSYNC:
                sid << defaultImageIDPrefix << "hoverTS" << ftr;
                break;
            }
            auto bmp = bitmapStore->getImageByStringID(sid.str());
            if (bmp)
                return bmp;
        }
    }
    else
    {
        switch (t)
        {
        case HOVER:
            sid << defaultImageIDPrefix << "hover" << std::setw(5) << std::setfill('0')
                << b->resourceID << ".svg";
            break;
        case HOVER_OVER_ON:
            sid << defaultImageIDPrefix << "hoverOn" << std::setw(5) << std::setfill('0')
                << b->resourceID << ".svg";
            break;
        case TEMPOSYNC:
            sid << defaultImageIDPrefix << "bmpTS" << std::setw(5) << std::setfill('0')
                << b->resourceID << ".svg";
        case HOVER_TEMPOSYNC:
            sid << defaultImageIDPrefix << "hoverTS" << std::setw(5) << std::setfill('0')
                << b->resourceID << ".svg";
            break;
        }
        auto bmp = bitmapStore->getImageByStringID(sid.str());
        if (bmp)
            return bmp;
    }

    return nullptr;
}

std::string Surge::GUI::Skin::hoverImageIdForResource(const int resource, HoverType t)
{
    std::ostringstream sid;
    switch (t)
    {
    case HOVER:
        sid << defaultImageIDPrefix << "hover" << std::setw(5) << std::setfill('0') << resource
            << ".svg";
        break;
    case HOVER_OVER_ON:
        sid << defaultImageIDPrefix << "hoverOn" << std::setw(5) << std::setfill('0') << resource
            << ".svg";
        break;
    case TEMPOSYNC:
        sid << defaultImageIDPrefix << "bmpTS" << std::setw(5) << std::setfill('0') << resource
            << ".svg";
        break;
    case HOVER_TEMPOSYNC:
        sid << defaultImageIDPrefix << "hoverTS" << std::setw(5) << std::setfill('0') << resource
            << ".svg";
        break;
    }
    return sid.str();
}

void Surge::GUI::Skin::Control::copyFromConnector(const Surge::Skin::Connector &c, int version)
{
    x = c.payload->posx;
    y = c.payload->posy;
    w = c.payload->w;
    h = c.payload->h;
    ui_id = c.payload->id;
    type = Control::UIID;
    defaultComponent = c.payload->defaultComponent;

    auto transferPropertyIf = [this, c](Surge::Skin::Component::Properties p,
                                        const std::string &target) {
        if (c.payload->properties.find(p) != c.payload->properties.end())
        {
            this->allprops[target] = c.payload->properties[p];
        }
    };

    if (c.payload->parentId != "")
    {
        this->allprops["base_parent"] = c.payload->parentId; // bit of a hack
    }

    auto dc = c.payload->defaultComponent;

    // There used to be a version 1 handling here which we stripped in XT
    classname = dc.payload->internalClassname;
    ultimateparentclassname = dc.payload->internalClassname;
    for (auto const &propNamePair : dc.payload->propertyNamesMap)
    {
        if (c.payload->properties.find(propNamePair.first) != c.payload->properties.end())
        {
            for (auto const &tt : propNamePair.second)
            {
                this->allprops[tt] = c.payload->properties[propNamePair.first];
            }
        }
    }
}

void Surge::GUI::Skin::resolveBaseParentOffsets(Skin::Control::ptr_t c)
{
    if (c->parentResolved)
        return;
    // When we enter here, all the objects will have been created by the loop above
    if (c->allprops.find("base_parent") != c->allprops.end())
    {
        // std::cout << "Control Parent " << _D(c->x) << _D(c->y) << std::endl;
        auto bp = c->allprops["base_parent"];
        auto pc = controlForUIID(bp);

        while (pc)
        {
            /*
             * A special case: If a group has control type 'none' then my control adopts it
             * This is the case only for none.
             */
            if (pc->classname == NoneClassName)
            {
                c->classname = NoneClassName;
                c->ultimateparentclassname = NoneClassName;
            }
            c->x += pc->x;
            c->y += pc->y;
            if (pc->allprops.find("base_parent") != pc->allprops.end())
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

Skin::Control::Control()
{
    // I assume this is only called from UI thread so is single threaded, thus
    static sessionid_t sidc = 1;
    sessionid = sidc++;
}

std::array<SurgeImage *, 3>
Skin::standardHoverAndHoverOnForControl(Skin::Control::ptr_t c, std::shared_ptr<SurgeImageStore> b)
{
    auto csb = backgroundBitmapForControl(c, b);
    return standardHoverAndHoverOnForCSB(csb, c, b);
}
std::array<SurgeImage *, 3> Skin::standardHoverAndHoverOnForIDB(int id,
                                                                std::shared_ptr<SurgeImageStore> b)
{
    auto csb = b->getImage(id);
    return standardHoverAndHoverOnForCSB(csb, nullptr, b);
}
std::array<SurgeImage *, 3> Skin::standardHoverAndHoverOnForCSB(SurgeImage *csb,
                                                                Skin::Control::ptr_t c,
                                                                std::shared_ptr<SurgeImageStore> b)
{
    std::array<SurgeImage *, 3> res;
    res[0] = csb;

    auto hoverBmp =
        hoverBitmapOverlayForBackgroundBitmap(c, csb, b, Surge::GUI::Skin::HoverType::HOVER);

    res[1] = hoverBmp ? hoverBmp : nullptr;

    auto hoverOnBmp = hoverBitmapOverlayForBackgroundBitmap(
        c, csb, b, Surge::GUI::Skin::HoverType::HOVER_OVER_ON);

    res[2] = hoverOnBmp ? hoverOnBmp : nullptr;
    return res;
}

std::optional<SkinDB::Entry> SkinDB::installSkinFromPathToUserDirectory(SurgeStorage *s,
                                                                        const fs::path &p)
{
    auto parentP = p.parent_path();
    auto nmP = p.lexically_relative(parentP);
    auto tgPath = s->userSkinsPath / nmP;

    try
    {
        fs::create_directories(tgPath);
        fs::copy(p, tgPath, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    }
    catch (const fs::filesystem_error &e)
    {
        // This will give us a broken skin but no need to tell the users
        FIXMEERROR << "Unable to install Skin: " << e.what();
        return {};
    }
    rescanForSkins(s);

    auto nameS = path_to_string(nmP);
    for (auto &a : availableSkins)
    {
        if (a.rootType == USER && a.name.find(nameS) == 0)
            return a;
    }
    return {};
}

} // namespace GUI
} // namespace Surge
