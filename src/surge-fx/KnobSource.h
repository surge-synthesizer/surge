#pragma once // this doesn't seem like the norm in this repo but still a wip

#include <juce_gui_basics/juce_gui_basics.h>
#include <sst/jucegui/data/Continuous.h>
// #include <sst/jucegui/data/Discrete.h>
#include <sst/jucegui/util/DebugHelpers.h>

#include <sst/jucegui/style/StyleSheet.h>
#include <sst/jucegui/components/Knob.h>
#include <sst/jucegui/components/BaseStyles.h>

struct ConcreteCM : sst::jucegui::data::ContinuousModulatable
{
    ConcreteCM() {};
    std::string label{"A Knob"};
    std::string getLabel() const override { return label; }
    float value{0};
    float getValue() const override { return value; }
    float getDefaultValue() const override { return (getMax() - getMin()) / 2.0; }
    void setValueFromGUI(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
        for (auto *l : modellisteners)
            l->dataChanged();
    }
    void setValueFromModel(const float &f) override
    {
        value = f;
        for (auto *l : guilisteners)
            l->dataChanged();
    }

    float min{0}, max{1};
    float getMin() const override { return min; }
    float getMax() const override { return max; }

    float mv{0.2};
    float getModulationValuePM1() const override { return mv; }
    void setModulationValuePM1(const float &f) override { mv = f; }
    bool isModulationBipolar() const override { return isBipolar(); } // sure why not
};

struct StyleSheetBuiltInImpl : public sst::jucegui::style::StyleSheet
{
    StyleSheetBuiltInImpl() {}
    ~StyleSheetBuiltInImpl() {}

    std::unordered_map<std::string, std::unordered_map<std::string, juce::Colour>> colours;
    std::unordered_map<std::string, std::unordered_map<std::string, juce::Font>> fonts;
    void setColour(const StyleSheet::Class &c, const StyleSheet::Property &p,
                   const juce::Colour &col) override
    {
        jassert(isValidPair(c, p));
        colours[c.cname][p.pname] = col;
    }
    void setFont(const StyleSheet::Class &c, const StyleSheet::Property &p,
                 const juce::Font &f) override
    {
        jassert(isValidPair(c, p));

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

        fonts[c.cname][p.pname] = f;

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    }

    void replaceFontsWithTypeface(const juce::Typeface::Ptr &p) override
    {
        for (auto &[cn, propFonts] : fonts)
        {
            for (auto &[pn, f] : propFonts)
            {
                auto nf = SST_JUCE_FONT_CTOR(p);
                nf.setHeight(f.getHeight());
                f = nf;
            }
        }
    }
    void replaceFontsWithFamily(const juce::String familyName) override { assert(false); }

    bool hasColour(const Class &c, const Property &p) const override
    {
        assert(p.type == Property::COLOUR);
        auto byC = colours.find(c.cname);
        if (byC != colours.end())
        {
            auto byP = byC->second.find(p.pname);
            if (byP != byC->second.end())
                return true;
        }
        return false;
    }

    juce::Colour getColour(const Class &c, const Property &p) const override
    {
        auto r = getColourOptional(c, p);
        if (r.has_value())
            return *r;

        std::cout << __FILE__ << ":" << __LINE__ << " COLOUR Missing : " << c.cname
                  << "::" << p.pname << std::endl;
        return juce::Colours::red;
    }

    std::optional<juce::Colour> getColourOptional(const Class &c, const Property &p) const override
    {
        assert(p.type == Property::COLOUR);
        auto byC = colours.find(c.cname);
        if (byC != colours.end())
        {
            auto byP = byC->second.find(p.pname);
            if (byP != byC->second.end())
            {
                jassert(isValidPair(c, p));

                return byP->second;
            }
        }

        auto parC = inheritFromTo.find(c.cname);
        if (parC != inheritFromTo.end())
        {
            for (const auto &k : parC->second)
            {
                auto sub = getColourOptional({k.c_str()}, p);
                if (sub.has_value())
                    return sub;
            }
        }
        return std::nullopt;
    }

    bool hasFont(const Class &c, const Property &p) const override
    {
        assert(p.type == Property::FONT);
        auto byC = fonts.find(c.cname);
        if (byC != fonts.end())
        {
            auto byP = byC->second.find(p.pname);
            if (byP != byC->second.end())
                return true;
        }
        return false;
    }

    juce::Font getFont(const Class &c, const Property &p) const override
    {
        auto r = getFontOptional(c, p);
        if (r.has_value())
            return *r;

        std::cout << __FILE__ << ":" << __LINE__ << " FONT Missing : " << c.cname << "::" << p.pname
                  << std::endl;
        return SST_JUCE_FONT_CTOR(36, juce::Font::italic);
    }
    std::optional<juce::Font> getFontOptional(const Class &c, const Property &p) const override
    {
        assert(p.type == Property::FONT);
        auto byC = fonts.find(c.cname);
        if (byC != fonts.end())
        {
            auto byP = byC->second.find(p.pname);
            if (byP != byC->second.end())
            {
                jassert(isValidPair(c, p));
                return byP->second;
            }
        }

        auto parC = inheritFromTo.find(c.cname);
        if (parC != inheritFromTo.end())
        {
            for (const auto &k : parC->second)
            {
                auto q = getFontOptional({k.c_str()}, p);
                if (q.has_value())
                    return *q;
            }
        }

        return std::nullopt;
    }
};

struct KnobStyleSheet : public StyleSheetBuiltInImpl
{
    bool initialized{false};
    KnobStyleSheet() {}
    void initialize()
    {
        if (initialized)
            return;

        initialized = true;

        {
            using n = sst::jucegui::components::base_styles::Base;
            setColour(n::styleClass, n::background, juce::Colour(0x25, 0x25, 0x28));
        }

        {
            using n = sst::jucegui::components::base_styles::SelectableRegion;
            setColour(n::styleClass, n::backgroundSelected, juce::Colour(0x45, 0x45, 0x48));
        }

        {
            using n = sst::jucegui::components::base_styles::Outlined;
            setColour(n::styleClass, n::outline, juce::Colour(0x50, 0x50, 0x50));
            setColour(n::styleClass, n::brightoutline, juce::Colour(0x70, 0x70, 0x70));
        }

        {
            using n = sst::jucegui::components::base_styles::BaseLabel;
            setColour(n::styleClass, n::labelcolor, juce::Colour(220, 220, 220));
            setColour(n::styleClass, n::labelcolor_hover, juce::Colour(240, 240, 235));
            setFont(n::styleClass, n::labelfont, SST_JUCE_FONT_CTOR(13));
        }

        // {
        //     using n = sst::jucegui::components::NamedPanel::Styles;
        //     setColour(n::styleClass, n::labelrule, juce::Colour(0x70, 0x70, 0x70));
        //     setColour(n::styleClass, n::selectedtab, juce::Colour(0xFF, 0x90, 00));
        //     setColour(n::styleClass, n::accentedPanel, juce::Colour(0xFF, 0x90, 00));
        // }

        // {
        //     using w = sst::jucegui::components::WindowPanel::Styles;
        //     setColour(w::styleClass, w::bgstart, juce::Colour(0x3B, 0x3D, 0x40));
        //     setColour(w::styleClass, w::bgend, juce::Colour(0x1B, 0x1D, 0x20));
        // }

        // {
        //     using n = sst::jucegui::components::base_styles::PushButton;
        //     setColour(n::styleClass, n::fill, juce::Colour(0x60, 0x60, 0x60));
        //     setColour(n::styleClass, n::fill_hover, juce::Colour(0x90, 0x85, 0x83));
        //     setColour(n::styleClass, n::fill_pressed, juce::Colour(0x80, 0x80, 0x80));
        // }

        // {
        //     using n = sst::jucegui::components::MenuButton::Styles;
        //     setColour(n::styleClass, n::menuarrow_hover, juce::Colour(0xFF, 0x90, 0x00));
        // }

        // {
        //     using n = sst::jucegui::components::JogUpDownButton::Styles;
        //     setColour(n::styleClass, n::jogbutton_hover, juce::Colour(0xFF, 0x90, 0x00));
        // }

        // {
        //     using n = sst::jucegui::components::base_styles::ValueBearing;
        //     setColour(n::styleClass, n::value, juce::Colour(0xFF, 0x90, 0x00));
        //     setColour(n::styleClass, n::value_hover, juce::Colour(0xFF, 0xA0, 0x30));
        //     setColour(n::styleClass, n::valuelabel, juce::Colour(0x20, 0x10, 0x20));
        //     setColour(n::styleClass, n::valuelabel_hover, juce::Colour(0x30, 0x20, 0x10));
        // }

        // {
        //     using n = sst::jucegui::components::MultiSwitch::Styles;
        //     setColour(n::styleClass, n::unselected_hover, juce::Colour(0x50, 0x50, 0x50));
        //     setColour(n::styleClass, n::valuebg, juce::Colour(0x30, 0x20, 0x00));
        // }

        {
            using n = sst::jucegui::components::base_styles::ValueGutter;
            setColour(n::styleClass, n::gutter, juce::Colour(0x05, 0x05, 0x00));
            setColour(n::styleClass, n::gutter_hover, juce::Colour(0x40, 0x25, 0x00));
        }

        {
            using n = sst::jucegui::components::base_styles::GraphicalHandle;
            setColour(n::styleClass, n::handle, juce::Colour(0xD0, 0xD0, 0xD0));
            setColour(n::styleClass, n::handle_outline, juce::Colour(0x0F, 0x09, 0x00));
            setColour(n::styleClass, n::handle_hover, juce::Colour(0xFF, 0xE0, 0xC0));

            setColour(n::styleClass, n::modulation_handle, juce::Colour(0xA0, 0xF0, 0xA0));
            setColour(n::styleClass, n::modulation_handle_hover, juce::Colour(0xB0, 0xFF, 0xB0));
        }

        {
            using n = sst::jucegui::components::base_styles::ModulationValueBearing;
            setColour(n::styleClass, n::modulated_by_other, juce::Colour(0x20, 0x40, 0x20));
            setColour(n::styleClass, n::modulated_by_selected, juce::Colour(0x30, 0x50, 0x30));
            setColour(n::styleClass, n::modulation_value, juce::Colour(0x20, 0xA0, 0x20));
            setColour(n::styleClass, n::modulation_opposite_value, juce::Colour(0x20, 0x80, 0x20));
            setColour(n::styleClass, n::modulation_value_hover, juce::Colour(0x40, 0xA0, 0x40));
            setColour(n::styleClass, n::modulation_opposite_value_hover,
                      juce::Colour(0x40, 0x80, 0x40));
        }

        {
            using n = sst::jucegui::components::Knob::Styles;
            setColour(n::styleClass, n::knobbase, juce::Colour(82, 82, 82));
        }

        // {
        //     using n = sst::jucegui::components::VUMeter::Styles;
        //     setColour(n::styleClass, n::vu_gutter, juce::Colour(0, 0, 0));
        //     setColour(n::styleClass, n::vu_gradstart, juce::Colour(200, 200, 100));
        //     setColour(n::styleClass, n::vu_gradend, juce::Colour(100, 100, 220));
        //     setColour(n::styleClass, n::vu_overload, juce::Colour(200, 50, 50));
        // }

        // {
        //     using n = sst::jucegui::components::TabbedComponent::Styles;
        //     setColour(n::styleClass, n::tabSelectedLabelColor, juce::Colour(0xFF, 0x90, 0x00));
        //     setColour(n::styleClass, n::tabUnselectedLabelColor, juce::Colour(0xAF, 0xA0, 0xA0));
        //     setColour(n::styleClass, n::tabSelectedFillColor, juce::Colour(0x0A, 0x0A, 0x0A));
        //     setColour(n::styleClass, n::tabUnselectedOutlineColor, juce::Colour(0xA0, 0xA0,
        //     0xA0));
        // }

        // {
        //     using n = sst::jucegui::components::TabularizedTreeViewer::Styles;

        //     setColour(n::styleClass, n::toggleboxcol, juce::Colour(190, 190, 190));
        //     setColour(n::styleClass, n::toggleglyphcol, juce::Colours::white);
        //     setColour(n::styleClass, n::toggleglyphhovercol, juce::Colour(0xFF, 90, 80));

        //     setColour(n::styleClass, n::connectorcol, juce::Colour(160, 160, 160));
        // }

        // {
        //     using n = sst::jucegui::components::ToolTip::Styles;
        //     setFont(n::styleClass, n::datafont,
        //             getFont(components::base_styles::BaseLabel::styleClass,
        //                     components::base_styles::BaseLabel::labelfont));
        // }
    }
};