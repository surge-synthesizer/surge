#include "globals.h"
#include "SurgeGUIEditor.h"
#include "DspUtilities.h"
#include "CSnapshotMenu.h"
#include "effect/Effect.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeStorage.h"
#include "filesystem/import.h"
#include "SkinColors.h"
#include "RuntimeFont.h"
#include "guihelpers.h"
#include "StringOps.h"

#include <iostream>
#include <iomanip>
#include <queue>

using namespace VSTGUI;

extern CFontRef displayFont;

// CSnapshotMenu

CSnapshotMenu::CSnapshotMenu(const CRect &size, IControlListener *listener, long tag,
                             SurgeStorage *storage)
    : COptionMenu(size, nullptr, tag, 0)
{
    this->storage = storage;
    this->listenerNotForParent = listener;
}
CSnapshotMenu::~CSnapshotMenu() {}

CMouseEventResult CSnapshotMenu::onMouseDown(CPoint &where, const CButtonState &button)
{
    if (listenerNotForParent && (button & (kMButton | kButton4 | kButton5)))
    {
        listenerNotForParent->controlModifierClicked(this, button);
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }
    return COptionMenu::onMouseDown(where, button);
}

void CSnapshotMenu::draw(CDrawContext *dc) { setDirty(false); }

bool CSnapshotMenu::canSave() { return false; }

void CSnapshotMenu::populate()
{
    int main = 0, sub = 0;
    const long max_main = 128, max_sub = 256;

    int idx = 0;
    TiXmlElement *sect = storage->getSnapshotSection(mtype);
    if (sect)
    {
        TiXmlElement *type = sect->FirstChildElement();

        while (type)
        {
            if (type->Value() && strcmp(type->Value(), "type") == 0)
            {
                auto sm = populateSubmenuFromTypeElement(type, this, main, sub, max_sub, idx);

                if (sm)
                {
                    addToTopLevelTypeMenu(type, sm, idx);
                }
            }
            else if (type->Value() && strcmp(type->Value(), "separator") == 0)
            {
                addSeparator();
            }

            type = type->NextSiblingElement();
            main++;

            if (main >= max_main)
            {
                break;
            }
        }
    }
    maxIdx = idx;
}

bool CSnapshotMenu::loadSnapshotByIndex(int idx)
{
    int cidx = 0;
    // This isn't that efficient but you know
    TiXmlElement *sect = storage->getSnapshotSection(mtype);
    if (sect)
    {
        std::queue<TiXmlElement *> typeD;
        typeD.push(TINYXML_SAFE_TO_ELEMENT(sect->FirstChild("type")));
        while (!typeD.empty())
        {
            auto type = typeD.front();
            typeD.pop();
            int type_id = 0;
            type->Attribute("i", &type_id);
            TiXmlElement *snapshot = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("snapshot"));
            while (snapshot)
            {
                int snapshotTypeID = type_id, tmpI = 0;
                if (snapshot->Attribute("i", &tmpI) != nullptr)
                {
                    snapshotTypeID = tmpI;
                }

                if (cidx == idx)
                {
                    selectedIdx = cidx;
                    loadSnapshot(snapshotTypeID, snapshot, cidx);
                    if (listenerNotForParent)
                        listenerNotForParent->valueChanged(this);
                    return true;
                }
                snapshot = TINYXML_SAFE_TO_ELEMENT(snapshot->NextSibling("snapshot"));
                cidx++;
            }

            auto subType = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("type"));
            while (subType)
            {
                typeD.push(subType);
                subType = TINYXML_SAFE_TO_ELEMENT(subType->NextSibling("type"));
            }

            auto next = TINYXML_SAFE_TO_ELEMENT(type->NextSibling("type"));
            if (next)
                typeD.push(next);
        }
    }
    if (idx < 0 && cidx + idx > 0)
    {
        // I've gone off the end
        return (loadSnapshotByIndex(cidx + idx));
    }
    return false;
}

VSTGUI::COptionMenu *CSnapshotMenu::populateSubmenuFromTypeElement(TiXmlElement *type,
                                                                   VSTGUI::COptionMenu *parent,
                                                                   int &main, int &sub,
                                                                   const long &max_sub, int &idx)
{
    /*
    ** Begin by grabbing all the snapshot elements
    */
    std::string txt;
    int type_id = 0;
    type->Attribute("i", &type_id);
    sub = 0;
    COptionMenu *subMenu = new COptionMenu(getViewSize(), 0, main, 0, 0, kNoDrawStyle);
    subMenu->setNbItemsPerColumn(20);
    TiXmlElement *snapshot = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("snapshot"));
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

        if (firstSnapshotByType.find(snapshotTypeID) == firstSnapshotByType.end())
            firstSnapshotByType[snapshotTypeID] = idx;
        auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(txt.c_str()));
        auto action = [this, snapshot, snapshotTypeID, idx](CCommandMenuItem *item) {
            this->selectedIdx = idx;
            this->loadSnapshot(snapshotTypeID, snapshot, idx);
            if (this->listenerNotForParent)
                this->listenerNotForParent->valueChanged(this);
        };
        loadArgsByIndex.push_back(std::make_pair(snapshotTypeID, snapshot));
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
    TiXmlElement *subType = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("type"));
    while (subType)
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

        if (firstSnapshotByType.find(type_id) == firstSnapshotByType.end())
            firstSnapshotByType[type_id] = idx;

        auto action = [this, type_id, type, idx](CCommandMenuItem *item) {
            this->selectedIdx = 0;
            this->loadSnapshot(type_id, type, idx);
            if (this->listenerNotForParent)
                this->listenerNotForParent->valueChanged(this);
        };

        loadArgsByIndex.push_back(std::make_pair(type_id, type));
        idx++;
        actionItem->setActions(action, nullptr);
        parent->addEntry(actionItem);
    }

    subMenu->forget();
    if (sub)
        return subMenu; // OK to return forgoten since it has lifetime of parent
    else
        return nullptr;
}

// COscMenu

COscMenu::COscMenu(const CRect &size, IControlListener *listener, long tag, SurgeStorage *storage,
                   OscillatorStorage *osc, std::shared_ptr<SurgeBitmaps> bitmapStore)
    : CSnapshotMenu(size, listener, tag, storage)
{
    strcpy(mtype, "osc");
    this->osc = osc;
    this->storage = storage;
    auto tb = bitmapStore->getBitmap(IDB_OSC_MENU);
    bmp = tb;
    populate();
}

void COscMenu::draw(CDrawContext *dc)
{
    if (!attemptedHoverLoad)
    {
        hoverBmp = skin->hoverBitmapOverlayForBackgroundBitmap(
            skinControl, dynamic_cast<CScalableBitmap *>(bmp), associatedBitmapStore,
            Surge::UI::Skin::HOVER);
        attemptedHoverLoad = true;
    }
    CRect size = getViewSize();
    int i = osc->type.val.i;
    int y = i * size.getHeight();

    // we're using code generated text for this menu from skin version 2 onwards
    // so there's no need to slice the graphics asset
    if (skin->getVersion() >= 2)
    {
        y = 0;
    }

    if (bmp)
        bmp->draw(dc, size, CPoint(0, y), 0xff);

    if (isHovered && hoverBmp)
        hoverBmp->draw(dc, size, CPoint(0, y), 0xff);

    if (skin->getVersion() >= 2)
    {
        dc->saveGlobalState();

        dc->setDrawMode(VSTGUI::kAntiAliasing | VSTGUI::kNonIntegralMode);
        dc->setFont(Surge::GUI::getLatoAtSize(font_size, font_style));

        if (isHovered)
        {
            dc->setFontColor(skin->getColor(Colors::Osc::Type::TextHover));
        }
        else
        {
            dc->setFontColor(skin->getColor(Colors::Osc::Type::Text));
        }

        std::string txt = osc_type_shortnames[i];

        if (text_allcaps)
        {
            std::transform(txt.begin(), txt.end(), txt.begin(), ::toupper);
        }

        dc->drawString(txt.c_str(), size.offset(text_hoffset, text_voffset), text_align, true);

        dc->restoreGlobalState();
    }

    setDirty(false);
}

void COscMenu::loadSnapshot(int type, TiXmlElement *e, int idx)
{
    assert(within_range(0, type, n_osc_types));
    auto sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
    if (sge)
    {
        auto sc = sge->current_scene;
        sge->oscilatorMenuIndex[sc][sge->current_osc[sc]] = idx;
    }
    osc->queue_type = type;
    osc->queue_xmldata = e;
}

bool COscMenu::onWheel(const VSTGUI::CPoint &where, const float &distance,
                       const VSTGUI::CButtonState &buttons)
{
    accumWheel += distance;
#if WINDOWS // rough hack but it still takes too many mousewheel clicks to get outside of Classic
            // osc subfolder onto other osc types!
    accumWheel += distance;
#endif

    auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
    if (sge)
    {
        auto sc = sge->current_scene;
        int currentIdx = sge->oscilatorMenuIndex[sc][sge->current_osc[sc]];
        if (accumWheel < -1)
        {
            currentIdx = currentIdx + 1;
            if (currentIdx >= maxIdx)
                currentIdx = 0;
            accumWheel = 0;
            auto args = loadArgsByIndex[currentIdx];
            loadSnapshot(args.first, args.second, currentIdx);
        }
        else if (accumWheel > 1)
        {
            currentIdx = currentIdx - 1;
            if (currentIdx < 0)
                currentIdx = maxIdx - 1;
            accumWheel = 0;
            auto args = loadArgsByIndex[currentIdx];
            loadSnapshot(args.first, args.second, currentIdx);
        }
    }
    return true;
}

/*void COscMenu::load_snapshot(int type, TiXmlElement *e)
{
        assert(within_range(0,type,n_osc_types));
        osc->type.val.i = type;
        //osc->retrigger.val.i =
        storage->patch.update_controls(false, osc);
        if(e)
        {
                for(int i=0; i<n_osc_params; i++)
                {
                        double d; int j;
                        char lbl[TXT_SIZE];
                        snprintf(lbl, TXT_SIZE, "p%i",i);
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

std::vector<float> CFxMenu::fxCopyPaste;

CFxMenu::CFxMenu(const CRect &size, IControlListener *listener, long tag, SurgeStorage *storage,
                 FxStorage *fx, FxStorage *fxbuffer, int slot)
    : CSnapshotMenu(size, listener, tag, storage)
{
    strcpy(mtype, "fx");
    this->fx = fx;
    this->fxbuffer = fxbuffer;
    this->slot = slot;
    selectedName = "";
    populate();
}

void CFxMenu::draw(CDrawContext *dc)
{
    CRect lbox = getViewSize();
    lbox.right--;
    lbox.bottom--;

    if (!triedToLoadBmp)
    {
        triedToLoadBmp = true;
        pBackground = associatedBitmapStore->getBitmap(IDB_MENU_AS_SLIDER);
        pBackgroundHover = skin->hoverBitmapOverlayForBackgroundBitmap(
            skinControl, dynamic_cast<CScalableBitmap *>(pBackground), associatedBitmapStore,
            Surge::UI::Skin::HoverType::HOVER);
    }

    if (pBackground)
    {
        pBackground->draw(dc, getViewSize(), CPoint(0, 0), 0xff);
    }

    if (isHovered && pBackgroundHover)
    {
        pBackgroundHover->draw(dc, getViewSize(), CPoint(0, 0), 0xff);
    }

    // hover color and position
    auto fgc = skin->getColor(Colors::Effect::Menu::Text);

    if (isHovered)
        fgc = skin->getColor(Colors::Effect::Menu::TextHover);

    dc->setFontColor(fgc);
    dc->setFont(displayFont);

    CRect txtbox(getViewSize());
    txtbox.inset(2, 2);
    txtbox.left += 4;
    txtbox.right -= 12;
    dc->drawString(fxslot_names[slot], txtbox, kLeftText, true);

    char fxname[NAMECHARS];
    snprintf(fxname, NAMECHARS, "%s", fx_type_names[fx->type.val.i]);
    dc->drawString(fxname, txtbox, kRightText, true);

    setDirty(false);
}

void CFxMenu::loadSnapshot(int type, TiXmlElement *e, int idx)
{
    if (!type)
        fxbuffer->type.val.i = type;

    if (e)
    {
        fxbuffer->type.val.i = type;
        // storage->patch.update_controls();
        selectedName = e->Attribute("name");

        Effect *t_fx = spawn_effect(type, storage, fxbuffer, 0);
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
            char lbl[TXT_SIZE], sublbl[TXT_SIZE];
            snprintf(lbl, TXT_SIZE, "p%i", i);
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

            snprintf(sublbl, TXT_SIZE, "p%i_temposync", i);
            fxbuffer->p[i].temposync =
                ((e->QueryIntAttribute(sublbl, &j) == TIXML_SUCCESS) && (j == 1));
            snprintf(sublbl, TXT_SIZE, "p%i_extend_range", i);
            fxbuffer->p[i].extend_range =
                ((e->QueryIntAttribute(sublbl, &j) == TIXML_SUCCESS) && (j == 1));
            snprintf(sublbl, TXT_SIZE, "p%i_deactivated", i);
            fxbuffer->p[i].deactivated =
                ((e->QueryIntAttribute(sublbl, &j) == TIXML_SUCCESS) && (j == 1));
        }
    }
#if TARGET_LV2
    auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
    if (sge)
    {
        sge->forceautomationchangefor(&(fxbuffer->type));
        for (int i = 0; i < n_fx_params; ++i)
            sge->forceautomationchangefor(&(fxbuffer->p[i]));
    }
#endif
}
void CFxMenu::saveSnapshot(TiXmlElement *e, const char *name)
{
    if (fx->type.val.i == 0)
    {
        return;
    }

    TiXmlElement *t = TINYXML_SAFE_TO_ELEMENT(e->FirstChild("type"));

    while (t)
    {
        int ii;

        if ((t->QueryIntAttribute("i", &ii) == TIXML_SUCCESS) && (ii == fx->type.val.i))
        {
            // if name already exists, delete old entry
            TiXmlElement *sn = TINYXML_SAFE_TO_ELEMENT(t->FirstChild("snapshot"));
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
                char lbl[TXT_SIZE], txt[TXT_SIZE], sublbl[TXT_SIZE];
                snprintf(lbl, TXT_SIZE, "p%i", p);

                if (fx->p[p].ctrltype != ct_none)
                {
                    neu.SetAttribute(lbl, fx->p[p].get_storage_value(txt));

                    if (fx->p[p].temposync)
                    {
                        snprintf(sublbl, TXT_SIZE, "p%i_temposync", p);
                        neu.SetAttribute(sublbl, "1");
                    }
                    if (fx->p[p].extend_range)
                    {
                        snprintf(sublbl, TXT_SIZE, "p%i_extend_range", p);
                        neu.SetAttribute(sublbl, "1");
                    }
                    if (fx->p[p].deactivated)
                    {
                        snprintf(sublbl, TXT_SIZE, "p%i_deactivated", p);
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
std::unordered_map<int, std::vector<CFxMenu::UserPreset>> CFxMenu::userPresets;

void CFxMenu::rescanUserPresets()
{
    userPresets.clear();
    scanForUserPresets = false;

    auto ud = storage->userFXPath;

    std::vector<fs::path> sfxfiles;

    std::deque<fs::path> workStack;
    workStack.push_back(fs::path(ud));

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
                        workStack.push_back(d);
                    }
                    else if (path_to_string(d.path().extension()) == ".srgfx")
                    {
                        sfxfiles.push_back(d.path());
                    }
                }
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        std::ostringstream oss;
        oss << "Experienced file system error when scanning user FX. " << e.what();
        Surge::UserInteractions::promptError(oss.str(), "FileSystem Error");
    }

    for (const auto &f : sfxfiles)
    {
        {
            UserPreset preset;
            preset.file = path_to_string(f);
            TiXmlDocument d;
            int t;

            if (!d.LoadFile(f))
                goto badPreset;

            auto r = TINYXML_SAFE_TO_ELEMENT(d.FirstChild("single-fx"));

            if (!r)
                goto badPreset;

            auto s = TINYXML_SAFE_TO_ELEMENT(r->FirstChild("snapshot"));

            if (!s)
                goto badPreset;

            if (!s->Attribute("name"))
                goto badPreset;

            preset.name = s->Attribute("name");

            if (s->QueryIntAttribute("type", &t) != TIXML_SUCCESS)
                goto badPreset;

            preset.type = t;

            for (int i = 0; i < n_fx_params; ++i)
            {
                double fl;
                std::string p = "p";

                if (s->QueryDoubleAttribute((p + std::to_string(i)).c_str(), &fl) == TIXML_SUCCESS)
                {
                    preset.p[i] = fl;
                }

                if (s->QueryDoubleAttribute((p + std::to_string(i) + "_temposync").c_str(), &fl) ==
                        TIXML_SUCCESS &&
                    fl != 0)
                {
                    preset.ts[i] = true;
                }

                if (s->QueryDoubleAttribute((p + std::to_string(i) + "_extend_range").c_str(),
                                            &fl) == TIXML_SUCCESS &&
                    fl != 0)
                {
                    preset.er[i] = true;
                }

                if (s->QueryDoubleAttribute((p + std::to_string(i) + "_deactivated").c_str(),
                                            &fl) == TIXML_SUCCESS &&
                    fl != 0)
                {
                    preset.da[i] = true;
                }
            }

            if (userPresets.find(preset.type) == userPresets.end())
            {
                userPresets[preset.type] = std::vector<UserPreset>();
            }

            userPresets[preset.type].push_back(preset);
        }

    badPreset:;
    }

    for (auto &a : userPresets)
    {
        std::sort(a.second.begin(), a.second.end(), [](UserPreset a, UserPreset b) {
            if (a.type == b.type)
            {
                return _stricmp(a.name.c_str(), b.name.c_str()) < 0;
            }
            else
            {
                return a.type < b.type;
            }
        });
    }
}

void CFxMenu::populate()
{
    /*
    ** Are there user presets
    */
    if (scanForUserPresets || true) // for now
    {
        rescanUserPresets();
    }

    CSnapshotMenu::populate();

    /*
    ** Add copy/paste/save
    */

    this->addSeparator();

    auto copyItem = new CCommandMenuItem(CCommandMenuItem::Desc("Copy"));
    auto copy = [this](CCommandMenuItem *item) { this->copyFX(); };
    copyItem->setActions(copy, nullptr);
    this->addEntry(copyItem);

    auto pasteItem = new CCommandMenuItem(CCommandMenuItem::Desc("Paste"));
    auto paste = [this](CCommandMenuItem *item) { this->pasteFX(); };
    pasteItem->setActions(paste, nullptr);
    this->addEntry(pasteItem);

    this->addSeparator();

    if (fx->type.val.i != fxt_off)
    {
        auto saveItem = new CCommandMenuItem(
            CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Save FX Preset")));
        saveItem->setActions([this](CCommandMenuItem *item) { this->saveFX(); });
        this->addEntry(saveItem);
    }

    auto rescanItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh FX Preset List")));
    rescanItem->setActions([this](CCommandMenuItem *item) {
        scanForUserPresets = true;
        auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
        if (sge)
            sge->queueRebuildUI();
    });
    this->addEntry(rescanItem);
}

void CFxMenu::copyFX()
{
    /*
    ** This is a junky implementation until I figure out save and load which will require me to
    *stream this
    */
    if (fxCopyPaste.size() == 0)
    {
        fxCopyPaste.resize(n_fx_params * 4 + 1); // type then (val; ts; extend; deact)
    }

    fxCopyPaste[0] = fx->type.val.i;
    for (int i = 0; i < n_fx_params; ++i)
    {
        int vp = i * 4 + 1;
        int tp = i * 4 + 2;
        int xp = i * 4 + 3;
        int dp = i * 4 + 4;

        switch (fx->p[i].valtype)
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
    memcpy((void *)fxbuffer, (void *)fx, sizeof(FxStorage));
}

void CFxMenu::pasteFX()
{
    if (fxCopyPaste.size() == 0)
    {
        return;
    }

    fxbuffer->type.val.i = (int)fxCopyPaste[0];

    Effect *t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);
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

        switch (fxbuffer->p[i].valtype)
        {
        case vt_float:
        {
            fxbuffer->p[i].val.f = fxCopyPaste[vp];
            if (fxbuffer->p[i].val.f < fxbuffer->p[i].val_min.f)
            {
                fxbuffer->p[i].val.f = fxbuffer->p[i].val_min.f;
            }
            if (fxbuffer->p[i].val.f > fxbuffer->p[i].val_max.f)
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

    selectedName = std::string("Copied ") + fx_type_names[fxbuffer->type.val.i];

    if (listenerNotForParent)
        listenerNotForParent->valueChanged(this);
}

void CFxMenu::saveFX()
{
    auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
    if (sge)
    {
        sge->promptForMiniEdit("", "Enter a name for the FX preset:", "Save FX Preset",
                               CPoint(-1, -1), [this](const std::string &s) { this->saveFXIn(s); });
    }
}
void CFxMenu::saveFXIn(const std::string &s)
{
    char fxName[TXT_SIZE];
    fxName[0] = 0;
    strxcpy(fxName, s.c_str(), TXT_SIZE);

    if (strlen(fxName) == 0)
    {
        return;
    }

    if (!Surge::Storage::isValidName(fxName))
    {
        return;
    }

    int ti = fx->type.val.i;

    std::ostringstream oss;
    oss << storage->userFXPath << PATH_SEPARATOR << fx_type_names[ti] << PATH_SEPARATOR;

    auto pn = oss.str();
    fs::create_directories(string_to_path(pn));

    auto fn = pn + fxName + ".srgfx";
    std::ofstream pfile(fn, std::ios::out);
    if (!pfile.is_open())
    {
        Surge::UserInteractions::promptError(
            std::string("Unable to open FX preset file '") + fn + "' for writing!", "Error");
        return;
    }

    // this used to say streaming_versio before (was a typo)
    // make sure both variants are checked when checking sv in the future on patch load
    pfile << "<single-fx streaming_version=\"" << ff_revision << "\">\n";

    // take care of 5 special XML characters
    std::string fxNameSub(fxName);
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("&"), std::string("&amp;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("<"), std::string("&lt;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string(">"), std::string("&gt;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("\""), std::string("&quot;"));
    Surge::Storage::findReplaceSubstring(fxNameSub, std::string("'"), std::string("&apos;"));

    pfile << "  <snapshot name=\"" << fxNameSub.c_str() << "\" \n";

    pfile << "     type=\"" << fx->type.val.i << "\"\n";
    for (int i = 0; i < n_fx_params; ++i)
    {
        if (fx->p[i].ctrltype != ct_none)
        {
            switch (fx->p[i].valtype)
            {
            case vt_float:
                pfile << "     p" << i << "=\"" << fx->p[i].val.f << "\"\n";
                break;
            case vt_int:
                pfile << "     p" << i << "=\"" << fx->p[i].val.i << "\"\n";
                break;
            }

            if (fx->p[i].can_temposync() && fx->p[i].temposync)
            {
                pfile << "     p" << i << "_temposync=\"1\"\n";
            }
            if (fx->p[i].can_extend_range() && fx->p[i].extend_range)
            {
                pfile << "     p" << i << "_extend_range=\"1\"\n";
            }
            if (fx->p[i].can_deactivate() && fx->p[i].deactivated)
            {
                pfile << "     p" << i << "_deactivated=\"1\"\n";
            }
        }
    }

    pfile << "  />\n";
    pfile << "</single-fx>\n";
    pfile.close();

    scanForUserPresets = true;
    auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
    if (sge)
        sge->queueRebuildUI();
}

void CFxMenu::loadUserPreset(const UserPreset &p)
{
    fxbuffer->type.val.i = p.type;

    Effect *t_fx = spawn_effect(fxbuffer->type.val.i, storage, fxbuffer, 0);

    if (t_fx)
    {
        t_fx->init_ctrltypes();
        t_fx->init_default_values();
        delete t_fx;
    }

    for (int i = 0; i < n_fx_params; i++)
    {
        switch (fxbuffer->p[i].valtype)
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
        fxbuffer->p[i].temposync = (int)p.ts[i];
        fxbuffer->p[i].extend_range = (int)p.er[i];
        fxbuffer->p[i].deactivated = (int)p.da[i];
    }

    selectedIdx = -1;
    selectedName = p.name;

    if (listenerNotForParent)
    {
        listenerNotForParent->valueChanged(this);
    }
}

void CFxMenu::addToTopLevelTypeMenu(TiXmlElement *type, VSTGUI::COptionMenu *subMenu, int &idx)
{
    if (!type || !subMenu)
        return;

    int type_id = 0;
    type->Attribute("i", &type_id);

    if (userPresets.find(type_id) == userPresets.end() || userPresets[type_id].size() == 0)
        return;

    auto factory_add = subMenu->addEntry("FACTORY PRESETS", 0);
    factory_add->setEnabled(0);

    auto user_add = subMenu->addEntry("USER PRESETS");
    user_add->setEnabled(0);

    for (auto &ps : userPresets[type_id])
    {
        auto fxName = ps.name;

#if WINDOWS
        Surge::Storage::findReplaceSubstring(fxName, std::string("&"), std::string("&&"));
#endif

        auto i = new CCommandMenuItem(CCommandMenuItem::Desc(fxName.c_str()));
        i->setActions([this, ps](CCommandMenuItem *item) { this->loadUserPreset(ps); });
        subMenu->addEntry(i);
    }
}
