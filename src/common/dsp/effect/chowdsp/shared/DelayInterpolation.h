#pragma once

#include <type_traits>

namespace chowdsp
{

/**
    A collection of structs to pass as the template argument when setting the
    interpolation type for the DelayLine class.
*/
namespace DelayLineInterpolationTypes
{
/**
    No interpolation between successive samples in the delay line will be
    performed. This is useful when the delay is a constant integer or to
    create lo-fi audio effects.
*/
struct None
{
    void reset(int newTotalSize) { totalSize = newTotalSize; }

    template <typename T> void updateInternalVariables(int & /*delayIntOffset*/, T & /*delayFrac*/)
    {
    }

    template <typename T>
    inline T call(const T *buffer, int delayInt, T /*delayFrac*/, const T & /*state*/)
    {
        return buffer[delayInt % totalSize];
    }

    int totalSize;
};

/**
    Successive samples in the delay line will be linearly interpolated. This
    type of interpolation has a low compuational cost where the delay can be
    modulated in real time, but it also introduces a low-pass filtering effect
    into your audio signal.
*/
struct Linear
{
    void reset(int newTotalSize) { totalSize = newTotalSize; }

    template <typename T> void updateInternalVariables(int & /*delayIntOffset*/, T & /*delayFrac*/)
    {
    }

    template <typename T>
    inline T call(const T *buffer, int delayInt, T delayFrac, const T & /*state*/)
    {
        auto index1 = delayInt;
        auto index2 = index1 + 1;

        if (index2 >= totalSize)
        {
            index1 %= totalSize;
            index2 %= totalSize;
        }

        auto value1 = buffer[index1];
        auto value2 = buffer[index2];

        return value1 + delayFrac * (value2 - value1);
    }

    int totalSize;
};

/**
    Successive samples in the delay line will be interpolated using a 3rd order
    Lagrange interpolator. This method incurs more computational overhead than
    linear interpolation but reduces the low-pass filtering effect whilst
    remaining amenable to real time delay modulation.
*/
struct Lagrange3rd
{
    void reset(int newTotalSize) { totalSize = newTotalSize; }

    template <typename T> void updateInternalVariables(int &delayIntOffset, T &delayFrac)
    {
        if (delayIntOffset >= 1)
        {
            delayFrac++;
            delayIntOffset--;
        }
    }

    template <typename T>
    inline T call(const T *buffer, int delayInt, T delayFrac, const T & /*state*/)
    {
        auto index1 = delayInt;
        auto index2 = index1 + 1;
        auto index3 = index2 + 1;
        auto index4 = index3 + 1;

        if (index4 >= totalSize)
        {
            index1 %= totalSize;
            index2 %= totalSize;
            index3 %= totalSize;
            index4 %= totalSize;
        }

        auto value1 = buffer[index1];
        auto value2 = buffer[index2];
        auto value3 = buffer[index3];
        auto value4 = buffer[index4];

        auto d1 = delayFrac - (T)1.0;
        auto d2 = delayFrac - (T)2.0;
        auto d3 = delayFrac - (T)3.0;

        auto c1 = -d1 * d2 * d3 / (T)6.0;
        auto c2 = d2 * d3 * (T)0.5;
        auto c3 = -d1 * d3 * (T)0.5;
        auto c4 = d1 * d2 / (T)6.0;

        return value1 * c1 + delayFrac * (value2 * c2 + value3 * c3 + value4 * c4);
    }

    int totalSize;
};

/**
    Successive samples in the delay line will be interpolated using a 5th order
    Lagrange interpolator. This method incurs more computational overhead than
    linear interpolation.
*/
struct Lagrange5th
{
    void reset(int newTotalSize) { totalSize = newTotalSize; }

    template <typename T> void updateInternalVariables(int &delayIntOffset, T &delayFrac)
    {
        if (delayIntOffset >= 2)
        {
            delayFrac += (T)2;
            delayIntOffset -= 2;
        }
    }

    template <typename T>
    inline T call(const T *buffer, int delayInt, T delayFrac, const T & /*state*/)
    {
        auto index1 = delayInt;
        auto index2 = index1 + 1;
        auto index3 = index2 + 1;
        auto index4 = index3 + 1;
        auto index5 = index4 + 1;
        auto index6 = index5 + 1;

        if (index6 >= totalSize)
        {
            index1 %= totalSize;
            index2 %= totalSize;
            index3 %= totalSize;
            index4 %= totalSize;
            index5 %= totalSize;
            index6 %= totalSize;
        }

        auto value1 = buffer[index1];
        auto value2 = buffer[index2];
        auto value3 = buffer[index3];
        auto value4 = buffer[index4];
        auto value5 = buffer[index5];
        auto value6 = buffer[index6];

        auto d1 = delayFrac - (T)1.0;
        auto d2 = delayFrac - (T)2.0;
        auto d3 = delayFrac - (T)3.0;
        auto d4 = delayFrac - (T)4.0;
        auto d5 = delayFrac - (T)5.0;

        auto c1 = -d1 * d2 * d3 * d4 * d5 / (T)120.0;
        auto c2 = d2 * d3 * d4 * d5 / (T)24.0;
        auto c3 = -d1 * d3 * d4 * d5 / (T)12.0;
        auto c4 = d1 * d2 * d4 * d5 / (T)12.0;
        auto c5 = -d1 * d2 * d3 * d5 / (T)24.0;
        auto c6 = d1 * d2 * d3 * d4 / (T)120.0;

        return value1 * c1 +
               delayFrac * (value2 * c2 + value3 * c3 + value4 * c4 + value5 * c5 + value6 * c6);
    }

    int totalSize;
};

/**
    Successive samples in the delay line will be interpolated using 1st order
    Thiran interpolation. This method is very efficient, and features a flat
    amplitude frequency response in exchange for less accuracy in the phase
    response. This interpolation method is stateful so is unsuitable for
    applications requiring fast delay modulation.
*/
struct Thiran
{
    void reset(int newTotalSize) { totalSize = newTotalSize; }

    template <typename T> void updateInternalVariables(int &delayIntOffset, T &delayFrac)
    {
        if (delayFrac < (T)0.618 && delayIntOffset >= 1)
        {
            delayFrac++;
            delayIntOffset--;
        }

        alpha = double((1 - delayFrac) / (1 + delayFrac));
    }

    template <typename T> inline T call(const T *buffer, int delayInt, T delayFrac, T &state)
    {
        auto index1 = delayInt;
        auto index2 = index1 + 1;

        if (index2 >= totalSize)
        {
            index1 %= totalSize;
            index2 %= totalSize;
        }

        auto value1 = buffer[index1];
        auto value2 = buffer[index2];

        auto output = delayFrac == 0 ? value1 : value2 + (T)alpha * (value1 - state);
        state = output;

        return output;
    }

    int totalSize;
    double alpha = 0.0;
};
} // namespace DelayLineInterpolationTypes

} // namespace chowdsp
