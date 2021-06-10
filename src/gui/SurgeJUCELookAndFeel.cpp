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

#include "SurgeJUCELookAndFeel.h"
void SurgeJUCELookAndFeel::drawLabel(juce::Graphics &graphics, juce::Label &label)
{
    LookAndFeel_V4::drawLabel(graphics, label);
}
void SurgeJUCELookAndFeel::drawTextEditorOutline(juce::Graphics &g, int width, int height,
                                                 juce::TextEditor &textEditor)
{
    if (dynamic_cast<juce::AlertWindow *>(textEditor.getParentComponent()) == nullptr)
    {
        if (textEditor.isEnabled())
        {
            if (textEditor.hasKeyboardFocus(true) && !textEditor.isReadOnly())
            {
                g.setColour(textEditor.findColour(juce::TextEditor::focusedOutlineColourId));
                // This is the only change from V4; use a 1 rather than 2 here
                g.drawRect(0, 0, width, height, 1);
            }
            else
            {
                g.setColour(textEditor.findColour(juce::TextEditor::outlineColourId));
                g.drawRect(0, 0, width, height);
            }
        }
    }
}
