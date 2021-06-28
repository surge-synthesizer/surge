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

#ifndef SURGE_XT_PATCHSELECTOR_H
#define SURGE_XT_PATCHSELECTOR_H

#include <JuceHeader.h>
#include "efvg/escape_from_vstgui.h"
#include "SkinSupport.h"
#include "WidgetBaseMixin.h"
#include "SurgeJUCEHelpers.h"

class SurgeStorage;

namespace Surge
{
namespace Widgets
{
struct PatchSelector : public juce::Component, public WidgetBaseMixin<PatchSelector>
{
    PatchSelector();
    ~PatchSelector();

    float getValue() const override { return 0; }
    void setValue(float f) override {}

    SurgeStorage *storage{nullptr};
    void setStorage(SurgeStorage *s) { storage = s; }

    void setIDs(int category, int patch)
    {
        current_category = category;
        current_patch = patch;
    }

    void setLabel(const std::string &l) { pname = l; }

    void setCategory(std::string l)
    {
        if (l.length())
        {
            category = "Category: " + path_to_string(string_to_path(l).filename());
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
            author = "By: " + l;
        }
        else
        {
            author = "";
        }

        repaint();
    }

    void mouseDown(const juce::MouseEvent &event) override;
    void paint(juce::Graphics &g) override;

    void loadPatch(int id);
    int sel_id = 0, enqueue_sel_id = 0;

    int getCurrentPatchId() const;
    int getCurrentCategoryId() const;

  protected:
    std::string pname;
    std::string category;
    std::string author;
    int current_category = 0, current_patch = 0;

    /**
     * populatePatchMenuForCategory
     *
     * recursively builds the nested patch menu. In the event that one of my children is checked,
     * return true so I too can be checked. otherwise, return false.
     */
    bool populatePatchMenuForCategory(int index, juce::PopupMenu &contextMenu, bool single_category,
                                      int &main_e, bool rootCall);
};
} // namespace Widgets
} // namespace Surge
#endif // SURGE_XT_PATCHSELECTOR_H
