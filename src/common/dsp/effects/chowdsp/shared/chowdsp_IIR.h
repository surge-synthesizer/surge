#pragma once

#include <algorithm>
#include <type_traits>

namespace chowdsp
{

/** IIR filter of arbitrary order.
 * Uses Transposed Direct Form II:
 * https://ccrma.stanford.edu/~jos/fp/Transposed_Direct_Forms.html
 */
template <size_t order, typename FloatType = float> class IIRFilter
{
  public:
    IIRFilter() = default;

    /** Reset filter state */
    virtual void reset() { std::fill(z, &z[order + 1], 0.0f); }

    /** Optimized processing call for first-order filter */
    template <int N = order>
    inline typename std::enable_if<N == 1, FloatType>::type processSample(FloatType x) noexcept
    {
        FloatType y = z[1] + x * b[0];
        z[order] = x * b[order] - y * a[order];
        return y;
    }

    /** Optimized processing call for second-order filter */
    template <int N = order>
    inline typename std::enable_if<N == 2, FloatType>::type processSample(FloatType x) noexcept
    {
        FloatType y = z[1] + x * b[0];
        z[1] = z[2] + x * b[1] - y * a[1];
        z[order] = x * b[order] - y * a[order];
        return y;
    }

    /** Generalized processing call for Nth-order filter */
    template <int N = order>
    inline typename std::enable_if<(N > 2), FloatType>::type processSample(FloatType x) noexcept
    {
        FloatType y = z[1] + x * b[0];

        for (int i = 1; i < order; ++i)
            z[i] = z[i + 1] + x * b[i] - y * a[i];

        z[order] = x * b[order] - y * a[order];

        return y;
    }

    /** Process block of samples */
    virtual void processBlock(FloatType *block, const int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
            block[n] = processSample(block[n]);
    }

    /** Set coefficients to new values */
    virtual void setCoefs(const FloatType (&newB)[order + 1], const FloatType (&newA)[order + 1])
    {
        std::copy(newB, &newB[order + 1], b);
        std::copy(newA, &newA[order + 1], a);
    }

  protected:
    FloatType a[order + 1];
    FloatType b[order + 1];
    FloatType z[order + 1];
};

} // namespace chowdsp
