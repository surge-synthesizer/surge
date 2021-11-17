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

#include "ModulationEditor.h"
#include "SurgeSynthesizer.h"
#include "SurgeGUIEditor.h"
#include "SurgeGUIUtils.h"
#include "RuntimeFont.h"
#include <sstream>
#include "widgets/MenuCustomComponents.h"
#include "widgets/ModulatableSlider.h"
#include "widgets/MultiSwitch.h"
#include "SurgeJUCEHelpers.h"

namespace Surge
{
namespace Overlays
{

struct ModulationSideControls : public juce::Component,
                                public Surge::GUI::IComponentTagValue::Listener,
                                public Surge::GUI::SkinConsumingComponent
{
    enum Tags
    {
        tag_sort_by = 0x147932,
        tag_filter_by,
        tag_add_source,
        tag_add_target,
        tag_add_go,
        tag_value_disp
    };

    ModulationEditor *editor{nullptr};
    ModulationSideControls(ModulationEditor *e) : editor(e) { create(); }
    void create()
    {
        auto makeL = [this](const std::string &s) {
            auto l = std::make_unique<juce::Label>(s);
            l->setText(s, juce::dontSendNotification);
            l->setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            if (skin)
                l->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            addAndMakeVisible(*l);
            return l;
        };

        auto makeW = [this](const std::vector<std::string> &l, int tag, bool en,
                            bool vert = false) {
            auto w = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            if (vert)
            {
                w->setRows(l.size());
                w->setColumns(1);
            }
            else
            {
                w->setRows(1);
                w->setColumns(l.size());
            }
            w->setTag(tag);
            w->setLabels(l);
            w->setEnabled(en);
            w->addListener(this);
            addAndMakeVisible(*w);
            return w;
        };

        sortL = makeL("Sort By");
        sortW = makeW(
            {
                "Source",
                "Target",
            },
            tag_sort_by, true);
        filterL = makeL("Filter By");
        filterW = makeW({"-"}, tag_filter_by, true);

        addL = makeL("Add Modulation");
        addSourceW = makeW({"Select Source"}, tag_add_source, true);
        addTargetW = makeW({"Select Target"}, tag_add_target, false);
        addGoW = makeW({"Add Modulation"}, tag_add_go, false);

        dispL = makeL("Value Display");
        dispW = makeW({"None", "Depth Only", "Value and Depth", "All"}, tag_value_disp, true, true);

        auto dwv = Surge::Storage::getUserDefaultValue(&(editor->synth->storage),
                                                       Storage::ModulationEditorValueDisplay, 3);
        dispW->setValue(dwv / 3.0);
        valueChanged(dispW.get());
    }

    void resized() override
    {
        float ypos = 4;
        float xpos = 4;

        auto h = 16.0;
        auto m = 3;

        auto b = juce::Rectangle<int>(xpos, ypos, getWidth() - 2 * xpos, h);
        sortL->setBounds(b);
        b = b.translated(0, h + m);
        sortW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        filterL->setBounds(b);
        b = b.translated(0, h + m);
        filterW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        addL->setBounds(b);
        b = b.translated(0, h + m);
        addSourceW->setBounds(b);
        b = b.translated(0, h + m);
        addTargetW->setBounds(b);
        b = b.translated(0, h + m);
        addGoW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        dispL->setBounds(b);
        b = b.translated(0, h + m);
        dispW->setBounds(b.withHeight(h * 4));
    }

    void paint(juce::Graphics &g) override
    {
        if (!skin)
        {
            g.fillAll(juce::Colours::red);
            return;
        }
        g.fillAll(skin->getColor(Colors::MSEGEditor::Panel));
    }

    void valueChanged(GUI::IComponentTagValue *c) override;

    void onSkinChanged() override
    {
        for (auto c : getChildren())
        {
            auto skc = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(c);
            if (skc)
                skc->setSkin(skin, associatedBitmapStore);
        }

        if (sortL && skin)
        {
            sortL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            filterL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            addL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            dispL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        }
    }

    std::unique_ptr<juce::Label> sortL, filterL, addL, dispL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> sortW, filterW, addSourceW, addTargetW,
        addGoW, dispW;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationSideControls);
};

// TODO: Skin Colors
struct ModulationListContents : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    ModulationEditor *editor{nullptr};
    ModulationListContents(ModulationEditor *e) : editor(e) {}

    void paint(juce::Graphics &g) override
    {
        g.fillAll(skin->getColor(Colors::MSEGEditor::Background));
    }

    struct Datum
    {
        int source_scene, source_id, source_index, destination_id, inScene;
        std::string pname, sname, moddepth;
        bool isBipolar;
        bool isPerScene;
        int idBase;
        float moddepth01;
        bool isMuted;
        ModulationDisplayInfoWindowStrings mss;
    };

    enum ValueDisplay
    {
        NOMOD = 0,
        MOD_ONLY = 1,
        CTR = 2,
        EXTRAS = 4,

        CTR_PLUS_MOD = MOD_ONLY | CTR,
        ALL = CTR_PLUS_MOD | EXTRAS
    } valueDisplay = ALL;

    struct DataRowEditor : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public Surge::GUI::IComponentTagValue::Listener
    {
        static constexpr int height = 32;
        Datum datum;
        ModulationListContents *contents{nullptr};
        DataRowEditor(const Datum &d, ModulationListContents *c) : datum(d), contents(c)
        {
            clearButton = std::make_unique<Surge::Widgets::TinyLittleIconButton>(1, [this]() {
                auto me = contents->editor;
                ModulationEditor::SelfModulationGuard g(me);
                me->synth->clearModulation(datum.destination_id + datum.idBase,
                                           (modsources)datum.source_id, datum.source_scene,
                                           datum.source_index);
                // The rebuild may delete this so defer
                auto c = contents;
                juce::Timer::callAfterDelay(1, [c, me]() {
                    c->rebuildFrom(me->synth);
                    me->ed->queue_refresh = true;
                });
            });
            addAndMakeVisible(*clearButton);

            muted = d.isMuted;
            muteButton = std::make_unique<Surge::Widgets::TinyLittleIconButton>(2, [this]() {
                auto me = contents->editor;
                ModulationEditor::SelfModulationGuard g(me);
                me->synth->muteModulation(datum.destination_id + datum.idBase,
                                          (modsources)datum.source_id, datum.source_scene,
                                          datum.source_index, !muted);
                muted = !muted;
                contents->rebuildFrom(me->synth);
            });

            addAndMakeVisible(*muteButton);

            surgeLikeSlider = std::make_unique<Surge::Widgets::ModulatableSlider>();
            surgeLikeSlider->setOrientation(Surge::ParamConfig::kHorizontal);
            surgeLikeSlider->setBipolarFn([]() { return true; });
            surgeLikeSlider->setIsLightStyle(true);
            surgeLikeSlider->setStorage(&(contents->editor->synth->storage));
            surgeLikeSlider->setAlwaysUseModHandle(true);
            surgeLikeSlider->addListener(this);
            addAndMakeVisible(*surgeLikeSlider);

            resetValuesFromDatum();
        }

        void resetValuesFromDatum()
        {
            surgeLikeSlider->setValue((datum.moddepth01 + 1) * 0.5);
            surgeLikeSlider->setQuantitizedDisplayValue((datum.moddepth01 + 1) * 0.5);
            surgeLikeSlider->setIsModulationBipolar(datum.isBipolar);

            muteButton->offset = 2;
            if (datum.isMuted)
                muteButton->offset = 3;
            repaint();
        }

        bool firstInSort{false}, hasFollower{false};
        bool isTop{false}, isAfterTop{false};
        void paint(juce::Graphics &g) override
        {
            static constexpr int indent = 1;
            auto b = getLocalBounds().withTrimmedLeft(indent);

            g.setColour(juce::Colour(0xFF979797));

            if (firstInSort)
            {
                // draw the top and the side
                g.drawLine(indent, 0, getWidth(), 0, 1);
            }

            g.drawLine(indent, 0, indent, getHeight(), 1);
            g.drawLine(getWidth() - indent, 0, getWidth() - indent, getHeight(), 1);

            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.setColour(juce::Colours::white);

            int fh = g.getCurrentFont().getHeight();
            auto firstLab = datum.sname;
            auto secondLab = datum.pname;

            if (contents->sortOrder == BY_TARGET)
                std::swap(firstLab, secondLab);

            auto tb = b.reduced(3, 0).withHeight(fh).withRight(controlsStart);
            bool longArrow = true;

            if (firstInSort)
            {
                longArrow = false;
                g.setColour(juce::Colours::white);
                g.drawText(firstLab, tb, juce::Justification::topLeft);
            }
            g.setColour(juce::Colours::white);

            auto tbl = tb.getBottomLeft();
            if (!longArrow)
                tb = tb.translated(0, fh + 4); // fix this to be a bit lower
            else
                tb = tb.withY((getHeight() - fh) / 2.0);
            tb = tb.withTrimmedLeft(15);
            auto tbe = juce::Point<float>(tb.getX(), tb.getCentreY());

            auto xStart = tbl.x + 7.f;
            auto xEnd = tbe.x;
            auto yStart = longArrow ? 0 : tbl.y + 2.0f;
            auto yEnd = tbe.y;
            if (contents->sortOrder == BY_SOURCE)
            {
                g.drawLine(xStart, yStart, xStart, yEnd, 1);
                g.drawArrow({xStart, yEnd, xEnd, yEnd}, 1, 3, 4);
            }
            else
            {
                if (firstInSort)
                    g.drawArrow({xStart, yEnd, xStart, yStart}, 1, 3, 4);
                else
                    g.drawLine(xStart, yEnd, xStart, yStart, 1);
                g.drawLine(xStart, yEnd, xEnd, yEnd, 1);
            }

            if (hasFollower)
                g.drawLine(xStart, yEnd, xStart, getHeight(), 1);

            g.drawText(secondLab, tb.withTrimmedLeft(2), juce::Justification::centredLeft);

            if ((isTop || isAfterTop) && !firstInSort)
            {
                g.setColour(juce::Colours::grey);
                auto tf = g.getCurrentFont();
                g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(7));
                tb = tb.withTop(0);
                g.drawText(firstLab, tb.withTrimmedLeft(2), juce::Justification::topLeft);
                g.setFont(tf);
            }

            if (contents->valueDisplay == NOMOD)
                return;

            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            auto sb = surgeLikeSlider->getBounds();
            auto bb = sb.withY(0).withHeight(getHeight()).reduced(0, 1);

            auto shift = 60, over = 13;
            auto lb = bb.translated(-shift, 0).withWidth(shift + over -
                                                         3); // damn thing isn't very symmetric
            auto rb = bb.withX(bb.getX() + bb.getWidth() - over).withWidth(shift + over);
            auto cb = bb.withTrimmedLeft(over).withTrimmedRight(over);

            g.setColour(juce::Colours::white);
            if (contents->valueDisplay & CTR)
                g.drawFittedText(datum.mss.val, cb, juce::Justification::centredTop, 1, 0.1);
            if (contents->valueDisplay & MOD_ONLY)
                g.drawFittedText(datum.mss.dvalplus, rb, juce::Justification::topLeft, 1, 0.1);

            if ((contents->valueDisplay & EXTRAS) == 0)
                return;
            if (datum.isBipolar)
                g.drawFittedText(datum.mss.dvalminus, lb, juce::Justification::topRight, 1, 0.1);

            g.setColour(juce::Colours::grey);
            g.drawFittedText(datum.mss.valplus, rb, juce::Justification::bottomLeft, 1, 0.1);

            if (datum.isBipolar)
                g.drawFittedText(datum.mss.valminus, lb, juce::Justification::bottomRight, 1, 0.1);
        }

        static constexpr int controlsStart = 150;
        static constexpr int controlsEnd = controlsStart + (14 + 2) * 2 + 140 + 2;

        void resized() override
        {
            int startX = controlsStart;
            int bh = 12;
            int sh = 26;
            int bY = ((height - bh) / 2) - 1;
            int sY = (height - sh) / 2 + 2;
            clearButton->setBounds(startX, bY, bh, bh);
            startX += bh + 2;
            muteButton->setBounds(startX, bY, bh, bh);
            startX += bh + 70;
            surgeLikeSlider->setBounds(startX, sY, 140, sh);
        }

        void valueChanged(Surge::GUI::IComponentTagValue *v) override
        {
            if (v == surgeLikeSlider.get())
            {
                auto me = contents->editor;
                ModulationEditor::SelfModulationGuard g(me);

                auto nm01 = v->getValue() * 2.f - 1.f;
                auto synth = contents->editor->synth;

                auto p = synth->storage.getPatch().param_ptr[datum.destination_id + datum.idBase];

                synth->setModulation(datum.destination_id + datum.idBase,
                                     (modsources)datum.source_id, datum.source_scene,
                                     datum.source_index, nm01);

                contents->populateDatum(datum, synth);
                resetValuesFromDatum();
            }
        }

        void onSkinChanged() override
        {
            clearButton->icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
            muteButton->icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
            surgeLikeSlider->setSkin(skin, associatedBitmapStore);
        }

        bool muted{false};
        std::unique_ptr<Surge::Widgets::TinyLittleIconButton> clearButton;
        std::unique_ptr<Surge::Widgets::TinyLittleIconButton> muteButton;
        std::unique_ptr<Surge::Widgets::ModulatableSlider> surgeLikeSlider;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DataRowEditor);
    };

    void moved() override
    {
        auto yPos = getBounds().getY();
        auto cmp = getComponentAt(3, -yPos);

        for (auto &r : rows)
        {
            r->isTop = false;
            r->isAfterTop = false;
        }

        if (cmp)
        {
            auto dre = dynamic_cast<DataRowEditor *>(cmp);
            if (dre)
            {
                dre->isTop = true;
            }
        }

        bool prior = false;

        for (auto &r : rows)
        {
            r->isAfterTop = prior;
            prior = r->isTop;
        }

        repaint();
    }

    enum SortOrder
    {
        BY_SOURCE,
        BY_TARGET
    } sortOrder{BY_SOURCE};

    std::string filterString;

    enum FilterOn
    {
        NONE,
        SOURCE,
        TARGET
    } filterOn{NONE};

    void clearFilters()
    {
        if (filterOn != NONE)
        {
            filterOn = NONE;
            rebuildFrom(editor->synth);
        }
    }

    void filterBySource(const std::string &s)
    {
        filterOn = SOURCE;
        filterString = s;
        rebuildFrom(editor->synth);
    }

    void filterByTarget(const std::string &s)
    {
        filterOn = TARGET;
        filterString = s;
        rebuildFrom(editor->synth);
    }

    std::vector<std::unique_ptr<DataRowEditor>> rows;

    void populateDatum(Datum &d, const SurgeSynthesizer *synth)
    {
        std::string sceneMod = "";
        d.isPerScene = synth->isModulatorDistinctPerScene((modsources)d.source_id);

        if (synth->isModulatorDistinctPerScene((modsources)d.source_id) &&
            !isScenelevel((modsources)d.source_id))
        {
            sceneMod = std::string(" (") + (d.source_scene == 0 ? "A" : "B") + ")";
        }

        char nm[256];
        SurgeSynthesizer::ID ptagid;
        if (synth->fromSynthSideId(d.destination_id + d.idBase, ptagid))
            synth->getParameterName(ptagid, nm);

        std::string sname = editor->ed->modulatorNameWithIndex(
            d.source_scene, d.source_id, d.source_index, false, d.inScene < 0);

        d.sname = sname + sceneMod;
        d.pname = nm;

        char pdisp[256];
        auto p = synth->storage.getPatch().param_ptr[d.destination_id + d.idBase];
        int ptag = p->id;
        auto thisms = (modsources)d.source_id;

        d.moddepth01 = synth->getModulation(ptag, thisms, d.source_scene, d.source_index);
        d.isBipolar = synth->isBipolarModulation(thisms);
        d.isMuted = synth->isModulationMuted(ptag, thisms, d.source_scene, d.source_index);
        p->get_display_of_modulation_depth(
            pdisp, synth->getModulation(ptag, thisms, d.source_scene, d.source_index),
            synth->isBipolarModulation(thisms), Parameter::InfoWindow, &(d.mss));
        d.moddepth = pdisp;
    }

    void rebuildFrom(SurgeSynthesizer *synth)
    {
        std::vector<Datum> dataRows;

        removeAllChildren();
        rows.clear();
        int ypos = 0;
        setSize(getWidth(), 10);
        auto append = [this, synth, &dataRows](const std::string &type,
                                               const std::vector<ModulationRouting> &r, int idBase,
                                               int scene) {
            if (r.empty())
                return;

            for (auto q : r)
            {
                Datum d;
                d.source_scene = q.source_scene;
                d.source_id = q.source_id;
                d.source_index = q.source_index;
                d.destination_id = q.destination_id;
                d.idBase = idBase;
                d.inScene = scene;
                populateDatum(d, synth);
                dataRows.push_back(d);
            }
        };

        {
            std::lock_guard<std::recursive_mutex> modLock(synth->storage.modRoutingMutex);

            // FIXME mutex lock
            append("Global Modulators", synth->storage.getPatch().modulation_global, 0, -1);
            append("Scene A - Voice Modulators",
                   synth->storage.getPatch().scene[0].modulation_voice,
                   synth->storage.getPatch().scene_start[0], 0);
            append("Scene A - Scene Modulators",
                   synth->storage.getPatch().scene[0].modulation_scene,
                   synth->storage.getPatch().scene_start[0], 0);
            append("Scene B - Voice Modulators",
                   synth->storage.getPatch().scene[1].modulation_voice,
                   synth->storage.getPatch().scene_start[1], 1);
            append("Scene B - Scene Modulators",
                   synth->storage.getPatch().scene[1].modulation_scene,
                   synth->storage.getPatch().scene_start[1], 1);
        }

        std::sort(dataRows.begin(), dataRows.end(), [this](const Datum &a, const Datum &b) -> bool {
            if (sortOrder == BY_SOURCE)
            {
                if (a.isPerScene && b.isPerScene)
                    if (a.source_scene != b.source_scene)
                        return a.source_scene < b.source_scene;

                if (a.source_id != b.source_id)
                    return a.source_id < b.source_id;

                if (a.destination_id != b.destination_id)
                    return a.destination_id < b.destination_id;
            }
            else
            {
                if (a.destination_id != b.destination_id)
                    return a.destination_id < b.destination_id;

                if (a.isPerScene && b.isPerScene)
                    if (a.source_scene != b.source_scene)
                        return a.source_scene < b.source_scene;

                if (a.source_id != b.source_id)
                    return a.source_id < b.source_id;
            }

            return false;
        });

        std::string priorN = "-";

        for (const auto &d : dataRows)
        {
            auto l = std::make_unique<DataRowEditor>(d, this);
            auto sortName = sortOrder == BY_SOURCE ? d.sname : d.pname;

            l->setSkin(skin, associatedBitmapStore);

            if (sortName != priorN)
            {
                priorN = sortName;
                l->firstInSort = true;
            }

            if (filterOn == NONE || (filterOn == SOURCE && d.sname == filterString) ||
                (filterOn == TARGET && d.pname == filterString))
            {
                l->setBounds(0, ypos, getWidth(), DataRowEditor::height);
                ypos += DataRowEditor::height;
                addAndMakeVisible(*l);
            }

            l->hasFollower = false;
            rows.push_back(std::move(l));
        }

        // this is a bit gross but i can't think of a better way
        for (int i = 1; i < rows.size(); ++i)
        {
            auto sni = sortOrder == BY_SOURCE ? rows[i]->datum.sname : rows[i]->datum.pname;
            auto snm = sortOrder == BY_SOURCE ? rows[i - 1]->datum.sname : rows[i - 1]->datum.pname;
            if (sni == snm)
                rows[i - 1]->hasFollower = true;
        }
        setSize(getWidth(), ypos);
        moved(); // to refresh the 'istop'
    }

    void updateAllValues(const SurgeSynthesizer *synth)
    {
        for (const auto &r : rows)
        {
            populateDatum(r->datum, synth);
            r->resetValuesFromDatum();
        }
    }

    void onSkinChanged() override
    {
        for (auto c : getChildren())
        {
            auto skc = dynamic_cast<Surge::GUI::SkinConsumingComponent *>(c);

            if (skc)
                skc->setSkin(skin, associatedBitmapStore);
            else
                std::cout << "Skipping an element " << std::endl;
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationListContents);
};

void ModulationSideControls::valueChanged(GUI::IComponentTagValue *c)

{
    auto tag = c->getTag();
    switch (tag)
    {
    case tag_sort_by:
    {
        auto v = c->getValue();
        if (v > 0.5)
            editor->modContents->sortOrder = ModulationListContents::BY_TARGET;
        else
            editor->modContents->sortOrder = ModulationListContents::BY_SOURCE;
        editor->modContents->rebuildFrom(editor->synth);
    }
    break;
    case tag_filter_by:
    {
        /*
         * Make a popup menu to filter
         */
        auto men = juce::PopupMenu();
        std::set<std::string> sources, targets;
        for (const auto &r : editor->modContents->rows)
        {
            sources.insert(r->datum.sname);
            targets.insert(r->datum.pname);
        }
        // FIXME add help component

        auto tcomp =
            std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Filter Modulation List", "");
        tcomp->setSkin(skin, associatedBitmapStore);
        men.addCustomItem(-1, std::move(tcomp));
        men.addSeparator();
        men.addItem(Surge::GUI::toOSCaseForMenu("Clear Filter"), [this]() {
            editor->modContents->clearFilters();
            filterL->setText("Filter By", juce::NotificationType::dontSendNotification);
            filterW->setLabels({"-"});
        });
        men.addSeparator();
        men.addSectionHeader("BY SOURCE");
        for (auto s : sources)
            men.addItem(s, [this, s]() {
                editor->modContents->filterBySource(s);
                filterL->setText("Filter By Source", juce::NotificationType::dontSendNotification);
                filterW->setLabels({s});
            });
        men.addSectionHeader("BY TARGET");
        for (auto t : targets)
            men.addItem(t, [this, t]() {
                editor->modContents->filterByTarget(t);
                filterL->setText("Filter By Target", juce::NotificationType::dontSendNotification);
                filterW->setLabels({t});
            });
        men.showMenuAsync(juce::PopupMenu::Options(), GUI::makeEndHoverCallback(filterW.get()));
    }
    break;
    case tag_add_source:
    {
        auto men = juce::PopupMenu();

        auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Add Modulation", "");
        tcomp->setSkin(skin, associatedBitmapStore);
        men.addCustomItem(-1, std::move(tcomp));
        men.addSeparator();
        men.addItem("Coming soon!", [this]() {});
        men.showMenuAsync(juce::PopupMenu::Options(), GUI::makeEndHoverCallback(addSourceW.get()));
    }
    break;
    case tag_value_disp:
    {
        int v = round(c->getValue() * 3);
        switch (v)
        {
        case 0:
            editor->modContents->valueDisplay = ModulationListContents::NOMOD;
            break;
        case 1:
            editor->modContents->valueDisplay = ModulationListContents::MOD_ONLY;
            break;
        case 2:
            editor->modContents->valueDisplay = ModulationListContents::CTR_PLUS_MOD;
            break;
        case 3:
            editor->modContents->valueDisplay = ModulationListContents::ALL;
            break;
        }
        editor->repaint();
        Surge::Storage::updateUserDefaultValue(&(editor->synth->storage),
                                               Storage::ModulationEditorValueDisplay, v);
    }
    break;
    }
}

ModulationEditor::ModulationEditor(SurgeGUIEditor *ed, SurgeSynthesizer *s)
    : ed(ed), synth(s), OverlayComponent("Modulation Editor")
{
    modContents = std::make_unique<ModulationListContents>(this);
    modContents->rebuildFrom(synth);
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(modContents.get(), false);
    addAndMakeVisible(*viewport);

    struct IdleTimer : juce::Timer
    {
        IdleTimer(ModulationEditor *ed) : moded(ed){};
        void timerCallback() override { moded->idle(); }
        ModulationEditor *moded;
    };
    idleTimer = std::make_unique<IdleTimer>(this);
    idleTimer->startTimerHz(60);

    synth->addModulationAPIListener(this);

    sideControls = std::make_unique<ModulationSideControls>(this);

    addAndMakeVisible(*sideControls);
}

void ModulationEditor::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }
void ModulationEditor::resized()
{
    auto t = getTransform().inverted();
    auto h = getHeight();
    auto w = getWidth();

    t.transformPoint(w, h);

    int bottomH = 35;
    int sideW = 140;

    sideControls->setBounds(0, 0, sideW, h);
    auto vpr = juce::Rectangle<int>(sideW, 0, w - sideW, h).reduced(2);
    viewport->setBounds(vpr);
    modContents->setSize(vpr.getWidth() - viewport->getScrollBarThickness() - 1, 10);
    modContents->rebuildFrom(synth);
}

ModulationEditor::~ModulationEditor()
{
    synth->removeModulationAPIListener(this);
    needsModUpdate = false;
    needsModValueOnlyUpdate = false;
    idleTimer->stopTimer();
}

/*
 * At a later date I can make this more efficient but for now if any modulation changes
 * in the main UI just rebuild the table.
 */
void ModulationEditor::idle()
{
    // fixme compare exchange week
    if (needsModUpdate)
    {
        needsModUpdate = false;
        modContents->rebuildFrom(synth);
    }
    if (needsModValueOnlyUpdate)
    {
        needsModValueOnlyUpdate = false;
        modContents->updateAllValues(synth);
    }
}

void ModulationEditor::updateParameterById(const SurgeSynthesizer::ID &pid)
{
    modContents->updateAllValues(synth);
}

void ModulationEditor::modSet(long ptag, modsources modsource, int modsourceScene, int index,
                              float value, bool isNew)
{
    if (!selfModulation)
    {
        if (isNew || value == 0)
            needsModUpdate = true;
        else
            needsModValueOnlyUpdate = true;
    }
}
void ModulationEditor::modMuted(long ptag, modsources modsource, int modsourceScene, int index,
                                bool mute)
{
    if (!selfModulation)
        needsModValueOnlyUpdate = true;
}
void ModulationEditor::modCleared(long ptag, modsources modsource, int modsourceScene, int index)
{
    if (!selfModulation)
        needsModUpdate = true;
}

void ModulationEditor::onSkinChanged()
{
    sideControls->setSkin(skin, associatedBitmapStore);
    modContents->setSkin(skin, associatedBitmapStore);
    modContents->rebuildFrom(synth);
    repaint();
}

} // namespace Overlays
} // namespace Surge
