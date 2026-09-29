// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <fft_convolve.hpp>
#include <memory>
#include <span>

// Owns all allocated FFT state. Preparation depends only on immutable samples
// and copied settings; it does not access a live effect or synth storage.
struct ConvolutionKernel
{
    pffft::TwoStageConvolver left, right;
    std::size_t size{};

    static std::unique_ptr<ConvolutionKernel> prepare(
        std::span<const float> left, std::span<const float> right,
        float inputRate, float outputRate, float start, float reverse,
        bool reverseDeactivated);
};
