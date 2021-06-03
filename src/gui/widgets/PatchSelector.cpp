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

namespace Surge
{
namespace Widgets
{
PatchSelector::PatchSelector() = default;
PatchSelector::~PatchSelector() = default;

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

#if DEBUG_PATCH_AREAS
    g.setColour(juce::Colours::red);
    g.drawRect(pbrowser);
    g.drawRect(cat);
    g.drawRect(auth);
#endif

    // patch browser text color
    g.setColour(skin->getColor(Colors::PatchBrowser::Text));

    // patch name
    g.setFont(Surge::GUI::getFontManager()->patchNameFont);
    g.drawText(pname.c_str(), pbrowser, juce::Justification::centred);

    // category/author name
    g.setFont(Surge::GUI::getFontManager()->displayFont);
    g.drawText(category, cat, juce::Justification::centredLeft);
    g.drawText(author, auth,
               skin->getVersion() >= 2 ? juce::Justification::centredRight
                                       : juce::Justification::centredLeft);
}

void PatchSelector::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isMiddleButtonDown())
    {
        notifyControlModifierClicked(e.mods);
        return;
    }

    auto contextMenu = juce::PopupMenu();
    int main_e = 0;
    // if RMB is down, only show the current category
    bool single_category =
        e.mods.isRightButtonDown() || e.mods.isCtrlDown() || e.mods.isCommandDown();
    bool has_3rdparty = false;
    int last_category = current_category;
    int root_count = 0, usercat_pos = 0, col_breakpoint = 0;
    auto patch_cat_size = storage->patch_category.size();

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

        contextMenu.addSectionHeader("PATCHES (" + menuName + ")");

        populatePatchMenuForCategory(rightMouseCategory, contextMenu, single_category, main_e,
                                     false);
    }
    else
    {
        if (patch_cat_size && storage->firstThirdPartyCategory > 0)
        {
            contextMenu.addSectionHeader("FACTORY PATCHES");
        }

        for (int i = 0; i < patch_cat_size; i++)
        {
            if ((!single_category) || (i == last_category))
            {
                if (!single_category &&
                    (i == storage->firstThirdPartyCategory || i == storage->firstUserCategory))
                {
                    std::string txt;

                    if (i == storage->firstThirdPartyCategory && storage->firstUserCategory != i)
                    {
                        has_3rdparty = true;
                        txt = "THIRD PARTY PATCHES";
                    }
                    else
                    {
                        txt = "USER PATCHES";
                    }

                    contextMenu.addColumnBreak();
                    contextMenu.addSectionHeader(txt);
                }

                // Remap index to the corresponding category in alphabetical order.
                int c = storage->patchCategoryOrdering[i];

                // find at which position the first user category root folder shows up
                if (storage->patch_category[i].isRoot)
                {
                    root_count++;

                    if (i == storage->firstUserCategory)
                    {
                        usercat_pos = root_count;
                    }
                }

                populatePatchMenuForCategory(c, contextMenu, single_category, main_e, true);
            }
        }
    }

    contextMenu.addColumnBreak();
    contextMenu.addSectionHeader("FUNCTIONS");

    auto initAction = [this]() {
        int i = 0;

        for (auto p : storage->patch_list)
        {
            if (p.name == storage->initPatchName &&
                storage->patch_category[p.category].name == storage->initPatchCategory)
            {
                loadPatch(i);
                break;
            }

            ++i;
        }
    };

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Initialize Patch"), initAction);

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Set Current Patch as Default"), [this]() {
        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchName,
                                               storage->patch_list[current_patch].name);

        Surge::Storage::updateUserDefaultValue(storage, Surge::Storage::InitialPatchCategory,
                                               storage->patch_category[current_category].name);
    });

    contextMenu.addSeparator();

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Patch Database..."), [this]() {
        auto sge = firstListenerOfType<SurgeGUIEditor>();

        if (sge)
        {
            sge->showPatchBrowserDialog();
        }
    });

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Refresh Patch List"),
                        [this]() { this->storage->refresh_patchlist(); });

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Load Patch From File..."), [this]() {
        juce::FileChooser c("Select Patch to Load", juce::File(storage->userDataPath), "*.fxp");
        auto r = c.browseForFileToOpen();

        if (r)
        {
            auto res = c.getResult();
            auto rString = res.getFullPathName().toStdString();
            auto sge = firstListenerOfType<SurgeGUIEditor>();

            if (sge)
            {
                sge->queuePatchFileLoad(rString);
            }
        }
    });

    contextMenu.addSeparator();

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open User Patches Folder..."),
                        [this]() { Surge::GUI::openFileOrFolder(this->storage->userDataPath); });

    contextMenu.addItem(Surge::GUI::toOSCaseForMenu("Open Factory Patches Folder..."), [this]() {
        Surge::GUI::openFileOrFolder(
            Surge::Storage::appendDirectory(this->storage->datapath, "patches_factory"));
    });

    contextMenu.addItem(
        Surge::GUI::toOSCaseForMenu("Open Third Party Patches Folder..."), [this]() {
            Surge::GUI::openFileOrFolder(
                Surge::Storage::appendDirectory(this->storage->datapath, "patches_3rdparty"));
        });

    contextMenu.addSeparator();

    auto sge = firstListenerOfType<SurgeGUIEditor>();

    if (sge)
    {
        auto hu = sge->helpURLForSpecial("patch-browser");

        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            auto ca = [lurl]() { juce::URL(lurl).launchInDefaultBrowser(); };

            contextMenu.addItem("[?] Patch Browser", ca);
        }
    }

    contextMenu.showMenuAsync(juce::PopupMenu::Options());
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

            subMenu->addItem(name, true, thisCheck, [this, p]() { this->loadPatch(p); });
            sub++;

            if (sub != 0 && sub % 32 == 0)
            {
                subMenu->addColumnBreak();

                if (single_category)
                {
                    subMenu->addSectionHeader("");
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
            name = menuName.c_str() + (subc + 1);
        }
        else
        {
            name = menuName.c_str();
        }

        if (!single_category)
        {
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
        enqueue_sel_id = id;
        notifyValueChanged();
    }
}

int PatchSelector::getCurrentPatchId() const { return current_patch; }

int PatchSelector::getCurrentCategoryId() const { return current_category; }

} // namespace Widgets
} // namespace Surge