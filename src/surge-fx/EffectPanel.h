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
#ifndef SURGE_SRC_SURGE_FX_EFFECTPANEL_H
#define SURGE_SRC_SURGE_FX_EFFECTPANEL_H

#include "BinaryData.h"
#include "SurgeStorage.h"
#include "juce_gui_basics/juce_gui_basics.h"
#include "sst/jucegui/layouts/JsonLayoutEngine.h"
#include "sst/jucegui/components/Knob.h"

#include <array>

namespace surge::fx::effectPanel
{
// Layouts share a 224px canvas. Inset full-width section rules by 4px, keep
// control centres aligned between rows, and reserve space for captions and option glyphs.
inline constexpr int contentWidth = 224;
inline constexpr int gutter = 10;
inline constexpr int width = contentWidth + 2 * gutter, height = 320;
inline constexpr int defaultZoom = 125;
inline constexpr int captionHeight = 14;
inline constexpr int captionFontHeight = 12;
inline const juce::Rectangle<int> frame(2, 2, width - 4, height - 4);
inline const juce::Point<int> contentOrigin(gutter, 30);
inline const juce::Rectangle<int> headerArea(gutter, 6, contentWidth, 22);
inline const juce::Rectangle<int> statusArea(gutter, 296, contentWidth, 18);

enum class Kind
{
    knob,
    fader,
    choice,
    power,
    extend,
    group
};
struct Control
{
    Kind kind;
    juce::Rectangle<float> bounds;
    int slot;
    juce::String label;
    bool useParamName;
};
using Layout = std::vector<Control>;

// Captions can use the space beside small knobs, but must not run into a neighbour's caption.
inline juce::Rectangle<int> captionBounds(const Layout &layout, const Control &control)
{
    const auto pixels = control.bounds.toNearestIntEdges();
    const int centre = pixels.getCentreX();
    int halfWidth = std::min({41, centre - 3, contentWidth - 3 - centre});
    for (const auto &other : layout)
    {
        if (&other == &control || (other.kind != Kind::knob && other.kind != Kind::choice) ||
            (!other.useParamName && other.label.isEmpty()))
            continue;
        const auto neighbour = other.bounds.toNearestIntEdges();
        if (std::abs(neighbour.getBottom() - pixels.getBottom()) < captionHeight)
            halfWidth = std::min(halfWidth, (std::abs(neighbour.getCentreX() - centre) - 2) / 2);
    }
    return {centre - halfWidth, pixels.getBottom(), 2 * halfWidth, captionHeight};
}

struct LayoutHost : sst::jucegui::layouts::JsonLayoutHost
{
    Layout controls;

    std::string resolveJsonPath(const std::string &path) const override
    {
        auto resource = path;
        std::replace(resource.begin(), resource.end(), '.', '_');
        int length = 0;
        if (const auto *data = BinaryData::getNamedResource(resource.c_str(), length))
            return std::string(data, length);
        return {};
    }

    void createBindAndPosition(const sst::jucegui::layouts::json_document::Control &control,
                               const sst::jucegui::layouts::json_document::Class &cls) override
    {
        Kind kind;
        if (cls.controlType == "knob")
            kind = Kind::knob;
        else if (cls.controlType == "fader")
            kind = Kind::fader;
        else if (cls.controlType == "choice")
            kind = Kind::choice;
        else if (cls.controlType == "power")
            kind = Kind::power;
        else if (cls.controlType == "extend-light")
            kind = Kind::extend;
        else if (cls.controlType == "group")
            kind = Kind::group;
        else
            return;

        const auto &p = control.position;
        const int slot = control.binding ? control.binding->index : -1;
        controls.push_back({kind,
                            {float(p.x), float(p.y), float(p.w), float(p.h)},
                            slot,
                            juce::String(control.label.value_or("")),
                            !control.label.has_value()});
    }
};

inline Layout loadLayout(const char *path)
{
    LayoutHost host;
    sst::jucegui::layouts::JsonLayoutEngine engine(host);
    auto result = engine.processJsonPath(path);
    if (!result)
    {
        juce::Logger::writeToLog(juce::String(path) + ": " +
                                 result.error.value_or("Invalid layout"));
        return {};
    }
    return std::move(host.controls);
}

inline constexpr auto layoutPaths = [] {
    std::array<const char *, n_fx_types> paths{};
    paths[fxt_off] = "off.json";
    paths[fxt_delay] = "delay.json";
    paths[fxt_reverb] = "reverb1.json";
    paths[fxt_phaser] = "phaser.json";
    paths[fxt_rotaryspeaker] = "rotary_speaker.json";
    paths[fxt_distortion] = "distortion.json";
    paths[fxt_eq] = "parametric_eq.json";
    paths[fxt_freqshift] = "frequency_shifter.json";
    paths[fxt_conditioner] = "conditioner.json";
    paths[fxt_chorus4] = "chorus.json";
    paths[fxt_vocoder] = "vocoder.json";
    paths[fxt_reverb2] = "reverb2.json";
    paths[fxt_flanger] = "flanger.json";
    paths[fxt_ringmod] = "ring_modulator.json";
    paths[fxt_airwindows] = "airwindows.json";
    paths[fxt_neuron] = "neuron.json";
    paths[fxt_geq11] = "graphic_eq.json";
    paths[fxt_resonator] = "resonator.json";
    paths[fxt_chow] = "chow.json";
    paths[fxt_exciter] = "exciter.json";
    paths[fxt_ensemble] = "ensemble.json";
    paths[fxt_combulator] = "combulator.json";
    paths[fxt_nimbus] = "nimbus.json";
    paths[fxt_tape] = "tape.json";
    paths[fxt_treemonster] = "treemonster.json";
    paths[fxt_waveshaper] = "waveshaper.json";
    paths[fxt_mstool] = "mid_side.json";
    paths[fxt_spring_reverb] = "spring_reverb.json";
    paths[fxt_bonsai] = "bonsai.json";
    paths[fxt_audio_input] = "audio_input.json";
    paths[fxt_floaty_delay] = "floaty_delay.json";
    paths[fxt_convolution] = "convolution.json";
    return paths;
}();

inline const Layout &layout(int effectType)
{
    static const auto layouts = [] {
        std::array<Layout, n_fx_types> result;
        for (int i = 0; i < n_fx_types; ++i)
            if (layoutPaths[i])
                result[i] = loadLayout(layoutPaths[i]);
        return result;
    }();
    static const Layout empty;
    return effectType >= 0 && effectType < n_fx_types ? layouts[effectType] : empty;
}

inline juce::AffineTransform panelTransform(juce::Rectangle<float> panel)
{
    return juce::AffineTransform::scale(panel.getWidth() / width, panel.getHeight() / height)
        .translated(panel.getX(), panel.getY());
}

using KnobStyles = sst::jucegui::components::Knob::Styles;
inline const juce::Colour panelColour(0xff262a2f), surroundColour(0xff1b1d20);
inline const juce::Colour borderColour(0xff444950), ruleColour(0xff626970);
inline const juce::Colour textColour(0xffc6cbd0), accentColour(0xffff9300);

inline auto panelStyle()
{
    using Style = sst::jucegui::style::StyleSheet;
    using namespace sst::jucegui::components::base_styles;
    static const auto style = [] {
        Style::initializeStyleSheets([] {});
        auto sheet = Style::getBuiltInStyleSheet(Style::DARK);
        const auto hover = [](juce::Colour c) { return c.brighter(0.1f); };
        sheet->setColour(Base::styleClass, Base::background, panelColour);
        sheet->setColour(Base::styleClass, Base::background_hover, hover(panelColour));
        sheet->setColour(BaseLabel::styleClass, BaseLabel::labelcolor, textColour);
        sheet->setColour(BaseLabel::styleClass, BaseLabel::labelcolor_hover, hover(textColour));
        sheet->setColour(Outlined::styleClass, Outlined::outline, surroundColour);
        sheet->setColour(Outlined::styleClass, Outlined::brightoutline, ruleColour);
        sheet->setColour(ValueBearing::styleClass, ValueBearing::value, accentColour);
        sheet->setColour(ValueBearing::styleClass, ValueBearing::value_hover, hover(accentColour));
        const juce::Colour gutter(0xff41474e), handle(0xffededed);
        sheet->setColour(ValueGutter::styleClass, ValueGutter::gutter, gutter);
        sheet->setColour(ValueGutter::styleClass, ValueGutter::gutter_hover, hover(gutter));
        sheet->setColour(KnobStyles::styleClass, KnobStyles::knobbase,
                         juce::Colours::transparentBlack);
        sheet->setColour(KnobStyles::styleClass, KnobStyles::handle, handle);
        sheet->setColour(KnobStyles::styleClass, KnobStyles::handle_hover, hover(handle));
        sheet->setFont(BaseLabel::styleClass, BaseLabel::labelfont,
                       juce::Font(juce::FontOptions().withPointHeight(11.f)));
        sheet->setColour(PopupMenu::styleClass, PopupMenu::background, panelColour);
        sheet->setColour(PopupMenu::styleClass, PopupMenu::labelcolor, textColour);
        sheet->setColour(PopupMenu::styleClass, PopupMenu::hightlightbackground, borderColour);
        sheet->setColour(PopupMenu::styleClass, PopupMenu::hightlighttextcolor,
                         juce::Colours::white);
        sheet->setFont(
            PopupMenu::styleClass, PopupMenu::menufont,
            sheet->getFont(KnobStyles::styleClass, KnobStyles::labelfont)
                .withHeight(captionFontHeight));
        sheet->setKnobRingStrokeWidth(3, 3, 25);
        return sheet;
    }();
    return style;
}

inline juce::Colour accent() { return accentColour; }
inline juce::Colour text() { return textColour; }

inline juce::Font font()
{
    return panelStyle()->getFont(KnobStyles::styleClass, KnobStyles::labelfont);
}

inline void paint(juce::Graphics &g, juce::Rectangle<float> panel)
{
    g.fillAll(surroundColour);
    if (panel.isEmpty())
        return;

    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(panelTransform(panel));
    g.setColour(panelColour);
    g.fillRoundedRectangle(frame.toFloat(), 2.f);
    g.setColour(borderColour);
    g.drawRoundedRectangle(frame.toFloat().reduced(0.5f), 2.f, 1.f);
}
} // namespace surge::fx::effectPanel

#endif // SURGE_SRC_SURGE_FX_EFFECTPANEL_H
