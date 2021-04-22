#include "HeadlessUtils.h"
#include "Player.h"
#include "filesystem/import.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <deque>

namespace Surge
{
namespace Headless
{
namespace NonTest
{

void restreamTemplatesWithModifications()
{
    auto templatesDir = string_to_path("resources/data/patches_factory/Templates");
    for (auto d : fs::directory_iterator(templatesDir))
    {
        std::cout << "ReStream " << path_to_string(d) << std::endl;
        auto surge = Surge::Headless::createSurge(44100);
        surge->loadPatchByPath(path_to_string(d).c_str(), -1, "Templates");
        for (int i = 0; i < 10; ++i)
            surge->process();

        auto oR = surge->storage.getPatch().streamingRevision;
        std::cout << "  Stream Revision : " << oR << " vs " << ff_revision << std::endl;
        if (oR < 15)
        {
            std::cout << "  Fixing Comb Filter" << std::endl;
            surge->storage.getPatch().correctlyTuneCombFilter = true;
        }

        if (oR == ff_revision)
        {
            std::cout << "  Patch is streamed at latest; skipping" << std::endl;
        }
        else
        {
            for (int i = 0; i < 10; ++i)
                surge->process();

            surge->savePatchToPath(d);
        }
    }
}

void statsFromPlayingEveryPatch()
{
    /*
    ** This is a very clean use of the built in APIs, just making a surge
    ** and a scale then asking headless to map it onto every patch
    ** and call us back with a result
    */
    auto surge = Surge::Headless::createSurge(44100);

    Surge::Headless::playerEvents_t scale =
        Surge::Headless::make120BPMCMajorQuarterNoteScale(0, 44100);

    auto callBack = [](const Patch &p, const PatchCategory &pc, const float *data, int nSamples,
                       int nChannels) -> void {
        bool writeWav = false; // toggle this to true to write each sample to a wav file
        std::cout << "cat/patch = " << pc.name << " / " << std::left << std::setw(30) << p.name;

        if (nSamples * nChannels > 0)
        {
            const auto minmaxres = std::minmax_element(data, data + nSamples * nChannels);
            auto mind = minmaxres.first;
            auto maxd = minmaxres.second;

            float rms = 0, L1 = 0;
            for (int i = 0; i < nSamples * nChannels; ++i)
            {
                rms += data[i] * data[i];
                L1 += fabs(data[i]);
            }
            L1 = L1 / (nChannels * nSamples);
            rms = sqrt(rms / nChannels / nSamples);

            std::cout << "  range = [" << std::setw(10) << std::fixed << *mind << ", "
                      << std::setw(10) << std::fixed << *maxd << "]"
                      << " L1=" << L1 << " rms=" << rms << " samp=" << nSamples
                      << " chan=" << nChannels;
            if (writeWav)
            {
                std::ostringstream oss;
                oss << "headless " << pc.name << " " << p.name << ".wav";
                Surge::Headless::writeToWav(data, nSamples, nChannels, 44100, oss.str());
            }
        }
        std::cout << std::endl;
    };

    Surge::Headless::playOnEveryPatch(surge, scale, callBack);
}

void standardCutoffCurve(int ft, int sft, std::ostream &os)
{
    /*
     * This is the frequency response graph
     */
    std::array<std::vector<float>, 127> ampRatios;
    std::array<std::vector<float>, 127> phases;
    std::vector<float> resonances;
    bool firstTime = true;

    std::string sectionheader = "";
    for (float res = 0; res <= 1.0; res += 0.2)
    {
        res = limit_range(res, 0.f, 0.99f);
        resonances.push_back(res);
        /*
         * Our strategy here is to use a sin oscilllator in both in dual mode
         * read off the L chan audio from both scenes and then calculate the RMS
         * and phase difference. I hope
         */
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scenemode.val.i = sm_dual;
        surge->storage.getPatch().scene[0].filterunit[0].type.val.i = ft;
        surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = sft;
        surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = 0;
        surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = res;

        surge->storage.getPatch().scene[0].osc[0].type.val.i = ot_sine;

        surge->storage.getPatch().scene[1].filterunit[0].type.val.i = fut_none;
        surge->storage.getPatch().scene[1].filterunit[0].subtype.val.i = 0;
        surge->storage.getPatch().scene[1].osc[0].type.val.i = ot_sine;

        auto proc = [surge](int n) {
            for (int i = 0; i < n; ++i)
                surge->process();
        };
        proc(10);

        if (firstTime)
        {
            firstTime = false;
            char fn[256], st[256], con[256], ron[256];
            surge->storage.getPatch().scene[0].filterunit[0].type.get_display(fn);
            surge->storage.getPatch().scene[0].filterunit[0].subtype.get_display(st);
            surge->storage.getPatch().scene[0].filterunit[0].cutoff.get_display(con);
            surge->storage.getPatch().scene[0].filterunit[0].resonance.get_display(ron);
            std::ostringstream oss;
            oss << fn << " (" << st << ") @ cutoff=" << con;
            sectionheader = oss.str();
        }
        for (int i = 0; i < 127; ++i)
        {
            proc(50); // let silence reign
            surge->playNote(0, i, 127, 0);

            // OK so we want probably 5000 samples or so
            const int n_blocks = 1024;
            const int n_samples = n_blocks * BLOCK_SIZE;
            float ablock[n_samples];
            float bblock[n_samples];

            for (int b = 0; b < n_blocks; ++b)
            {
                surge->process();
                memcpy(ablock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[0][0][0]),
                       BLOCK_SIZE * sizeof(float));
                memcpy(bblock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[1][0][0]),
                       BLOCK_SIZE * sizeof(float));
            }

            float rmsa = 0, rmsb = 0;
            for (int s = 0; s < n_samples; ++s)
            {
                rmsa += ablock[s] * ablock[s];
                rmsb += bblock[s] * bblock[s];
            }

            /*
             * Lets estimate phase shift. This only works in az tighter range
             */
            float phaseShift = 0;
            if (i > 0 && i < 127)
            {
                int startAt = (int)(n_samples * 0.3);
                int averageOver = (int)(n_samples * 0.3);
                std::vector<float> startsA, startsB;
                for (int s = startAt; s < startAt + averageOver; ++s)
                {
                    if (ablock[s] < 0 && ablock[s + 1] > 0)
                    {
                        auto pct = (0.f - ablock[s]) / (ablock[s + 1] - ablock[s]);
                        startsA.push_back(s + pct);
                    }

                    if (bblock[s] < 0 && bblock[s + 1] > 0)
                    {
                        auto pct = (0.f - bblock[s]) / (bblock[s + 1] - bblock[s]);
                        startsB.push_back(s + pct);
                    }
                }

                auto phaseCuts = std::min(startsA.size(), startsB.size());
                std::vector<float> dPhases;
                for (auto s = 1; s < phaseCuts; ++s)
                {
                    float dPhase = (startsA[s] - startsB[s]) / (startsB[s] - startsB[s - 1]);
                    if (dPhase > 1.0)
                        dPhase -= 1;
                    else if (dPhase < 0)
                        dPhase += 1;
                    dPhases.push_back(dPhase);
                }

                float mDP = 0;
                for (auto v : dPhases)
                    mDP += v;
                mDP /= dPhases.size();
                float vDP = 0;
                for (auto v : dPhases)
                    vDP += (v - mDP) * (v - mDP);
                vDP = sqrt(vDP / dPhases.size());
                if (vDP < .1 && mDP >= 0 && mDP <= 1.0)
                {
                    phaseShift = mDP;
                }
                // std::cout << i << " Mean and Variance phase " << mDP << " " << vDP << std::endl;
            }

            surge->releaseNote(0, i, 0);
            ampRatios[i].push_back(rmsa / rmsb);
            phases[i].push_back(phaseShift);
        }
    }

    os << "[BEGIN]" << std::endl;
    os << "[SECTION]" << sectionheader << "[/SECTION]" << std::endl;
    os << "[YLAB]RMS Filter / Signal[/YLAB]" << std::endl;
    os << "[YLOG]True[/YLOG]" << std::endl;

    os << "Note";
    for (auto r : resonances)
        os << ", res " << r;
    os << std::endl;

    for (int i = 0; i < 127; ++i)
    {
        os << i;
        for (auto r : ampRatios[i])
            os << ", " << r;
        os << std::endl;
    }
    os << "[END]" << std::endl;

    os << "[BEGIN]" << std::endl;
    os << "[SECTION]" << sectionheader << "[/SECTION]" << std::endl;
    os << "[YLAB]Phase Difference / 2 PI[/YLAB]" << std::endl;
    os << "Note";
    for (auto r : resonances)
        os << ", res " << r;
    os << std::endl;

    for (int i = 0; i < 127; ++i)
    {
        os << i;
        for (auto r : phases[i])
            os << ", " << r;
        os << std::endl;
    }
    os << "[END]" << std::endl;
}

void middleCSawIntoFilterVsCutoff(int ft, int sft, std::ostream &os)
{
    /*
     * This is the frequency response graph
     */
    const int nNotes = 40;
    const int note0 = 20;
    const int dNote = 2;
    std::array<std::vector<float>, nNotes> ampRatios;
    std::vector<float> resonances;
    bool firstTime = true;

    std::string sectionheader = "";
    for (float res = 0; res <= 1.0; res += 0.2)
    {
        res = limit_range(res, 0.f, 0.99f);
        resonances.push_back(res);
        /*
         * Our strategy here is to use a sin oscilllator in both in dual mode
         * read off the L chan audio from both scenes and then calculate the RMS
         * and phase difference. I hope
         */
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scenemode.val.i = sm_dual;
        surge->storage.getPatch().scene[0].filterunit[0].type.val.i = ft;
        surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = sft;
        surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = res;

        surge->storage.getPatch().scene[1].filterunit[0].type.val.i = fut_none;
        surge->storage.getPatch().scene[1].filterunit[0].subtype.val.i = 0;

        auto proc = [surge](int n) {
            for (int i = 0; i < n; ++i)
                surge->process();
        };
        proc(10);

        if (firstTime)
        {
            firstTime = false;
            char fn[256], st[256], con[256], ron[256];
            surge->storage.getPatch().scene[0].filterunit[0].type.get_display(fn);
            surge->storage.getPatch().scene[0].filterunit[0].subtype.get_display(st);
            std::ostringstream oss;
            oss << fn << " (" << st << ") Cutoff Sweep on Middle C" << con;
            sectionheader = oss.str();
        }
        for (int n = 0; n < nNotes; ++n)
        {
            proc(50);
            int note = n * dNote + note0;

            surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = note - 69;

            proc(50); // let silence reign
            surge->playNote(0, 60, 127, 0);

            // OK so we want probably 5000 samples or so
            const int n_blocks = 1024;
            const int n_samples = n_blocks * BLOCK_SIZE;
            float ablock[n_samples];
            float bblock[n_samples];

            for (int b = 0; b < n_blocks; ++b)
            {
                surge->process();
                memcpy(ablock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[0][0][0]),
                       BLOCK_SIZE * sizeof(float));
                memcpy(bblock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[1][0][0]),
                       BLOCK_SIZE * sizeof(float));
            }

            float rmsa = 0, rmsb = 0;
            for (int s = 0; s < n_samples; ++s)
            {
                rmsa += ablock[s] * ablock[s];
                rmsb += bblock[s] * bblock[s];
            }

            surge->releaseNote(0, 60, 0);
            ampRatios[n].push_back(rmsa / rmsb);
        }
    }

    os << "[BEGIN]" << std::endl;
    os << "[SECTION]" << sectionheader << "[/SECTION]" << std::endl;
    os << "[YLAB]RMS Filter / Signal[/YLAB]" << std::endl;
    os << "[YLOG]True[/YLOG]" << std::endl;

    os << "Cutoff";
    for (auto r : resonances)
        os << ", res " << r;
    os << std::endl;

    for (int n = 0; n < nNotes; ++n)
    {
        os << n * dNote + note0;
        for (auto r : ampRatios[n])
            os << ", " << r;
        os << std::endl;
    }
    os << "[END]" << std::endl;
}

void middleCSawIntoFilterVsReso(int ft, int sft, std::ostream &os)
{
    /*
     * This is the frequency response graph
     */
    const int nR = 21;
    float dr = 1.f / (nR - 1);
    std::array<std::vector<float>, nR> ampRatios;
    std::vector<float> cutoffs;
    bool firstTime = true;

    std::string sectionheader = "";
    for (int co = 0; co < 6; co++)
    {
        int conote = co * 12 + 69 - 36;

        cutoffs.push_back(440 * pow(2, (conote - 69.f) / 12.f));
        /*
         * Our strategy here is to use a sin oscilllator in both in dual mode
         * read off the L chan audio from both scenes and then calculate the RMS
         * and phase difference. I hope
         */
        auto surge = Surge::Headless::createSurge(48000);
        surge->storage.getPatch().scenemode.val.i = sm_dual;
        surge->storage.getPatch().scene[0].filterunit[0].type.val.i = ft;
        surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = sft;
        surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = conote;

        surge->storage.getPatch().scene[1].filterunit[0].type.val.i = fut_none;
        surge->storage.getPatch().scene[1].filterunit[0].subtype.val.i = 0;

        auto proc = [surge](int n) {
            for (int i = 0; i < n; ++i)
                surge->process();
        };
        proc(10);

        if (firstTime)
        {
            firstTime = false;
            char fn[256], st[256], con[256], ron[256];
            surge->storage.getPatch().scene[0].filterunit[0].type.get_display(fn);
            surge->storage.getPatch().scene[0].filterunit[0].subtype.get_display(st);
            std::ostringstream oss;
            oss << fn << " (" << st << ") Cutoff Sweep on Middle C" << con;
            sectionheader = oss.str();
        }
        for (int r = 0; r < nR; ++r)
        {
            float res = limit_range(r * dr, 0.f, 0.99f);

            proc(50);

            surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = res;

            proc(50); // let silence reign
            surge->playNote(0, 60, 127, 0);

            // OK so we want probably 5000 samples or so
            const int n_blocks = 1024;
            const int n_samples = n_blocks * BLOCK_SIZE;
            float ablock[n_samples];
            float bblock[n_samples];

            for (int b = 0; b < n_blocks; ++b)
            {
                surge->process();
                memcpy(ablock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[0][0][0]),
                       BLOCK_SIZE * sizeof(float));
                memcpy(bblock + b * BLOCK_SIZE, (const void *)(&surge->sceneout[1][0][0]),
                       BLOCK_SIZE * sizeof(float));
            }

            float rmsa = 0, rmsb = 0;
            for (int s = 0; s < n_samples; ++s)
            {
                rmsa += ablock[s] * ablock[s];
                rmsb += bblock[s] * bblock[s];
            }

            surge->releaseNote(0, 60, 0);
            ampRatios[r].push_back(rmsa / rmsb);
        }
    }

    os << "[BEGIN]" << std::endl;
    os << "[SECTION]" << sectionheader << "[/SECTION]" << std::endl;
    os << "[YLAB]RMS Filter / Signal[/YLAB]" << std::endl;
    os << "[YLOG]True[/YLOG]" << std::endl;

    os << "Resonance";
    for (auto r : cutoffs)
        os << ", cut= " << r;
    os << std::endl;

    for (int n = 0; n < nR; ++n)
    {
        os << limit_range(n * dr, 0.f, 0.99f);
        for (auto r : ampRatios[n])
            os << ", " << r;
        os << std::endl;
    }
    os << "[END]" << std::endl;
}

void filterAnalyzer(int ft, int sft, std::ostream &os)
{
    standardCutoffCurve(ft, sft, os);
    middleCSawIntoFilterVsCutoff(ft, sft, os);
    middleCSawIntoFilterVsReso(ft, sft, os);
}

[[noreturn]] void performancePlay(const std::string &patchName, int mode)
{
    auto surge = Surge::Headless::createSurge(48000);
    std::cout << "Performance Mode with surge at 48k\n"
              << "-- Ctrl-C to exit\n"
              << "-- patchName = " << patchName << "\n"
              << "-- mode = " << mode << std::endl;

    surge->loadPatchByPath(patchName.c_str(), -1, "RUNTIME");

#if SINE_STRESS_TEST
    /*
     * This was for my sine stress testing
     */
    for (int s = 0; s < n_scenes; ++s)
        for (int o = 0; o < n_oscs; ++o)
            surge->storage.getPatch().scene[s].osc[o].p[0].val.i = mode;
#endif

    for (int i = 0; i < 10; ++i)
        surge->process();

    surge->playNote(0, 60, 127, 0);

    int ct = 0;
    int nt = 0;
    int noteOnEvery = 48000 / BLOCK_SIZE / 10;
    std::deque<int> notesOn;
    notesOn.push_back(60);

#define PLAY_NOTES 1

    int target = 48000 / BLOCK_SIZE;
    auto cpt = std::chrono::high_resolution_clock::now();
    std::chrono::seconds oneSec(1);
    auto msOne = std::chrono::duration_cast<std::chrono::microseconds>(oneSec).count();
    while (true)
    {
        surge->process();
#if PLAY_NOTES

        if (nt++ == noteOnEvery)
        {
            int nextNote = notesOn.back() + 1;
            if (notesOn.size() == 10)
            {
                auto removeNote = notesOn.front();
                notesOn.pop_front();
                surge->releaseNote(0, removeNote, 0);
                nextNote = removeNote - 1;
            }
            if (nextNote < 10)
                nextNote = 120;
            if (nextNote > 121)
                nextNote = 10;

            notesOn.push_back(nextNote);
            surge->playNote(0, nextNote, 127, 0);
            nt = 0;
        }
#endif
        if (ct++ == target)
        {
            auto et = std::chrono::high_resolution_clock::now();
            auto diff = et - cpt;
            auto ms = std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
            double pct = 1.0 * ms / msOne * 100.0;

            float sum = 0;
            for (int i = 0; i < BLOCK_SIZE; ++i)
                sum += surge->output[0][i] * surge->output[0][i];

            std::cout << "Resetting timers " << ms << "ms / " << pct << "% L2N=" << sum
                      << std::endl;
            ct = 0;
            cpt = et;
        }
    }
}

void generateNLFeedbackNorms()
{
    /*
     * This generates a normalization for the NL feedback. Here's how you use it
     *
     * In filters/NonlinearFeedback.cpp if there's a normalization table there
     * disable it and make sure the law is just 1/f^1/3
     *
     * Build and run this with surge-headless --non-test --generate-nlf-norms
     *
     * Copy and paste the resulting table back into NLFB.cpp
     *
     * What it does is: Play a C major chord on init into the OBXD-24 LP for 3 seconds
     * with resonance at 1, calculate the RMS
     *
     * Then do that for every subtype of NLF and calculate the RMS
     *
     * Then print out ratio basically.
     */

    bool genLowpass = false; // if false generate highpass

    auto setup = [](int type, int subtype) {
        auto surge = Surge::Headless::createSurge(48000);
        for (auto i = 0; i < 10; ++i)
        {
            surge->process();
        }
        surge->storage.getPatch().scene[0].filterunit[0].type.val.i = type;
        surge->storage.getPatch().scene[0].filterunit[0].subtype.val.i = subtype;
        surge->storage.getPatch().scene[0].filterunit[0].cutoff.val.f = 30;
        surge->storage.getPatch().scene[0].filterunit[0].resonance.val.f = 0.95;
        for (auto i = 0; i < 10; ++i)
        {
            surge->process();
        }
        char td[256], sd[256], cd[256];

        surge->storage.getPatch().scene[0].filterunit[0].type.get_display(td);
        surge->storage.getPatch().scene[0].filterunit[0].subtype.get_display(sd);
        surge->storage.getPatch().scene[0].filterunit[0].cutoff.get_display(cd);
        std::cout << "// Created a filter: " << td << " / " << sd << " Cutoff=" << cd << std::endl;
        return surge;
    };

    auto singleRMS = [genLowpass](std::shared_ptr<SurgeSynthesizer> surge) {
        for (auto i = 0; i < 10; ++i)
        {
            surge->process();
        }

        std::vector<int> notes = {{48, 60, 65, 67}};
        int noff = -12;
        if (!genLowpass)
            noff = +15;
        for (auto n : notes)
        {
            surge->playNote(0, n - noff, 127, 0);
            surge->process();
        }

        for (int i = 0; i < 100; ++i)
            surge->process();

        int blocks = (floor)(2.0 * samplerate * BLOCK_SIZE_INV);
        double rms = 0; // double to avoid so much overflow risk
        for (int i = 0; i < blocks; ++i)
        {
            surge->process();
            for (int s = 0; s < BLOCK_SIZE; ++s)
            {
                rms += surge->output[0][s] * surge->output[0][s] +
                       surge->output[1][s] * surge->output[1][s];
            }
        }

        for (auto n : notes)
        {
            surge->releaseNote(0, n - noff, 0);
            surge->process();
        }
        for (int i = 0; i < 100; ++i)
            surge->process();

        return sqrt(rms);
    };

    auto playRMS = [singleRMS](std::shared_ptr<SurgeSynthesizer> surge) {
        float ra = 0;
        int nAvg = 20;

        for (int i = 0; i < nAvg; ++i)
        {
            float lra = singleRMS(surge);
            std::cout << "LRA = " << lra << std::endl;
            ra += lra;
        }
        return ra / nAvg;
    };

    // Remember if you don't set the normalization to FALSE in NLF.cpp this will be bad
    auto basecaseRMS = playRMS(setup(genLowpass ? fut_obxd_4pole : fut_hp24, genLowpass ? 3 : 0));
    std::cout << "// no filter RMS is " << basecaseRMS << std::endl;

    std::vector<float> results;
    for (int nlfst = 0; nlfst < 12; nlfst++)
    {
        auto r = playRMS(setup(genLowpass ? fut_cutoffwarp_lp : fut_cutoffwarp_hp, nlfst));
        // auto r = playRMS( setup( fut_cutoffwarp_hp, nlfst));
        std::cout << "// RMS=" << r << " Ratio=" << r / basecaseRMS << std::endl;
        results.push_back(r);
    }
    std::cout << "      bool useNormalization = true;\n"
              << "      float normNumerator = 1.0f;\n"
              << "      constexpr float " << (genLowpass ? "lp" : "hp") << "NormTable[12] = {";
    std::string pfx = "\n         ";
    for (auto v : results)
    {
        std::cout << pfx << basecaseRMS / v;
        pfx = ",\n         ";
    }
    std::cout << "\n      };\n"
              << "      if (useNormalization) normNumerator = lpNormTable[subtype];\n";
}

} // namespace NonTest
} // namespace Headless
} // namespace Surge
