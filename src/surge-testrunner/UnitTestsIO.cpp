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

#include "catch2/catch_amalgamated.hpp"

#include "UnitTestUtilities.h"
#include "WavetableScriptEvaluator.h"

#include <chrono>
#include <thread>

#include "UserDefaults.h"
#include <unordered_map>

using namespace Surge::Test;
using namespace std::chrono_literals;

TEST_CASE("We Can Read Wavetables", "[io]")
{
    /*
    ** ToDo:
    ** .wt file
    ** oneshot
    ** srgmarkers
    ** etc
    */
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    std::string metadata;

    SECTION("Wavetable.wav")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("resources/test-data/wav/Wavetable.wav", wt, metadata);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 256);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }

    SECTION("05_BELL.WAV")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("resources/test-data/wav/05_BELL.WAV", wt, metadata);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 33);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }

    SECTION("pluckalgo.wav")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("resources/test-data/wav/pluckalgo.wav", wt, metadata);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 9);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }
}

namespace
{
// assembles a RIFF file a chunk at a time, so each case can be malformed on purpose
struct TestWav
{
    std::ostringstream body;

    void tag(const char *c) { body.write(c, 4); }
    void u16(uint16_t v)
    {
        body.put((char)(v & 0xFF));
        body.put((char)((v >> 8) & 0xFF));
    }
    void u32(uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            body.put((char)((v >> (8 * i)) & 0xFF));
    }

    // 32 bit IEEE float, which is one of the two formats the loader accepts
    void fmtChunk(uint16_t channels = 1)
    {
        tag("fmt ");
        u32(16);
        u16(3);
        u16(channels);
        u32(44100);
        u32(44100 * 4);
        u16(4);
        u16(32);
    }

    // the 2048 sample frame marker, which is what gives the loader a loop length
    void clmChunk()
    {
        tag("clm ");
        u32(8);
        body.write("<!>2048", 7);
        body.put(0);
    }

    void dataChunk(uint32_t bytes)
    {
        tag("data");
        u32(bytes);
        for (uint32_t i = 0; i < bytes; ++i)
            body.put(0);
    }

    fs::path write(const std::string &name, const char *form = "WAVE")
    {
        auto p = fs::temp_directory_path() / name;
        std::ofstream o(p, std::ios::binary);
        auto b = body.str();

        o.write("RIFF", 4);
        for (int i = 0; i < 4; ++i)
            o.put((char)(((4 + b.size()) >> (8 * i)) & 0xFF));
        o.write(form, 4);
        o.write(b.data(), b.size());

        return p;
    }
};

struct WavErrorCatcher : SurgeStorage::ErrorListener
{
    std::string message;
    void onSurgeError(const std::string &msg, const std::string &title,
                      const SurgeStorage::ErrorType &type) override
    {
        message = msg;
    }
};

bool loadTestWav(const fs::path &p, std::string &md, std::string *error = nullptr)
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    WavErrorCatcher ec;
    surge->storage.addErrorListener(&ec);

    auto *wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
    bool loaded{true};

    REQUIRE_NOTHROW(loaded = surge->storage.load_wt_wav_portable(path_to_string(p), wt, md));

    surge->storage.removeErrorListener(&ec);

    if (error)
        *error = ec.message;

    return loaded;
}
} // namespace

TEST_CASE("WAV with a zero channel count", "[io]")
{
    // the sample count divides by numChannels, which nothing validated
    TestWav w;
    w.fmtChunk(0);
    w.dataChunk(16);

    std::string md;
    auto f = w.write("surge_wav_zero_channels.wav");

    // a file we cannot size should be refused, not sized to zero and carried on with
    REQUIRE(!loadTestWav(f, md));
    fs::remove(f);
}

TEST_CASE("Malformed WAV chunks are refused", "[io]")
{
    // cases where a chunk's declared size disagrees with what its branch reads
    std::string md;

    SECTION("a RIFF container which is not a WAVE")
    {
        // an AVI shares the outer RIFF header, so only the form type tells them apart
        TestWav w;
        w.fmtChunk();
        w.clmChunk();
        w.dataChunk(8192);

        auto f = w.write("surge_wav_avi_form.wav", "AVI ");
        REQUIRE(!loadTestWav(f, md));
        fs::remove(f);
    }

    SECTION("a format chunk too short to describe a format")
    {
        TestWav w;
        w.tag("fmt ");
        w.u32(0);

        auto f = w.write("surge_wav_empty_fmt.wav");
        REQUIRE(!loadTestWav(f, md));
        fs::remove(f);
    }

    // already refused, but the message blamed the wrong thing
    SECTION("a data chunk with no format chunk to size it")
    {
        TestWav w;
        w.clmChunk();
        w.dataChunk(8192);

        std::string err;
        auto f = w.write("surge_wav_no_fmt.wav");

        REQUIRE(!loadTestWav(f, md, &err));
        REQUIRE_THAT(err, Catch::Matchers::ContainsSubstring("no format chunk"));
        fs::remove(f);
    }

    SECTION("a WAVE carrying metadata but no format chunk and no data")
    {
        TestWav w;
        w.clmChunk();

        std::string err;
        auto f = w.write("surge_wav_only_clm.wav");

        REQUIRE(!loadTestWav(f, md, &err));
        REQUIRE_THAT(err, Catch::Matchers::ContainsSubstring("no format chunk"));
        fs::remove(f);
    }

    // The metadata chunks are optional, so a truncated one should be stepped over and
    // the rest of the file should still load rather than being rejected outright.
    SECTION("a cue chunk declaring more cue points than it holds")
    {
        TestWav w;
        w.fmtChunk();
        w.tag("cue ");
        w.u32(4);
        w.u32(0x0FFFFFFF);
        w.clmChunk();
        w.dataChunk(8192);

        auto f = w.write("surge_wav_overlarge_cue.wav");
        REQUIRE(loadTestWav(f, md));
        fs::remove(f);
    }

    SECTION("a sample chunk too short to hold its header")
    {
        TestWav w;
        w.fmtChunk();
        w.tag("smpl");
        w.u32(4);
        w.u32(0);
        w.clmChunk();
        w.dataChunk(8192);

        auto f = w.write("surge_wav_short_smpl.wav");
        REQUIRE(loadTestWav(f, md));
        fs::remove(f);
    }

    SECTION("a surge chunk too short to hold its length")
    {
        TestWav w;
        w.fmtChunk();
        w.tag("srge");
        w.u32(0);
        w.clmChunk();
        w.dataChunk(8192);

        auto f = w.write("surge_wav_short_srge.wav");
        REQUIRE(loadTestWav(f, md));
        fs::remove(f);
    }

    SECTION("metadata which is not null terminated")
    {
        // the chunk is exactly the bytes it declares, so the string has to stop there
        TestWav w;
        w.fmtChunk();
        w.tag("wtmd");
        w.u32(8);
        w.body.write("ABCDEFGH", 8);
        w.clmChunk();
        w.dataChunk(8192);

        auto f = w.write("surge_wav_unterminated_wtmd.wav");
        REQUIRE(loadTestWav(f, md));
        REQUIRE(md == "ABCDEFGH");
        fs::remove(f);
    }
}

TEST_CASE("All Factory Wavetables Are Loadable", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    REQUIRE(surge.get());
    for (auto p : surge->storage.wt_list)
    {
        // Skip .wtscript files
        if (p.path.extension() == ".wtscript")
        {
            continue;
        }
        // Skip user folder
        if (!surge->storage.wt_category[p.category].isFactory)
        {
            continue;
        }
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        wt->size = -1;
        wt->n_tables = -1;
        surge->storage.load_wt(path_to_string(p.path), wt,
                               &(surge->storage.getPatch().scene[0].osc[0]));
        REQUIRE(wt->size > 0);
        REQUIRE(wt->n_tables > 0);
    }
}

#if HAS_LUA
TEST_CASE("All Factory .wtscript Files Validate", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    REQUIRE(surge.get());

    auto la = std::make_unique<Surge::WavetableScript::LuaWTEvaluator>();
    auto oscdata = &(surge->storage.getPatch().scene[0].osc[0]);

    for (auto p : surge->storage.wt_list)
    {
        // Skip non .wtscript files
        if (p.path.extension() != ".wtscript")
        {
            continue;
        }
        // Skip user folder
        if (!surge->storage.wt_category[p.category].isFactory)
        {
            continue;
        }
        INFO("Loading wtscript " << p.path);

        oscdata->wavetable_display_name = "";
        REQUIRE(la->loadWtscriptForTesting(p.path, &surge->storage, oscdata));
        REQUIRE(oscdata->wavetable_display_name != "");
    }
}
#endif

TEST_CASE("All Patches Are Loadable", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    REQUIRE(surge.get());
    int i = 0;
    for (auto p : surge->storage.patch_list)
    {
        INFO("Loading patch [" << p.name << "] from ["
                               << surge->storage.patch_category[p.category].name << " / isFactory="
                               << surge->storage.patch_category[p.category].isFactory << "]");
        surge->loadPatch(i);
        ++i;

        // A tiny oddity that the surge state pops up if we have tuning patches in the library so
        surge->storage.remapToConcertCKeyboard();
        surge->storage.retuneTo12TETScaleC261Mapping();
    }
}

TEST_CASE("DAW Streaming And Unstreaming", "[io][mpe][tun]")
{
    // The basic plan of attack is, in a section, set up two surges,
    // stream onto data on the first and off of data on the second
    // and voila

    auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                     std::shared_ptr<SurgeSynthesizer> dest) {
        void *d = nullptr;
        src->populateDawExtraState();
        auto sz = src->saveRaw(&d);
        REQUIRE(src->storage.getPatch().dawExtraState.isPopulated);

        dest->loadRaw(d, sz, false);
        dest->loadFromDawExtraState();
        REQUIRE(dest->storage.getPatch().dawExtraState.isPopulated);

        // Why does this crash macos?
        // if(d) free(d);
    };

    SECTION("MPE Enabled State Saves")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        REQUIRE(surgeSrc->mpeEnabled == false);
        REQUIRE(surgeDest->mpeEnabled == false);

        surgeSrc->mpeEnabled = true;
        REQUIRE(surgeDest->mpeEnabled == false);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeDest->mpeEnabled == true);

        surgeSrc->mpeEnabled = false;
        REQUIRE(surgeSrc->mpeEnabled == false);
        REQUIRE(surgeDest->mpeEnabled == true);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeSrc->mpeEnabled == false);
        REQUIRE(surgeDest->mpeEnabled == false);
    }

    SECTION("MPE Pitch Bend State Saves")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        // I purposefully use two values here which are not my default
        auto v1 = 54;
        auto v2 = 13;

        // Test from defaulted dest
        surgeSrc->storage.mpePitchBendRange = v2;
        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeDest->storage.mpePitchBendRange == v2);

        // Test from set dest
        surgeSrc->storage.mpePitchBendRange = v1;
        surgeDest->storage.mpePitchBendRange = v1;
        REQUIRE(surgeSrc->storage.mpePitchBendRange == v1);
        REQUIRE(surgeDest->storage.mpePitchBendRange == v1);

        surgeSrc->storage.mpePitchBendRange = v2;
        REQUIRE(surgeSrc->storage.mpePitchBendRange == v2);
        REQUIRE(surgeDest->storage.mpePitchBendRange == v1);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeDest->storage.mpePitchBendRange == v2);
    }

    SECTION("Everything Standard Stays Standard")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);
        REQUIRE(surgeSrc->storage.isStandardTuning);
        REQUIRE(surgeSrc->storage.isStandardMapping);
        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeSrc->storage.isStandardTuning);
        REQUIRE(surgeSrc->storage.isStandardMapping);
        REQUIRE(surgeDest->storage.isStandardTuning);
        REQUIRE(surgeDest->storage.isStandardMapping);
    }

    SECTION("SCL State Saves")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);
        Tunings::Scale s = Tunings::readSCLFile("resources/test-data/scl/zeus22.scl");

        REQUIRE(surgeSrc->storage.isStandardTuning);
        REQUIRE(surgeDest->storage.isStandardTuning);

        surgeSrc->storage.retuneToScale(s);
        REQUIRE(!surgeSrc->storage.isStandardTuning);
        REQUIRE(surgeDest->storage.isStandardTuning);
        REQUIRE(surgeSrc->storage.currentScale.count != surgeDest->storage.currentScale.count);
        REQUIRE(surgeSrc->storage.currentScale.count == s.count);

        fromto(surgeSrc, surgeDest);
        REQUIRE(!surgeSrc->storage.isStandardTuning);
        REQUIRE(!surgeDest->storage.isStandardTuning);

        REQUIRE(surgeSrc->storage.currentScale.count == surgeDest->storage.currentScale.count);
        REQUIRE(surgeSrc->storage.currentScale.count == s.count);

        REQUIRE(surgeSrc->storage.currentScale.rawText == surgeDest->storage.currentScale.rawText);
    }

    SECTION("Save And Restore KBM")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        auto k = Tunings::readKBMFile("resources/test-data/scl/mapping-a440-constant.kbm");

        REQUIRE(surgeSrc->storage.isStandardMapping);
        REQUIRE(surgeDest->storage.isStandardMapping);

        surgeSrc->storage.remapToKeyboard(k);
        REQUIRE(!surgeSrc->storage.isStandardMapping);
        REQUIRE(surgeDest->storage.isStandardMapping);

        fromto(surgeSrc, surgeDest);
        REQUIRE(!surgeSrc->storage.isStandardMapping);
        REQUIRE(!surgeDest->storage.isStandardMapping);
        REQUIRE(surgeSrc->storage.currentMapping.tuningConstantNote == 69);
        REQUIRE(surgeDest->storage.currentMapping.tuningConstantNote == 69);

        REQUIRE(surgeDest->storage.currentMapping.rawText ==
                surgeSrc->storage.currentMapping.rawText);

        surgeSrc->storage.remapToConcertCKeyboard();
        REQUIRE(surgeSrc->storage.isStandardMapping);
        REQUIRE(!surgeDest->storage.isStandardMapping);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeSrc->storage.isStandardMapping);
        REQUIRE(surgeDest->storage.isStandardMapping);
    }

    SECTION("Save And Restore Parameter MIDI Learn - Simple")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        // Simplest case
        surgeSrc->storage.getPatch().param_ptr[118]->midictrl = 57;
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[118]->midictrl != 57);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[118]->midictrl == 57);
    }

    SECTION("Save And Restore Parameter MIDI Learn - Empty")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        fromto(surgeSrc, surgeDest);
        for (int i = 0; i < n_global_params + n_scene_params; ++i)
        {
            REQUIRE(surgeSrc->storage.getPatch().param_ptr[i]->midictrl ==
                    surgeDest->storage.getPatch().param_ptr[i]->midictrl);
            REQUIRE(surgeSrc->storage.getPatch().param_ptr[i]->midictrl == -1);
        }
    }

    SECTION("Save And Restore Parameter MIDI Learn - Multiple")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        // Bigger Case
        surgeSrc->storage.getPatch().param_ptr[118]->midictrl = 57;
        surgeSrc->storage.getPatch().param_ptr[123]->midictrl = 59;
        surgeSrc->storage.getPatch().param_ptr[172]->midictrl = 82;
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[123]->midictrl == 59);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[172]->midictrl == 82);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[118]->midictrl != 57);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[123]->midictrl != 59);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[172]->midictrl != 82);

        fromto(surgeSrc, surgeDest);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[118]->midictrl == 57);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[123]->midictrl == 59);
        REQUIRE(surgeSrc->storage.getPatch().param_ptr[172]->midictrl == 82);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[118]->midictrl == 57);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[123]->midictrl == 59);
        REQUIRE(surgeDest->storage.getPatch().param_ptr[172]->midictrl == 82);
    }

    SECTION("Save And Restore MIDI Learn For Macros")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        for (int i = 0; i < n_customcontrollers; ++i)
        {
            REQUIRE(surgeSrc->storage.controllers[i] == 41 + i);
            REQUIRE(surgeDest->storage.controllers[i] == 41 + i);
        }

        surgeSrc->storage.controllers[2] = 75;
        surgeSrc->storage.controllers[4] = 79;
        fromto(surgeSrc, surgeDest);
        for (int i = 0; i < n_customcontrollers; ++i)
        {
            REQUIRE(surgeSrc->storage.controllers[i] == surgeDest->storage.controllers[i]);
        }
        REQUIRE(surgeDest->storage.controllers[2] == 75);
        REQUIRE(surgeDest->storage.controllers[4] == 79);
    }
}

TEST_CASE("Stream Wavetable Names", "[io]")
{
    SECTION("Name Restored For Old Patch")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        REQUIRE(surge->loadPatchByPath("resources/test-data/patches/Church.fxp", -1, "Test"));
        REQUIRE(std::string(surge->storage.getPatch().scene[0].osc[0].wavetable_display_name) ==
                "(Patch Wavetable)");
    }

    SECTION("Name Set When Loading a Factory Patch")
    {
        auto surge = Surge::Headless::createSurge(44100, true);
        REQUIRE(surge);
        REQUIRE(surge->storage.wt_list.size() > 0);

        auto patch = &(surge->storage.getPatch());
        patch->scene[0].osc[0].type.val.i = ot_wavetable;
        for (int i = 0; i < 2; ++i)
            surge->process();

        for (int i = 0; i < 40; ++i)
        {
            int wti;
            // Exclude .wtscript files
            do
            {
                wti = rand() % surge->storage.wt_list.size();
            } while (surge->storage.wt_list[wti].path.extension() == ".wtscript");
            INFO("Loading random wavetable " << wti << " at run " << i);

            surge->storage.load_wt(wti, &patch->scene[0].osc[0].wt, &patch->scene[0].osc[0]);
            REQUIRE(std::string(patch->scene[0].osc[0].wavetable_display_name) ==
                    surge->storage.wt_list[wti].name);
        }
    }

    SECTION("Name Survives Restore")
    {
        auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                         std::shared_ptr<SurgeSynthesizer> dest) {
            void *d = nullptr;
            src->populateDawExtraState();
            auto sz = src->saveRaw(&d);
            REQUIRE(src->storage.getPatch().dawExtraState.isPopulated);

            dest->loadRaw(d, sz, false);
            dest->loadFromDawExtraState();
            REQUIRE(dest->storage.getPatch().dawExtraState.isPopulated);

            // Why does this crash macos?
            // if(d) free(d);
        };

        auto surgeS = Surge::Headless::createSurge(44100, true);
        REQUIRE(surgeS->storage.wt_list.size() > 0);

        auto surgeD = Surge::Headless::createSurge(44100, true);
        REQUIRE(surgeD);

        for (int i = 0; i < 50; ++i)
        {
            auto patch = &(surgeS->storage.getPatch());
            std::vector<bool> iswts;
            std::vector<std::string> names;

            for (int s = 0; s < n_scenes; ++s)
                for (int o = 0; o < n_oscs; ++o)
                {
                    bool isWT = 1.0 * rand() / RAND_MAX > 0.7;
                    iswts.push_back(isWT);

                    auto patch = &(surgeS->storage.getPatch());
                    if (isWT)
                    {
                        patch->scene[s].osc[o].type.val.i = ot_wavetable;
                        for (int i = 0; i < 2; ++i)
                            surgeS->process();

                        int wti;
                        // Exclude .wtscript files
                        do
                        {
                            wti = rand() % surgeS->storage.wt_list.size();
                        } while (surgeS->storage.wt_list[wti].path.extension() == ".wtscript");

                        surgeS->storage.load_wt(wti, &patch->scene[s].osc[o].wt,
                                                &patch->scene[s].osc[o]);
                        REQUIRE(std::string(patch->scene[s].osc[o].wavetable_display_name) ==
                                surgeS->storage.wt_list[wti].name);

                        if (1.0 * rand() / RAND_MAX > 0.8)
                        {
                            auto sn = std::string("renamed blurg ") + std::to_string(rand());
                            patch->scene[s].osc[o].wavetable_display_name = sn;
                            REQUIRE(std::string(patch->scene[s].osc[o].wavetable_display_name) ==
                                    sn);
                        }
                        names.push_back(patch->scene[s].osc[o].wavetable_display_name);
                    }
                    else
                    {
                        patch->scene[s].osc[o].type.val.i = ot_sine;
                        names.push_back("");
                    }
                }

            fromto(surgeS, surgeD);
            auto patchD = &(surgeD->storage.getPatch());

            int idx = 0;
            for (int s = 0; s < n_scenes; ++s)
                for (int o = 0; o < n_oscs; ++o)
                {
                    if (iswts[idx])
                        REQUIRE(std::string(patchD->scene[s].osc[o].wavetable_display_name) ==
                                names[idx]);
                    idx++;
                }
        }
    }
}

TEST_CASE("Load Patches With Embedded KBM", "[io]")
{
    SECTION("Check Restore")
    {
        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideTuningOnPatchLoad, true);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideMappingOnPatchLoad, true);
            surge->loadPatchByPath("resources/test-data/patches/HasKBM.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardScale);
            REQUIRE(!surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideTuningOnPatchLoad, true);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideMappingOnPatchLoad, true);
            surge->loadPatchByPath("resources/test-data/patches/HasSCL.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardScale);
            REQUIRE(surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideTuningOnPatchLoad, true);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideMappingOnPatchLoad, true);
            surge->loadPatchByPath("resources/test-data/patches/HasSCLandKBM.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(!surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideTuningOnPatchLoad, true);
            surge->storage.userDefaultsProvider->addOverride(
                Surge::Storage::OverrideMappingOnPatchLoad, true);
            surge->loadPatchByPath("resources/test-data/patches/HasSCL_165Vintage.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(surge->storage.isStandardMapping);
        }
    }
}

/*
 * This test is here just so I have a place to hang code that builds patches
 */
TEST_CASE("Patch Version Builder", "[io]")
{
#if BUILD_PATCHES_SV14
    SECTION("Build All 14 Filters")
    {
        REQUIRE(ff_revision == 14);
        for (int i = 0; i < n_fu_types; ++i)
        {
            std::cout << fut_names[i] << std::endl;
            for (int j = 0; j < fut_subcount[i]; ++j)
            {
                auto surge = Surge::Headless::createSurge(44100);

                for (int s = 0; s < n_scenes; ++s)
                {
                    for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                    {
                        surge->storage.getPatch().scene[s].filterunit[fu].type.val.i = i;
                        surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i = j;
                    }
                }
                std::ostringstream oss;
                oss << "resources/test-data/patches/all-filters/s14/filt_" << i << "_" << j
                    << ".fxp";
                auto p = string_to_path(oss.str());
                surge->savePatchToPath(p);
            }
        }
    }
#endif

#if BUILD_PATCHES_SV15
    SECTION("Build All 15 Filters")
    {
        REQUIRE(ff_revision == 15);
        for (int i = 0; i < n_fu_types; ++i)
        {
            std::cout << fut_names[i] << std::endl;
            for (int j = 0; j < fut_subcount[i]; ++j)
            {
                auto surge = Surge::Headless::createSurge(44100);

                for (int s = 0; s < n_scenes; ++s)
                {
                    for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                    {
                        surge->storage.getPatch().scene[s].filterunit[fu].type.val.i = i;
                        surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i = j;
                    }
                }
                std::ostringstream oss;
                oss << "resources/test-data/patches/all-filters/s15/filt_" << i << "_" << j
                    << ".fxp";
                auto p = string_to_path(oss.str());
                surge->savePatchToPath(p);
            }
        }
    }
#endif

    auto p14 = string_to_path("resources/test-data/patches/all-filters/s14");
    for (auto ent : fs::directory_iterator(p14))
    {
        DYNAMIC_SECTION("Test SV14 Filter " << path_to_string(ent))
        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath(path_to_string(ent).c_str(), -1, "TEST");
            surge->process();
            auto ft = surge->storage.getPatch().scene[0].filterunit[0].type.val.i;
            auto st = surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i;
            auto lft = ft;
            auto lst = st;
            if (ff_revision >= 27)
            {
                // If the engiunue is later than revision 27 in the code this should lift
                if (ft == sst::filters::FilterType::fut_obxd_4pole)
                {
                    if (st == sst::filters::FilterSubType::st_obxd4pole_24dB)
                    {
                        lst = sst::filters::FilterSubType::st_obxd4pole_broken24dB;
                    }
                }
                if (ft == sst::filters::FilterType::fut_bp12)
                {
                    if (st == sst::filters::FilterSubType::st_Driven)
                    {
                        lst = sst::filters::FilterSubType::st_bp12_LegacyDriven;
                    }
                    if (st == sst::filters::FilterSubType::st_Clean)
                    {
                        lst = sst::filters::FilterSubType::st_bp12_LegacyClean;
                    }
                }
            }
            for (int s = 0; s < n_scenes; ++s)
            {
                for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                {
                    INFO(path_to_string(ent) << " " << lft << " " << lst << " " << s << " " << fu);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].type.val.i == lft);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i == lst);
                }
            }

            INFO("Patch for filter " << sst::filters::filter_type_names[ft]);
            if (ff_revision == 14)
            {
                std::ostringstream cand_fn;
                cand_fn << "filt_" << ft << "_" << st << ".fxp";
                auto entfn = path_to_string(ent.path().filename());
                REQUIRE(entfn == cand_fn.str());
            }
            else if (ff_revision > 14)
            {
                using sst::filters::FilterType;
                const auto fft = (FilterType)ft;
                int fnft = ft;
                int fnst = st;
                switch (fft)
                {
                case FilterType::fut_none:
                case FilterType::fut_lp12:
                case FilterType::fut_lp24:
                case FilterType::fut_lpmoog:
                case FilterType::fut_hp12:
                case FilterType::fut_hp24:
                case FilterType::fut_SNH:
                case FilterType::fut_vintageladder:
                case FilterType::fut_k35_lp:
                case FilterType::fut_k35_hp:
                case FilterType::fut_diode:
                case FilterType::fut_cutoffwarp_lp:
                case FilterType::fut_cutoffwarp_hp:
                case FilterType::fut_cutoffwarp_n:
                case FilterType::fut_cutoffwarp_bp:
                case FilterType::num_filter_types:
                    // These types were unchanged
                    break;
                    // These are the types which changed 14 -> 15
                case FilterType::fut_obxd_4pole:
                    if (ff_revision >= 27)
                    {
                        if (lst == sst::filters::FilterSubType::st_obxd4pole_broken24dB)
                        {
                            fnst = sst::filters::FilterSubType::st_obxd4pole_24dB;
                        }
                    }
                    break;
                case FilterType::fut_comb_pos:
                    fnft = fut_14_comb;
                    fnst = st;
                    break;
                case FilterType::fut_comb_neg:
                    fnft = fut_14_comb;
                    fnst = st + 2;
                    break;
                case FilterType::fut_obxd_2pole_lp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 0;
                    break;
                case FilterType::fut_obxd_2pole_bp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 1;
                    break;
                case FilterType::fut_obxd_2pole_hp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 2;
                    break;
                case FilterType::fut_obxd_2pole_n:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 3;
                    break;
                case FilterType::fut_notch12:
                    fnft = fut_14_notch12;
                    break;
                case FilterType::fut_notch24:
                    fnft = fut_14_notch12;
                    fnst = st + 2;
                    break;
                case FilterType::fut_bp12:
                    fnft = fut_14_bp12;
                    if (ff_revision < 27)
                    {
                        fnst = lst;
                    }
                    else
                    {
                        if (lst == sst::filters::FilterSubType::st_bp12_LegacyDriven)
                            fnst = sst::filters::FilterSubType::st_Driven;
                        if (lst == sst::filters::FilterSubType::st_bp12_LegacyClean)
                            fnst = sst::filters::FilterSubType::st_Clean;
                    }
                    break;
                case FilterType::fut_bp24:
                    fnft = fut_14_bp12;
                    fnst = st + 3;
                    break;
                default:
                    break;
                }
                std::ostringstream cand_fn;
                cand_fn << "filt_" << fnft << "_" << fnst << ".fxp";
                auto entfn = path_to_string(ent.path().filename());
                REQUIRE(entfn == cand_fn.str());
            }
        }
    }

    auto p15 = string_to_path("resources/test-data/patches/all-filters/s15");
    for (auto ent : fs::directory_iterator(p15))
    {
        DYNAMIC_SECTION("Test SV15 Filters " << path_to_string(ent))
        {
            REQUIRE(ff_revision >= 15);
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath(path_to_string(ent).c_str(), -1, "TEST");
            surge->process();
            auto ft = surge->storage.getPatch().scene[0].filterunit[0].type.val.i;
            auto st = surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i;

            auto lft = ft;
            auto lst = st;
            if (ff_revision >= 27)
            {
                // If the engiunue is later than revision 27 in the code this should lift
                if (ft == sst::filters::FilterType::fut_obxd_4pole)
                {
                    if (st == sst::filters::FilterSubType::st_obxd4pole_broken24dB)
                    {
                        lst = sst::filters::FilterSubType::st_obxd4pole_24dB;
                    }
                }
                if (ft == sst::filters::FilterType::fut_bp12)
                {
                    if (st == sst::filters::FilterSubType::st_bp12_LegacyDriven)
                    {
                        lst = sst::filters::FilterSubType::st_Driven;
                    }
                    if (st == sst::filters::FilterSubType::st_bp12_LegacyClean)
                    {
                        lst = sst::filters::FilterSubType::st_Clean;
                    }
                }
            }
            for (int s = 0; s < n_scenes; ++s)
            {
                for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                {
                    INFO(path_to_string(ent) << " " << ft << " " << st << " " << s << " " << fu);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].type.val.i == ft);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i == st);
                }
            }

            std::ostringstream cand_fn;
            cand_fn << "filt_" << lft << "_" << lst << ".fxp";
            auto entfn = path_to_string(ent.path().filename());
            REQUIRE(entfn == cand_fn.str());
        }
    }
}

TEST_CASE("Mono Voice Priority Streams", "[io]")
{
    auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                     std::shared_ptr<SurgeSynthesizer> dest) {
        void *d = nullptr;
        auto sz = src->saveRaw(&d);

        dest->loadRaw(d, sz, false);
    };

    SECTION("Mono Voice Priority Streams Properly")
    {
        int mvp = ALWAYS_LOWEST;
        for (int i = 0; i < 20; ++i)
        {
            int r1 = rand() % (mvp + 1);
            int r2 = rand() % (mvp + 1);
            INFO("Checking type " << r1 << " " << r2);
            auto ssrc = Surge::Headless::createSurge(44100);
            ssrc->storage.getPatch().scene[0].monoVoicePriorityMode = (MonoVoicePriorityMode)r1;
            ssrc->storage.getPatch().scene[1].monoVoicePriorityMode = (MonoVoicePriorityMode)r2;
            auto sdst = Surge::Headless::createSurge(44100);

            REQUIRE(sdst->storage.getPatch().scene[0].monoVoicePriorityMode == ALWAYS_LATEST);
            REQUIRE(sdst->storage.getPatch().scene[1].monoVoicePriorityMode == ALWAYS_LATEST);

            fromto(ssrc, sdst);

            REQUIRE(sdst->storage.getPatch().scene[0].monoVoicePriorityMode ==
                    (MonoVoicePriorityMode)r1);
            REQUIRE(sdst->storage.getPatch().scene[1].monoVoicePriorityMode ==
                    (MonoVoicePriorityMode)r2);
        }
    }
}

TEST_CASE("XML Direct", "[io]")
{
    // This is not a public API but we want to make sure it
    // doesn't nuke surge with garbage
    SECTION("Nothin")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string blank{};
        surge->storage.getPatch().load_xml(blank.c_str(), blank.size(), false);
    }

    SECTION("Not XML")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"This Is Not A Standard String, says Renee"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Not XML")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"This Is Not A Standard String, says Renee"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Funny root node")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<funny/>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Invalid XML")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<funny></business>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Empty Patch")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch/>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Empty Parameters")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
    }

    SECTION("Tag without its attribute")
    {
        // TiXmlElement::Attribute returns null for an attribute that isn't there,
        // so a <tag/> carrying no tag built a std::string from nullptr. Patches
        // come from other people, so this is reachable by opening one.
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><meta><tags><tag/></tags></meta></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        REQUIRE(surge->storage.getPatch().tags.empty());
    }

    SECTION("Tags mixing named and unnamed")
    {
        // one malformed entry shouldn't cost the patch its other tags
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><meta><tags>"
                         "<tag tag=\"bass\"/><tag/><tag tag=\"lead\"/>"
                         "</tags></meta></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        REQUIRE(surge->storage.getPatch().tags.size() == 2);
    }
}
