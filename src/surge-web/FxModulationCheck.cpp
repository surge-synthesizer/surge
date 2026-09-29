// SPDX-License-Identifier: GPL-3.0-or-later
#include "SurgeSynthesizer.h"
#include "AllocationProbe.h"
#include <array>
#include <cstdio>
#include <stdexcept>
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
struct Parent : SurgeSynthesizer::PluginLayer
{
    void surgeParameterUpdated(const SurgeSynthesizer::ID &, float) override {}
    void surgeMacroUpdated(long, float) override {}
};
struct Listener : SurgeSynthesizer::ModulationAPIListener
{
    std::array<ModulationRouting, n_fx_params * 6> removed{};
    std::size_t count{};
    bool overflow{};
    void modSet(long, modsources, int, int, float, bool) override {}
    void modMuted(long, modsources, int, int, bool) override {}
    void modCleared(long id, modsources source, int scene, int index) override
    {
        if (count == removed.size()) { overflow = true; return; }
        auto &r = removed[count++];r.destination_id=id;r.source_id=source;r.source_scene=scene;r.source_index=index;
    }
};
int main(int argc, char **argv)
{
    if (argc != 2) return 2;
    try
    {
        AllocationProbe::begin();
        auto *probe = ::operator new(128);
        ::operator delete(probe);
        AllocationProbe::end();
        require(AllocationProbe::allocations == 1 && AllocationProbe::deallocations == 1,
                "Allocation probe did not observe its control");
        Parent parent;
        auto synth = std::make_unique<SurgeSynthesizer>(&parent, argv[1]);
        auto &patch = synth->storage.getPatch();
        Listener listener;
        synth->modListeners.insert(&listener);
        for (int slot = 0; slot < n_fx_slots; ++slot)
        {
            auto &routes = patch.modulation_global;routes.clear();
            std::array<ModulationRouting, n_fx_params * 6> expected{};std::size_t count=0;
            for (auto &p : patch.fx[slot].p)
            {
                p.set_type(ct_none); // Old getModulationIndicesBetween skips this destination.
                for (int scene=0;scene<2;++scene)for(int index=0;index<3;++index)
                {
                    ModulationRouting r{};r.destination_id=p.id;r.source_id=ms_slfo1;
                    r.source_scene=scene;r.source_index=index;r.depth=.25f;r.muted=index==1;
                    routes.push_back(r);expected[count++]=r;
                }
            }
            ModulationRouting unrelated{};unrelated.destination_id=patch.fx[(slot+1)%n_fx_slots].p[0].id;
            unrelated.source_id=ms_slfo1;unrelated.source_scene=1;unrelated.source_index=2;unrelated.depth=.5f;
            routes.insert(routes.begin()+3,unrelated);
            const auto capacity=routes.capacity();listener.count=0;listener.overflow=false;patch.isDirty=false;
            AllocationProbe::begin();
            synth->clearFxModulation(slot);
            const bool noAllocation=AllocationProbe::end();
            require(noAllocation,"FX route cleanup touched the C++ heap");
            require(routes.capacity()==capacity&&routes.size()==1&&routes[0].destination_id==unrelated.destination_id&&routes[0].depth==unrelated.depth&&
                    routes[0].source_id==unrelated.source_id&&routes[0].source_scene==unrelated.source_scene&&
                    routes[0].source_index==unrelated.source_index&&routes[0].muted==unrelated.muted,
                    "FX cleanup altered unrelated routes or released vector storage");
            require(patch.isDirty&&listener.count==count&&!listener.overflow,"Missing clear notification or dirty state");
            for(std::size_t i=0;i<count;++i)
                require(listener.removed[i].destination_id==expected[i].destination_id&&listener.removed[i].source_id==expected[i].source_id&&
                        listener.removed[i].source_scene==expected[i].source_scene&&listener.removed[i].source_index==expected[i].source_index,
                        "Clear notification changed destination, source, scene or index");
            listener.count=0;patch.isDirty=false;
            synth->clearFxModulation(slot);synth->clearFxModulation(-1);synth->clearFxModulation(n_fx_slots);
            require(listener.count==0&&!patch.isDirty&&routes.size()==1,"Empty or invalid cleanup changed state");
        }
        synth->modListeners.erase(&listener);
        std::puts("FX routing: inactive destinations, both scenes, indices, notifications and unrelated routes passed without C++ heap operations");
        return 0;
    }
    catch(const std::exception &e){std::fprintf(stderr,"%s\n",e.what());return 1;}
}
