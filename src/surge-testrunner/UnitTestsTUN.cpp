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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"
#include "Tunings.h"

#include "catch2/catch_amalgamated.hpp"
#include "Tunings.h"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("Retune Surge XT to Scala Files", "[tun]")
{
    auto surge = Surge::Headless::createSurge(44100);
    surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
    auto n2f = [surge](int n) { return surge->storage.note_to_pitch(n); };

    // Tunings::Scale s = Tunings::readSCLFile("/Users/paul/dev/music/test_scl/Q4.scl" );
    SECTION("12-intune SCL file")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);
        INFO(surge->storage.scaleConstantNote() << " " << surge->storage.scaleConstantPitch() << " "
                                                << n2f(surge->storage.scaleConstantNote()));
        REQUIRE(n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch());
        REQUIRE(n2f(surge->storage.scaleConstantNote() + 12) ==
                surge->storage.scaleConstantPitch() * 2);
        REQUIRE(n2f(surge->storage.scaleConstantNote() + 12 + 12) ==
                surge->storage.scaleConstantPitch() * 4);
        REQUIRE(n2f(surge->storage.scaleConstantNote() - 12) ==
                surge->storage.scaleConstantPitch() / 2);
    }

    SECTION("Zeus 22")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch());
        REQUIRE(n2f(surge->storage.scaleConstantNote() + s.count) ==
                surge->storage.scaleConstantPitch() * 2);
        REQUIRE(n2f(surge->storage.scaleConstantNote() + 2 * s.count) ==
                surge->storage.scaleConstantPitch() * 4);
        REQUIRE(n2f(surge->storage.scaleConstantNote() - s.count) ==
                surge->storage.scaleConstantPitch() / 2);
    }

    SECTION("6 Exact")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch());
        REQUIRE(n2f(surge->storage.scaleConstantNote() + s.count) ==
                surge->storage.scaleConstantPitch() * 2);
        REQUIRE(n2f(surge->storage.scaleConstantNote() + 2 * s.count) ==
                surge->storage.scaleConstantPitch() * 4);
        REQUIRE(n2f(surge->storage.scaleConstantNote() - s.count) ==
                surge->storage.scaleConstantPitch() / 2);
    }
}

TEST_CASE("Notes at Appropriate Frequencies", "[tun]")
{
    auto surge = surgeOnSine();
    REQUIRE(surge.get());

    SECTION("Untuned - Standard Tuning")
    {
        auto f60 = frequencyForNote(surge, 60);
        auto f72 = frequencyForNote(surge, 72);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(f72 == Approx(261.63 * 2).margin(.1));
        REQUIRE(f69 == Approx(440.0).margin(.1));
    }

    SECTION("Straight Tuning via Scala File")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);
        auto f60 = frequencyForNote(surge, 60);
        auto f72 = frequencyForNote(surge, 72);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(f72 == Approx(261.63 * 2).margin(.1));
        REQUIRE(f69 == Approx(440.0).margin(.1));

        auto fPrior = f60;
        auto twoToTwelth = pow(2.0f, 1.0 / 12.0);
        for (int i = 61; i < 72; ++i)
        {
            auto fNow = frequencyForNote(surge, i);
            REQUIRE(fNow / fPrior == Approx(twoToTwelth).margin(.0001));
            fPrior = fNow;
        }
    }

    SECTION("Zeus 22")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        auto f60 = frequencyForNote(surge, 60);
        auto fDouble = frequencyForNote(surge, 60 + s.count);
        auto fHalf = frequencyForNote(surge, 60 - s.count);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(fDouble == Approx(261.63 * 2).margin(.1));
        REQUIRE(fHalf == Approx(261.63 / 2).margin(.1));
    }

    SECTION("6 Exact")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto f60 = frequencyForNote(surge, 60);
        auto fDouble = frequencyForNote(surge, 60 + s.count);
        auto fHalf = frequencyForNote(surge, 60 - s.count);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(fDouble == Approx(261.63 * 2).margin(.1));
        REQUIRE(fHalf == Approx(261.63 / 2).margin(.1));
    }
}

TEST_CASE("KBM File Parsing", "[tun]")
{
    SECTION("Default Keyboard is Default")
    {
        auto k = Tunings::KeyboardMapping();
        REQUIRE(k.count == 0);
        REQUIRE(k.firstMidi == 0);
        REQUIRE(k.lastMidi == 127);
        REQUIRE(k.middleNote == 60);
        REQUIRE(k.tuningConstantNote == 60);
        REQUIRE(k.tuningFrequency == Approx(261.62558));
        REQUIRE(k.octaveDegrees == 0);
        for (auto i = 0; i < k.keys.size(); ++i)
            REQUIRE(k.keys[i] == i);

        // REQUIRE( k.rawText == "" );
    }

    SECTION("Parse A440 File")
    {
        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-a440-constant.kbm");
        REQUIRE(k.name == "resources/test-data/scl/mapping-a440-constant.kbm");
        REQUIRE(k.count == 12);
        REQUIRE(k.firstMidi == 0);
        REQUIRE(k.lastMidi == 127);
        REQUIRE(k.middleNote == 60);
        REQUIRE(k.tuningConstantNote == 69);
        REQUIRE(k.tuningFrequency == Approx(440.0));
        REQUIRE(k.octaveDegrees == 12);
        for (auto i = 0; i < k.keys.size(); ++i)
            REQUIRE(k.keys[i] == i);
    }

    SECTION("Parse 7 to 12 Mapping File")
    {
        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-a442-7-to-12.kbm");
        REQUIRE(k.name == "resources/test-data/scl/mapping-a442-7-to-12.kbm");
        REQUIRE(k.count == 12);
        REQUIRE(k.firstMidi == 0);
        REQUIRE(k.lastMidi == 127);
        REQUIRE(k.middleNote == 59);
        REQUIRE(k.tuningConstantNote == 68);
        REQUIRE(k.tuningFrequency == Approx(442.0));
        REQUIRE(k.octaveDegrees == 7);

        std::vector<int> values = {0, 1, -1, 2, -1, 3, 4, -1, 5, -1, 6, -1};
        REQUIRE(values.size() == k.count);
        REQUIRE(values.size() == k.keys.size());
        for (int i = 0; i < k.keys.size(); ++i)
            REQUIRE(k.keys[i] == values[i]);
    }
}

TEST_CASE("KBM File Remaps Center", "[tun]")
{
    auto surge = surgeOnSine();
    REQUIRE(surge.get());

    SECTION("Marvel 12 Mapped And Unmapped")
    {
        float unmapped[3];

        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/marvel12.scl");
        surge->storage.retuneToScale(s);
        auto f60 = frequencyForNote(surge, 60);
        auto f72 = frequencyForNote(surge, 72);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(f72 == Approx(261.63 * 2).margin(.1));
        REQUIRE(f69 == Approx(448.2).margin(.1));
        unmapped[0] = f60;
        unmapped[1] = f72;
        unmapped[2] = f69;

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-a440-constant.kbm");

        surge->storage.remapToKeyboard(k);

        f60 = frequencyForNote(surge, 60);
        f72 = frequencyForNote(surge, 72);
        f69 = frequencyForNote(surge, 69);
        REQUIRE(f69 == Approx(440.0).margin(.1));
        REQUIRE(unmapped[2] / 440.0 == Approx(unmapped[0] / f60).margin(.001));
        REQUIRE(unmapped[2] / 440.0 == Approx(unmapped[1] / f72).margin(.001));
    }

    // and back and then back again
    SECTION("Can Map and Remap Consistently")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/marvel12.scl");
        auto k440 = Tunings::readKBMFile("resources/test-data/scl/mapping-a440-constant.kbm");

        surge->storage.retuneToScale(s);
        surge->storage.remapToConcertCKeyboard();

        auto f60std = frequencyForNote(surge, 60);
        auto f69std = frequencyForNote(surge, 69);

        surge->storage.remapToKeyboard(k440);
        auto f60map = frequencyForNote(surge, 60);
        auto f69map = frequencyForNote(surge, 69);

        REQUIRE(f60std == Approx(261.63).margin(0.1));
        REQUIRE(f69map == Approx(440.0).margin(0.1));
        REQUIRE(f69std / f60std == Approx(f69map / f60map).margin(.001));

        for (int i = 0; i < 50; ++i)
        {
            auto fr = 1.0f * rand() / (float)RAND_MAX;
            if (fr > 0)
            {
                surge->storage.remapToKeyboard(k440);
                auto f60 = frequencyForNote(surge, 60);
                auto f69 = frequencyForNote(surge, 69);
                REQUIRE(f60 == f60map);
                REQUIRE(f69 == f69map);
            }
            else
            {
                surge->storage.remapToConcertCKeyboard();
                auto f60 = frequencyForNote(surge, 60);
                auto f69 = frequencyForNote(surge, 69);
                REQUIRE(f60 == f60std);
                REQUIRE(f69 == f69std);
            }
        }
    }

    SECTION("Scale Ratio is Unchanged")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/marvel12.scl");
        auto k440 = Tunings::readKBMFile("resources/test-data/scl/mapping-a440-constant.kbm");

        surge->storage.retuneToScale(s);
        surge->storage.remapToConcertCKeyboard();
        auto f60 = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(261.63).margin(.1));

        std::vector<float> ratios;
        for (int i = 61; i < 72; ++i)
            ratios.push_back(frequencyForNote(surge, i) / f60);

        surge->storage.remapToConcertCKeyboard();
        auto f60map = frequencyForNote(surge, 60);
        for (int i = 61; i < 72; ++i)
        {
            auto fi = frequencyForNote(surge, i);
            REQUIRE(fi / f60map == Approx(ratios[i - 61]).margin(0.001));
        }
    }
}

TEST_CASE("Non-Uniform Keyboard Mapping", "[tun]")
{
    auto surge = surgeOnSine();
    REQUIRE(surge.get());

    auto mt = [](float c) {
        auto t = Tunings::Tone();
        t.type = Tunings::Tone::kToneCents;
        t.cents = c;
        t.floatValue = c / 1200.0 + 1.0;
        return t;
    };
    // This is the "white keys" scale
    Tunings::Scale s;
    s.count = 7;
    s.tones.push_back(mt(200));
    s.tones.push_back(mt(400));
    s.tones.push_back(mt(500));
    s.tones.push_back(mt(700));
    s.tones.push_back(mt(900));
    s.tones.push_back(mt(1100));
    s.tones.push_back(mt(1200));

    Tunings::Scale sWonky;
    sWonky.count = 7;
    sWonky.tones.push_back(mt(220));
    sWonky.tones.push_back(mt(390));
    sWonky.tones.push_back(mt(517));
    sWonky.tones.push_back(mt(682));
    sWonky.tones.push_back(mt(941));
    sWonky.tones.push_back(mt(1141));
    sWonky.tones.push_back(mt(1200));

    SECTION("7 Note Scale")
    {
        std::vector<float> frequencies;
        // When I map it directly I get 440 at note 65 (since I skipped 4 black keys)
        surge->storage.retuneToScale(s);
        for (int i = 0; i < 8; ++i)
        {
            frequencies.push_back(frequencyForNote(surge, 60 + i));
        }
        REQUIRE(frequencies[0] == Approx(261.63).margin(0.1));
        REQUIRE(frequencies[5] == Approx(440.00).margin(0.1));

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-whitekeys-c261.kbm");
        REQUIRE(k.count == 12);
        REQUIRE(k.octaveDegrees == 7);
        surge->storage.remapToKeyboard(k);
        auto f60 = frequencyForNote(surge, 60);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(0.1));
        REQUIRE(f69 == Approx(440).margin(0.1));

        // If we have remapped to white keys this will be true
        REQUIRE(frequencyForNote(surge, 60) == Approx(frequencies[0]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 62) == Approx(frequencies[1]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 64) == Approx(frequencies[2]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 65) == Approx(frequencies[3]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 67) == Approx(frequencies[4]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 69) == Approx(frequencies[5]).margin(0.1));
        REQUIRE(frequencyForNote(surge, 71) == Approx(frequencies[6]).margin(0.1));
    }

    SECTION("7 Note Scale With Tuning Centers")
    {
        auto k261 = Tunings::readKBMFile("resources/test-data/scl/mapping-whitekeys-c261.kbm");
        auto k440 = Tunings::readKBMFile("resources/test-data/scl/mapping-whitekeys-a440.kbm");

        surge->storage.retuneToScale(sWonky);
        surge->storage.remapToKeyboard(k261);
        auto f60 = frequencyForNote(surge, 60);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(0.1));
        REQUIRE(f69 != Approx(440.0).margin(0.1));

        surge->storage.retuneToScale(sWonky);
        surge->storage.remapToKeyboard(k440);
        auto f60_440 = frequencyForNote(surge, 60);
        auto f69_440 = frequencyForNote(surge, 69);

        REQUIRE(f60_440 != Approx(261.63).margin(0.1));
        REQUIRE(f69_440 == Approx(440.0).margin(0.1));

        REQUIRE(f69_440 / f60_440 == Approx(f69 / f60).margin(0.001));
    }
}

TEST_CASE("Zero Size Maps", "[tun]")
{
    auto surge = surgeOnSine();
    REQUIRE(surge.get());

    SECTION("Note 61")
    {
        auto f60std = frequencyForNote(surge, 60);
        auto f61std = frequencyForNote(surge, 61);
        REQUIRE(f60std == Approx(261.63).margin(0.1));

        auto k61 = Tunings::readKBMFile("resources/test-data/scl/empty-note61.kbm");
        REQUIRE(k61.count == 0);
        surge->storage.remapToKeyboard(k61);

        auto f60map = frequencyForNote(surge, 60);
        auto f61map = frequencyForNote(surge, 61);
        REQUIRE(frequencyForNote(surge, 61) == Approx(280).margin(0.1));
        REQUIRE(f61std / f60std == Approx(f61map / f60map).margin(0.001));
    }

    SECTION("Note 69")
    {
        auto f60std = frequencyForNote(surge, 60);
        auto f69std = frequencyForNote(surge, 69);
        REQUIRE(f60std == Approx(261.63).margin(0.1));
        REQUIRE(f69std == Approx(440.0).margin(0.1));

        auto k69 = Tunings::readKBMFile("resources/test-data/scl/empty-note69.kbm");
        REQUIRE(k69.count == 0);
        surge->storage.remapToKeyboard(k69);

        auto f60map = frequencyForNote(surge, 60);
        auto f69map = frequencyForNote(surge, 69);
        REQUIRE(frequencyForNote(surge, 69) == Approx(452).margin(0.1));
        REQUIRE(f69std / f60std == Approx(f69map / f60map).margin(0.001));
    }

    SECTION("Note 69 With Tuning")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/marvel12.scl");
        surge->storage.retuneToScale(s);
        auto f60std = frequencyForNote(surge, 60);
        auto f69std = frequencyForNote(surge, 69);
        REQUIRE(f60std == Approx(261.63).margin(0.1));
        REQUIRE(f69std != Approx(440.0).margin(0.1));

        auto k69 = Tunings::readKBMFile("resources/test-data/scl/empty-note69.kbm");
        REQUIRE(k69.count == 0);
        surge->storage.remapToKeyboard(k69);

        auto f60map = frequencyForNote(surge, 60);
        auto f69map = frequencyForNote(surge, 69);
        REQUIRE(frequencyForNote(surge, 69) == Approx(452).margin(0.1));
        REQUIRE(f69std / f60std == Approx(f69map / f60map).margin(0.001));
    }
}

TEST_CASE("An Octave Is an Octave", "[tun]")
{
    auto surge = surgeOnSine();
    REQUIRE(surge.get());

    SECTION("Untuned OSC Octave")
    {
        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("Untuned Scene Octave")
    {
        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("Tuned to 12 OSC Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("Tuned to 12 Scene Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("22 Note Scale OSC Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(s.count == 22);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("22 Note Scale Scene Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(s.count == 22);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("6 Note Scale OSC Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(s.count == 6);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].osc[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }

    SECTION("6 Note Scale Scene Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(s.count == 6);

        auto f60 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = -1;
        auto f60m1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 1;
        auto f60p1 = frequencyForNote(surge, 60);
        surge->storage.getPatch().scene[0].octave.val.i = 0;
        auto f60z = frequencyForNote(surge, 60);
        REQUIRE(f60 == Approx(f60z).margin(0.1));
        REQUIRE(f60 == Approx(f60m1 * 2).margin(0.1));
        REQUIRE(f60 == Approx(f60p1 / 2).margin(0.1));
    }
}

TEST_CASE("Channel to Octave Mapping", "[tun]")
{
    SECTION("When disabled and no tuning applied, channel 2 is the same as channel 1")
    {
        auto surge = surgeOnSine();
        float f1, f2;
        for (int key = 30; key < 70; key += 3)
        {
            f1 = frequencyForNote(surge, key, 1, 0, 0);
            f2 = frequencyForNote(surge, key, 1, 0, 1);
            REQUIRE(f2 == Approx(f1).margin(0.001));
        }
    }
    SECTION("When enabled and no tuning applied, channel 2 is one octave higher than channel 1")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        float f1, f2;
        for (int key = 30; key < 70; key += 3)
        { // Limited range because frequencyForNote starts having trouble measuring high
          // frequencies.
            INFO("key is " << key);
            f1 = frequencyForNote(surge, key, 1, 0, 0);
            f2 = frequencyForNote(surge, key, 1, 0, 1);
            REQUIRE(f2 == Approx(f1 * 2).margin(0.1));
        }
    }
    SECTION("When enabled and tuning applied, channel 2 is one octave higher than channel 1")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/31edo.scl");
        surge->storage.retuneToScale(s);
        float f1, f2;
        for (int key = 30; key < 70; key += 3)
        {
            f1 = frequencyForNote(surge, key, 1, 0, 0);
            f2 = frequencyForNote(surge, key, 1, 0, 1);
            REQUIRE(f2 == Approx(f1 * 2).margin(0.1));
        }
    }
    SECTION("When enabled and no tuning applied, note 60 is mapped to the correct octaves in "
            "different channels")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        float f1, f2;
        for (int chanOff = -2; chanOff < 3; chanOff++)
        { // Only checking reasonable octaves because frequencyForNote actually examines the
          // waveform
            INFO("chanOff is " << chanOff);
            f1 = frequencyForNote(surge, 60, 2, 0, (chanOff + 16) % 16);
            f2 = frequencyForNote(surge, 60, 2, 0, (chanOff + 1 + 16) % 16);
            REQUIRE(f2 == Approx(f1 * 2).margin(0.1));
        }
    }
    SECTION("When enabled with MPE on, MPE wins")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        surge->mpeEnabled = true;
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        float f1, f2;
        for (int chanOff = -2; chanOff < 3; chanOff++)
        { // Only checking reasonable octaves because frequencyForNote actually examines the
          // waveform
            INFO("chanOff is " << chanOff);
            f1 = frequencyForNote(surge, 60, 2, 0, (chanOff + 16) % 16);
            f2 = frequencyForNote(surge, 60, 2, 0, (chanOff + 1 + 16) % 16);
            REQUIRE(f2 == Approx(f1).margin(0.1));
        }
    }

    SECTION("When enabled and tuning applied, note 60 is mapped to the correct octaves in "
            "different channels")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/31edo.scl");
        surge->storage.retuneToScale(s);
        float f1, f2;
        for (int chanOff = -2; chanOff < 3; chanOff++)
        {
            INFO("chanOff is " << chanOff);
            f1 = frequencyForNote(surge, 60, 2, 0, (chanOff + 16) % 16);
            f2 = frequencyForNote(surge, 60, 2, 0, (chanOff + 1 + 16) % 16);
            REQUIRE(f2 == Approx(f1 * 2).margin(0.1));
        }
    }

    SECTION("ED2-7 Confirm that it still doubles with short scale")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        auto scale = Tunings::evenDivisionOfSpanByM(2, 7);
        surge->storage.retuneToScale(scale);

        for (int i = 0; i < 10; ++i)
            surge->process();
        auto f0 = frequencyForNote(surge, 60, 2, 0, 0);
        auto f1 = frequencyForNote(surge, 60, 2, 0, 1);
        auto f2 = frequencyForNote(surge, 60, 2, 0, 2);
        auto f15 = frequencyForNote(surge, 60, 2, 0, 15);
        auto f14 = frequencyForNote(surge, 60, 2, 0, 14);

        auto fr = Tunings::MIDI_0_FREQ * 32;
        REQUIRE(f0 == Approx(fr).margin(1));
        REQUIRE(f1 == Approx(fr * 2).margin(1));
        REQUIRE(f2 == Approx(fr * 4).margin(1));
        REQUIRE(f15 == Approx(fr / 2).margin(1));
        REQUIRE(f14 == Approx(fr / 4).margin(1));
    }

    SECTION("ED3-7 Triples Frequency")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        auto scale = Tunings::evenDivisionOfSpanByM(3, 7);
        surge->storage.retuneToScale(scale);

        for (int i = 0; i < 10; ++i)
            surge->process();
        auto f0 = frequencyForNote(surge, 60, 2, 0, 0);
        auto f1 = frequencyForNote(surge, 60, 2, 0, 1);

        auto fr = Tunings::MIDI_0_FREQ * 32;
        REQUIRE(f0 == Approx(fr).margin(1));
        REQUIRE(f1 == Approx(fr * 3).margin(1));
    }

    SECTION("ED4-7 Quads Frequency")
    {
        auto surge = surgeOnSine();
        surge->storage.mapChannelToOctave = true;
        auto scale = Tunings::evenDivisionOfSpanByM(4, 7);
        surge->storage.retuneToScale(scale);

        for (int i = 0; i < 10; ++i)
            surge->process();
        auto f0 = frequencyForNote(surge, 60, 2, 0, 0);
        auto f1 = frequencyForNote(surge, 60, 2, 0, 1);

        auto fr = Tunings::MIDI_0_FREQ * 32;
        REQUIRE(f0 == Approx(fr).margin(1));
        REQUIRE(f1 == Approx(fr * 4).margin(1));
    }
}

TEST_CASE("Non-Monotonic Tunings", "[tun]")
{
    SECTION("SCL Non-monotonicity")
    {
        auto surge = surgeOnSine();

        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);
        std::vector<float> straight, shuffle;

        for (int i = 0; i <= 12; ++i)
            straight.push_back(frequencyForNote(surge, 60 + i));

        Tunings::Scale sh = Tunings::readSCLFile("resources/test-data/scl/12-shuffled.scl");
        surge->storage.retuneToScale(sh);
        for (int i = 0; i <= 12; ++i)
            shuffle.push_back(frequencyForNote(surge, 60 + i));

        std::vector<int> indices = {0, 2, 1, 3, 5, 4, 6, 7, 8, 10, 9, 11, 12};
        for (int i = 0; i <= 12; ++i)
        {
            INFO("Comparing index " << i << " with " << indices[i]);
            REQUIRE(straight[i] == Approx(shuffle[indices[i]]).margin(0.1));
        }
    }

    SECTION("KBM Non-monotonicity")
    {
        auto surge = surgeOnSine();

        std::vector<float> straight, shuffle;
        for (int i = 0; i <= 12; ++i)
            straight.push_back(frequencyForNote(surge, 60 + i));

        auto k = Tunings::readKBMFile("resources/test-data/scl/shuffle-a440-constant.kbm");
        surge->storage.remapToKeyboard(k);
        for (int i = 0; i <= 12; ++i)
            shuffle.push_back(frequencyForNote(surge, 60 + i));

        std::vector<int> indices = {0, 2, 1, 3, 4, 6, 5, 7, 8, 9, 11, 10, 12};
        for (int i = 0; i <= 12; ++i)
        {
            INFO("Comparing index " << i << " with " << indices[i]);
            REQUIRE(straight[i] == Approx(shuffle[indices[i]]).margin(0.1));
        }
    }
}

TEST_CASE("Mapping Below And Outside of Count", "[tun]")
{
    SECTION("A Bit Below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note54-to-259-6.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[54 + 256] == Approx(259.6 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("12 Below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note48-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[48 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("12 Above")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note72-to-500.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[72 + 256] == Approx(500.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Above")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note80-to-1000.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[80 + 256] == Approx(1000.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Bit Below With 6 Note Scale")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note54-to-259-6.kbm");
        surge->storage.remapToKeyboard(k);

        REQUIRE(surge->storage.table_pitch[54 + 256] == Approx(259.6 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Below With 6 Note Scale")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Above With 6 Note Scale")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("resources/test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note80-to-1000.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[80 + 256] == Approx(1000.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Below With ED3-17")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("resources/test-data/scl/ED3-17.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A Lot Above With ED3-17")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("resources/test-data/scl/ED3-17.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-note80-to-1000.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[80 + 256] == Approx(1000.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }
}

TEST_CASE("HPF Ignores Tuning Properly", "[tun][dsp]")
{
    INFO("Testing condition reported in #1545");
    // This is a simple test which just guarantees monotonicity of the filter and
    // midpoint at the cutoff point both with tuning and not

    SECTION("Frequency And RMS In Default")
    {
        auto surge = surgeOnSine();
        REQUIRE(surge.get());

        for (auto note = 40; note < 90; ++note)
        {
            auto fr = frequencyAndRMSForNote(surge, note);
            REQUIRE(fr.second == Approx(0.25).margin(0.02));
        }
    }

    SECTION("Frequency vs HPF untuned")
    {
        auto surge = surgeOnSine();
        REQUIRE(surge.get());

        // f = 440 * 2^x/12
        // so if we want f to be 220
        // 2^x/12 = 1/2
        // x=-12
        surge->storage.getPatch().scene[0].lowcut.val.f = -12.0;
        char txt[TXT_SIZE];
        surge->storage.getPatch().scene[0].lowcut.get_display(txt);

        double priorRMS = 0;
        for (auto note = 20; note < 90; note += 2)
        {
            auto fr = frequencyAndRMSForNote(surge, note);

            REQUIRE(priorRMS < fr.second);
            priorRMS = fr.second;

            if (fr.first < 220)
                REQUIRE(fr.second < 0.1);
            else
                REQUIRE(fr.second > 0.1);
        }
    }

    SECTION("Frequency vs HPF 31 Note Scale")
    {
        auto surge = surgeOnSine();
        REQUIRE(surge.get());
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/31edo.scl");
        surge->storage.retuneToScale(s);

        surge->storage.getPatch().scene[0].lowcut.val.f = -12.0;
        char txt[TXT_SIZE];
        surge->storage.getPatch().scene[0].lowcut.get_display(txt);

        double priorRMS = 0;
        for (auto note = 1; note < 120; note += 2)
        {
            auto fr = frequencyAndRMSForNote(surge, note);

            REQUIRE(priorRMS < fr.second);
            priorRMS = fr.second;

            if (fr.first < 220)
                REQUIRE(fr.second < 0.1);
            else
                REQUIRE(fr.second > 0.1);
        }
    }
}

TEST_CASE("Ignoring Tuning Tables Are Correct", "[tun][dsp]")
{
    INFO("Testing condition reported in #1545");
    // This is a simple test which just guarantees monotonicity of the filter and
    // midpoint at the cutoff point both with tuning and not

    SECTION("Self-consistent when untuned")
    {
        auto surge = surgeOnSine();
        for (int i = 0; i < 1000; ++i)
        {
            auto e = 512.f * rand() / (float)RAND_MAX;
            REQUIRE(surge->storage.note_to_pitch(e) ==
                    surge->storage.note_to_pitch_ignoring_tuning(e));
            REQUIRE(surge->storage.note_to_pitch_inv(e) ==
                    surge->storage.note_to_pitch_inv_ignoring_tuning(e));

            float s, c, si, ci;
            surge->storage.note_to_omega(e, s, c);
            surge->storage.note_to_omega_ignoring_tuning(e, si, ci);
            REQUIRE(s == Approx(si).margin(1e-5));
            REQUIRE(c == Approx(ci).margin(1e-5));
        }
    }

    SECTION("Ignoring if retuned compares with untuned")
    {
        auto surge = surgeOnSine();
        auto surgeTuned = surgeOnSine();
        surgeTuned->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/31edo.scl");
        surgeTuned->storage.retuneToScale(s);

        for (int i = 0; i < 1000; ++i)
        {
            auto e = 512.f * rand() / (float)RAND_MAX;
            REQUIRE(surge->storage.note_to_pitch(e) ==
                    surgeTuned->storage.note_to_pitch_ignoring_tuning(e));
            REQUIRE(surge->storage.note_to_pitch(e) != surgeTuned->storage.note_to_pitch(e));

            REQUIRE(surge->storage.note_to_pitch_inv(e) ==
                    surgeTuned->storage.note_to_pitch_inv_ignoring_tuning(e));
            REQUIRE(surge->storage.note_to_pitch_inv(e) !=
                    surgeTuned->storage.note_to_pitch_inv(e));

            float s, c, si, ci;
            surge->storage.note_to_omega(e, s, c);
            surgeTuned->storage.note_to_omega_ignoring_tuning(e, si, ci);
            REQUIRE(s == Approx(si).margin(1e-5));
            REQUIRE(c == Approx(ci).margin(1e-5));
        }
    }
}

TEST_CASE("Modulation Tuning Mode And KBM", "[tun]")
{
    for (auto m = 0; m < 2; ++m)
    {
        auto mode = m ? SurgeStorage::RETUNE_MIDI_ONLY : SurgeStorage::RETUNE_ALL;
        auto moden = m ? "Retune MIDI Only" : "Retune All";
        DYNAMIC_SECTION("Modulation Tuning and KBM with " << moden)
        {
            auto surge = surgeOnSine();
            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("resources/test-data/scl/ED2-15.scl");
            surge->storage.retuneToScale(scl);

            auto kbm = Tunings::readKBMFile("resources/test-data/scl/60-262-60-c.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(60) ==
                    Approx(60 / 12));

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            kbm = Tunings::readKBMFile("resources/test-data/scl/62-294-62-d.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(62) ==
                    Approx(62.f / 12));

            auto f62 = frequencyForNote(surge, 62);
            REQUIRE(f62 == Approx(293.66).margin(1));

            kbm = Tunings::readKBMFile("resources/test-data/scl/64-330-64-e.kbm");
            surge->storage.remapToKeyboard(kbm);

            auto f64 = frequencyForNote(surge, 64);
            REQUIRE(f64 == Approx(329.62).margin(1));
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(64) ==
                    Approx(64.f / 12));
        }

        DYNAMIC_SECTION("Saw Modulation Tuning And KBM With " << moden)
        {
            auto surge = surgeOnSaw();
            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("resources/test-data/scl/ED2-15.scl");
            surge->storage.retuneToScale(scl);

            auto kbm = Tunings::readKBMFile("resources/test-data/scl/60-262-60-c.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(60) ==
                    Approx(60 / 12));

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            kbm = Tunings::readKBMFile("resources/test-data/scl/62-294-62-d.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(62) ==
                    Approx(62.f / 12));

            auto f62 = frequencyForNote(surge, 62);
            REQUIRE(f62 == Approx(293.66).margin(1));

            kbm = Tunings::readKBMFile("resources/test-data/scl/64-330-64-e.kbm");
            surge->storage.remapToKeyboard(kbm);

            auto f64 = frequencyForNote(surge, 64);
            REQUIRE(f64 == Approx(329.62).margin(1));
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(64) ==
                    Approx(64.f / 12));
        }

        DYNAMIC_SECTION("Pitch Bend Distance 12 in Sine " << moden)
        {
            auto surge = surgeOnSine();
            surge->mpeEnabled = false;
            surge->storage.getPatch().scene[0].pbrange_up.val.i = 12;
            surge->storage.getPatch().scene[0].pbrange_dn.val.i = 12;

            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("resources/test-data/scl/ED2-06.scl");
            surge->storage.retuneToScale(scl);

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            surge->pitchBend(0, 8191);
            auto f60Up = frequencyForNote(surge, 60);

            surge->pitchBend(0, -8192);
            auto f60Dn = frequencyForNote(surge, 60);

            if (mode == SurgeStorage::RETUNE_MIDI_ONLY)
            {
                REQUIRE(f60Up == Approx(f60 * 2).margin(3));
                REQUIRE(f60Dn == Approx(f60 / 2).margin(3));
            }
            else
            {
                REQUIRE(f60Up == Approx(f60 * 4).margin(6));
                REQUIRE(f60Dn == Approx(f60 / 4).margin(6));
            }
        }

        DYNAMIC_SECTION("Pitch Bend Distance 12 in Mode Saw " << moden)
        {
            auto surge = surgeOnSaw();
            surge->mpeEnabled = false;
            surge->storage.getPatch().scene[0].pbrange_up.val.i = 12;
            surge->storage.getPatch().scene[0].pbrange_dn.val.i = 12;

            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("resources/test-data/scl/ED2-06.scl");
            surge->storage.retuneToScale(scl);

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            surge->pitchBend(0, 8191);
            auto f60Up = frequencyForNote(surge, 60);

            surge->pitchBend(0, -8192);
            auto f60Dn = frequencyForNote(surge, 60);

            if (mode == SurgeStorage::RETUNE_MIDI_ONLY)
            {
                REQUIRE(f60Up == Approx(f60 * 2).margin(5));
                REQUIRE(f60Dn == Approx(f60 / 2).margin(5));
            }
            else
            {
                REQUIRE(f60Up == Approx(f60 * 4).margin(10));
                REQUIRE(f60Dn == Approx(f60 / 4).margin(10));
            }
        }
    }
}

TEST_CASE("Note To Pitch Invalid Ranges", "[tun]")
{
    SECTION("Note To Pitch Ignoring Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);
        float rl = surge->storage.note_to_pitch_ignoring_tuning(-256);
        float ru = surge->storage.note_to_pitch_ignoring_tuning(256);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float ro = surge->storage.note_to_pitch_ignoring_tuning(x);
            if (x <= -256)
                REQUIRE(ro == rl);
            if (x >= 256)
                REQUIRE(ro == ru);
        }
    }

    SECTION("N2PI Ignoring Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);
        float rl = surge->storage.note_to_pitch_inv_ignoring_tuning(-256);
        float ru = surge->storage.note_to_pitch_inv_ignoring_tuning(256);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float ro = surge->storage.note_to_pitch_inv_ignoring_tuning(x);
            if (x <= -256)
                REQUIRE(ro == rl);
            if (x >= 256)
                REQUIRE(ro == ru);
        }
    }

    SECTION("N2P Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);
        float sl, cl, su, cu;
        surge->storage.note_to_omega(-256, sl, cl);
        surge->storage.note_to_omega(256, su, cu);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float so, co;
            surge->storage.note_to_omega(x, so, co);
            if (x <= -256)
            {
                REQUIRE(so == sl);
                REQUIRE(co == cl);
            }
            if (x >= 256)
            {
                REQUIRE(so == su);
                REQUIRE(co == cu);
            }
        }
    }

    SECTION("N2OI Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);
        float sl, cl, su, cu;
        surge->storage.note_to_omega_ignoring_tuning(-256, sl, cl);
        surge->storage.note_to_omega_ignoring_tuning(256, su, cu);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float so, co;
            surge->storage.note_to_omega_ignoring_tuning(x, so, co);
            if (x <= -256)
            {
                REQUIRE(so == sl);
                REQUIRE(co == cl);
            }
            if (x >= 256)
            {
                REQUIRE(so == su);
                REQUIRE(co == cu);
            }
        }
    }

    SECTION("N2P Mod Tuned Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);

        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);

        REQUIRE(!surge->storage.tuningTableIs12TET());

        float rl = surge->storage.note_to_pitch(-256);
        float ru = surge->storage.note_to_pitch(256);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float ro = surge->storage.note_to_pitch(x);
            if (x <= -256)
                REQUIRE(ro == rl);
            if (x >= 256)
                REQUIRE(ro == ru);
        }
    }

    SECTION("N2PI Mod Tuned Span -1000 to 1000")
    {
        auto surge = Surge::Headless::createSurge(44100);

        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);

        REQUIRE(!surge->storage.tuningTableIs12TET());

        float rl = surge->storage.note_to_pitch_inv(-256);
        float ru = surge->storage.note_to_pitch_inv(256);
        for (float x = -1000; x < 1000; x += 0.27)
        {
            float ro = surge->storage.note_to_pitch_inv(x);
            if (x <= -256)
                REQUIRE(ro == rl);
            if (x >= 256)
                REQUIRE(ro == ru);
        }
    }
}

TEST_CASE("Octave Per Channel And Porta", "[tun]")
{
    namespace hs = Surge::Headless;

    struct TestCase
    {
        struct Note
        {
            Note(int note, int channel, bool isOn) : n(note), c(channel), on(isOn) {}
            int n, c;
            bool on;
        };
        TestCase(const std::string &n, MonoVoicePriorityMode prioritYMode, double result,
                 bool shouldPass, const std::vector<Note> &notes, int scaleLen = 12)
            : name(n), pri(prioritYMode), res(result), should(shouldPass), notes(notes),
              scaleLen(scaleLen)
        {
        }
        std::string name;
        MonoVoicePriorityMode pri;
        double res;
        bool should;
        std::vector<Note> notes;
        int scaleLen;
    };

    auto mno = Tunings::MIDI_0_FREQ;
    auto n60 = mno * 32;
    // clang-format off
    std::vector<TestCase> cases = {
        {"Different Note Same Channel", ALWAYS_HIGHEST, n60 * 2, true,
                {{60, 0, 1}, {72, 0, 1}}},
        {"Different Note Different Channel", ALWAYS_HIGHEST, n60 * 4, true,
         {{60, 0, 1}, {72, 1, 1}}},
        {"Same Note Different Channel", ALWAYS_HIGHEST, n60 * 2, true,
                {{60, 0, 1}, {60, 1, 1}}},
        {"Same Key But Lower From Channel", ALWAYS_HIGHEST, n60, true,
                      {{60, 0, 1}, {60, 15, 1}}},
        {"Higher Key But Lower From Channel", ALWAYS_HIGHEST, n60, true,
                      {{60, 0, 1}, {63, 15, 1}}},
        {"Lower Key But Higher From Channel", ALWAYS_HIGHEST, 440.0, true,
                     {{60, 0, 1}, {69 - 12, 1, 1}}},
        {"Note Off Same Channel", ALWAYS_HIGHEST, n60, true,
                           {{60, 0, 1}, {72, 0, 1}, {72, 0, 0}}},
        {"Note Off Different Channel", ALWAYS_HIGHEST, n60, true,
                   {{60, 0, 1}, {72, 1, 1}, {72, 1, 0}}},
        {"Same Note Off Different Channel", ALWAYS_HIGHEST, n60, true,
                   {{60, 0, 1}, {60, 1, 1}, {60, 1, 0}}},
        {"Higher Note Lower Pitch Off Different Channel", ALWAYS_HIGHEST, n60, true,
           {{60, 0, 1}, {64, 15, 1}, {64, 15, 0}}},
        {"Higher Note Higher Pitch Off Different Channel", ALWAYS_HIGHEST, n60, true,
           {{60, 0, 1}, {92, 15, 1}, {92, 15, 0}}},

        {"Different Note Same Channel", ALWAYS_LOWEST, n60, true,
                {{60, 0, 1}, {72, 0, 1}}},
        {"Different Note Same Channel Drop", ALWAYS_LOWEST, n60/2, true,
        {{60, 0, 1}, {48, 0, 1}}},
        {"Same Note Different Channel", ALWAYS_LOWEST, n60, true,
                {{60, 0, 1}, {60, 1, 1}}},
        {"Drop Note Different Channel", ALWAYS_LOWEST, n60/2, true,
                    {{60, 0, 1}, {60-24, 1, 1}}},
        {"Drop Note Different Channel Release", ALWAYS_LOWEST, n60, true,
                    {{60, 0, 1}, {60-24, 1, 1}, {60-24, 1, 0}}},
        {"Drop Note Higher Different Channel", ALWAYS_LOWEST, n60, true,
                    {{60, 0, 1}, {57, 1, 1}}},
        {"Drop Note Higher Different Channel Release", ALWAYS_LOWEST, n60, true,
            {{60, 0, 1}, {57, 1, 1}, {57,1,0}}},
        {"Different Note Same Channel ED2-31", ALWAYS_HIGHEST, n60 * 2, true,
             {{60, 0, 1}, {60+31, 0, 1}}, 31},
        {"Note on off Same Channel ED2-12", ALWAYS_HIGHEST, n60, true,
     {{60, 0, 1}, {60+31, 0, 1}, {60+31, 0, 0 }}, 12},
        {"Note on off Same Channel ED2-24", ALWAYS_HIGHEST, n60 , true,
      {{60, 0, 1}, {60+31, 0, 1}, {60+31, 0, 0 }}, 24},
        {"Note on off Same Channel ED2-31", ALWAYS_HIGHEST, n60, true,
             {{60, 0, 1}, {60+31, 0, 1}, {60+31, 0, 0 }}, 31},
        {"Note on pure play ED2-31", ALWAYS_HIGHEST, 547.340, true,
        {{0, 3, 1}}, 31},
        {"Note on off ED2-31", ALWAYS_HIGHEST, 547.340, true,
            {{0, 3, 1}, {10, 3, 1}, {10, 3, 0 }}, 31},
        {"Note on off ED2-31 on Ch 0", ALWAYS_HIGHEST, 547.340, true,
                 {{31 * 3, 0, 1}, {31 * 3 + 10, 0, 1}, {31 * 3 + 10, 0, 0 }}, 31},
        {"Note on off ED2-31 on Ch 1", ALWAYS_HIGHEST, 547.340, true,
              {{31 * 2, 1, 1}, {31 * 2 + 10, 1, 1}, {31 * 2 + 10, 1, 0 }}, 31},
        {"Note on 1 pure play ED2-31", ALWAYS_HIGHEST, 559.863, true,
            {{1, 3, 1}}, 31},
        {"Note on off 1 ED2-31", ALWAYS_HIGHEST, 559.863, true,
            {{1, 3, 1}, {10, 3, 1}, {10, 3, 0 }}, 31},

        { "Then in conjunction ED2-31", ALWAYS_HIGHEST, 611.6505, true,
           {{26, 2, 1}, {5, 3, 1}}, 31},
        { "ED2-31 10/3 to 23/2", ALWAYS_HIGHEST, 684.203, true,
           {{10, 3, 1}, {23, 2, 1}}, 31},
        { "ED2-31 10/3 to 18/2", ALWAYS_HIGHEST, 684.203, true,
            {{10, 3, 1}, {18, 2, 1}}, 31},

        { "ED2-31 high prio release last note after 29/2, 3/3, 8/3", ALWAYS_HIGHEST, 585.398, true,
            {{29, 2, 1}, {3, 3, 1}, {8, 3, 1}, {8, 3, 0}}, 31},
        { "ED2-31 low prio release last note after 0/3, 26/2, 21/2", ALWAYS_LOWEST, 489.320, true,
            {{0, 3, 1}, {26, 2, 1}, {21, 2, 1}, {21, 2, 0}}, 31},
    };
    // clang-format on

    for (auto tuningstyle : {SurgeStorage::RETUNE_ALL, SurgeStorage::RETUNE_MIDI_ONLY})
    {
        for (auto mode : {pm_mono, pm_mono_fp, pm_mono_st, pm_mono_st_fp})
        {
            for (const auto &c : cases)
            {
                DYNAMIC_SECTION("Mode "
                                << mode << " : Pri : " << c.pri << " : Case : " << c.name
                                << " : Scale : ED2-" << c.scaleLen << " TuningApplication : "
                                << (tuningstyle == SurgeStorage::RETUNE_ALL ? "All" : "MIDI")
                                << (c.should ? "" : " - SKIPPING"))
                {
                    auto surge = surgeOnSine();
                    surge->storage.getPatch().scene[0].monoVoicePriorityMode = c.pri;
                    surge->storage.getPatch().scene[0].polymode.val.i = mode;
                    surge->storage.mapChannelToOctave = true;
                    surge->storage.setTuningApplicationMode(tuningstyle);
                    surge->storage.retuneToScale(Tunings::evenDivisionOfSpanByM(2, c.scaleLen));

                    int len = BLOCK_SIZE * 20;
                    int idx = 0;
                    auto events = hs::playerEvents_t();
                    for (const auto nt : c.notes)
                    {
                        auto on = hs::Event();
                        on.type = nt.on ? hs::Event::NOTE_ON : hs::Event::NOTE_OFF;
                        on.channel = nt.c;
                        on.data1 = nt.n;
                        on.data2 = 100;
                        on.atSample = len * idx;
                        events.push_back(on);
                        idx++;
                    }

                    auto on = hs::Event();
                    on.type = hs::Event::NO_EVENT;
                    on.atSample = len * idx;
                    events.push_back(on);

                    float *buffer;
                    int nS, nC;
                    hs::playAsConfigured(surge, events, &buffer, &nS, &nC);
                    delete[] buffer;

                    events.clear();
                    on.atSample = len * 3;
                    events.push_back(on);
                    hs::playAsConfigured(surge, events, &buffer, &nS, &nC);

                    int nSTrim = (int)(nS / 2 * 0.8);
                    int start = (int)(nS / 2 * 0.05);
                    auto freq = frequencyFromData(buffer, nS, nC, 0, start, nSTrim,
                                                  surge->storage.samplerate);

                    delete[] buffer;

                    if (c.should)
                        REQUIRE(freq == Approx(c.res).margin(1));
                    else
                        REQUIRE_FALSE(freq == Approx(c.res).margin(1));
                }
            }
        }
    }
}

TEST_CASE("Portamento With Repeated Notes", "[tun]")
{
    for (auto tuningstyle : {SurgeStorage::RETUNE_ALL, SurgeStorage::RETUNE_MIDI_ONLY})
    {
        DYNAMIC_SECTION("Repeated note with porta in "
                        << (tuningstyle == SurgeStorage::RETUNE_ALL ? "All" : "MIDI"))
        {
            auto surge = surgeOnSine();
            surge->storage.setTuningApplicationMode(tuningstyle);
            surge->storage.retuneToScale(Tunings::evenDivisionOfSpanByM(2, 31));
            surge->storage.getPatch().scene[0].portamento.val.f = 1;

            namespace hs = Surge::Headless;
            auto events = hs::playerEvents_t();
            {
                auto on = hs::Event();
                on.type = hs::Event::NOTE_ON;
                on.channel = 0;
                on.data1 = 90;
                on.data2 = 100;
                on.atSample = 0;
                events.push_back(on);
            }

            {
                auto on = hs::Event();
                on.type = hs::Event::NOTE_OFF;
                on.channel = 0;
                on.data1 = 90;
                on.data2 = 100;
                on.atSample = 44100 * 3;
                events.push_back(on);
            }

            {
                auto on = hs::Event();
                on.type = hs::Event::NO_EVENT;
                on.channel = 0;
                on.data1 = 90;
                on.data2 = 100;
                on.atSample = 44100 * 3 + BLOCK_SIZE * 10;
                events.push_back(on);
            }

            float *buffer;
            int nS, nC;
            hs::playAsConfigured(surge, events, &buffer, &nS, &nC);
            delete[] buffer;

            for (int i = 0; i < 10; ++i)
            {
                events.clear();
                {
                    auto on = hs::Event();
                    on.type = hs::Event::NOTE_ON;
                    on.channel = 0;
                    on.data1 = 90;
                    on.data2 = 100;
                    on.atSample = 0;
                    events.push_back(on);
                }

                {
                    auto on = hs::Event();
                    on.type = hs::Event::NOTE_OFF;
                    on.channel = 0;
                    on.data1 = 90;
                    on.data2 = 100;
                    on.atSample = BLOCK_SIZE * 80;
                    events.push_back(on);
                }
                {
                    auto on = hs::Event();
                    on.type = hs::Event::NO_EVENT;
                    on.channel = 0;
                    on.data1 = 90;
                    on.data2 = 100;
                    on.atSample = BLOCK_SIZE * 90;
                    events.push_back(on);
                }
                hs::playAsConfigured(surge, events, &buffer, &nS, &nC);

                int nSTrim = (int)(nS / 2 * 0.8);
                int start = (int)(nS / 2 * 0.05);
                auto freq =
                    frequencyFromData(buffer, nS, nC, 0, start, nSTrim, surge->storage.samplerate);

                delete[] buffer;

                REQUIRE(freq ==
                        Approx(surge->storage.currentTuning.frequencyForMidiNote(90)).margin(2));
            }
        }
    }

    SECTION("Portamento in Octave Per Channel Mode")
    {
        auto surge = surgeOnSine();
        surge->storage.setTuningApplicationMode(SurgeStorage::RETUNE_MIDI_ONLY);
        surge->storage.mapChannelToOctave = true;
        surge->storage.retuneToScale(Tunings::evenDivisionOfSpanByM(2, 31));
        surge->storage.getPatch().scene[0].portamento.val.f = 1;

        namespace hs = Surge::Headless;
        auto events = hs::playerEvents_t();
        {
            auto on = hs::Event();
            on.type = hs::Event::NOTE_ON;
            on.channel = 3;
            on.data1 = 0;
            on.data2 = 100;
            on.atSample = 0;
            events.push_back(on);
        }

        {
            auto on = hs::Event();
            on.type = hs::Event::NOTE_OFF;
            on.channel = 3;
            on.data1 = 0;
            on.data2 = 100;
            on.atSample = 44100 * 3;
            events.push_back(on);
        }

        {
            auto on = hs::Event();
            on.type = hs::Event::NO_EVENT;
            on.channel = 3;
            on.data1 = 0;
            on.data2 = 100;
            on.atSample = 44100 * 3 + BLOCK_SIZE * 10;
            events.push_back(on);
        }

        float *buffer;
        int nS, nC;
        hs::playAsConfigured(surge, events, &buffer, &nS, &nC);
        delete[] buffer;

        for (int i = 0; i < 10; ++i)
        {
            events.clear();
            {
                auto on = hs::Event();
                on.type = hs::Event::NOTE_ON;
                on.channel = 3;
                on.data1 = 0;
                on.data2 = 100;
                on.atSample = 0;
                events.push_back(on);
            }

            {
                auto on = hs::Event();
                on.type = hs::Event::NOTE_OFF;
                on.channel = 3;
                on.data1 = 0;
                on.data2 = 100;
                on.atSample = BLOCK_SIZE * 80;
                events.push_back(on);
            }
            {
                auto on = hs::Event();
                on.type = hs::Event::NO_EVENT;
                on.channel = 3;
                on.data1 = 0;
                on.data2 = 100;
                on.atSample = BLOCK_SIZE * 90;
                events.push_back(on);
            }
            hs::playAsConfigured(surge, events, &buffer, &nS, &nC);

            int nSTrim = (int)(nS / 2 * 0.8);
            int start = (int)(nS / 2 * 0.05);
            auto freq =
                frequencyFromData(buffer, nS, nC, 0, start, nSTrim, surge->storage.samplerate);

            delete[] buffer;

            REQUIRE(freq ==
                    Approx(surge->storage.currentTuning.frequencyForMidiNote(31 * 3)).margin(2));
        }
    }
}