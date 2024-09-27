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

#ifndef SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATABLECONTROLINTERFACE_H
#define SURGE_SRC_SURGE_XT_GUI_WIDGETS_MODULATABLECONTROLINTERFACE_H

#include "juce_gui_basics/juce_gui_basics.h"

namespace Surge
{
namespace Widgets
{
/*
 * Anything which binds to a modulatable control should inherit
 * this utility / marker interface
 */
struct ModulatableControlInterface
{
    // there is a general assumption that these won't return nullptr.
    virtual Surge::GUI::IComponentTagValue *asControlValueInterface() = 0;
    virtual juce::Component *asJuceComponent() = 0;

    /*
     * This section I've fully reviewed
     */

    /**
     * The control represents a slider which is in semitones (such as pitch)
     * @param b true for semitones
     */
    virtual void setIsSemitone(bool b) { isSemitone = b; }
    bool isSemitone{false};

    /**
     * Hand a function pointer to determine if the slider is bipolar or unipoar.
     * @param f A function which returns true for bipolar sliders
     */
    virtual void setBipolarFn(std::function<bool()> f) { isBipolarFn = f; }
    std::function<bool()> isBipolarFn{[]() { return false; }};

    /*
     * The modulation functions form a variety of clusters which let the
     * element know if it can be modulated, if it has bene, if it actively
     * is being, and so forth
     */

    /**
     * The sliders know if they are currently attached to a parameter which
     * is a valid modulation for the state of the client. This is basically just
     * a cache external clients use; it is not used internally.
     * @param b
     */
    virtual void setIsValidToModulate(bool b) { iv2m = b; }; // make these == 0 later
    virtual bool getIsValidToModulate() const { return iv2m; }
    bool iv2m{false};

    /*
     * Editors can be labeled. it is up to he editor whether to show a label.
     * A label comes either as a dynamic label function or as an actual label
     * but not as both.
     */
    virtual void setDynamicLabel(std::function<std::string()> f)
    {
        labelFn = f;
        hasLabelFn = true;
    }
    virtual void setLabel(const std::string &s)
    {
        label = s;
        hasLabelFn = false;
    }
    std::string label;
    std::function<std::string()> labelFn;
    bool hasLabelFn{false};

    /*
     * The modulation interface has a collection of API points. Is the
     * slider currently modulated by either the active or any other modulator,
     * are we in mod edit mode, what's my value. The old API had gotten
     * rather confusing and unruly so lets re-state it here
     */
    enum ModulationState
    {
        UNMODULATED,
        MODULATED_BY_ACTIVE,
        MODULATED_BY_OTHER
    } modulationState{UNMODULATED};
    void setModulationState(ModulationState m) { modulationState = m; }
    void setModulationState(bool isModulated, bool isModulatedByActive)
    {
        if (isModulated && isModulatedByActive)
            modulationState = MODULATED_BY_ACTIVE;
        else if (isModulated)
            modulationState = MODULATED_BY_OTHER;
        else
            modulationState = UNMODULATED;
    }

    void setIsEditingModulation(bool b) { isEditingModulation = b; }
    bool isEditingModulation{false};

    void setIsModulationBipolar(bool b) { isModulationBipolar = b; }
    bool isModulationBipolar{false};

    virtual void setModValue(float v) { modValue = v; }
    virtual float getModValue() const { return modValue; }
    float modValue{0.f};

    /* Deactivation can occur by a function or by a value */
    virtual void setDeactivatedFn(std::function<bool()> f)
    {
        hasDeactivatedFn = true;
        isDeactivatedFn = f;
    }
    virtual void setDeactivated(bool b)
    {
        hasDeactivatedFn = false;
        isDeactivated = b;
    }
    virtual bool getDeactivated() const { return false; }
    std::function<bool()> isDeactivatedFn;
    bool isDeactivated{false}, hasDeactivatedFn{false};

    virtual void setTempoSync(bool b) { isTemposync = b; }
    bool isTemposync{false};

    /*
     * After an edit event we want to know if it was from a drag or wheel
     */
    enum EditTypeWas
    {
        NOEDIT,
        DRAG,
        WHEEL,
        DOUBLECLICK,
    } editTypeWas{NOEDIT};
    EditTypeWas getEditTypeWas() { return editTypeWas; }

    /*
     * The slider and knob sometimes has quantized motion for continuous values.
     * As such it has a quantized display value set on it which in normal case
     * is the same as the value, but in some cases is not (like when you control drag).
     */
    virtual void setQuantitizedDisplayValue(float f) { quantizedDisplayValue = f; }
    float quantizedDisplayValue{0.f};

    // Needed for scroll wheel behavior on stepped sliders
    bool isStepped{false};
    virtual void setIsStepped(bool s) { isStepped = s; }
    int intRange{0};
    virtual void setIntStepRange(int i) { intRange = i; }

    // Font handling. FIXME - implement this
    juce::Font font{juce::FontOptions()};
    virtual void setFont(juce::Font f) { font = f; }
    virtual void setFontStyle(int x) {}
    virtual void setTextAlign(juce::Justification x) {}
    virtual void setFontSize(int s) {}
    virtual void setTextHOffset(int x) {}
    virtual void setTextVOffset(int x) {}
};
} // namespace Widgets
} // namespace Surge

#endif // SURGE_XT_MODULATABLECONTROLINTERFACE_H
