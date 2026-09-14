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

#include "MTSESPTuning.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>

#include "fmt/core.h"

#ifndef SURGE_SKIP_ODDSOUND_MTS
#include "libMTSClient.h"
#endif

namespace Surge
{
namespace Storage
{

namespace
{
// Pitches closer than this are the same pitch. The scale text we generate carries six decimals
// of cents, so an exact reconstruction lands far inside it.
constexpr double sameCentsTolerance{1e-3};

/*
 * A tone is only written as a ratio when one matches to within floating point noise. Anything
 * looser and an irrational tone, such as any step of 12-TET, now and then lands close enough
 * to some fraction to come out as a meaningless ratio like 749/500.
 */
constexpr double ratioCentsTolerance{1e-9};
constexpr int64_t maxRatioDenominator{1000}, maxRatioNumerator{10000};

double centsBetween(double from, double to) { return 1200.0 * std::log2(to / from); }

// Walk the continued fraction convergents, which are the best small ratios near a value
std::optional<std::pair<int64_t, int64_t>> ratioForCents(double cents)
{
    auto v = std::pow(2.0, cents / 1200.0);
    int64_t hPrev{0}, h{1}, kPrev{1}, k{0};

    for (int i = 0; i < 32; ++i)
    {
        auto a = (int64_t)std::floor(v);
        auto hNext = a * h + hPrev;
        auto kNext = a * k + kPrev;

        if (hNext > maxRatioNumerator || kNext > maxRatioDenominator)
        {
            break;
        }

        hPrev = h;
        h = hNext;
        kPrev = k;
        k = kNext;

        if (std::fabs(centsBetween((double)k, (double)h) - cents) < ratioCentsTolerance)
        {
            return std::make_pair(h, k);
        }

        auto frac = v - (double)a;

        if (frac < 1e-12)
        {
            break;
        }

        v = 1.0 / frac;
    }

    return std::nullopt;
}

std::string toneText(double cents)
{
    if (auto r = ratioForCents(cents))
    {
        return fmt::format("{}/{}", r->first, r->second);
    }

    return fmt::format("{:.6f}", cents);
}

// A single line which the SCL parser won't mistake for a comment
std::string descriptionFor(const std::string &scaleName)
{
    auto res = scaleName;
    std::replace_if(res.begin(), res.end(), [](char c) { return c == '\n' || c == '\r'; }, ' ');

    auto first = res.find_first_not_of(" \t!");
    auto last = res.find_last_not_of(" \t");

    if (first == std::string::npos)
    {
        return "MTS-ESP Tuning";
    }

    return res.substr(first, last - first + 1);
}

// A frequency which isn't positive can't be expressed as a tone, so stand 12-TET in for it
std::array<double, 128> usableFrequencies(const MTSESPTuningInfo &info)
{
    auto res = info.frequencies;

    for (int i = 0; i < 128; ++i)
    {
        if (!std::isfinite(res[i]) || res[i] <= 0.0)
        {
            res[i] = Tunings::MIDI_0_FREQ * std::pow(2.0, i / 12.0);
        }
    }

    return res;
}

bool isValidKey(int key) { return key >= 0 && key <= 127; }

// The frequency of a filtered note is only a placeholder, so it can't anchor a mapping
int nearestUnfilteredKey(const MTSESPTuningInfo &info, int key)
{
    for (int d = 0; d < 128; ++d)
    {
        if (isValidKey(key - d) && !info.filtered[key - d])
        {
            return key - d;
        }

        if (isValidKey(key + d) && !info.filtered[key + d])
        {
            return key + d;
        }
    }

    return key;
}

bool reproduces(const Tunings::Tuning &tuning, const MTSESPTuningInfo &info,
                const std::array<double, 128> &freqs)
{
    for (int i = 0; i < 128; ++i)
    {
        if (info.filtered[i] == tuning.isMidiNoteMapped(i))
        {
            return false;
        }

        if (!info.filtered[i] &&
            std::fabs(centsBetween(freqs[i], tuning.frequencyForMidiNote(i))) > sameCentsTolerance)
        {
            return false;
        }
    }

    return true;
}

/*
 * Describe the table as a scale of mapSize tones starting at startKey, mapped linearly with
 * filtered notes left unmapped, and keep it only if it reproduces every note.
 */
std::optional<Tunings::Tuning> tuningForShape(const MTSESPTuningInfo &info,
                                              const std::array<double, 128> &freqs, int mapSize,
                                              int startKey, int refKey)
{
    if (mapSize < 1 || mapSize > 127 || !isValidKey(startKey) || !isValidKey(refKey))
    {
        return std::nullopt;
    }

    // Read the tones from the lowest repetition of the pattern, which is the same as any other
    // repetition if the table really does repeat. The check at the end catches it if it doesn't.
    auto readFrom = startKey % mapSize;

    if (readFrom + mapSize > 127)
    {
        return std::nullopt;
    }

    refKey = nearestUnfilteredKey(info, refKey);

    bool anyFiltered{false};

    for (int i = 0; i < mapSize; ++i)
    {
        anyFiltered = anyFiltered || info.filtered[readFrom + i];
    }

    auto description = descriptionFor(info.scaleName);

    auto scl = fmt::format("! Inferred by Surge XT from MTS-ESP tuning data\n"
                           "!\n"
                           "{}\n"
                           " {}\n"
                           "!\n",
                           description, mapSize);

    for (int i = 1; i <= mapSize; ++i)
    {
        scl += " " + toneText(centsBetween(freqs[readFrom], freqs[readFrom + i])) + "\n";
    }

    // A mapping with no key list is linear, so we only need one to leave notes unmapped
    auto kbmSize = anyFiltered ? mapSize : 0;

    auto kbm = fmt::format("! Inferred by Surge XT from MTS-ESP tuning data\n"
                           "!\n"
                           "! Size of map\n"
                           "{}\n"
                           "! First and last MIDI notes to map\n"
                           "0\n"
                           "127\n"
                           "! Middle note where the first entry of the scale is mapped\n"
                           "{}\n"
                           "! Reference note where frequency is fixed\n"
                           "{}\n"
                           "! Frequency for MIDI note {}\n"
                           "{:.12g}\n"
                           "! Scale degree for formal octave\n"
                           "{}\n"
                           "! Mapping\n",
                           kbmSize, startKey, refKey, refKey, freqs[refKey], kbmSize);

    if (anyFiltered)
    {
        for (int i = 0; i < mapSize; ++i)
        {
            kbm += info.filtered[readFrom + i] ? "x\n" : std::to_string(i) + "\n";
        }
    }

    try
    {
        auto scale = Tunings::parseSCLData(scl);
        auto mapping = Tunings::parseKBMData(kbm);

        scale.name = description;
        mapping.name = description;

        // Every note on the reference key's side may be filtered, so allow an unmapped center
        auto tuning = Tunings::Tuning(scale, mapping, true);

        if (reproduces(tuning, info, freqs))
        {
            return tuning;
        }
    }
    catch (const Tunings::TuningError &)
    {
    }

    return std::nullopt;
}

// The smallest number of keys over which the table repeats at the stated period
int repeatingMapSize(const MTSESPTuningInfo &info, const std::array<double, 128> &freqs)
{
    if (!(info.periodRatio > 1.0))
    {
        return -1;
    }

    auto periodCents = 1200.0 * std::log2(info.periodRatio);

    for (int m = 1; m <= 127; ++m)
    {
        bool anyPair{false}, allMatch{true};

        for (int k = 0; k + m <= 127 && allMatch; ++k)
        {
            if (info.filtered[k] || info.filtered[k + m])
            {
                continue;
            }

            anyPair = true;
            allMatch =
                std::fabs(centsBetween(freqs[k], freqs[k + m]) - periodCents) < sameCentsTolerance;
        }

        if (anyPair && allMatch)
        {
            return m;
        }
    }

    return -1;
}
} // namespace

bool MTSESPTuningInfo::operator==(const MTSESPTuningInfo &other) const
{
    return frequencies == other.frequencies && filtered == other.filtered &&
           scaleName == other.scaleName && periodRatio == other.periodRatio &&
           mapSize == other.mapSize && mapStartKey == other.mapStartKey && refKey == other.refKey;
}

Tunings::Tuning tuningFromMTSESPInfo(const MTSESPTuningInfo &info)
{
    auto freqs = usableFrequencies(info);

    // First, the shape the source told us about
    if (info.mapSize > 0 && isValidKey(info.mapStartKey))
    {
        auto ref = isValidKey(info.refKey) ? info.refKey : info.mapStartKey;

        if (auto t = tuningForShape(info, freqs, info.mapSize, info.mapStartKey, ref))
        {
            return *t;
        }
    }

    /*
     * Many sources don't describe their scale, and the period then reads as the default of an
     * octave. We only look for a pattern repeating at that period, since every equal division
     * also repeats trivially after a single key, which would make for a one tone scale.
     */
    auto mapSize = repeatingMapSize(info, freqs);

    if (mapSize > 0)
    {
        auto start = isValidKey(info.mapStartKey) ? info.mapStartKey
                     : isValidKey(info.refKey)    ? info.refKey
                                                  : 60;
        auto ref = isValidKey(info.refKey) ? info.refKey : start;

        if (auto t = tuningForShape(info, freqs, mapSize, start, ref))
        {
            return *t;
        }
    }

    // Finally, one tone per key, which reproduces any table no matter how irregular
    if (auto t = tuningForShape(info, freqs, 127, 0, isValidKey(info.refKey) ? info.refKey : 69))
    {
        return *t;
    }

    return Tunings::Tuning();
}

MTSESPTuningInfo mtsESPInfoForTuning(const Tunings::Tuning &tuning)
{
    MTSESPTuningInfo res;

    const auto &scale = tuning.scale;
    const auto &mapping = tuning.keyboardMapping;

    for (int i = 0; i < 128; ++i)
    {
        res.frequencies[i] = tuning.frequencyForMidiNote(i);
        res.filtered[i] = !tuning.isMidiNoteMapped(i);
    }

    res.scaleName = scale.description;

    // The formal octave of a scale is its last tone, which is not necessarily a 2/1
    res.periodRatio =
        scale.count > 0 ? std::pow(2.0, scale.tones[scale.count - 1].cents / 1200.0) : 2.0;

    // A mapping with no explicit key list repeats the scale pattern every scale.count keys
    res.mapSize = std::clamp((mapping.count > 0) ? mapping.count : scale.count, 0, 127);
    res.mapStartKey = std::clamp(mapping.middleNote, 0, 127);
    res.refKey = std::clamp(mapping.tuningConstantNote, 0, 127);

    return res;
}

#ifndef SURGE_SKIP_ODDSOUND_MTS
MTSESPTuningInfo mtsESPInfoFromClient(MTSClient *client)
{
    MTSESPTuningInfo res;

    if (!client)
    {
        return res;
    }

    /*
     * Channel 0 rather than -1, on purpose. The client library remembers whether the latest
     * note filter query named a channel, and every later lookup uses that to decide whether
     * multichannel tables apply, including the audio thread's lookups. Asking without a
     * channel here would quietly switch multichannel tuning off for the synth.
     */
    for (int i = 0; i < 128; ++i)
    {
        res.frequencies[i] = MTS_NoteToFrequency(client, (char)i, 0);
        res.filtered[i] = MTS_ShouldFilterNote(client, (char)i, 0);
    }

    if (auto name = MTS_GetScaleName(client))
    {
        res.scaleName = name;
    }

    res.periodRatio = MTS_GetPeriodRatio(client);

    // These return a plain char, which is unsigned on some platforms, and -1 is meaningful
    res.mapSize = (signed char)MTS_GetMapSize(client);
    res.mapStartKey = (signed char)MTS_GetMapStartKey(client);
    res.refKey = (signed char)MTS_GetRefKey(client);

    return res;
}
#endif

} // namespace Storage
} // namespace Surge
