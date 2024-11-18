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

#include "PatchDBViewer.h"
#include "PatchDB.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "fmt/core.h"

namespace Surge
{
namespace Overlays
{

struct PatchDBFiltersDisplay : juce::Component
{
    PatchDBFiltersDisplay() {}
    struct Item : juce::Component
    {
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Item);
    };

    void paint(juce::Graphics &g) { g.fillAll(juce::Colours::orchid); }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchDBFiltersDisplay)
};

struct SharedTreeViewItem : public juce::TreeViewItem
{
    SharedTreeViewItem(SurgeGUIEditor *ed, SurgeStorage *s) : editor(ed), storage(s) {}
    int getItemHeight() const override { return 13; }

    SurgeStorage *storage;
    SurgeGUIEditor *editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTreeViewItem)
};
struct PatchDBSQLTreeViewItem : public SharedTreeViewItem
{
    struct TextSubItem : public SharedTreeViewItem
    {
        TextSubItem(SurgeGUIEditor *ed, SurgeStorage *s, const std::string &ts)
            : SharedTreeViewItem(ed, s), text(ts)
        {
        }
        bool mightContainSubItems() override { return false; }

        void paintItem(juce::Graphics &g, int width, int height) override
        {
            juce::TreeViewItem::paintItem(g, width, height);
            // g.setFont(skin->fontManager->getLatoAtSize(9));
            g.drawText(text, 2, 0, width - 2, height, juce::Justification::centredLeft);
        }

        std::string text;
    };

    struct DBCatSubItem : public SharedTreeViewItem
    {
        DBCatSubItem(SurgeGUIEditor *ed, SurgeStorage *s,
                     const Surge::PatchStorage::PatchDB::catRecord ts)
            : SharedTreeViewItem(ed, s), cat(std::move(ts))
        {
        }
        bool mightContainSubItems() override { return !cat.isleaf; }

        void paintItem(juce::Graphics &g, int width, int height) override
        {
            juce::TreeViewItem::paintItem(g, width, height);
            // g.setFont(skin->fontManager->getLatoAtSize(9));
            g.drawText(cat.leaf_name, 2, 0, width - 2, height, juce::Justification::centredLeft);
        }

        void itemOpennessChanged(bool isOpenNow) override
        {
            if (!isOpenNow)
            {
                while (getNumSubItems() > 0)
                    removeSubItem(0);
            }
            else
            {
                auto res = storage->patchDB->childCategoriesOf(cat.id);
                for (auto r : res)
                {
                    addSubItem(new DBCatSubItem(editor, storage, r));
                }
            }
        }

        Surge::PatchStorage::PatchDB::catRecord cat;
    };
    struct CategoryItem : public SharedTreeViewItem
    {
        CategoryItem(SurgeGUIEditor *ed, SurgeStorage *s, Surge::PatchStorage::PatchDB::CatType t)
            : SharedTreeViewItem(ed, s), type(t)
        {
        }
        bool mightContainSubItems() override { return true; }
        void paintItem(juce::Graphics &g, int width, int height) override
        {
            juce::TreeViewItem::paintItem(g, width, height);
            // g.setFont(skin->fontManager->getLatoAtSize(9));
            std::string text = "Factory";
            g.setColour(juce::Colours::black);
            if (type == PatchStorage::PatchDB::THIRD_PARTY)
                text = "Third Party";
            if (type == PatchStorage::PatchDB::USER)
                text = "User";
            g.drawText(text, 2, 0, width - 2, height, juce::Justification::centredLeft);
        }

        void itemOpennessChanged(bool isOpenNow) override
        {
            if (!isOpenNow)
            {
                while (getNumSubItems() > 0)
                    removeSubItem(0);
            }
            else
            {
                auto res = storage->patchDB->rootCategoriesForType(type);
                for (auto r : res)
                {
                    addSubItem(new DBCatSubItem(editor, storage, r));
                }
            }
        }

        Surge::PatchStorage::PatchDB::CatType type;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CategoryItem);
    };

    struct FeatureQueryItem : public SharedTreeViewItem
    {

        FeatureQueryItem(SurgeGUIEditor *ed, SurgeStorage *s, std::string feature, int type)
            : SharedTreeViewItem(ed, s), feature(feature), type(type)
        {
        }
        int type;
        std::string feature;

        bool mightContainSubItems() override { return true; }
        void paintItem(juce::Graphics &g, int width, int height) override
        {
            juce::TreeViewItem::paintItem(g, width, height);
            // g.setFont(skin->fontManager->getLatoAtSize(9));
            g.drawText(juce::CharPointer_UTF8(feature.c_str()), 2, 0, width - 2, height,
                       juce::Justification::centredLeft);
        }

        void itemOpennessChanged(bool isOpenNow) override
        {
            if (isOpenNow)
            {

                if (type == 1)
                {
                    auto res = storage->patchDB->readAllFeatureValueString(feature);
                    for (const auto &r : res)
                        addSubItem(new TextSubItem(editor, storage, r));
                }
                else
                {
                    auto res = storage->patchDB->readAllFeatureValueInt(feature);
                    for (auto r : res)
                        addSubItem(new TextSubItem(editor, storage, std::to_string(r)));
                }
            }
            else
            {
                while (getNumSubItems() > 0)
                    removeSubItem(0);
            }
        }
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FeatureQueryItem);
    };

    struct SpecialQueryItem : public SharedTreeViewItem
    {
        enum SpecialType
        {
            AUTHOR,
            TAG,
            FEATURE,
            FAVORITE
        } type;

        SpecialQueryItem(SurgeGUIEditor *ed, SurgeStorage *s, SpecialType st)
            : SharedTreeViewItem(ed, s), type(st)
        {
        }
        bool mightContainSubItems() override { return true; }
        void paintItem(juce::Graphics &g, int width, int height) override
        {
            juce::TreeViewItem::paintItem(g, width, height);
            // g.setFont(skin->fontManager->getLatoAtSize(9));
            std::string text = "";
            switch (type)
            {
            case AUTHOR:
                text = "By Author";
                break;
            case TAG:
                text = "By Tag";
                break;
            case FEATURE:
                text = "By Feature";
                break;
            case FAVORITE:
                text = "Favorites";
                break;
            }
            g.setColour(juce::Colours::black);
            g.drawText(text, 2, 0, width - 2, height, juce::Justification::centredLeft);
        }

        void itemOpennessChanged(bool isOpenNow) override
        {
            if (isOpenNow)
            {
                auto features = storage->patchDB->readAllFeatures();
                for (auto f : features)
                    addSubItem(new FeatureQueryItem(editor, storage, f.first, f.second));
            }
            else
            {
                while (getNumSubItems() > 0)
                    removeSubItem(0);
            }

            std::cout << type << " " << isOpenNow << std::endl;
        }
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SpecialQueryItem);
    };

    PatchDBSQLTreeViewItem(SurgeGUIEditor *ed, SurgeStorage *s) : SharedTreeViewItem(ed, s)
    {
        addSubItem(new CategoryItem(editor, storage, Surge::PatchStorage::PatchDB::FACTORY));
        addSubItem(new CategoryItem(editor, storage, Surge::PatchStorage::PatchDB::THIRD_PARTY));
        addSubItem(new CategoryItem(editor, storage, Surge::PatchStorage::PatchDB::USER));

        addSubItem(new SpecialQueryItem(editor, storage, SpecialQueryItem::AUTHOR));
        addSubItem(new SpecialQueryItem(editor, storage, SpecialQueryItem::TAG));
        addSubItem(new SpecialQueryItem(editor, storage, SpecialQueryItem::FEATURE));
        addSubItem(new SpecialQueryItem(editor, storage, SpecialQueryItem::FAVORITE));
    }

    bool mightContainSubItems() override { return true; }
    void paintItem(juce::Graphics &g, int width, int height) override
    {
        // TODO: Surge XT logo
        // g.setFont(skin->fontManager->getLatoAtSize(9));
        g.setColour(juce::Colours::black);
        g.drawText("Surge XT", 2, 0, width - 2, height, juce::Justification::centredLeft);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchDBSQLTreeViewItem);
};

class PatchDBSQLTableModel : public juce::TableListBoxModel
{
  public:
    PatchDBSQLTableModel(SurgeGUIEditor *ed, SurgeStorage *s) : editor(ed), storage(s) {}
    int getNumRows() override { return data.size(); }

    void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                            bool rowIsSelected) override
    {
        // this is obviously a dumb hack
        if (rowNumber % 6 < 3)
            g.fillAll(juce::Colour(170, 170, 200));
        else
            g.fillAll(juce::Colour(190, 190, 190));
    }

    void paintCell(juce::Graphics &g, int rowNumber, int columnId, int width, int height,
                   bool rowIsSelected) override
    {
        // FIXME - make sure the condition this is handling is handled everywhere consistently
        if (rowNumber >= data.size())
        {
            return;
        }

        g.setColour(juce::Colour(100, 100, 100));
        g.setColour(juce::Colour(0, 0, 0));
        auto d = data[rowNumber];
        auto s = std::to_string(d.id);
        switch (columnId)
        {
        case 2:
            s = d.name;
            break;
        case 3:
            s = d.cat;
            break;
        case 4:
            s = d.author;
            break;
        }
        // g.setFont(skin->fontManager->getLatoAtSize(9));
        g.drawText(s, 0, 0, width, height, juce::Justification::centredLeft);
    }

    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent &event) override
    {
        // FIXME - make sure the condition this is handling is handled everywhere consistently
        if (rowNumber >= data.size())
        {
            return;
        }

        auto d = data[rowNumber];
        editor->queuePatchFileLoad(d.file);
        editor->closeOverlay(SurgeGUIEditor::PATCH_BROWSER);
    }

    void executeQuery(const std::string &n) { data = storage->patchDB->rawQueryForNameLike(n); }

    std::vector<Surge::PatchStorage::PatchDB::patchRecord> data;
    SurgeStorage *storage;
    SurgeGUIEditor *editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchDBSQLTableModel);
};

PatchDBViewer::PatchDBViewer(SurgeGUIEditor *e, SurgeStorage *s)
    : editor(e), storage(s), OverlayComponent("PatchDB Viewer")
{
    storage->initializePatchDb();
    createElements();
}
PatchDBViewer::~PatchDBViewer()
{
    treeView->setRootItem(nullptr);
    if (countdownClock)
    {
        countdownClock->stopTimer();
    }
};

void PatchDBViewer::createElements()
{
    setSize(750, 450); // cleaner obvs
    tableModel = std::make_unique<PatchDBSQLTableModel>(editor, storage);
    table = std::make_unique<juce::TableListBox>("Patch Table", tableModel.get());
    table->getHeader().addColumn("id", 1, 40);
    table->getHeader().addColumn("name", 2, 200);
    table->getHeader().addColumn("category", 3, 250);
    table->getHeader().addColumn("author", 4, 200);

    table->setBounds(200, 50, getWidth() - 200, getHeight() - 50);
    table->setRowHeight(18);
    addAndMakeVisible(*table);

    nameTypein = std::make_unique<juce::TextEditor>("Patch Name");
    nameTypein->setBounds(10, 10, 400, 30);
    nameTypein->addListener(this);
    addAndMakeVisible(*nameTypein);

    treeView = std::make_unique<juce::TreeView>("Tree View for Categories");
    treeView->setColour(juce::TreeView::backgroundColourId, juce::Colours::white);
    treeView->setColour(juce::TreeView::evenItemsColourId, juce::Colour(200, 200, 255));
    treeView->setColour(juce::TreeView::oddItemsColourId, juce::Colour(240, 240, 255));
    treeView->setRootItemVisible(false);

    treeView->setBounds(0, 50, 200, getHeight() - 50);
    treeRoot = std::make_unique<PatchDBSQLTreeViewItem>(editor, storage);
    treeView->setRootItem(treeRoot.get());
    addAndMakeVisible(*treeView);

    auto tb = std::make_unique<juce::TextButton>();
    tb->setButtonText("Debug");
    doDebug = std::move(tb);
    doDebug->addListener(this);
    addAndMakeVisible(*doDebug);

    filters = std::make_unique<PatchDBFiltersDisplay>();
    addAndMakeVisible(*filters);

    jobCountdown = std::make_unique<juce::Label>();
    jobCountdown->setText("COUNTDOWN", juce::NotificationType::dontSendNotification);
    jobCountdown->setColour(juce::Label::backgroundColourId,
                            juce::Colour(0xFF, 0x90, 0).withAlpha(0.4f));
    jobCountdown->setJustificationType(juce::Justification::centred);
    // jobCountdown->setFont(skin->fontManager->getFiraMonoAtSize(36));
    addChildComponent(*jobCountdown);

    if (storage->patchDB->numberOfJobsOutstanding() > 0)
    {
        checkJobsOverlay();
    }
    else
    {
        executeQuery();
    }
}

struct CountdownTimer : public juce::Timer
{
    CountdownTimer(PatchDBViewer *d) : v(d) {}
    PatchDBViewer *v{nullptr};
    void timerCallback() override
    {
        if (v)
            v->checkJobsOverlay();
    }
};
void PatchDBViewer::checkJobsOverlay()
{
    auto jo = storage->patchDB->numberOfJobsOutstanding();
    if (jo == 0)
    {
        jobCountdown->setVisible(false);
        executeQuery();
        if (countdownClock)
        {
            countdownClock->stopTimer();
        }
    }
    else
    {
        jobCountdown->setText(fmt::format("Jobs Outstanding : {:d}", jo),
                              juce::NotificationType::dontSendNotification);
        jobCountdown->setVisible(true);
        if (!countdownClock)
        {
            countdownClock = std::make_unique<CountdownTimer>(this);
        }
        if (!countdownClock->isTimerRunning())
        {
            countdownClock->startTimerHz(30);
        }
    }
}
void PatchDBViewer::executeQuery()
{
    tableModel->executeQuery(nameTypein->getText().toStdString());
    table->updateContent();
}
void PatchDBViewer::textEditorTextChanged(juce::TextEditor &editor) { executeQuery(); }

void PatchDBViewer::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }
void PatchDBViewer::resized()
{
    if (jobCountdown)
        jobCountdown->setBounds(getLocalBounds());

    if (nameTypein)
        nameTypein->setBounds(10, 10, 400, 30);

    if (doDebug)
        doDebug->setBounds(420, 10, 100, 30);

    if (filters)
        filters->setBounds(200, 50, getWidth() - 202, 48);

    if (table)
        table->setBounds(200, 100, getWidth() - 202, getHeight() - 102);

    if (treeView)
        treeView->setBounds(2, 50, 196, getHeight() - 52);
}
void PatchDBViewer::buttonClicked(juce::Button *button)
{
    if (button == doDebug.get())
    {
        storage->patchDB->addDebugMessage(std::string("Debugging with ") + std::to_string(rand()));
    }
}

} // namespace Overlays
} // namespace Surge