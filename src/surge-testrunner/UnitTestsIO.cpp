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
#include "dsp/oscillators/WavetableOscillator.h"
#include "PatchFileHeaderStructs.h"
#include "sst/basic-blocks/mechanics/endian-ops.h"

namespace mech = sst::basic_blocks::mechanics;

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

// a zstd frame whose header declares `claimed` bytes while carrying one
std::vector<unsigned char> zstdFrameClaiming(uint64_t claimed)
{
    std::vector<unsigned char> f{0x28, 0xB5, 0x2F, 0xFD}; // zstd magic
    f.push_back(0xE0);                                    // single segment, 8 byte size field

    for (int i = 0; i < 8; ++i)
        f.push_back((unsigned char)((claimed >> (8 * i)) & 0xFF));

    for (unsigned char c : {0x09, 0x00, 0x00}) // one raw block, last, one byte
        f.push_back(c);
    f.push_back(0x41);

    return f;
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

TEST_CASE("Wavetable headers are range checked before allocating", "[io]")
{
    // the buffer is sized from the header counts, so range check them first
    auto writeWt = [](const std::string &name, uint32_t nSamples, uint16_t nTables,
                      uint16_t flags) {
        auto p = fs::temp_directory_path() / name;
        std::ofstream o(p, std::ios::binary);

        o.write("vawt", 4);
        for (int i = 0; i < 4; ++i)
            o.put((char)((nSamples >> (8 * i)) & 0xFF));
        for (int i = 0; i < 2; ++i)
            o.put((char)((nTables >> (8 * i)) & 0xFF));
        for (int i = 0; i < 2; ++i)
            o.put((char)((flags >> (8 * i)) & 0xFF));

        return p;
    };

    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    auto *wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
    std::string md;

    SECTION("a float wavetable claiming the maximum of both counts")
    {
        auto f = writeWt("surge_wt_huge_f32.wt", 0x7FFFFFFF, 0x7FFF, 0);
        bool loaded{true};

        REQUIRE_NOTHROW(loaded = surge->storage.load_wt_wt(path_to_string(f), wt, md));
        REQUIRE(!loaded);
        fs::remove(f);
    }

    SECTION("the same header on the int16 path")
    {
        auto f = writeWt("surge_wt_huge_i16.wt", 0x7FFFFFFF, 0x7FFF, wtf_int16);
        bool loaded{true};

        REQUIRE_NOTHROW(loaded = surge->storage.load_wt_wt(path_to_string(f), wt, md));
        REQUIRE(!loaded);
        fs::remove(f);
    }
}

TEST_CASE("Truncated patches are refused before they are read", "[io]")
{
    // load_patch read the whole 32 byte header after checking only datasize > 4
    auto build = [](size_t sz, uint32_t xmlsize, uint32_t wt00) {
        char *b = (char *)malloc(sz);

        memset(b, 0, sz);
        memcpy(b, "sub3", 4);

        for (int i = 0; i < 4 && (size_t)(4 + i) < sz; ++i)
            b[4 + i] = (char)((xmlsize >> (8 * i)) & 0xFF);
        for (int i = 0; i < 4 && (size_t)(8 + i) < sz; ++i)
            b[8 + i] = (char)((wt00 >> (8 * i)) & 0xFF);

        return b;
    };

    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    SECTION("a patch shorter than its own header")
    {
        char *b = build(8, 0, 0);

        REQUIRE_NOTHROW(surge->loadRaw(b, 8, false));
        free(b);
    }

    SECTION("a wavetable header sitting exactly at the end")
    {
        // xmlsize of 0 leaves dr on end, which the old start-pointer check allowed
        char *b = build(32, 0, 64);

        REQUIRE_NOTHROW(surge->loadRaw(b, 32, false));
        free(b);
    }

    SECTION("a wavetable header whose frames are not present")
    {
        // BuildWT copies per the header counts, not wtsize: 16k out of 16 bytes
        const size_t sz = 32 + 12 + 16;
        char *b = build(sz, 0, 12 + 16);
        char *w = b + 32;

        memcpy(w, "vawt", 4);

        uint32_t nsamples = 4096;
        uint16_t ntables = 1, flags = 0;

        for (int i = 0; i < 4; ++i)
            w[4 + i] = (char)((nsamples >> (8 * i)) & 0xFF);
        for (int i = 0; i < 2; ++i)
            w[8 + i] = (char)((ntables >> (8 * i)) & 0xFF);
        for (int i = 0; i < 2; ++i)
            w[10 + i] = (char)((flags >> (8 * i)) & 0xFF);

        REQUIRE_NOTHROW(surge->loadRaw(b, (int)sz, false));
        free(b);
    }
}

TEST_CASE("Arbitrary block storage does not trust its declared size", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    // ~17.6 TB, against a cap of 256 MB
    auto f = zstdFrameClaiming(0x0000100000000000ULL);
    unsigned int consumed{1};

    REQUIRE_NOTHROW(consumed =
                        surge->storage.getPatch().load_arbitrary_block_storage(f.data(), f.size()));
    REQUIRE(consumed == 0);
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

TEST_CASE("Wavetable script snapshots do not trust their declared size", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100, true);
    REQUIRE(surge.get());

    // a real factory script, so the snapshot blob is the only suspect part
    fs::path src;
    for (const auto &p : surge->storage.wt_list)
    {
        if (p.path.extension() == ".wtscript" && surge->storage.wt_category[p.category].isFactory)
        {
            src = p.path;
            break;
        }
    }
    REQUIRE(!src.empty());

    std::ifstream in(src, std::ios::binary | std::ios::ate);
    REQUIRE(in);
    std::vector<char> xml(static_cast<std::size_t>(in.tellg()));
    in.seekg(0);
    in.read(xml.data(), xml.size());
    in.close();

    auto blob = zstdFrameClaiming(0x0000100000000000ULL); // ~17.6 TB, against a cap of 256 MB
    auto f = fs::temp_directory_path() / "surge_wtscript_huge_snapshot.wtscript";

    {
        std::ofstream o(f, std::ios::binary);
        o.write("wts1", 4);

        for (auto v : {(uint32_t)xml.size(), (uint32_t)blob.size()})
            for (int i = 0; i < 4; ++i)
                o.put((char)((v >> (8 * i)) & 0xFF));

        o.write(xml.data(), xml.size());
        o.write((const char *)blob.data(), blob.size());
    }

    auto la = std::make_unique<Surge::WavetableScript::LuaWTEvaluator>();
    auto oscdata = &(surge->storage.getPatch().scene[0].osc[0]);
    bool loaded{false};

    // the script still loads, the snapshot blob is the only thing refused
    REQUIRE_NOTHROW(loaded = la->loadWtscriptForTesting(f, &surge->storage, oscdata));
    REQUIRE(loaded);
    REQUIRE(!oscdata->wtSnapshots[0]);

    fs::remove(f);
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

        // A tiny oddity that the surge state pops up if we have tuning patches in the
        // library so
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

TEST_CASE("Global Modulation Routings Are Not Scene Tagged For Shared Modulators", "[io][mod]")
{
    // Macros and MIDI controllers are a single object shared by both scenes, so
    // ModulationRouting::source_scene carries no information for them. Streams written
    // before #4960 tagged a macro to FX routing with whichever scene happened to be
    // active. That routing still sounds, and the Modulation List still shows it, but the
    // macro's own context menu asks for scene A only and so never finds it. See #8053.

    auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                     std::shared_ptr<SurgeSynthesizer> dest) {
        void *d = nullptr;
        auto sz = src->saveRaw(&d);

        dest->loadRaw(d, sz, false);
    };

    // put a reverb in A Insert FX 1 and hand back the index of a param we can modulate
    auto reverbInAIns1 = [](std::shared_ptr<SurgeSynthesizer> s) {
        auto *pt = &(s->storage.getPatch().fx[fxslot_ains1].type);

        s->setParameter01(s->idForParameter(pt),
                          1.f * float(fxt_reverb) / (pt->val_max.i - pt->val_min.i), false);

        for (int i = 0; i < 10; ++i)
        {
            s->process();
        }

        for (int i = 0; i < n_fx_params; ++i)
        {
            if (s->storage.getPatch().fx[fxslot_ains1].p[i].modulateable)
            {
                return i;
            }
        }

        FAIL("no modulateable param in the reverb");

        return 0;
    };

    SECTION("A Macro To FX Routing Tagged Scene B Loads As Scene A")
    {
        auto src = Surge::Headless::createSurge(44100);
        auto dest = Surge::Headless::createSurge(44100);

        auto pidx = reverbInAIns1(src);
        auto *fxp = &(src->storage.getPatch().fx[fxslot_ains1].p[pidx]);

        // this is what a pre-#4960 stream contained
        REQUIRE(src->setModDepth01(fxp->id, ms_ctrl7, 1, 0, 0.5));

        // and this is the bug: the macro menu always asks for scene A
        REQUIRE_FALSE(src->isAnyActiveModulation(fxp->id, ms_ctrl7, 0));

        fromto(src, dest);

        auto *dfxp = &(dest->storage.getPatch().fx[fxslot_ains1].p[pidx]);

        REQUIRE(dest->isAnyActiveModulation(dfxp->id, ms_ctrl7, 0));
        REQUIRE(dest->getModDepth01(dfxp->id, ms_ctrl7, 0, 0) == Approx(0.5).margin(1e-5));

        for (const auto &mg : dest->storage.getPatch().modulation_global)
        {
            if (!isModulatorDistinctPerScene((modsources)mg.source_id))
            {
                REQUIRE(mg.source_scene == 0);
            }
        }
    }

    SECTION("A Scene LFO To FX Routing Keeps Its Scene")
    {
        // the flip side; scene LFOs really do exist once per scene, so #2285 still holds
        auto src = Surge::Headless::createSurge(44100);
        auto dest = Surge::Headless::createSurge(44100);

        auto pidx = reverbInAIns1(src);
        auto *fxp = &(src->storage.getPatch().fx[fxslot_ains1].p[pidx]);

        REQUIRE(src->setModDepth01(fxp->id, ms_slfo1, 1, 0, 0.5));

        fromto(src, dest);

        auto *dfxp = &(dest->storage.getPatch().fx[fxslot_ains1].p[pidx]);

        REQUIRE(dest->isAnyActiveModulation(dfxp->id, ms_slfo1, 1));
        REQUIRE_FALSE(dest->isAnyActiveModulation(dfxp->id, ms_slfo1, 0));
    }

    SECTION("Pasting A Scene Does Not Scene Tag A Macro Routing")
    {
        // copying scene A onto scene B used to stamp the paste scene onto every global
        // routing it carried across, which recreated the bug in a current build
        auto surge = Surge::Headless::createSurge(44100);

        auto pidx = reverbInAIns1(surge);
        auto *fxp = &(surge->storage.getPatch().fx[fxslot_ains1].p[pidx]);

        REQUIRE(surge->setModDepth01(fxp->id, ms_ctrl7, 0, 0, 0.5));
        REQUIRE(surge->isAnyActiveModulation(fxp->id, ms_ctrl7, 0));

        auto isValid = [&surge](int id, modsources ms) { return surge->isValidModulation(id, ms); };

        surge->storage.clipboard_copy(cp_scene, 0, -1);
        surge->storage.clipboard_paste(cp_scene, 1, -1, ms_original, isValid);

        for (const auto &mg : surge->storage.getPatch().modulation_global)
        {
            if (!isModulatorDistinctPerScene((modsources)mg.source_id))
            {
                REQUIRE(mg.source_scene == 0);
            }
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

    SECTION("extraoscdata with an oversized extra_n")
    {
        // extra_n is the loop bound for writes into a fixed max_config array and
        // it arrives from the file, so a patch could ask us to write far past the
        // end of it. Patches get shared and downloaded, so opening one is enough.
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><extraoscdata>"
                         "<od osc=\"0\" scene=\"0\" extra_n=\"100000\"/>"
                         "</extraoscdata></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);

        auto &ec = surge->storage.getPatch().scene[0].osc[0].extraConfig;
        REQUIRE(ec.nData == (int)OscillatorStorage::ExtraConfigurationData::max_config);
    }

    SECTION("extraoscdata with a negative extra_n")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><extraoscdata>"
                         "<od osc=\"0\" scene=\"0\" extra_n=\"-5\"/>"
                         "</extraoscdata></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);

        auto &ec = surge->storage.getPatch().scene[0].osc[0].extraConfig;
        REQUIRE(ec.nData == 0);
    }

    SECTION("msegs with out of range scene and index")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><msegs>"
                         "<mseg scene=\"99\" i=\"99\"/><mseg scene=\"-1\" i=\"-1\"/>"
                         "</msegs></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        SUCCEED("an out of range mseg scene or index did not write outside the patch");
    }

    SECTION("formulae with out of range scene and index")
    {
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><formulae>"
                         "<formula scene=\"99\" i=\"99\"/><formula scene=\"-1\" i=\"-1\"/>"
                         "</formulae></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        SUCCEED("an out of range formula scene or index did not write outside the patch");
    }

    SECTION("lfo bank labels with out of range indices or no value")
    {
        // three indices and the label text all come from the file
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><lfobanklabels>"
                         "<label scene=\"99\" lfo=\"99\" idx=\"99\" v=\"x\"/>"
                         "<label scene=\"-1\" lfo=\"-1\" idx=\"-1\" v=\"x\"/>"
                         "<label scene=\"0\" lfo=\"0\" idx=\"0\"/>"
                         "</lfobanklabels></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        SUCCEED("out of range label indices and a missing value were both discarded");
    }

    SECTION("extraoscdata with out of range scene and osc")
    {
        // scene and osc index fixed arrays of n_scenes and n_oscs, and both come
        // from the file without being checked against those bounds.
        auto surge = Surge::Headless::createSurge(44100);
        std::string test{"<patch><parameters/><extraoscdata>"
                         "<od osc=\"99\" scene=\"99\" extra_n=\"1\"/>"
                         "<od osc=\"-1\" scene=\"-1\" extra_n=\"1\"/>"
                         "</extraoscdata></patch>"};
        surge->storage.getPatch().load_xml(test.c_str(), test.size(), false);
        SUCCEED("an out of range scene or osc index did not write outside the patch");
    }
}

namespace
{
// A wavetable with entirely distinct, never-zero samples, so a re-slice can be checked for
// exact content preservation and the trailing-silence trim has nothing to latch on to.
void buildRampWT(Wavetable *wt, int frames, int frameSize)
{
    std::vector<float> data((size_t)frames * frameSize);

    for (size_t i = 0; i < data.size(); ++i)
    {
        data[i] = (float)(i + 1) / (float)data.size();
    }

    wt_header wh;

    memset(&wh, 0, sizeof(wt_header));
    wh.n_samples = frameSize;
    wh.n_tables = frames;
    wh.flags = 0;

    REQUIRE(wt->BuildWT(data.data(), wh, false));
}

// The ramp above is all positive, which the wavetable oscillator's integrator turns into a
// long lived offset - no good for asking whether a voice has actually gone quiet. This one
// is zero mean per frame.
void buildSineWT(Wavetable *wt, int frames, int frameSize)
{
    std::vector<float> data((size_t)frames * frameSize);

    for (int f = 0; f < frames; ++f)
    {
        for (int k = 0; k < frameSize; ++k)
        {
            data[(size_t)f * frameSize + k] = std::sin(2.0 * M_PI * k / (double)frameSize);
        }
    }

    wt_header wh;

    memset(&wh, 0, sizeof(wt_header));
    wh.n_samples = frameSize;
    wh.n_tables = frames;
    wh.flags = 0;

    REQUIRE(wt->BuildWT(data.data(), wh, false));
}
} // namespace

TEST_CASE("Wavetables Can Be Resliced At Runtime", "[io]")
{
    SECTION("Frame size drives frame count")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        const auto before = wt.FlattenSource();
        REQUIRE(before.size() == 512);

        REQUIRE(wt.Reslice(32, -1, wt.flags));
        REQUIRE(wt.size == 32);
        REQUIRE(wt.n_tables == 16);
        REQUIRE(wt.SourceFrameCount() == 16);

        // Same samples, just cut up differently
        REQUIRE(wt.FlattenSource() == before);

        REQUIRE(wt.Reslice(16, -1, wt.flags));
        REQUIRE(wt.size == 16);
        REQUIRE(wt.n_tables == 32);
        REQUIRE(wt.FlattenSource() == before);
    }

    SECTION("A non-power-of-two frame size rounds down")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        REQUIRE(wt.Reslice(60, -1, wt.flags));
        REQUIRE(wt.size == 32);
        REQUIRE(wt.n_tables == 16);
    }

    SECTION("A frame size below the floor clamps rather than failing")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        REQUIRE(wt.Reslice(2, -1, wt.flags));
        REQUIRE(wt.size == Wavetable::min_reslice_size);
    }

    SECTION("Reducing the frame count truncates the tail")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        const auto before = wt.FlattenSource();

        REQUIRE(wt.Reslice(-1, 5, wt.flags));
        REQUIRE(wt.size == 64);
        REQUIRE(wt.n_tables == 5);

        const auto after = wt.FlattenSource();
        REQUIRE(after.size() == 320);
        REQUIRE(std::equal(after.begin(), after.end(), before.begin()));
    }

    SECTION("Growing the frame count past the sample budget shrinks the frame size")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        // 20 frames will not fit at 64, nor at 32, but does at 16
        REQUIRE(wt.Reslice(-1, 20, wt.flags));
        REQUIRE(wt.size == 16);
        REQUIRE(wt.n_tables == 20);
        REQUIRE(wt.FlattenSource().size() == 320);
    }

    SECTION("A frame count that cannot be reached even at the floor is capped")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        // 512 samples at the 16 sample floor is 32 frames, so 400 is unreachable
        REQUIRE(wt.Reslice(-1, 400, wt.flags));
        REQUIRE(wt.size == Wavetable::min_reslice_size);
        REQUIRE(wt.n_tables == 32);
    }

    SECTION("Sample mode round trips without accumulating padding")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        const auto before = wt.FlattenSource();

        REQUIRE(wt.Reslice(-1, -1, wt.flags | wtf_is_sample));
        REQUIRE(wt.flags & wtf_is_sample);
        // BuildWT appends three silent frames for a sample
        REQUIRE(wt.n_tables == 11);
        // ...which SourceFrameCount and FlattenSource both see through
        REQUIRE(wt.SourceFrameCount() == 8);
        REQUIRE(wt.FlattenSource() == before);

        // Toggling sample mode repeatedly must not grow the table each time
        for (int i = 0; i < 4; ++i)
        {
            REQUIRE(wt.Reslice(-1, -1, wt.flags & ~wtf_is_sample));
            REQUIRE(wt.n_tables == 8);
            REQUIRE(wt.FlattenSource() == before);

            REQUIRE(wt.Reslice(-1, -1, wt.flags | wtf_is_sample));
            REQUIRE(wt.n_tables == 11);
            REQUIRE(wt.FlattenSource() == before);
        }
    }

    SECTION("A sample leaves room for the padding inside max_subtables")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        REQUIRE(wt.Reslice(-1, max_subtables, wt.flags | wtf_is_sample));
        REQUIRE(wt.n_tables <= max_subtables);
        REQUIRE(wt.SourceFrameCount() <= max_subtables - 3);
    }

    SECTION("The encoding flags are cleared, since we rebuild from float")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);
        wt.flags |= wtf_int16 | wtf_int16_is_16;

        REQUIRE(wt.Reslice(32, -1, wt.flags));
        REQUIRE((wt.flags & wtf_int16) == 0);
        REQUIRE((wt.flags & wtf_int16_is_16) == 0);
    }

    SECTION("An unbuilt wavetable refuses to reslice")
    {
        Wavetable wt;
        REQUIRE(!wt.Reslice(64, -1, 0));
    }
}

TEST_CASE("Queued Wavetable Reslices Run On The Audio Thread", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    auto *osc = &(surge->storage.getPatch().scene[0].osc[0]);
    osc->queue_type = ot_wavetable;

    // Let the startup wavetable load settle, so the reslice is the only thing queued
    for (int i = 0; i < 5; ++i)
        surge->process();

    auto &wt = osc->wt;
    REQUIRE(wt.queue_id == -1);
    REQUIRE(wt.queue_filename.empty());

    buildRampWT(&wt, 8, 64);
    wt.current_id = 3;
    osc->wavetable_display_name = "Some Wavetable";

    wt.reslice_size = 32;
    wt.reslice_frames = -1;
    wt.reslice_flags = wt.flags | wtf_user_modified;
    wt.queue_reslice = true;

    surge->storage.perform_queued_wtloads();

    REQUIRE(!wt.queue_reslice);
    REQUIRE(wt.size == 32);
    REQUIRE(wt.n_tables == 16);

    // Detached from the wt_list entry, but the name it was loaded under is kept
    REQUIRE(wt.current_id == -1);
    REQUIRE(wt.flags & wtf_user_modified);
    REQUIRE(osc->wavetable_display_name == "Some Wavetable");
    REQUIRE(wt.refresh_display);
}

TEST_CASE("A Resliced Wavetable Survives A Patch Round Trip", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    auto *osc = &(surge->storage.getPatch().scene[0].osc[0]);
    osc->type.val.i = ot_wavetable;

    // The headless storage has no wavetable library, so stand one entry up by hand. The
    // patch loader re-attaches a nameless-id wavetable to the list entry its display name
    // matches, and this is the entry a re-sliced table must NOT be re-attached to.
    Patch entry;
    entry.name = "Reslice Test WT";
    entry.path = "reslice-test.wt";
    surge->storage.wt_list.push_back(entry);

    osc->wavetable_display_name = entry.name;

    auto &wt = osc->wt;
    buildRampWT(&wt, 8, 64);
    REQUIRE(wt.Reslice(32, -1, wt.flags | wtf_is_sample | wtf_loop_sample | wtf_user_modified));

    const auto expected = wt.FlattenSource();
    const auto expectedTables = wt.n_tables;

    void *data = nullptr;
    auto sz = surge->storage.getPatch().save_patch(&data);
    REQUIRE(sz > 0);
    surge->storage.getPatch().load_patch(data, sz, false);

    auto &rt = surge->storage.getPatch().scene[0].osc[0].wt;

    REQUIRE(rt.size == 32);
    REQUIRE(rt.n_tables == expectedTables);
    REQUIRE(rt.flags & wtf_is_sample);
    REQUIRE(rt.flags & wtf_loop_sample);
    REQUIRE(rt.flags & wtf_user_modified);
    // Still detached, so undo restores the edit rather than reloading the original file
    REQUIRE(rt.current_id == -1);

    // ...whereas without the modified bit the same patch does get re-attached, which is
    // what the bit exists to suppress
    rt.flags &= ~wtf_user_modified;

    void *plain = nullptr;
    auto psz = surge->storage.getPatch().save_patch(&plain);
    REQUIRE(psz > 0);
    surge->storage.getPatch().load_patch(plain, psz, false);

    REQUIRE(surge->storage.getPatch().scene[0].osc[0].wt.current_id == 0);

    // The patch blob is int16, so this is a lossy but faithful round trip
    const auto got = rt.FlattenSource();
    REQUIRE(got.size() == expected.size());

    for (size_t i = 0; i < got.size(); ++i)
    {
        REQUIRE(got[i] == Approx(expected[i]).margin(1e-3));
    }
}

TEST_CASE("Looped Samples Keep Sounding", "[dsp]")
{
    auto playAndMeasureTail = [](bool loop) {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge.get());

        auto *osc = &(surge->storage.getPatch().scene[0].osc[0]);
        osc->queue_type = ot_wavetable;

        for (int i = 0; i < 5; ++i)
            surge->process();

        auto &wt = osc->wt;
        buildSineWT(&wt, 8, 1024);

        int flags = wt.flags | wtf_is_sample;

        if (loop)
        {
            flags |= wtf_loop_sample;
        }

        REQUIRE(wt.Reslice(-1, -1, flags));

        // A one voice unison seeds sampleloop with 1, so an unlooped sample plays once and
        // stops. Without pinning it the unison count would be doing the looping for us.
        osc->p[WavetableOscillator::wt_unison_voices].val.i = 1;

        surge->playNote(0, 60, 127, 0);

        float tail = 0;

        for (int q = 0; q < 200; ++q)
        {
            surge->process();

            if (q > 100)
            {
                for (int s = 0; s < BLOCK_SIZE; ++s)
                    tail += fabs(surge->output[0][s]);
            }
        }

        return tail;
    };

    const auto oneShot = playAndMeasureTail(false);
    const auto looped = playAndMeasureTail(true);

    // Eight frames at 261 Hz is about 1300 samples, so by block 100 an unlooped sample is
    // long finished and only the DC blocker's decay is left
    REQUIRE(looped > 1.f);
    REQUIRE(oneShot < looped * 0.01f);
}

TEST_CASE("A Rejected Wavetable Header Leaves The Wavetable Alone", "[io]")
{
    SECTION("BuildWT keeps the previous table on a bad header")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        const auto before = wt.FlattenSource();

        wt_header wh;

        memset(&wh, 0, sizeof(wt_header));
        wh.n_samples = max_wtable_size * 2;
        wh.n_tables = 1;
        wh.flags = 0;

        std::vector<float> junk((size_t)wh.n_samples, 0.5f);

        REQUIRE(!wt.BuildWT(junk.data(), wh, false));

        // Crucially it does not now claim a size its buffers cannot back
        REQUIRE(wt.size == 64);
        REQUIRE(wt.n_tables == 8);
        REQUIRE(wt.everBuilt);
        REQUIRE(wt.FlattenSource() == before);
    }

    SECTION("Too many frames is rejected the same way")
    {
        Wavetable wt;
        buildRampWT(&wt, 8, 64);

        wt_header wh;

        memset(&wh, 0, sizeof(wt_header));
        wh.n_samples = 64;
        wh.n_tables = max_subtables + 1;
        wh.flags = 0;

        std::vector<float> junk((size_t)wh.n_samples * wh.n_tables, 0.5f);

        REQUIRE(!wt.BuildWT(junk.data(), wh, false));
        REQUIRE(wt.size == 64);
        REQUIRE(wt.n_tables == 8);
    }
}

TEST_CASE("A Patch With An Unbuildable Wavetable Does Not Leave The Oscillator Empty", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());

    auto *osc = &(surge->storage.getPatch().scene[0].osc[0]);
    osc->type.val.i = ot_wavetable;

    // Big enough that a corrupted header still claims fewer bytes than the blob holds,
    // so it gets past the size guard in load_patch and reaches BuildWT
    buildRampWT(&osc->wt, 16, 2048);

    void *data = nullptr;
    auto sz = surge->storage.getPatch().save_patch(&data);
    REQUIRE(sz > 0);

    // Take a copy, since save_patch owns its buffer and we are about to scribble on it
    std::vector<char> blob((char *)data, (char *)data + sz);

    auto *ph = (sst::io::patch_header *)blob.data();
    const auto xmlsize = mech::endian_read_int32LE(ph->xmlsize);
    auto *wth = (wt_header *)(blob.data() + sizeof(sst::io::patch_header) + xmlsize);

    REQUIRE(mech::endian_read_int32LE(wth->n_samples) == 2048);

    // A frame size BuildWT must refuse
    wth->n_samples = mech::endian_write_int32LE(max_wtable_size * 2);
    wth->n_tables = mech::endian_write_int16LE(1);

    surge->storage.getPatch().load_patch(blob.data(), (int)blob.size(), false);

    auto &rt = surge->storage.getPatch().scene[0].osc[0].wt;

    // Either the previous table survived or a default was queued, but the oscillator is
    // never left describing a table it does not have
    REQUIRE(rt.size != max_wtable_size * 2);
    REQUIRE((rt.everBuilt || rt.queue_id == 0));

    surge->storage.perform_queued_wtloads();
    REQUIRE(rt.everBuilt);

    // And it can still be saved without tripping the assert in save_patch
    void *again = nullptr;
    REQUIRE(surge->storage.getPatch().save_patch(&again) > 0);
}
