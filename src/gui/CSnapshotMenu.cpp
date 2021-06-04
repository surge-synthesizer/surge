#include "globals.h"
#include "SurgeGUIEditor.h"
#include "DSPUtils.h"
#include "CSnapshotMenu.h"
#include "Effect.h"
#include "CScalableBitmap.h"
#include "SurgeBitmaps.h"
#include "SurgeStorage.h"
#include "filesystem/import.h"
#include "SkinColors.h"
#include "RuntimeFont.h"
#include "SurgeGUIUtils.h"
#include "StringOps.h"

#include <iostream>
#include <iomanip>
#include <queue>

using namespace VSTGUI;

// CSnapshotMenu

CSnapshotMenu::CSnapshotMenu(const CRect &size, IControlListener *listener, long tag,
                             SurgeStorage *storage)
    : CControl(size, nullptr, tag)
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

    menu.showMenuAsync(juce::PopupMenu::Options());

    return kMouseEventHandled;
}

void CSnapshotMenu::draw(CDrawContext *dc) { setDirty(false); }

bool CSnapshotMenu::canSave() { return false; }

void CSnapshotMenu::populate()
{
    int idx = 0;
    TiXmlElement *sect = storage->getSnapshotSection(mtype);

    if (sect)
    {
        TiXmlElement *type = sect->FirstChildElement();

        while (type)
        {
            if (type->Value() && strcmp(type->Value(), "type") == 0)
            {
                populateSubmenuFromTypeElement(type, menu, idx, true);
            }
            else if (type->Value() && strcmp(type->Value(), "separator") == 0)
            {
                menu.addSeparator();
            }

            type = type->NextSiblingElement();
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

void CSnapshotMenu::populateSubmenuFromTypeElement(TiXmlElement *type, juce::PopupMenu &parent,
                                                   int &idx, bool isTop)
{
    /*
    ** Begin by grabbing all the snapshot elements
    */
    std::string txt;
    int type_id = 0;
    type->Attribute("i", &type_id);
    int sub = 0;

    juce::PopupMenu subMenu;

    if (isTop)
    {
        setMenuStartHeader(type, subMenu);
    }
    TiXmlElement *snapshot = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("snapshot"));
    while (snapshot)
    {
        txt = snapshot->Attribute("name");

        int snapshotTypeID = type_id;
        int tmpI;

        if (snapshot->Attribute("i", &tmpI) != nullptr)
        {
            snapshotTypeID = tmpI;
        }

        if (firstSnapshotByType.find(snapshotTypeID) == firstSnapshotByType.end())
            firstSnapshotByType[snapshotTypeID] = idx;

        auto action = [this, snapshot, snapshotTypeID, idx]() {
            this->selectedIdx = idx;
            this->loadSnapshot(snapshotTypeID, snapshot, idx);
            if (this->listenerNotForParent)
                this->listenerNotForParent->valueChanged(this);
        };
        loadArgsByIndex.push_back(std::make_pair(snapshotTypeID, snapshot));
        subMenu.addItem(txt, action);
        idx++;

        snapshot = TINYXML_SAFE_TO_ELEMENT(snapshot->NextSibling("snapshot"));
        sub++;
    }

    /*
    ** Next see if we have any subordinate types
    */
    TiXmlElement *subType = TINYXML_SAFE_TO_ELEMENT(type->FirstChild("type"));
    while (subType)
    {
        populateSubmenuFromTypeElement(subType, subMenu, idx);
        subType = TINYXML_SAFE_TO_ELEMENT(subType->NextSibling("type"));
    }

    if (isTop)
    {
        addToTopLevelTypeMenu(type, subMenu, idx);
    }

    /*
    ** Then add myself to parent
    */
    txt = type->Attribute("name");

    if (sub)
    {
        parent.addSubMenu(txt, subMenu);
    }
    else
    {
        if (firstSnapshotByType.find(type_id) == firstSnapshotByType.end())
            firstSnapshotByType[type_id] = idx;

        auto action = [this, type_id, type, idx]() {
            this->selectedIdx = 0;
            this->loadSnapshot(type_id, type, idx);
            if (this->listenerNotForParent)
                this->listenerNotForParent->valueChanged(this);
        };

        loadArgsByIndex.push_back(std::make_pair(type_id, type));
        parent.addItem(txt, action);
        idx++;
    }
}

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

CFxMenu::~CFxMenu() {}

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
            Surge::GUI::Skin::HoverType::HOVER);
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
    dc->setFont(Surge::GUI::getFontManager()->displayFont);

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

void CFxMenu::populate()
{
    /*
    ** Are there user presets
    */
    Surge::FxUserPreset::forcePresetRescan(storage);

    CSnapshotMenu::populate();

    /*
    ** Add copy/paste/save
    */

    menu.addSeparator();
    menu.addItem("Copy", [this]() { this->copyFX(); });
    menu.addItem("Paste", [this]() { this->pasteFX(); });

    menu.addSeparator();

    if (fx->type.val.i != fxt_off)
    {
        menu.addItem(Surge::GUI::toOSCaseForMenu("Save FX Preset"), [this]() { this->saveFX(); });
    }

    auto rsA = [this]() {
        Surge::FxUserPreset::forcePresetRescan(this->storage);
        auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
        if (sge)
            sge->queueRebuildUI();
    };
    menu.addItem(Surge::GUI::toOSCaseForMenu("Refresh FX Preset List"), rsA);
}

Surge::FxClipboard::Clipboard CFxMenu::fxClipboard;

void CFxMenu::copyFX()
{
    Surge::FxClipboard::copyFx(storage, fx, fxClipboard);
    memcpy((void *)fxbuffer, (void *)fx, sizeof(FxStorage));
}

void CFxMenu::pasteFX()
{
    Surge::FxClipboard::pasteFx(storage, fxbuffer, fxClipboard);

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
                               CPoint(-1, -1), [this](const std::string &s) {
                                   Surge::FxUserPreset::saveFxIn(this->storage, fx, s);
                                   auto *sge = dynamic_cast<SurgeGUIEditor *>(listenerNotForParent);
                                   if (sge)
                                       sge->queueRebuildUI();
                               });
    }
}

void CFxMenu::loadUserPreset(const Surge::FxUserPreset::Preset &p)
{
    Surge::FxUserPreset::loadPresetOnto(p, storage, fxbuffer);

    selectedIdx = -1;
    selectedName = p.name;

    if (listenerNotForParent)
    {
        listenerNotForParent->valueChanged(this);
    }
}

void CFxMenu::addToTopLevelTypeMenu(TiXmlElement *type, juce::PopupMenu &subMenu, int &idx)
{
    if (!type)
        return;

    int type_id = 0;
    type->Attribute("i", &type_id);

    auto ps = Surge::FxUserPreset::getPresetsForSingleType(type_id);

    if (ps.empty())
    {
        return;
    }

    subMenu.addColumnBreak();
    subMenu.addSectionHeader("User Presets");

    for (auto &pst : ps)
    {
        auto fxName = pst.name;
        subMenu.addItem(fxName, [this, pst]() { this->loadUserPreset(pst); });
    }
}
void CFxMenu::setMenuStartHeader(TiXmlElement *type, juce::PopupMenu &subMenu)
{
    if (!type)
        return;

    int type_id = 0;
    type->Attribute("i", &type_id);

    if (!Surge::FxUserPreset::hasPresetsForSingleType(type_id))
    {
        return;
    }

    subMenu.addSectionHeader("Factory Presets");
}
