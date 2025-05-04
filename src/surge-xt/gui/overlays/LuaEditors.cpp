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

#include "LuaEditors.h"
#include "SurgeGUIEditor.h"
#include "RuntimeFont.h"
#include "SkinSupport.h"
#include "SkinColors.h"
#include "WavetableScriptEvaluator.h"
#include "LuaSupport.h"
#include "SurgeImage.h"
#include "SurgeImageStore.h"
#include "widgets/MultiSwitch.h"
#include "widgets/NumberField.h"
#include "overlays/TypeinParamEditor.h"
#include "widgets/MenuCustomComponents.h"
#include <fmt/core.h>
#include "widgets/OscillatorWaveformDisplay.h"
#include "util/LuaTokeniserSurge.h"

namespace Surge
{
namespace Overlays
{

/*
TextfieldPopup
Base class that can be used for creating other textfield popups like the search
*/
TextfieldButton::TextfieldButton(juce::String &svg, int rowToAddTo) : juce::Component()
{
    row = rowToAddTo;
    xml = juce::XmlDocument::parse(svg);

    svgGraphics = juce::Drawable::createFromSVG(*xml);

    setBounds(juce::Rectangle(0, 0, TextfieldPopup::STYLE_BUTTON_SIZE,
                              TextfieldPopup::STYLE_BUTTON_SIZE));

    addAndMakeVisible(*svgGraphics);
    const juce::Rectangle bounds =
        juce::Rectangle(0.f, 0.f, (float)TextfieldPopup::STYLE_BUTTON_SIZE,
                        (float)TextfieldPopup::STYLE_BUTTON_SIZE);

    svgGraphics->setTransformToFit(bounds, juce::RectanglePlacement::yTop);
}

void TextfieldButton::paint(juce::Graphics &g)
{
    Component::paint(g);
    auto bounds = getBounds();

    float alpha = isMouseOver && enabled ? 0.2 : 0;

    g.setFillType(juce::FillType(juce::Colours::white.withAlpha(alpha)));

    g.fillRoundedRectangle(0, 0, TextfieldPopup::STYLE_BUTTON_SIZE,
                           TextfieldPopup::STYLE_BUTTON_SIZE, 2);
}

void TextfieldButton::setEnabled(bool v)
{
    enabled = v;

    updateGraphicState();
    repaint();
}

void TextfieldButton::mouseEnter(const juce::MouseEvent &event)
{
    isMouseOver = true;
    repaint();
}

void TextfieldButton::mouseExit(const juce::MouseEvent &event)
{
    isMouseOver = false;
    repaint();
}

void TextfieldButton::mouseDown(const juce::MouseEvent &event) { isMouseDown = true; }

void TextfieldButton::mouseUp(const juce::MouseEvent &event)
{
    if (isSelectable)
    {
        select(isSelected() == false ? true : false);
    }

    if (isMouseDown)
    {
        onClick();
    }

    isMouseDown = false;
}

void TextfieldButton::updateGraphicState()
{
    svgGraphics->setAlpha(enabled ? (isSelectable && selected) || (!isSelectable) ? 1 : 0.5 : 0.15);
}

void TextfieldButton::setSelectable()
{
    isSelectable = true;
    updateGraphicState();
    repaint();
}

void TextfieldButton::select(bool v)
{
    selected = v;
    updateGraphicState();
    repaint();
}

Textfield::Textfield(int r) : juce::TextEditor() { row = r; };

void Textfield::paint(juce::Graphics &g)
{
    juce::TextEditor::paint(g);

    if (isEmpty())
    {
        // draw prefix text
        g.setColour(colour.withAlpha(0.5f));

        g.setFont(getFont());
        const auto bounds = getBounds();
        const int border = getBorder().getLeft();
        const auto textBounds =
            juce::Rectangle(getLeftIndent() + border, 0, bounds.getWidth(), bounds.getHeight());

        g.drawText(header, textBounds, juce::Justification::verticallyCentred, true);
    }
}

void Textfield::focusLost(FocusChangeType)
{
    if (onFocusLost != nullptr)
    {
        onFocusLost();
    }
}

void Textfield::setHeader(juce::String h) { header = h; }

void Textfield::setHeaderColor(juce::Colour c) { colour = c; }

void Textfield::setColour(int colourID, juce::Colour newColour)
{
    Component::setColour(colourID, newColour);
    colour = newColour;
}

TextfieldPopup::TextfieldPopup(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t skin)
    : juce::Component(), juce::TextEditor::Listener(), juce::KeyListener(),
      Surge::GUI::SkinConsumingComponent()
{
    ed = &editor;
    currentSkin = skin;

    juce::Rectangle boundsLabel = juce::Rectangle(95, 2, 80, 20);

    createTextfield(0);

    labelResult = std::make_unique<juce::Label>();

    labelResult->setBounds(boundsLabel);
    labelResult->setFont(juce::FontOptions(10));
    labelResult->setJustificationType(juce::Justification::left);
    labelResult->setColour(juce::Label::textColourId, juce::Colour(255, 255, 255));

    addAndMakeVisible(*labelResult);

    setBounds(juce::Rectangle(0, 0, 150, 20));

    setVisible(false);
    resize();
}

void TextfieldPopup::showRows(int r)
{
    rowsVisible = r;
    resize();
}

void TextfieldPopup::paint(juce::Graphics &g)
{
    const auto bounds = getBounds();
    const auto rect = juce::Rectangle(0, 0, bounds.getWidth(), bounds.getHeight());

    const auto color = currentSkin->getColor(Colors::FormulaEditor::Background).darker(1.3);

    g.setFillType(juce::FillType(color));
    g.fillRect(rect);

    juce::Component::paint(g);
}

void TextfieldPopup::show()
{
    setVisible(true);
    textfield[0]->grabKeyboardFocus();
}

void TextfieldPopup::hide() { setVisible(false); }

void TextfieldPopup::textEditorEscapeKeyPressed(juce::TextEditor &) { hide(); }

void TextfieldPopup::onClick(std::unique_ptr<TextfieldButton> &btn) {}

void TextfieldPopup::createButton(juce::String svg, int row)
{
    button[buttonCount] = std::make_unique<TextfieldButton>(svg, row);
    auto btn = &button[buttonCount];

    auto callback = [this, btn]() { onClick(*btn); };
    button[buttonCount]->onClick = callback;
    addAndMakeVisible(*button[buttonCount]);

    buttonCount++;
}

void TextfieldPopup::setText(int id, std ::string str) { textfield[id]->setText(str); }
juce::String TextfieldPopup::getText(int id) { return textfield[id]->getText(); }

void TextfieldPopup::createTextfield(int row)
{
    textfield[textfieldCount] = std::make_unique<Textfield>(row);

    textfield[textfieldCount]->setBorder(juce::BorderSize(-1, 4, 0, 4));
    textfield[textfieldCount]->setFont(
        currentSkin->fontManager->getLatoAtSize(9.5, juce::Font::plain));

    textfield[textfieldCount]->setColour(juce::TextEditor::ColourIds::textColourId,
                                         currentSkin->getColor(Colors::Dialog::Button::Text));

    textfield[textfieldCount]->setColour(
        juce::TextEditor::backgroundColourId,
        currentSkin->getColor(Colors::FormulaEditor::Background).darker(0.4));

    textfield[textfieldCount]->setColour(
        juce::TextEditor::focusedOutlineColourId,
        currentSkin->getColor(Colors::FormulaEditor::Background).brighter(0.08));
    textfield[textfieldCount]->setColour(
        juce::TextEditor::outlineColourId,
        currentSkin->getColor(Colors::FormulaEditor::Background).brighter(0));

    textfield[textfieldCount]->setHeaderColor(currentSkin->getColor(Colors::Dialog::Button::Text));

    textfield[textfieldCount]->addListener(this);
    textfield[textfieldCount]->addKeyListener(this);

    addAndMakeVisible(*textfield[textfieldCount]);

    textfield[textfieldCount]->setText("");
    textfield[textfieldCount]->setEscapeAndReturnKeysConsumed(true);

    textfield[textfieldCount]->onFocusLost = [this]() {
        if (this->onFocusLost != nullptr)
        {
            this->onFocusLost();
        }
    };

    textfieldCount++;
}

void TextfieldPopup::setTextWidth(int w) { textWidth = w; }

void TextfieldPopup::setHeader(juce::String t) { textfield[0]->setHeader(t); }

bool TextfieldPopup::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent)
{
    return originatingComponent->keyPressed(key);
}

void TextfieldPopup::setButtonOffsetAtRow(int row, int offset) { buttonOffset[row] = offset; }

void TextfieldPopup::resize()
{
    int maxLeft[4] = {0};

    for (int i = 0; i < buttonCount; i++)
    {
        maxLeft[button[i]->row] += 1;
    }

    int buttonTotalInRow = *std::max_element(maxLeft, maxLeft + 4);

    int marginBetweenTextAndButtons =
        buttonTotalInRow > 0 ? STYLE_MARGIN_BETWEEN_TEXT_AND_BUTTONS : 0;
    int totalWidth = STYLE_MARGIN + textWidth + marginBetweenTextAndButtons + STYLE_MARGIN +
                     (STYLE_BUTTON_SIZE + STYLE_BUTTON_MARGIN) * buttonTotalInRow;

    int textHeight = rowsVisible <= 1
                         ? STYLE_TEXT_HEIGHT
                         : STYLE_TEXT_HEIGHT * rowsVisible + ((rowsVisible - 1) * STYLE_ROW_MARGIN);

    int totalHeight = textHeight + STYLE_MARGIN * 2;

    totalWidth += buttonTotalInRow > 0 ? STYLE_MARGIN * 2 : 0; // auto auto auto

    const auto bounds = juce::Rectangle(
        ed->getWidth() - totalWidth - ed->getScrollbarThickness() + 2, 2, totalWidth, totalHeight);

    setBounds(bounds);

    const auto boundsLabel = labelResult->getBounds();

    labelResult->setBounds(STYLE_MARGIN + textWidth + STYLE_MARGIN,
                           STYLE_MARGIN + STYLE_TEXT_HEIGHT * 0.5 - boundsLabel.getHeight() * 0.5,
                           boundsLabel.getWidth(), boundsLabel.getHeight());

    for (int i = 0; i < textfieldCount; i++)
    {
        auto y = STYLE_MARGIN + (STYLE_ROW_MARGIN + STYLE_TEXT_HEIGHT) * i;
        textfield[i]->setBounds(STYLE_MARGIN, y, textWidth, STYLE_TEXT_HEIGHT);
    }

    int x[4] = {0};

    for (int i = 0; i < buttonCount; i++)
    {
        int buttonY = STYLE_MARGIN + button[i]->row * (STYLE_TEXT_HEIGHT + STYLE_ROW_MARGIN) +
                      STYLE_TEXT_HEIGHT * 0.5 - STYLE_BUTTON_SIZE * 0.5;

        int buttonX = x[button[i]->row] + (buttonOffset[button[i]->row]);

        auto bounds = juce::Rectangle(STYLE_MARGIN + textWidth + STYLE_MARGIN +
                                          marginBetweenTextAndButtons + STYLE_MARGIN + buttonX,
                                      buttonY, STYLE_BUTTON_SIZE, STYLE_BUTTON_SIZE);
        button[i]->setBounds(bounds);

        x[button[i]->row] += (STYLE_BUTTON_SIZE + STYLE_BUTTON_MARGIN);
    }
}

// ---------------------------------------

GotoLine::GotoLine(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t skin)
    : TextfieldPopup(editor, skin)
{
    STYLE_MARGIN_BETWEEN_TEXT_AND_BUTTONS = 0;
    // close
    createButton({R"(
<svg height="24" width="24" fill="#FFFFFF"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 8.2546475,-942.56172 -1.1099577,-1.10942 4.4397962,-4.44028 -4.4397962,-4.43942 1.1099577,-1.11028 4.4398655,4.44028 4.439762,-4.44028 1.110026,1.11028 -4.439848,4.43942 4.439848,4.44028 -1.110026,1.10942 -4.439762,-4.43942 z"/></svg>)"},
                 0);
    setHeader("Go to line");
    setTextWidth(130);
}

bool GotoLine::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent)
{
    auto command = key.getModifiers().isCommandDown();

    if (!command)
        originatingComponent->keyPressed(key);
    if (key.getKeyCode() == key.returnKey)
    {
        hide();
        ed->grabKeyboardFocus();
    }
    else if (key.getKeyCode() == key.escapeKey)
    {
        hide();
        ed->moveCaretTo(startCaretPosition, false);
        ed->scrollToLine(startScroll);
        ed->grabKeyboardFocus();
    }
    else
    {
        int line = std::max(0, textfield[0]->getText().getIntValue() - 1);
        line = std::min(ed->getDocument().getNumLines(), line);

        int numLines = ed->getNumLinesOnScreen();
        currentLine = line;

        ed->scrollToLine(std::max(0, line - int(numLines * 0.5)));

        auto textLineLength = ed->getDocument().getLine(line).length();
        auto pos = juce::CodeDocument::Position(ed->getDocument(), line, textLineLength);
        ed->moveCaretTo(pos, false);
    }

    ed->repaint();

    return command == false;
}

void GotoLine::show()
{
    currentLine = ed->getCaretPos().getLineNumber();
    textfield[0]->setText("");
    textfield[0]->setInputRestrictions(4, "0123456789");
    TextfieldPopup::show();
    startCaretPosition = ed->getCaretPos();
    startScroll = ed->getFirstLineOnScreen();
}

void GotoLine::hide()
{
    TextfieldPopup::hide();
    ed->repaint();
}

void GotoLine::focusLost(FocusChangeType) { hide(); }

int GotoLine::getCurrentLine() { return currentLine; }

void GotoLine::onClick(std::unique_ptr<TextfieldButton> &btn) { hide(); }

/*
---------------------------------------
Search box
---------------------------------------
*/

CodeEditorSearch::CodeEditorSearch(juce::CodeEditorComponent &editor, Surge::GUI::Skin::ptr_t skin)
    : TextfieldPopup(editor, skin)
{
    setTextWidth(110);
    setButtonOffsetAtRow(1, -STYLE_MARGIN_BETWEEN_TEXT_AND_BUTTONS);

    // Case sensitivity
    createButton(
        {R"(<svg width="24" height="24" fill="#ffffff"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 1.7664625,-941.78764 4.8158546,-12.84227 h 2.3057731 l 4.8158558,12.84227 h -2.218212 l -1.138293,-3.26893 H 5.1521543 l -1.1674799,3.26893 z m 4.0569928,-5.13691 H 9.6469522 L 7.778984,-952.23658 H 7.6622364 Z m 11.5288647,5.42879 q -1.488537,0 -2.364147,-0.80264 -0.875609,-0.80265 -0.875609,-2.11606 0,-1.28424 1.006952,-2.11606 1.00695,-0.83183 2.583048,-0.83183 0.671301,0 1.313415,0.11675 0.642114,0.11675 1.109107,0.32105 v -0.35024 q 0,-0.84642 -0.598334,-1.37178 -0.598334,-0.52538 -1.590692,-0.52538 -0.671301,0 -1.225854,0.27729 -0.554552,0.27727 -0.963171,0.80264 l -1.371788,-1.02155 q 0.700487,-0.84643 1.590691,-1.25504 0.890203,-0.40862 1.999309,-0.40862 2.013903,0 3.006261,0.94858 0.992358,0.94858 0.992358,2.84573 v 5.19528 h -1.83878 v -1.07991 h -0.116749 q -0.408618,0.6713 -1.109106,1.02155 -0.700488,0.35024 -1.546911,0.35024 z m 0.350244,-1.5761 q 1.021546,0 1.736627,-0.70048 0.715081,-0.70049 0.715081,-1.63448 -0.408618,-0.2335 -0.977764,-0.36483 -0.569147,-0.13135 -1.123699,-0.13135 -0.933985,0 -1.45935,0.40862 -0.525367,0.40861 -0.525367,1.07992 0,0.58374 0.466992,0.96317 0.466993,0.37943 1.16748,0.37943 z"/></svg>)"}, 0);
    button[0]->setSelectable();

    // whole word match
    createButton(
        {R"(<svg width="24" height="24" fill="#ffffff"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path fill-rule="evenodd" clip-rule="evenodd" d="M 2.8719134,-956.14323 H 21.300668 v 1.31634 H 2.8719134 Z m 17.1124196,2.63268 h -1.316344 v 10.53071 h 1.316344 z m -3.590975,5.77477 a 3.0973468,3.0973468 0 0 0 -0.473887,-1.03464 2.2983286,2.2983286 0 0 0 -0.801647,-0.69765 2.411534,2.411534 0 0 0 -1.13995,-0.25406 c -0.260634,0 -0.500212,0.0316 -0.720037,0.096 a 2.3167574,2.3167574 0 0 0 -0.596307,0.26985 2.2693691,2.2693691 0 0 0 -0.480458,0.41859 l -0.23563,0.33962 v -4.15173 h -1.17549 v 9.76987 h 1.17549 v -0.7569 l 0.165858,0.23036 c 0.114521,0.13427 0.248787,0.2501 0.400168,0.3541 0.154011,0.10265 0.327767,0.18428 0.523907,0.24483 0.19613,0.0605 0.413325,0.0895 0.655533,0.0895 0.464671,0 0.876686,-0.0934 1.233415,-0.27906 0.358039,-0.18824 0.656852,-0.44493 0.897742,-0.77139 0.240888,-0.32908 0.422541,-0.71477 0.544961,-1.15706 0.122419,-0.44492 0.184293,-0.92538 0.184293,-1.44401 a 4.9441711,4.9441711 0 0 0 -0.157961,-1.26633 z m -1.946873,-0.79902 c 0.198769,0.0934 0.371215,0.23168 0.513379,0.41334 0.143475,0.18429 0.255366,0.41201 0.335664,0.68054 0.06714,0.22905 0.107943,0.48837 0.117151,0.7727 l -0.0092,0.16454 c 0,0.43044 -0.04344,0.81613 -0.131636,1.14389 a 2.4826162,2.4826162 0 0 1 -0.365945,0.80823 c -0.154011,0.21326 -0.342245,0.37517 -0.55418,0.48179 -0.423862,0.21325 -1.000415,0.21851 -1.407162,0.0198 a 1.6638531,1.6638531 0 0 1 -0.517327,-0.38963 1.6757001,1.6757001 0 0 1 -0.286958,-0.4831 c 0,0 -0.235621,-0.58841 -0.235621,-1.24658 0,-0.65816 0.235621,-1.31896 0.235621,-1.31896 0.08161,-0.233 0.179024,-0.41859 0.294864,-0.56603 0.150062,-0.18824 0.336976,-0.34094 0.558128,-0.45414 0.221144,-0.1132 0.480459,-0.16981 0.772685,-0.16981 0.250105,0 0.479147,0.0487 0.680546,0.14348 z m 6.854183,6.8713 H 2.8719134 v 1.31634 H 21.300668 Z M 5.2584357,-945.61252 4.30014,-942.93903 H 2.8719134 l 0.032911,-0.0948 3.2131816,-9.32758 h 1.238675 l 3.271106,9.42236 H 9.2035093 l -1.0241095,-2.67349 z m 1.4756227,-4.70986 h -0.028962 l -1.1912865,3.62387 h 2.4233823 z"/></svg>)"},
        0);
    button[1]->setSelectable();

    // arrow up
    createButton(
        {R"(<svg height="24" width="24" fill="#ffffff"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 11.068654,-940.68461 v -12.05133 l -5.5431153,5.54312 -1.3857783,-1.41053 7.9187346,-7.91873 7.918733,7.91873 -1.385777,1.41053 -5.543114,-5.54312 v 12.05133 z"/></svg>)"},
        0);

    // arrow down
    createButton({R"(
<svg height="24" width="24" fill="#FFFFFF"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 11.120883,-955.82203 v 12.05132 l -5.5431141,-5.54311 -1.3857786,1.41052 7.9187347,7.91874 7.918734,-7.91874 -1.385778,-1.41052 -5.543114,5.54311 v -12.05132 z"/></svg>)"},
                 0);

    // replace one
    createButton({R"(
<svg height="24" width="24" fill="#FFFFFF"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 12.0417,-939.991 c -1.1516,0 -2.23389,-0.219 -3.24678,-0.656 -1.01289,-0.437 -1.89397,-1.03 -2.64323,-1.779 -0.74926,-0.749 -1.34242,-1.63 -1.77949,-2.643 -0.43707,-1.013 -0.65561,-2.096 -0.65561,-3.247 0,-1.152 0.21854,-2.234 0.65561,-3.247 0.43707,-1.013 1.03023,-1.894 1.77949,-2.643 0.74926,-0.749 1.63034,-1.343 2.64323,-1.78 1.01289,-0.437 2.09518,-0.655 3.24678,-0.655 1.1517,0 2.2339,0.218 3.2468,0.655 1.0129,0.437 1.894,1.031 2.6432,1.78 0.7493,0.749 1.3425,1.63 1.7795,2.643 0.4371,1.013 0.6556,2.095 0.6556,3.247 0,1.151 -0.2185,2.234 -0.6556,3.247 -0.437,1.013 -1.0302,1.894 -1.7795,2.643 -0.7492,0.749 -1.6303,1.342 -2.6432,1.779 -1.0129,0.437 -2.0951,0.656 -3.2468,0.656 z m 0,-1.665 c 1.8593,0 3.4341,-0.645 4.7245,-1.936 1.2904,-1.29 1.9356,-2.865 1.9356,-4.724 0,-1.859 -0.6452,-3.434 -1.9356,-4.725 -1.2904,-1.29 -2.8652,-1.935 -4.7245,-1.935 -1.8593,0 -3.4341,0.645 -4.72449,1.935 -1.2904,1.291 -1.93559,2.866 -1.93559,4.725 0,1.859 0.64519,3.434 1.93559,4.724 1.29039,1.291 2.86519,1.936 4.72449,1.936 z m 0,-6.66 z m -0.4162,4.162 h 1.665 v -8.325 H 9.96043 v 1.665 h 1.66507 z"/></svg>)"},
                 1);

    // replace all
    createButton({R"(
<svg height="24" width="24" fill="#FFFFFF"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 3.0417,-943.071 v -2 h 14 v 2 z m 2,-4 v -2 h 14 v 2 z m 2,-4 v -2 h 14 v 2 z"/></svg>)"},
                 1);

    // close
    createButton({R"(
<svg height="24" width="24" fill="#FFFFFF"><path fill="none" d="m 0.07087901,-959.94314 23.93899499,-0.002 v 23.939 l -23.85812279,-0.0801 z"/><path d="m 8.2546475,-942.56172 -1.1099577,-1.10942 4.4397962,-4.44028 -4.4397962,-4.43942 1.1099577,-1.11028 4.4398655,4.44028 4.439762,-4.44028 1.110026,1.11028 -4.439848,4.43942 4.439848,4.44028 -1.110026,1.10942 -4.439762,-4.43942 z"/></svg>)"},
                 0);

    setHeader("Find");

    createTextfield(1);
    textfield[1]->setHeader("Replace");

    // showRows(2);

    repaint();
}

void CodeEditorSearch::onClick(std::unique_ptr<TextfieldButton> &btn)
{
    // case sensitive
    if (btn == button[0])
    {
        search(true);
    }

    // whole word
    if (btn == button[1])
    {
        search(true);
    }

    // previous
    if (btn == button[2])
    {
        showResult(-1, true);
    }

    // next
    if (btn == button[3])
    {
        showResult(1, true);
    }

    // replace
    if (btn == button[4])
    {
        replaceResults(false);
    }

    // replace all
    if (btn == button[5])
    {
        replaceResults(true);
    }

    if (btn == button[6])
    {
        hide();
    }
}

int *CodeEditorSearch::getResult() { return result; }

int CodeEditorSearch::getResultTotal() { return resultTotal; }

bool CodeEditorSearch::isActive() { return active; }

void CodeEditorSearch::mouseDown(const juce::MouseEvent &event) { saveCaretStartPosition(true); }

void CodeEditorSearch::saveCaretStartPosition(bool onlyReadCaretPosition)
{
    if (!ed->isReadOnly())
    {
        if (!saveCaretStartPositionLock &&
            onlyReadCaretPosition == false) // prevent caretMoved feedback loop
        {
            saveCaretStartPositionLock = true;
            auto sel = ed->getHighlightedRegion();

            if (sel.getEnd() - sel.getStart() != 0)
            {
                // move caret to beginning of selected
                if (ed->getCaretPosition() > sel.getStart())
                {
                    juce::CodeDocument::Position pos =
                        juce::CodeDocument::Position(ed->getDocument(), sel.getStart());
                    ed->moveCaretTo(pos, false);
                }
            }
            startCaretPosition = ed->getCaretPos();

            saveCaretStartPositionLock = false;
        }

        if (onlyReadCaretPosition && !saveCaretStartPositionLock)
        {
            startCaretPosition = ed->getCaretPos();
        }
    }
}

void CodeEditorSearch::focusLost(FocusChangeType)
{
    removeHighlightColors();
    ed->repaint();
}

void CodeEditorSearch::setHighlightColors()
{
    auto color = currentSkin->getColor(Colors::FormulaEditor::Background);

    ed->setColour(
        juce::CodeEditorComponent::highlightColourId,
        color.interpolatedWith(currentSkin->getColor(Colors::FormulaEditor::Lua::Keyword), 0.6));
}

void CodeEditorSearch::removeHighlightColors()
{
    ed->setColour(juce::CodeEditorComponent::highlightColourId,
                  currentSkin->getColor(Colors::FormulaEditor::Highlight));
}

void CodeEditorSearch::show()
{
    TextfieldPopup::show();

    // set selected text as search query unless it includes a newline character
    auto sel = ed->getHighlightedRegion();
    juce::String txt = ed->getTextInRange(sel);

    saveCaretStartPosition(false);

    if (!txt.containsChar('\n') && sel.getLength() != 0)
    {
        textfield[0]->setText(txt);
    }

    textfield[0]->moveCaretToStartOfLine(false);
    textfield[0]->moveCaretToEndOfLine(true);

    search(true);
    textfield[0]->grabKeyboardFocus();
    ed->repaint(); // force update selection color
}

void CodeEditorSearch::hide()
{
    TextfieldPopup::hide();
    removeHighlightColors();
    ed->repaint();
}

void CodeEditorSearch::textEditorEscapeKeyPressed(juce::TextEditor &) { hide(); }

void CodeEditorSearch::textEditorReturnKeyPressed(juce::TextEditor &) {}

bool CodeEditorSearch::keyPressed(const juce::KeyPress &key, juce::Component *originatingComponent)
{
    auto keyCode = key.getKeyCode();

    if (key.getKeyCode() == key.tabKey && rowsVisible > 1)
    {
        if (originatingComponent == textfield[0].get())
        {
            textfield[1]->grabKeyboardFocus();
            textfield[1]->moveCaretToEnd();
            textfield[1]->moveCaretToStartOfLine(false);
            textfield[1]->moveCaretToEndOfLine(true);
        }
        else
        {
            textfield[0]->grabKeyboardFocus();
            textfield[0]->moveCaretToStartOfLine(false);
            textfield[0]->moveCaretToEndOfLine(true);
        }
        return true;
    }

    if (key.getKeyCode() == key.returnKey)
    {
        // next / previous search
        if (originatingComponent == textfield[0].get())
        {
            if (key.getModifiers().isShiftDown())
            {
                showResult(-1, true);
            }
            else
            {
                showResult(1, true);
            }
        }
        // replace next / all
        else
        {
            if (key.getModifiers().isShiftDown())
            {
                replaceResults(true);
            }
            else
            {
                replaceResults(false);
            }
        }
        return true;
    }

    if (key.getKeyCode() == key.escapeKey)
    {
        hide();
        return true;
    }

    return false;
}

void CodeEditorSearch::replaceResults(bool all)
{
    if (resultTotal > 0)
    {
        resultHasChanged = true;

        if (all)
        {
            int iterationCount = 0;
            int iterationMax = resultTotal;

            while (resultTotal > 0)
            {
                replaceCurrentResult(textfield[1]->getText());
            }
        }
        else
        {
            replaceCurrentResult(textfield[1]->getText());
        }
    }
}

void CodeEditorSearch::replaceCurrentResult(juce::String txt)
{
    if (resultTotal != 0)
    {
        ed->getDocument().replaceSection(
            result[resultCurrent], result[resultCurrent] + textfield[0]->getTotalNumChars(), txt);

        int diff = txt.length() - latestSearch.length();

        for (int i = 0; i < resultTotal; i++)
        {
            if (i >= resultCurrent)
            {
                int nextResult = i + 1 < 512 ? result[i + 1] : 0;
                result[i] = nextResult;
                result[i] += diff;
            }
        }

        resultTotal--;

        showResult(0, true);
    }
}

void CodeEditorSearch::textEditorTextChanged(juce::TextEditor &textEditor)
{
    juce::TextEditor *other = dynamic_cast<juce::TextEditor *>(textfield[0].get());

    if (&textEditor == other)
        search(true);
}

void CodeEditorSearch::setCurrentResult(int res)
{
    resultCurrent = res;
    showResult(0, true);
}

void CodeEditorSearch::showResult(int increment, bool moveCaret)
{
    int id = resultCurrent + 1;

    // up / down
    button[2]->setEnabled(resultTotal < 2 ? false : true);
    button[3]->setEnabled(resultTotal < 2 ? false : true);

    // replace
    button[4]->setEnabled(resultTotal == 0 ? false : true);
    button[5]->setEnabled(resultTotal == 0 ? false : true);

    if (resultTotal == 0)
    {
        auto bgColor = currentSkin->getColor(Colors::FormulaEditor::Background).darker(1.3);

        labelResult->setColour(
            juce::Label::textColourId,
            currentSkin->getColor(Colors::FormulaEditor::Text).interpolatedWith(bgColor, 0.5));

        removeHighlightColors();
        id = 0;
    }
    else
    {
        labelResult->setColour(juce::Label::textColourId,
                               currentSkin->getColor(Colors::FormulaEditor::Text));
        setHighlightColors();
    }

    labelResult->setText(juce::String(std::to_string(id) + '/' + std::to_string(resultTotal)),
                         juce::NotificationType::dontSendNotification);

    repaint();
    ed->repaint();

    if (resultTotal == 0)
    {
        return;
    }

    resultCurrent = (resultCurrent + increment + resultTotal) % resultTotal;

    saveCaretStartPositionLock = true;

    if (moveCaret)
    {
        ed->setHighlightedRegion(juce::Range(
            result[resultCurrent], result[resultCurrent] + textfield[0]->getTotalNumChars()));
    }

    id = resultCurrent + 1;
    labelResult->setText(juce::String(std::to_string(id) + '/' + std::to_string(resultTotal)),
                         juce::NotificationType::dontSendNotification);

    saveCaretStartPositionLock = false;
}

void CodeEditorSearch::search(bool moveCaret)
{
    // move to start pos
    resultHasChanged = true;
    saveCaretStartPositionLock = true;
    if (moveCaret)
        ed->moveCaretTo(startCaretPosition, false);
    saveCaretStartPositionLock = false;

    auto caret = ed->getCaretPos();

    int caretPos = caret.getPosition();

    juce::String txt = ed->getDocument().getAllContent();
    int pos = 0;
    int count = 0;

    // case sensitivity
    int res = !button[0]->isSelected() ? txt.indexOfIgnoreCase(pos, textfield[0]->getText())
                                       : txt.indexOf(pos, textfield[0]->getText());
    resultCurrent = 0;

    bool firstFound = false;
    bool skip = false;

    while (res != -1 && count < 512)
    {
        // whole word search
        if (button[1]->isSelected())
        {
            auto posBefore = (std::max(0, res - 1));
            auto posAfter = std::min(ed->getDocument().getNumCharacters() - 1,
                                     res + textfield[0]->getTotalNumChars());

            auto strBefore = posBefore == 0 ? 0 : txt[posBefore];
            auto strAfter = txt[posAfter];

            if (!((strBefore >= 65 && strBefore <= 90) || !(strBefore >= 97 && strBefore <= 122)) ||
                !((strAfter >= 65 && strAfter <= 90) || !(strAfter >= 97 && strAfter <= 122)))
                skip = true;
        }

        if (caretPos <= res && !firstFound && !skip)
        {
            resultCurrent = count;
            firstFound = true;
        }

        if (!skip)
        {
            result[count] = res;
            count++;
        }

        pos = res + 1;
        res = !button[0]->isSelected() ? txt.indexOfIgnoreCase(pos, textfield[0]->getText())
                                       : txt.indexOf(pos, textfield[0]->getText());

        skip = false;
    }
    latestSearch = textfield[0]->getText();
    resultTotal = count;
    showResult(0, moveCaret);
}

juce::String CodeEditorSearch::getSearchQuery() { return textfield[0]->getText(); }

void CodeEditorSearch::showReplace(bool showReplaceRow)
{
    if (showReplaceRow)
    {
        showRows(2);
        textfield[1]->grabKeyboardFocus();
        textfield[1]->moveCaretToStartOfLine(false);
        textfield[1]->moveCaretToEndOfLine(true);

        textfield[0]->moveCaretToEndOfLine(false);
    }
    else
    {
        showRows(1);
        textfield[0]->grabKeyboardFocus();
        textfield[0]->moveCaretToStartOfLine(false);
        textfield[0]->moveCaretToEndOfLine(true);

        textfield[1]->moveCaretToEndOfLine(false);
    }
}

// ---------------------------------------

SurgeCodeEditorComponent::SurgeCodeEditorComponent(juce::CodeDocument &d, juce::CodeTokeniser *t,
                                                   Surge::GUI::Skin::ptr_t &skin)
    : juce::CodeEditorComponent(d, t)
{
    currentSkin = skin;
    searchMapCache = std::make_unique<juce::Image>(juce::Image::PixelFormat::ARGB, 10, 512, true);
}

void SurgeCodeEditorComponent::focusLost(juce::Component::FocusChangeType e)
{
    if (onFocusLost != nullptr)
    {
        onFocusLost();
    }
}

void SurgeCodeEditorComponent::mouseDoubleClick(const juce::MouseEvent &e)
{

    deselectAll();
    juce::CodeDocument::Position tokenStart(getPositionAt(e.x, e.y));
    juce::CodeDocument::Position tokenEnd(tokenStart);

    if (e.getNumberOfClicks() > 2)
    {
        getDocument().findLineContaining(tokenStart, tokenStart, tokenEnd);
        selectRegion(tokenStart, tokenEnd);
    }
    else
    {
        findWordAt(tokenStart, tokenStart, tokenEnd);
        selectRegion(tokenStart, tokenEnd);
    }
}

void SurgeCodeEditorComponent::findWordAt(juce ::CodeDocument::Position &pos,
                                          juce ::CodeDocument::Position &from,
                                          juce::CodeDocument::Position &to)
{
    auto lineText = getDocument().getLine(pos.getLineNumber());

    auto indexFrom = pos.getIndexInLine();

    auto currentChar = lineText.substring(indexFrom, indexFrom + 1)[0];
    while ((isalpha(currentChar) || isdigit(currentChar)) && indexFrom > 0)
    {
        indexFrom--;
        currentChar = lineText.substring(indexFrom, indexFrom + 1)[0];
    }
    if (indexFrom != 0)
        indexFrom++;
    auto indexTo = pos.getIndexInLine();
    currentChar = lineText.substring(indexTo, indexTo + 1)[0];
    while ((isalpha(currentChar) || isdigit(currentChar)) && indexTo < lineText.length())
    {

        indexTo++;
        currentChar = lineText.substring(indexTo, indexTo + 1)[0];
    }

    auto linePos = juce::CodeDocument::Position(getDocument(), pos.getLineNumber(), 0);

    from.setPosition(linePos.getPosition() + indexFrom);
    to.setPosition(linePos.getPosition() + indexTo);
}

void SurgeCodeEditorComponent::addPopupMenuItems(juce::PopupMenu &menuToAddTo,
                                                 const juce::MouseEvent *mouseClickEvent)
{
    juce::CodeEditorComponent::addPopupMenuItems(menuToAddTo, mouseClickEvent);

#if MAC
    std::string commandStr = u8"\U00002318";

#else
    std::string commandStr = "Ctrl";
#endif

    auto find = juce::PopupMenu::Item("Find...").setAction([this]() {
        search->show();
        search->showReplace(false);
        gotoLine->hide();
    });
    find.shortcutKeyDescription = commandStr + " + F";

    auto replace = juce::PopupMenu::Item("Replace...").setAction([this]() {
        search->show();
        search->showReplace(true);
        gotoLine->hide();
    });
    replace.shortcutKeyDescription = commandStr + " + H";

    auto gotoline =
        juce::PopupMenu::Item(Surge::GUI::toOSCase("Go to Line...")).setAction([this]() {
            search->hide();
            gotoLine->show();
        });
    gotoline.shortcutKeyDescription = commandStr + " + G";

    menuToAddTo.addSeparator();

    if (search == nullptr)
    {
        find.setEnabled(false);
        replace.setEnabled(false);
    }

    if (gotoLine == nullptr)
    {
        gotoline.setEnabled(false);
    }

    menuToAddTo.addItem(find);
    menuToAddTo.addItem(replace);
    menuToAddTo.addItem(gotoline);
}

bool SurgeCodeEditorComponent::keyPressed(const juce::KeyPress &key)
{
    bool code = CodeEditorComponent::keyPressed(key);

    // update search results
    if (search != nullptr)
    {
        if (search->isVisible())
        {
            search->search(false);
        }
    }

    return code;
}

void SurgeCodeEditorComponent::setSearch(CodeEditorSearch &s) { search = &s; }

void SurgeCodeEditorComponent::setGotoLine(GotoLine &s) { gotoLine = &s; }

void SurgeCodeEditorComponent::paint(juce::Graphics &g)
{
    juce::Colour bgColor = findColour(juce::CodeEditorComponent::backgroundColourId).withAlpha(1.f);

    auto bounds = getBounds();
    bounds.translate(0, -2);

    g.setFillType(juce::FillType(bgColor));
    g.fillRect(bounds);

    // draw the current line for gotoLine module
    if (gotoLine != nullptr && gotoLine->isVisible())
    {
        auto currentLine = gotoLine->getCurrentLine();
        auto topLine = getFirstLineOnScreen();
        auto numLines = getNumLinesOnScreen();
        auto lineHeight = getLineHeight();

        if (currentLine >= topLine && currentLine < topLine + numLines)
        {
            auto highlightColor = bgColor.interpolatedWith(
                currentSkin->getColor(Colors::FormulaEditor::Lua::Keyword), 0.3);
            auto y = (currentLine - topLine) * lineHeight;

            g.setFillType(juce::FillType(highlightColor));
            g.fillRect(0, y, getWidth() - getScrollbarThickness(), lineHeight);
        }
    }

    // Draw search matches
    if (search != nullptr && search->isVisible() && search->getResultTotal() > 0)
    {
        auto result = search->getResult();
        int resultTotal = search->getResultTotal();

        int firstLine = getFirstLineOnScreen();
        int lastLine = firstLine + getNumLinesOnScreen();

        auto c = Colors::FormulaEditor::Lua::Keyword;

        auto highlightColor = bgColor.interpolatedWith(currentSkin->getColor(c), 0.5);

        for (int i = 0; i < resultTotal; i++)
        {
            auto pos = juce::CodeDocument::Position(getDocument(), result[i]);
            auto line = pos.getLineNumber();

            if (line >= firstLine && line <= lastLine)
            {
                auto posEnd = juce::CodeDocument::Position(
                    getDocument(), result[i] + search->getSearchQuery().length());

                auto bounds = getCharacterBounds(pos);
                auto boundsEnd = getCharacterBounds(posEnd);

                g.setFillType(juce::FillType(highlightColor));

                int width = boundsEnd.getX() - bounds.getX();
                int height = bounds.getHeight();

                juce::Path path;
                auto rect = juce::Rectangle(bounds.getX(), bounds.getY(), width, height);
                path.addRectangle(rect);
                juce::PathStrokeType strokeType(1.2f);
                g.setColour(highlightColor);
                g.strokePath(path, strokeType);
            }
        }
    }

    juce::CodeEditorComponent::paint(g);
}

void SurgeCodeEditorComponent::paintOverChildren(juce::Graphics &g)
{
    // Draw search map on scrollbar only when result has changed and cache the result
    if (search != nullptr && search->isVisible())
    {
        if (search->resultHasChanged)
        {
            auto result = search->getResult();
            int resultTotal = search->getResultTotal();

            juce::Colour bgColor =
                findColour(juce::CodeEditorComponent::backgroundColourId).withAlpha(1.f);
            auto colorKeyword = Colors::FormulaEditor::Lua::Keyword;
            auto fillColor =
                juce::Colour(colorKeyword.r, colorKeyword.g, colorKeyword.b, 1.f).withAlpha(1.f);

            juce::Colour color =
                juce::Colour((unsigned char)0, (unsigned char)0, (unsigned char)0, 0.f);

            searchMapCache->clear(
                juce::Rectangle(0, 0, searchMapCache->getWidth(), searchMapCache->getHeight()),
                color);

            juce::Graphics mapG = juce::Graphics(*searchMapCache);

            auto numLines = getDocument().getNumLines();

            mapG.setColour(fillColor);

            for (int i = 0; i < resultTotal; i++)
            {
                auto linePos =
                    juce::CodeDocument::Position(getDocument(), result[i]).getLineNumber();

                int y = ((float)linePos / (float)numLines) * searchMapCache->getHeight();

                int height = std::max(
                    2, (int)((1.0 / (float)numLines) * searchMapCache->getHeight() * 0.25));

                mapG.fillRect(0, y, searchMapCache->getWidth(), height);
            }

            search->resultHasChanged = false;
        }

        g.drawImage(*searchMapCache,
                    getWidth() - getScrollbarThickness() + getScrollbarThickness() * 0.2, 0,
                    getScrollbarThickness() * 0.8, getHeight(), 0, 0, searchMapCache->getWidth(),
                    searchMapCache->getHeight(), false);
    }
}

void SurgeCodeEditorComponent::handleEscapeKey()
{
    if (search->isVisible())
    {
        search->hide();
        return;
    }

    if (gotoLine->isVisible())
    {
        gotoLine->hide();
        return;
    }

    juce::Component *c = this;

    while (c)
    {
        if (auto fm = dynamic_cast<FormulaModulatorEditor *>(c))
        {
            fm->escapeKeyPressed();
            return;
        }

        c = c->getParentComponent();
    }
}

void SurgeCodeEditorComponent::caretPositionMoved()
{
    if (search != nullptr)
    {
        search->saveCaretStartPosition(true);
    }
}

void SurgeCodeEditorComponent::mouseWheelMove(const juce::MouseEvent &e,
                                              const juce::MouseWheelDetails &wheel)
{
    juce::MouseWheelDetails w(wheel);
    w.deltaY *= 4;

    enum
    {
        verticalScrollbar,
        horizontalScrollbar
    };

    // makes it possible to mouse wheel scroll and select text at the same time
    if (e.mods.isShiftDown())
    {
        auto scrollbar = dynamic_cast<juce::ScrollBar *>(getChildren()[horizontalScrollbar]);

        if (scrollbar != nullptr)
        {
            auto pos = scrollbar->getCurrentRange().getStart();
            auto width = scrollbar->getCurrentRangeSize();
            auto maxScroll = std::max((double)0, scrollbar->getMaximumRangeLimit() - width);

            scrollToColumn(std::min((double)maxScroll, pos - w.deltaY * 10));
        }
    }
    else
    {
        auto maxScroll = std::max(0, getDocument().getNumLines() - getNumLinesOnScreen());
        auto scrollPos = getFirstLineOnScreen();

        scrollToLine(std::min((double)maxScroll, (double)scrollPos - w.deltaY * 10));
    }
}

// Handles auto indentation

void SurgeCodeEditorComponent::handleReturnKey()
{
    auto pos = this->getCaretPos();
    auto txt = pos.getLineText();
    int tabs = 0;
    int indexInLine = pos.getIndexInLine();
    int actualCharactersBeforeCaret = 0;
    bool indent = false;
    for (int i = 0; i < indexInLine; i++)
    {
        if (txt.substring(i, i + 1) == " ")
        {
            tabs++;
        }
        else if (txt.substring(i, i + 1) == "\t")
        {
            tabs += this->getTabSize();
        }
        else
        {
            auto trimmedTxt = txt.trim();

            if (txt.substring(i, i + 8) == "function")
            {
                indent = true;
            }

            else if (txt.substring(i, i + 14) == "local function")
            {
                indent = true;
            }
            else if (trimmedTxt.substring(trimmedTxt.length() - 10, trimmedTxt.length()) ==
                     "function()")
            {
                indent = true;
            }
            else if (txt.substring(i, i + 2) == "if" &&
                     trimmedTxt.substring(trimmedTxt.length() - 4, trimmedTxt.length()) == "then")
            {
                indent = true;
            }
            else if (trimmedTxt.substring(0, 4) == "else")
            {
                indent = true;
            }
            else if (trimmedTxt.substring(trimmedTxt.length() - 2, trimmedTxt.length()) == "do" ||
                     trimmedTxt.substring(0, 5) == "while")
            {
                indent = true;
            }

            tabs += indent == true ? this->getTabSize() : 0;

            break;
        }

        if (i < indexInLine)
        {
            actualCharactersBeforeCaret = tabs;
        }
    }

    this->insertTextAtCaret("\n");
    this->insertTextAtCaret(std::string(
        std::min(actualCharactersBeforeCaret + (indent == true ? this->getTabSize() : 0), tabs),
        ' '));
}

struct EditorColors
{
    static void setColorsFromSkin(juce::CodeEditorComponent *comp,
                                  const Surge::GUI::Skin::ptr_t &skin)
    {
        juce::CodeEditorComponent::ColourScheme cs = comp->getColourScheme();

        cs.set("Bracket", skin->getColor(Colors::FormulaEditor::Lua::Bracket));
        cs.set("Comment", skin->getColor(Colors::FormulaEditor::Lua::Comment));
        cs.set("Error", skin->getColor(Colors::FormulaEditor::Lua::Error));
        cs.set("Float", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Integer", skin->getColor(Colors::FormulaEditor::Lua::Number));
        cs.set("Identifier", skin->getColor(Colors::FormulaEditor::Lua::Identifier));
        cs.set("Keyword", skin->getColor(Colors::FormulaEditor::Lua::Keyword));
        cs.set("Operator", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("Punctuation", skin->getColor(Colors::FormulaEditor::Lua::Interpunction));
        cs.set("String", skin->getColor(Colors::FormulaEditor::Lua::String));

        comp->setColourScheme(cs);

        comp->setColour(juce::CodeEditorComponent::backgroundColourId,
                        skin->getColor(Colors::FormulaEditor::Background).withMultipliedAlpha(0));
        comp->setColour(juce::CodeEditorComponent::highlightColourId,
                        skin->getColor(Colors::FormulaEditor::Highlight));
        comp->setColour(juce::CodeEditorComponent::defaultTextColourId,
                        skin->getColor(Colors::FormulaEditor::Text));
        comp->setColour(juce::CodeEditorComponent::lineNumberBackgroundId,
                        skin->getColor(Colors::FormulaEditor::LineNumBackground));
        comp->setColour(juce::CodeEditorComponent::lineNumberTextId,
                        skin->getColor(Colors::FormulaEditor::LineNumText));

        comp->retokenise(0, -1);
    }
};

CodeEditorContainerWithApply::~CodeEditorContainerWithApply()
{ // saveState();
}

CodeEditorContainerWithApply::CodeEditorContainerWithApply(SurgeGUIEditor *ed, SurgeStorage *s,
                                                           Surge::GUI::Skin::ptr_t skin,
                                                           bool addComponents)
    : editor(ed), storage(s)
{
    applyButton = std::make_unique<juce::TextButton>("Apply");
    applyButton->setButtonText("Apply");
    applyButton->addListener(this);

    mainDocument = std::make_unique<juce::CodeDocument>();
    mainDocument->addListener(this);
    mainDocument->setNewLineCharacters("\n");
    tokenizer = std::make_unique<LuaTokeniserSurge>();

    mainEditor = std::make_unique<SurgeCodeEditorComponent>(*mainDocument, tokenizer.get(), skin);
    mainEditor->setTabSize(4, true);
    mainEditor->addKeyListener(this);

    EditorColors::setColorsFromSkin(mainEditor.get(), skin);

    // modules
    gotoLine = std::make_unique<GotoLine>(*mainEditor, skin);
    gotoLine->addKeyListener(this);

    search = std::make_unique<CodeEditorSearch>(*mainEditor, skin);
    search->addKeyListener(this);

    mainEditor->setSearch(*search);
    mainEditor->setGotoLine(*gotoLine);

    if (addComponents)
    {
        addAndMakeVisible(applyButton.get());
        addAndMakeVisible(mainEditor.get());
        addChildComponent(search.get());
        addChildComponent(gotoLine.get());
    }

    applyButton->setEnabled(false);
}

void CodeEditorContainerWithApply::buttonClicked(juce::Button *button)
{
    if (button == applyButton.get())
    {
        applyCode();
    }
}

void CodeEditorContainerWithApply::onSkinChanged()
{
    mainEditor->setFont(skin->getFont(Fonts::LuaEditor::Code));
    search->setSkin(skin);
    EditorColors::setColorsFromSkin(mainEditor.get(), skin);
}

void CodeEditorContainerWithApply::codeDocumentTextInserted(const juce::String &newText,
                                                            int insertIndex)
{
    applyButton->setEnabled(true);
    setApplyEnabled(true);
}

void CodeEditorContainerWithApply::codeDocumentTextDeleted(int startIndex, int endIndex)
{
    applyButton->setEnabled(true);
    setApplyEnabled(true);
}

bool CodeEditorContainerWithApply::keyPressed(const juce::KeyPress &key, juce::Component *o)
{
    auto keyCode = key.getKeyCode();

    if (keyCode == juce::KeyPress::tabKey)
    {
        if (key.getModifiers().isShiftDown())
        {
            mainEditor->unindentSelection();
        }
        else
        {
            auto sel = mainEditor->getHighlightedRegion();

            if (sel.getLength() == 0)
            {
                mainEditor->insertTabAtCaret();
            }
            else
            {
                mainEditor->indentSelection();
            }
        }

        return true;
    }
    else if (key.getModifiers().isCommandDown() && keyCode == juce::KeyPress::returnKey)
    {
        applyCode();

        return true;
    }
    else if (key.getModifiers().isCommandDown() && keyCode == 68) // Ctrl/Cmd+D
    {
        auto pos = mainEditor->getCaretPos();
        auto sel = mainEditor->getHighlightedRegion();
        auto txt = mainEditor->getTextInRange(sel);
        bool isMultiline = false;
        bool doSel = true;
        int offset = 0;

        // pos.setPositionMaintained(true);

        if (txt.isEmpty())
        {
            txt = pos.getLineText();
            doSel = false;
        }

        if (txt.containsChar('\n'))
        {
            int count = 0;

            // see if selection is multiline
            for (auto c : txt)
            {
                if (c == '\n' && count < 2)
                {
                    count++;
                }
            }

            // if we have any character after newline, we're still multiline
            if (!txt.endsWithChar('\n'))
            {
                count++;
            }

            isMultiline = count > 1;
            offset = -pos.getIndexInLine();
        }

        mainDocument->insertText(pos.movedBy(isMultiline ? 0 : offset), txt);

        // go back to original position
        mainEditor->moveCaretTo(pos, false);
        // move to latest position after insertion,
        // optionally reselecting the text if a selection existed
        mainEditor->moveCaretTo(pos.movedBy(txt.length()), doSel);

        return true;
    }
    // find
    else if (key.getModifiers().isCommandDown() && keyCode == 70)
    {
        gotoLine->hide();

        search->show();
        search->showReplace(false);
        return true;
    }
    // find & replace
    else if (key.getModifiers().isCommandDown() && keyCode == 72)
    {
        gotoLine->hide();

        search->show();
        search->showReplace(true);

        return true;
    }
    // go to line
    else if (key.getModifiers().isCommandDown() && keyCode == 71)
    {
        bool isgoto = (gotoLine == nullptr);

        search->hide();
        gotoLine->show();

        return true;
    }

    else if (key.getKeyCode() == key.backspaceKey)
    {
        // auto remove closure

        std::string closure[10] = {"(", ")", "[", "]", "{", "}", "\"", "\"", "'", "'"};

        bool found = false;
        for (int i = 0; i < 5; i++)
        {
            found = autoCompleteDeclaration(key, closure[i * 2], closure[i * 2 + 1]);
            if (found)
                break;
        }

        if (found)
            return true;

        // overwrite backspacing

        auto selectionStart = mainEditor->getSelectionStart().getPosition();
        auto selectionEnd = mainEditor->getSelectionEnd().getPosition();
        auto selection = selectionEnd - selectionStart;

        if (selection == 0)
        {
            auto line = mainEditor->getCaretPos().getLineNumber();
            auto lineText = mainEditor->getDocument().getLine(line);
            auto lineTextOriginalLength = lineText.length();
            auto caretPos = mainEditor->getCaretPos().getIndexInLine();
            auto caretPosOriginal = mainEditor->getCaretPos();

            if (caretPos == 0)
                return Component ::keyPressed(key);

            auto previousTab = floor((float)(caretPos - 1) / 4.0) * mainEditor->getTabSize();
            auto tabPos = previousTab;
            auto counter = 0;
            bool aborted = false;

            for (int i = caretPos - 1; i >= tabPos; i--)
            {

                previousTab = i;

                if (lineText.substring(i, i + 1).toStdString().compare(" ") != 0)
                {
                    aborted = true;
                    break;
                }
                counter++;
            }
            // make sure it doesnt overshoot
            if (aborted && counter != 0)
                previousTab++;

            lineText = lineText.substring(0, previousTab) +
                       lineText.substring(caretPos, lineTextOriginalLength);

            auto startIndex =
                juce::CodeDocument::Position(mainEditor->getDocument(), line, 0).getPosition();

            mainEditor->getDocument().replaceSection(startIndex,
                                                     startIndex + lineTextOriginalLength, lineText);

            mainEditor->moveCaretTo(caretPosOriginal.movedBy(previousTab - caretPos), false);
            // sdf
        }
        else
        {
            return Component ::keyPressed(key);
        }

        // if (!found)
        //   return Component ::keyPressed(key);

        return true;
    }
    // auto complete closure
    else if (key.getTextCharacter() == 40 || key.getTextCharacter() == 41)
    {
        return autoCompleteDeclaration(key, "(", ")");
    }
    else if (key.getTextCharacter() == 91 || key.getTextCharacter() == 93)
    {
        return autoCompleteDeclaration(key, "[", "]");
    }
    else if (key.getTextCharacter() == 123 || key.getTextCharacter() == 125)
    {
        return autoCompleteDeclaration(key, "{", "}");
    }
    else if (key.getTextCharacter() == 34)
    {
        return autoCompleteDeclaration(key, "\"", "\"");
    }
    else if (key.getTextCharacter() == 39)
    {
        return autoCompleteDeclaration(key, "'", "'");
    }
    else
    {
        return Component::keyPressed(key);
    }

    return false;
}

void CodeEditorContainerWithApply::removeTrailingWhitespaceFromDocument()
{
    const auto caretPos = mainEditor->getCaretPos();
    const auto caretLine = caretPos.getLineNumber();
    const auto numLines = mainEditor->getDocument().getNumLines();
    uint32_t charsRemoved = 0;

    for (int i = 0; i < numLines; i++)
    {
        juce::CodeDocument::Position lineStart{mainEditor->getDocument(), i, 0};
        auto s = lineStart.getLineText();

        int eol = 1;
        if (s.contains("\r\n"))
        {
            eol = 2;
        }
        const auto sizeOld = s.length() - eol; // disregard EOL
        juce::CodeDocument::Position lineEnd{mainEditor->getDocument(), i, sizeOld};

        s = s.trimEnd();

        const auto sizeNew = s.length();

        if (i <= caretLine && sizeOld > sizeNew)
        {
            charsRemoved += sizeOld - sizeNew;

            if (sizeOld - sizeNew > 0)
            {
                mainEditor->getDocument().replaceSection(lineStart.getPosition(),
                                                         lineEnd.getPosition(), s);
            }
        }
    }

    mainEditor->moveCaretTo(caretPos.movedBy(-charsRemoved), false);
}

bool CodeEditorContainerWithApply::autoCompleteDeclaration(juce::KeyPress key, std::string start,
                                                           std::string end)
{
    auto keyChar = juce::String::charToString((key.getTextCharacter())).toStdString();

    if (keyChar != start && keyChar != end && key.getKeyCode() != 8)
        return false;

    // get text line
    auto text = mainDocument->getAllContent();
    auto textLine = mainDocument->getLine(mainEditor->getCaretPos().getLineNumber());

    auto selectionStart = mainEditor->getSelectionStart().getPosition();
    auto selectionEnd = mainEditor->getSelectionEnd().getPosition();
    // get character before and after caret

    auto caretPosition = mainEditor->getCaretPosition();
    // text selection
    auto selectedText = text.substring(selectionStart, selectionEnd);

    // selection
    auto selection =
        mainEditor->getSelectionEnd().getPosition() - mainEditor->getSelectionStart().getPosition();

    // abort for certain cases

    auto nextChar = text.substring(caretPosition, caretPosition + 1);
    bool isBlank = nextChar.toStdString() == " ";
    bool isNewLine = nextChar == "\n";
    bool isEndChar = nextChar.toStdString() == end;

    if (key.getKeyCode() != 8 && selection == 0)
    {
        if (!(isBlank || isNewLine || isEndChar))
        {
            return false;
        }
    }

    // auto delete closure
    if (key.getKeyCode() == 8)
    {
        if (selection == 0)
        {
            auto charBefore = text.substring(caretPosition - 1, caretPosition).toStdString();
            auto charAfter = text.substring(caretPosition, caretPosition + 1).toStdString();

            if (charBefore == start && charAfter == end)
            {
                mainEditor->getDocument().replaceSection(caretPosition - 1, caretPosition + 1, "");
                return true;
            }
            else
            {
                return false;
            }
        }

        return false;
    }

    // count characters on line
    int count = 0;

    for (int i = 0; i < textLine.length(); i++)
    {
        auto character = textLine.substring(i, i + 1).toStdString();

        if (start != end)
        {
            if (character == start)
                count++;
            if (character == end)
                count--;
        }
        else
        {
            if (character == start)
                count++;
            count = count % 2;
        }
    }

    auto charBefore = text.substring(caretPosition - 1, caretPosition).toStdString();
    auto charAfter = text.substring(caretPosition, caretPosition + 1).toStdString();

    // just move caret forward
    if (charAfter == end && keyChar == end && selection == 0)
    {
        mainEditor->moveCaretRight(false, false);
        return true;
    }
    // auto close
    else if ((charAfter != end && keyChar == start && count == 0) ||
             (charAfter == end && keyChar == start && count == 0))
    {
        mainEditor->insertTextAtCaret(start + selectedText.toStdString() + end);
        auto pos = juce::CodeDocument::Position(*mainDocument, selectionStart);
        pos.moveBy(1);
        mainEditor->moveCaretTo(pos, false);
        pos.moveBy(selection);
        mainEditor->moveCaretTo(pos, true);
        return true;
    }
    // do default
    else
    {
        return Component::keyPressed(key);
    }

    return true;
}

void CodeEditorContainerWithApply::paint(juce::Graphics &g) { g.fillAll(juce::Colours::black); }

void CodeEditorContainerWithApply::initState(DAWExtraStateStorage::EditorState::CodeEditorState &s)
{
    state = &s;
}

void CodeEditorContainerWithApply::saveState()
{

    auto documentScroll = mainEditor->getFirstLineOnScreen();
    auto caretPos = mainEditor->getCaretPosition();
    auto selectStart = mainEditor->getSelectionStart().getPosition();
    auto selectEnd = mainEditor->getSelectionEnd().getPosition();

    state->selectStart = selectStart;
    state->selectEnd = selectEnd;
    state->scroll = documentScroll;
    state->caretPosition = caretPos;

    state->popupOpen = search->isVisible() || gotoLine->isVisible() ? true : false;

    if (search->isVisible())
    {
        if (search->rowsVisible == 1)
        {
            state->popupType = 0;
            state->popupText1 = search->getText(0).toStdString();
        }
        else
        {
            state->popupType = 1;
            state->popupText1 = search->getText(0).toStdString();
            state->popupText2 = search->getText(1).toStdString();
        }

        state->popupCaseSensitive = search->getButtonSelected(0);
        state->popupWholeWord = search->getButtonSelected(1);

        state->popupCurrentResult = search->getCurrentResult();
    }

    if (gotoLine->isVisible())
    {
        state->popupType = 2;
        state->popupText1 = gotoLine->getText(0).toStdString();
    }
}

void CodeEditorContainerWithApply::loadState()
{

    // auto &state = getEditState();

    // restore code editor

    auto pos = juce::CodeDocument::Position(mainEditor->getDocument(), state->caretPosition);
    bool selected = state->selectStart - state->selectEnd == 0 ? false : true;

    mainEditor->scrollToLine(state->scroll);

    if (selected)
    {

        auto selectStart =
            juce::CodeDocument::Position(mainEditor->getDocument(), state->selectStart);

        auto selectEnd = juce::CodeDocument::Position(mainEditor->getDocument(), state->selectEnd);

        if (pos.getPosition() > selectStart.getPosition())
        {

            mainEditor->moveCaretTo(selectStart, false);
            mainEditor->moveCaretTo(selectEnd, true);
        }
        else
        {
            mainEditor->moveCaretTo(selectEnd, false);
            mainEditor->moveCaretTo(selectStart, true);
        }
    }
    else
    {
        mainEditor->moveCaretTo(pos, false);
    }

    // restore popup

    if (state->popupOpen)
    {
        switch (state->popupType)
        {
        case 0: // Find
            search->show();
            search->showReplace(false);
            search->setText(0, state->popupText1);
            search->showReplace(false);
            search->setCurrentResult(state->popupCurrentResult);
            search->setButtonSelected(0, state->popupCaseSensitive);
            search->setButtonSelected(1, state->popupWholeWord);
            break;
        case 1: // Find&Replace
            search->show();
            search->showReplace(true);
            search->setText(0, state->popupText1);
            search->setText(1, state->popupText2);
            search->setCurrentResult(state->popupCurrentResult);
            search->setButtonSelected(0, state->popupCaseSensitive);
            search->setButtonSelected(1, state->popupWholeWord);
            break;
        case 2: // Goto line
            gotoLine->show();
            gotoLine->setText(0, state->popupText1);
            break;
        }
    }
}

struct ExpandingFormulaDebugger : public juce::Component,
                                  public Surge::GUI::SkinConsumingComponent,
                                  juce::TextEditor::Listener
{
    bool isOpen{false};

    std::unique_ptr<Textfield> searchfield;

    ExpandingFormulaDebugger(FormulaModulatorEditor *ed) : editor(ed)
    {

        debugTableDataModel = std::make_unique<DebugDataModel>();

        debugTableDataModel->setEditor(editor);

        debugTableDataModel.get()->onClick = [this, ed]() { refreshDebuggerView(); };

        debugTable = std::make_unique<juce::TableListBox>("Debug", debugTableDataModel.get());
        debugTable->getHeader().addColumn("key", 1, 50);
        debugTable->getHeader().addColumn("value", 2, 50);
        debugTable->setHeaderHeight(0);
        debugTable->getHeader().setVisible(false);
        debugTable->setRowHeight(14);
        addAndMakeVisible(*debugTable);

        searchfield = std::make_unique<Textfield>(0);

        searchfield->setHeader("Filter");

        searchfield->addListener(this);
        addAndMakeVisible(*searchfield);

        searchfield->setText(editor->getEditState().debuggerFilterText);

        searchfield->onTextChange = [this]() {
            editor->getEditState().debuggerFilterText = searchfield->getText().toStdString();
        };

        // searchfield = std::make_unique<Textfield>(0);

        // searchfield->setBorder(juce::BorderSize(-1, 4, 0, 4));
    }

    FormulaModulatorEditor *editor{nullptr};

    pdata tp[n_scene_params];

    void textEditorTextChanged(juce::TextEditor &) override
    {
        // std::cout << "text editor changed" << searchfield->getText() << "\n";
        updateDebuggerWithOptionalStep(false);
    }

    void initializeLfoDebugger()
    {
        auto lfodata = editor->lfos;

        tp[lfodata->delay.param_id_in_scene].i = lfodata->delay.val.i;
        tp[lfodata->attack.param_id_in_scene].i = lfodata->attack.val.i;
        tp[lfodata->hold.param_id_in_scene].i = lfodata->hold.val.i;
        tp[lfodata->decay.param_id_in_scene].i = lfodata->decay.val.i;
        tp[lfodata->sustain.param_id_in_scene].i = lfodata->sustain.val.i;
        tp[lfodata->release.param_id_in_scene].i = lfodata->release.val.i;

        tp[lfodata->magnitude.param_id_in_scene].i = lfodata->magnitude.val.i;
        tp[lfodata->rate.param_id_in_scene].i = lfodata->rate.val.i;
        tp[lfodata->shape.param_id_in_scene].i = lfodata->shape.val.i;
        tp[lfodata->start_phase.param_id_in_scene].i = lfodata->start_phase.val.i;
        tp[lfodata->deform.param_id_in_scene].i = lfodata->deform.val.i;
        tp[lfodata->trigmode.param_id_in_scene].i = lm_keytrigger;

        lfoDebugger = std::make_unique<LFOModulationSource>();
        lfoDebugger->assign(editor->storage, editor->lfos, tp, 0, nullptr, nullptr,
                            editor->formulastorage, true);

        if (editor->lfo_id < n_lfos_voice)
        {
            lfoDebugger->setIsVoice(true);
        }
        else
        {
            lfoDebugger->setIsVoice(false);
        }

        if (lfoDebugger->isVoice)
        {
            lfoDebugger->formulastate.velocity = 100;
        }

        lfoDebugger->attack();

        stepLfoDebugger();

        if (editor->editor)
            editor->editor->enqueueAccessibleAnnouncement("Reset Debugger");
    }

    void refreshDebuggerView() { updateDebuggerWithOptionalStep(false); }

    void stepLfoDebugger() { updateDebuggerWithOptionalStep(true); }

    void updateDebuggerWithOptionalStep(bool doStep)
    {
        if (doStep)
        {
            Surge::Formula::setupEvaluatorStateFrom(lfoDebugger->formulastate,
                                                    editor->storage->getPatch(), editor->scene);

            lfoDebugger->process_block();
        }
        else
        {

            auto &formulastate = lfoDebugger->formulastate;
            auto &localcopy = tp;
            auto lfodata = editor->lfos;
            auto storage = editor->storage;

            formulastate.rate = localcopy[lfodata->rate.param_id_in_scene].f;
            formulastate.amp = localcopy[lfodata->magnitude.param_id_in_scene].f;
            formulastate.phase = localcopy[lfodata->start_phase.param_id_in_scene].f;
            formulastate.deform = localcopy[lfodata->deform.param_id_in_scene].f;
            formulastate.tempo = storage->temposyncratio * 120.0;
            formulastate.songpos = storage->songpos;

            Surge::Formula::setupEvaluatorStateFrom(lfoDebugger->formulastate,
                                                    editor->storage->getPatch(), editor->scene);
            float out[Surge::Formula::max_formula_outputs];
            Surge::Formula::valueAt(lfoDebugger->getIntPhase(), lfoDebugger->getPhase(), storage,
                                    lfoDebugger->fs, &formulastate, out, false);
        }

        auto f = searchfield->getText();
        auto st = Surge::Formula::createDebugDataOfModState(
            lfoDebugger->formulastate, searchfield->getText().toStdString(),
            editor->getEditState().debuggerGroupState);

        if (debugTableDataModel && debugTable)
        {
            debugTableDataModel->setRows(st);
            debugTable->updateContent();
            debugTable->repaint();
        }

        if (editor->editor)
            editor->editor->enqueueAccessibleAnnouncement("Stepped Debugger");
    }

    std::unique_ptr<juce::TableListBox> debugTable;

    struct DebugDataModel : public juce::TableListBoxModel,
                            public Surge::GUI::SkinConsumingComponent
    {

        std::function<void()> onClick;
        FormulaModulatorEditor *editor;

        void setEditor(FormulaModulatorEditor *ed) { editor = ed; }

        std::vector<Surge::Formula::DebugRow> rows;
        void setRows(const std::vector<Surge::Formula::DebugRow> &r) { rows = r; }
        int getNumRows() override { return rows.size(); }

        void cellClicked(int rowNumber, int columnId, const juce::MouseEvent &) override
        {

            const auto &r = rows[rowNumber];

            if (r.isHeader == true)
            {
                editor->getEditState().debuggerGroupState[r.group] =
                    editor->getEditState().debuggerGroupState[r.group] == false;
            }
            onClick();
        }

        void paintRowBackground(juce::Graphics &g, int rowNumber, int width, int height,
                                bool rowIsSelected) override
        {
            const auto &r = rows[rowNumber];

            auto color = rowNumber % 2 == 0 ? Colors ::FormulaEditor::Debugger::LightRow
                                            : Colors::FormulaEditor::Debugger::Row;

            auto interpolateStrength = r.isHeader ? 0.15 : r.isUserDefined ? 0.0 : 0;

            g.fillAll(skin->getColor(color).interpolatedWith(
                skin->getColor(Colors ::FormulaEditor::Lua::Identifier), interpolateStrength));
        }

        std::string getText(int rowNumber, int columnId)
        {
            const auto &r = rows[rowNumber];

            if (columnId == 1)
            {
                return r.label;
            }
            else if (columnId == 2)
            {
                if (!r.hasValue)
                {
                    return "";
                }
                else if (auto fv = std::get_if<float>(&r.value))
                {
                    return fmt::format("{:.3f}", *fv);
                }
                else if (auto sv = std::get_if<std::string>(&r.value))
                {
                    return *sv;
                }
            }
            return "";
        }

        void paintCell(juce::Graphics &g, int rowNumber, int columnId, int w, int h,
                       bool rowIsSelected) override
        {
            if (rowNumber < 0 || rowNumber >= rows.size())
                return;

            auto b = juce::Rectangle<int>(0, 0, w, h);

            const auto &r = rows[rowNumber];

            g.setFont(skin->fontManager->getFiraMonoAtSize(8.5));
            b = b.withTrimmedLeft(4);

            float alpha = 1;

            if (r.filterFlag == Surge::Formula::DebugRow::Ignore)
                alpha = 0.5;

            if (r.isInternal)
                g.setColour(skin->getColor(Colors::FormulaEditor::Debugger::InternalText));
            else
                g.setColour(skin->getColor(Colors::FormulaEditor::Debugger::Text).withAlpha(alpha));

            if (r.isHeader && columnId == 1)
            {

                g.setFont(skin->fontManager->getFiraMonoAtSize(8.5, juce::Font::bold));
                g.drawText(getText(rowNumber, columnId), b, juce::Justification::centredLeft);

                // draw arrow

                auto path = juce::Path();
                auto size = 4;
                auto arrowMarginX = 4.5;
                auto arrowMarginY = h * 0.5;

                path.startNewSubPath(0, -size * 0.4);
                path.lineTo(-size * 0.5, size * 0.4);
                path.lineTo(size * 0.5, size * 0.4);

                float arrowRotation = 0;

                arrowRotation = editor->getEditState().debuggerGroupState[r.group] ? M_PI : 0;

                path.applyTransform(juce::AffineTransform()
                                        .rotated(arrowRotation)
                                        .translated(arrowMarginX, arrowMarginY));

                g.setFillType(
                    juce::FillType(skin->getColor(Colors::FormulaEditor::Debugger::Text)));

                g.fillPath(path);
            }
            else if (columnId == 1)
            {
                b = b.withTrimmedLeft(r.depth * 10);
                g.drawText(getText(rowNumber, columnId), b, juce::Justification::centredLeft);
            }
            else if (columnId == 2)
            {
                b = b.withTrimmedRight(2);
                g.drawText(getText(rowNumber, columnId), b, juce::Justification::centredRight);
            }
            else
            {
                g.setColour(juce::Colours::red);
                g.fillRect(b);
            }
        }

        struct DebugCell : juce::Label
        {

            int row{0}, col{0};
            DebugDataModel *model{nullptr};
            DebugCell(DebugDataModel *m) : model(m) {}
            void paint(juce::Graphics &g) override
            {
                model->paintCell(g, row, col, getWidth(), getHeight(), false);
            }

            void updateAccessibility()
            {
                setAccessible(true);
                setText(model->getText(row, col), juce::dontSendNotification);
            }

            JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DebugCell);
        };
        friend class DebugCell;

        Component *refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected,
                                           Component *existingComponentToUpdate) override
        {
            DebugCell *cell{nullptr};
            if (existingComponentToUpdate)
            {
                cell = dynamic_cast<DebugCell *>(existingComponentToUpdate);
                if (!cell)
                    delete existingComponentToUpdate;
            }
            if (!cell)
            {
                cell = new DebugCell(this);
            }
            cell->row = rowNumber;
            cell->col = columnId;
            cell->updateAccessibility();
            cell->setInterceptsMouseClicks(false, true);
            return cell;
        }
    };

    std::unique_ptr<DebugDataModel> debugTableDataModel;

    std::unique_ptr<juce::Label> dPhaseLabel;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override
    {

        searchfield->applyFontToAllText(skin->fontManager->getLatoAtSize(9.5, juce::Font::plain));
        searchfield->setColour(juce::TextEditor::ColourIds::textColourId,
                               skin->getColor(Colors::Dialog::Button::Text));
        searchfield->setColour(juce::TextEditor::backgroundColourId,
                               skin->getColor(Colors::FormulaEditor::Background).darker(0.4));
        searchfield->setColour(juce::TextEditor::focusedOutlineColourId,
                               skin->getColor(Colors::FormulaEditor::Background).brighter(0.08));
        searchfield->setColour(juce::TextEditor::outlineColourId,
                               skin->getColor(Colors::FormulaEditor::Background).brighter(0));

        searchfield->setText(searchfield->getText());
        searchfield->setHeaderColor(skin->getColor(Colors::Dialog::Button::Text));

        debugTableDataModel->setSkin(skin, associatedBitmapStore);

        searchfield->applyColourToAllText(skin->getColor(Colors::Dialog::Button::Text), true);
    }

    void setOpen(bool b)
    {
        isOpen = b;
        editor->getEditState().debuggerOpen = b;
        setVisible(b);
        editor->resized();
    }

    void resized() override
    {
        if (isOpen)
        {
            int margin = 0;

            // debugTable->setBounds(getLocalBounds().reduced(margin));

            debugTable->setBounds(getLocalBounds().translated(0, 9).reduced(0, 9));
            auto w = getLocalBounds().reduced(margin).getWidth() - 10;
            debugTable->getHeader().setColumnWidth(1, w / 2);
            debugTable->getHeader().setColumnWidth(2, w / 2);

            searchfield->setBounds(getLocalBounds().withHeight(18));
        }
    }

    std::unique_ptr<LFOModulationSource> lfoDebugger;

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExpandingFormulaDebugger);
};

struct FormulaControlArea : public juce::Component,
                            public Surge::GUI::SkinConsumingComponent,
                            public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x575200,
        tag_code_apply,
        tag_debugger_show,
        tag_debugger_init,
        tag_debugger_step
    };

    FormulaModulatorEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};

    FormulaControlArea(FormulaModulatorEditor *ol, SurgeGUIEditor *ed) : overlay(ol), editor(ed)
    {
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    }

    void resized() override
    {
        if (skin)
        {
            rebuild();
        }
    }

    void rebuild()
    {
        int labelHeight = 12;
        int buttonHeight = 14;
        int margin = 2;
        int xpos = 10;
        int ypos = 1 + labelHeight + margin;
        int marginPos = xpos + margin;
        removeAllChildren();

        {
            int btnWidth = 100;

            codeL = newL("Code");
            codeL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*codeL);

            codeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);
            codeS->setBounds(btnrect);
            codeS->setStorage(overlay->storage);
            codeS->setTitle("Code Selection");
            codeS->setDescription("Code Selection");
            codeS->setLabels({"Editor", "Prelude"});
            codeS->addListener(this);
            codeS->setTag(tag_select_tab);
            codeS->setHeightOfOneImage(buttonHeight);
            codeS->setRows(1);
            codeS->setColumns(2);
            codeS->setDraggable(true);
            codeS->setValue(overlay->getEditState().codeOrPrelude);
            codeS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*codeS);

            btnWidth = 60;

            applyS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(getWidth() / 2 - 30, ypos - 1, btnWidth, buttonHeight);
            applyS->setBounds(btnrect);
            applyS->setStorage(overlay->storage);
            applyS->setTitle("Apply");
            applyS->setDescription("Apply");
            applyS->setLabels({"Apply"});
            applyS->addListener(this);
            applyS->setTag(tag_code_apply);
            applyS->setHeightOfOneImage(buttonHeight);
            applyS->setRows(1);
            applyS->setColumns(1);
            applyS->setDraggable(true);
            applyS->setSkin(skin, associatedBitmapStore);
            applyS->setEnabled(false);
            addAndMakeVisible(*applyS);
        }

        // Debugger Controls from the left
        {
            debugL = newL("Debugger");
            debugL->setBounds(getWidth() - 10 - 100, 1, 100, labelHeight);
            debugL->setJustificationType(juce::Justification::centredRight);
            addAndMakeVisible(*debugL);

            int btnWidth = 60;
            int bpos = getWidth() - 10 - btnWidth;
            int ypos = 1 + labelHeight + margin;

            auto ma = [&](const std::string &l, tags t) {
                auto res = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
                auto btnrect = juce::Rectangle<int>(bpos, ypos - 1, btnWidth, buttonHeight);

                res->setBounds(btnrect);
                res->setStorage(overlay->storage);
                res->setLabels({l});
                res->addListener(this);
                res->setTag(t);
                res->setHeightOfOneImage(buttonHeight);
                res->setRows(1);
                res->setColumns(1);
                res->setDraggable(false);
                res->setSkin(skin, associatedBitmapStore);
                res->setValue(0);
                return res;
            };

            auto isOpen = overlay->debugPanel->isOpen;
            showS = ma(isOpen ? "Hide" : "Show", tag_debugger_show);
            addAndMakeVisible(*showS);
            bpos -= btnWidth + margin;

            stepS = ma("Step", tag_debugger_step);
            stepS->setVisible(isOpen);
            addChildComponent(*stepS);
            bpos -= btnWidth + margin;

            initS = ma("Init", tag_debugger_init);
            initS->setVisible(isOpen);
            addChildComponent(*initS);
            bpos -= btnWidth + margin;
        }
    }

    std::unique_ptr<juce::Label> newL(const std::string &s)
    {
        auto res = std::make_unique<juce::Label>(s, s);
        res->setText(s, juce::dontSendNotification);
        res->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
        res->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        return res;
    }

    int32_t controlModifierClicked(GUI::IComponentTagValue *pControl,
                                   const juce::ModifierKeys &mods, bool isDoubleClickEvent) override
    {
        int tag = pControl->getTag();

        switch (tag)
        {
        case tag_select_tab:
        case tag_code_apply:
        case tag_debugger_show:
        case tag_debugger_init:
        case tag_debugger_step:
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = editor->helpURLForSpecial("formula-editor");
            auto hurl = editor->fullyResolvedHelpURL(msurl);

            editor->addHelpHeaderTo("Formula Editor", hurl, contextMenu);

            contextMenu.showMenuAsync(editor->popupMenuOptions(this, false),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        break;
        default:
            break;
        }
        return 1;
    }

    void valueChanged(GUI::IComponentTagValue *c) override
    {
        auto tag = (tags)(c->getTag());

        switch (tag)
        {
        case tag_select_tab:
        {
            if (c->getValue() > 0.5)
            {
                overlay->showPreludeCode();
            }
            else
            {
                overlay->showModulatorCode();
            }
        }
        break;
        case tag_code_apply:
            overlay->applyCode();
            break;
        case tag_debugger_show:
        {
            if (overlay->debugPanel->isOpen)
            {
                overlay->debugPanel->setOpen(false);
                showS->setLabels({"Show"});
                stepS->setVisible(false);
                initS->setVisible(false);
            }
            else
            {
                overlay->debugPanel->setOpen(true);
                showS->setLabels({"Hide"});
                stepS->setVisible(true);
                initS->setVisible(true);
            }
            repaint();
        }
        case tag_debugger_init:
            overlay->debugPanel->initializeLfoDebugger();
            break;
        case tag_debugger_step:
        {
            if (!overlay->debugPanel->lfoDebugger)
            {
                overlay->debugPanel->initializeLfoDebugger();
            }
            overlay->debugPanel->stepLfoDebugger();
        }
        break;
        default:
            break;
        }
    }

    std::unique_ptr<juce::Label> codeL, debugL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> codeS, applyS, showS, initS, stepS;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormulaControlArea);
};

FormulaModulatorEditor::FormulaModulatorEditor(SurgeGUIEditor *ed, SurgeStorage *s, LFOStorage *ls,
                                               FormulaModulatorStorage *fs, int lid, int scene,
                                               Surge::GUI::Skin::ptr_t skin)
    : CodeEditorContainerWithApply(ed, s, skin, false), lfos(ls), scene(scene), formulastorage(fs),
      lfo_id(lid), editor(ed)
{
    mainEditor->setScrollbarThickness(8);
    mainEditor->setTitle("Formula Modulator Code");
    mainEditor->setDescription("Formula Modulator Code");
    mainEditor->onFocusLost = [this]() { this->saveState(); };

    mainDocument->insertText(0, fs->formulaString);
    mainDocument->clearUndoHistory();
    mainDocument->setSavePoint();

    preludeDocument = std::make_unique<juce::CodeDocument>();
    preludeDocument->insertText(0, Surge::LuaSupport::getFormulaPrelude());

    preludeDisplay =
        std::make_unique<SurgeCodeEditorComponent>(*preludeDocument, tokenizer.get(), skin);
    preludeDisplay->setTabSize(4, true);
    preludeDisplay->setReadOnly(true);
    preludeDisplay->setScrollbarThickness(8);
    preludeDisplay->setTitle("Formula Modulator Prelude Code");
    preludeDisplay->setDescription("Formula Modulator Prelude Code");

    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);

    controlArea = std::make_unique<FormulaControlArea>(this, editor);
    addAndMakeVisible(*controlArea);
    addAndMakeVisible(*mainEditor);

    addChildComponent(*preludeDisplay);

    addChildComponent(*search);
    addChildComponent(*gotoLine);

    search->onFocusLost = [this]() { this->saveState(); };
    gotoLine->onFocusLost = [this]() { this->saveState(); };

    debugPanel = std::make_unique<ExpandingFormulaDebugger>(this);
    debugPanel->setVisible(false);
    addChildComponent(*debugPanel);

    switch (getEditState().codeOrPrelude)
    {
    case 0:
        showModulatorCode();
        break;
    case 1:
        showPreludeCode();
        break;
    }

    if (getEditState().debuggerOpen)
    {
        debugPanel->setOpen(true);
        debugPanel->initializeLfoDebugger();
        repaint();
    }

    initState(getEditState().codeEditor);
    loadState();
}

FormulaModulatorEditor::~FormulaModulatorEditor() {}

DAWExtraStateStorage::EditorState::FormulaEditState &FormulaModulatorEditor::getEditState()
{
    return storage->getPatch().dawExtraState.editor.formulaEditState[scene][lfo_id];
}

void FormulaModulatorEditor::onSkinChanged()
{
    CodeEditorContainerWithApply::onSkinChanged();
    preludeDisplay->setFont(skin->getFont(Fonts::LuaEditor::Code));
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);
    controlArea->setSkin(skin, associatedBitmapStore);
    debugPanel->setSkin(skin, associatedBitmapStore);
}

void FormulaModulatorEditor::applyCode()
{
    removeTrailingWhitespaceFromDocument();

    editor->undoManager()->pushFormula(scene, lfo_id, *formulastorage);
    formulastorage->setFormula(mainDocument->getAllContent().toStdString());
    storage->getPatch().isDirty = true;
    editor->forceLfoDisplayRepaint();
    updateDebuggerIfNeeded();
    editor->repaintFrame();
    // juce::SystemClipboard::copyTextToClipboard(formulastorage->formulaString);
    setApplyEnabled(false);
    mainEditor->grabKeyboardFocus();

    if (debugPanel->isOpen)
        debugPanel->initializeLfoDebugger();
}

void FormulaModulatorEditor::forceRefresh()
{
    mainDocument->replaceAllContent(formulastorage->formulaString);
    editor->repaintFrame();
}

void FormulaModulatorEditor::setApplyEnabled(bool b)
{
    if (controlArea)
    {
        controlArea->applyS->setEnabled(b);
        controlArea->applyS->repaint();
    }
}

void FormulaModulatorEditor::resized()
{
    auto t = getTransform().inverted();
    auto width = getWidth();
    auto height = getHeight();
    t.transformPoint(width, height);

    int controlHeight = 35;
    int debugPanelWidth = 0;
    int debugPanelMargin = 0;

    if (debugPanel->isVisible())
    {
        debugPanelWidth = 215;
        debugPanelMargin = 2;
    }
    auto edRect = juce::Rectangle<int>(2, 2, width - 4 - debugPanelMargin - debugPanelWidth,
                                       height - controlHeight - 4);
    mainEditor->setBounds(edRect);
    preludeDisplay->setBounds(edRect);
    if (debugPanel->isVisible())
    {
        debugPanel->setBounds(width - 4 - debugPanelWidth + debugPanelMargin, 2, debugPanelWidth,
                              height - 4 - controlHeight);
    }
    controlArea->setBounds(0, height - controlHeight, width, controlHeight);

    search->resize();
    gotoLine->resize();
}

void FormulaModulatorEditor::showModulatorCode()
{
    preludeDisplay->setVisible(false);
    mainEditor->setVisible(true);
    getEditState().codeOrPrelude = 0;
}

void FormulaModulatorEditor::showPreludeCode()
{
    preludeDisplay->setVisible(true);
    mainEditor->setVisible(false);
    getEditState().codeOrPrelude = 1;
}

void FormulaModulatorEditor::escapeKeyPressed()
{
    auto c = getParentComponent();
    while (c)
    {
        if (auto olw = dynamic_cast<OverlayWrapper *>(c))
        {
            olw->onClose();
            return;
        }
        c = c->getParentComponent();
    }
}

void FormulaModulatorEditor::updateDebuggerIfNeeded()
{
    {
        if (debugPanel->isOpen)
        {
            bool anyUpdate{false};
            auto lfodata = lfos;

#define CK(x)                                                                                      \
    {                                                                                              \
        auto &r = debugPanel->tp[lfodata->x.param_id_in_scene];                                    \
                                                                                                   \
        if (r.i != lfodata->x.val.i)                                                               \
        {                                                                                          \
            r.i = lfodata->x.val.i;                                                                \
            anyUpdate = true;                                                                      \
        }                                                                                          \
    }

            CK(rate);
            CK(magnitude);
            CK(start_phase);
            CK(deform);

            if (debugPanel->lfoDebugger->formulastate.tempo != storage->temposyncratio * 120)
            {
                anyUpdate = true;
            }

#undef CK

#define CKENV(x, y)                                                                                \
    {                                                                                              \
        auto &tgt = debugPanel->lfoDebugger->formulastate.x;                                       \
        auto src = lfodata->y.value_to_normalized(lfodata->y.val.f);                               \
                                                                                                   \
        if (tgt != src)                                                                            \
        {                                                                                          \
            tgt = src;                                                                             \
            anyUpdate = true;                                                                      \
        }                                                                                          \
    }
            CKENV(del, delay);
            CKENV(a, attack);
            CKENV(h, hold);
            CKENV(dec, decay);
            CKENV(s, sustain);
            CKENV(r, release);

#undef CKENV

            if (anyUpdate)
            {
                debugPanel->refreshDebuggerView();
                editor->repaintFrame();
            }
        }
    }
    updateDebuggerCounter = (updateDebuggerCounter + 1) & 31;
}

std::optional<std::pair<std::string, std::string>>
FormulaModulatorEditor::getPreCloseChickenBoxMessage()
{
    if (controlArea->applyS->isEnabled())
    {
        return std::make_pair("Close Formula Editor",
                              "Do you really want to close the formula editor? Any "
                              "changes that were not applied will be lost!");
    }
    return std::nullopt;
}

struct WavetablePreviewComponent : public juce::Component, public Surge::GUI::SkinConsumingComponent
{
    WavetableScriptEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};

    WavetablePreviewComponent(WavetableScriptEditor *ol, SurgeGUIEditor *ed,
                              Surge::GUI::Skin::ptr_t skin)
        : overlay(ol), editor(ed)
    {
        setSkin(skin);
    }

    void setSingleFrame() { mode = 0; }
    void setFilmstrip() { mode = 1; }

    void paint(juce::Graphics &g) override
    {
        auto height = getHeight();
        auto width = getWidth();
        auto middle = height * 0.5;

        juce::Rectangle<float> drawArea(axisSpaceX, 0, width - axisSpaceX, height);
        juce::Rectangle<float> vaxisArea(0, 0, axisSpaceX, height);

        auto font = skin->fontManager->getLatoAtSize(8);

        g.setColour(skin->getColor(Colors::MSEGEditor::Background));
        g.fillRect(drawArea);

        // Vertical axis
        if (axisSpaceX > 0)
        {
            std::vector<std::string> txt = {"1.0", "0.0", "-1.0"};
            g.setFont(font);
            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::SecondaryText));
            g.drawText(txt[0], vaxisArea.getX() - 3, 4, vaxisArea.getWidth(), 12,
                       juce::Justification::topRight);
            g.drawText(txt[1], vaxisArea.getX() - 3, middle - 12, vaxisArea.getWidth(), 12,
                       juce::Justification::bottomRight);
            g.drawText(txt[2], vaxisArea.getX() - 3, height - 14, vaxisArea.getWidth(), 12,
                       juce::Justification::centredRight);
        }

        if (mode == 0)
        {
            // Grid
            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryHorizontal));

            for (float y : {0.25f, 0.75f})
                g.drawLine(drawArea.getX() - 8, height * y, width, height * y);

            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::SecondaryVertical));

            for (float x : {0.25f, 0.5f, 0.75f})
                g.drawLine(drawArea.getX() + drawArea.getWidth() * x, 0,
                           drawArea.getX() + drawArea.getWidth() * x, height);

            // Borders
            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
            g.drawLine(0, 0, width, 0);
            g.drawLine(0, height, width, height);
            g.drawLine(axisSpaceX, 0, axisSpaceX, height);
            g.drawLine(width, 0, width, height);
            g.drawLine(axisSpaceX, middle, width, middle);

            // Graph
            auto p = juce::Path();
            if (!points.empty())
            {
                float dx = (width - axisSpaceX) / float(points.size() - 1);

                for (int i = 0; i < points.size(); ++i)
                {
                    float xp = dx * i;
                    float yp = 0.5f * (1 - points[i]) * height;

                    if (yp < 0.0f) // clamp to vertical bounds
                        yp = 0.0f;
                    else if (yp > height)
                        yp = height;

                    if (i == 0)
                        p.startNewSubPath(xp + axisSpaceX, middle);

                    p.lineTo(xp + axisSpaceX, yp);

                    if (i == points.size() - 1)
                        p.lineTo(xp + axisSpaceX, middle);
                }

                auto cg = juce::ColourGradient::vertical(
                    skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                    skin->getColor(Colors::MSEGEditor::GradientFill::StartColor), drawArea);
                cg.addColour(0.5, skin->getColor(Colors::MSEGEditor::GradientFill::EndColor));

                g.setGradientFill(cg);
                g.fillPath(p);

                g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
                g.strokePath(p, juce::PathStrokeType(1.0));
            }

            // Text
            g.setFont(font);
            g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
            g.drawText(std::to_string(frameNumber), axisSpaceX + 4, 4, width - 8, height - 8,
                       juce::Justification::topRight);
        }
        else
        {
            assert(mode == 1); // there are only two modes right now

            auto gs = juce::Graphics::ScopedSaveState(g);
            g.reduceClipRegion(axisSpaceX, 0, width - axisSpaceX, height);
            auto xpos = startX;

            int idx{0};

            for (int idx = 0; idx < frameCount; ++idx)
            {
                if (xpos + height < axisSpaceX || xpos > width - axisSpaceX)
                {
                    // We are outside the clip window. Do nothing.
                    xpos += height + fsGap;
                    continue;
                }

                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));

                auto p = juce::Path();
                auto pStroke = juce::Path();

                // alternate checkerboard bg
                auto bgColor = skin->getColor((Colors::MSEGEditor::Background));

                constexpr double brightnessThresh = 0.1;
                constexpr double brightnessDelta = 0.1;

                if (idx % 2 == 1)
                {
                    if (bgColor.getBrightness() < brightnessThresh)
                    {
                        g.setColour(bgColor.brighter(brightnessDelta));
                    }
                    else
                    {
                        g.setColour(bgColor.darker(brightnessDelta * 2.0));
                    }
                }
                else
                {
                    g.setColour(bgColor);
                }

                g.fillRect(xpos + axisSpaceX, 0, height, height);

                auto cpointOpt = overlay->evaluator->getFrame(idx);
                if (!cpointOpt.has_value())
                {
                    xpos += height + fsGap;
                    continue;
                }

                const auto &cpoint = *cpointOpt;

                if (!cpoint.empty())
                {
                    float dx = (height) / float(cpoint.size() - 1);
                    float xp{0};

                    for (int i = 0; i < cpoint.size(); ++i)
                    {
                        xp = dx * i + xpos;
                        float yp = 0.5f * (1 - cpoint[i]) * height;

                        if (yp < 0.0f) // clamp to vertical bounds
                            yp = 0.0f;
                        else if (yp > height)
                            yp = height;

                        if (i == 0)
                        {
                            p.startNewSubPath(xp + axisSpaceX, middle);
                            p.lineTo(xp + axisSpaceX, yp);
                            pStroke.startNewSubPath(xp + axisSpaceX, yp);
                        }
                        else
                        {
                            p.lineTo(xp + axisSpaceX, yp);
                            pStroke.lineTo(xp + axisSpaceX, yp);
                        }

                        if (i == cpoint.size() - 1)
                            p.lineTo(xp + axisSpaceX, middle);
                    }

                    auto cg = juce::ColourGradient::vertical(
                        skin->getColor(Colors::MSEGEditor::GradientFill::StartColor),
                        skin->getColor(Colors::MSEGEditor::GradientFill::StartColor), drawArea);
                    cg.addColour(0.5, skin->getColor(Colors::MSEGEditor::GradientFill::EndColor));

                    g.setGradientFill(cg);
                    g.fillPath(p);

                    g.setColour(skin->getColor(Colors::MSEGEditor::Curve));
                    g.strokePath(pStroke, juce::PathStrokeType(1.0));
                }

                g.setFont(font);
                g.setColour(skin->getColor(Colors::MSEGEditor::Axis::Text));
                g.drawText(std::to_string(idx + 1), xpos + axisSpaceX + 4, 4, height - 8,
                           height - 8, juce::Justification::topRight);
                g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
                g.drawVerticalLine(xpos + height + axisSpaceX, 0, height);

                xpos += height + fsGap;
            }

            // Borders
            g.setColour(skin->getColor(Colors::MSEGEditor::Grid::Primary));
            g.drawLine(0, 0, width, 0);
            g.drawLine(0, height, width, height);
            g.drawLine(axisSpaceX, 0, axisSpaceX, height);
            g.drawLine(width, 0, width, height);
        }
    }

    void resized() override {}
    bool isHandMove{false};
    void mouseEnter(const juce::MouseEvent &event) override
    {
        if (event.x > axisSpaceX)
        {
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            isHandMove = true;
        }
        else
        {
            isHandMove = false;
        }
    }
    void mouseMove(const juce::MouseEvent &event) override
    {
        if (event.x > axisSpaceX)
        {
            if (!isHandMove)
                setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            isHandMove = true;
        }
        else
        {
            if (isHandMove)
                setMouseCursor(juce::MouseCursor::NormalCursor);
            isHandMove = false;
        }
    }
    void mouseExit(const juce::MouseEvent &event) override
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        isHandMove = false;
    }
    void mouseDown(const juce::MouseEvent &event) override
    {
        lastDrag = event.getPosition().x + -event.getPosition().y;
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDrag(const juce::MouseEvent &event) override
    {
        if (mode == 0)
        {
            int currentDrag = event.getPosition().x + -event.getPosition().y;
            int delta = (currentDrag - lastDrag) * 2;
            lastDrag = currentDrag;

            int value = 0;
            if (delta > 0)
                value = 1;
            else if (delta < 0)
                value = -1;

            overlay->setCurrentFrame(value);
            repaint();
        }
        else
        {
            int currentDrag = event.getPosition().x + -event.getPosition().y;
            int delta = (currentDrag - lastDrag) * 2;
            lastDrag = currentDrag;

            if (delta != 0)
            {
                adjustStartX(delta);
                repaint();
            }
        }
    }

    void adjustStartX(int delta)
    {
        auto paintWidth = frameCount * (getHeight() + fsGap) - fsGap + 2 + axisSpaceX - getWidth();
        if (paintWidth > 0)
        {
            startX += delta;
            startX = std::min(0, startX);
            startX = std::max(-paintWidth, startX);
        }
        else
        {
            startX = 0;
        }
    }

    float accum{0.f};
    void mouseWheelMove(const juce::MouseEvent &event,
                        const juce::MouseWheelDetails &wheel) override
    {
        accum += wheel.deltaX * 400;
        while (accum > 1)
        {
            accum -= 1;
            adjustStartX(1);
            repaint();
        }
        while (accum < -1)
        {
            accum += 1;
            adjustStartX(-1);
            repaint();
        }
    }
    void mouseUp(const juce::MouseEvent &event) override
    {
        if (event.x < axisSpaceX)
        {
            isHandMove = false;
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }
        else
        {
            isHandMove = true;
        }
    }

    void mouseDoubleClick(const juce::MouseEvent &event) override
    {
        if (mode == 1)
        {
            startX = 0;
            repaint();
        }
    }

    void onSkinChanged() override { repaint(); }

    int frameNumber{1};
    std::vector<float> points;
    int frameCount{1};

    int mode{1};

  private:
    int lastDrag;
    int startX{0};
    static constexpr int fsGap{0};
    static constexpr int axisSpaceX{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetablePreviewComponent);
};

struct WavetableScriptControlArea : public juce::Component,
                                    public Surge::GUI::SkinConsumingComponent,
                                    public Surge::GUI::IComponentTagValue::Listener
{
    enum tags
    {
        tag_select_tab = 0x597500,
        tag_code_apply,
        tag_current_frame,
        tag_frames_value,
        tag_res_value,
        tag_generate_wt,
        tag_select_rendermode
    };

    WavetableScriptEditor *overlay{nullptr};
    SurgeGUIEditor *editor{nullptr};

    WavetableScriptControlArea(WavetableScriptEditor *ol, SurgeGUIEditor *ed)
        : overlay(ol), editor(ed)
    {
        setAccessible(true);
        setTitle("Controls");
        setDescription("Controls");
        setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    }

    void resized() override
    {
        if (skin)
        {
            rebuild();
        }
    }

    void rebuild()
    {
        removeAllChildren();

        {
            int btnWidth = 100;

            int labelHeight = 12;
            int buttonHeight = 14;
            int numfieldWidth = 32;
            int numfieldHeight = 12;

            int margin = 2;
            int xpos = 10;
            int ypos = 1 + labelHeight + margin;
            int marginPos = xpos + margin;

            codeL = newL("Code");
            codeL->setBounds(xpos, 1, 100, labelHeight);
            addAndMakeVisible(*codeL);

            renderModeL = newL("Display Mode");
            renderModeL->setBounds(xpos + btnWidth + 5, 1, 100, labelHeight);
            addAndMakeVisible(*renderModeL);

            codeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            auto btnrect = juce::Rectangle<int>(marginPos, ypos - 1, btnWidth, buttonHeight);
            codeS->setBounds(btnrect);
            codeS->setStorage(overlay->storage);
            codeS->setTitle("Code Selection");
            codeS->setDescription("Code Selection");
            codeS->setLabels({"Editor", "Prelude"});
            codeS->addListener(this);
            codeS->setTag(tag_select_tab);
            codeS->setHeightOfOneImage(buttonHeight);
            codeS->setRows(1);
            codeS->setColumns(2);
            codeS->setDraggable(true);
            codeS->setValue(overlay->getEditState().codeOrPrelude);
            codeS->setSkin(skin, associatedBitmapStore);
            addAndMakeVisible(*codeS);

            renderModeS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect =
                juce::Rectangle<int>(marginPos + btnWidth + 5, ypos - 1, btnWidth, buttonHeight);
            renderModeS->setBounds(btnrect);
            renderModeS->setStorage(overlay->storage);
            renderModeS->setTitle("Display Mode");
            renderModeS->setDescription("Display Mode");
            renderModeS->setLabels({"Single", "Filmstrip"});
            renderModeS->addListener(this);
            renderModeS->setTag(tag_select_rendermode);
            renderModeS->setHeightOfOneImage(buttonHeight);
            renderModeS->setRows(1);
            renderModeS->setColumns(2);
            renderModeS->setDraggable(true);
            renderModeS->setValue(overlay->rendererComponent->mode);
            renderModeS->setSkin(skin, associatedBitmapStore);
            renderModeS->setAccessible(false);
            addAndMakeVisible(*renderModeS);

            btnWidth = 60;

            applyS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(getWidth() / 2 - 30, ypos - 1, btnWidth, buttonHeight);
            applyS->setBounds(btnrect);
            applyS->setStorage(overlay->storage);
            applyS->setTitle("Apply");
            applyS->setDescription("Apply");
            applyS->setLabels({"Apply"});
            applyS->addListener(this);
            applyS->setTag(tag_code_apply);
            applyS->setHeightOfOneImage(buttonHeight);
            applyS->setRows(1);
            applyS->setColumns(1);
            applyS->setDraggable(true);
            applyS->setSkin(skin, associatedBitmapStore);
            applyS->setEnabled(false);
            addAndMakeVisible(*applyS);

            int bpos = getWidth() - marginPos - numfieldWidth * 3 - btnWidth - 10;
            auto images = skin->standardHoverAndHoverOnForIDB(IDB_MSEG_SNAPVALUE_NUMFIELD,
                                                              associatedBitmapStore);

            currentFrameL = newL("View");
            currentFrameL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*currentFrameL);

            currentFrameN = std::make_unique<Surge::Widgets::NumberField>();
            currentFrameN->setControlMode(Surge::Skin::Parameters::WTSE_FRAMES);
            currentFrameN->setIntValue(1);
            currentFrameN->addListener(this);
            currentFrameN->setTag(tag_current_frame);
            currentFrameN->setStorage(overlay->storage);
            currentFrameN->setTitle("Current Frame");
            currentFrameN->setDescription("Current Frame");
            currentFrameN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos, numfieldWidth, numfieldHeight);
            currentFrameN->setBounds(btnrect);
            currentFrameN->setBackgroundDrawable(images[0]);
            currentFrameN->setHoverBackgroundDrawable(images[1]);
            currentFrameN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            currentFrameN->setHoverTextColour(
                skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            addAndMakeVisible(*currentFrameN);

            bpos += numfieldWidth + 5;

            framesL = newL("Frames");
            framesL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*framesL);

            framesN = std::make_unique<Surge::Widgets::NumberField>();
            framesN->setControlMode(Surge::Skin::Parameters::WTSE_FRAMES);
            framesN->setIntValue(overlay->osc->wavetable_formula_nframes);
            framesN->addListener(this);
            framesN->setTag(tag_frames_value);
            framesN->setStorage(overlay->storage);
            framesN->setTitle("Max Frame");
            framesN->setDescription("Max Frame");
            framesN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos, numfieldWidth, numfieldHeight);
            framesN->setBounds(btnrect);
            framesN->setBackgroundDrawable(images[0]);
            framesN->setHoverBackgroundDrawable(images[1]);
            framesN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            framesN->setHoverTextColour(skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            framesN->onReturnPressed = [w = juce::Component::SafePointer(this)](auto tag, auto nf) {
                if (w)
                {
                    w->overlay->rerenderFromUIState();
                    return true;
                }
                return false;
            };
            addAndMakeVisible(*framesN);

            bpos += numfieldWidth + 5;

            resolutionL = newL("Samples");
            resolutionL->setBounds(bpos - 3, 1, 100, labelHeight);
            addAndMakeVisible(*resolutionL);

            resolutionN = std::make_unique<Surge::Widgets::NumberField>();
            resolutionN->setControlMode(Surge::Skin::Parameters::WTSE_RESOLUTION);
            resolutionN->setIntValue(overlay->osc->wavetable_formula_res_base);
            resolutionN->addListener(this);
            resolutionN->setTag(tag_res_value);
            resolutionN->setStorage(overlay->storage);
            resolutionN->setTitle("Samples");
            resolutionN->setDescription("Samples");
            resolutionN->setSkin(skin, associatedBitmapStore);
            btnrect = juce::Rectangle<int>(bpos, ypos, numfieldWidth, numfieldHeight);
            resolutionN->setBounds(btnrect);
            resolutionN->setBackgroundDrawable(images[0]);
            resolutionN->setHoverBackgroundDrawable(images[1]);
            resolutionN->setTextColour(skin->getColor(Colors::MSEGEditor::NumberField::Text));
            resolutionN->setHoverTextColour(
                skin->getColor(Colors::MSEGEditor::NumberField::TextHover));
            addAndMakeVisible(*resolutionN);

            bpos += numfieldWidth + 5;

            generateS = std::make_unique<Surge::Widgets::MultiSwitchSelfDraw>();
            btnrect = juce::Rectangle<int>(bpos, ypos - 1, btnWidth, buttonHeight);
            generateS->setBounds(btnrect);
            generateS->setStorage(overlay->storage);
            generateS->setTitle("Generate");
            generateS->setDescription("Generate");
            generateS->setLabels({"Generate"});
            generateS->addListener(this);
            generateS->setTag(tag_generate_wt);
            generateS->setHeightOfOneImage(buttonHeight);
            generateS->setRows(1);
            generateS->setColumns(1);
            generateS->setDraggable(false);
            generateS->setSkin(skin, associatedBitmapStore);
            generateS->setEnabled(true);
            addAndMakeVisible(*generateS);

            if (overlay->rendererComponent->mode == 1)
            {
                currentFrameL->setVisible(false);
                currentFrameN->setVisible(false);
            }
        }
    }

    std::unique_ptr<juce::Label> newL(const std::string &s)
    {
        auto res = std::make_unique<juce::Label>(s, s);
        res->setText(s, juce::dontSendNotification);
        res->setFont(skin->fontManager->getLatoAtSize(9, juce::Font::bold));
        res->setColour(juce::Label::textColourId, skin->getColor(Colors::MSEGEditor::Text));
        return res;
    }

    int32_t controlModifierClicked(GUI::IComponentTagValue *pControl,
                                   const juce::ModifierKeys &mods, bool isDoubleClickEvent) override
    {
        int tag = pControl->getTag();

        std::vector<std::pair<std::string, float>> options;
        bool hasTypein = false;
        std::string menuName;

        switch (tag)
        {
        case tag_select_tab:
        case tag_code_apply:
        case tag_current_frame:
        case tag_res_value:
        case tag_generate_wt:
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = editor->helpURLForSpecial("wtse-editor");
            auto hurl = editor->fullyResolvedHelpURL(msurl);

            editor->addHelpHeaderTo("WTSE Editor", hurl, contextMenu);

            contextMenu.showMenuAsync(editor->popupMenuOptions(this, false),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        break;
        case tag_frames_value:
        {
            hasTypein = true;
            menuName = "WTSE Wavetable Frame Amount";

            auto addStop = [&options](int v) {
                options.push_back(
                    std::make_pair(std::to_string(v), Parameter::intScaledToFloat(v, 256, 1)));
            };

            addStop(10);
            addStop(16);
            addStop(20);
            addStop(32);
            addStop(50);
            addStop(64);
            addStop(100);
            addStop(128);
            addStop(200);
            addStop(256);
            break;
        }
        default:
            break;
        }

        if (!options.empty())
        {
            auto contextMenu = juce::PopupMenu();

            auto msurl = SurgeGUIEditor::helpURLForSpecial(overlay->storage, "wtse-editor");
            auto hurl = SurgeGUIEditor::fullyResolvedHelpURL(msurl);
            auto tcomp = std::make_unique<Surge::Widgets::MenuTitleHelpComponent>(menuName, hurl);

            tcomp->setSkin(skin, associatedBitmapStore);
            auto hment = tcomp->getTitle();

            contextMenu.addCustomItem(-1, std::move(tcomp), nullptr, hment);

            contextMenu.addSeparator();

            for (const auto &op : options)
            {
                auto val = op.second;

                contextMenu.addItem(op.first, true, (val == pControl->getValue()),
                                    [val, pControl, this]() {
                                        pControl->setValue(val);
                                        valueChanged(pControl);

                                        auto iv = pControl->asJuceComponent();

                                        if (iv)
                                        {
                                            iv->repaint();
                                        }
                                    });
            }

            if (hasTypein)
            {
                contextMenu.addSeparator();

                auto c = pControl->asJuceComponent();

                auto handleTypein = [c, pControl, this](const std::string &s) {
                    auto i = std::atoi(s.c_str());

                    if (i >= 1 && i <= 256)
                    {
                        pControl->setValue(Parameter::intScaledToFloat(i, 256, 1));
                        valueChanged(pControl);

                        if (c)
                        {
                            c->repaint();
                        }

                        return true;
                    }
                    return false;
                };

                auto val =
                    std::to_string(Parameter::intUnscaledFromFloat(pControl->getValue(), 256, 1));

                auto showTypein = [this, c, handleTypein, menuName, pControl, val]() {
                    if (!typeinEditor)
                    {
                        typeinEditor =
                            std::make_unique<Surge::Overlays::TypeinLambdaEditor>(handleTypein);
                        getParentComponent()->addChildComponent(*typeinEditor);
                    }

                    typeinEditor->callback = handleTypein;
                    typeinEditor->setMainLabel(menuName);
                    typeinEditor->setValueLabels("current: " + val, "");
                    typeinEditor->setSkin(skin, associatedBitmapStore);
                    typeinEditor->setEditableText(val);
                    typeinEditor->setReturnFocusTarget(c);

                    auto topOfControl = c->getParentComponent()->getY();
                    auto pb = c->getBounds();
                    auto cx = pb.getCentreX();

                    auto r = typeinEditor->getRequiredSize();
                    cx -= r.getWidth() / 2;
                    r = r.withBottomY(topOfControl).withX(cx);
                    typeinEditor->setBounds(r);

                    typeinEditor->setVisible(true);
                    typeinEditor->grabFocus();
                };

                contextMenu.addItem(Surge::GUI::toOSCase("Edit Value: ") + val, true, false,
                                    showTypein);
            }
            contextMenu.showMenuAsync(editor->popupMenuOptions(),
                                      Surge::GUI::makeEndHoverCallback(pControl));
        }
        return 1;
    }

    void valueChanged(GUI::IComponentTagValue *c) override
    {
        auto tag = (tags)(c->getTag());

        switch (tag)
        {
        case tag_select_tab:
        {
            if (c->getValue() > 0.5)
            {
                overlay->showPreludeCode();
            }
            else
            {
                overlay->showModulatorCode();
            }
        }
        break;
        case tag_code_apply:
            overlay->applyCode();
            break;
        case tag_current_frame:
        {
            int currentFrame = currentFrameN->getIntValue();
            int maxFrames = framesN->getIntValue();
            if (currentFrame > maxFrames)
            {
                currentFrame = maxFrames;
                currentFrameN->setIntValue(currentFrame);
            }
            overlay->rendererComponent->frameNumber = currentFrame;
            overlay->rerenderFromUIState();
        }
        break;
        case tag_frames_value:
        {
            /*
            int currentFrame = currentFrameN->getIntValue();
            int maxFrames = framesN->getIntValue();
            if (currentFrame > maxFrames)
            {
                currentFrameN->setIntValue(maxFrames);
                overlay->rendererComponent->frameNumber = maxFrames;
            }
            overlay->osc->wavetable_formula_nframes = maxFrames;
            overlay->rerenderFromUIState();
            */
            overlay->setApplyEnabled(true);
        }
        break;
        case tag_res_value:
        {
            /*
            overlay->osc->wavetable_formula_res_base = resolutionN->getIntValue();
            overlay->rerenderFromUIState();
            */
            overlay->setApplyEnabled(true);
        }
        break;

        case tag_generate_wt:
            overlay->applyCode();
            overlay->generateWavetable();
            break;

        case tag_select_rendermode:
        {
            auto rm = renderModeS->getIntegerValue();
            if (rm == 1)
            {
                // FILMSTRIP
                currentFrameN->setVisible(false);
                currentFrameL->setVisible(false);

                overlay->rerenderFromUIState();
                overlay->rendererComponent->setFilmstrip();
            }
            else
            {
                assert(rm == 0);
                // Frame
                currentFrameN->setVisible(true);
                currentFrameL->setVisible(true);

                overlay->rerenderFromUIState();
                overlay->rendererComponent->setSingleFrame();
            }
        }
        default:
            break;
        }
    }

    std::unique_ptr<Surge::Overlays::TypeinLambdaEditor> typeinEditor;
    std::unique_ptr<juce::Label> codeL, renderModeL, currentFrameL, framesL, resolutionL;
    std::unique_ptr<Surge::Widgets::MultiSwitchSelfDraw> codeS, renderModeS, applyS, generateS;
    std::unique_ptr<Surge::Widgets::NumberField> currentFrameN, framesN, resolutionN;

    void paint(juce::Graphics &g) override { g.fillAll(skin->getColor(Colors::MSEGEditor::Panel)); }

    void onSkinChanged() override { rebuild(); }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableScriptControlArea);
};

WavetableScriptEditor::WavetableScriptEditor(SurgeGUIEditor *ed, SurgeStorage *s,
                                             OscillatorStorage *os, int oid, int scene,
                                             Surge::GUI::Skin::ptr_t skin)

    : CodeEditorContainerWithApply(ed, s, skin, false), osc(os), osc_id(oid), scene(scene),
      editor(ed)
{
    mainEditor->setScrollbarThickness(8);
    mainEditor->setTitle("Wavetable Code");
    mainEditor->setDescription("Wavetable Code");
    mainEditor->onFocusLost = [this]() { this->saveState(); };

    if (osc->wavetable_formula == "")
    {
        mainDocument->insertText(0,
                                 Surge::WavetableScript::LuaWTEvaluator::defaultWavetableScript());
    }
    else
    {
        mainDocument->insertText(0, osc->wavetable_formula);
    }

    mainDocument->clearUndoHistory();
    mainDocument->setSavePoint();

    preludeDocument = std::make_unique<juce::CodeDocument>();
    preludeDocument->insertText(0, Surge::LuaSupport::getWTSEPrelude());

    preludeDisplay =
        std::make_unique<SurgeCodeEditorComponent>(*preludeDocument, tokenizer.get(), skin);
    preludeDisplay->setTabSize(4, true);
    preludeDisplay->setReadOnly(true);
    preludeDisplay->setScrollbarThickness(8);
    preludeDisplay->setTitle("Wavetable Prelude Code");
    preludeDisplay->setDescription("Wavetable Prelude Code");
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);

    controlArea = std::make_unique<WavetableScriptControlArea>(this, editor);
    addAndMakeVisible(*controlArea);
    addAndMakeVisible(*mainEditor);
    addChildComponent(*search);
    addChildComponent(*gotoLine);

    addChildComponent(*preludeDisplay);

    search->onFocusLost = [this]() { this->saveState(); };
    gotoLine->onFocusLost = [this]() { this->saveState(); };

    rendererComponent = std::make_unique<WavetablePreviewComponent>(this, editor, skin);
    addAndMakeVisible(*rendererComponent);

    switch (getEditState().codeOrPrelude)
    {
    case 0:
        showModulatorCode();
        break;
    case 1:
        showPreludeCode();
        break;
    }

    evaluator = std::make_unique<Surge::WavetableScript::LuaWTEvaluator>();
    evaluator->setStorage(storage);

    initState(getEditState().codeEditor);
    loadState();
}

WavetableScriptEditor::~WavetableScriptEditor() = default;

DAWExtraStateStorage::EditorState::WavetableScriptEditState &WavetableScriptEditor::getEditState()
{
    return storage->getPatch().dawExtraState.editor.wavetableScriptEditState[scene][osc_id];
}

void WavetableScriptEditor::onSkinChanged()
{
    CodeEditorContainerWithApply::onSkinChanged();
    preludeDisplay->setFont(skin->getFont(Fonts::LuaEditor::Code));
    EditorColors::setColorsFromSkin(preludeDisplay.get(), skin);
    controlArea->setSkin(skin, associatedBitmapStore);
    rendererComponent->setSkin(skin, associatedBitmapStore);
}

void WavetableScriptEditor::setupEvaluator()
{
    auto resi = controlArea->resolutionN->getIntValue();
    auto respt = 32;
    for (int i = 1; i < resi; ++i)
        respt *= 2;

    evaluator->setStorage(storage);
    evaluator->setScript(mainDocument->getAllContent().toStdString());
    evaluator->setResolution(respt);
    evaluator->setFrameCount(controlArea->framesN->getIntValue());
}

void WavetableScriptEditor::applyCode()
{
    removeTrailingWhitespaceFromDocument();

    osc->wavetable_formula = mainDocument->getAllContent().toStdString();
    osc->wavetable_formula_res_base = controlArea->resolutionN->getIntValue();
    osc->wavetable_formula_nframes = controlArea->framesN->getIntValue();

    lastFrames = -1;
    setupEvaluator();
    rerenderFromUIState();
    editor->repaintFrame();
    setApplyEnabled(false);
    mainEditor->grabKeyboardFocus();

    repaint();
}

void WavetableScriptEditor::forceRefresh()
{
    mainDocument->replaceAllContent(osc->wavetable_formula);
    editor->repaintFrame();
}

void WavetableScriptEditor::setApplyEnabled(bool b)
{
    if (controlArea)
    {
        controlArea->applyS->setEnabled(b);
        controlArea->applyS->repaint();
    }
}

void WavetableScriptEditor::resized()
{
    auto t = getTransform().inverted();
    auto width = getWidth();
    auto height = getHeight();
    t.transformPoint(width, height);

    int itemWidth = 100;
    int topHeight = 20;
    int controlHeight = 35;
    int rendererHeight = 125;

    auto edRect =
        juce::Rectangle<int>(2, 2, width - 4, height - controlHeight - rendererHeight - 6);
    mainEditor->setBounds(edRect);
    preludeDisplay->setBounds(edRect);
    controlArea->setBounds(0, height - controlHeight, width, controlHeight + rendererHeight);
    rendererComponent->setBounds(2, height - rendererHeight - controlHeight - 2, width - 2,
                                 rendererHeight);

    search->resize();
    gotoLine->resize();
    rerenderFromUIState();
}

void WavetableScriptEditor::showModulatorCode()
{
    preludeDisplay->setVisible(false);
    mainEditor->setVisible(true);
    getEditState().codeOrPrelude = 0;
}

void WavetableScriptEditor::showPreludeCode()
{
    preludeDisplay->setVisible(true);
    mainEditor->setVisible(false);
    getEditState().codeOrPrelude = 1;
}

void WavetableScriptEditor::escapeKeyPressed()
{
    auto c = getParentComponent();
    while (c)
    {
        if (auto olw = dynamic_cast<OverlayWrapper *>(c))
        {
            olw->onClose();
            return;
        }
        c = c->getParentComponent();
    }
}

void WavetableScriptEditor::rerenderFromUIState()
{
    auto resi = controlArea->resolutionN->getIntValue();
    auto nfr = controlArea->framesN->getIntValue();
    auto rm = controlArea->renderModeS->getIntegerValue();
    auto cfr = rendererComponent->frameNumber;

    if (rm == lastRm)
    {
        if (rm == 0 && resi == lastRes && nfr == lastFrames && cfr == lastFrame)
        {
            return;
        }

        if (rm == 1 && resi == lastRes && nfr == lastFrames)
        {
            return;
        }
    }

    lastRes = resi;
    lastFrame = cfr;
    lastFrames = nfr;
    lastRm = rm;

    auto respt = 32;
    for (int i = 1; i < resi; ++i)
        respt *= 2;

    setupEvaluator();

    if (rm == 0)
    {
        auto rs = evaluator->getFrame(cfr - 1);
        if (rs.has_value())
        {
            rendererComponent->points = *rs;
        }
        else
        {
            rendererComponent->points.clear();
        }
    }
    else
    {
        rendererComponent->frameCount = nfr;
        rendererComponent->adjustStartX(0);
    }
    rendererComponent->repaint();
}

void WavetableScriptEditor::setCurrentFrame(int value)
{
    int frameNumber = rendererComponent->frameNumber;
    int maxFrames = controlArea->framesN->getIntValue();
    frameNumber += value;

    if (frameNumber < 1)
        frameNumber = 1;
    else if (frameNumber > maxFrames)
        frameNumber = maxFrames;

    rendererComponent->frameNumber = frameNumber;
    controlArea->currentFrameN->setIntValue(frameNumber);
}

void WavetableScriptEditor::generateWavetable()
{
    auto resi = controlArea->resolutionN->getIntValue();
    auto nfr = controlArea->framesN->getIntValue();
    auto respt = 32;

    for (int i = 1; i < resi; ++i)
        respt *= 2;

    wt_header wh;
    float *wd = nullptr;
    setupEvaluator();
    evaluator->populateWavetable(wh, &wd);
    storage->waveTableDataMutex.lock();
    osc->wt.BuildWT(wd, wh, wh.flags & wtf_is_sample);
    osc->wavetable_display_name = evaluator->getSuggestedWavetableName();
    storage->waveTableDataMutex.unlock();

    delete[] wd;
    editor->oscWaveform->repaintForceForWT();
    editor->repaintFrame();
}

std::optional<std::pair<std::string, std::string>>
WavetableScriptEditor::getPreCloseChickenBoxMessage()
{
    if (controlArea->applyS->isEnabled())
    {
        return std::make_pair("Close Wavetable Editor",
                              "Do you really want to close the wavetable editor? Any "
                              "changes that were not applied will be lost!");
    }
    return std::nullopt;
}

} // namespace Overlays
} // namespace Surge
