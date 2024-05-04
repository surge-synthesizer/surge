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

#include "XMLConfiguredMenus.h"
#include "SurgeStorage.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"
#include "SurgeImage.h"
#include "AccessibleHelpers.h"
#include "MenuCustomComponents.h"
#include "widgets/EffectChooser.h"

namespace Surge
{
namespace Widgets
{

void XMLMenuPopulator::scanXMLPresets()
{
    TiXmlElement *sect = storage->getSnapshotSection(mtype);

    if (sect)
    {
        TiXmlElement *type = sect->FirstChildElement();

        while (type)
        {
            if (type->Value() && strcmp(type->Value(), "type") == 0)
            {
                scanXMLPresetForType(type, std::vector<std::string>());
            }
            else if (type->Value() && strcmp(type->Value(), "sectionheader") == 0)
            {
                Item sec;
                sec.isSectionHeader = true;
                if (type->Attribute("label"))
                    sec.name = type->Attribute("label");
                else
                    sec.name = "UNLABELED SECTION";
                int b = 0;
                if (type->QueryIntAttribute("columnbreak", &b) != TIXML_SUCCESS)
                    b = 0;
                sec.hasColumnBreak = b;
                allPresets.push_back(sec);
            }
            else if (type->Value() && strcmp(type->Value(), "separator") == 0)
            {
                Item sep;
                sep.isSeparator = true;
                allPresets.push_back(sep);
            }

            type = type->NextSiblingElement();
        }
    }
}

void XMLMenuPopulator::scanXMLPresetForType(TiXmlElement *type,
                                            const std::vector<std::string> &path)
{
    if (!(type->Value() && strcmp(type->Value(), "type") == 0))
        return;

    auto n = type->Attribute("name");
    if (!n)
        return;
    /*
     * OK we have two cases. If it is a type on its own then it is an entry in path.
     * If it has children, then the children are snapshots and are entries in my named path
     */

    if (type->NoChildren())
    {
        Item it;
        it.xmlElement = type;
        it.pathElements = path;
        it.name = n;
        int t = 0;
        if (type->QueryIntAttribute("i", &t) == TIXML_SUCCESS)
            it.itemType = t;
        allPresets.push_back(it);
    }
    else
    {
        auto p = path;
        p.push_back(n);
        int t = 0;
        if (type->QueryIntAttribute("i", &t) != TIXML_SUCCESS)
        {
            // ERROR
            jassert(false);
            std::cout << "INTERNAL MENU ERROR" << std::endl;
            return;
        }
        auto kid = type->FirstChildElement();
        while (kid)
        {
            std::string kval = kid->Value();
            if (kval == "type")
            {
                scanXMLPresetForType(kid, p);
            }
            else if (kval == "snapshot")
            {
                Item it;
                it.xmlElement = kid;
                it.pathElements = p;
                it.itemType = t;
                auto kn = kid->Attribute("name");
                it.name = kn ? kn : "-ERROR-";
                allPresets.push_back(it);
            }
            else
            {
                std::cout << "Wuh? " << kval << std::endl;
            }
            kid = kid->NextSiblingElement();
        }
    }
}

void XMLMenuPopulator::populate()
{
    /*
     * New scanners
     */
    allPresets.clear();
    scanXMLPresets();
    scanExtraPresets();

    struct Tree
    {
        enum
        {
            SEP,
            SECHEAD,
            FACPS,
            USPS,
            FOLD
        } type;

        ~Tree()
        {
            for (auto c : children)
                delete c;
        }

        std::string name;
        std::vector<std::string> fullPath;
        int idx = -1;
        int depth = 0;
        Tree *parent{nullptr};
        std::vector<Tree *> children;
        bool hasUser{false};
        bool hasFac{false};
        bool colBreak{false};

        void addByPath(const Item &i, int idx, int depth = 0)
        {
            if (i.pathElements.size() == depth)
            {
                auto t = new Tree();

                t->name = i.name;
                t->idx = idx;
                t->fullPath = i.pathElements;
                t->parent = this;
                t->depth = depth;
                t->type =
                    i.isSeparator ? SEP : (i.isSectionHeader ? SECHEAD : (i.isUser ? USPS : FACPS));
                t->colBreak = i.hasColumnBreak;

                children.push_back(t);
            }
            else
            {
                Tree *addToThis = nullptr;

                for (auto c : children)
                {
                    if (c->type == FOLD && c->name == i.pathElements[depth])
                    {
                        addToThis = c;
                    }
                }

                if (!addToThis)
                {
                    addToThis = new Tree();
                    addToThis->name = i.pathElements[depth];
                    addToThis->fullPath = std::vector<std::string>(
                        i.pathElements.begin(), i.pathElements.begin() + depth + 1);
                    addToThis->type = FOLD;
                    addToThis->parent = this;
                    addToThis->depth = depth + 1;

                    children.push_back(addToThis);
                }

                addToThis->addByPath(i, idx, depth + 1);
            }
        }

        void updateFacUserFlag()
        {
            hasFac = false;
            hasUser = false;

            for (auto c : children)
            {
                switch (c->type)
                {
                case FOLD:
                {
                    c->updateFacUserFlag();
                    hasFac |= c->hasFac;
                    hasUser |= c->hasUser;
                }
                break;
                case FACPS:
                    hasFac = true;
                    break;
                case USPS:
                    hasUser = true;
                    break;
                case SEP:
                case SECHEAD:
                    break;
                }
            }
        }

        void buildJuceMenu(juce::PopupMenu &m, XMLMenuPopulator *host)
        {
            bool inFac = true;

            if (depth == 1 && hasFac && hasUser)
            {
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(m,
                                                                                "FACTORY PRESETS");
            }

            for (auto c : children)
            {
                // std::cout << c->type << " " << c->name << std::endl;
                switch (c->type)
                {
                case FACPS:
                {
                    auto idx = c->idx;
                    m.addItem(c->name, [host, idx, n = c->name]() { host->loadByIndex(n, idx); });
                }
                break;
                case USPS:
                {
                    if (inFac && depth == 1 && hasFac && hasUser)
                    {
                        inFac = false;
                        m.addColumnBreak();
                        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                            m, "USER PRESETS");
                    }
                    auto idx = c->idx;
                    m.addItem(c->name, [host, n = c->name, idx]() { host->loadByIndex(n, idx); });
                }
                break;
                case SEP:
                {
                    m.addSeparator();
                }
                break;
                case SECHEAD:
                {
                    if (c->colBreak)
                        m.addColumnBreak();
                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(m, c->name);
                }
                break;
                case FOLD:
                {
                    auto subM = juce::PopupMenu();
                    c->buildJuceMenu(subM, host);
                    m.addSubMenu(c->name, subM);
                }
                break;
                }
            }
        }
    };

    auto rootTree = std::make_unique<Tree>();
    int idx = 0;

    for (const auto &p : allPresets)
    {
        rootTree->addByPath(p, idx);
        idx++;
    }

    rootTree->updateFacUserFlag();
    menu = juce::PopupMenu();

    rootTree->buildJuceMenu(menu, this);

    maxIdx = allPresets.size();
}

OscillatorMenu::OscillatorMenu() : juce::Component(), WidgetBaseMixin<OscillatorMenu>(this)
{
    selectedIdx = -1;
    strcpy(mtype, "osc");
    setDescription("Oscillator Type");
    setTitle("Oscillator Type");
}

OscillatorMenu::~OscillatorMenu() = default;

void OscillatorMenu::paint(juce::Graphics &g)
{
    bg->draw(g, 1.0);
    if (isHovered && bgHover)
        bgHover->draw(g, 1.0);

    int i = osc->type.val.i;

    g.setFont(skin->fontManager->getLatoAtSize(font_size, font_style));

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
        sge->undoManager()->pushOscillator(sc, sge->current_osc[sc]);

        auto announce = std::string("Oscillator Type is ") + osc_type_names[type];
        sge->enqueueAccessibleAnnouncement(announce);
    }
    osc->queue_type = type;
    osc->queue_xmldata = e;
}

void OscillatorMenu::setOscillatorStorage(OscillatorStorage *o)
{
    osc = o;

    if (selectedIdx == -1)
    {
        int idx = 0;

        for (auto pr : allPresets)
        {
            if (pr.itemType == osc->type.val.i && selectedIdx < 0)
            {
                selectedIdx = idx;
            }

            idx++;
        }

        if (selectedIdx < 0)
        {
            selectedIdx = 0;
        }
    }
}

void OscillatorMenu::populate()
{
    XMLMenuPopulator::populate();

    menu.addSeparator();

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        auto hu = sge->helpURLForSpecial("osc-select");
        auto lurl = hu;

        if (hu != "")
        {
            lurl = sge->fullyResolvedHelpURL(hu);
        }

        auto sc = sge->current_scene;
        auto osc = sge->current_osc[sc];

        std::string title = fmt::format("Osc {} Type", osc + 1);

        auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(title, lurl);
        hmen->setSkin(skin, associatedBitmapStore);
        hmen->setCentered(false);

        menu.addCustomItem(-1, std::move(hmen), nullptr, title);

        if (storage->oscReceiving)
        {
            menu.addSeparator();

            auto oscName = storage->getPatch().scene[sc].osc[osc].type.get_osc_name();

            auto i =
                juce::PopupMenu::Item(fmt::format("OSC: {}", oscName))
                    .setEnabled(true)
                    .setAction([oscName]() { juce::SystemClipboard::copyTextToClipboard(oscName); })
                    .setColour(
                        sge->currentSkin->getColor(Colors::PopupMenu::Text).withAlpha(0.75f));

            menu.addItem(i);
        }
    }
}

void OscillatorMenu::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    populate();

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    stuckHoverOn();
    menu.showMenuAsync(sge->popupMenuOptions(this),
                       Surge::GUI::makeAsyncCallback<OscillatorMenu>(this, [](auto *that, int) {
                           that->stuckHoverOff();
                           that->isHovered = false;
                           that->repaint();
                       }));
}

void OscillatorMenu::mouseWheelMove(const juce::MouseEvent &event,
                                    const juce::MouseWheelDetails &wheel)
{
    int dir = wheelAccumulationHelper.accumulate(wheel, false, true);

    if (dir != 0)
    {
        jogBy(-dir);
    }
}
void OscillatorMenu::mouseEnter(const juce::MouseEvent &event) { startHover(event.position); }

void OscillatorMenu::mouseExit(const juce::MouseEvent &event) { endHover(); }

bool OscillatorMenu::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu || action == Return)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        menu.showMenuAsync(sge->popupMenuOptions());
        return true;
    }

    switch (action)
    {
    case Increase:
        jogBy(+1);
        return true;
        break;
    case Decrease:
        jogBy(-1);
        return true;
        break;
    default:
        return false; // but bail out if it does
        break;
    }

    return false;
}

template <typename T> struct XMLValue
{
    static std::string value(T *comp) { return "ERROR"; }
};

template <> struct XMLValue<OscillatorMenu>
{
    static std::string value(OscillatorMenu *comp)
    {
        int ids = comp->osc->type.val.i;
        return osc_type_names[ids];
    }
};

template <typename T> struct XMLMenuAH : public juce::AccessibilityHandler
{
    struct XMLMenuTextValue : public juce::AccessibilityTextValueInterface
    {
        XMLMenuTextValue(T *c) : comp(c) {}
        T *comp;

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override { return XMLValue<T>::value(comp); }
        void setValueAsString(const juce::String &newValue) override{};
    };

    explicit XMLMenuAH(T *s)
        : comp(s),
          juce::AccessibilityHandler(
              *s, juce::AccessibilityRole::button,
              juce::AccessibilityActions()
                  .addAction(juce::AccessibilityActionType::showMenu,
                             [this]() { this->showMenu(); })
                  .addAction(juce::AccessibilityActionType::press, [this]() { this->showMenu(); }),
              AccessibilityHandler::Interfaces{std::make_unique<XMLMenuTextValue>(s)})

    {
    }
    void showMenu() { comp->menu.showMenuAsync(juce::PopupMenu::Options()); }

    T *comp;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XMLMenuAH);
};
std::unique_ptr<juce::AccessibilityHandler> OscillatorMenu::createAccessibilityHandler()
{
    return std::make_unique<XMLMenuAH<OscillatorMenu>>(this);
}

FxMenu::FxMenu() : juce::Component(), WidgetBaseMixin<FxMenu>(this)
{
    selectedIdx = -1;
    strcpy(mtype, "fx");
    setDescription("FX Type");
    setTitle("FX Type");
}

void FxMenu::paint(juce::Graphics &g)
{
    if (bg)
        bg->draw(g, 1.0);
    if (isHovered && bgHover)
        bgHover->draw(g, 1.0);

    auto fgc = skin->getColor(Colors::Effect::Menu::Text);
    if (isHovered)
        fgc = skin->getColor(Colors::Effect::Menu::TextHover);

    g.setFont(skin->fontManager->displayFont);
    g.setColour(fgc);
    auto r = getLocalBounds().reduced(2).withTrimmedLeft(4).withTrimmedRight(12);
    g.drawText(fxslot_names[current_fx], r, juce::Justification::centredLeft);
    g.drawText(fx_type_shortnames[fx->type.val.i], r, juce::Justification::centredRight);
}

void FxMenu::setFxStorage(FxStorage *s)
{
    fx = s;

    if (selectedIdx == -1)
    {
        int idx = 0;

        for (auto pr : allPresets)
        {
            if (pr.itemType == fx->type.val.i && selectedIdx < 0)
            {
                selectedIdx = idx;
            }

            idx++;
        }

        if (selectedIdx < 0)
        {
            selectedIdx = 0;
        }
    }
}

void FxMenu::mouseDown(const juce::MouseEvent &event)
{
    if (forwardedMainFrameMouseDowns(event))
    {
        return;
    }

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    populate();

    stuckHoverOn();
    menu.showMenuAsync(sge->popupMenuOptions(this),
                       Surge::GUI::makeAsyncCallback<FxMenu>(this, [](auto *that, int) {
                           that->stuckHoverOff();
                           that->isHovered = false;
                           that->repaint();
                       }));
}

void FxMenu::mouseEnter(const juce::MouseEvent &event) { startHover(event.position); }

void FxMenu::mouseExit(const juce::MouseEvent &event) { endHover(); }

void FxMenu::loadSnapshot(int type, TiXmlElement *e, int idx)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->undoManager()->pushFX(current_fx);
    }
    if (type > -1)
    {
        fxbuffer->type.val.i = type;

        Effect *t_fx = spawn_effect(type, storage, fxbuffer, 0);
        if (t_fx)
        {
            t_fx->init_ctrltypes();
            t_fx->init_default_values();
            delete t_fx;
        }

        if (e)
        {
            // storage->patch.update_controls();
            selectedName = e->Attribute("name");

            for (int i = 0; i < n_fx_params; i++)
            {
                double d;
                int j;
                std::string lbl;

                lbl = fmt::format("p{:d}", i);

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

                lbl = fmt::format("p{:d}_temposync", i);
                fxbuffer->p[i].temposync =
                    ((e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS) && (j == 1));

                lbl = fmt::format("p{:d}_extend_range", i);
                fxbuffer->p[i].set_extend_range(
                    ((e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS) && (j == 1)));

                lbl = fmt::format("p{:d}_deactivated", i);
                fxbuffer->p[i].deactivated =
                    ((e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS) && (j == 1));

                lbl = fmt::format("p{:d}_deform_type", i);

                if (e->QueryIntAttribute(lbl, &j) == TIXML_SUCCESS)
                    fxbuffer->p[i].deform_type = j;
            }
        }
        else
        {
            selectedName = "Off";
        }
    }
}

void FxMenu::populateForContext(bool isCalledInEffectChooser)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    // Are there any user presets?
    storage->fxUserPreset->doPresetRescan(storage);

    XMLMenuPopulator::populate();

    auto cfxid = -1;
    auto cfxtype = 0;
    bool addDeact = false;
    bool isDeact = false;
    bool enableClear = true;

    if (sge)
    {
        cfxid = sge->effectChooser->currentClicked;

        if (cfxid >= 0)
        {
            auto deactbm = sge->effectChooser->getDeactivatedBitmask();

            addDeact = true;
            isDeact = deactbm & (1 << cfxid);
            cfxtype = sge->effectChooser->fxTypes[cfxid];
            enableClear = cfxtype != fxt_off;
        }
    }

    auto cfx = std::string{"Current FX Slot"};
    auto helpMenuText = std::string{"FX Presets"};
    auto helpMenuScreeReaderText = helpMenuText;

    if (cfxid >= 0 && cfxid < n_fx_slots)
    {
        if (isCalledInEffectChooser)
        {
            cfx = fxslot_longnames[cfxid];
            helpMenuText = cfx;
        }

        helpMenuScreeReaderText =
            fmt::format("FX Presets: {} {}", fxslot_longnames[cfxid], fx_type_names[cfxtype]);
    }

    menu.addColumnBreak();
    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(menu, "FUNCTIONS");

    if (addDeact)
    {
        menu.addItem(
            Surge::GUI::toOSCase(fmt::format("{} {}", (isDeact ? "Activate" : "Deactivate"), cfx)),
            [that = juce::Component::SafePointer(sge->effectChooser.get())]() {
                that->toggleSelectedDeactivation();
                that->repaint();
            });
    }

    menu.addItem(Surge::GUI::toOSCase(fmt::format("Clear {}", cfx)), enableClear, false,
                 [this, cfxid, that = juce::Component::SafePointer(sge->effectChooser.get())]() {
                     that->setEffectSlotDeactivation(cfxid, false);
                     that->repaint();
                     loadSnapshot(fxt_off, nullptr, 0);
                     if (getControlListener())
                     {
                         getControlListener()->valueChanged(asControlValueInterface());
                     }
                     repaint();
                 });

    if (sge)
    {
        auto initSubmenu = juce::PopupMenu();

        initSubmenu.addItem(Surge::GUI::toOSCase("Clear Scene A Insert FX Chain"), true, false,
                            [this, sge]() { sge->enqueueFXChainClear(0); });

        initSubmenu.addItem(Surge::GUI::toOSCase("Clear Scene B Insert FX Chain"), true, false,
                            [this, sge]() { sge->enqueueFXChainClear(1); });

        initSubmenu.addItem(Surge::GUI::toOSCase("Clear Send FX Chain"), true, false,
                            [this, sge]() { sge->enqueueFXChainClear(2); });

        initSubmenu.addItem(Surge::GUI::toOSCase("Clear Global FX Chain"), true, false,
                            [this, sge]() { sge->enqueueFXChainClear(3); });

        initSubmenu.addItem(Surge::GUI::toOSCase("Clear All FX Chains"), true, false,
                            [this, sge]() { sge->enqueueFXChainClear(-1); });

        menu.addSubMenu(Surge::GUI::toOSCase("Clear Chains"), initSubmenu);
    }

    menu.addSeparator();

    auto rsA = [this, sge]() {
        this->storage->fxUserPreset->doPresetRescan(this->storage, true);

        if (sge)
        {
            sge->queueRebuildUI();
        }
    };

    menu.addItem(Surge::GUI::toOSCase("Refresh FX Preset List"), rsA);
    if (fx->type.val.i != fxt_off)
    {
        menu.addItem(Surge::GUI::toOSCase("Save FX Preset As..."), [this]() { this->saveFX(); });
    }

    menu.addSeparator();

    menu.addItem(Surge::GUI::toOSCase("Copy FX Preset"), [this]() { this->copyFX(); });
    if (Surge::FxClipboard::isPasteAvailable(fxClipboard))
    {
        menu.addItem(Surge::GUI::toOSCase("Paste FX Preset"), [this]() {
            this->pasteFX();
            this->storage->getPatch().isDirty = true;
        });
    }

    menu.addSeparator();

    if (sge)
    {
        auto hu = sge->helpURLForSpecial("fx-presets");
        auto lurl = hu;

        if (hu != "")
        {
            lurl = sge->fullyResolvedHelpURL(hu);
        }

        auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
            helpMenuText, helpMenuScreeReaderText, lurl);
        hmen->setSkin(skin, associatedBitmapStore);
        hmen->setCentered(false);

        menu.addCustomItem(-1, std::move(hmen), nullptr, helpMenuScreeReaderText);

        if (storage->oscReceiving)
        {
            menu.addSeparator();

            auto oscName = storage->getPatch().fx[sge->current_fx].type.get_osc_name();

            auto i =
                juce::PopupMenu::Item(fmt::format("OSC: {}", oscName))
                    .setEnabled(true)
                    .setAction([oscName]() { juce::SystemClipboard::copyTextToClipboard(oscName); })
                    .setColour(
                        sge->currentSkin->getColor(Colors::PopupMenu::Text).withAlpha(0.75f));

            menu.addItem(i);
        }
    }
}

Surge::FxClipboard::Clipboard FxMenu::fxClipboard;

void FxMenu::copyFX()
{
    Surge::FxClipboard::copyFx(storage, fx, fxClipboard);
    *fxbuffer = *fx;
}

void FxMenu::pasteFX()
{
    Surge::FxClipboard::pasteFx(storage, fxbuffer, fxClipboard);

    selectedName = std::string("Copied ") + fx_type_shortnames[fxbuffer->type.val.i];

    notifyValueChanged();
}

void FxMenu::saveFX()
{
    auto *sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->promptForMiniEdit(
            "", "Enter the preset name:", "Save FX Preset", juce::Point<int>{},
            [this](const std::string &s) {
                this->storage->fxUserPreset->saveFxIn(this->storage, fx, s);
                auto *sge = firstListenerOfType<SurgeGUIEditor>();
                if (sge)
                    sge->queueRebuildUI();
            },
            this);
    }
}

void FxMenu::loadUserPreset(const Surge::Storage::FxUserPreset::Preset &p)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        sge->undoManager()->pushFX(current_fx);
    }

    this->storage->fxUserPreset->loadPresetOnto(p, storage, fxbuffer);

    selectedIdx = -1;
    selectedName = p.name;

    notifyValueChanged();
}

void FxMenu::scanExtraPresets()
{
    try
    {
        storage->fxUserPreset->doPresetRescan(storage);
        for (const auto &tp : storage->fxUserPreset->getPresetsByType())
        {
            // So let's run all presets until we find the first item with type tp.first
            auto alit = allPresets.begin();
            while (alit->itemType != tp.first && alit != allPresets.end())
                alit++;
            std::vector<std::string> rootPath;
            rootPath.push_back(alit->pathElements[0]);

            while (alit != allPresets.end() && alit->itemType == tp.first)
                alit++;

            // OK so alit now points at the end of the list
            alit = allPresets.insert(alit, tp.second.size(), Item());

            // OK so insert 'size' into all presets at the end of the position of the type

            for (const auto ps : tp.second)
            {
                alit->itemType = tp.first;
                alit->name = ps.name;
                alit->isUser = !ps.isFactory;
                alit->path = string_to_path(ps.file);
                auto thisPath = rootPath;
                for (const auto &q : ps.subPath)
                    thisPath.push_back(path_to_string(q));

                alit->pathElements = thisPath;
                alit->hasFxUserPreset = true;
                alit->fxPreset = ps;
                alit++;
            }
        }
    }
    catch (const fs::filesystem_error &e)
    {
        // really nothing to do about this other than continue without the preset
        std::ostringstream oss;
        oss << "Experienced file system error when scanning user FX. " << e.what();

        if (storage)
            storage->reportError(oss.str(), "FileSystem Error");
    }
}

void FxMenu::loadByIndex(const std::string &name, int index)
{
    auto q = allPresets[index];
    if (q.xmlElement)
    {
        loadSnapshot(q.itemType, q.xmlElement, index);
    }
    else
    {
        loadUserPreset(q.fxPreset);
    }
    selectedIdx = index;
    if (getControlListener())
        getControlListener()->valueChanged(asControlValueInterface());
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (sge)
    {
        auto announce = std::string("Loaded FX Preset  ") + name;
        sge->enqueueAccessibleAnnouncement(announce);
    }
    repaint();
}

void FxMenu::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    int dir = wheelAccumulationHelper.accumulate(wheel, false, true);

    if (dir != 0)
    {
        jogBy(-dir);
    }
}

bool FxMenu::keyPressed(const juce::KeyPress &key)
{
    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == None)
        return false;

    if (action == OpenMenu || action == Return)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        menu.showMenuAsync(sge->popupMenuOptions(this));
        return true;
    }

    switch (action)
    {
    case Increase:
        jogBy(+1);
        return true;
        break;
    case Decrease:
        jogBy(-1);
        return true;
        break;
    default:
        return false; // but bail out if it does
        break;
    }

    return false;
}

template <> struct XMLValue<FxMenu>
{
    static std::string value(FxMenu *comp)
    {
        std::string r = fx_type_shortnames[comp->fx->type.val.i];
        r += " in ";
        r += fxslot_names[comp->current_fx];
        return r;
    }
};

std::unique_ptr<juce::AccessibilityHandler> FxMenu::createAccessibilityHandler()
{
    return std::make_unique<XMLMenuAH<FxMenu>>(this);
}

} // namespace Widgets
} // namespace Surge
