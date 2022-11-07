# Dynamic Names, Deactivation and Bipolarity

In Surge 1.9, we have finally (finally!) implemented dynamic names, bipolarity, and activation
status for sliders. This allows you to do things like rename on value changes etc. at
draw time without having to trigger an entire UI rebuild. The mechanism is made slightly
complicated by the fact that Parameter can't have any complex object members (so you can't have
a `std::function` as a member on Parameter). So here's how it works!

## General Stuff

1. You end up writing little static function objects with functions to determine
   names and bools and stuff
2. Those functions will be called a lot at render time. Make them snappy.
3. Those functions belong to objects managed outside the lifecycle of Parameter.
   Probably a pointer to a static is your best bet.

## Dynamic Names

The name of a parameter is what the slider shows. If your parameter requires a dynamic
name, you need to do the following:

1. In `Parameter.cpp`, add the control type to `Parameter::supportsDynamicName`
2. When you set up your parameter, but *after* you set the control type, set the
   `dynamicName` object to a pointer to a class of type `ParameterDynamicName`. The
   Parameter does *not* take ownership of the pointer, so the common pattern is
   to make a little static stateless class with the stateless name operator and
   assign a pointer to that to the object.
3. Usually, a menu changes the nature of the names requiring a redraw (not a rebuild).
   To make a parameter force a redraw on value change, add it to the list in
   `Parameter::is_nonlocal_on_change`.

A few worked examples are in the codebase. `EuroTwist.cpp` sets up a class
which has dynamic names based on the engine type. It shows how you find the
associated oscillator using control groups, use that to look up oscillator mode,
understand your parameter index, and return the right name.

Similarly, the `lfoPhaseName` handler which is set up for all LFOs in `SurgePatch.cpp`
creates a static handler and assigns it to all the `start_phase` dynamic
names to swap Phase and Shuffle text labels based on LFO type.

## Dynamic Activation

With Surge 1.7 we've introduced deactivated sliders, and with Surge 1.9 we can cluster them.
Clusters come in two forms:

1. "Follow deactivation" clusters. If parameter A is deactivated, so is parameter B.
   A good example is the LPG Response/LPG Decay treatment in `EuroTwist.cpp`.
   In this case you want a menu to activate the item.
2. "Follow value" clusters. Based on some other value, deactivate this slider. In this case
   you don't want a menu, since the menu is the value driver.

### Follow Deactivation Pattern

In this pattern we have a 'primary' parameter. This is the parameter which holds the
deactivation status, which the DSP engine asks `p.deactivated`, etc.... The control type
participates in `can_deactivate` and this is unchanged.

To cluster other params with this one, you implement and set on the
`ParameterDynamicDeactivationFunction` which contains two methods,
`getValue()` which returns true/false for deactivated, and
`getPrimaryActivationDriver` which returns a pointer to the deactivation parent,
or null if there is no parent.

So basically, a good implementation of `getValue()` is to look up the parent param and
return its decctivation status, and for `getPrimaryActivationDriver()` to look up the
parent parameter and return a pointer to it. LPG parameters in EuroTwist do precisely this.

### Follow Value Pattern

In this pattern you only need to implement `getValue()` based on your function.
The LFO does this with Rate and Phase for Envelope LFO type, as shown again in
`SurgePatch.cpp`. In this case, do not advertise a leader param, since there isn't one
(or at least, if there is one, you don't toggle it by setting `deactivated`).

## Dynamic Bipolarity

Dynamic bipolarity is much more complicated than name or activation, since it
changes not just the UI element, but also the text formatting of the value.
As such, dynamic bipolarity in 1.9 only works with percent-based units, and they
have a specific type. We may expand on this in the future.

The core decision we made with dynamic bipolarity is that we don't change the
underlying value, but rather make a display-only change. So a dynamic bipolar
value has the internal `val.f` always bound in +/- 1 range.
Your DSP needs to assume that and clamp, model, etc... accordingly.

As of this writing, dynamic bipolarity is also only available for
`ct_percent_bipolar_w_dynamic_unipolar_formatting` control type.

To use dynamic bipolarity, make your parameter of the above type and then
set the `dynamicBipolar` member to a reference to a non-owned object.
That implements a `ParameterDynamicBool` and its `getValue()` member which,
if it returns false, will *format* the parameter as unipolar (so bind
strings and modulations in 0%...100% range, instead of -100%...100% range) and swap the slider
background, even though the underlying values will still be -1...1.

A worked example of this is in `EuroTwist.cpp` and is implemented with
the special flag `kScaleBasedOnIsBiPolar` on `displayinfo`.

