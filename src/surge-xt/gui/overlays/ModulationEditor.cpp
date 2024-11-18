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
#include <fmt/core.h>

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
    ModulationSideControls(ModulationEditor *e, SurgeGUIEditor *sge) : editor(e)
    {
        this->sge = sge;
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        create();
    }
    void create()
    {
        auto makeL = [this](const std::string &s) {
            auto l = std::make_unique<juce::Label>(s);
            l->setText(s, juce::dontSendNotification);
            if (skin)
                l->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            l->setAccessible(true);
            l->setTitle(s);
            l->setDescription(s);
            l->setWantsKeyboardFocus(false);
            addAndMakeVisible(*l);
            return l;
        };

        auto makeW = [this](const std::vector<std::string> &l, int tag, bool en,
                            const std::string &acctitle, bool vert = false) {
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
            w->setAccessible(true);
            w->setWantsKeyboardFocus(true);
            w->setTitle(acctitle);
            w->setDescription(acctitle);
            w->setStorage(&(editor->synth->storage));
            addAndMakeVisible(*w);
            return w;
        };

        sortL = makeL("Sort By");
        sortW = makeW(
            {
                "Source",
                "Target",
            },
            tag_sort_by, true, "Sort List By...");
        auto sortOrder =
            editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.sortOrder;
        sortW->setValue(sortOrder);

        filterL = makeL("Filter By");
        filterW = makeW({"-"}, tag_filter_by, true, "Filter List By...");

        auto fo =
            editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn;
        if (fo != 0)
        {
            auto fs = editor->synth->storage.getPatch()
                          .dawExtraState.editor.modulationEditorState.filterString;
            filterW->setLabels({fs});
        }

        addL = makeL("Add Modulation");
        addSourceW = makeW({"Select Source"}, tag_add_source, true, "Select Source");
        addTargetW = makeW({"Select Target"}, tag_add_target, false, "Select Target");

        dispL = makeL("Value Display");
        dispW = makeW({"None", "Depths", "Values and Depths", "Values, Depths and Ranges"},
                      tag_value_disp, true, "Value Displays", true);
        dispW->setAccessible(false);

        auto dwv = Surge::Storage::getUserDefaultValue(&(editor->synth->storage),
                                                       Storage::ModListValueDisplay, 3);
        dispW->setValue(dwv / 3.0);
        dispW->setDraggable(true);
        valueChanged(dispW.get());

        copyW = std::make_unique<Surge::Widgets::SelfDrawButton>("Copy to Clipboard");
        copyW->setAccessible(true);
        copyW->setStorage(&(editor->synth->storage));
        copyW->setTitle("Copy to Clipboard");
        copyW->setDescription("Copy to Clipboard");
        copyW->setSkin(skin);
        copyW->onClick = [this]() { doCopyToClipboard(); };
        addAndMakeVisible(*copyW);
    }

    void resized() override
    {
        float ypos = 4;
        float xpos = 4;

        auto h = 16.0;
        auto m = 4;

        auto b = juce::Rectangle<int>(xpos, ypos, getWidth() - 2 * xpos, h);
        sortL->setBounds(b);
        b = b.translated(0, h);
        sortW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        filterL->setBounds(b);
        b = b.translated(0, h);
        filterW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        addL->setBounds(b);
        b = b.translated(0, h);
        addSourceW->setBounds(b);
        b = b.translated(0, h + m);
        addTargetW->setBounds(b);
        b = b.translated(0, h * 1.5 + m);

        dispL->setBounds(b);
        b = b.translated(0, h);
        dispW->setBounds(b.withHeight(h * 4));

        b = b.translated(0, h * 4.5 + m);
        copyW->setBounds(b);
    }

    void paint(juce::Graphics &g) override
    {
        if (!skin)
        {
            return;
        }

        g.fillAll(skin->getColor(Colors::MSEGEditor::Panel));
    }

    void valueChanged(GUI::IComponentTagValue *c) override;
    int32_t controlModifierClicked(GUI::IComponentTagValue *p, const juce::ModifierKeys &mods,
                                   bool isDoubleClickEvent) override
    {
        auto tag = p->getTag();

        switch (tag)
        {
        case tag_filter_by:
        case tag_add_source:
        case tag_add_target:
            valueChanged(p);
            return true;
        case tag_sort_by:
        case tag_value_disp:
            auto men = juce::PopupMenu();
            auto st = &(editor->synth->storage);
            auto msurl = st ? sge->helpURLForSpecial(st, "mod-list") : std::string();
            auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
            std::string name =
                (tag == tag_sort_by) ? "Sort Modulation List" : "Modulation List Value Display";

            auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(name, hurl);

            tcomp->setSkin(skin, associatedBitmapStore);
            tcomp->setCentered(false);

            auto hment = tcomp->getTitle();
            men.addCustomItem(-1, std::move(tcomp), nullptr, hment);

            men.showMenuAsync(sge->popupMenuOptions());

            return true;
        }
        return true;
    }

    modsources add_ms;
    int add_ms_idx, add_ms_sc;
    int dest_id;

    void showAddSourceMenu();
    void showAddTargetMenu();
    void doAdd();

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
            sortL->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
            filterL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            filterL->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
            addL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            addL->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
            dispL->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
            dispL->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
        }
    }

    void doCopyToClipboard();

    std::unique_ptr<juce::Label> sortL, filterL, addL, dispL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> sortW, filterW, addSourceW, addTargetW,
        dispW;
    std::unique_ptr<Surge::Widgets::SelfDrawButton> copyW;
    SurgeGUIEditor *sge;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationSideControls);
};

struct ModulationListContents : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    struct ModListIconButton : public Surge::Widgets::TinyLittleIconButton
    {
        SurgeStorage *storage{nullptr};
        ModListIconButton(int off, std::function<void()> cb, SurgeStorage *s)
            : Surge::Widgets::TinyLittleIconButton(off, std::move(cb)), storage(s)
        {
        }

        std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
        {
            return std::make_unique<juce::AccessibilityHandler>(
                *this, juce::AccessibilityRole::button,
                juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                       [this]() { this->callback(); }));
        }

        bool keyPressed(const juce::KeyPress &key) override
        {
            if (!storage)
                return false;
            auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);
            if (action == Widgets::Return)
            {
                callback();
                return true;
            }
            return false;
        }

        std::function<void()> onFocusFromTabFn{nullptr};
        void focusGained(FocusChangeType cause) override
        {
            if (onFocusFromTabFn && cause == FocusChangeType::focusChangedByTabKey)
            {
                onFocusFromTabFn();
            }
        }
    };
    ModulationEditor *editor{nullptr};
    ModulationListContents(ModulationEditor *e) : editor(e)
    {
        auto &mes = e->synth->storage.getPatch().dawExtraState.editor.modulationEditorState;
        sortOrder = (SortOrder)mes.sortOrder;

        filterOn = (FilterOn)mes.filterOn;
        switch (filterOn)
        {
        case NONE:
            clearFilters();
            break;
        case SOURCE:
            filterBySource(mes.filterString);
            break;
        case TARGET:
            filterByTarget(mes.filterString);
            break;
        case TARGET_CG:
            filterByTargetControlGroup((ControlGroup)mes.filterInt, mes.filterString);
            break;
        case TARGET_SCENE:
            filterByTargetScene(mes.filterInt, mes.filterString);
            break;
        }

        setAccessible(true);
    }

    void paint(juce::Graphics &g) override
    {
        if (rows.empty())
        {
            g.setFont(skin->fontManager->getLatoAtSize(20));
            g.setColour(skin->getColor(Colors::ModulationListOverlay::DimText));
            g.drawText("No Modulations Assigned", getLocalBounds(), juce::Justification::centred);
        }
    }

    struct Datum
    {
        int source_scene, source_id, source_index, destination_id, inScene;
        std::string pname, sname, moddepth;
        bool isBipolar;
        bool isPerScene;
        int idBase;
        int pscene;
        ControlGroup pControlGroup;
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

    int viewportWidth{100};
    void setViewportWidth(int w) { viewportWidth = w; }

    struct DataRowEditor : public juce::Component,
                           public Surge::GUI::SkinConsumingComponent,
                           public Surge::GUI::IComponentTagValue::Listener
    {
        static constexpr int height = 32;
        Datum datum;
        ModulationListContents *contents{nullptr};
        int idx{0};

        DataRowEditor(const Datum &d, int idxi, ModulationListContents *c)
            : datum(d), idx(idxi), contents(c)
        {
            clearButton = std::make_unique<ModListIconButton>(
                1,
                [this]() {
                    auto me = contents->editor;
                    contents->preferredFocusRow = idx;
                    ModulationEditor::SelfModulationGuard g(me);
                    contents->editor->ed->pushModulationToUndoRedo(
                        datum.destination_id + datum.idBase, (modsources)datum.source_id,
                        datum.source_scene, datum.source_index, Surge::GUI::UndoManager::UNDO);
                    me->synth->clearModulation(datum.destination_id + datum.idBase,
                                               (modsources)datum.source_id, datum.source_scene,
                                               datum.source_index, false);
                    // The rebuild may delete this so defer
                    auto c = contents;
                    juce::Timer::callAfterDelay(1, [c, me]() {
                        c->rebuildFrom(me->synth);
                        me->ed->queue_refresh = true;
                    });
                },
                &c->editor->synth->storage);
            clearButton->setAccessible(true);
            clearButton->setTitle("Clear");
            clearButton->setDescription("Clear");
            clearButton->setWantsKeyboardFocus(true);
            clearButton->onFocusFromTabFn = [this]() {
                int nAbove = 7;
                if (idx >= nAbove)
                {
                    auto p = idx - nAbove;
                    int off = (p == 0 ? 0 : -1);
                    contents->editor->viewport->setViewPosition(0, p * height + off);
                }
            };
            addAndMakeVisible(*clearButton);

            muted = d.isMuted;
            muteButton = std::make_unique<ModListIconButton>(
                2,
                [this]() {
                    auto me = contents->editor;
                    contents->preferredFocusRow = idx;
                    ModulationEditor::SelfModulationGuard g(me);
                    contents->editor->ed->pushModulationToUndoRedo(
                        datum.destination_id + datum.idBase, (modsources)datum.source_id,
                        datum.source_scene, datum.source_index, Surge::GUI::UndoManager::UNDO);

                    me->synth->muteModulation(datum.destination_id + datum.idBase,
                                              (modsources)datum.source_id, datum.source_scene,
                                              datum.source_index, !muted);
                    muted = !muted;
                    contents->rebuildFrom(me->synth);
                },
                &c->editor->synth->storage);
            muteButton->setAccessible(true);
            muteButton->setWantsKeyboardFocus(true);
            addAndMakeVisible(*muteButton);

            pencilButton = std::make_unique<ModListIconButton>(
                0,
                [this]() {
                    auto sge = contents->editor->ed;
                    contents->preferredFocusRow = idx;
                    auto p = contents->editor->synth->storage.getPatch()
                                 .param_ptr[datum.destination_id + datum.idBase];
                    sge->promptForUserValueEntry(p, pencilButton.get(), datum.source_id,
                                                 datum.source_scene, datum.source_index);
                },
                &c->editor->synth->storage);
            pencilButton->setAccessible(true);
            pencilButton->setTitle("Edit");
            pencilButton->setDescription("Edit");
            pencilButton->setWantsKeyboardFocus(true);
            addAndMakeVisible(*pencilButton);

            surgeLikeSlider = std::make_unique<Surge::Widgets::ModulatableSlider>();
            surgeLikeSlider->setOrientation(Surge::ParamConfig::kHorizontal);
            surgeLikeSlider->setBipolarFn([]() { return true; });
            surgeLikeSlider->setIsLightStyle(true);
            surgeLikeSlider->setStorage(&(contents->editor->synth->storage));
            surgeLikeSlider->setAlwaysUseModHandle(true);
            surgeLikeSlider->addListener(this);
            surgeLikeSlider->setAccessible(true);
            surgeLikeSlider->setTitle("Depth");
            surgeLikeSlider->setDescription("Depth");
            surgeLikeSlider->setWantsKeyboardFocus(true);
            surgeLikeSlider->customToAccessibleString = [this] { return datum.mss.dvalplus; };
            addAndMakeVisible(*surgeLikeSlider);

            setAccessible(true);
            setFocusContainerType(juce::Component::FocusContainerType::focusContainer);
            resetValuesFromDatum();
        }

        void controlBeginEdit(GUI::IComponentTagValue *control) override
        {
            auto synth = contents->editor->ed->synth;
            for (auto l : contents->editor->ed->synth->modListeners)
            {
                auto p = synth->storage.getPatch().param_ptr[datum.destination_id + datum.idBase];

                auto nm01 = control->getValue() * 2.f - 1.f;

                l->modBeginEdit(p->id, (modsources)datum.source_id, datum.source_scene,
                                datum.source_index, nm01);
            }
        }

        void controlEndEdit(GUI::IComponentTagValue *control) override
        {
            auto synth = contents->editor->ed->synth;
            for (auto l : contents->editor->ed->synth->modListeners)
            {
                auto p = synth->storage.getPatch().param_ptr[datum.destination_id + datum.idBase];

                auto nm01 = control->getValue() * 2.f - 1.f;

                l->modEndEdit(p->id, (modsources)datum.source_id, datum.source_scene,
                              datum.source_index, nm01);
            }
        }

        void resetValuesFromDatum()
        {
            std::string accPostfix = datum.sname + " to " + datum.pname;

            surgeLikeSlider->setValue((datum.moddepth01 + 1) * 0.5);
            surgeLikeSlider->setQuantitizedDisplayValue((datum.moddepth01 + 1) * 0.5);
            surgeLikeSlider->setIsModulationBipolar(datum.isBipolar);
            surgeLikeSlider->setTitle("Depth " + accPostfix);
            surgeLikeSlider->setDescription("Depth " + accPostfix);

            muteButton->offset = 2;
            muteButton->setTitle("Mute " + accPostfix);
            muteButton->setDescription("Mute " + accPostfix);

            if (datum.isMuted)
            {
                muteButton->setTitle("UnMute " + accPostfix);
                muteButton->setDescription("UnMute " + accPostfix);
                muteButton->offset = 3;
            }

            clearButton->setTitle("Clear " + accPostfix);
            clearButton->setDescription("Clear " + accPostfix);

            pencilButton->setTitle("Edit " + accPostfix);
            pencilButton->setDescription("Edit " + accPostfix);

            auto t = std::string("Source: ") + datum.sname + (" to  Target: ") + datum.pname;
            setTitle(t);
            setDescription(t);

            repaint();
        }

        void beTheFocusedRow()
        {
            if (!contents->editor)
                return;
            if (!contents->editor->viewport)
                return;
            if (!surgeLikeSlider->isVisible())
                return;

            int nAbove = 7;
            if (idx >= nAbove)
            {
                auto p = idx - nAbove;
                int off = (p == 0 ? 0 : -1);
                contents->editor->viewport->setViewPosition(0, p * height + off);
            }
            else
            {
                contents->editor->viewport->setViewPosition(0, 0);
            }
            if (surgeLikeSlider->isShowing())
                surgeLikeSlider->grabKeyboardFocus();
        }
        bool firstInSort{false}, hasFollower{false};
        bool isTop{false}, isAfterTop{false}, isLast{false};

        void paint(juce::Graphics &g) override
        {
            static constexpr int indent = 1;
            auto b = getLocalBounds().withTrimmedLeft(indent);

            g.fillAll(skin->getColor(Colors::MSEGEditor::Background));

            g.setColour(skin->getColor(Colors::ModulationListOverlay::Border));

            if (firstInSort)
            {
                // draw the top and the side
                g.drawLine(indent, 0, getWidth(), 0, 1);
            }
            if (isLast)
            {
                g.drawLine(indent, getHeight() - 1, getWidth(), getHeight() - 1, 1);
            }

            g.drawLine(indent, 0, indent, getHeight(), 1);
            g.drawLine(getWidth() - indent, 0, getWidth() - indent, getHeight(), 1);

            g.setFont(skin->fontManager->getLatoAtSize(9));
            g.setColour(skin->getColor(Colors::ModulationListOverlay::Text));

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
                g.setColour(skin->getColor(Colors::ModulationListOverlay::Text));

                g.drawText(firstLab, tb, juce::Justification::topLeft);
            }
            g.setColour(skin->getColor(Colors::ModulationListOverlay::Arrows));

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
                {
                    g.drawArrow({xStart, yEnd, xStart, yStart}, 1, 3, 4);
                }
                else
                {
                    g.drawLine(xStart, yEnd, xStart, yStart, 1);
                }

                g.drawLine(xStart, yEnd, xEnd, yEnd, 1);
            }

            if (hasFollower)
            {
                g.drawLine(xStart, yEnd, xStart, getHeight(), 1);
            }

            g.setColour(skin->getColor(Colors::ModulationListOverlay::Text));

            g.drawText(secondLab, tb.withTrimmedLeft(2), juce::Justification::centredLeft);

            if ((isTop || isAfterTop) && !firstInSort)
            {
                g.setColour(skin->getColor(Colors::ModulationListOverlay::DimText));
                auto tf = g.getCurrentFont();
                g.setFont(skin->fontManager->getLatoAtSize(7));
                tb = tb.withTop(0);
                g.drawText(firstLab, tb.withTrimmedLeft(2), juce::Justification::topLeft);
                g.setFont(tf);
            }

            if (contents->valueDisplay == NOMOD)
            {
                return;
            }

            g.setFont(skin->fontManager->getLatoAtSize(9));
            auto sb = surgeLikeSlider->getBounds();
            auto bb = sb.withY(0).withHeight(getHeight()).reduced(0, 1);

            auto shift = 60, over = 13;
            auto lb = bb.translated(-shift, 0).withWidth(shift + over -
                                                         3); // damn thing isn't very symmetric
            auto rb = bb.withX(bb.getX() + bb.getWidth() - over).withWidth(shift + over);
            auto cb = bb.withTrimmedLeft(over).withTrimmedRight(over);

            g.setColour(skin->getColor(Colors::ModulationListOverlay::Text));

            if (contents->valueDisplay & CTR)
            {
                g.drawFittedText(datum.mss.val, cb, juce::Justification::centredTop, 1, 0.1);
            }

            if (contents->valueDisplay & MOD_ONLY)
            {
                g.setColour(skin->getColor(Colors::Slider::Modulation::Positive));
                g.drawFittedText(datum.mss.dvalplus, rb, juce::Justification::topLeft, 1, 0.1);

                if (datum.isBipolar)
                {
                    g.setColour(skin->getColor(Colors::Slider::Modulation::Negative));
                    g.drawFittedText(datum.mss.dvalminus, lb, juce::Justification::topRight, 1,
                                     0.1);
                }
            }

            if ((contents->valueDisplay & EXTRAS) == 0)
            {
                return;
            }

            g.setColour(skin->getColor(Colors::Slider::Modulation::Positive));
            g.drawFittedText(datum.mss.valplus, rb, juce::Justification::bottomLeft, 1, 0.1);

            if (datum.isBipolar)
            {
                g.setColour(skin->getColor(Colors::Slider::Modulation::Negative));
                g.drawFittedText(datum.mss.valminus, lb, juce::Justification::bottomRight, 1, 0.1);
            }
        }

        static constexpr int controlsStart = 160;
        static constexpr int controlsEnd = controlsStart + (14 + 2) * 3 + 140 + 2;

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
            startX += bh + 2;
            pencilButton->setBounds(startX, bY, bh, bh);
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

                contents->editor->ed->pushModulationToUndoRedo(
                    p->id, (modsources)datum.source_id, datum.source_scene, datum.source_index,
                    Surge::GUI::UndoManager::UNDO);
                synth->setModDepth01(datum.destination_id + datum.idBase,
                                     (modsources)datum.source_id, datum.source_scene,
                                     datum.source_index, nm01);

                contents->populateDatum(datum, synth);
                resetValuesFromDatum();
            }
        }

        int32_t controlModifierClicked(Surge::GUI::IComponentTagValue *p,
                                       const juce::ModifierKeys &mods,
                                       bool isDoubleClickEvent) override
        {
            if (isDoubleClickEvent)
            {
                surgeLikeSlider->setValue(0.5f);
                valueChanged(surgeLikeSlider.get());
            }

            return 0;
        }

        void onSkinChanged() override
        {
            clearButton->icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
            muteButton->icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
            pencilButton->icons = associatedBitmapStore->getImage(IDB_MODMENU_ICONS);
            surgeLikeSlider->setSkin(skin, associatedBitmapStore);
        }

        bool muted{false};
        std::unique_ptr<ModListIconButton> clearButton;
        std::unique_ptr<ModListIconButton> muteButton;
        std::unique_ptr<ModListIconButton> pencilButton;
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
            r->isAfterTop = prior && yPos < -4; // that's about scroll for first. #5602
            prior = r->isTop;
        }

        repaint();
    }

    enum SortOrder
    {
        BY_SOURCE = 0,
        BY_TARGET
    } sortOrder{BY_SOURCE};

    std::string filterString;
    int filterInt;

    enum FilterOn
    {
        NONE,
        SOURCE,
        TARGET,
        TARGET_SCENE,
        TARGET_CG
    } filterOn{NONE};

    void clearFilters()
    {
        if (filterOn != NONE)
        {
            filterOn = NONE;
            rebuildFrom(editor->synth);

            editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn =
                (int)NONE;
        }
    }

    void filterBySource(const std::string &s)
    {
        filterOn = SOURCE;
        filterString = s;
        rebuildFrom(editor->synth);

        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn =
            (int)SOURCE;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterString =
            s;
    }

    void filterByTarget(const std::string &s)
    {
        filterOn = TARGET;
        filterString = s;
        rebuildFrom(editor->synth);

        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn =
            (int)TARGET;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterString =
            s;
    }

    void filterByTargetScene(int s, const std::string &lb)
    {
        filterOn = TARGET_SCENE;
        filterInt = s;
        rebuildFrom(editor->synth);
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn =
            (int)TARGET_SCENE;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterInt = s;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterString =
            lb;
    }

    void filterByTargetControlGroup(ControlGroup c, const std::string &lb)
    {
        filterOn = TARGET_CG;
        filterInt = c;
        rebuildFrom(editor->synth);
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterOn =
            (int)TARGET_CG;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterInt = c;
        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.filterString =
            lb;
    }

    /*
     * Rows is the visible UI, dataRows is the entire set of data. In the case of filtered
     * displays, it is not the case that rows[i].datum == dataRows[i]
     */
    std::vector<std::unique_ptr<DataRowEditor>> rows;
    std::vector<Datum> dataRows;

    void populateDatum(Datum &d, const SurgeSynthesizer *synth)
    {
        std::string sceneMod = "";
        d.isPerScene = synth->isModulatorDistinctPerScene((modsources)d.source_id);

        if (synth->isModulatorDistinctPerScene((modsources)d.source_id))
        {
            sceneMod = std::string(" (") + (d.source_scene == 0 ? "A" : "B") + ")";
        }

        char nm[1024];
        SurgeSynthesizer::ID ptagid;
        auto p = synth->storage.getPatch().param_ptr[d.destination_id + d.idBase];
        if (p->ctrlgroup == cg_LFO)
        {
            std::string pfx = p->scene == 1 ? "A " : "B ";
            p->create_fullname(p->get_name(), nm, p->ctrlgroup, p->ctrlgroup_entry,
                               (pfx + ModulatorName::modulatorName(
                                          &synth->storage, p->ctrlgroup_entry, true, p->scene))
                                   .c_str());
        }
        else
        {
            if (synth->fromSynthSideId(d.destination_id + d.idBase, ptagid))
                synth->getParameterName(ptagid, nm);
        }
        d.pscene = p->scene;
        d.pControlGroup = p->ctrlgroup;

        std::string sname = ModulatorName::modulatorNameWithIndex(
            &synth->storage, d.source_scene, d.source_id, d.source_index, false, d.inScene < 0);

        d.sname = sname + sceneMod;
        d.pname = nm;

        char pdisp[TXT_SIZE];
        int ptag = p->id;
        auto thisms = (modsources)d.source_id;

        d.moddepth01 = synth->getModDepth01(ptag, thisms, d.source_scene, d.source_index);
        d.isBipolar = synth->isBipolarModulation(thisms);
        d.isMuted = synth->isModulationMuted(ptag, thisms, d.source_scene, d.source_index);
        p->get_display_of_modulation_depth(
            pdisp, synth->getModDepth(ptag, thisms, d.source_scene, d.source_index),
            synth->isBipolarModulation(thisms), Parameter::InfoWindow, &(d.mss));
        d.moddepth = pdisp;
    }

    void rebuildFrom(SurgeSynthesizer *synth)
    {
        removeAllChildren();
        dataRows.clear();
        rows.clear();
        int ypos = 0;
        auto append = [this, synth](const std::string &type,
                                    const std::vector<ModulationRouting> &r, int idBase,
                                    int scene) {
            if (r.empty())
                return;

            for (const auto &q : r)
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

                if (a.source_index != b.source_index)
                    return a.source_index < b.source_index;

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

                if (a.source_index != b.source_index)
                    return a.source_index < b.source_index;
            }

            return false;
        });

        std::string priorN = "-";

        int idx = 0;
        for (const auto &d : dataRows)
        {
            auto included = filterOn == NONE || (filterOn == SOURCE && d.sname == filterString) ||
                            (filterOn == TARGET && d.pname == filterString) ||
                            (filterOn == TARGET_CG && d.pControlGroup == filterInt) ||
                            (filterOn == TARGET_SCENE && d.pscene == filterInt);

            if (!included)
            {
                continue;
            }
            auto l = std::make_unique<DataRowEditor>(d, idx++, this);
            auto sortName = sortOrder == BY_SOURCE ? d.sname : d.pname;

            l->setSkin(skin, associatedBitmapStore);

            if (sortName != priorN)
            {
                priorN = sortName;
                l->firstInSort = true;
            }

            if (included)
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

        if (!rows.empty())
        {
            rows.back()->isLast = true;
        }

        bool needVSB = true;
        int sbw = 10;

        if (editor && editor->viewport && editor->viewport->getHeight() > 0)
        {
            if (ypos > editor->viewport->getHeight())
            {
                needVSB = true;
            }
            else
            {
                needVSB = false;
            }
            sbw = editor->viewport->getScrollBarThickness() + 2;
            editor->viewport->setScrollBarsShown(sbw, false);

            if (rows.empty())
            {
                ypos = editor->viewport->getHeight();
            }
        }

        auto w = viewportWidth - (needVSB ? sbw : 0) - 3;

        setSize(w, ypos);

        for (const auto &r : rows)
        {
            auto b = r->getBounds().withWidth(w - 1);
            r->setBounds(b);
        }

        moved(); // to refresh the 'istop'

        if (preferredFocusRow < 0 || preferredFocusRow >= dataRows.size())
            preferredFocusRow = 0;

        if (preferredFocusRow >= 0 && preferredFocusRow < rows.size() && rows[preferredFocusRow])
            rows[preferredFocusRow]->beTheFocusedRow();
    }

    int preferredFocusRow{0};

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
            {
                skc->setSkin(skin, associatedBitmapStore);
            }
            else
            {
                // std::cout << "Skipping an element " << std::endl;
            }
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModulationListContents);
};

void ModulationSideControls::doCopyToClipboard()
{
    std::ostringstream oss;
    oss << "Modulation List for " << editor->ed->getStorage()->getPatch().name << "\n";
    for (const auto &r : editor->modContents->dataRows)
    {
        oss << "  Source: " << r.sname << "; Target: " << r.pname << "; Depth:  " << r.mss.valplus
            << " " << (r.isBipolar ? "(Bipolar)" : "(Unipolar)") << "\n";
    }
    juce::SystemClipboard::copyTextToClipboard(oss.str());
}

void ModulationSideControls::valueChanged(GUI::IComponentTagValue *c)
{
    auto tag = c->getTag();
    switch (tag)
    {
    case tag_sort_by:
    {
        auto v = c->getValue();

        if (v > 0.5)
        {
            editor->modContents->sortOrder = ModulationListContents::BY_TARGET;
        }
        else
        {
            editor->modContents->sortOrder = ModulationListContents::BY_SOURCE;
        }

        editor->synth->storage.getPatch().dawExtraState.editor.modulationEditorState.sortOrder =
            editor->modContents->sortOrder;
        editor->modContents->rebuildFrom(editor->synth);
    }
    break;
    case tag_filter_by:
    {
        auto men = juce::PopupMenu();
        std::set<std::string> sources, targets;

        for (const auto &r : editor->modContents->dataRows)
        {
            sources.insert(r.sname);
            targets.insert(r.pname);
        }

        if (!sources.empty() && !targets.empty())
        {
            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "BY SOURCE");

            for (auto s : sources)
                men.addItem(s, [this, s]() {
                    editor->modContents->filterBySource(s);
                    filterL->setText("Filter By Source",
                                     juce::NotificationType::dontSendNotification);
                    filterW->setLabels({s});
                });

            men.addColumnBreak();
            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "BY TARGET");

            for (auto t : targets)
                men.addItem(t, [this, t]() {
                    editor->modContents->filterByTarget(t);
                    filterL->setText("Filter By Target",
                                     juce::NotificationType::dontSendNotification);
                    filterW->setLabels({t});
                });

            men.addColumnBreak();
            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men,
                                                                            "BY TARGET SECTION");

            for (int t = cg_GLOBAL; t < endCG; ++t)
            {
                if (t == 1)
                    continue;

                auto lab = fmt::format("{}", ControlGroupDisplay[t]);

                men.addItem(lab, [this, t, lab]() {
                    editor->modContents->filterByTargetControlGroup((ControlGroup)t, lab);
                    filterL->setText("Filter By Target Section",
                                     juce::NotificationType::dontSendNotification);
                    filterW->setLabels({lab});
                });
            }

            men.addColumnBreak();
            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "BY TARGET SCENE");

            for (int t = 0; t < n_scenes + 1; ++t)
            {
                auto lab = fmt::format("Scene {}", (char)('A' + (t - 1)));

                if (t == 0)
                {
                    lab = "Global";
                }

                men.addItem(lab, [this, t, lab]() {
                    editor->modContents->filterByTargetScene(t, lab);
                    filterL->setText("Filter By Target Scene",
                                     juce::NotificationType::dontSendNotification);
                    filterW->setLabels({lab});
                });
            }

            men.addSeparator();

            men.addItem(Surge::GUI::toOSCase("Clear Filter"), [this]() {
                editor->modContents->clearFilters();
                filterL->setText("Filter By", juce::NotificationType::dontSendNotification);
                filterW->setLabels({"-"});
            });

            men.addSeparator();
        }

        auto st = &(editor->synth->storage);
        auto msurl = st ? sge->helpURLForSpecial(st, "mod-list") : std::string();
        auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

        auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(
            "Filter Modulation List", hurl);

        tcomp->setSkin(skin, associatedBitmapStore);
        tcomp->setCentered(false);

        auto hment = tcomp->getTitle();
        men.addCustomItem(-1, std::move(tcomp), nullptr, hment);

        men.showMenuAsync(sge->popupMenuOptions());
    }
    break;
    case tag_add_source:
    {
        showAddSourceMenu();
    }
    break;
    case tag_add_target:
    {
        showAddTargetMenu();
    }
    break;
    case tag_add_go:
    {
        doAdd();
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

        Surge::Storage::updateUserDefaultValue(&(editor->synth->storage),
                                               Storage::ModListValueDisplay, v);
        editor->repaint();
    }
    break;
    }
}

void ModulationSideControls::showAddSourceMenu()
{
    auto men = juce::PopupMenu();

    auto addMacroSub = juce::PopupMenu();
    auto addVLFOASub = juce::PopupMenu();
    auto addSLFOASub = juce::PopupMenu();
    auto addVLFOBSub = juce::PopupMenu();
    auto addSLFOBSub = juce::PopupMenu();
    auto addEGASub = juce::PopupMenu();
    auto addEGBSub = juce::PopupMenu();
    auto addMIDISub = juce::PopupMenu();
    auto addMIDIASub = juce::PopupMenu();
    auto addMIDIBSub = juce::PopupMenu();
    auto addMiscSub = juce::PopupMenu();

    juce::PopupMenu *popMenu{nullptr};
    auto synth = editor->synth;
    auto sge = editor->ed;

    auto st = &(synth->storage);
    auto msurl = st ? sge->helpURLForSpecial(st, "mod-list") : std::string();
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto tcomp =
        std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Add Modulation From", hurl);
    auto hment = tcomp->getTitle();
    tcomp->setSkin(skin, associatedBitmapStore);
    men.addCustomItem(-1, std::move(tcomp), nullptr, hment);
    men.addSeparator();

    for (int sc = 0; sc < n_scenes; ++sc)
    {
        for (int k = 1; k < n_modsources; k++)
        {
            modsources ms = (modsources)modsource_display_order[k];
            popMenu = nullptr;

            if (ms >= ms_ctrl1 && ms <= ms_ctrl1 + n_customcontrollers - 1 && sc == 0)
            {
                popMenu = &addMacroSub;
            }
            else if (ms >= ms_lfo1 && ms <= ms_lfo1 + n_lfos_voice - 1)
            {
                if (sc == 0)
                    popMenu = &addVLFOASub;
                else
                    popMenu = &addVLFOBSub;
            }
            else if (ms >= ms_slfo1 && ms <= ms_slfo1 + n_lfos_scene - 1)
            {
                if (sc == 0)
                    popMenu = &addSLFOASub;
                else
                    popMenu = &addSLFOBSub;
            }
            else if (ms >= ms_ampeg && ms <= ms_filtereg)
            {
                if (sc == 0)
                    popMenu = &addEGASub;
                else
                    popMenu = &addEGBSub;
            }
            else if (ms >= ms_random_bipolar && ms <= ms_alternate_unipolar && sc == 0)
            {
                popMenu = &addMiscSub;
            }
            else if (synth->isModulatorDistinctPerScene(ms))
            {
                if (sc == 0)
                    popMenu = &addMIDIASub;
                else
                    popMenu = &addMIDIBSub;
            }
            else if (sc == 0)
            {
                popMenu = &addMIDISub;
            }

            if (popMenu)
            {
                if (synth->supportsIndexedModulator(sc, ms))
                {
                    int maxidx = synth->getMaxModulationIndex(sc, ms);
                    auto subm = juce::PopupMenu();

                    for (int i = 0; i < maxidx; ++i)
                    {
                        auto subn = ModulatorName::modulatorNameWithIndex(&synth->storage, sc, ms,
                                                                          i, false, false);

                        subm.addItem(subn, [this, ms, i, sc, subn]() {
                            add_ms = ms;
                            add_ms_idx = i;
                            add_ms_sc = sc;
                            addSourceW->setLabels({subn});
                            addTargetW->setEnabled(true);
                            addTargetW->setLabels({"Select Target"});
                            dest_id = -1;
                            repaint();
                        });
                    }

                    popMenu->addSubMenu(ModulatorName::modulatorNameWithIndex(
                                            &synth->storage, sc, ms, -1, false, false, false),
                                        subm);
                }
                else
                {
                    auto sn = ModulatorName::modulatorNameWithIndex(&synth->storage, sc, ms, 0,
                                                                    false, false);

                    popMenu->addItem(sn, [this, sn, sc, ms]() {
                        add_ms = ms;
                        add_ms_idx = 0;
                        add_ms_sc = sc;
                        addSourceW->setLabels({sn});
                        addTargetW->setEnabled(true);
                        addTargetW->setLabels({"Select Target"});
                        dest_id = -1;
                    });
                }
            }
        }
    }
    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "GLOBAL");
    men.addSubMenu("Macros", addMacroSub);
    men.addSubMenu("MIDI", addMIDISub);
    men.addSubMenu("Internal", addMiscSub);
    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "SCENE A");
    men.addSubMenu("Voice LFOs", addVLFOASub);
    men.addSubMenu("Scene LFOs", addSLFOASub);
    men.addSubMenu("Envelopes", addEGASub);
    men.addSubMenu("MIDI", addMIDIASub);
    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "SCENE B");
    men.addSubMenu("Voice LFOs", addVLFOBSub);
    men.addSubMenu("Scene LFOs", addSLFOBSub);
    men.addSubMenu("Envelopes", addEGBSub);
    men.addSubMenu("MIDI", addMIDIBSub);

    men.showMenuAsync(sge->popupMenuOptions());
}

void ModulationSideControls::showAddTargetMenu()
{
    if (!addTargetW->isEnabled())
    {
        return;
    }

    auto synth = editor->synth;

    auto men = juce::PopupMenu();

    auto st = &(synth->storage);
    auto msurl = st ? sge->helpURLForSpecial(st, "mod-list") : std::string();
    auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);

    auto tcomp =
        std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Add Modulation To", hurl);
    auto hment = tcomp->getTitle();
    tcomp->setSkin(skin, associatedBitmapStore);
    men.addCustomItem(-1, std::move(tcomp), nullptr, hment);
    men.addSeparator();

    std::array<std::map<int, std::map<int, std::vector<std::pair<int, std::string>>>>, n_scenes + 1>
        parsBySceneThenCGThenCGE;
    int SENTINEL_ENTRY = 10000001;

    for (const auto *p : synth->storage.getPatch().param_ptr)
    {
        if (synth->isValidModulation(p->id, add_ms) &&
            ((!synth->isModulatorDistinctPerScene(add_ms)) || (p->scene == add_ms_sc + 1) ||
             (p->scene == 0)))
        {
            auto sc = p->scene;
            auto cg = p->ctrlgroup;
            auto cge = p->ctrlgroup_entry;

            /*
             * This is so gross. See #5518. Basically some parameters which are in the object
             * model in cg_global belong in the UI in cg_something else. So let's hunt them down
             */
            if (sc > 0)
            {
                if (p->id == editor->synth->storage.getPatch().scene[sc - 1].pitch.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].portamento.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].fm_depth.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].drift.id)
                {
                    cg = cg_OSC;
                    cge = SENTINEL_ENTRY;
                }
                if (p->id == editor->synth->storage.getPatch().scene[sc - 1].noise_colour.id)
                {
                    cg = cg_MIX;
                    cge = SENTINEL_ENTRY;
                }
                if (p->id == editor->synth->storage.getPatch().scene[sc - 1].vca_level.id)
                {
                    cg = cg_ENV;
                    cge = SENTINEL_ENTRY;
                }
                if (p->id == editor->synth->storage.getPatch().scene[sc - 1].feedback.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].lowcut.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].filter_balance.id ||
                    p->id == editor->synth->storage.getPatch().scene[sc - 1].wsunit.drive.id)
                {
                    cg = cg_FILTER;
                    cge = SENTINEL_ENTRY;
                }

                if (cg == cg_LFO)
                {
                    if (cge >= ms_lfo1 && cge <= ms_lfo6)
                    {
                        cg = (ControlGroup)1000;
                    }
                    else
                    {
                        cg = (ControlGroup)1001;
                    }
                }
            }

            if (!synth->isActiveModulation(p->id, add_ms, add_ms_sc, add_ms_idx))
                parsBySceneThenCGThenCGE[sc][cg][cge].emplace_back(p->id, p->get_full_name());
        }
    }

    auto cgEntryName = [this](auto scene, auto cg, auto cge) -> std::string {
        switch (cg)
        {
        case cg_OSC:
            return std::string("Osc ") + std::to_string(cge + 1) + " (" +
                   osc_type_names
                       [this->editor->synth->storage.getPatch().scene[scene].osc[cge].type.val.i] +
                   ")";
            break;
        case cg_ENV:
            return cge == 0 ? "AEG" : "FEG";
            break;
        case cg_FX:
            return fxslot_names[cge] + std::string(" (") +
                   fx_type_shortnames[this->editor->synth->storage.getPatch().fx[cge].type.val.i] +
                   ")";
            break;
        case cg_FILTER:
            return std::string("Filter ") + std::to_string(cge + 1) + +" (" +
                   sst::filters::filter_type_names[this->editor->synth->storage.getPatch()
                                                       .scene[scene]
                                                       .filterunit[cge]
                                                       .type.val.i] +
                   ")";
            break;
        case 1000:
        case 1001:
            return ModulatorName::modulatorName(editor->ed->getStorage(), cge, false,
                                                editor->ed->current_scene);
            break;
        default:
            return std::string("CGE=") + std::to_string(cge);
            break;
        }
        return "ERROR";
    };

    int si = 0;

    for (const auto &scene : parsBySceneThenCGThenCGE)
    {
        if (!scene.empty())
        {
            switch (si)
            {
            case 0:
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(men, "GLOBAL");
                break;
            default:
                char ai = 'A';
                ai += si;
                ai -= 1;
                Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
                    men, std::string("SCENE ") + ai);
                break;
            }
        }

        for (const auto &controlGroup : scene)
        {
            auto cgMen = juce::PopupMenu();
            bool makeSub = (controlGroup.second.size() > 1 || controlGroup.first == cg_FX) &&
                           controlGroup.first != cg_MIX;

            for (const auto &controlGroupEntry : controlGroup.second)
            {
                auto subMenu = juce::PopupMenu();
                auto *addToThis = &cgMen;

                if (controlGroupEntry.first == SENTINEL_ENTRY && makeSub)
                    cgMen.addSeparator();

                if (makeSub && controlGroupEntry.first != SENTINEL_ENTRY)
                    addToThis = &subMenu;

                for (const auto &paramPair : controlGroupEntry.second)
                {
                    auto f = paramPair.first;
                    addToThis->addItem(paramPair.second, [this, f]() {
                        dest_id = f;
                        doAdd();
                        repaint();
                    });
                }

                if (makeSub && controlGroupEntry.first != SENTINEL_ENTRY)
                    cgMen.addSubMenu(
                        cgEntryName(si - 1, controlGroup.first, controlGroupEntry.first), subMenu);
            }

            std::string mainN = "";

            switch (controlGroup.first)
            {
            case cg_GLOBAL:
                mainN = (si == 0) ? "Patch" : "Scene";
                break;
            case cg_OSC:
                mainN = "Oscillators";
                break;
            case cg_MIX:
                mainN = "Mixer";
                break;
            case cg_FILTER:
                mainN = "Filters";
                break;
            case cg_ENV:
                mainN = "Envelopes";
                break;
            case 1000:
                mainN = "Voice LFOs";
                break;
            case 1001:
                mainN = "Scene LFOs";
                break;
            case cg_FX:
                mainN = "FX";
                break;
            default:
                mainN = "ERROR";
            }
            men.addSubMenu(mainN, cgMen);
        }

        si++;
    }

    men.showMenuAsync(sge->popupMenuOptions());
}

void ModulationSideControls::doAdd()
{
    auto synth = editor->synth;
    editor->ed->pushModulationToUndoRedo(dest_id, add_ms, add_ms_sc, add_ms_idx,
                                         Surge::GUI::UndoManager::UNDO);
    synth->setModDepth01(dest_id, add_ms, add_ms_sc, add_ms_idx, 0.01);
    addSourceW->setLabels({"Select Source"});
    addTargetW->setLabels({"Select Target"});
    addTargetW->setEnabled(false);
    addSourceW->grabKeyboardFocus();
    repaint();
}

ModulationEditor::ModulationEditor(SurgeGUIEditor *ed, SurgeSynthesizer *s)
    : ed(ed), synth(s), OverlayComponent("Modulation Editor")
{
    modContents = std::make_unique<ModulationListContents>(this);
    modContents->rebuildFrom(synth);
    viewport = std::make_unique<juce::Viewport>();
    viewport->setViewedComponent(modContents.get(), false);
    viewport->setAccessible(true);

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

    sideControls = std::make_unique<ModulationSideControls>(this, ed);

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
    viewport->setScrollBarsShown(true, false);
    modContents->setViewportWidth(w - sideW);
    // modContents->setSize(vpr.getWidth() - viewport->getScrollBarThickness() - 1, 10);
    modContents->rebuildFrom(synth);
}

ModulationEditor::~ModulationEditor()
{
    synth->removeModulationAPIListener(this);
    needsModUpdate = false;
    needsModValueOnlyUpdate = false;
    idleTimer->stopTimer();
}

void ModulationEditor::rebuildContents()
{
    if (synth && modContents)
        modContents->rebuildFrom(synth);
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
