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
#include "PatchSelector.h"
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

#include "PatchSelector.h"
#include "SurgeStorage.h"
#include "SurgeGUIUtils.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "UserDefaults.h"
#include "widgets/MenuCustomComponents.h"
#include "overlays/PatchStoreDialog.h"
#include "PatchDB.h"
#include "fmt/core.h"
#include "SurgeJUCEHelpers.h"
#include "AccessibleHelpers.h"

/*
 * It is an arbitrary number that we set as an ID for patch menu items.
 * It is not necessarily to be unique among all menu items, only among a sub menu, so it can
 * be a constant.
 */
static constexpr int ID_TO_PRESELECT_MENU_ITEMS = 636133;

namespace Surge
{
namespace Widgets
{

struct PatchDBTypeAheadProvider : public TypeAheadDataProvider,
                                  public Surge::GUI::SkinConsumingComponent
{
    SurgeStorage *storage{nullptr};
    PatchSelector *selector{nullptr};
    PatchDBTypeAheadProvider(PatchSelector *s) : selector(s) {}

    std::vector<PatchStorage::PatchDB::patchRecord> lastSearchResult;
    std::vector<int> searchFor(const std::string &s) override
    {
        lastSearchResult = storage->patchDB->queryFromQueryString(s);
        std::vector<int> res(lastSearchResult.size());
        std::iota(res.begin(), res.end(), 0);
        selector->searchUpdated();
        return res;
    }
    std::string textBoxValueForIndex(int idx) override
    {
        if (idx >= 0 && idx < lastSearchResult.size())
            return lastSearchResult[idx].name;
        return "<<ERROR>>";
    }

    std::string accessibleTextForIndex(int idx) override
    {
        if (idx >= 0 && idx < lastSearchResult.size())
            return lastSearchResult[idx].name + " in " + lastSearchResult[idx].cat;
        return "<<ERROR>>";
    }

    int getRowHeight() override { return 23; }
    int getDisplayedRows() override { return 12; }

    juce::Colour rowBg{juce::Colours::white}, hlRowBg{juce::Colours::lightblue},
        rowText{juce::Colours::black}, hlRowText{juce::Colours::red},
        rowSubText{juce::Colours::orange}, hlRowSubText{juce::Colours::hotpink},
        divider{juce::Colours::orchid};

    void paintDataItem(int searchIndex, juce::Graphics &g, int width, int height,
                       bool rowIsSelected) override
    {
        g.fillAll(rowBg);
        if (rowIsSelected)
        {
            auto r = juce::Rectangle<int>(0, 0, width, height).reduced(2, 2);
            g.setColour(hlRowBg);
            g.fillRect(r);
        }

        g.setFont(skin->fontManager->getLatoAtSize(12));
        if (searchIndex >= 0 && searchIndex < lastSearchResult.size())
        {
            auto pr = lastSearchResult[searchIndex];
            auto r = juce::Rectangle<int>(4, 0, width - 8, height - 1);
            if (rowIsSelected)
                g.setColour(hlRowText);
            else
                g.setColour(rowText);
            g.drawText(pr.name, r, juce::Justification::centredTop);

            if (rowIsSelected)
                g.setColour(hlRowSubText);
            else
                g.setColour(rowSubText);
            g.setFont(skin->fontManager->getLatoAtSize(8));
            g.drawText(pr.cat, r, juce::Justification::bottomLeft);
            g.drawText(pr.author, r, juce::Justification::bottomRight);
        }
        g.setColour(divider);
        g.drawLine(4, height - 1, width - 4, height - 1, 1);
    }

    void paintOverChildren(juce::Graphics &g, const juce::Rectangle<int> &bounds) override
    {
        auto q = bounds.reduced(2, 2);
        auto res = lastSearchResult.size();
        std::string txt;

        if (res <= 0)
        {
            txt = "No results";
        }
        else
        {
            txt = fmt::format("{:d} result{:s}", res, (res > 1 ? "s" : ""));
        }

        g.setColour(rowText.withAlpha(0.666f));
        g.setFont(skin->fontManager->getLatoAtSize(7, juce::Font::bold));
        g.drawText(txt, q, juce::Justification::topLeft);
    }
};

struct PatchSelector::TB : juce::Component
{
    SurgeStorage *storage{nullptr};
    TB(const std::string &s)
    {
        setAccessible(true);
        setWantsKeyboardFocus(true);
        setTitle(s);
        setDescription(s);
    }
    void paint(juce::Graphics &g) override
    {
        /* Useful for debugging
        g.setColour(juce::Colours::blue);
        g.drawRect(getLocalBounds(), 1);
         */
    }

    void mouseDown(const juce::MouseEvent &e) override
    {
        if (e.mods.isPopupMenu() && onMenu())
        {
            return;
        }
        onPress();
    }

    void mouseEnter(const juce::MouseEvent &e) override { onEnterExit(true); }

    void mouseExit(const juce::MouseEvent &e) override { onEnterExit(false); }

    bool keyPressed(const juce::KeyPress &e) override
    {
        auto [action, mod] = Surge::Widgets::accessibleEditAction(e, storage);
        if (action == OpenMenu)
        {
            return onMenu();
        }
        if (action == Return)
        {
            return onPress();
        }
        return false;
    }

    std::function<void(bool)> onEnterExit = [](auto b) {};
    std::function<bool()> onMenu = []() { return false; }, onPress = []() { return false; };

    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override
    {
        return std::make_unique<juce::AccessibilityHandler>(
            *this, juce::AccessibilityRole::button,
            juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,
                                                   [this]() {}));
    }
};

PatchSelector::PatchSelector() : juce::Component(), WidgetBaseMixin<PatchSelector>(this)
{
    patchDbProvider = std::make_unique<PatchDBTypeAheadProvider>(this);
    typeAhead = std::make_unique<Surge::Widgets::TypeAhead>("patch select", patchDbProvider.get());
    typeAhead->setVisible(false);
    typeAhead->addTypeAheadListener(this);
    typeAhead->setToElementZeroOnReturn = true;

    addChildComponent(*typeAhead);

    searchButton = std::make_unique<TB>("Open Search DB");
    searchButton->onEnterExit = [w = juce::Component::SafePointer(this)](bool b) {
        if (w)
        {
            w->searchHover = b;
            w->favoritesHover = false;
            w->repaint();
        }
    };
    searchButton->onPress = [w = juce::Component::SafePointer(this)]() {
        if (w)
        {
            if (!w->storage->userDataPathValid)
            {
                auto sge = w->firstListenerOfType<SurgeGUIEditor>();
                if (sge)
                    sge->messageBox("Search is not available without a writable user dir",
                                    "Search Not Available");
            }
            else
            {
                w->typeaheadButtonPressed();
            }
        }
        return true;
    };
    addAndMakeVisible(*searchButton);

    favoriteButton = std::make_unique<TB>("Favorites");
    addAndMakeVisible(*favoriteButton);
    favoriteButton->onEnterExit = [w = juce::Component::SafePointer(this)](bool b) {
        if (w)
        {
            w->searchHover = false;
            w->favoritesHover = b;
            w->repaint();
        }
    };
    favoriteButton->onPress = [w = juce::Component::SafePointer(this)]() {
        if (w)
        {
            if (!w->storage->userDataPathValid)
            {
                auto sge = w->firstListenerOfType<SurgeGUIEditor>();
                if (sge)
                    sge->messageBox("Favorites are not available without a writable user dir",
                                    "Favorites Not Available");
            }
            else
            {
                w->toggleFavoriteStatus();
            }
        }
        return true;
    };
    favoriteButton->onMenu = [w = juce::Component::SafePointer(this)]() {
        if (w)
        {
            w->showFavoritesMenu();
        }
        return true;
    };

    setWantsKeyboardFocus(true);
};
PatchSelector::~PatchSelector() { typeAhead->removeTypeAheadListener(this); };

void PatchSelector::setStorage(SurgeStorage *s)
{
    storage = s;
    storage->patchDB->initialize();
    patchDbProvider->storage = s;
    searchButton.get()->storage = s;
    favoriteButton.get()->storage = s;
}

void PatchSelector::paint(juce::Graphics &g)
{
    auto pbrowser = getLocalBounds();

    auto cat = getLocalBounds().withTrimmedLeft(3).withWidth(150).withHeight(getHeight() * 0.5);
    auto auth = getLocalBounds();

    if (skin->getVersion() >= 2)
    {
        cat = cat.translated(0, getHeight() * 0.5);
        auth = auth.withTrimmedRight(3)
                   .withWidth(150)
                   .translated(getWidth() - 150 - 3, 0)
                   .withTop(cat.getY())
                   .withHeight(cat.getHeight());
    }
    else
    {
        auth = cat;
        auth = auth.translated(0, pbrowser.getHeight() / 2);

        cat = cat.translated(0, 1);
        auth = auth.translated(0, -1);
    }

    // favorites rect
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(favoritesRect);
        auto img = associatedBitmapStore->getImage(IDB_FAVORITE_BUTTON);
        int yShift = 13 * ((favoritesHover ? 1 : 0) + (isFavorite ? 2 : 0));
        img->drawAt(g, favoritesRect.getX(), favoritesRect.getY() - yShift, 1.0);
    }

    // search rect
    {
        juce::Graphics::ScopedSaveState gs(g);
        g.reduceClipRegion(searchRect);
        auto img = associatedBitmapStore->getImage(IDB_SEARCH_BUTTON);
        int yShift = 13 * ((searchHover ? 1 : 0) + (isTypeaheadSearchOn ? 2 : 0));
        img->drawAt(g, searchRect.getX(), searchRect.getY() - yShift, 1.0);
    }

    // patch name
    if (!isTypeaheadSearchOn)
    {
        auto catsz = skin->fontManager->displayFont.getStringWidthFloat(category);
        auto authsz = skin->fontManager->displayFont.getStringWidthFloat(author);

        auto catszwith =
            skin->fontManager->displayFont.getStringWidthFloat("Category: " + category);
        auto authszwith = skin->fontManager->displayFont.getStringWidthFloat("By: " + author);
        auto mainsz = skin->fontManager->patchNameFont.getStringWidthFloat(pname);

        bool useCatAndBy{false}, alignTop{false};

        /*
         * Would the main text overlap with the category or author if we add "Category: " or "By: "?
         * That's the same as if the category with + half the size is less that the width / 2.
         * Add a 3 px extra buffer too just to stop exact hits
         */
        if ((catszwith + mainsz / 2) < getWidth() / 2 - 3 &&
            (authszwith + mainsz / 2) < getWidth() / 2 - 3)
        {
            useCatAndBy = true;
        }

        /*
         * Alternately if the category without and the name clash then the name will
         * draw fitted but since the rectangles overlap we need to push it to the top
         * of the box
         */
        if ((catsz + mainsz / 2) > getWidth() / 2 || (authsz + mainsz / 2) > getWidth() / 2)
        {
            alignTop = true;
        }

        auto pnRect =
            pbrowser.withLeft(searchRect.getRight()).withRight(favoritesRect.getX()).reduced(4, 0);

        g.setFont(skin->fontManager->patchNameFont);

        auto uname = pname;

        if (isDirty)
        {
            uname += "*";
        }

        juce::Colour tc = skin->getColor(Colors::PatchBrowser::TextHover);

        if (!browserHover || tc == juce::Colours::transparentBlack)
        {
            tc = skin->getColor(Colors::PatchBrowser::Text);
        }

        g.setColour(tc);

        g.drawFittedText(uname, pnRect,
                         alignTop ? juce::Justification::centredTop : juce::Justification::centred,
                         1, 0.1);

        // category/author name
        g.setFont(skin->fontManager->displayFont);

        g.drawText((useCatAndBy ? "Category: " : "") + category, cat,
                   juce::Justification::centredLeft);
        g.drawText((useCatAndBy ? "By: " : "") + author, auth,
                   skin->getVersion() >= 2 ? juce::Justification::centredRight
                                           : juce::Justification::centredLeft);
    }
}

void PatchSelector::resized()
{
    auto fsize = 15;

    favoritesRect = getLocalBounds()
                        .withTrimmedBottom(getHeight() - fsize)
                        .withTrimmedLeft(getWidth() - fsize)
                        .reduced(1, 1)
                        .translated(-2, 1);
    favoriteButton->setBounds(favoritesRect);
    searchRect = getLocalBounds()
                     .withTrimmedBottom(getHeight() - fsize)
                     .withWidth(fsize)
                     .reduced(1, 1)
                     .translated(2, 1);
    searchButton->setBounds(searchRect);

    auto tad = getLocalBounds().reduced(fsize + 4, 0).translated(0, -2);

    typeAhead->setBounds(tad);
}

void PatchSelector::mouseEnter(const juce::MouseEvent &)
{
    browserHover = true;
    repaint();

    if (tooltipCountdown < 0)
    {
        tooltipCountdown = 3;
        juce::Timer::callAfterDelay(100, Surge::GUI::makeSafeCallback<PatchSelector>(
                                             this, [](auto *that) { that->shouldTooltip(); }));
    }
}

void PatchSelector::mouseMove(const juce::MouseEvent &e)
{
    if (tooltipCountdown >= 0)
    {
        tooltipCountdown = 3;
    }

    // todo : apply mouse tolerance here
    if (tooltipShowing && e.position.getDistanceFrom(tooltipMouseLocation.toFloat()) > 1)
    {
        toggleCommentTooltip(false);
    }
    else
    {
        tooltipMouseLocation = e.position;
    }

    auto pfh = favoritesHover;
    favoritesHover = false;

    if (favoritesRect.contains(e.position.toInt()))
    {
        browserHover = false;
        favoritesHover = true;
    }

    auto psh = searchHover;
    searchHover = false;

    if (searchRect.contains(e.position.toInt()))
    {
        browserHover = false;
        searchHover = true;
    }

    if (!favoritesHover && !searchHover && !browserHover)
    {
        browserHover = true;
        repaint();
    }

    if (favoritesHover != pfh || searchHover != psh)
    {
        repaint();
    }
}

void PatchSelector::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isMiddleButtonDown())
    {
        notifyControlModifierClicked(e.mods);
        return;
    }

    if (favoritesRect.contains(e.position.toInt()))
    {
        if (e.mods.isPopupMenu())
        {
            showFavoritesMenu();
            return;
        }
        else
        {
            toggleFavoriteStatus();
        }
        return;
    }

    if (e.mods.isShiftDown() || searchRect.contains(e.position.toInt()))
    {
        typeaheadButtonPressed();
        return;
    }

    tooltipCountdown = -1;
    toggleCommentTooltip(false);

    stuckHover = true;
    showClassicMenu(e.mods.isPopupMenu(), e.mods.isCommandDown());
}

void PatchSelector::shouldTooltip()
{
    if (tooltipCountdown < 0 || typeAhead->isVisible())
    {
        return;
    }

    tooltipCountdown--;

    if (tooltipCountdown == 0)
    {
        tooltipCountdown = -1;
        toggleCommentTooltip(true);
    }
    else
    {
        juce::Timer::callAfterDelay(100, Surge::GUI::makeSafeCallback<PatchSelector>(
                                             this, [](auto *that) { that->shouldTooltip(); }));
    }
}

void PatchSelector::toggleCommentTooltip(bool b)
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        if (b && !comment.empty())
        {
            tooltipShowing = true;
            sge->showPatchCommentTooltip(comment);
        }
        else
        {
            tooltipShowing = false;
            sge->hidePatchCommentTooltip();
        }
    }
}

void PatchSelector::openPatchBrowser()
{
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        sge->showOverlay(SurgeGUIEditor::PATCH_BROWSER);
    }
}
void PatchSelector::showClassicMenu(bool single_category, bool userOnly)
{
    auto contextMenu = juce::PopupMenu();
    int main_e = 0;
    bool has_3rdparty = false;
    int last_category = current_category;
    auto patch_cat_size = storage->patch_category.size();
    int tutorialCat = -1, midiPCCat = -1;

    bool anyUser = false;
    for (auto &c : storage->patch_category)
    {
        if (!c.isFactory && c.numberOfPatchesInCategoryAndChildren)
        {
            anyUser = true;
        }
    }
    if (userOnly && !anyUser)
    {
        userOnly = false;
    }

    if (single_category)
    {
        /*
        ** in the init scenario we don't have a category yet. Our two choices are
        ** don't pop up the menu or pick one. I choose to pick one. If I can
        ** find the one called "Init" use that. Otherwise pick 0.
        */
        int rightMouseCategory = current_category;

        if (current_category < 0)
        {
            if (storage->patchCategoryOrdering.size() == 0)
            {
                return;
            }

            for (auto c : storage->patchCategoryOrdering)
            {
                if (_stricmp(storage->patch_category[c].name.c_str(), "Init") == 0)
                {
                    rightMouseCategory = c;
                }
            }

            if (rightMouseCategory < 0)
            {
                rightMouseCategory = storage->patchCategoryOrdering[0];
            }
        }

        // get just the category name and not the path leading to it
        std::string menuName = storage->patch_category[rightMouseCategory].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != std::string::npos)
        {
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);
        }

        std::transform(menuName.begin(), menuName.end(), menuName.begin(), ::toupper);

        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(
            contextMenu, "PATCHES (" + menuName + ")");

        populatePatchMenuForCategory(rightMouseCategory, contextMenu, single_category, main_e,
                                     false);
    }
    else
    {
        bool addedFavorites = false;
        if (!userOnly && patch_cat_size && storage->firstThirdPartyCategory > 0)
        {
            Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                            "FACTORY PATCHES");
        }

        for (int i = (userOnly ? storage->firstUserCategory : 0); i < patch_cat_size; i++)
        {
            if (i == storage->firstThirdPartyCategory || i == storage->firstUserCategory)
            {
                std::string txt;
                bool favs = false;

                if (i == storage->firstThirdPartyCategory && storage->firstUserCategory != i)
                {
                    txt = "THIRD PARTY PATCHES";
                    contextMenu.addColumnBreak();
                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                    txt);
                }
                else if (anyUser)
                {
                    favs = true;
                    txt = "USER PATCHES";
                    contextMenu.addColumnBreak();
                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu,
                                                                                    txt);
                }

                if (favs && optionallyAddFavorites(contextMenu, false))
                {
                    contextMenu.addSeparator();
                    addedFavorites = true;
                }
            }

            // remap index to the corresponding category in alphabetical order.
            int c = storage->patchCategoryOrdering[i];

            if (storage->patch_category[c].isFactory &&
                storage->patch_category[c].name == "Tutorials")
            {
                tutorialCat = c;
            }
            else if (!storage->patch_category[c].isFactory &&
                     storage->patch_category[c].name == storage->midiProgramChangePatchesSubdir)
            {
                midiPCCat = c;
            }
            else
            {
                populatePatchMenuForCategory(c, contextMenu, single_category, main_e, true);
            }
        }

        if (!addedFavorites)
        {
            optionallyAddFavorites(contextMenu, true);
        }
    }

    if (midiPCCat >= 0 &&
        storage->patch_category[midiPCCat].numberOfPatchesInCategoryAndChildren > 0)
    {
        contextMenu.addSeparator();
        populatePatchMenuForCategory(midiPCCat, contextMenu, single_category, main_e, true);
    }

    contextMenu.addColumnBreak();
    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(contextMenu, "FUNCTIONS");

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    contextMenu.addItem(Surge::GUI::toOSCase("Initialize Patch"), [this]() { loadInitPatch(); });

    contextMenu.addItem(Surge::GUI::toOSCase("Set Current Patch as Default"), [this]() {
        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchName,
                                               storage->patch_list[current_patch].name);

        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchCategory,
                                               storage->patch_category[current_category].name);

        Surge::Storage::updateUserDefaultValue(
            storage, Surge::Storage::InitialPatchCategoryType,
            storage->patch_category[current_category].isFactory ? "Factory" : "User");

        storage->initPatchName = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::InitialPatchName, "Init Saw");
        storage->initPatchCategory = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::InitialPatchCategory, "Templates");
        storage->initPatchCategoryType = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::InitialPatchCategoryType, "Factory");
    });

    contextMenu.addSeparator();

    if (sge)
    {
        Surge::GUI::addMenuWithShortcut(
            contextMenu, Surge::GUI::toOSCase("Save Patch"),
            sge->showShortcutDescription("Ctrl + S", u8"\U00002318S"),
            [this, sge]() { sge->showOverlay(SurgeGUIEditor::SAVE_PATCH); });

        contextMenu.addItem(Surge::GUI::toOSCase("Load Patch from File..."), [this, sge]() {
            auto patchPath = storage->userPatchesPath;

            patchPath = Surge::Storage::getUserDefaultPath(storage, Surge::Storage::LastPatchPath,
                                                           patchPath);

            sge->fileChooser = std::make_unique<juce::FileChooser>(
                "Select Patch to Load", juce::File(path_to_string(patchPath)), "*.fxp");

            sge->fileChooser->launchAsync(
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [this, patchPath, sge](const juce::FileChooser &c) {
                    sge->undoManager()->pushPatch();
                    auto ress = c.getResults();
                    if (ress.size() != 1)
                        return;

                    auto res = c.getResult();
                    auto rString = res.getFullPathName().toStdString();

                    std::cout << "queuePatchFileLoad: " << rString << std::endl;
                    sge->queuePatchFileLoad(rString);

                    auto dir =
                        string_to_path(res.getParentDirectory().getFullPathName().toStdString());

                    if (dir != patchPath)
                    {
                        Surge::Storage::updateUserDefaultPath(storage,
                                                              Surge::Storage::LastPatchPath, dir);
                    }
                });
        });

        contextMenu.addSeparator();

        if (isUser)
        {
            contextMenu.addItem(Surge::GUI::toOSCase("Rename Patch"), [this, sge]() {
                sge->showOverlay(
                    SurgeGUIEditor::SAVE_PATCH, [this](Overlays::OverlayComponent *co) {
                        auto psd = dynamic_cast<Surge::Overlays::PatchStoreDialog *>(co);
                        if (!psd)
                            return;
                        psd->setIsRename(true);
                        psd->setEnclosingParentTitle("Rename Patch");
                        const auto priorPath = storage->patch_list[current_patch].path;
                        psd->onOK = [this, priorPath]() {
                            /*
                             * OK so the database doesn't like deleting files while it is indexing.
                             * We should fix this (#6793) but for now put the delete action at the
                             * end of the db processing thread. BUT that will run on the patchdb
                             * thread so bounce it from here to there and then back here.
                             */
                            auto nextStep = [this, priorPath]() {
                                auto doDelete = [this, priorPath]() {
                                    fs::remove(priorPath);
                                    storage->refresh_patchlist();
                                    storage->initializePatchDb(true);
                                };
                                juce::MessageManager::getInstance()->callAsync(doDelete);
                            };
                            storage->patchDB->doAfterCurrentQueueDrained(nextStep);
                        };
                    });
            });

            contextMenu.addItem(Surge::GUI::toOSCase("Delete Patch"), [this, sge]() {
                auto onOk = [this]() {
                    try
                    {
                        fs::remove(storage->patch_list[current_patch].path);
                        storage->refresh_patchlist();
                        storage->initializePatchDb(true);
                    }
                    catch (const fs::filesystem_error &e)
                    {
                        std::ostringstream oss;
                        oss << "Experienced filesystem error while deleting patch " << e.what();
                        storage->reportError(oss.str(), "Filesystem Error");
                    }
                    isUser = false;
                };

                sge->alertOKCancel("Delete Patch",
                                   std::string("Do you really want to delete\n") +
                                       storage->patch_list[current_patch].path.u8string() +
                                       "?\nThis cannot be undone!",
                                   onOk);
            });
        }

        contextMenu.addSeparator();

#if INCLUDE_PATCH_BROWSER
        Surge::GUI::addMenuWithShortcut(
            contextMenu, Surge::GUI::toOSCase("Patch Database..."),
            sge->showShortcutDescription("Alt + P", u8"\U00002325P"),
            [this, sge]() { sge->showOverlay(SurgeGUIEditor::PATCH_BROWSER); });
#endif
    }

    contextMenu.addItem(Surge::GUI::toOSCase("Refresh Patch Browser"),
                        [this]() { this->storage->refresh_patchlist(); });

    contextMenu.addSeparator();

    if (current_patch >= 0 && current_patch < storage->patch_list.size() &&
        storage->patch_list[current_patch].category >= storage->firstUserCategory)
    {
        Surge::GUI::addRevealFile(contextMenu, storage->patch_list[current_patch].path);
    }

    contextMenu.addItem(Surge::GUI::toOSCase("Open User Patches Folder..."),
                        [this]() { Surge::GUI::openFileOrFolder(this->storage->userPatchesPath); });

    contextMenu.addItem(Surge::GUI::toOSCase("Open Factory Patches Folder..."), [this]() {
        Surge::GUI::openFileOrFolder(this->storage->datapath / "patches_factory");
    });

    contextMenu.addItem(Surge::GUI::toOSCase("Open Third Party Patches Folder..."), [this]() {
        Surge::GUI::openFileOrFolder(this->storage->datapath / "patches_3rdparty");
    });

    contextMenu.addSeparator();

    if (tutorialCat >= 0)
    {
        populatePatchMenuForCategory(tutorialCat, contextMenu, single_category, main_e, true);
    }

    if (sge)
    {
        contextMenu.addSeparator();

        auto hu = sge->helpURLForSpecial("patch-browser");
        auto lurl = hu;

        if (hu != "")
        {
            lurl = sge->fullyResolvedHelpURL(hu);
        }

        auto hmen = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>("Patch Browser", lurl);

        hmen->setSkin(skin, associatedBitmapStore);
        hmen->setCentered(false);
        auto hment = hmen->getTitle();
        contextMenu.addCustomItem(-1, std::move(hmen), nullptr, hment);
    }

    auto o = juce::PopupMenu::Options();

    if (sge)
    {
        o = sge->popupMenuOptions(getBounds().getBottomLeft())
                .withInitiallySelectedItem(ID_TO_PRESELECT_MENU_ITEMS);
    }

    contextMenu.showMenuAsync(o, [that = juce::Component::SafePointer(this)](int) {
        if (that)
        {
            that->stuckHover = false;
            that->endHover();
        }
    });
}

bool PatchSelector::optionallyAddFavorites(juce::PopupMenu &p, bool addColumnBreak,
                                           bool addToSubMenu)
{
    std::vector<std::pair<int, Patch>> favs;
    int i = 0;

    for (auto p : storage->patch_list)
    {
        if (p.isFavorite)
        {
            favs.emplace_back(i, p);
        }

        i++;
    }

    if (favs.empty())
    {
        return false;
    }

    std::sort(favs.begin(), favs.end(),
              [](const auto &a, const auto &b) { return a.second.name < b.second.name; });

    if (addColumnBreak)
    {
        p.addColumnBreak();
        Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(p, "FAVORITES");
    }

    if (addToSubMenu)
    {
        auto subMenu = juce::PopupMenu();

        for (auto f : favs)
        {
            subMenu.addItem(juce::CharPointer_UTF8(f.second.name.c_str()),
                            [this, f]() { this->loadPatch(f.first); });
        }

        subMenu.addSeparator();
        subMenu.addItem(Surge::GUI::toOSCase("Export favorites to..."),
                        [this]() { exportFavorites(); });
        subMenu.addItem(Surge::GUI::toOSCase("Load favorites from..."),
                        [this]() { importFavorites(); });

        p.addSubMenu("Favorites", subMenu);
    }
    else
    {
        for (auto f : favs)
        {
            p.addItem(juce::CharPointer_UTF8(f.second.name.c_str()),
                      [this, f]() { this->loadPatch(f.first); });
        }
        p.addSeparator();
        p.addItem(Surge::GUI::toOSCase("Export favorites to..."), [this]() { exportFavorites(); });
        p.addItem(Surge::GUI::toOSCase("Load favorites from..."), [this]() { importFavorites(); });
    }

    return true;
}

void PatchSelector::exportFavorites()
{
    auto favoritesCallback = [this](const juce::FileChooser &c) {
        auto isSubDir = [](auto p, auto root) {
            while (p != fs::path() && p != p.parent_path())
            {
                if (p == root)
                {
                    return true;
                }
                p = p.parent_path();
            }
            return false;
        };

        auto result = c.getResults();

        if (result.isEmpty() || result.size() > 1)
        {
            return;
        }

        auto fsp = fs::path{result[0].getFullPathName().toStdString()};
        fsp = fsp.replace_extension(".surgefav");

        std::ofstream ofs(fsp);

        for (auto p : storage->patch_list)
        {
            if (p.isFavorite)
            {
                auto q = p.path;
                if (isSubDir(q, storage->datapath))
                {
                    q = q.lexically_relative(storage->datapath);
                    ofs << "FACTORY:" << q.u8string() << std::endl;
                }
                else if (isSubDir(q, storage->userPatchesPath))
                {
                    q = q.lexically_relative(storage->userPatchesPath);
                    ofs << "USER:" << q.u8string() << std::endl;
                }
            }
        }
        ofs.close();
    };
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (!sge)
        return;
    sge->fileChooser =
        std::make_unique<juce::FileChooser>("Export Favorites", juce::File(), "*.surgefav");
    sge->fileChooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                      juce::FileBrowserComponent::canSelectFiles |
                                      juce::FileBrowserComponent::warnAboutOverwriting,
                                  favoritesCallback);
}

void PatchSelector::importFavorites()
{
    auto importCallback = [this](const juce::FileChooser &c) {
        auto result = c.getResults();

        if (result.isEmpty() || result.size() > 1)
        {
            return;
        }

        auto fsp = fs::path{result[0].getFullPathName().toStdString()};
        fsp = fsp.replace_extension(".surgefav");

        std::ifstream ifs(fsp);

        std::set<fs::path> imports;

        for (std::string line; getline(ifs, line);)
        {
            if (line.find("FACTORY:") == 0)
            {
                auto q = storage->datapath / fs::path(line.substr(std::string("FACTORY:").size()));
                imports.insert(q);
            }
            else if (line.find("USER:") == 0)
            {
                auto q =
                    storage->userPatchesPath / fs::path(line.substr(std::string("USER:").size()));
                imports.insert(q);
            }
        }

        int i = 0;
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        if (!sge)
            return;
        bool refresh = false;
        for (auto p : storage->patch_list)
        {
            if (!p.isFavorite && imports.find(p.path) != imports.end())
            {
                sge->setSpecificPatchAsFavorite(i, true);
                refresh = true;
            }
            i++;
        }

        if (refresh)
            sge->queue_refresh = true;

        ifs.close();
    };
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (!sge)
        return;
    sge->fileChooser =
        std::make_unique<juce::FileChooser>("Import Favorites", juce::File(), "*.surgefav");
    sge->fileChooser->launchAsync(juce::FileBrowserComponent::canSelectFiles, importCallback);
}

bool PatchSelector::populatePatchMenuForCategory(int c, juce::PopupMenu &contextMenu,
                                                 bool single_category, int &main_e, bool rootCall)
{
    bool amIChecked = false;
    PatchCategory cat = storage->patch_category[c];

    // stop it going in the top menu which is a straight iteration
    if (rootCall && !cat.isRoot)
    {
        return false;
    }

    // don't do empty categories
    if (cat.numberOfPatchesInCategoryAndChildren == 0)
    {
        return false;
    }

    int splitcount = 256;

    // Go through the whole patch list in alphabetical order and filter
    // out only the patches that belong to the current category.
    std::vector<int> ctge;

    for (auto p : storage->patchOrdering)
    {
        if (storage->patch_list[p].category == c)
        {
            ctge.push_back(p);
        }
    }

    // Divide categories with more entries than splitcount into subcategories f.ex. bass (1, 2) etc
    int n_subc = 1 + (std::max(2, (int)ctge.size()) - 1) / splitcount;

    for (int subc = 0; subc < n_subc; subc++)
    {
        std::string name;
        juce::PopupMenu availMenu;
        juce::PopupMenu *subMenu;

        if (single_category)
        {
            subMenu = &contextMenu;
        }
        else
        {
            subMenu = &availMenu;
        }

        int sub = 0;

        for (int i = subc * splitcount; i < std::min((subc + 1) * splitcount, (int)ctge.size());
             i++)
        {
            int p = ctge[i];

            name = storage->patch_list[p].name;

            bool thisCheck = false;

            if (p == current_patch)
            {
                thisCheck = true;
                amIChecked = true;
            }

            bool isFav = storage->patch_list[p].isFavorite;
            auto item = juce::PopupMenu::Item(name).setEnabled(true).setTicked(thisCheck).setAction(
                [this, p]() { this->loadPatch(p); });

            if (thisCheck)
                item.setID(ID_TO_PRESELECT_MENU_ITEMS);

            if (isFav && associatedBitmapStore)
            {
                auto img = associatedBitmapStore->getImage(IDB_FAVORITE_MENU_ICON);
                if (img && img->getDrawableButUseWithCaution())
                    item.setImage(img->getDrawableButUseWithCaution()->createCopy());
            }
            subMenu->addItem(item);
            sub++;

            if (sub != 0 && sub % 32 == 0)
            {
                subMenu->addColumnBreak();

                if (single_category)
                {
                    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(*subMenu, "");
                }
            }
        }

        for (auto &childcat : cat.children)
        {
            // this isn't the best approach but it works
            int idx = 0;

            for (auto &cc : storage->patch_category)
            {
                if (cc.name == childcat.name && cc.internalid == childcat.internalid)
                {
                    break;
                }

                idx++;
            }

            bool checkedKid = populatePatchMenuForCategory(idx, *subMenu, false, main_e, false);

            if (checkedKid)
            {
                amIChecked = true;
            }
        }

        // get just the category name and not the path leading to it
        std::string menuName = storage->patch_category[c].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != std::string::npos)
        {
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);
        }

        if (n_subc > 1)
        {
            name = fmt::format("{} {}", menuName, subc + 1).c_str();
        }
        else
        {
            name = menuName.c_str();
        }

        if (!single_category)
        {
            if (amIChecked)
                contextMenu.addSubMenu(name, *subMenu, true, nullptr, amIChecked,
                                       ID_TO_PRESELECT_MENU_ITEMS);
            else
                contextMenu.addSubMenu(name, *subMenu, true, nullptr, amIChecked);
        }

        main_e++;
    }

    return amIChecked;
}

void PatchSelector::loadPatch(int id)
{
    if (id >= 0)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();
        if (sge)
            sge->undoManager()->pushPatch();

        enqueue_sel_id = id;
        notifyValueChanged();
    }
}

void PatchSelector::loadInitPatch()
{
    int i = 0;
    bool lookingForFactory = (storage->initPatchCategoryType == "Factory");

    for (auto p : storage->patch_list)
    {
        if (p.name == storage->initPatchName &&
            storage->patch_category[p.category].name == storage->initPatchCategory &&
            storage->patch_category[p.category].isFactory == lookingForFactory)
        {
            loadPatch(i);
            break;
        }

        ++i;
    }
}

int PatchSelector::getCurrentPatchId() const { return current_patch; }

int PatchSelector::getCurrentCategoryId() const { return current_category; }

void PatchSelector::itemSelected(int providerIndex)
{
    auto sr = patchDbProvider->lastSearchResult[providerIndex];
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    toggleTypeAheadSearch(false);
    if (sge)
    {
        sge->queuePatchFileLoad(sr.file);
    }
}

void PatchSelector::itemFocused(int providerIndex)
{
#if WINDOWS
    auto sge = firstListenerOfType<SurgeGUIEditor>();
    if (!sge)
        return;

    auto sr = patchDbProvider->lastSearchResult[providerIndex];
    auto doAcc = Surge::Storage::getUserDefaultValue(
        storage, Surge::Storage::UseNarratorAnnouncementsForPatchTypeahead, true);
    if (doAcc)
    {
        sge->enqueueAccessibleAnnouncement(sr.name + " in " + sr.cat);
    }
#endif
}

void PatchSelector::idle() { wasTypeaheadCanceledSinceLastIdle = false; }

void PatchSelector::typeaheadCanceled()
{
    wasTypeaheadCanceledSinceLastIdle = true;
    toggleTypeAheadSearch(false);
}

void PatchSelector::onSkinChanged()
{
    auto transBlack = juce::Colours::black.withAlpha(0.f);
    typeAhead->setColour(juce::TextEditor::outlineColourId, transBlack);
    typeAhead->setColour(juce::TextEditor::backgroundColourId, transBlack);
    typeAhead->setColour(juce::TextEditor::focusedOutlineColourId, transBlack);
    typeAhead->setFont(skin->fontManager->patchNameFont);
    typeAhead->setIndents(4, (typeAhead->getHeight() - typeAhead->getTextHeight()) / 2);

    typeAhead->setColour(juce::TextEditor::textColourId,
                         skin->getColor(Colors::Dialog::Entry::Text));
    typeAhead->setColour(juce::TextEditor::highlightedTextColourId,
                         skin->getColor(Colors::Dialog::Entry::Text));
    typeAhead->setColour(juce::TextEditor::highlightColourId,
                         skin->getColor(Colors::Dialog::Entry::Focus));

    typeAhead->setColour(Surge::Widgets::TypeAhead::ColourIds::borderid,
                         skin->getColor(Colors::PatchBrowser::TypeAheadList::Border));
    typeAhead->setColour(Surge::Widgets::TypeAhead::ColourIds::emptyBackgroundId,
                         skin->getColor(Colors::PatchBrowser::TypeAheadList::Background));

    patchDbProvider->rowBg = skin->getColor(Colors::PatchBrowser::TypeAheadList::Background);
    patchDbProvider->rowText = skin->getColor(Colors::PatchBrowser::TypeAheadList::Text);
    patchDbProvider->hlRowBg =
        skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightBackground);
    patchDbProvider->hlRowText = skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightText);
    patchDbProvider->rowSubText = skin->getColor(Colors::PatchBrowser::TypeAheadList::SubText);
    patchDbProvider->hlRowSubText =
        skin->getColor(Colors::PatchBrowser::TypeAheadList::HighlightSubText);
    patchDbProvider->divider = skin->getColor(Colors::PatchBrowser::TypeAheadList::Separator);

    patchDbProvider->setSkin(skin, associatedBitmapStore);
}

void PatchSelector::toggleTypeAheadSearch(bool b)
{
    isTypeaheadSearchOn = b;
    auto *sge = firstListenerOfType<SurgeGUIEditor>();

    if (isTypeaheadSearchOn)
    {
        bool enable = true;
        auto txt = pname;
        if (!typeAhead->lastSearch.empty())
            txt = typeAhead->lastSearch;

        storage->initializePatchDb();

        if (storage->patchDB->numberOfJobsOutstanding() > 0)
        {
            enable = false;
            txt = "Updating patch database: " +
                  std::to_string(storage->patchDB->numberOfJobsOutstanding()) + " items left";
        }

        bool patchStickySearchbox = Surge::Storage::getUserDefaultValue(
            storage, Surge::Storage::RetainPatchSearchboxAfterLoad, true);

        typeAhead->dismissMode = patchStickySearchbox
                                     ? TypeAhead::DISMISS_ON_CMD_RETURN_RETAIN_ON_RETURN
                                     : TypeAhead::DISMISS_ON_RETURN_RETAIN_ON_CMD_RETURN;

        typeAhead->setJustification(juce::Justification::centred);
        typeAhead->setIndents(4, (typeAhead->getHeight() - typeAhead->getTextHeight()) / 2);
        typeAhead->setText(txt, juce::NotificationType::dontSendNotification);

        if (!typeAhead->isVisible() && sge)
        {
            sge->vkbForward++;
        }

        typeAhead->setVisible(true);
        typeAhead->setEnabled(enable);

        typeAhead->grabKeyboardFocus();
        typeAhead->selectAll();

        if (!enable)
        {
            juce::Timer::callAfterDelay(250, [that = juce::Component::SafePointer(this)]() {
                if (that)
                    that->enableTypeAheadIfReady();
            });
        }
        else
        {
            typeAhead->searchAndShowLBox();
        }
    }
    else
    {
        if (typeAhead->isVisible() && sge)
            sge->vkbForward--;
        typeAhead->setVisible(false);
    }
    repaint();
    return;
}

void PatchSelector::enableTypeAheadIfReady()
{
    if (!isTypeaheadSearchOn)
        return;

    bool enable = true;
    auto txt = pname;
    if (!typeAhead->lastSearch.empty())
        txt = typeAhead->lastSearch;

    if (storage->patchDB->numberOfJobsOutstanding() > 0)
    {
        enable = false;
        txt = "Updating patch database: " +
              std::to_string(storage->patchDB->numberOfJobsOutstanding()) + " items left";
    }

    bool patchStickySearchbox = Surge::Storage::getUserDefaultValue(
        storage, Surge::Storage::RetainPatchSearchboxAfterLoad, true);

    typeAhead->dismissMode = patchStickySearchbox
                                 ? TypeAhead::DISMISS_ON_CMD_RETURN_RETAIN_ON_RETURN
                                 : TypeAhead::DISMISS_ON_RETURN_RETAIN_ON_CMD_RETURN;

    typeAhead->setText(txt, juce::NotificationType::dontSendNotification);
    typeAhead->setEnabled(enable);

    if (enable)
    {
        typeAhead->grabKeyboardFocus();
        typeAhead->selectAll();
        typeAhead->searchAndShowLBox();
    }
    else
    {
        juce::Timer::callAfterDelay(250, [that = juce::Component::SafePointer(this)]() {
            if (that)
                that->enableTypeAheadIfReady();
        });
    }
}

bool PatchSelector::keyPressed(const juce::KeyPress &key)
{
    if (isTypeaheadSearchOn && storage->patchDB->numberOfJobsOutstanding() > 0)
    {
        // Any keypress while we are waiting is ignored other than perhaps escape
        if (key.getKeyCode() == juce::KeyPress::escapeKey)
        {
            toggleTypeAheadSearch(false);
            repaint();
        }
        return true;
    }

    auto [action, mod] = Surge::Widgets::accessibleEditAction(key, storage);

    if (action == OpenMenu)
    {
        showClassicMenu(false, key.getModifiers().isCommandDown());
        return true;
    }

    if (action == Return)
    {
        showClassicMenu(false, key.getModifiers().isCommandDown());
        return true;
    }

    return false;
}

class PatchSelectorAH : public juce::AccessibilityHandler
{
  public:
    explicit PatchSelectorAH(PatchSelector *sel)
        : selector(sel),
          juce::AccessibilityHandler(*sel, juce::AccessibilityRole::label,
                                     juce::AccessibilityActions()
                                         .addAction(juce::AccessibilityActionType::press,
                                                    [sel] { sel->showClassicMenu(); })
                                         .addAction(juce::AccessibilityActionType::showMenu,
                                                    [sel] { sel->showClassicMenu(); }),
                                     {std::make_unique<PatchSelectorValueInterface>(sel)})
    {
    }

  private:
    class PatchSelectorValueInterface : public juce::AccessibilityTextValueInterface
    {
      public:
        explicit PatchSelectorValueInterface(PatchSelector *sel) : selector(sel) {}

        bool isReadOnly() const override { return true; }
        juce::String getCurrentValueAsString() const override
        {
            return selector->getPatchNameAccessibleValue();
        }
        void setValueAsString(const juce::String &) override {}

      private:
        PatchSelector *selector;

        //==============================================================================
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelectorValueInterface)
    };

    PatchSelector *selector;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelectorAH)
};

std::unique_ptr<juce::AccessibilityHandler> PatchSelector::createAccessibilityHandler()
{
    return std::make_unique<PatchSelectorAH>(this);
}

void PatchSelectorCommentTooltip::paint(juce::Graphics &g)
{
    namespace clr = Colors::PatchBrowser::CommentTooltip;
    g.fillAll(skin->getColor(clr::Border));
    g.setColour(skin->getColor(clr::Background));
    g.fillRect(getLocalBounds().reduced(1));
    g.setColour(skin->getColor(clr::Text));
    g.setFont(skin->fontManager->getLatoAtSize(9));
    g.drawMultiLineText(comment, 4, g.getCurrentFont().getHeight() + 2, getWidth(),
                        juce::Justification::left);
}

void PatchSelectorCommentTooltip::positionForComment(const juce::Point<int> &centerPoint,
                                                     const std::string &c,
                                                     const int maxTooltipWidth)
{
    comment = c;

    std::stringstream ss(comment);
    std::string to;

    int numLines = 0;

    auto ft = skin->fontManager->getLatoAtSize(9);
    auto width = 0.f;
    auto maxWidth = (float)maxTooltipWidth;

    while (std::getline(ss, to, '\n'))
    {
        auto w = ft.getStringWidthFloat(to);

        // in case of an empty line, we still need to count it as an extra row
        // so bump it up a bit so that the rows calculation ceils to 1
        if (w == 0.f)
        {
            w = 1.f;
        }

        auto rows = std::ceil(w / maxWidth);

        width = std::max(w, width);
        numLines += (int)rows;
    }

    auto height = std::max((numLines * (ft.getHeight() + 2)) + 2, 16.f);
    auto margin = 10;

    auto r = juce::Rectangle<int>()
                 .withCentre(juce::Point(centerPoint.x, centerPoint.y))
                 .withSizeKeepingCentre(std::min(width + margin, maxWidth), height)
                 .translated(0, height / 2);

    setBounds(r);
    repaint();
}

void PatchSelector::searchUpdated()
{
    outstandingSearches++;
    auto cb = [ptr = juce::Component::SafePointer(this)]() {
        if (ptr)
        {
            ptr->outstandingSearches--;
            if (ptr->isTypeaheadSearchOn && ptr->outstandingSearches == 0)
            {
                auto sge = ptr->firstListenerOfType<SurgeGUIEditor>();
                if (sge)
                {
                    auto items = ptr->patchDbProvider->lastSearchResult.size();
                    auto ann = fmt::format("Found {} patches; Down to navigate", items);
                    sge->enqueueAccessibleAnnouncement(ann);
                }
            }
        }
    };
    juce::Timer::callAfterDelay(1.0 * 1000, cb);
}

void PatchSelector::typeaheadButtonPressed()
{
    tooltipCountdown = -1;
    toggleCommentTooltip(false);

    if (wasTypeaheadCanceledSinceLastIdle)
    {
        toggleTypeAheadSearch(false);
    }
    else
    {
        toggleTypeAheadSearch(!isTypeaheadSearchOn);
    }
}

void PatchSelector::setIsFavorite(bool b)
{
    isFavorite = b;
    favoriteButton->setTitle(b ? "Remove from Favorites" : "Add to Favorites");
    favoriteButton->setDescription(b ? "Remove from Favorites" : "Add to Favorites");
    if (favoriteButton->getAccessibilityHandler())
    {
        favoriteButton->getAccessibilityHandler()->notifyAccessibilityEvent(
            juce::AccessibilityEvent::titleChanged);
    }
    repaint();
}

void PatchSelector::showFavoritesMenu()
{
    juce::PopupMenu menu;

    tooltipCountdown = -1;
    toggleCommentTooltip(false);

    Surge::Widgets::MenuCenteredBoldLabel::addToMenuAsSectionHeader(menu, "FAVORITES");

    auto haveFavs = optionallyAddFavorites(menu, false, false);

    if (haveFavs)
    {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        stuckHover = true;
        menu.showMenuAsync(sge->popupMenuOptions(favoritesRect.getBottomLeft()),
                           [that = juce::Component::SafePointer(this)](int) {
                               if (that)
                               {
                                   that->stuckHover = false;
                                   that->endHover();
                               }
                           });
    }
}

void PatchSelector::toggleFavoriteStatus()
{
    setIsFavorite(!isFavorite);
    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        sge->setPatchAsFavorite(pname, isFavorite);
        repaint();
    }
}
} // namespace Widgets
} // namespace Surge