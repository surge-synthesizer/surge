/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2021 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "XMLConfiguredMenus.h"
#include "SurgeStorage.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Widgets
{

void XMLMenuPopulator::populate()
{
    menu = juce::PopupMenu();

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

bool XMLMenuPopulator::loadSnapshotByIndex(int idx)
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
                    if (getControlListener())
                        getControlListener()->valueChanged(asControlValueInterface());
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

void XMLMenuPopulator::populateSubmenuFromTypeElement(TiXmlElement *type, juce::PopupMenu &parent,
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
            if (this->getControlListener())
                this->getControlListener()->valueChanged(asControlValueInterface());
        };
        // loadArgsByIndex.push_back(std::make_pair(snapshotTypeID, snapshot));
        loadArgsByIndex.emplace_back(snapshotTypeID, snapshot);
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
            if (this->getControlListener())
                this->getControlListener()->valueChanged(asControlValueInterface());
        };

        loadArgsByIndex.emplace_back(type_id, type);
        parent.addItem(txt, action);
        idx++;
    }
}

OscillatorMenu::OscillatorMenu() { strcpy(mtype, "osc"); }

void OscillatorMenu::paint(juce::Graphics &g)
{
    bg->draw(g, 1.0);
    if (isHovered && bgHover)
        bgHover->draw(g, 1.0);

    int i = osc->type.val.i;

    g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(font_size, font_style));

    if (isHovered)
    {
        g.setColour(skin->getColor(Colors::Osc::Type::TextHover));
    }
    else
    {
        g.setColour(skin->getColor(Colors::Osc::Type::Text));
    }

    std::string txt = osc_type_shortnames[i];
    if (text_allcaps)
    {
        std::transform(txt.begin(), txt.end(), txt.begin(), ::toupper);
    }

    auto tr = getLocalBounds().translated(text_hoffset, text_voffset);
    g.drawText(txt, tr, text_align);
}

void OscillatorMenu::loadSnapshot(int type, TiXmlElement *e, int idx)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        auto sc = sge->current_scene;
        sge->oscilatorMenuIndex[sc][sge->current_osc[sc]] = idx;
    }
    osc->queue_type = type;
    osc->queue_xmldata = e;
}

void OscillatorMenu::mouseDown(const juce::MouseEvent &event)
{
    menu.showMenuAsync(juce::PopupMenu::Options());
    isHovered = false;
    repaint();
}

void OscillatorMenu::mouseWheelMove(const juce::MouseEvent &event,
                                    const juce::MouseWheelDetails &wheel)
{
    int dir = wheelAccumulationHelper.accumulate(wheel, false, true);
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge && dir != 0)
    {
        auto sc = sge->current_scene;
        int currentIdx = sge->oscilatorMenuIndex[sc][sge->current_osc[sc]];
        if (dir < 0)
        {
            currentIdx = currentIdx + 1;
            if (currentIdx >= maxIdx)
                currentIdx = 0;
            auto args = loadArgsByIndex[currentIdx];
            if (args.actionType == XMLMenuPopulator::LoadArg::XML)
            {
                loadSnapshot(args.type, args.el, currentIdx);
            }
            else
            {
                jassert(false);
            }
            repaint();
        }
        else if (dir > 0)
        {
            currentIdx = currentIdx - 1;
            if (currentIdx < 0)
                currentIdx = maxIdx - 1;
            auto args = loadArgsByIndex[currentIdx];
            if (args.actionType == XMLMenuPopulator::LoadArg::XML)
            {
                loadSnapshot(args.type, args.el, currentIdx);
            }
            else
            {
                jassert(false);
            }
            repaint();
        }
    }
}
void OscillatorMenu::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    repaint();
}

void OscillatorMenu::mouseExit(const juce::MouseEvent &event)
{
    isHovered = false;
    repaint();
}

FxMenu::FxMenu() { strcpy(mtype, "fx"); }

void FxMenu::paint(juce::Graphics &g)
{
    if (bg)
        bg->draw(g, 1.0);
    if (isHovered && bgHover)
        bgHover->draw(g, 1.0);

    auto fgc = skin->getColor(Colors::Effect::Menu::Text);
    if (isHovered)
        fgc = skin->getColor(Colors::Effect::Menu::TextHover);

    g.setFont(Surge::GUI::getFontManager()->displayFont);
    g.setColour(fgc);
    auto r = getLocalBounds().reduced(2).withTrimmedLeft(4).withTrimmedRight(12);
    g.drawText(fxslot_names[current_fx], r, juce::Justification::centredLeft);
    g.drawText(fx_type_names[fx->type.val.i], r, juce::Justification::centredRight);
}

void FxMenu::mouseDown(const juce::MouseEvent &event)
{
    menu.showMenuAsync(juce::PopupMenu::Options());
    isHovered = false;
    repaint();
}

void FxMenu::mouseEnter(const juce::MouseEvent &event)
{
    isHovered = true;
    repaint();
}

void FxMenu::mouseExit(const juce::MouseEvent &event)
{
    isHovered = false;
    repaint();
}

void FxMenu::loadSnapshot(int type, TiXmlElement *e, int idx)
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

void FxMenu::populate()
{
    /*
    ** Are there user presets
    */
    Surge::FxUserPreset::forcePresetRescan(storage);

    XMLMenuPopulator::populate();

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
        auto *sge = firstListenerOfType<SurgeGUIEditor>();
        if (sge)
            sge->queueRebuildUI();
    };
    menu.addItem(Surge::GUI::toOSCaseForMenu("Refresh FX Preset List"), rsA);
}

Surge::FxClipboard::Clipboard FxMenu::fxClipboard;

void FxMenu::copyFX()
{
    Surge::FxClipboard::copyFx(storage, fx, fxClipboard);
    memcpy((void *)fxbuffer, (void *)fx, sizeof(FxStorage));
}

void FxMenu::pasteFX()
{
    Surge::FxClipboard::pasteFx(storage, fxbuffer, fxClipboard);

    selectedName = std::string("Copied ") + fx_type_names[fxbuffer->type.val.i];

    notifyValueChanged();
}

void FxMenu::saveFX()
{
    auto *sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->promptForMiniEdit("", "Enter a name for the FX preset:", "Save FX Preset",
                               VSTGUI::CPoint(-1, -1), [this](const std::string &s) {
                                   Surge::FxUserPreset::saveFxIn(this->storage, fx, s);
                                   auto *sge = firstListenerOfType<SurgeGUIEditor>();
                                   if (sge)
                                       sge->queueRebuildUI();
                               });
    }
}

void FxMenu::loadUserPreset(const Surge::FxUserPreset::Preset &p)
{
    Surge::FxUserPreset::loadPresetOnto(p, storage, fxbuffer);

    selectedIdx = -1;
    selectedName = p.name;

    notifyValueChanged();
}

void FxMenu::addToTopLevelTypeMenu(TiXmlElement *type, juce::PopupMenu &subMenu, int &idx)
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
void FxMenu::setMenuStartHeader(TiXmlElement *type, juce::PopupMenu &subMenu)
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

} // namespace Widgets
} // namespace Surge