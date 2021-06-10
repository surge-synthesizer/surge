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

#include "PatchDBViewer.h"
#include "PatchDB.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"

namespace Surge
{
namespace Overlays
{
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
        // FIXME - make sure the codition this is handling is handled everywhere consistently
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
        g.setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
        g.drawText(s.c_str(), 0, 0, width, height, juce::Justification::centredLeft);
    }

    void cellDoubleClicked(int rowNumber, int columnId, const juce::MouseEvent &event) override
    {
        // FIXME - make sure the codition this is handling is handled everywhere consistently
        if (rowNumber >= data.size())
        {
            return;
        }

        auto d = data[rowNumber];
        editor->queuePatchFileLoad(d.file);
        editor->closePatchBrowserDialog();
    }

    void executeQuery(const std::string &n) { data = storage->patchDB->rawQueryForNameLike(n); }

    std::vector<Surge::PatchStorage::PatchDB::record> data;
    SurgeStorage *storage;
    SurgeGUIEditor *editor;
};

PatchDBViewer::PatchDBViewer(SurgeGUIEditor *e, SurgeStorage *s)
    : editor(e), storage(s), juce::Component("PatchDB Viewer")
{
    createElements();
}
PatchDBViewer::~PatchDBViewer() = default;

void PatchDBViewer::createElements()
{
    setSize(750, 450); // cleaner obvs
    tableModel = std::make_unique<PatchDBSQLTableModel>(editor, storage);
    table = std::make_unique<juce::TableListBox>("Patch Table", tableModel.get());
    table->getHeader().addColumn("id", 1, 40);
    table->getHeader().addColumn("name", 2, 200);
    table->getHeader().addColumn("category", 3, 250);
    table->getHeader().addColumn("author", 4, 200);

    table->setBounds(0, 50, getWidth(), getHeight() - 50);
    table->setRowHeight(18);
    addAndMakeVisible(*table);

    nameTypein = std::make_unique<juce::TextEditor>("Patch Name");
    nameTypein->setBounds(10, 10, 400, 30);
    nameTypein->addListener(this);
    addAndMakeVisible(*nameTypein);

    executeQuery();
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
    if (nameTypein)
        nameTypein->setBounds(10, 10, 400, 30);

    if (table)
        table->setBounds(2, 50, getWidth() - 4, getHeight() - 52);
}

} // namespace Overlays
} // namespace Surge