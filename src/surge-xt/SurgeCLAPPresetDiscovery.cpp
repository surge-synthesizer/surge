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

#if HAS_CLAP_JUCE_EXTENSIONS

#include <iostream>
#include <memory>
#include <cstring>
#include <fstream>
#include <vector>

#include <clap/clap.h>

#include "SurgeStorage.h"
#include "SurgeSynthProcessor.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"
#include "PatchFileHeaderStructs.h"

namespace sst::surge_xt::preset_discovery
{
#define PSOUT std::cout << "[preset] " << __LINE__ << " " << __func__ << " "

static const clap_preset_discovery_provider_descriptor desc{
    CLAP_VERSION, "org.surge-synth-team.surge-xt.preset-indexer", "Surge XT Presets",
    "Surge Synth Team"};

struct PresetProvider
{
    const clap_preset_discovery_indexer *indexer{nullptr};
    std::unique_ptr<SurgeStorage> storage;

    PresetProvider(const clap_preset_discovery_indexer *idx) : indexer(idx)
    {
        provider.desc = &desc;
        provider.provider_data = this;
        provider.init = [](auto a) {
            auto p = reinterpret_cast<PresetProvider *>(a->provider_data);
            return p->init();
        };
        provider.destroy = [](auto a) {
            auto p = reinterpret_cast<PresetProvider *>(a->provider_data);
            delete p;
        };
        provider.get_metadata = [](auto a, auto k, const auto *l, auto *m) {
            auto p = reinterpret_cast<PresetProvider *>(a->provider_data);
            return p->get_metadata(k, l, m);
        };
        provider.get_extension = [](auto a, auto b) {
            auto p = reinterpret_cast<PresetProvider *>(a->provider_data);
            return p->get_extension(b);
        };
    }

    ~PresetProvider() {}

    bool init()
    {
        SurgeStorage::SurgeStorageConfig config;
        config.createUserDirectory = false;

        storage = std::make_unique<SurgeStorage>(config);

        auto res = true;
        auto fxp = clap_preset_discovery_filetype{"Surge XT Patch", "", "fxp"};
        res = res && indexer->declare_filetype(indexer, &fxp);

        // PATH_MAX is unreliably available
        static constexpr int pathSize{8192};
        if (fs::is_directory(storage->datapath / "patches_factory"))
        {
            char floc[pathSize];
            strncpy(floc, (storage->datapath / "patches_factory").u8string().c_str(), pathSize - 1);
            auto factory = clap_preset_discovery_location{
                CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT, "Surge XT Factory Presets",
                CLAP_PRESET_DISCOVERY_LOCATION_FILE, floc};
            res = res && indexer->declare_location(indexer, &factory);
        }

        if (fs::is_directory(storage->datapath / "patches_3rdparty"))
        {
            char tploc[pathSize];
            strncpy(tploc, (storage->datapath / "patches_3rdparty").u8string().c_str(),
                    pathSize - 1);
            auto third_party = clap_preset_discovery_location{
                CLAP_PRESET_DISCOVERY_IS_FACTORY_CONTENT, "Surge XT Third Party Presets",
                CLAP_PRESET_DISCOVERY_LOCATION_FILE, tploc};
            res = res && indexer->declare_location(indexer, &third_party);
        }

        if (fs::is_directory(storage->userPatchesPath))
        {
            char uloc[pathSize];
            strncpy(uloc, storage->userPatchesPath.u8string().c_str(), pathSize - 1);

            auto userpatch = clap_preset_discovery_location{
                CLAP_PRESET_DISCOVERY_IS_USER_CONTENT, "Surge XT User Presets",
                CLAP_PRESET_DISCOVERY_LOCATION_FILE, uloc};
            res = res && indexer->declare_location(indexer, &userpatch);
        }
        return res;
    }

    bool get_metadata(uint32_t location_kind, const char *location,
                      const clap_preset_discovery_metadata_receiver_t *rcv)
    {

        namespace mech = sst::basic_blocks::mechanics;

        auto bail = [rcv, location](const std::string &s) {
            std::string l{location};
            std::string ms = l + ": " + s;
            rcv->on_error(rcv, 0, ms.c_str());
            return false;
        };
        /*
         * This is our FXP cracker, customized for this extraction
         */
        auto p = fs::path{location};
        std::ifstream stream(p, std::ios::in | std::ios::binary);
        if (!stream.is_open())
            return bail("Unable to open file");

        std::vector<char> fxChunk;
        fxChunk.resize(sizeof(sst::io::fxChunkSetCustom));
        stream.read(fxChunk.data(), fxChunk.size());
        if (!stream)
            return bail("Cannot read chunk header");

        auto *fxp = (sst::io::fxChunkSetCustom *)(fxChunk.data());
        if ((mech::endian_read_int32BE(fxp->chunkMagic) != 'CcnK') ||
            (mech::endian_read_int32BE(fxp->fxMagic) != 'FPCh') ||
            (mech::endian_read_int32BE(fxp->fxID) != 'cjs3'))
        {
            return bail("This is not a Surge FXP file");
        }

        std::vector<char> patchHeaderChunk;
        patchHeaderChunk.resize(sizeof(sst::io::patch_header));
        stream.read(patchHeaderChunk.data(), patchHeaderChunk.size());
        if (!stream)
            return bail("Unable to read patch header");
        auto *ph = (sst::io::patch_header *)(patchHeaderChunk.data());
        auto xmlSz = mech::endian_read_int32LE(ph->xmlsize);

        if (!memcpy(ph->tag, "sub3", 4) || xmlSz < 0 || xmlSz > 1024 * 1024 * 1024)
        {
            return bail("Not a Surge XML containing FXP");
        }

        std::vector<char> xmlData;
        xmlData.resize(xmlSz + 1);
        stream.read(xmlData.data(), xmlData.size() - 1);
        if (!stream)
            return bail("Unable to read XML data");
        xmlData[xmlSz] = 0;

        TiXmlDocument doc;
        doc.Parse(xmlData.data(), nullptr, TIXML_ENCODING_LEGACY);
        if (doc.Error())
        {
            return bail(doc.ErrorDesc());
        }

        auto patch = TINYXML_SAFE_TO_ELEMENT(doc.FirstChild("patch"));
        if (!patch)
            return bail("XML does not contain a patch");
        auto meta = TINYXML_SAFE_TO_ELEMENT(patch->FirstChild("meta"));
        if (!meta)
            return bail("XML does not contain a meta tag");

        if (!meta->Attribute("name"))
            return bail("XML meta does not contain a name");

        auto res = rcv->begin_preset(rcv, meta->Attribute("name"), "");
        if (!res)
            return bail("Cannot begin preset");

        clap_universal_plugin_id_t clp{"clap", "org.surge-synth-team.surge-xt"};
        rcv->add_plugin_id(rcv, &clp);

        if (meta->Attribute("author"))
            rcv->add_creator(rcv, meta->Attribute("author"));

        if (meta->Attribute("comment"))
            rcv->set_description(rcv, meta->Attribute("comment"));

        if (meta->Attribute("license"))
            rcv->add_extra_info(rcv, "license", meta->Attribute("license"));

        if (meta->Attribute("category"))
            rcv->add_extra_info(rcv, "category", meta->Attribute("category"));

        return true;
    }

    const void *get_extension(const char *eid) { return nullptr; }

    struct clap_preset_discovery_provider provider;
};

uint32_t pd_count(const struct clap_preset_discovery_factory *factory) { return 1; }

const clap_preset_discovery_provider_descriptor_t *
pd_get_descriptor(const struct clap_preset_discovery_factory *factory, uint32_t index)
{
    return &desc;
}

const clap_preset_discovery_provider_t *
pd_create(const struct clap_preset_discovery_factory *factory,
          const clap_preset_discovery_indexer_t *indexer, const char *provider_id)
{
    if (strcmp(provider_id, desc.id) == 0)
    {
        auto res = new PresetProvider(indexer);
        return &res->provider;
    }
    return nullptr;
}

static const struct clap_preset_discovery_factory surgePresetDiscoveryFactory
{
    pd_count, pd_get_descriptor, pd_create
};
} // namespace sst::surge_xt::preset_discovery

const void *SurgeSynthProcessor::getSurgePresetDiscoveryFactory()
{
    return &sst::surge_xt::preset_discovery::surgePresetDiscoveryFactory;
}
#endif
