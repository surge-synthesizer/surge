#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"

using namespace Surge::Test;

TEST_CASE("Retune Surge to .scl files", "[tun]")
{
    auto surge = Surge::Headless::createSurge(44100);
    surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
    auto n2f = [surge](int n) { return surge->storage.note_to_pitch(n); };

    // Tunings::Scale s = Tunings::readSCLFile("/Users/paul/dev/music/test_scl/Q4.scl" );
    SECTION("12-intune SCL file")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/12-intune.scl");
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
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        REQUIRE(n2f(surge->storage.scaleConstantNote()) == surge->storage.scaleConstantPitch());
        REQUIRE(n2f(surge->storage.scaleConstantNote() + s.count) ==
                surge->storage.scaleConstantPitch() * 2);
        REQUIRE(n2f(surge->storage.scaleConstantNote() + 2 * s.count) ==
                surge->storage.scaleConstantPitch() * 4);
        REQUIRE(n2f(surge->storage.scaleConstantNote() - s.count) ==
                surge->storage.scaleConstantPitch() / 2);
    }

    SECTION("6 exact")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
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

    SECTION("Untuned - so regular tuning")
    {
        auto f60 = frequencyForNote(surge, 60);
        auto f72 = frequencyForNote(surge, 72);
        auto f69 = frequencyForNote(surge, 69);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(f72 == Approx(261.63 * 2).margin(.1));
        REQUIRE(f69 == Approx(440.0).margin(.1));
    }

    SECTION("Straight tuning scl file")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/12-intune.scl");
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
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl");
        surge->storage.retuneToScale(s);
        auto f60 = frequencyForNote(surge, 60);
        auto fDouble = frequencyForNote(surge, 60 + s.count);
        auto fHalf = frequencyForNote(surge, 60 - s.count);

        REQUIRE(f60 == Approx(261.63).margin(.1));
        REQUIRE(fDouble == Approx(261.63 * 2).margin(.1));
        REQUIRE(fHalf == Approx(261.63 / 2).margin(.1));
    }

    SECTION("6 exact")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
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
        auto k = Tunings::readKBMFile("test-data/scl/mapping-a440-constant.kbm");
        REQUIRE(k.name == "test-data/scl/mapping-a440-constant.kbm");
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

    SECTION("Parse 7 to 12 mapping File")
    {
        auto k = Tunings::readKBMFile("test-data/scl/mapping-a442-7-to-12.kbm");
        REQUIRE(k.name == "test-data/scl/mapping-a442-7-to-12.kbm");
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

    SECTION("Marvel 12 mapped and unmapped")
    {
        float unmapped[3];

        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/marvel12.scl");
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

        auto k = Tunings::readKBMFile("test-data/scl/mapping-a440-constant.kbm");

        surge->storage.remapToKeyboard(k);

        f60 = frequencyForNote(surge, 60);
        f72 = frequencyForNote(surge, 72);
        f69 = frequencyForNote(surge, 69);
        REQUIRE(f69 == Approx(440.0).margin(.1));
        REQUIRE(unmapped[2] / 440.0 == Approx(unmapped[0] / f60).margin(.001));
        REQUIRE(unmapped[2] / 440.0 == Approx(unmapped[1] / f72).margin(.001));
    }

    // and back and then back again
    SECTION("Can Map and ReMap consistently")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/marvel12.scl");
        auto k440 = Tunings::readKBMFile("test-data/scl/mapping-a440-constant.kbm");

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

    SECTION("Scale Ratio is Unch")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/marvel12.scl");
        auto k440 = Tunings::readKBMFile("test-data/scl/mapping-a440-constant.kbm");

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

TEST_CASE("Non-uniform keyboard mapping", "[tun]")
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

        auto k = Tunings::readKBMFile("test-data/scl/mapping-whitekeys-c261.kbm");
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

    SECTION("7 Note Scale with Tuning Centers")
    {
        auto k261 = Tunings::readKBMFile("test-data/scl/mapping-whitekeys-c261.kbm");
        auto k440 = Tunings::readKBMFile("test-data/scl/mapping-whitekeys-a440.kbm");

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

        auto k61 = Tunings::readKBMFile("test-data/scl/empty-note61.kbm");
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

        auto k69 = Tunings::readKBMFile("test-data/scl/empty-note69.kbm");
        REQUIRE(k69.count == 0);
        surge->storage.remapToKeyboard(k69);

        auto f60map = frequencyForNote(surge, 60);
        auto f69map = frequencyForNote(surge, 69);
        REQUIRE(frequencyForNote(surge, 69) == Approx(452).margin(0.1));
        REQUIRE(f69std / f60std == Approx(f69map / f60map).margin(0.001));
    }

    SECTION("Note 69 with Tuning")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/marvel12.scl");
        surge->storage.retuneToScale(s);
        auto f60std = frequencyForNote(surge, 60);
        auto f69std = frequencyForNote(surge, 69);
        REQUIRE(f60std == Approx(261.63).margin(0.1));
        REQUIRE(f69std != Approx(440.0).margin(0.1));

        auto k69 = Tunings::readKBMFile("test-data/scl/empty-note69.kbm");
        REQUIRE(k69.count == 0);
        surge->storage.remapToKeyboard(k69);

        auto f60map = frequencyForNote(surge, 60);
        auto f69map = frequencyForNote(surge, 69);
        REQUIRE(frequencyForNote(surge, 69) == Approx(452).margin(0.1));
        REQUIRE(f69std / f60std == Approx(f69map / f60map).margin(0.001));
    }
}

TEST_CASE("An Octave is an Octave", "[tun]")
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

    SECTION("Tuned to 12 OSC octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/12-intune.scl");
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
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/12-intune.scl");
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

    SECTION("22 note scale OSC Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl");
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

    SECTION("22 note scale Scene Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl");
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

    SECTION("6 note scale OSC Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
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

    SECTION("6 note scale Scene Octave")
    {
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
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

TEST_CASE("Non-Monotonic Tunings", "[tun]")
{
    SECTION("SCL Non-monotonicity")
    {
        auto surge = surgeOnSine();

        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/12-intune.scl");
        surge->storage.retuneToScale(s);
        std::vector<float> straight, shuffle;

        for (int i = 0; i <= 12; ++i)
            straight.push_back(frequencyForNote(surge, 60 + i));

        Tunings::Scale sh = Tunings::readSCLFile("test-data/scl/12-shuffled.scl");
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

        auto k = Tunings::readKBMFile("test-data/scl/shuffle-a440-constant.kbm");
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

TEST_CASE("Mapping below and outside of count")
{
    SECTION("A bit below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note54-to-259-6.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[54 + 256] == Approx(259.6 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("Twelve below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note48-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[48 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot below")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("Twelve above")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note72-to-500.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[72 + 256] == Approx(500.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot above")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note80-to-1000.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[80 + 256] == Approx(1000.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A bit below with 6ns")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note54-to-259-6.kbm");
        surge->storage.remapToKeyboard(k);

        REQUIRE(surge->storage.table_pitch[54 + 256] == Approx(259.6 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot below witn 6ns")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot above with 6ns")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("test-data/scl/6-exact.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note80-to-1000.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[80 + 256] == Approx(1000.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot below witn ED3-17")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("test-data/scl/ED3-17.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note42-to-100.kbm");

        surge->storage.remapToKeyboard(k);
        REQUIRE(surge->storage.table_pitch[42 + 256] == Approx(100.0 / 8.175798915).margin(1e-4));

        for (int i = 256; i < 256 + 128; ++i)
        {
            REQUIRE(surge->storage.table_pitch[i] > surge->storage.table_pitch[i - 1]);
        }
    }

    SECTION("A lot above with ED3-17")
    {
        auto surge = Surge::Headless::createSurge(44100);
        surge->storage.tuningApplicationMode = SurgeStorage::RETUNE_ALL;
        REQUIRE(surge.get());

        auto s = Tunings::readSCLFile("test-data/scl/ED3-17.scl");
        surge->storage.retuneToScale(s);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-note80-to-1000.kbm");

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
        char txt[256];
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

    SECTION("Frequency vs HPF 31 note scale")
    {
        auto surge = surgeOnSine();
        REQUIRE(surge.get());
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/31edo.scl");
        surge->storage.retuneToScale(s);

        surge->storage.getPatch().scene[0].lowcut.val.f = -12.0;
        char txt[256];
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

TEST_CASE("Ignoring Tuning Tables are Correct", "[dsp][tun]")
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

    SECTION("Ignoring in retuned compares with untuned")
    {
        auto surge = surgeOnSine();
        auto surgeTuned = surgeOnSine();
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/31edo.scl");
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

TEST_CASE("Modulation Tuning Mode and KBM", "[tun]")
{
    for (auto m = 0; m < 2; ++m)
    {
        auto mode = m ? SurgeStorage::RETUNE_MIDI_ONLY : SurgeStorage::RETUNE_ALL;
        auto moden = m ? "Retune Midi ONly" : "Retune All";
        DYNAMIC_SECTION("Modulation Tuning and KBM with " << moden)
        {
            auto surge = surgeOnSine();
            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("test-data/scl/ED2-15.scl");
            surge->storage.retuneToScale(scl);

            auto kbm = Tunings::readKBMFile("test-data/scl/60-262-60-c.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(60) ==
                    Approx(60 / 12));

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            kbm = Tunings::readKBMFile("test-data/scl/62-294-62-d.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(62) ==
                    Approx(62.f / 12));

            auto f62 = frequencyForNote(surge, 62);
            REQUIRE(f62 == Approx(293.66).margin(1));

            kbm = Tunings::readKBMFile("test-data/scl/64-330-64-e.kbm");
            surge->storage.remapToKeyboard(kbm);

            auto f64 = frequencyForNote(surge, 64);
            REQUIRE(f64 == Approx(329.62).margin(1));
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(64) ==
                    Approx(64.f / 12));
        }

        DYNAMIC_SECTION("SAW Modulation Tuning and KBM with " << moden)
        {
            auto surge = surgeOnSaw();
            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("test-data/scl/ED2-15.scl");
            surge->storage.retuneToScale(scl);

            auto kbm = Tunings::readKBMFile("test-data/scl/60-262-60-c.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(60) ==
                    Approx(60 / 12));

            auto f60 = frequencyForNote(surge, 60);
            REQUIRE(f60 == Approx(261.62).margin(1));

            kbm = Tunings::readKBMFile("test-data/scl/62-294-62-d.kbm");
            surge->storage.remapToKeyboard(kbm);
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(62) ==
                    Approx(62.f / 12));

            auto f62 = frequencyForNote(surge, 62);
            REQUIRE(f62 == Approx(293.66).margin(1));

            kbm = Tunings::readKBMFile("test-data/scl/64-330-64-e.kbm");
            surge->storage.remapToKeyboard(kbm);

            auto f64 = frequencyForNote(surge, 64);
            REQUIRE(f64 == Approx(329.62).margin(1));
            REQUIRE(surge->storage.currentTuning.logScaledFrequencyForMidiNote(64) ==
                    Approx(64.f / 12));
        }

        DYNAMIC_SECTION("Pitch Bend Distance 12 in mode sin " << moden)
        {
            auto surge = surgeOnSine();
            surge->mpeEnabled = false;
            surge->storage.getPatch().scene[0].pbrange_up.val.i = 12;
            surge->storage.getPatch().scene[0].pbrange_dn.val.i = 12;

            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("test-data/scl/ED2-06.scl");
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

        DYNAMIC_SECTION("Pitch Bend Distance 12 in mode saw " << moden)
        {
            auto surge = surgeOnSaw();
            surge->mpeEnabled = false;
            surge->storage.getPatch().scene[0].pbrange_up.val.i = 12;
            surge->storage.getPatch().scene[0].pbrange_dn.val.i = 12;

            REQUIRE(surge.get());
            surge->storage.setTuningApplicationMode(mode);

            auto scl = Tunings::readSCLFile("test-data/scl/ED2-06.scl");
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
