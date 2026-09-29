// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the browser's real pending-publication storage, without an audio
// consumer, so this check cannot accidentally pass after adoption already ran.
#include "SurgeStorage.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <vector>
#if !SURGE_WEB
#error "This check requires browser wavetable publication ownership"
#endif

static void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
static void build(Wavetable &table, unsigned size, unsigned short frames, float value)
{
    wt_header header{{'v','a','w','t'}, size, frames, 0};
    std::vector<float> values(size * frames, value);
    require(table.BuildWT(values.data(), header, false), "Unable to build test table");
}
static void verify(const Wavetable &table, int size, int frames, float value)
{
    require(table.everBuilt && table.size == size && table.n_tables == frames,
            "Snapshot dimensions differ");
    for (int frame = 0; frame < frames; ++frame)
        for (int sample = 0; sample < size; ++sample)
            require(table.TableF32WeakPointers[0][frame][sample] == value,
                    "Snapshot sample differs");
}
int main()
{
    try
    {
        fs::create_directories("/user");
        fs::create_directories("/factory");
        auto storage = std::make_unique<SurgeStorage>("/factory");
        storage->browserManagesWavetables = true;
        auto &live = storage->getPatch().scene[0].osc[0].wt;
        build(live, 64, 2, 0.125f);
        auto replacement = std::make_unique<Wavetable>();
        build(*replacement, 128, 3, -0.25f);
        replacement->current_id = 17;
        replacement->current_filename = "Restored.wt";
        replacement->frame_size_if_absent = 128;
        storage->replaceOscillatorWavetable(0, 0, *replacement);
        auto &pending = storage->browserGeneratedWavetables[0];
        require(pending && pending->controlReplacement &&
                    pending->state.load() == SurgeStorage::BrowserGeneratedWavetable::pending,
                "Replacement was not queued");
        verify(live, 64, 2, 0.125f);
        auto captured = std::make_unique<Wavetable>();
        storage->copyOscillatorWavetable(0, 0, *captured);
        verify(*captured, 128, 3, -0.25f);
        require(captured->current_id == 17 && captured->current_filename == "Restored.wt" &&
                    captured->frame_size_if_absent == 128, "Snapshot metadata differs");
        require(storage->export_wt_wt_portable("/user/captured.wt", captured.get(), ""),
                "Snapshot export failed");
        auto imported = std::make_unique<Wavetable>();
        storage->load_wt("/user/captured.wt", imported.get(), nullptr);
        verify(*imported, 128, 3, -0.25f);

        build(*replacement, 64, 1, 0.5f);
        storage->replaceOscillatorWavetable(0, 0, *replacement);
        auto latest = std::make_unique<Wavetable>();
        storage->copyOscillatorWavetable(0, 0, *latest);
        verify(*latest, 64, 1, 0.5f);
        verify(*captured, 128, 3, -0.25f); // Picker retains its independent snapshot.
        verify(live, 64, 2, 0.125f);

        ++storage->wtGenPublishToken[0]; // Superseded publication cannot become current.
        storage->copyOscillatorWavetable(0, 0, *latest);
        verify(*latest, 64, 2, 0.125f);
        pending->publishToken = storage->wtGenPublishToken[0].load();
        pending->controlReplacement = false; // Worker result awaiting committed metadata.
        storage->copyOscillatorWavetable(0, 0, *latest);
        verify(*latest, 64, 2, 0.125f);
        live.swapData(*pending->table);
        pending->state.store(SurgeStorage::BrowserGeneratedWavetable::published);
        storage->copyOscillatorWavetable(0, 0, *latest);
        verify(*latest, 64, 1, 0.5f);
        verify(*captured, 128, 3, -0.25f);
        std::puts("Pending, superseded, worker and adopted wavetable snapshots preserve export data");
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
