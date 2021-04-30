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

ModulationEditor::ModulationEditor(SurgeGUIEditor *ed, SurgeSynthesizer *s)
    : ed(ed), synth(s), juce::Component("Modulation Editor")
{
    setSize(750, 450); // FIXME

    textBox = std::make_unique<juce::TextEditor>("Mod editor text box");
    textBox->setMultiLine(true, false);
    textBox->setBounds(5, 5, 740, 440);
    textBox->setFont(Surge::GUI::getFontManager()->getLatoAtSize(9));
    addAndMakeVisible(*textBox);

    /*
     * Obviously clean this up
     */
    std::ostringstream oss;
    auto append = [&oss, this](const std::string &type, const std::vector<ModulationRouting> &r) {
        if (r.empty())
            return;
        oss << type << "\n";
        char nm[TXT_SIZE], dst[TXT_SIZE];
        for (auto q : r)
        {
            SurgeSynthesizer::ID ptagid;
            if (synth->fromSynthSideId(q.destination_id, ptagid))
                synth->getParameterName(ptagid, nm);
            ;
            oss << "    > " << this->ed->modulatorName(q.source_id, false) << " to " << nm << " at "
                << q.depth << "\n";
        }
    };
    append("Global", synth->storage.getPatch().modulation_global);
    append("Scene A Voice", synth->storage.getPatch().scene[0].modulation_voice);
    append("Scene A Scene", synth->storage.getPatch().scene[0].modulation_scene);
    append("Scene B Voice", synth->storage.getPatch().scene[1].modulation_voice);
    append("Scene B Scene", synth->storage.getPatch().scene[1].modulation_scene);

    textBox->setText(oss.str());
}

ModulationEditor::~ModulationEditor() = default;