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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_PATCHSELECTOR_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_PATCHSELECTOR_H

#include "SkinSupport.h"
#include "WidgetBaseMixin.h"

#include "juce_gui_basics/juce_gui_basics.h"
#include "widgets/TypeAheadTextEditor.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{

struct PatchDBTypeAheadProvider;

struct PatchSelector : public juce::Component,
                       public WidgetBaseMixin<PatchSelector>,
                       public TypeAhead::TypeAheadListener
{
    PatchSelector();
    ~PatchSelector();

    float getValue() const override { return 0; }
    void setValue(float f) override {}

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s);

    void setIDs(int category, int patch)
    {
        current_category = category;
        current_patch = patch;

        if (auto *handler = getAccessibilityHandler())
        {
            if (handler->getValueInterface())
            {
                handler->notifyAccessibilityEvent(juce::AccessibilityEvent::valueChanged);
            }
            handler->notifyAccessibilityEvent(juce::AccessibilityEvent::titleChanged);
        }
    }

    bool isFavorite{false};
    void setIsFavorite(bool b);

    bool isUser{false};
    void setIsUser(bool b)
    {
        isUser = b;
        repaint();
    }

    void setLabel(const std::string &l) { pname = l; }
    void setCategory(std::string l)
    {
        if (l.length())
        {
            category = path_to_string(string_to_path(l).filename());
        }
        else
        {
            category = "";
        }

        repaint();
    }

    void setAuthor(std::string l)
    {
        if (l.length())
        {
            author = l;
        }
        else
        {
            author = "";
        }

        repaint();
    }
    void setComment(const std::string &c) { comment = c; }

    void setTags(const std::vector<SurgePatch::Tag> &itag) { tags = itag; }

    /// TypeAhead API
    void itemSelected(int providerIndex) override;
    void itemFocused(int providerIndex) override;
    void typeaheadCanceled() override;

    void resized() override;
    void mouseDown(const juce::MouseEvent &event) override;
    bool favoritesHover{false}, searchHover{false}, browserHover{false}, stuckHover{false};
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseEnter(const juce::MouseEvent &event) override;
    void mouseExit(const juce::MouseEvent &event) override { endHover(); }

    bool isCurrentlyHovered() override { return favoritesHover || searchHover || browserHover; }

    void endHover() override
    {
        if (stuckHover)
            return;

        browserHover = false;
        favoritesHover = false;
        searchHover = false;
        tooltipCountdown = -1;
        // toggleCommentTooltip(false);
        repaint();
    }

    bool keyPressed(const juce::KeyPress &key) override;
    void showClassicMenu(bool singleCategory = false, bool userOnly = false);
    bool optionallyAddFavorites(juce::PopupMenu &into, bool addColumnBreakAndHeader,
                                bool addToSubmenu = true);
    void exportFavorites();
    void importFavorites();
    void openPatchBrowser();

    void paint(juce::Graphics &g) override;

    void loadPatch(int id);
    void loadInitPatch();
    int sel_id = 0, enqueue_sel_id = 0;

    int getCurrentPatchId() const;
    int getCurrentCategoryId() const;

    void idle(); // called by SGE::idle

    bool wasTypeaheadCanceledSinceLastIdle{false};
    bool isTypeaheadSearchOn{false};
    void toggleTypeAheadSearch(bool);
    void enableTypeAheadIfReady();
    void searchUpdated();
    void typeaheadButtonPressed();
    uint32_t outstandingSearches{0};
    std::unique_ptr<Surge::Widgets::TypeAhead> typeAhead;
    std::unique_ptr<PatchDBTypeAheadProvider> patchDbProvider;

    std::string getPatchNameAccessibleValue() { return pname + " by " + author; }

    void onSkinChanged() override;
    bool isDirty{false};

  protected:
    juce::Rectangle<int> favoritesRect, searchRect;

    std::string pname;
    std::string category;
    std::string author;
    std::string comment;
    std::vector<SurgePatch::Tag> tags;
    int current_category = 0, current_patch = 0;

    int tooltipCountdown{-1};
    void toggleCommentTooltip(bool b);
    void shouldTooltip();
    juce::Point<float> tooltipMouseLocation;
    bool tooltipShowing{false};

    /**
     * populatePatchMenuForCategory
     *
     * recursively builds the nested patch menu. In the event that one of my children is checked,
     * return true so I too can be checked. otherwise, return false.
     */
    bool populatePatchMenuForCategory(int index, juce::PopupMenu &contextMenu, bool single_category,
                                      int &main_e, bool rootCall);

    // a little transparent button to alow ally and focus over find and fav
    struct TB;
    std::unique_ptr<TB> searchButton, favoriteButton;
    void showFavoritesMenu();
    void toggleFavoriteStatus();

  private:
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

  protected:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelector);
};

struct PatchSelectorCommentTooltip : public juce::Component,
                                     public Surge::GUI::SkinConsumingComponent
{
    PatchSelectorCommentTooltip(){};
    void paint(juce::Graphics &g) override;

    std::string comment;
    void positionForComment(const juce::Point<int> &centerPoint, const std::string &comment,
                            const int maxTooltipWidth);

  protected:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSelectorCommentTooltip);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_PATCHSELECTOR_H
