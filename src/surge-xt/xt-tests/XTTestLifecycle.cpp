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

#include <algorithm>
#include <cmath>

#include "catch2/catch_amalgamated.hpp"
#include "SurgeSynthProcessor.h"

TEST_CASE("Host reset silences the engine", "[xt-lifecycle]")
{
    juce::MessageManager::getInstance();

    constexpr int blockSize{512};
    constexpr double sampleRate{48000};

    auto proc = SurgeSynthProcessor();
    proc.setRateAndBufferSizeDetails(sampleRate, blockSize);
    proc.prepareToPlay(sampleRate, blockSize);

    auto buffer = juce::AudioBuffer<float>(6, blockSize);

    auto gatedVoices = [&proc]() {
        int gv{0};

        for (const auto &sceneVoices : proc.surge->voices)
        {
            for (const auto &v : sceneVoices)
            {
                if (v->state.gate)
                {
                    gv++;
                }
            }
        }

        return gv;
    };

    auto peak = [&buffer]() {
        float m{0.f};

        for (int c = 0; c < 2; ++c)
        {
            const auto *d = buffer.getReadPointer(c);

            for (int i = 0; i < blockSize; ++i)
            {
                m = std::max(m, std::fabs(d[i]));
            }
        }

        return m;
    };

    auto runBlock = [&proc, &buffer](juce::MidiBuffer m) {
        buffer.clear();
        proc.processBlock(buffer, m);
    };

    auto noteOn = juce::MidiBuffer();
    noteOn.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9f), 0);
    runBlock(noteOn);

    for (int i = 0; i < 4; ++i)
    {
        runBlock(juce::MidiBuffer());
    }

    REQUIRE(gatedVoices() == 1);
    REQUIRE(peak() > 0.01f);

    // The note is never released, so only the reset can quiet us down
    proc.reset();

    REQUIRE(gatedVoices() == 0);
    REQUIRE(proc.surge->voices[0].empty());
    REQUIRE(proc.surge->voices[1].empty());

    runBlock(juce::MidiBuffer());
    REQUIRE(peak() < 1e-6f);

    juce::MessageManager::deleteInstance();
}
