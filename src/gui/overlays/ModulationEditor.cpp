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
#include "RuntimeFont.h"
#include <sstream>

namespace Surge
{
namespace Overlays
{
struct ModulationListBoxModel : public juce::ListBoxModel
{
    struct Datum
    {
        enum RowType
        {
            HEADER,
            SHOW_ROW,
            EDIT_ROW
        } type;

        explicit Datum(RowType t) : type(t) {}

        std::string hLab;
        int dest_id = -1, source_id = -1;
        std::string dest_name, source_name;
        int modNum = -1;

        float depth = 0;
        bool isBipolar = false;
        // const Parameter *p;
        Parameter *p = nullptr; // When we do proper consting return to this
        ModulationDisplayInfoWindowStrings mss;
    };
    std::vector<Datum> rows;
    std::string debugRows;

    struct HeaderComponent : public juce::Component
    {
        HeaderComponent(ModulationListBoxModel *mod, int row) : mod(mod), row(row) {}
        void paint(juce::Graphics &g) override
        {
            g.fillAll(juce::Colour(30, 30, 30));
            auto r = getBounds().expanded(-1);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(10, juce::Font::bold));
            g.setColour(juce::Colour(255, 255, 255));
            g.drawText(mod->rows[row].hLab, r, juce::Justification::left);
        }
        ModulationListBoxModel *mod;
        int row;
    };

    struct ShowComponent : public juce::Component
    {
        ShowComponent(ModulationListBoxModel *mod, int row) : mod(mod), row(row) {}
        void paint(juce::Graphics &g) override
        {
            auto r = getBounds().withTrimmedLeft(15);

            auto dat = mod->rows[row];

            g.setColour(juce::Colour(48, 48, 48));
            if (mod->rows[row].modNum % 2)
                g.setColour(juce::Colour(32, 32, 32));
            g.fillRect(r);

            auto er = r.expanded(-1);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.setColour(juce::Colour(255, 255, 255));
            g.drawText(dat.source_name, r, juce::Justification::left);
            auto rR = r.withTrimmedRight(r.getWidth() / 2);
            g.drawText(dat.dest_name, rR, juce::Justification::right);

            auto rL = r.withTrimmedLeft(2 * r.getWidth() / 3).withTrimmedRight(4);
            if (dat.isBipolar)
                g.drawText(dat.mss.dvalminus, rL, juce::Justification::left);
            g.drawText(dat.mss.val, rL, juce::Justification::centred);
            g.drawText(dat.mss.dvalplus, rL, juce::Justification::right);
        }
        ModulationListBoxModel *mod;
        int row;
    };

    struct EditComponent : public juce::Component,
                           public juce::Slider::Listener,
                           public juce::Button::Listener
    {
        std::unique_ptr<juce::Button> clearButton;
        std::unique_ptr<juce::Button> muteButton;
        std::unique_ptr<juce::Slider> modSlider;
        EditComponent(ModulationListBoxModel *mod, int row) : mod(mod), row(row)
        {
            clearButton = std::make_unique<juce::TextButton>("Clear");
            clearButton->setButtonText("Clear");
            clearButton->addListener(this);
            addAndMakeVisible(*clearButton);

            muteButton = std::make_unique<juce::ToggleButton>("Mute");
            muteButton->setButtonText("Mute");
            muteButton->addListener(this);
            muteButton->setToggleState(
                mod->moded->synth->isModulationMuted(mod->rows[row].dest_id,
                                                     (modsources)mod->rows[row].source_id),
                juce::NotificationType::dontSendNotification);
            addAndMakeVisible(*muteButton);

            modSlider = std::make_unique<juce::Slider>("Modulation");
            modSlider->setSliderStyle(juce::Slider::LinearHorizontal);
            modSlider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            modSlider->setRange(-1, 1);
            modSlider->setValue(mod->rows[row].p->get_modulation_f01(mod->rows[row].depth),
                                juce::NotificationType::dontSendNotification);
            modSlider->addListener(this);
            addAndMakeVisible(*modSlider);
        }
        void sliderValueChanged(juce::Slider *slider) override
        {
            auto rd = mod->rows[row];
            auto um = rd.p->set_modulation_f01(slider->getValue());
            mod->moded->synth->setModulation(rd.dest_id, (modsources)rd.source_id,
                                             slider->getValue());
            mod->updateRowByModnum(rd.modNum);
            mod->moded->repaint();
            // std::cout << "SVC " << slider->getValue() << std::endl;
        }
        void buttonClicked(juce::Button *button) override
        {
            if (button == clearButton.get())
            {
                mod->moded->synth->clearModulation(mod->rows[row].dest_id,
                                                   (modsources)mod->rows[row].source_id);
                mod->updateRows();
                mod->moded->listBox->updateContent();
            }
            if (button == muteButton.get())
            {
                mod->moded->synth->muteModulation(mod->rows[row].dest_id,
                                                  (modsources)mod->rows[row].source_id,
                                                  button->getToggleState());
            }
        }
        void resized() override
        {
            auto b = getBounds().withTrimmedLeft(15).expanded(-1).withRight(
                getBounds().getTopLeft().getX() + 50);
            clearButton->setBounds(b);

            b = getBounds().withTrimmedLeft(15).withTrimmedLeft(35).expanded(-1).withRight(
                getBounds().getTopLeft().getX() + 97);
            muteButton->setBounds(b);

            b = getBounds().withTrimmedLeft(15).withTrimmedLeft(100 + 3).expanded(-1).withRight(
                getWidth() / 2);
            modSlider->setBounds(b);

            Component::resized();
        }
        void paint(juce::Graphics &g) override
        {
            auto r = getBounds().withTrimmedLeft(15);

            g.setColour(juce::Colour(48, 48, 48));
            if (mod->rows[row].modNum % 2)
                g.setColour(juce::Colour(32, 32, 32));

            auto dat = mod->rows[row];

            g.fillRect(r);

            auto er = r.expanded(-1);
            g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
            g.setColour(juce::Colour(200, 200, 255));
            // g.drawText("Edit Controls Coming Soon", r, juce::Justification::left);

            g.setColour(juce::Colour(255, 255, 255));
            auto rL = r.withTrimmedLeft(2 * r.getWidth() / 3).withTrimmedRight(4);
            if (dat.isBipolar)
                g.drawText(dat.mss.valminus, rL, juce::Justification::left);
            g.drawText(dat.mss.valplus, rL, juce::Justification::right);
        }
        ModulationListBoxModel *mod;
        int row;
    };

    ModulationListBoxModel(ModulationEditor *ed) : moded(ed) { updateRows(); }

    void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                          bool rowIsSelected) override
    {
        g.fillAll(juce::Colour(0, 0, 0));
    }

    int getNumRows() override { return rows.size(); }

    void updateRowByModnum(int modnum)
    {
        // bit inefficient for now
        for (auto &r : rows)
        {
            if (r.modNum == modnum)
            {
                char pdisp[TXT_SIZE];
                int ptag = r.p->id;
                modsources thisms = (modsources)r.source_id;
                r.p->get_display_of_modulation_depth(pdisp, moded->synth->getModDepth(ptag, thisms),
                                                     moded->synth->isBipolarModulation(thisms),
                                                     Parameter::InfoWindow, &(r.mss));
                r.depth = moded->synth->getModDepth(ptag, thisms);
            }
        }
    }
    void updateRows()
    {
        rows.clear();
        std::ostringstream oss;
        int modNum = 0;
        auto append = [&oss, &modNum, this](const std::string &type,
                                            const std::vector<ModulationRouting> &r, int idBase) {
            if (r.empty())
                return;

            oss << type << "\n";
            auto h = Datum(Datum::HEADER);
            h.hLab = type;
            h.modNum = -1;
            rows.push_back(h);

            char nm[TXT_SIZE], dst[TXT_SIZE];
            for (auto q : r)
            {
                SurgeSynthesizer::ID ptagid;
                if (moded->synth->fromSynthSideId(q.destination_id + idBase, ptagid))
                    moded->synth->getParameterName(ptagid, nm);
                std::string sname = moded->ed->modulatorName(q.source_id, false);
                auto rDisp = Datum(Datum::SHOW_ROW);
                rDisp.source_id = q.source_id;
                rDisp.dest_id = q.destination_id + idBase;
                rDisp.source_name = sname;
                rDisp.dest_name = nm;
                rDisp.modNum = modNum++;
                rDisp.p = moded->synth->storage.getPatch().param_ptr[rDisp.dest_id];

                char pdisp[256];
                auto p = moded->synth->storage.getPatch().param_ptr[q.destination_id + idBase];
                int ptag = p->id;
                modsources thisms = (modsources)q.source_id;
                p->get_display_of_modulation_depth(pdisp, moded->synth->getModDepth(ptag, thisms),
                                                   moded->synth->isBipolarModulation(thisms),
                                                   Parameter::InfoWindow, &(rDisp.mss));
                rDisp.depth = moded->synth->getModDepth(ptag, thisms);
                rDisp.isBipolar = moded->synth->isBipolarModulation(thisms);
                rows.push_back(rDisp);

                rDisp.type = Datum::EDIT_ROW;
                rows.push_back(rDisp);

                oss << "    > " << this->moded->ed->modulatorName(q.source_id, false) << " to "
                    << nm << " at " << q.depth << "\n";
            }
        };
        append("Global Modulators", moded->synth->storage.getPatch().modulation_global, 0);
        append("Scene A - Voice Modulators",
               moded->synth->storage.getPatch().scene[0].modulation_voice,
               moded->synth->storage.getPatch().scene_start[0]);
        append("Scene A - Scene Modulators",
               moded->synth->storage.getPatch().scene[0].modulation_scene,
               moded->synth->storage.getPatch().scene_start[0]);
        append("Scene B - Voice Modulators",
               moded->synth->storage.getPatch().scene[1].modulation_voice,
               moded->synth->storage.getPatch().scene_start[1]);
        append("Scene B - Scene Modulators",
               moded->synth->storage.getPatch().scene[1].modulation_scene,
               moded->synth->storage.getPatch().scene_start[0]);
        debugRows = oss.str();
    }

    juce::Component *refreshComponentForRow(int rowNumber, bool isRowSelected,
                                            juce::Component *existingComponentToUpdate) override
    {
        auto row = existingComponentToUpdate;
        if (rowNumber >= 0 && rowNumber < rows.size())
        {
            if (row)
            {
                bool replace = false;
                switch (rows[rowNumber].type)
                {
                case Datum::HEADER:
                    replace = !dynamic_cast<HeaderComponent *>(row);
                    break;
                case Datum::SHOW_ROW:
                    replace = !dynamic_cast<ShowComponent *>(row);
                    break;
                case Datum::EDIT_ROW:
                    replace = !dynamic_cast<EditComponent *>(row);
                    break;
                }
                if (replace)
                {
                    delete row;
                    row = nullptr;
                }
            }
            if (!row)
            {
                switch (rows[rowNumber].type)
                {
                case Datum::HEADER:
                    row = new HeaderComponent(this, rowNumber);
                    break;
                case Datum::SHOW_ROW:
                    row = new ShowComponent(this, rowNumber);
                    break;
                case Datum::EDIT_ROW:
                    row = new EditComponent(this, rowNumber);
                    break;
                }
            }
        }
        else
        {
            if (row)
            {
                // Nothing to display, free the custom component
                delete row;
                row = nullptr;
            }
        }

        return row;
    }

    ModulationEditor *moded;
};

ModulationEditor::ModulationEditor(SurgeGUIEditor *ed, SurgeSynthesizer *s)
    : ed(ed), synth(s), juce::Component("Modulation Editor")
{
    listBoxModel = std::make_unique<ModulationListBoxModel>(this);
    listBox = std::make_unique<juce::ListBox>("Mod Editor List", listBoxModel.get());
    listBox->setBounds(5, 5, 740, 440);
    listBox->setRowHeight(18);
    addAndMakeVisible(*listBox);
}

void ModulationEditor::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }
void ModulationEditor::resized()
{
    if (listBox)
        listBox->setBounds(2, 2, getWidth() - 4, getHeight() - 4);
}
ModulationEditor::~ModulationEditor() = default;

} // namespace Overlays
} // namespace Surge
