#ifndef WDF_H_INCLUDED
#define WDF_H_INCLUDED

#include "omega.h"
#include <string>
#include <cmath>
#include <memory>
#include <type_traits>

namespace chowdsp
{

/**
 * A framework for creating circuit emulations with Wave Digital Filters.
 * For more technical information, see:
 * -
 * https://www.eit.lth.se/fileadmin/eit/courses/eit085f/Fettweis_Wave_Digital_Filters_Theory_and_Practice_IEEE_Proc_1986_-_This_is_a_real_challange.pdf
 * - https://searchworks.stanford.edu/view/11891203
 *
 * To start, initialize all your circuit elements and connections.
 * Be sure to pick a "root" node, and call `root.connectToNode (adaptor);`
 *
 * To run the simulation, call the following code
 * once per sample:
 * ```
 * // set source inputs here...
 *
 * root.incident (adaptor.reflected());
 * // if probing the root node, do that here...
 * adaptor.incident (root.reflected());
 *
 * // probe other elements here...
 * ```
 *
 * To probe a node, call `element.voltage()` or `element.current()`.
 */
namespace WDF
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
    virtual void incident(double x) noexcept = 0;

    /** Sub-classes override this function to propagate a reflected wave. */
    virtual double reflected() noexcept = 0;

    /** Probe the voltage across this circuit element. */
    inline double voltage() const noexcept { return (a + b) / 2.0; }

    /**Probe the current through this circuit element. */
    inline double current() const noexcept { return (a - b) / (2.0 * R); }

    // These classes need access to a,b
    friend class YParameter;
    friend class WDFParallel;
    friend class WDFSeries;

    template <typename Port1Type, typename Port2Type> friend class WDFParallelT;

    template <typename Port1Type, typename Port2Type> friend class WDFSeriesT;

    double R = 1.0e-9;  // impedance
    double G = 1.0 / R; // admittance

  protected:
    double a = 0.0; // incident wave
    double b = 0.0; // reflected wave

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
    Resistor(double value) : WDFNode("Resistor"), R_value(value) { calcImpedance(); }
    virtual ~Resistor() {}

    /** Sets the resistance value of the WDF resistor, in Ohms. */
    void setResistanceValue(double newR)
    {
        if (newR == R_value)
            return;

        R_value = newR;
        propagateImpedance();
    }

    /** Computes the impedance of the WDF resistor, Z_R = R. */
    inline void calcImpedance() override
    {
        R = R_value;
        G = 1.0 / R;
    }

    /** Accepts an incident wave into a WDF resistor. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistor. */
    inline double reflected() noexcept override
    {
        b = 0.0;
        return b;
    }

  private:
    double R_value = 1.0e-9;
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
    Capacitor(double value, double fs, double alpha = 1.0)
        : WDFNode("Capacitor"), C_value(value), fs(fs), alpha(alpha), b_coef((1.0 - alpha) / 2.0),
          a_coef((1.0 + alpha) / 2.0)
    {
        calcImpedance();
    }
    virtual ~Capacitor() {}

    /** Sets the capacitance value of the WDF capacitor, in Farads. */
    void setCapacitanceValue(double newC)
    {
        if (newC == C_value)
            return;

        C_value = newC;
        propagateImpedance();
    }

    /** Computes the impedance of the WDF capacitor,
     *                 1
     * Z_C = ---------------------
     *       (1 + alpha) * f_s * C
     */
    inline void calcImpedance() override
    {
        R = 1.0 / ((1.0 + alpha) * C_value * fs);
        G = 1.0 / R;
    }

    /** Accepts an incident wave into a WDF capacitor. */
    inline void incident(double x) noexcept override
    {
        a = x;
        z = a;
    }

    /** Propagates a reflected wave from a WDF capacitor. */
    inline double reflected() noexcept override
    {
        b = b_coef * b + a_coef * z;
        return b;
    }

  private:
    double C_value = 1.0e-6;
    double z = 0.0;

    const double fs;
    const double alpha;

    const double b_coef;
    const double a_coef;
};

/** WDF Inductor Node */
class Inductor : public WDFNode
{
  public:
    /** Creates a new WDF Inductor.
     * @param value: Inductance value in Farads
     * @param fs: WDF sample rate
     * @param alpha: alpha value to be used for the alpha transform,
     *               use 0 for Backwards Euler, use 1 for Bilinear Transform.
     */
    Inductor(double value, double fs, double alpha = 1.0)
        : WDFNode("Inductor"), L_value(value), fs(fs), alpha(alpha), b_coef((1.0 - alpha) / 2.0),
          a_coef((1.0 + alpha) / 2.0)
    {
        calcImpedance();
    }
    virtual ~Inductor() {}

    /** Sets the inductance value of the WDF capacitor, in Henries. */
    void setInductanceValue(double newL)
    {
        if (newL == L_value)
            return;

        L_value = newL;
        propagateImpedance();
    }

    /** Computes the impedance of the WDF capacitor,
     * Z_L = (1 + alpha) * f_s * L
     */
    inline void calcImpedance() override
    {
        R = (1.0 + alpha) * L_value * fs;
        G = 1.0 / R;
    }

    /** Accepts an incident wave into a WDF inductor. */
    inline void incident(double x) noexcept override
    {
        a = x;
        z = a;
    }

    /** Propagates a reflected wave from a WDF inductor. */
    inline double reflected() noexcept override
    {
        b = b_coef * b - a_coef * z;
        return b;
    }

  private:
    double L_value = 1.0e-6;
    double z = 0.0;

    const double fs;
    const double alpha;

    const double b_coef;
    const double a_coef;
};

/** WDF Switch (non-adaptable) */
class Switch : public WDFNode
{
  public:
    Switch() : WDFNode("Switch") {}
    virtual ~Switch() {}

    inline void calcImpedance() override {}

    /** Sets the state of the switch. */
    void setClosed(bool shouldClose) { closed = shouldClose; }

    /** Accepts an incident wave into a WDF switch. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF switch. */
    inline double reflected() noexcept override
    {
        b = closed ? -a : a;
        return b;
    }

  private:
    bool closed = true;
};

/** WDF Open circuit (non-adaptable) */
class Open : public WDFNode
{
  public:
    Open() : WDFNode("Open") {}
    virtual ~Open()
    {
        R = 1.0e15;
        G = 1.0 / R;
    }

    inline void calcImpedance() override {}

    /** Accepts an incident wave into a WDF open. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF open. */
    inline double reflected() noexcept override
    {
        b = a;
        return b;
    }
};

/** WDF Short circuit (non-adaptable) */
class Short : public WDFNode
{
  public:
    Short() : WDFNode("Short") {}
    virtual ~Short()
    {
        R = 1.0e-15;
        G = 1.0 / R;
    }

    inline void calcImpedance() override {}

    /** Accepts an incident wave into a WDF short. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF short. */
    inline double reflected() noexcept override
    {
        b = -a;
        return b;
    }
};

/** WDF Voltage Polarity Inverter */
class PolarityInverter : public WDFNode
{
  public:
    /** Creates a new WDF polarity inverter
     * @param port1: the port to connect to the inverter
     */
    PolarityInverter(WDFNode *port1) : WDFNode("Polarity Inverter"), port1(port1)
    {
        port1->connectToNode(this);
        calcImpedance();
    }
    virtual ~PolarityInverter() {}

    /** Calculates the impedance of the WDF inverter
     * (same impedance as the connected node).
     */
    inline void calcImpedance() override
    {
        R = port1->R;
        G = 1.0 / R;
    }

    /** Accepts an incident wave into a WDF inverter. */
    inline void incident(double x) noexcept override
    {
        a = x;
        port1->incident(-x);
    }

    /** Propagates a reflected wave from a WDF inverter. */
    inline double reflected() noexcept override
    {
        b = -port1->reflected();
        return b;
    }

  private:
    WDFNode *port1 = nullptr;
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
        G = 1.0 / R;
    }

    /** Accepts an incident wave into a WDF inverter. */
    inline void incident(double x) noexcept override
    {
        a = x;
        port1->incident(-x);
    }

    /** Propagates a reflected wave from a WDF inverter. */
    inline double reflected() noexcept override
    {
        b = -port1->reflected();
        return b;
    }

    std::unique_ptr<PortType> port1;
};

/** WDF y-parameter 2-port (short circuit admittance) */
class YParameter : public WDFNode
{
  public:
    YParameter(WDFNode *port1, double y11, double y12, double y21, double y22)
        : WDFNode("YParameter"), port1(port1)
    {
        y[0][0] = y11;
        y[0][1] = y12;
        y[1][0] = y21;
        y[1][1] = y22;

        port1->connectToNode(this);
        calcImpedance();
    }

    virtual ~YParameter() {}

    inline void calcImpedance() override
    {
        denominator = y[1][1] + port1->R * y[0][0] * y[1][1] - port1->R * y[0][1] * y[1][0];
        R = (port1->R * y[0][0] + 1.0) / denominator;
        G = 1.0 / R;

        double rSq = port1->R * port1->R;
        double num1A = -y[1][1] * rSq * y[0][0] * y[0][0];
        double num2A = y[0][1] * y[1][0] * rSq * y[0][0];

        A = (num1A + num2A + y[1][1]) / (denominator * (port1->R * y[0][0] + 1.0));
        B = -port1->R * y[0][1] / (port1->R * y[0][0] + 1.0);
        C = -y[1][0] / denominator;
    }

    inline void incident(double x) noexcept override
    {
        a = x;
        port1->incident(A * port1->b + B * x);
    }

    inline double reflected() noexcept override
    {
        b = C * port1->reflected();
        return b;
    }

  private:
    WDFNode *port1;
    double y[2][2] = {{0.0, 0.0}, {0.0, 0.0}};

    double denominator = 1.0;
    double A = 1.0f;
    double B = 1.0f;
    double C = 1.0f;
};

/** WDF 3-port adapter base class */
class WDFAdaptor : public WDFNode
{
  public:
    WDFAdaptor(WDFNode *port1, WDFNode *port2, std::string type)
        : WDFNode(type), port1(port1), port2(port2)
    {
        port1->connectToNode(this);
        port2->connectToNode(this);
    }
    virtual ~WDFAdaptor() {}

  protected:
    WDFNode *port1 = nullptr;
    WDFNode *port2 = nullptr;
};

/** WDF 3-port parallel adaptor */
class WDFParallel : public WDFAdaptor
{
  public:
    /** Creates a new WDF parallel adaptor from two connected ports. */
    WDFParallel(WDFNode *port1, WDFNode *port2) : WDFAdaptor(port1, port2, "Parallel")
    {
        calcImpedance();
    }
    virtual ~WDFParallel() {}

    /** Computes the impedance for a WDF parallel adaptor.
     *  1     1     1
     * --- = --- + ---
     * Z_p   Z_1   Z_2
     */
    inline void calcImpedance() override
    {
        G = port1->G + port2->G;
        R = 1.0 / G;
        port1Reflect = port1->G / G;
        port2Reflect = port2->G / G;
    }

    /** Accepts an incident wave into a WDF parallel adaptor. */
    inline void incident(double x) noexcept override
    {
        port1->incident(x + (port2->b - port1->b) * port2Reflect);
        port2->incident(x + (port2->b - port1->b) * -port1Reflect);
        a = x;
    }

    /** Propagates a reflected wave from a WDF parallel adaptor. */
    inline double reflected() noexcept override
    {
        b = port1Reflect * port1->reflected() + port2Reflect * port2->reflected();
        return b;
    }

  private:
    double port1Reflect = 1.0;
    double port2Reflect = 1.0;
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
        G = port1->G + port2->G;
        R = 1.0 / G;
        port1Reflect = port1->G / G;
        port2Reflect = port2->G / G;
    }

    /** Accepts an incident wave into a WDF parallel adaptor. */
    inline void incident(double x) noexcept override
    {
        port1->incident(x + (port2->b - port1->b) * port2Reflect);
        port2->incident(x + (port2->b - port1->b) * -port1Reflect);
        a = x;
    }

    /** Propagates a reflected wave from a WDF parallel adaptor. */
    inline double reflected() noexcept override
    {
        b = port1Reflect * port1->reflected() + port2Reflect * port2->reflected();
        return b;
    }

    std::unique_ptr<Port1Type> port1;
    std::unique_ptr<Port2Type> port2;

  private:
    double port1Reflect = 1.0;
    double port2Reflect = 1.0;
};

/** WDF 3-port series adaptor */
class WDFSeries : public WDFAdaptor
{
  public:
    /** Creates a new WDF series adaptor from two connected ports. */
    WDFSeries(WDFNode *port1, WDFNode *port2) : WDFAdaptor(port1, port2, "Series")
    {
        calcImpedance();
    }
    virtual ~WDFSeries() {}

    /** Computes the impedance for a WDF parallel adaptor.
     * Z_s = Z_1 + Z_2
     */
    inline void calcImpedance() override
    {
        R = port1->R + port2->R;
        G = 1.0 / R;
        port1Reflect = port1->R / R;
        port2Reflect = port2->R / R;
    }

    /** Accepts an incident wave into a WDF series adaptor. */
    inline void incident(double x) noexcept override
    {
        port1->incident(port1->b - port1Reflect * (x + port1->b + port2->b));
        port2->incident(port2->b - port2Reflect * (x + port1->b + port2->b));

        a = x;
    }

    /** Propagates a reflected wave from a WDF series adaptor. */
    inline double reflected() noexcept override
    {
        b = -(port1->reflected() + port2->reflected());
        return b;
    }

  private:
    double port1Reflect = 1.0;
    double port2Reflect = 1.0;
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
        R = port1->R + port2->R;
        G = 1.0 / R;
        port1Reflect = port1->R / R;
        port2Reflect = port2->R / R;
    }

    /** Accepts an incident wave into a WDF series adaptor. */
    inline void incident(double x) noexcept override
    {
        port1->incident(port1->b - port1Reflect * (x + port1->b + port2->b));
        port2->incident(port2->b - port2Reflect * (x + port1->b + port2->b));

        a = x;
    }

    /** Propagates a reflected wave from a WDF series adaptor. */
    inline double reflected() noexcept override
    {
        b = -(port1->reflected() + port2->reflected());
        return b;
    }

    std::unique_ptr<Port1Type> port1;
    std::unique_ptr<Port2Type> port2;

  private:
    double port1Reflect = 1.0;
    double port2Reflect = 1.0;
};

/** WDF Voltage source with series resistance */
class ResistiveVoltageSource : public WDFNode
{
  public:
    /** Creates a new resistive voltage source.
     * @param value: initial resistance value, in Ohms
     */
    ResistiveVoltageSource(double value = 1.0e-9) : WDFNode("Resistive Voltage"), R_value(value)
    {
        calcImpedance();
    }
    virtual ~ResistiveVoltageSource() {}

    /** Sets the resistance value of the series resistor, in Ohms. */
    void setResistanceValue(double newR)
    {
        if (newR == R_value)
            return;

        R_value = newR;
        propagateImpedance();
    }

    /** Computes the impedance for a WDF resistive voltage source
     * Z_Vr = Z_R
     */
    inline void calcImpedance() override
    {
        R = R_value;
        G = 1.0 / R;
    }

    /** Sets the voltage of the voltage source, in Volts */
    void setVoltage(double newV) { Vs = newV; }

    /** Accepts an incident wave into a WDF resistive voltage source. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistive voltage source. */
    inline double reflected() noexcept override
    {
        b = Vs;
        return b;
    }

  private:
    double Vs = 0.0;
    double R_value = 1.0e-9;
};

/** WDF Ideal Voltage source (non-adaptable) */
class IdealVoltageSource : public WDFNode
{
  public:
    IdealVoltageSource() : WDFNode("IdealVoltage") { calcImpedance(); }
    virtual ~IdealVoltageSource() {}

    inline void calcImpedance() override {}

    /** Sets the voltage of the voltage source, in Volts */
    void setVoltage(double newV) { Vs = newV; }

    /** Accepts an incident wave into a WDF ideal voltage source. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF ideal voltage source. */
    inline double reflected() noexcept override
    {
        b = -a + 2.0 * Vs;
        return b;
    }

  private:
    double Vs = 0.0;
};

/** WDF Current source with parallel resistance */
class ResistiveCurrentSource : public WDFNode
{
  public:
    /** Creates a new resistive current source.
     * @param value: initial resistance value, in Ohms
     */
    ResistiveCurrentSource(double value = 1.0e9) : WDFNode("Resistive Current"), R_value(value)
    {
        calcImpedance();
    }
    virtual ~ResistiveCurrentSource() {}

    /** Sets the resistance value of the parallel resistor, in Ohms. */
    void setResistanceValue(double newR)
    {
        if (newR == R_value)
            return;

        R_value = newR;
        propagateImpedance();
    }

    /** Computes the impedance for a WDF resistive current source
     * Z_Ir = Z_R
     */
    inline void calcImpedance() override
    {
        R = R_value;
        G = 1.0 / R;
    }

    /** Sets the current of the current source, in Amps */
    void setCurrent(double newI) { Is = newI; }

    /** Accepts an incident wave into a WDF resistive current source. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF resistive current source. */
    inline double reflected() noexcept override
    {
        b = 2 * R * Is;
        return b;
    }

  private:
    double Is = 0.0;
    double R_value = 1.0e9;
};

/** WDF Current source (non-adpatable) */
class IdealCurrentSource : public WDFNode
{
  public:
    IdealCurrentSource() : WDFNode("Ideal Current") { calcImpedance(); }
    virtual ~IdealCurrentSource() {}

    inline void calcImpedance() override {}

    /** Sets the current of the current source, in Amps */
    void setCurrent(double newI) { Is = newI; }

    /** Accepts an incident wave into a WDF ideal current source. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF ideal current source. */
    inline double reflected() noexcept override
    {
        b = 2 * next->R * Is + a;
        return b;
    }

  private:
    double Is = 0.0;
};

/** Signum function to determine the sign of the input. */
template <typename T> inline int signum(T val) { return (T(0) < val) - (val < T(0)); }

/** WDF diode pair (non-adaptable)
 * See Werner et al., "An Improved and Generalized Diode Clipper Model for Wave Digital Filters"
 * https://www.researchgate.net/publication/299514713_An_Improved_and_Generalized_Diode_Clipper_Model_for_Wave_Digital_Filters
 */
class DiodePair : public WDFNode
{
  public:
    /** Creates a new WDF diode pair, with the given diode specifications.
     * @param Is: reverse saturation current
     * @param Vt: thermal voltage
     */
    DiodePair(double Is, double Vt) : WDFNode("DiodePair"), Is(Is), Vt(Vt) {}

    virtual ~DiodePair() {}

    inline void calcImpedance() override {}

    /** Accepts an incident wave into a WDF diode pair. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF diode pair. */
    inline double reflected() noexcept override
    {
        // See eqn (18) from reference paper
        double lambda = (double)signum(a);
        b = a + 2 * lambda *
                    (next->R * Is - Vt * Omega::omega4(std::log(next->R * Is / Vt) +
                                                       (lambda * a + next->R * Is) / Vt));
        return b;
    }

  private:
    const double Is; // reverse saturation current
    const double Vt; // thermal voltage
};

/** WDF diode (non-adaptable)
 * See Werner et al., "An Improved and Generalized Diode Clipper Model for Wave Digital Filters"
 * https://www.researchgate.net/publication/299514713_An_Improved_and_Generalized_Diode_Clipper_Model_for_Wave_Digital_Filters
 */
class Diode : public WDFNode
{
  public:
    /** Creates a new WDF diode, with the given diode specifications.
     * @param Is: reverse saturation current
     * @param Vt: thermal voltage
     */
    Diode(double Is, double Vt) : WDFNode("Diode"), Is(Is), Vt(Vt) {}

    virtual ~Diode() {}

    inline void calcImpedance() override {}

    /** Accepts an incident wave into a WDF diode. */
    inline void incident(double x) noexcept override { a = x; }

    /** Propagates a reflected wave from a WDF diode. */
    inline double reflected() noexcept override
    {
        // See eqn (10) from reference paper
        b = a + 2 * next->R * Is -
            2 * Vt * Omega::omega4(std::log(next->R * Is / Vt) + (a + next->R * Is) / Vt);
        return b;
    }

  private:
    const double Is; // reverse saturation current
    const double Vt; // thermal voltage
};

} // namespace WDF

} // namespace chowdsp

#endif // WDF_H_INCLUDED
