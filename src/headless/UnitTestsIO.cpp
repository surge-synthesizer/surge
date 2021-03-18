#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "HeadlessUtils.h"
#include "Player.h"

#include "catch2/catch2.hpp"

#include "UnitTestUtilities.h"
#include "DeferredAssetLoader.h"
#include <chrono>
#include <thread>

#include <unordered_map>

using namespace Surge::Test;
using namespace std::chrono_literals;

TEST_CASE("We can read a collection of wavetables", "[io]")
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

    SECTION("Wavetable.wav")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("test-data/wav/Wavetable.wav", wt);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 256);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }

    SECTION("05_BELL.WAV")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("test-data/wav/05_BELL.WAV", wt);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 33);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }

    SECTION("pluckalgo.wav")
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        surge->storage.load_wt_wav_portable("test-data/wav/pluckalgo.wav", wt);
        REQUIRE(wt->size == 2048);
        REQUIRE(wt->n_tables == 9);
        REQUIRE((wt->flags & wtf_is_sample) == 0);
    }
}

TEST_CASE("All .wt and .wav factory assets load", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());
    for (auto p : surge->storage.wt_list)
    {
        auto wt = &(surge->storage.getPatch().scene[0].osc[0].wt);
        wt->size = -1;
        wt->n_tables = -1;
        surge->storage.load_wt(path_to_string(p.path), wt,
                               &(surge->storage.getPatch().scene[0].osc[0]));
        REQUIRE(wt->size > 0);
        REQUIRE(wt->n_tables > 0);
    }
}

TEST_CASE("All Patches are Loadable", "[io]")
{
    auto surge = Surge::Headless::createSurge(44100);
    REQUIRE(surge.get());
    int i = 0;
    for (auto p : surge->storage.patch_list)
    {
        INFO("Loading patch [" << p.name << "] from ["
                               << surge->storage.patch_category[p.category].name << "]");
        surge->loadPatch(i);
        ++i;

        // A tiny oddity that the surge state pops up if we have tuning patches in the library so
        surge->storage.remapToConcertCKeyboard();
        surge->storage.retuneTo12TETScaleC261Mapping();
    }
}

TEST_CASE("DAW Streaming and Unstreaming", "[io][mpe][tun]")
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
        Tunings::Scale s = Tunings::readSCLFile("test-data/scl/zeus22.scl");

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

    SECTION("Save and Restore KBM")
    {
        auto surgeSrc = Surge::Headless::createSurge(44100);
        auto surgeDest = Surge::Headless::createSurge(44100);

        auto k = Tunings::readKBMFile("test-data/scl/mapping-a440-constant.kbm");

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

    SECTION("Save and Restore Param Midi Controls - Simple")
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

    SECTION("Save and Restore Param Midi Controls - Empty")
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

    SECTION("Save and Restore Param Midi Controls - Multi")
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

    SECTION("Save and Restore Custom Controllers")
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

TEST_CASE("Stream WaveTable Names", "[io]")
{
    SECTION("Name Restored for Old Patch")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);
        REQUIRE(surge->loadPatchByPath("test-data/patches/Church.fxp", -1, "Test"));
        REQUIRE(std::string(surge->storage.getPatch().scene[0].osc[0].wavetable_display_name) ==
                "(Patch Wavetable)");
    }

    SECTION("Name Set when Loading Factory")
    {
        auto surge = Surge::Headless::createSurge(44100);
        REQUIRE(surge);

        auto patch = &(surge->storage.getPatch());
        patch->scene[0].osc[0].type.val.i = ot_wavetable;

        for (int i = 0; i < 40; ++i)
        {
            int wti = rand() % surge->storage.wt_list.size();
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

        auto surgeS = Surge::Headless::createSurge(44100);
        auto surgeD = Surge::Headless::createSurge(44100);
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
                        int wti = rand() % surgeS->storage.wt_list.size();
                        surgeS->storage.load_wt(wti, &patch->scene[s].osc[o].wt,
                                                &patch->scene[s].osc[o]);
                        REQUIRE(std::string(patch->scene[s].osc[o].wavetable_display_name) ==
                                surgeS->storage.wt_list[wti].name);

                        if (1.0 * rand() / RAND_MAX > 0.8)
                        {
                            auto sn = std::string("renamed blurg ") + std::to_string(rand());
                            strncpy(patch->scene[s].osc[o].wavetable_display_name, sn.c_str(), 256);
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

TEST_CASE("Load Patches with Embedded KBM")
{
    std::vector<std::string> patches = {};
    SECTION("Check Restore")
    {
        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath("test-data/patches/HasKBM.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(!surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath("test-data/patches/HasSCL.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath("test-data/patches/HasSCLandKBM.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(!surge->storage.isStandardMapping);
        }

        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath("test-data/patches/HasSCL_165Vintage.fxp", -1, "Test");
            REQUIRE(!surge->storage.isStandardTuning);
            REQUIRE(surge->storage.isStandardMapping);
        }
    }
}

TEST_CASE("IDs are Stable", "[io]")
{
#define GENERATE_ID_TO_STDOUT 0
#if GENERATE_ID_TO_STDOUT
    SECTION("GENERATE IDS")
    {
        auto surge = Surge::Headless::createSurge(44100);
        auto patch = &(surge->storage.getPatch());

        int np = patch->param_ptr.size();
        std::cout << "param_ptr_size=" << np << std::endl;

        for (int i = 0; i < np; ++i)
        {
            std::cout << "name=" << patch->param_ptr[i]->get_storage_name() << "|id="
                      << patch->param_ptr[i]->id
                      // << "|scene=" << patch->param_ptr[i]->scene
                      << std::endl;
        }
    }
#endif

    SECTION("Compare IDs")
    {
        std::string idfile = "test-data/param-ids-1.6.5.txt";
        INFO("Comparing current surge with " << idfile);

        // Step one: Read the file
        std::ifstream fp(idfile.c_str());
        std::string line;

        auto splitOnPipeEquals = [](std::string s) {
            std::vector<std::string> bars;
            size_t barpos;
            std::unordered_map<std::string, std::string> res;
            while ((barpos = s.find("|")) != std::string::npos)
            {
                auto f = s.substr(0, barpos);
                s = s.substr(barpos + 1);
                bars.push_back(f);
            }
            bars.push_back(s);
            for (auto b : bars)
            {
                auto eqpos = b.find("=");
                REQUIRE(eqpos != std::string::npos);
                auto n = b.substr(0, eqpos);
                auto v = b.substr(eqpos + 1);
                res[n] = v;
            }
            return res;
        };

        std::unordered_map<std::string, int> nameIdMap;
        while (std::getline(fp, line))
        {
            if (line.find("name=", 0) == 0)
            {
                auto m = splitOnPipeEquals(line);
                REQUIRE(m.find("name") != m.end());
                REQUIRE(m.find("id") != m.end());
                auto n = m["name"];
                nameIdMap[n] = std::atoi(m["id"].c_str());
            }
        }

        auto surge = Surge::Headless::createSurge(44100);
        auto patch = &(surge->storage.getPatch());

        int np = patch->param_ptr.size();
        for (int i = 0; i < np; ++i)
        {
            std::string sn = patch->param_ptr[i]->get_storage_name();
            int id = patch->param_ptr[i]->id;
            INFO("Comparing " << sn << " with id " << id);
            REQUIRE(nameIdMap.find(sn) != nameIdMap.end());
            REQUIRE(nameIdMap[sn] == id);
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
                oss << "test-data/patches/all-filters/s14/filt_" << i << "_" << j << ".fxp";
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
                oss << "test-data/patches/all-filters/s15/filt_" << i << "_" << j << ".fxp";
                auto p = string_to_path(oss.str());
                surge->savePatchToPath(p);
            }
        }
    }
#endif

    auto p14 = string_to_path("test-data/patches/all-filters/s14");
    for (auto ent : fs::directory_iterator(p14))
    {
        DYNAMIC_SECTION("Test SV14 Filter " << path_to_string(ent))
        {
            auto surge = Surge::Headless::createSurge(44100);
            surge->loadPatchByPath(path_to_string(ent).c_str(), -1, "TEST");
            surge->process();
            auto ft = surge->storage.getPatch().scene[0].filterunit[0].type.val.i;
            auto st = surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i;
            for (int s = 0; s < n_scenes; ++s)
            {
                for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                {
                    INFO(path_to_string(ent) << " " << ft << " " << st << " " << s << " " << fu)
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].type.val.i == ft);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i == st);
                }
            }

            INFO("Patch for filter " << fut_names[ft]);
            if (ff_revision == 14)
            {
                std::ostringstream cand_fn;
                cand_fn << "filt_" << ft << "_" << st << ".fxp";
                auto entfn = path_to_string(ent.path().filename());
                REQUIRE(entfn == cand_fn.str());
            }
            else if (ff_revision > 14)
            {
                fu_type fft = (fu_type)ft;
                int fnft = ft;
                int fnst = st;
                switch (fft)
                {
                case fut_none:
                case fut_lp12:
                case fut_lp24:
                case fut_lpmoog:
                case fut_hp12:
                case fut_hp24:
                case fut_SNH:
                case fut_vintageladder:
                case fut_obxd_4pole:
                case fut_k35_lp:
                case fut_k35_hp:
                case fut_diode:
                case fut_cutoffwarp_lp:
                case fut_cutoffwarp_hp:
                case fut_cutoffwarp_n:
                case fut_cutoffwarp_bp:
                case n_fu_types:
                    // These types were unchanged
                    break;
                    // These are the types which changed 14 -> 15
                case fut_comb_pos:
                    fnft = fut_14_comb;
                    fnst = st;
                    break;
                case fut_comb_neg:
                    fnft = fut_14_comb;
                    fnst = st + 2;
                    break;
                case fut_obxd_2pole_lp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 0;
                    break;
                case fut_obxd_2pole_bp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 1;
                    break;
                case fut_obxd_2pole_hp:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 2;
                    break;
                case fut_obxd_2pole_n:
                    fnft = fut_14_obxd_2pole;
                    fnst = st * 4 + 3;
                    break;
                case fut_notch12:
                    fnft = fut_14_notch12;
                    fnst = st;
                    break;
                case fut_notch24:
                    fnft = fut_14_notch12;
                    fnst = st + 2;
                    break;
                case fut_bp12:
                    fnft = fut_14_bp12;
                    fnst = st;
                    break;
                case fut_bp24:
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

    auto p15 = string_to_path("test-data/patches/all-filters/s15");
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
            for (int s = 0; s < n_scenes; ++s)
            {
                for (int fu = 0; fu < n_filterunits_per_scene; ++fu)
                {
                    INFO(path_to_string(ent) << " " << ft << " " << st << " " << s << " " << fu)
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].type.val.i == ft);
                    REQUIRE(surge->storage.getPatch().scene[s].filterunit[fu].subtype.val.i == st);
                }
            }

            std::ostringstream cand_fn;
            cand_fn << "filt_" << ft << "_" << st << ".fxp";
            auto entfn = path_to_string(ent.path().filename());
            REQUIRE(entfn == cand_fn.str());
        }
    }
}

TEST_CASE("MonoVoicePriority Streams", "[io]")
{
    auto fromto = [](std::shared_ptr<SurgeSynthesizer> src,
                     std::shared_ptr<SurgeSynthesizer> dest) {
        void *d = nullptr;
        auto sz = src->saveRaw(&d);

        dest->loadRaw(d, sz, false);
    };
    SECTION("MVP Streams Properly")
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

#if BUILD_DEFERRED_ASSET_LOADER
TEST_CASE("Deferred Asset Loader", "[io]")
{
    auto skipThisTest = (getenv("SURGE_DISABLE_NETWORK_TESTS") != nullptr);
    if (skipThisTest)
    {
        SECTION("Skipping Network Tests") { REQUIRE(skipThisTest); }
        return;
    }

    SECTION("Retrieve a single image")
    {
        auto surge = Surge::Headless::createSurge(44100);

        auto dal = Surge::Storage::DeferredAssetLoader(&(surge->storage));
        REQUIRE(surge);

        std::string testUrl = "https://surge-synthesizer.github.io/assets/images/test/Music.png";
        // In the event we have a documents cache already make the name random. Add time later
        testUrl += "?rand=" + std::to_string(rand());

        auto n = std::chrono::system_clock::now();
        auto es = std::chrono::duration_cast<std::chrono::seconds>(n.time_since_epoch());
        testUrl += "&time=" + std::to_string(es.count());

        INFO("Trying test URL " << testUrl)
        REQUIRE(!dal.hasCachedCopyOf(testUrl));

        INFO("Retrieving " << testUrl);
        std::atomic<bool> got(false);
        dal.retrieveSingleAsset(
            testUrl, [&got](const Surge::Storage::DeferredAssetLoader::url_t &) { got = true; },
            [](auto &u, const std::string &msg) { REQUIRE(false); });

        int tries = 2; // make this longer before I commit
        while (tries > 0 && !got)
        {
            INFO("Try " << tries)
            std::this_thread::sleep_for(1000ms);
            tries--;
        }

        REQUIRE(got);
        REQUIRE(dal.hasCachedCopyOf(testUrl));
        REQUIRE(fs::exists(dal.pathOfCachedCopy(testUrl)));
    }

    SECTION("Retrieve a missing image")
    {
        auto surge = Surge::Headless::createSurge(44100);

        auto dal = Surge::Storage::DeferredAssetLoader(&(surge->storage));
        REQUIRE(surge);

        std::string testUrl =
            "https://surge-synthesizer.github.io/assets/images/test/NOT_THERE_AT_ALL.png";
        // In the event we have a documents cache already make the name random. Add time later
        testUrl += "?rand=" + std::to_string(rand());

        auto n = std::chrono::system_clock::now();
        auto es = std::chrono::duration_cast<std::chrono::seconds>(n.time_since_epoch());
        testUrl += "&time=" + std::to_string(es.count());

        INFO("Trying test URL " << testUrl)
        REQUIRE(!dal.hasCachedCopyOf(testUrl));

        INFO("Retrieving " << testUrl);
        std::atomic<bool> got(false), erred(false);
        std::string msg;
        dal.retrieveSingleAsset(
            testUrl, [&got](const Surge::Storage::DeferredAssetLoader::url_t &) { got = true; },
            [&erred, &msg](auto &u, const std::string &em) {
                msg = em;
                erred = true;
            });

        int tries = 20;
        while (tries > 0 && !(got || erred))
        {
            std::this_thread::sleep_for(1000ms);
            tries--;
        }

        REQUIRE(!got);
        REQUIRE(erred);
        REQUIRE(!dal.hasCachedCopyOf(testUrl));
    }
}
#endif