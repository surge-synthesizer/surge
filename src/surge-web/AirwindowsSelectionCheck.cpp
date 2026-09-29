// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeStorage.h"
#include "dsp/effects/airwindows/AirWindowsEffect.h"
#include "AllocationProbe.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <cstring>
#include <thread>
#include <chrono>
static void require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
static void checkMailboxThreads()
{
    struct Request { int sequence{}; };
    struct Prepared
    {
        int sequence;
        std::thread::id owner;
        std::atomic<unsigned> &live, &destroyed;
        std::atomic<bool> &wrongOwner;
        Prepared(int seq, std::atomic<unsigned> &l, std::atomic<unsigned> &d, std::atomic<bool> &w)
            : sequence(seq), owner(std::this_thread::get_id()), live(l), destroyed(d), wrongOwner(w)
        { live.fetch_add(1); }
        ~Prepared()
        {
            if (owner != std::this_thread::get_id()) wrongOwner = true;
            live.fetch_sub(1);
            destroyed.fetch_add(1);
        }
    };
    ControlPreparationMailbox<Request, Prepared> mailbox;
    std::atomic<bool> stop{false}, wrongOwner{false};
    std::atomic<unsigned> live{0}, destroyed{0};
    unsigned constructed = 0, highWater = 0;
    std::thread control([&] {
        auto factory = [&](const Request &request) {
            if (request.sequence % 17 == 0) throw std::runtime_error("Injected preparation failure");
            auto result = std::make_unique<Prepared>(request.sequence, live, destroyed, wrongOwner);
            ++constructed;
            highWater = std::max(highWater, live.load());
            return result;
        };
        while (!stop.load())
        {
            try { mailbox.service(factory); } catch (const std::runtime_error &) {}
            std::this_thread::yield();
        }
        mailbox.service(factory); // Reclaim the final consumed result on control.
    });
    bool valid = true, timedOut = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    AllocationProbe::begin();
    for (int sequence = 1; sequence <= 2000 && !timedOut; ++sequence)
    {
        while (!mailbox.submit({sequence}))
        {
            if (std::chrono::steady_clock::now() > deadline) { timedOut = true; break; }
            std::this_thread::yield();
        }
        if (timedOut) break;
        if (mailbox.submit({-1})) valid = false; // Capacity is exactly one request.
        bool received = false;
        while (!received)
        {
            mailbox.consume([&](const Request &request, Prepared *prepared) noexcept {
                valid &= request.sequence == sequence;
                valid &= sequence % 17 == 0 ? prepared == nullptr
                                            : prepared && prepared->sequence == sequence;
                received = true;
            });
            if (std::chrono::steady_clock::now() > deadline) { timedOut = true; break; }
            std::this_thread::yield();
        }
    }
    const bool noHeap = AllocationProbe::end();
    stop = true;
    control.join();
    require(valid && !timedOut && noHeap && !wrongOwner && live == 0 &&
                highWater == 1 && constructed == destroyed,
            "Threaded mailbox lost ownership, exceeded capacity, failed recovery or touched audio heap");
    std::puts("Mailbox: 2000 threaded requests, injected failures, bounded ownership and control-only reclamation passed");
}

int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    try
    {
        AllocationProbe::begin();
        auto *control = ::operator new(128);
        ::operator delete(control);
        AllocationProbe::end();
        require(AllocationProbe::allocations == 1 && AllocationProbe::deallocations == 1,
                "Allocation probe failed its control");
        checkMailboxThreads();
        auto storage = std::make_unique<SurgeStorage>(argv[1]);
        const auto count = AirWinBaseClass::pluginRegistry().size();
        unsigned checked = 0;
        for (float rate : {44100.f, 48000.f})
        for (int variant = 0; variant < count; ++variant)
        {
            storage->setSamplerate(rate);
            auto actualParameters = std::make_unique<FxStorage>(storage->getPatch().fx[0]);
            auto referenceParameters = std::make_unique<FxStorage>(*actualParameters);
            auto actualValues = std::make_unique<std::array<pdata, n_global_params>>();
            auto referenceValues = std::make_unique<std::array<pdata, n_global_params>>();
            AirWindowsEffect actual(storage.get(), actualParameters.get(), actualValues->data());
            AirWindowsEffect reference(storage.get(), referenceParameters.get(), referenceValues->data());
            const int previous = (variant + 1) % count;
            for (auto *effect : {&actual, &reference})
            {
                auto &parameters = effect == &actual ? *actualParameters : *referenceParameters;
                effect->init_ctrltypes();
                effect->init_default_values();
                parameters.p[0].val.i = previous;
                effect->setupSubFX(previous, false);
                for (int i = 0; i < n_fx_params - 1; ++i)
                {
                    effect->param_lags[i].newValue(.21f + i * .01f);
                    effect->param_lags[i].instantize();
                    effect->param_lags[i].newValue(.37f + i * .01f);
                    parameters.p[i + 1].val.f = .123456f;
                    parameters.p[i + 1].midictrl = 10 + i;
                    parameters.p[i + 1].midichan = i % 4;
                    parameters.p[i + 1].temposync = i % 2;
                    parameters.p[i + 1].absolute = i % 3;
                    parameters.p[i + 1].deform_type = i;
                    parameters.p[i + 1].oscName = std::string(512, 'x');
                }
            }
            for (const auto &p : actualParameters->p) (*actualValues)[p.id] = p.val;
            for (const auto &p : referenceParameters->p) (*referenceValues)[p.id] = p.val;
            auto *oldProcessor = actual.airwin.get();
            for (int invalid : {-1, static_cast<int>(count)})
            {
                bool rejected = false;
                try { actual.prepareSelection(invalid); }
                catch (const std::out_of_range &) { rejected = true; }
                require(rejected && actual.airwin.get() == oldProcessor,
                        "Invalid selector changed the committed processor");
            }
            AllocationProbe::begin();
            const auto request = actual.captureSelectionRequest(variant);
            const bool captureNoHeap = AllocationProbe::end();
            require(captureNoHeap, "Selection snapshot touched the C++ heap");
            // The control-side basis deliberately has stale values and flags.
            // Also alter live values after capture: preparation must use the
            // request rather than reading the currently mutable parameter array.
            auto basis = std::make_unique<FxStorage>(*actualParameters);
            for (int i = 0; i < n_fx_params; ++i)
            {
                basis->p[i].val.f = .75f;
                basis->p[i].midictrl = 0;
                basis->p[i].midichan = 0;
                basis->p[i].temposync = !basis->p[i].temposync;
                basis->p[i].absolute = !basis->p[i].absolute;
                basis->p[i].deform_type = -1;
                basis->p[i].oscName.clear();
                if (i) actualParameters->p[i].val.f = .876543f;
            }
            basis->p[1].id++;
            bool rejectedSlot = false;
            try { actual.prepareSelection(*basis, request); }
            catch (const std::invalid_argument &) { rejectedSlot = true; }
            require(rejectedSlot, "Preparation accepted metadata from another slot");
            basis->p[1].id--;
            auto selection = actual.prepareSelection(*basis, request);
            for (int i = 0; i < n_fx_params; ++i)
                request.parameters[i].apply(actualParameters->p[i]);
            auto obsolete = request;
            obsolete.sampleRate += 1;
            bool rejectedRate = false;
            try { actual.prepareSelection(*basis, obsolete); }
            catch (const std::out_of_range &) { rejectedRate = true; }
            require(rejectedRate, "Preparation accepted an obsolete sample rate");
            storage->setSamplerate(rate + 1);
            require(!actual.adoptSelection(*selection), "Adoption accepted an obsolete sample rate");
            storage->setSamplerate(rate);
            require(actual.lastSelected == previous && actual.airwin.get() == oldProcessor &&
                        actualParameters->p[0].val.i == previous,
                    "Control preparation modified the committed effect");
            require(!reference.adoptSelection(*selection), "Another effect accepted the preparation");
            AllocationProbe::begin();
            const bool adopted = actual.adoptSelection(*selection);
            const bool noHeap = AllocationProbe::end();
            require(adopted && noHeap, "Prepared adoption failed or touched the C++ heap");
            require(selection->processor.get() == oldProcessor && selection->consumed &&
                        !actual.adoptSelection(*selection), "Outgoing ownership or single adoption failed");
            for (int i = 0; i < n_fx_params - 1; ++i)
                require(actual.param_lags[i].v == reference.param_lags[i].v &&
                            actual.param_lags[i].target_v == reference.param_lags[i].target_v &&
                            actual.param_lags[i].first_run == reference.param_lags[i].first_run,
                        "Prepared adoption reset smoothing history");
            selection.reset(); // Reclaim the old processor/metadata off the callback.
            referenceParameters->p[0].val.i = variant;
            alignas(16) std::array<float, BLOCK_SIZE> left{}, right{}, refLeft{}, refRight{};
            reference.process(refLeft.data(), refRight.data());
            AllocationProbe::begin();
            actual.process(left.data(), right.data());
            const bool processNoHeap = AllocationProbe::end();
            require(processNoHeap, "First prepared processing block touched the C++ heap");
            for (int i = 0; i < n_fx_params; ++i)
            {
                const auto &a = actualParameters->p[i], &b = referenceParameters->p[i];
                require(a.ctrltype == b.ctrltype && a.valtype == b.valtype &&
                            a.val.i == b.val.i && a.val_min.i == b.val_min.i &&
                            a.val_max.i == b.val_max.i && a.val_default.i == b.val_default.i &&
                            std::strcmp(a.get_name(), b.get_name()) == 0 &&
                            a.deactivated == b.deactivated && a.midictrl == b.midictrl &&
                            a.midichan == b.midichan && a.temposync == b.temposync &&
                            a.absolute == b.absolute && a.deform_type == b.deform_type &&
                            a.oscName == b.oscName,
                        "Prepared selector metadata differs from native direct selection");
            }
            for (int i = 0; i < n_fx_params - 1; ++i)
                require(actual.param_lags[i].v == reference.param_lags[i].v &&
                            actual.param_lags[i].target_v == reference.param_lags[i].target_v &&
                            actual.param_lags[i].first_run == reference.param_lags[i].first_run,
                        "Prepared first-block smoothing differs from native direct selection");
            for (int i = 0; i < actual.airwin->paramCount; ++i)
                require(actualParameters->p[i + 1].user_data == actual.fxFormatters[i].get(),
                        "Prepared metadata retained a temporary formatter");
            for (int i = 0; i < BLOCK_SIZE; ++i)
                require(std::isfinite(left[i]) && std::isfinite(right[i]), "Non-finite prepared output");
            bool finished = false;
            const int queued = (variant + 1) % count;
            AllocationProbe::begin();
            require(!actual.processBrowserSelection(queued, finished) && !finished,
                    "A queued selection was adopted before preparation");
            require(AllocationProbe::end(), "Mailbox request capture touched the C++ heap");
            actual.serviceBrowserSelection(*basis);
            actualParameters->p[1].midictrl++;
            AllocationProbe::begin();
            require(!actual.processBrowserSelection(queued, finished) && !finished,
                    "A preparation overwrote a subsequent MIDI assignment edit");
            require(AllocationProbe::end(), "Stale request rejection touched the C++ heap");
            actual.serviceBrowserSelection(*basis); // Reclaim the rejected preparation.
            actual.processBrowserSelection(queued, finished); // Capture the new state.
            actual.serviceBrowserSelection(*basis);
            AllocationProbe::begin();
            const bool queuedAdoption = actual.processBrowserSelection(queued, finished);
            const bool queuedNoHeap = AllocationProbe::end();
            require(queuedAdoption && finished && queuedNoHeap && actual.lastSelected == queued,
                    "Prepared mailbox adoption failed or touched the C++ heap");
            require(actualParameters->p[1].midictrl == request.parameters[1].midictrl + 1,
                    "Recaptured selection lost the MIDI assignment edit");
            actual.serviceBrowserSelection(*basis); // Control owns outgoing destruction.
            auto obsoleteSelection = actual.prepareSelection((variant + 1) % count);
            actualParameters->p[0].val.i = (variant + 2) % count;
            actual.setupSubFX(actualParameters->p[0].val.i, false);
            require(!actual.adoptSelection(*obsoleteSelection),
                    "An intervening processor change did not invalidate its old preparation");
            ++checked;
        }
        std::printf("Prepared Airwindows selection: %u cases match native defaults and smoothing, retain formatter ownership, and capture/adopt/process without C++ heap operations\n", checked);
        return 0;
    }
    catch (const std::exception &e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
