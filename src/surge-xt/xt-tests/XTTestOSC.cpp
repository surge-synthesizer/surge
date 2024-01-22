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

#include "catch2/catch_amalgamated.hpp"
#include "SurgeSynthProcessor.h"

TEST_CASE("Can Make an SSP", "[xt-osc]")
{
    juce::MessageManager::getInstance();
    auto s = SurgeSynthProcessor();
    REQUIRE(s.supportsMPE());

    juce::MessageManager::deleteInstance();
}

TEST_CASE("Simulate the Audio Thread", "[xt-osc]")
{
    auto mm = juce::MessageManager::getInstance();

    auto s = SurgeSynthProcessor();
    std::atomic<bool> keepGoing{true};
    std::atomic<int32_t> gatedVoices{0};
    auto t = std::thread([&s, &keepGoing, &gatedVoices]() {
        using namespace std::chrono_literals;

        while (keepGoing)
        {
            auto a = juce::AudioBuffer<float>(6, 512);
            auto m = juce::MidiBuffer();
            s.processBlock(a, m);

            auto gv = 0;
            auto &srg = s.surge;
            for (const auto &sv : srg->voices)
                for (const auto &v : sv)
                    if (v->state.gate)
                        gv++;

            gatedVoices = gv;
            std::this_thread::sleep_for(1ms);
        }
    });

    mm->runDispatchLoop();

    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
    REQUIRE(gatedVoices == 0);

    auto msg = juce::OSCMessage("/mnote", 60.f, 90.f);
    s.oscHandler.oscMessageReceived(msg);
    std::this_thread::sleep_for(100ms);
    REQUIRE(gatedVoices == 1);

    msg = juce::OSCMessage("/mnote", 60.f, 0.f);
    s.oscHandler.oscMessageReceived(msg);
    std::this_thread::sleep_for(100ms);
    REQUIRE(gatedVoices == 0);

    keepGoing = false;
    t.join();
    juce::MessageManager::deleteInstance();
}