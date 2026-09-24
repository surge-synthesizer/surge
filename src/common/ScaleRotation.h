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

#ifndef SURGE_SRC_COMMON_SCALEROTATION_H
#define SURGE_SRC_COMMON_SCALEROTATION_H

#include "Tunings.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace Surge
{
namespace Tuning
{

namespace detail
{

/*
 * One interval above the root, kept as an exact ratio for as long as the arithmetic allows
 * it. A rotation multiplies and divides at most three of a scale's tones together, so a
 * just scale rotates exactly and 3/2 does not decay into 701.955 cents on the way.
 */
struct Interval
{
    bool isRatio{true};
    int64_t num{1}, den{1};
    double cents{0.0};
};

inline int64_t gcd64(int64_t a, int64_t b)
{
    while (b != 0)
    {
        auto t = a % b;

        a = b;
        b = t;
    }

    return a < 0 ? -a : a;
}

/*
 * Above this a product of two terms can pass int64_t, so the interval falls back to cents.
 * Only a scale written with absurd ratios ever reaches it.
 */
inline bool ratioFits(int64_t a, int64_t b)
{
    constexpr int64_t limit{3037000499LL}; // floor(sqrt(INT64_MAX))

    return std::llabs(a) < limit && std::llabs(b) < limit;
}

inline Interval makeCents(double c)
{
    Interval i;

    i.isRatio = false;
    i.cents = c;

    return i;
}

inline Interval makeRatio(int64_t n, int64_t d)
{
    if (n == 0 || d == 0)
    {
        return makeCents(0.0);
    }

    auto g = gcd64(n, d);

    if (g > 1)
    {
        n /= g;
        d /= g;
    }

    Interval i;

    i.isRatio = true;
    i.num = n;
    i.den = d;
    i.cents = 1200.0 * std::log(1.0 * n / d) / std::log(2.0);

    return i;
}

inline Interval multiply(const Interval &a, const Interval &b)
{
    if (a.isRatio && b.isRatio && ratioFits(a.num, a.den) && ratioFits(b.num, b.den))
    {
        return makeRatio(a.num * b.num, a.den * b.den);
    }

    return makeCents(a.cents + b.cents);
}

inline Interval divide(const Interval &a, const Interval &b)
{
    if (a.isRatio && b.isRatio && ratioFits(a.num, a.den) && ratioFits(b.num, b.den))
    {
        return makeRatio(a.num * b.den, a.den * b.num);
    }

    return makeCents(a.cents - b.cents);
}

inline Tunings::Tone toTone(const Interval &i)
{
    Tunings::Tone t;

    if (i.isRatio)
    {
        t.type = Tunings::Tone::kToneRatio;
        t.ratio_n = i.num;
        t.ratio_d = i.den;
        t.cents = i.cents;
        t.stringRep = std::to_string(i.num) + "/" + std::to_string(i.den);
    }
    else
    {
        t.type = Tunings::Tone::kToneCents;
        t.cents = i.cents;
        t.stringRep = std::to_string(i.cents);
    }

    t.floatValue = t.cents / 1200.0 + 1.0;

    return t;
}

} // namespace detail

/*
 * Rotate a scale so that one of its degrees becomes the new root, keeping every interval
 * of the original intact - the modal rotation. Degree 0 is the root itself and degree n
 * is scale.tones[n - 1], which is the numbering both the interval matrix headers and the
 * radial editor's tone list already use on screen.
 *
 * With invert set the rotated scale is then mirrored, so its steps run in the opposite
 * order. Rotating and then inverting keeps one promise true of both operations: the degree
 * you name becomes the new root. Rotating by degree 0 with invert is therefore the plain
 * inversion of the scale as it stands.
 */
inline Tunings::Scale rotateScale(const Tunings::Scale &s, int degree, bool invert)
{
    const auto n = s.count;

    if (n < 1 || (int)s.tones.size() < n)
    {
        return s;
    }

    degree = ((degree % n) + n) % n;

    if (degree == 0 && !invert)
    {
        return s;
    }

    // A scale is exact in ratios only if every one of its tones is, so one cents tone
    // makes the whole result cents rather than leaving a mixture behind.
    auto allRatios = true;

    for (const auto &t : s.tones)
    {
        allRatios = allRatios && t.type == Tunings::Tone::kToneRatio;
    }

    auto intervalOf = [allRatios](const Tunings::Tone &t) {
        return allRatios ? detail::makeRatio(t.ratio_n, t.ratio_d) : detail::makeCents(t.cents);
    };

    // The scale's degrees laid out over two periods, so that rotating past the top wraps
    // round without a special case.
    std::vector<detail::Interval> cumulative;

    cumulative.reserve(2 * n + 1);
    cumulative.push_back(detail::Interval{}); // the root, 1/1

    for (const auto &t : s.tones)
    {
        cumulative.push_back(intervalOf(t));
    }

    const auto period = cumulative[n];

    for (auto i = 1; i <= n; ++i)
    {
        cumulative.push_back(detail::multiply(cumulative[i], period));
    }

    std::vector<detail::Interval> rotated;

    rotated.reserve(n);

    for (auto m = 1; m <= n; ++m)
    {
        rotated.push_back(detail::divide(cumulative[degree + m], cumulative[degree]));
    }

    if (invert)
    {
        // Mirror the rotated scale: every step keeps its size but the steps run in the
        // opposite order, which reflects each degree about the root.
        const auto top = rotated.back();
        std::vector<detail::Interval> mirrored;

        mirrored.reserve(n);

        for (auto m = 1; m <= n; ++m)
        {
            const auto &below =
                (n - m == 0) ? cumulative[0] : rotated[static_cast<size_t>(n - m) - 1];

            mirrored.push_back(detail::divide(top, below));
        }

        rotated = mirrored;
    }

    auto res = s;

    res.tones.clear();
    res.tones.reserve(n);

    for (auto m = 0; m < n; ++m)
    {
        auto t = detail::toTone(rotated[m]);

        t.lineno = m;
        res.tones.push_back(t);
    }

    res.count = n;
    res.rawText = "";

    return res;
}

} // namespace Tuning
} // namespace Surge

#endif // SURGE_SRC_COMMON_SCALEROTATION_H
