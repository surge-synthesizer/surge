/*
** Surge Synthesizer is Free and Open Source Software
**
** Surge is made available under the Gnu General Public License, v3.0
** https://www.gnu.org/licenses/gpl-3.0.en.html
**
** Copyright 2004-2020 by various individuals as described by the Git transaction log
**
** All source at: https://github.com/surge-synthesizer/surge.git
**
** Surge was a commercial product from 2004-2018, with Copyright and ownership
** in that period held by Claes Johanson at Vember Audio. Claes made Surge
** open source in September 2018.
*/

#include "SurgeGUIEditor.h"
#include "CPatchBrowser.h"
#include "UserInteractions.h"
#include "SkinColors.h"
#include "guihelpers.h"

#include <vector>

using namespace VSTGUI;
using namespace std;

extern CFontRef displayFont;
extern CFontRef patchNameFont;

void CPatchBrowser::draw(CDrawContext *dc)
{
    CRect pbrowser = getViewSize();
    CRect cat(pbrowser), auth(pbrowser);

    cat.left += 3;
    cat.right = cat.left + 150;
    cat.setHeight(pbrowser.getHeight() / 2);
    if (skin->getVersion() >= 2)
    {
        cat.offset(0, (pbrowser.getHeight() / 2) - 1);

        auth.right -= 3;
        auth.left = auth.right - 150;
        auth.top = cat.top;
        auth.bottom = cat.bottom;
    }
    else
    {
        auth = cat;
        auth.offset(0, pbrowser.getHeight() / 2);

        cat.offset(0, 1);
        auth.offset(0, -1);
    }
    // debug draws
    // dc->drawRect(pbrowser);
    // dc->drawRect(cat);
    // dc->drawRect(auth);

    // patch browser text color
    dc->setFontColor(skin->getColor(Colors::PatchBrowser::Text));

    // patch name
    dc->setFont(patchNameFont);
    dc->drawString(pname.c_str(), pbrowser, kCenterText, true);

    // category/author name
    dc->setFont(displayFont);
    dc->drawString(category.c_str(), cat, kLeftText, true);
    dc->drawString(author.c_str(), auth, skin->getVersion() >= 2 ? kRightText : kLeftText, true);

    setDirty(false);
}

CMouseEventResult CPatchBrowser::onMouseDown(CPoint &where, const CButtonState &button)
{
    if (listener && (button & (kMButton | kButton4 | kButton5)))
    {
        listener->controlModifierClicked(this, button);
        return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    CRect menurect(0, 0, 0, 0);
    menurect.offset(where.x, where.y);
    COptionMenu *contextMenu =
        new COptionMenu(menurect, 0, 0, 0, 0, COptionMenu::kMultipleCheckStyle);

    int main_e = 0;
    // if RMB is down, only show the current category
    bool single_category = button & (kRButton | kControl), has_3rdparty = false;
    int last_category = current_category;
    int root_count = 0, usercat_pos = 0, col_breakpoint = 0;
    auto patch_cat_size = storage->patch_category.size();

    if (single_category)
    {
        contextMenu->setNbItemsPerColumn(32);

        /*
        ** in the init scenario we don't have a category yet. Our two choices are
        ** don't pop up the menu or pick one. I choose to pick one. If I can
        ** find the one called "Init" use that. Otherwise pick 0.
        */
        int rightMouseCategory = current_category;
        if (current_category < 0)
        {
            if (storage->patchCategoryOrdering.size() == 0)
                return kMouseEventHandled;
            for (auto c : storage->patchCategoryOrdering)
            {
                if (_stricmp(storage->patch_category[c].name.c_str(), "Init") == 0)
                {
                    rightMouseCategory = c;
                    ;
                }
            }
            if (rightMouseCategory < 0)
            {
                rightMouseCategory = storage->patchCategoryOrdering[0];
            }
        }

        populatePatchMenuForCategory(rightMouseCategory, contextMenu, single_category, main_e,
                                     false);
    }
    else
    {
        if (patch_cat_size && storage->firstThirdPartyCategory > 0)
        {
            auto factory_add = contextMenu->addEntry("FACTORY PATCHES");
            factory_add->setEnabled(0);
        }

        for (int i = 0; i < patch_cat_size; i++)
        {
            if ((!single_category) || (i == last_category))
            {
                if (!single_category &&
                    (i == storage->firstThirdPartyCategory || i == storage->firstUserCategory))
                {
                    string txt;

                    if (i == storage->firstThirdPartyCategory && storage->firstUserCategory != i)
                    {
                        has_3rdparty = true;
                        txt = "THIRD PARTY PATCHES";
                    }
                    else
                    {
                        txt = "USER PATCHES";
                    }

                    auto add = contextMenu->addEntry(txt.c_str());
                    add->setEnabled(0);
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

#if WINDOWS
        // make sure user category always ends up in second column
        col_breakpoint = std::max(usercat_pos ? usercat_pos : (root_count + 1), 32);
        contextMenu->setNbItemsPerColumn(col_breakpoint - 1 + 2);
#endif
    }

#if WINDOWS
    // since we're starting user presets on second column,
    // we don't need this separator if we have no user presets,
    // because then we'd have a dangling separator at the bottom of first column
    if (usercat_pos || !has_3rdparty || storage->firstThirdPartyCategory == 0)
    {
        contextMenu->addSeparator();
    }
#else
    // on Mac and Linux we cannot do multicolumns (at least not until JUCE!)
    // so just add a separator and move on
    if (!has_3rdparty || storage->firstThirdPartyCategory == 0)
    {
        contextMenu->addSeparator();
    }
#endif

    auto loadF = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Load Patch from File...")));
    loadF->setActions([this](CCommandMenuItem *item) {
        Surge::UserInteractions::promptFileOpenDialog(
            "", "fxp", "Surge FXP Files", [this](std::string fn) {
                auto sge = dynamic_cast<SurgeGUIEditor *>(listener);
                if (!sge)
                    return;
                sge->queuePatchFileLoad(fn);
            });
    });
    contextMenu->addEntry(loadF);

    auto refreshItem = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Refresh Patch List")));
    auto refreshAction = [this](CCommandMenuItem *item) { this->storage->refresh_patchlist(); };
    refreshItem->setActions(refreshAction, nullptr);
    contextMenu->addEntry(refreshItem);

    contextMenu->addSeparator();

    auto showU = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Open User Patches Folder...")));
    showU->setActions([this](CCommandMenuItem *item) {
        Surge::UserInteractions::openFolderInFileBrowser(this->storage->userDataPath);
    });
    contextMenu->addEntry(showU);

    auto showF = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Open Factory Patches Folder...")));
    showF->setActions([this](CCommandMenuItem *item) {
        Surge::UserInteractions::openFolderInFileBrowser(
            Surge::Storage::appendDirectory(this->storage->datapath, "patches_factory"));
    });
    contextMenu->addEntry(showF);

    auto show3 = new CCommandMenuItem(
        CCommandMenuItem::Desc(Surge::UI::toOSCaseForMenu("Open Third Party Patches Folder...")));
    show3->setActions([this](CCommandMenuItem *item) {
        Surge::UserInteractions::openFolderInFileBrowser(
            Surge::Storage::appendDirectory(this->storage->datapath, "patches_3rdparty"));
    });
    contextMenu->addEntry(show3);

    contextMenu->addSeparator();

    contextMenu->cleanupSeparators(false);

    auto *sge = dynamic_cast<SurgeGUIEditor *>(listener);
    if (sge)
    {
        auto hu = sge->helpURLForSpecial("patch-browser");
        if (hu != "")
        {
            auto lurl = sge->fullyResolvedHelpURL(hu);
            auto hi = new CCommandMenuItem(CCommandMenuItem::Desc("[?] Patch Browser"));
            auto ca = [lurl](CCommandMenuItem *i) { Surge::UserInteractions::openURL(lurl); };
            hi->setActions(ca, nullptr);
            contextMenu->addEntry(hi);
        }
    }

    if (sge)
    {
        /*
         * So why are we doing this? Well idle updates (like automation of something
         * that causes a rebuild) can rebuild the UI under us, but also, since the menu
         * sends a message to the synth to queue a rebuild, it is possible that the load,
         * process and rebuild could happen *before* popup returns. So imagine
         *
         * addView
         * popup() [frees the ui queue so the idle is running again
         *     - click menu
         *     - set patchid queue
         *     - audio loads the patch
         *     - idle queue runs (normally this happens after popup closes)
         *     - UI rebuilds
         * popup returns
         * remove self from frame - but hey I've been GCed by that rebuild
         * splat
         *
         * in the normal course of course you get
         *
         * addView
         * popup()
         *      - click menu
         *      - patch id queue
         * close()
         * idle runs after close
         *
         * but not every time. So on slower boxes sometimes (and only sometimes)
         * the menu would crash. Solve this by having the idle skipped while the
         * menu is open.
         */
        sge->pause_idle_updates = true;

        getFrame()->addView(contextMenu); // add to frame
        contextMenu->setDirty();
        contextMenu->popup();
        getFrame()->removeView(contextMenu, true); // remove from frame and forget

        sge->pause_idle_updates = false;

#if !LINUX || TARGET_JUCE_UI
        // on linux none of this matters because popup always returns right away
        // so if we flush now it actually crashes us for the above reason.
        // INstead linux should flush in the valueChanged callback
        sge->flushEnqueuedPatchId();
#endif
    }
    return kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

bool CPatchBrowser::populatePatchMenuForCategory(int c, COptionMenu *contextMenu,
                                                 bool single_category, int &main_e, bool rootCall)
{
    bool amIChecked = false;
    PatchCategory cat = storage->patch_category[c];
    if (rootCall && !cat.isRoot)
        return false; // stop it going in the top menu which is a straight iteration
    if (cat.numberOfPatchesInCategoryAndChildren == 0)
        return false; // Don't do empty categories

    int splitcount = 256;
    // Go through the whole patch list in alphabetical order and filter
    // out only the patches that belong to the current category.
    vector<int> ctge;
    for (auto p : storage->patchOrdering)
    {
        if (storage->patch_list[p].category == c)
        {
            ctge.push_back(p);
        }
    }

    // Divide categories with more entries than splitcount into subcategories f.ex. bass (1,2) etc
    // etc
    int n_subc = 1 + (max(2, (int)ctge.size()) - 1) / splitcount;
    for (int subc = 0; subc < n_subc; subc++)
    {
        string name;
        COptionMenu *subMenu;

        if (single_category)
            subMenu = contextMenu;
        else
        {
            subMenu = new COptionMenu(getViewSize(), nullptr, main_e, 0, 0,
                                      COptionMenu::kMultipleCheckStyle);
            subMenu->setNbItemsPerColumn(32);
        }

        int sub = 0;

        for (int i = subc * splitcount; i < min((subc + 1) * splitcount, (int)ctge.size()); i++)
        {
            int p = ctge[i];

            name = storage->patch_list[p].name;

#if WINDOWS
            Surge::Storage::findReplaceSubstring(name, string("&"), string("&&"));
#endif

            auto actionItem = new CCommandMenuItem(CCommandMenuItem::Desc(name.c_str()));
            auto action = [this, p](CCommandMenuItem *item) { this->loadPatch(p); };

            if (p == current_patch)
            {
                actionItem->setChecked(true);
                amIChecked = true;
            }
            actionItem->setActions(action, nullptr);
            subMenu->addEntry(actionItem);
            sub++;
        }

        for (auto &childcat : cat.children)
        {
            // this isn't the best approach but it works
            int idx = 0;
            for (auto &cc : storage->patch_category)
            {
                if (cc.name == childcat.name && cc.internalid == childcat.internalid)
                    break;
                idx++;
            }

            bool checkedKid = populatePatchMenuForCategory(idx, subMenu, false, main_e, false);
            if (checkedKid)
            {
                amIChecked = true;
            }
        }

        string menuName = storage->patch_category[c].name;

        if (menuName.find_last_of(PATH_SEPARATOR) != string::npos)
            menuName = menuName.substr(menuName.find_last_of(PATH_SEPARATOR) + 1);

        if (n_subc > 1)
            name = menuName.c_str() + (subc + 1);
        else
            name = menuName.c_str();

        // tuck in the category name by 4 spaces, but only the root categories
        if (rootCall)
            name = "    " + name;

#if WINDOWS
        Surge::Storage::findReplaceSubstring(name, string("&"), string("&&"));
#endif

        if (!single_category)
        {
            CMenuItem *entry = contextMenu->addEntry(subMenu, name.c_str());
            if (c == current_category || amIChecked)
                entry->setChecked(true);
            subMenu->forget(); // Important, so that the refcounter gets it right
        }
        main_e++;
    }
    return amIChecked;
}

void CPatchBrowser::loadPatch(int id)
{
    if (listener && (id >= 0))
    {
        enqueue_sel_id = id;
        listener->valueChanged(this);
    }
}
