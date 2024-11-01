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
#ifndef SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_WDF_SSE_H
#define SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_WDF_SSE_H

#include <string>
#include <cmath>
#include <memory>
#include <type_traits>
#include "sst/basic-blocks/dsp/FastMath.h"
#include "portable_intrinsics.h"

namespace chowdsp
{

/**
 * Some SIMD versions of the WDF classes in wdf.h
 */
namespace WDF_SSE
{

namespace detail
{
template <typename T>
typename std::enable_if<std::is_default_constructible<T>::value, void>::type
default_construct(std::unique_ptr<T> &ptr)
{
    ptr = std::make_unique<T>();
}

template <typename T>
typename std::enable_if<!std::is_default_constructible<T>::value, void>::type
default_construct(std::unique_ptr<T> &ptr)
{
    ptr.reset(nullptr);
}
} // namespace detail

/** Wave digital filter base class */
class WDF
{
  public:
    WDF(std::string type) : type(type) {}
    virtual ~WDF() {}

    /** Sub-classes override this function to recompute
     * the impedance of this element.
     */
    virtual void calcImpedance() = 0;

    /** Sub-classes override this function to propagate
     * an impedance change to the upstream elements in
     * the WDF tree.
     */
    virtual void propagateImpedance() = 0;

    /** Sub-classes override this function to accept an incident wave. */
    virtual void incident(SIMD_M128 x) noexcept = 0;

    /** Sub-classes override this function to propagate a reflected wave. */
    virtual SIMD_M128 reflected() noexcept = 0;

    /** Probe the voltage across this circuit element. */
    inline SIMD_M128 voltage() const noexcept { return vMul(vAdd(a, b), vLoad1(0.5f)); }

    /**Probe the current through this circuit element. */
    inline SIMD_M128 current() const noexcept { return vMul(vSub(a, b), vMul(vLoad1(0.5f), G)); }

    // These classes need access to a,b
    friend class YParameter;
    friend class WDFParallel;
    friend class WDFSeries;

    template <typename Port1Type, typename Port2Type> friend class WDFParallelT;

    template <typename Port1Type, typename Port2Type> friend class WDFSeriesT;

    SIMD_M128 R; // impedance
    SIMD_M128 G; // admittance

  protected:
    SIMD_M128 a = vZero; // incident wave
    SIMD_M128 b = vZero; // reflected wave

  private:
    const std::string type;
};

/** WDF node base class */
class WDFNode : public WDF
{
  public:
    WDFNode(std::string type) : WDF(type) {}
    virtual ~WDFNode() {}

    /** Connects this WDF node to an upstream node in the WDF tree. */
    void connectToNode(WDF *node) { next = node; }

    /** When this function is called from a downstream
     * element in the WDF tree, the impedance is recomputed
     * and then propagated upstream to the next element in the
     * WDF tree.
     */
    inline void propagateImpedance() override
    {
        calcImpedance();

        if (next != nullptr)
            next->propagateImpedance();
    }

  protected:
    WDF *next = nullptr;
};

/** WDF Resistor Node */
class Resistor : public WDFNode
{
  public:
    /** Creates a new WDF Resistor with a given resistance.
     * @param value: resistance in Ohms
     */
    Resistor(float value) : WDFNode("Resistor"), R_value(vLoad1(value)) { calcImpedance(); }
    virtual ~Resistor() {}

    // @TODO
    /** Sets the resistance value of the WDF resistor, in Ohms.
    // void setResistanceValue(float newR)
    // {
    //     if (newR == R_value)
    //         return;

    //     R_value = newR;
    //     propagateImpedance();
    // }*/

    /** Computes the impedance of the WDF resistor, Z_R = R. */
    inline void calcImpedance() override
    {
        R = R_value;
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
    }

    /** Accepts an incident wave into a WDF resistor. */
    inline void incident(SIMD_M128 x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistor. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vLoad1(0.0f);
        return b;
    }

  private:
    SIMD_M128 R_value = vLoad1(1.0e-9f);
};

/** WDF Capacitor Node */
class Capacitor : public WDFNode
{
  public:
    /** Creates a new WDF Capacitor.
     * @param value: Capacitance value in Farads
     * @param fs: WDF sample rate
     * @param alpha: alpha value to be used for the alpha transform,
     *               use 0 for Backwards Euler, use 1 for Bilinear Transform.
     */
    Capacitor(float value, float fs, float alpha = 1.0f)
        : WDFNode("Capacitor"), C_value(vLoad1(value)), fs(vLoad1(fs)), alpha(vLoad1(alpha)),
          b_coef(vLoad1((1.0f - alpha) / 2.0f)), a_coef(vLoad1((1.0f + alpha) / 2.0f))
    {
        calcImpedance();
    }
    virtual ~Capacitor() {}

    // @TODO
    /** Sets the capacitance value of the WDF capacitor, in Farads.
    void setCapacitanceValue(double newC)
    {
        if (newC == C_value)
            return;

        C_value = newC;
        propagateImpedance();
    } */

    /** Computes the impedance of the WDF capacitor,
     *                 1
     * Z_C = ---------------------
     *       (1 + alpha) * f_s * C
     */
    inline void calcImpedance() override
    {
        R = SIMD_MM(div_ps)(vLoad1(1.0f), vMul(vMul(vAdd(vLoad1(1.0f), alpha), C_value), fs));
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
    }

    /** Accepts an incident wave into a WDF capacitor. */
    inline void incident(SIMD_M128 x) noexcept override
    {
        a = x;
        z = a;
    }

    /** Propagates a reflected wave from a WDF capacitor. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vAdd(vMul(b_coef, b), vMul(a_coef, z));
        return b;
    }

  private:
    SIMD_M128 C_value;
    SIMD_M128 z = vZero;

    const SIMD_M128 fs;
    const SIMD_M128 alpha;

    const SIMD_M128 b_coef;
    const SIMD_M128 a_coef;
};

/** WDF Voltage Polarity Inverter */
template <typename PortType> class PolarityInverterT : public WDFNode
{
  public:
    /** Creates a new WDF polarity inverter */
    PolarityInverterT() : WDFNode("Polarity Inverter") { detail::default_construct(port1); }
    virtual ~PolarityInverterT() {}

    void initialise()
    {
        port1->connectToNode(this);
        calcImpedance();
    }

    /** Calculates the impedance of the WDF inverter
     * (same impedance as the connected node).
     */
    inline void calcImpedance() override
    {
        R = port1->R;
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
    }

    /** Accepts an incident wave into a WDF inverter. */
    inline void incident(SIMD_M128 x) noexcept override
    {
        a = x;
        port1->incident(vNeg(x));
    }

    /** Propagates a reflected wave from a WDF inverter. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vNeg(port1->reflected());
        return b;
    }

    std::unique_ptr<PortType> port1;
};

/** WDF 3-port parallel adaptor */
template <typename Port1Type, typename Port2Type> class WDFParallelT : public WDFNode
{
  public:
    /** Creates a new WDF parallel adaptor from two connected ports. */
    WDFParallelT() : WDFNode("Parallel")
    {
        detail::default_construct(port1);
        detail::default_construct(port2);
    }
    virtual ~WDFParallelT() {}

    void initialise()
    {
        port1->connectToNode(this);
        port2->connectToNode(this);
        calcImpedance();
    }

    /** Computes the impedance for a WDF parallel adaptor.
     *  1     1     1
     * --- = --- + ---
     * Z_p   Z_1   Z_2
     */
    inline void calcImpedance() override
    {
        G = vAdd(port1->G, port2->G);
        R = SIMD_MM(div_ps)(vLoad1(1.0f), G);
        port1Reflect = SIMD_MM(div_ps)(port1->G, G);
        port2Reflect = SIMD_MM(div_ps)(port2->G, G);
    }

    /** Accepts an incident wave into a WDF parallel adaptor. */
    inline void incident(SIMD_M128 x) noexcept override
    {
        port1->incident(vAdd(x, vMul(vSub(port2->b, port1->b), port2Reflect)));
        port2->incident(vAdd(x, vMul(vSub(port2->b, port1->b), vNeg(port1Reflect))));
        a = x;
    }

    /** Propagates a reflected wave from a WDF parallel adaptor. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vAdd(vMul(port1Reflect, port1->reflected()), vMul(port2Reflect, port2->reflected()));
        return b;
    }

    std::unique_ptr<Port1Type> port1;
    std::unique_ptr<Port2Type> port2;

  private:
    SIMD_M128 port1Reflect;
    SIMD_M128 port2Reflect;
};

/** WDF 3-port series adaptor */
template <typename Port1Type, typename Port2Type> class WDFSeriesT : public WDFNode
{
  public:
    /** Creates a new WDF series adaptor from two connected ports. */
    WDFSeriesT() : WDFNode("Series")
    {
        detail::default_construct(port1);
        detail::default_construct(port2);
    }
    virtual ~WDFSeriesT() {}

    void initialise()
    {
        port1->connectToNode(this);
        port2->connectToNode(this);
        calcImpedance();
    }

    /** Computes the impedance for a WDF parallel adaptor.
     * Z_s = Z_1 + Z_2
     */
    inline void calcImpedance() override
    {
        R = vAdd(port1->R, port2->R);
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
        port1Reflect = SIMD_MM(div_ps)(port1->R, R);
        port2Reflect = SIMD_MM(div_ps)(port2->R, R);
    }

    /** Accepts an incident wave into a WDF series adaptor. */
    inline void incident(SIMD_M128 x) noexcept override
    {
        port1->incident(vSub(port1->b, vMul(port1Reflect, vAdd(x, vAdd(port1->b, port2->b)))));
        port2->incident(vSub(port2->b, vMul(port2Reflect, vAdd(x, vAdd(port1->b, port2->b)))));

        a = x;
    }

    /** Propagates a reflected wave from a WDF series adaptor. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vNeg(vAdd(port1->reflected(), port2->reflected()));
        return b;
    }

    std::unique_ptr<Port1Type> port1;
    std::unique_ptr<Port2Type> port2;

  private:
    SIMD_M128 port1Reflect;
    SIMD_M128 port2Reflect;
};

/** WDF Voltage source with series resistance */
class ResistiveVoltageSource : public WDFNode
{
  public:
    /** Creates a new resistive voltage source.
     * @param value: initial resistance value, in Ohms
     */
    ResistiveVoltageSource(float value = 1.0e-9f)
        : WDFNode("Resistive Voltage"), R_value(vLoad1(value))
    {
        calcImpedance();
    }
    virtual ~ResistiveVoltageSource() {}

    // @TODO
    /** Sets the resistance value of the series resistor, in Ohms.
    void setResistanceValue(double newR)
    {
        if (newR == R_value)
            return;

        R_value = newR;
        propagateImpedance();
    } */

    /** Computes the impedance for a WDF resistive voltage source
     * Z_Vr = Z_R
     */
    inline void calcImpedance() override
    {
        R = R_value;
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
    }

    /** Sets the voltage of the voltage source, in Volts */
    void setVoltage(SIMD_M128 newV) { Vs = newV; }

    /** Accepts an incident wave into a WDF resistive voltage source. */
    inline void incident(SIMD_M128 x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistive voltage source. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = Vs;
        return b;
    }

  private:
    SIMD_M128 Vs;
    SIMD_M128 R_value;
};

/** WDF Current source with parallel resistance */
class ResistiveCurrentSource : public WDFNode
{
  public:
    /** Creates a new resistive current source.
     * @param value: initial resistance value, in Ohms
     */
    ResistiveCurrentSource(float value = 1.0e9f)
        : WDFNode("Resistive Current"), R_value(vLoad1(value))
    {
        calcImpedance();
    }
    virtual ~ResistiveCurrentSource() {}

    // @TODO
    /** Sets the resistance value of the parallel resistor, in Ohms.
    void setResistanceValue(double newR)
    {
        if (newR == R_value)
            return;

        R_value = newR;
        propagateImpedance();
    } */

    /** Computes the impedance for a WDF resistive current source
     * Z_Ir = Z_R
     */
    inline void calcImpedance() override
    {
        R = R_value;
        G = SIMD_MM(div_ps)(vLoad1(1.0f), R);
    }

    /** Sets the current of the current source, in Amps */
    void setCurrent(SIMD_M128 newI) { Is = newI; }

    /** Accepts an incident wave into a WDF resistive current source. */
    inline void incident(SIMD_M128 x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistive current source. */
    inline SIMD_M128 reflected() noexcept override
    {
        b = vMul(vLoad1(2.0f), vMul(R, Is));
        return b;
    }

  private:
    SIMD_M128 Is;
    SIMD_M128 R_value;
};

} // namespace WDF_SSE

} // namespace chowdsp

#endif // SURGE_SRC_COMMON_DSP_EFFECTS_CHOWDSP_SHARED_WDF_SSE_H
