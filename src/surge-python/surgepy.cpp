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
#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include <thread>
#include <utility>

#include "SurgeSynthesizer.h"
#include "SurgeStorage.h"
#include "version.h"
#include "filesystem/import.h"

namespace py = pybind11;

/*
 * Generate stub types by running `pip install pybind11-stubgen && pybind11-stubgen surgepy`
 * and copy `out/` folder contents to `./surgepy`.
 */

class PythonPluginLayerProxy : public SurgeSynthesizer::PluginLayer
{
  public:
    void surgeParameterUpdated(const SurgeSynthesizer::ID &id, float d) override {}
    void surgeMacroUpdated(long macroNum, float d) override {}
};

static std::unique_ptr<PythonPluginLayerProxy> spysetup_parent = nullptr;
static std::mutex spysetup_mutex;

/*
 * The way we've decided to expose to python is through some wrapper objects
 * which give us the control group / control group entry / param hierarchy.
 * So here's some small helper objects
 */

struct SurgePyNamedParam
{
    std::string name;
    SurgeSynthesizer::ID id;
    std::string getName() const { return name; }
    SurgeSynthesizer::ID getID() const { return id; }
    std::string toString() const { return std::string("<SurgeNamedParam '") + name + "'>"; }
};

struct SurgePyControlGroupEntry
{
    std::vector<SurgePyNamedParam> params;
    int entry = -1;
    int scene = -1;
    std::string groupName;

    int getEntry() const { return entry; }
    int getScene() const { return scene; }
    std::vector<SurgePyNamedParam> getParams() const { return params; }

    std::string toString() const
    {
        auto res = std::string("<SurgeControlGroupEntry entry=") + std::to_string(entry);
        switch (scene)
        {
        case 0:
            break;
        case 1:
            res += "/sceneA";
            break;
        case 2:
            res += "/sceneB";
            break;
        }
        res += " in " + groupName + ">";
        return res;
    }
};

struct SurgePyControlGroup
{
    std::vector<SurgePyControlGroupEntry> entries;
    ControlGroup id = endCG;
    std::string name;

    SurgePyControlGroup() = default;
    SurgePyControlGroup(ControlGroup id, std::string name) : id(id), name(std::move(name)){};

    int getControlGroupId() const { return (int)id; }
    std::string getControlGroupName() const { return name; }
    std::vector<SurgePyControlGroupEntry> getEntries() const { return entries; }

    std::string toString() const
    {
        return std::string("<SurgeControlGroup cg=") + std::to_string(id) + ", " + name + ">";
    }
};

struct SurgePyModSource
{
    modsources ms = ms_original;
    std::string name;

    SurgePyModSource() = default;
    explicit SurgePyModSource(modsources ms) : ms(ms) { name = modsource_names[(int)ms]; }

    int getModSource() const { return ms; }
    std::string getName() const { return name; }
    std::string toString() const { return std::string("<SurgeModSource ") + name + ">"; }
};

struct SurgePyModRouting
{
    SurgePyModSource source;
    SurgePyNamedParam dest;
    int source_scene;
    int source_index;
    float depth;
    float normalizedDepth;
};

class SurgePyPatchConverter
{
  public:
    explicit SurgePyPatchConverter(SurgeSynthesizer *synth) : synth(synth){};
    ~SurgePyPatchConverter() = default;

    void addParam(py::dict &to, const std::string &at, const Parameter &from) const
    {
        auto id = synth->idForParameter(&from);
        char txt[256];
        synth->getParameterName(id, txt);
        auto newp = SurgePyNamedParam();
        newp.name = txt;
        newp.id = id;
        to[at.c_str()] = newp;
    }

    void addParam(py::list &to, const Parameter &from) const
    {
        auto id = synth->idForParameter(&from);
        char txt[256];
        synth->getParameterName(id, txt);
        auto newp = SurgePyNamedParam();
        newp.name = txt;
        newp.id = id;
        to.append(newp);
    }
    py::dict makeFX(const SurgePatch &patch, int s)
    {
        auto res = py::dict();
        auto fxi = &(patch.fx[s]);
#define ADD(x) addParam(res, #x, fxi->x);
        ADD(type);
        ADD(return_level);
#undef ADD

        auto pars = py::list();
        for (int i = 0; i < n_fx_params; ++i)
            addParam(pars, fxi->p[i]);
        res["p"] = pars;

        return res;
    }

    py::dict makeFU(const SurgePatch &patch, int sc, int fuI)
    {
        auto fu = &(patch.scene[sc].filterunit[fuI]);
        auto res = py::dict();
#define ADD(x) addParam(res, #x, fu->x);
        ADD(type);
        ADD(subtype);
        ADD(cutoff);
        ADD(resonance);
        ADD(envmod);
        ADD(keytrack);
#undef ADD
        return res;
    }
    py::dict makeOsc(const SurgePatch &patch, int sc, int oscI)
    {
        auto osc = &(patch.scene[sc].osc[oscI]);
        auto res = py::dict();
#define ADD(x) addParam(res, #x, osc->x);
        ADD(type);
        ADD(pitch);
        ADD(octave);
        ADD(keytrack);
        ADD(retrigger);
#undef ADD

        auto pars = py::list();
        for (int i = 0; i < n_osc_params; ++i)
            addParam(pars, osc->p[i]);
        res["p"] = pars;

        return res;
    }
    py::dict makeScene(const SurgePatch &patch, int scI)
    {
        auto res = py::dict();

        auto sc = &(patch.scene[scI]);

        auto osc = py::list();
        for (int i = 0; i < n_oscs; ++i)
            osc.append(makeOsc(patch, scI, i));
        res["osc"] = osc;

        auto fu = py::list();
        fu.append(makeFU(patch, scI, 0));
        fu.append(makeFU(patch, scI, 1));
        res["filterunit"] = fu;

        auto ws = py::dict();
        addParam(ws, "type", sc->wsunit.type);
        addParam(ws, "drive", sc->wsunit.drive);
        res["wsunit"] = ws;

        // ADSRSTORAGE
        py::dict e0, e1;
#define ADSRADD(x)                                                                                 \
    addParam(e0, #x, sc->adsr[0].x);                                                               \
    addParam(e1, #x, sc->adsr[1].x);

        ADSRADD(a);
        ADSRADD(d);
        ADSRADD(s);
        ADSRADD(r);

        ADSRADD(a_s);
        ADSRADD(d_s);
        ADSRADD(r_s);
        ADSRADD(mode);

#undef ADSRADD
        auto adsr = py::list();
        adsr.append(e0);
        adsr.append(e1);
        res["adsr"] = adsr;

        // LFOSTORAGE
        auto lfos = py::list();
        for (int i = 0; i < n_lfos; ++i)
        {
            auto lfo = py::dict();
#define ADD(x) addParam(lfo, #x, sc->lfo[i].x);

            ADD(rate);
            ADD(shape);
            ADD(start_phase);
            ADD(magnitude);
            ADD(deform);
            ADD(trigmode);
            ADD(unipolar);
            ADD(delay);
            ADD(hold);
            ADD(attack);
            ADD(decay);
            ADD(sustain);
            ADD(release);
#undef ADD
            lfos.append(lfo);
        }
        res["lfo"] = lfos;

#define ADD(x) addParam(res, #x, sc->x);
        ADD(pitch);
        ADD(octave);
        ADD(fm_depth);
        ADD(fm_switch);
        ADD(drift);
        ADD(noise_colour);
        ADD(keytrack_root);

#define ADDMIX(x)                                                                                  \
    ADD(level_##x);                                                                                \
    ADD(mute_##x);                                                                                 \
    ADD(solo_##x);                                                                                 \
    ADD(route_##x);

        ADDMIX(o1);
        ADDMIX(o2);
        ADDMIX(o3);
        ADDMIX(ring_12);
        ADDMIX(ring_23);
        ADDMIX(noise);
#undef ADDMIX

        ADD(level_pfg);
        ADD(vca_level);
        ADD(pbrange_dn);
        ADD(pbrange_up);
        ADD(vca_velsense);
        ADD(polymode);
        ADD(portamento);
        ADD(volume);
        ADD(pan);
        ADD(width);

        py::list sl;
        addParam(sl, sc->send_level[0]);
        addParam(sl, sc->send_level[1]);
        res["send_level"] = sl;

        ADD(f2_cutoff_is_offset);
        ADD(f2_link_resonance);
        ADD(feedback);
        ADD(filterblock_configuration);
        ADD(filter_balance);
        ADD(lowcut);
#undef ADD

        return res;
    }
    py::dict asDict(const SurgePatch &patch)
    {
        auto res = py::dict();
        auto scenes = py::list();
        for (int i = 0; i < n_scenes; ++i)
            scenes.append(makeScene(patch, i));
        res["scene"] = scenes;

        auto fxs = py::list();
        for (int i = 0; i < n_fx_slots; ++i)
            fxs.append(makeFX(patch, i));
        res["fx"] = fxs;

#define ADD(x) addParam(res, #x, patch.x);
        ADD(scene_active);
        ADD(scenemode);
        ADD(splitpoint);
        ADD(volume);
        ADD(polylimit);
        ADD(fx_bypass);
        ADD(fx_disable);
        ADD(character);
#undef ADD

        return res;
    }

    SurgeSynthesizer *synth = nullptr;
};

static std::unordered_map<ControlGroup, SurgePyControlGroup> spysetup_cgMap;
static std::unordered_map<modsources, SurgePyModSource> spysetup_msMap;

class SurgeSynthesizerWithPythonExtensions : public SurgeSynthesizer
{
  public:
    explicit SurgeSynthesizerWithPythonExtensions(PluginLayer *sparent) : SurgeSynthesizer(sparent)
    {
        std::lock_guard<std::mutex> lg(spysetup_mutex);
        if (spysetup_cgMap.empty())
        {
            // First time setup
#define CGM(c) spysetup_cgMap[c] = SurgePyControlGroup(c, #c);

            CGM(cg_GLOBAL);
            CGM(cg_OSC);
            CGM(cg_MIX);
            CGM(cg_LFO);
            CGM(cg_FX);
            CGM(cg_FILTER);
            CGM(cg_ENV);

            for (auto &me : spysetup_cgMap)
            {
                auto cg = me.first;
                auto pc = me.second;

                // Populate with a lazy multi pass
                std::set<std::pair<int, int>> entrySet;
                for (auto pa : storage.getPatch().param_ptr)
                {
                    if (pa && pa->ctrlgroup == cg)
                    {
                        entrySet.insert(std::make_pair(pa->ctrlgroup_entry, pa->scene));
                    }
                }

                for (auto ent : entrySet)
                {
                    auto entO = SurgePyControlGroupEntry();
                    entO.groupName = me.second.name;
                    entO.entry = ent.first;
                    entO.scene = ent.second;
                    for (auto pa : storage.getPatch().param_ptr)
                    {
                        if (pa && pa->ctrlgroup == cg && pa->ctrlgroup_entry == ent.first &&
                            pa->scene == ent.second)
                        {
                            auto paO = SurgePyNamedParam();
                            paO.name = pa->get_full_name();
                            paO.id = idForParameter(pa);
                            entO.params.push_back(paO);
                        }
                    }
                    me.second.entries.push_back(entO);
                }
            }

#define CMS(c) spysetup_msMap[c] = SurgePyModSource(c);

            CMS(ms_velocity);
            CMS(ms_releasevelocity);
            CMS(ms_keytrack);
            CMS(ms_lowest_key);
            CMS(ms_highest_key);
            CMS(ms_latest_key);
            CMS(ms_polyaftertouch);
            CMS(ms_aftertouch);
            CMS(ms_modwheel);
            CMS(ms_breath);
            CMS(ms_expression);
            CMS(ms_sustain);
            CMS(ms_pitchbend);
            CMS(ms_timbre);
            CMS(ms_alternate_bipolar);
            CMS(ms_alternate_unipolar);
            CMS(ms_random_bipolar);
            CMS(ms_random_unipolar);
            CMS(ms_filtereg);
            CMS(ms_ampeg);
            CMS(ms_lfo1);
            CMS(ms_lfo2);
            CMS(ms_lfo3);
            CMS(ms_lfo4);
            CMS(ms_lfo5);
            CMS(ms_lfo6);
            CMS(ms_slfo1);
            CMS(ms_slfo2);
            CMS(ms_slfo3);
            CMS(ms_slfo4);
            CMS(ms_slfo5);
            CMS(ms_slfo6);
            CMS(ms_ctrl1);
            CMS(ms_ctrl2);
            CMS(ms_ctrl3);
            CMS(ms_ctrl4);
            CMS(ms_ctrl5);
            CMS(ms_ctrl6);
            CMS(ms_ctrl7);
            CMS(ms_ctrl8);
        }
    }

    SurgePyControlGroup getControlGroup(int cg) const
    {
        if (spysetup_cgMap.find((ControlGroup)cg) == spysetup_cgMap.end())
        {
            throw std::out_of_range("getControlGroup called with invalid control group value");
        }
        return spysetup_cgMap[(ControlGroup)cg];
    }

    SurgePyModSource getModSource(int ms) const
    {
        if (spysetup_msMap.find((modsources)ms) == spysetup_msMap.end())
        {
            throw std::out_of_range("getModSource called with invalid mod source group value");
        }
        return spysetup_msMap[(modsources)ms];
    }

    std::string getParameterName_py(const SurgeSynthesizer::ID &id)
    {
        char txt[256];
        getParameterName(id, txt);
        return std::string(txt);
    }

    SurgeSynthesizer::ID createSynthSideId(int id)
    {
        SurgeSynthesizer::ID idr;
        fromSynthSideId(id, idr);
        return idr;
    }

    void playNoteWithInts(int ch, int note, int vel, int detune)
    {
        playNote(ch, note, vel, detune);
    }

    void pitchBendWithInts(int ch, int bend) { pitchBend(ch, bend); }

    void polyAftertouchWithInts(int channel, int key, int value)
    {
        polyAftertouch(channel, key, value);
    }

    void channelAftertouchWithInts(int channel, int value) { channelAftertouch(channel, value); }

    void channelControllerWithInts(int channel, int cc, int value)
    {
        channelController(channel, cc, value);
    }

    float getv(Parameter *p, const pdata &v)
    {
        if (p->valtype == vt_float)
            return v.f;
        if (p->valtype == vt_int)
            return v.i;
        if (p->valtype == vt_bool)
            return v.b;
        return 0;
    }

    float getParamMin(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (p)
            return getv(p, p->val_min);
        return 0;
    }

    float getParamMax(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (p)
            return getv(p, p->val_max);
        return 0;
    }

    float getParamDef(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (p)
            return getv(p, p->val_default);
        return 0;
    }

    float getParamVal(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (p)
            return getv(p, p->val);
        return 0;
    }

    std::string getParamDisplay(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];

        if (p)
        {
            return p->get_display();
        }

        return "<error>";
    }

    std::string getParamValType(const SurgePyNamedParam &id)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (!p)
            return "<error>";
        switch (p->valtype)
        {
        case vt_float:
            return "float";
        case vt_int:
            return "int";
        case vt_bool:
            return "bool";
        }
        return "<error>";
    }

    std::string getParamInfo(const SurgePyNamedParam &id)
    {
        const auto val = getParamVal(id);
        const auto min = getParamMin(id);
        const auto max = getParamMax(id);
        const auto def = getParamDef(id);
        const auto val_type = getParamValType(id);
        const auto display = getParamDisplay(id);

        std::ostringstream oss;
        oss << "Parameter name: " << id.getName() << std::endl;
        oss << "Parameter value: " << val << std::endl;
        oss << "Parameter min: " << min << std::endl;
        oss << "Parameter max: " << max << std::endl;
        oss << "Parameter default: " << def << std::endl;
        oss << "Parameter value type: " << val_type << std::endl;
        oss << "Parameter display value: " << display << std::endl;

        return oss.str();
    }

    void setParamVal(const SurgePyNamedParam &id, float f)
    {
        auto p = storage.getPatch().param_ptr[id.getID().getSynthSideId()];
        if (!p)
            return;

        setParameter01(id.getID(), p->value_to_normalized(f));
    }

    void releaseNoteWithInts(int ch, int note, int vel) { releaseNote(ch, note, vel); }

    bool loadWavetablePy(int scene, int osc, const std::string &s)
    {
        auto path = string_to_path(s);

        if (scene < 0 || scene >= n_scenes || osc < 0 || osc >= n_oscs)
        {
            throw std::invalid_argument("OSC and SCENE out of range in loadWavetable");
        }
        if (!fs::exists(path))
        {
            throw std::invalid_argument((std::string("File not found: ") + s).c_str());
        }
        std::cout << "Would load " << scene << " " << osc << " with " << s << std::endl;
        auto os = &(storage.getPatch().scene[scene].osc[osc]);
        auto wt = &(os->wt);
        storage.load_wt(s, wt, os);

        return true;
    }
    bool loadPatchPy(const std::string &s)
    {
        auto path = string_to_path(s);

        if (!fs::exists(path))
        {
            throw std::invalid_argument((std::string("File not found: ") + s).c_str());
        }

        bool result = loadPatchByPath(s.c_str(), -1, path.filename().u8string().c_str());

        // update tempo if we want to change it on patch load
        if (storage.unstreamedTempo > -1.f)
        {
            time_data.tempo = storage.unstreamedTempo;
        }

        return result;
    }

    void savePatchPy(const std::string &s) { savePatchToPath(string_to_path(s)); }

    std::string factoryDataPath() const { return storage.datapath.u8string(); }

    std::string userDataPath() const { return storage.userDataPath.u8string(); }

    py::array_t<float> getOutput()
    {
        return py::array_t<float>({2, BLOCK_SIZE}, {2 * sizeof(float), sizeof(float)},
                                  (const float *)(&output[0][0]));
    }

    void setModulationPy(const SurgePyNamedParam &to, SurgePyModSource const &from, float amt,
                         int scene, int index)
    {
        setModDepth01(to.getID().getSynthSideId(), (modsources)from.getModSource(), scene, index,
                      amt);
    }

    float getModulationPy(const SurgePyNamedParam &to, const SurgePyModSource &from, int scene,
                          int index)
    {
        return getModDepth01(to.getID().getSynthSideId(), (modsources)from.getModSource(), scene,
                             index);
    }

    bool isValidModulationPy(const SurgePyNamedParam &to, const SurgePyModSource &from)
    {
        return isValidModulation(to.getID().getSynthSideId(), (modsources)from.getModSource());
    }

    bool isActiveModulationPy(const SurgePyNamedParam &to, const SurgePyModSource &from, int scene,
                              int index)
    {
        return isActiveModulation(to.getID().getSynthSideId(), (modsources)from.getModSource(),
                                  scene, index);
    }

    bool isBipolarModulationPy(const SurgePyModSource &from)
    {
        return isBipolarModulation((modsources)from.getModSource());
    }

    py::array_t<float> createMultiBlock(int nBlocks)
    {
        auto res = py::array_t<float>({2, nBlocks * BLOCK_SIZE},
                                      {nBlocks * BLOCK_SIZE * sizeof(float), sizeof(float)});
        auto buf = res.request(true);
        memset(buf.ptr, 0, 2 * BLOCK_SIZE * nBlocks * sizeof(float));
        return res;
    }

    void processMultiBlock(const py::array_t<float> &arr, int startBlock = 0, int nBlocks = -1)
    {
        auto buf = arr.request(true);

        /*
         * Error condition checks
         */
        if (buf.itemsize != sizeof(float))
        {
            std::ostringstream oss;
            oss << "Input numpy array must have dtype float; you provided an array with "
                << buf.format << "/" << buf.size;
            throw std::invalid_argument(oss.str().c_str());
        }

        if (buf.ndim != 2)
        {
            std::ostringstream oss;
            oss << "Input numpy array must have 2 dimensions (2, m*BLOCK_SIZE); you provided an "
                   "array with "
                << buf.ndim << " dimensions";
            throw std::invalid_argument(oss.str().c_str());
        }

        if (buf.shape[0] != 2 || buf.shape[1] % BLOCK_SIZE != 0)
        {
            std::ostringstream oss;
            oss << "Input numpy array must have dimensions (2, m*BLOCK_SIZE); you provided an "
                   "array with "
                << buf.shape[0] << "x" << buf.shape[1];
            throw std::invalid_argument(oss.str().c_str());
        }

        size_t maxBlockStorage = buf.shape[1] / BLOCK_SIZE;

        if (startBlock >= maxBlockStorage)
        {
            std::ostringstream oss;
            oss << "Start block of " << startBlock << " is beyond the end of input storage with "
                << maxBlockStorage << " blocks";
            throw std::invalid_argument(oss.str().c_str());
        }

        int blockIterations = maxBlockStorage - startBlock;

        if (nBlocks > 0)
        {
            blockIterations = nBlocks;
        }

        if (startBlock + blockIterations > maxBlockStorage)
        {
            std::ostringstream oss;
            oss << "Start block / nBlock combo " << startBlock << " " << nBlocks
                << " is beyond the end of input storage with " << maxBlockStorage << " blocks";
            throw std::invalid_argument(oss.str().c_str());
        }

        auto ptr = static_cast<float *>(buf.ptr);
        float *dL = ptr + startBlock * BLOCK_SIZE;
        float *dR = ptr + buf.shape[1] + startBlock * BLOCK_SIZE;

        process_input = false;

        for (auto i = 0; i < blockIterations; ++i)
        {
            process();
            time_data.ppqPos += (double)BLOCK_SIZE * time_data.tempo / (60. * storage.samplerate);
            memcpy((void *)dL, (void *)(output[0]), BLOCK_SIZE * sizeof(float));
            memcpy((void *)dR, (void *)(output[1]), BLOCK_SIZE * sizeof(float));

            dL += BLOCK_SIZE;
            dR += BLOCK_SIZE;
        }
    }

    void processMultiBlockWithInput(const py::array_t<float> &in_arr,
                                    const py::array_t<float> &out_arr, int startBlock = 0,
                                    int nBlocks = -1)
    {
        auto in_buf = in_arr.request();
        auto out_buf = out_arr.request(true);

        /*
         * Error condition checks
         */
        if (in_buf.itemsize != sizeof(float))
        {
            std::ostringstream oss;
            oss << "Input numpy array must have dtype float; you provided an array with "
                << in_buf.format << "/" << in_buf.size;
            throw std::invalid_argument(oss.str().c_str());
        }

        if (out_buf.itemsize != sizeof(float))
        {
            std::ostringstream oss;
            oss << "Output numpy array must have dtype float; you provided an array with "
                << out_buf.format << "/" << out_buf.size;
            throw std::invalid_argument(oss.str().c_str());
        }

        if (out_buf.ndim != 2)
        {
            std::ostringstream oss;
            oss << "Output numpy array must have 2 dimensions (2, m*BLOCK_SIZE); you provided an "
                   "array with "
                << out_buf.ndim << " dimensions";
            throw std::invalid_argument(oss.str().c_str());
        }

        if (out_buf.shape[0] != 2 || out_buf.shape[1] % BLOCK_SIZE != 0)
        {
            std::ostringstream oss;
            oss << "Output numpy array must have dimensions (2, m*BLOCK_SIZE); you provided an "
                   "array with "
                << out_buf.shape[0] << "x" << out_buf.shape[1];
            throw std::invalid_argument(oss.str().c_str());
        }

        if (in_buf.shape != out_buf.shape)
        {
            std::ostringstream oss;
            oss << "Input numpy array dimensions must match output; you provided an "
                   "array with "
                << in_buf.shape[0] << "x" << in_buf.shape[1];
            throw std::invalid_argument(oss.str().c_str());
        }

        size_t maxBlockStorage = out_buf.shape[1] / BLOCK_SIZE;

        if (startBlock >= maxBlockStorage)
        {
            std::ostringstream oss;
            oss << "Start block of " << startBlock << " is beyond the end of input storage with "
                << maxBlockStorage << " blocks";
            throw std::invalid_argument(oss.str().c_str());
        }

        int blockIterations = maxBlockStorage - startBlock;

        if (nBlocks > 0)
        {
            blockIterations = nBlocks;
        }

        if (startBlock + blockIterations > maxBlockStorage)
        {
            std::ostringstream oss;
            oss << "Start block / nBlock combo " << startBlock << " " << nBlocks
                << " is beyond the end of input storage with " << maxBlockStorage << " blocks";
            throw std::invalid_argument(oss.str().c_str());
        }

        process_input = true;

        auto in_ptr = static_cast<float *>(in_buf.ptr);
        float *iL = in_ptr + startBlock * BLOCK_SIZE;
        float *iR = in_ptr + in_buf.shape[1] + startBlock * BLOCK_SIZE;
        auto out_ptr = static_cast<float *>(out_buf.ptr);
        float *oL = out_ptr + startBlock * BLOCK_SIZE;
        float *oR = out_ptr + out_buf.shape[1] + startBlock * BLOCK_SIZE;

        for (auto i = 0; i < blockIterations; ++i)
        {
            memcpy((void *)(input[0]), (void *)iL, BLOCK_SIZE * sizeof(float));
            memcpy((void *)(input[1]), (void *)iR, BLOCK_SIZE * sizeof(float));
            process();
            time_data.ppqPos += (double)BLOCK_SIZE * time_data.tempo / (60. * storage.samplerate);
            memcpy((void *)oL, (void *)(output[0]), BLOCK_SIZE * sizeof(float));
            memcpy((void *)oR, (void *)(output[1]), BLOCK_SIZE * sizeof(float));

            iL += BLOCK_SIZE;
            iR += BLOCK_SIZE;
            oL += BLOCK_SIZE;
            oR += BLOCK_SIZE;
        }
    }

    py::dict getPatchAsPy()
    {
        auto pc = SurgePyPatchConverter(this);
        return pc.asDict(storage.getPatch());
    }

    SurgePyNamedParam surgePyNamedParamById(int id)
    {
        auto s = SurgePyNamedParam();
        SurgeSynthesizer::ID ido;
        fromSynthSideId(id, ido);
        s.id = ido;
        char txt[256];
        getParameterName(ido, txt);
        s.name = txt;
        return s;
    }

    py::dict getAllModRoutings()
    {
        auto res = py::dict();
        auto sv = [](auto s) {
            auto r = s;
            std::sort(r.begin(), r.end(), [](const auto &a, const auto &b) {
                if (a.source_id == b.source_id)
                    return a.destination_id < b.destination_id;
                else
                    return a.source_id < b.source_id;
            });
            return r;
        };

        auto gmr = py::list();
        auto t = sv(storage.getPatch().modulation_global);

        for (auto gm : t)
        {
            auto r = SurgePyModRouting();

            r.source = SurgePyModSource((modsources)gm.source_id);
            r.dest = surgePyNamedParamById(gm.destination_id);
            r.depth = gm.depth;
            r.source_scene = gm.source_scene;
            r.source_index = gm.source_index;
            r.normalizedDepth = getModDepth01(gm.destination_id, (modsources)gm.source_id,
                                              gm.source_scene, gm.source_index);
            gmr.append(r);
        }

        res["global"] = gmr;

        auto scm = py::list();

        for (int sc = 0; sc < n_scenes; sc++)
        {
            auto ts = py::dict();
            auto sms = py::list();
            auto st = sv(storage.getPatch().scene[sc].modulation_scene);

            for (auto gm : st)
            {
                auto r = SurgePyModRouting();

                r.source = SurgePyModSource((modsources)gm.source_id);
                r.dest =
                    surgePyNamedParamById(gm.destination_id + storage.getPatch().scene_start[sc]);
                r.depth = gm.depth;
                r.source_scene = gm.source_scene;
                r.source_index = gm.source_index;
                r.normalizedDepth =
                    getModDepth01(gm.destination_id + storage.getPatch().scene_start[sc],
                                  (modsources)gm.source_id, r.source_scene, r.source_index);
                sms.append(r);
            }

            ts["scene"] = sms;

            auto smv = py::list();
            auto vt = sv(storage.getPatch().scene[sc].modulation_voice);

            for (auto gm : vt)
            {
                auto r = SurgePyModRouting();

                r.source = SurgePyModSource((modsources)gm.source_id);
                r.dest =
                    surgePyNamedParamById(gm.destination_id + storage.getPatch().scene_start[sc]);
                r.depth = gm.depth;
                r.source_scene = gm.source_scene;
                r.source_index = gm.source_index;
                r.normalizedDepth =
                    getModDepth01(gm.destination_id + storage.getPatch().scene_start[sc],
                                  (modsources)gm.source_id, gm.source_scene, gm.source_index);
                smv.append(r);
            }

            ts["voice"] = smv;
            scm.append(ts);
        }

        res["scene"] = scm;

        return res;
    }

    void loadSCLFile(const std::string &s)
    {
        try
        {
            storage.retuneToScale(Tunings::readSCLFile(s));
        }
        catch (Tunings::TuningError &e)
        {
            throw std::invalid_argument(e.what()); // convert so python sees it
        }
    }

    void loadKBMFile(const std::string &s)
    {
        try
        {
            storage.remapToKeyboard(Tunings::readKBMFile(s));
        }
        catch (Tunings::TuningError &e)
        {
            throw std::invalid_argument(e.what()); // convert so python sees it
        }
    }

    void retuneToStandardTuning() { storage.retuneTo12TETScaleC261Mapping(); }

    void remapToStandardKeyboard() { storage.remapToConcertCKeyboard(); }

    void retuneToStandardScale() { storage.retuneTo12TETScale(); }

    void setTuningApplicationMode(SurgeStorage::TuningApplicationMode m)
    {
        storage.tuningApplicationMode = m;
    }

    SurgeStorage::TuningApplicationMode getTuningApplicationMode() const
    {
        return storage.tuningApplicationMode;
    }

    void setMPEEnabled(bool m) { storage.mpeEnabled = m; }

    bool getMPEEnabled() const { return storage.mpeEnabled; }
};

SurgeSynthesizer *createSurge(float sr)
{
    if (spysetup_parent == nullptr)
        spysetup_parent = std::make_unique<PythonPluginLayerProxy>();
    auto surge = new SurgeSynthesizerWithPythonExtensions(spysetup_parent.get());
    surge->setSamplerate(sr);
    surge->time_data.tempo = 120;
    surge->time_data.ppqPos = 0;
    return surge;
}

// Prefix _ if using shared object within a Python package built with scikit-build
#ifdef SKBUILD
PYBIND11_MODULE(_surgepy, m)
#else
PYBIND11_MODULE(surgepy, m)
#endif
{
    m.doc() = "Python bindings for Surge XT Synthesizer";
    m.def("createSurge", &createSurge, "Create a Surge XT instance", py::arg("sampleRate"));
    m.def(
        "getVersion", []() { return Surge::Build::FullVersionStr; }, "Get the version of Surge XT");
    py::class_<SurgeSynthesizer::ID>(m, "SurgeSynthesizer_ID")
        .def(py::init<>())
        .def("getSynthSideId", &SurgeSynthesizer::ID::getSynthSideId)
        .def("__repr__", &SurgeSynthesizer::ID::toString);

    py::class_<SurgeSynthesizerWithPythonExtensions>(m, "SurgeSynthesizer")
        .def("__repr__",
             [](SurgeSynthesizerWithPythonExtensions &s) {
                 return std::string("<SurgeSynthesizer samplerate=") +
                        std::to_string((int)s.storage.samplerate) + "Hz>";
             })
        .def("getControlGroup", &SurgeSynthesizerWithPythonExtensions::getControlGroup,
             "Gather the parameters groups for a surge.constants.cg_ control group",
             py::arg("entry"))

        .def("getNumInputs", &SurgeSynthesizer::getNumInputs)
        .def("getNumOutputs", &SurgeSynthesizer::getNumOutputs)
        .def("getBlockSize", &SurgeSynthesizer::getBlockSize)
        .def("getFactoryDataPath", &SurgeSynthesizerWithPythonExtensions::factoryDataPath)
        .def("getUserDataPath", &SurgeSynthesizerWithPythonExtensions::userDataPath)
        .def("getSampleRate",
             [](SurgeSynthesizerWithPythonExtensions &s) { return s.storage.samplerate; })

        .def("fromSynthSideId", &SurgeSynthesizer::fromSynthSideId)
        .def("createSynthSideId", &SurgeSynthesizerWithPythonExtensions::createSynthSideId)

        .def("getParameterName", &SurgeSynthesizerWithPythonExtensions::getParameterName_py,
             "Given a parameter, return its name as displayed by the synth.")

        .def("playNote", &SurgeSynthesizerWithPythonExtensions::playNoteWithInts,
             "Trigger a note on this Surge XT instance.", py::arg("channel"), py::arg("midiNote"),
             py::arg("velocity"), py::arg("detune") = 0)
        .def("releaseNote", &SurgeSynthesizerWithPythonExtensions::releaseNoteWithInts,
             "Release a note on this Surge XT instance.", py::arg("channel"), py::arg("midiNote"),
             py::arg("releaseVelocity") = 0)
        .def("pitchBend", &SurgeSynthesizerWithPythonExtensions::pitchBendWithInts,
             "Set the pitch bend value on channel ch", py::arg("channel"), py::arg("bend"))
        .def("allNotesOff", &SurgeSynthesizer::allNotesOff, "Turn off all playing notes")
        .def("polyAftertouch", &SurgeSynthesizerWithPythonExtensions::polyAftertouchWithInts,
             "Send the poly aftertouch MIDI message", py::arg("channel"), py::arg("key"),
             py::arg("value"))
        .def("channelAftertouch", &SurgeSynthesizerWithPythonExtensions::channelAftertouchWithInts,
             "Send the channel aftertouch MIDI message", py::arg("channel"), py::arg("value"))
        .def("channelController", &SurgeSynthesizerWithPythonExtensions::channelControllerWithInts,
             "Set MIDI controller on channel to value", py::arg("channel"), py::arg("cc"),
             py::arg("value"))

        .def("getParamMin", &SurgeSynthesizerWithPythonExtensions::getParamMin,
             "Parameter minimum value, as a float.")
        .def("getParamMax", &SurgeSynthesizerWithPythonExtensions::getParamMax,
             "Parameter maximum value, as a float")
        .def("getParamDef", &SurgeSynthesizerWithPythonExtensions::getParamDef,
             "Parameter default value, as a float")
        .def("getParamVal", &SurgeSynthesizerWithPythonExtensions::getParamVal,
             "Parameter current value in this Surge XT instance, as a float")
        .def("getParamValType", &SurgeSynthesizerWithPythonExtensions::getParamValType,
             "Parameter types float, int or bool are supported")

        .def("getParamDisplay", &SurgeSynthesizerWithPythonExtensions::getParamDisplay,
             "Parameter value display (stringified and formatted)")
        .def("getParamInfo", &SurgeSynthesizerWithPythonExtensions::getParamInfo,
             "Parameter value info (formatted)")

        .def("setParamVal", &SurgeSynthesizerWithPythonExtensions::setParamVal,
             "Set a parameter value", py::arg("param"), py::arg("toThis"))

        .def("loadPatch", &SurgeSynthesizerWithPythonExtensions::loadPatchPy,
             "Load a Surge XT .fxp patch from the file system.", py::arg("path"))
        .def("savePatch", &SurgeSynthesizerWithPythonExtensions::savePatchPy,
             "Save the current state of Surge XT to an .fxp file.", py::arg("path"))

        .def("loadWavetable", &SurgeSynthesizerWithPythonExtensions::loadWavetablePy,
             "Load a wavetable file directly into a scene and oscillator immediately on this "
             "thread.",
             py::arg("scene"), py::arg("osc"), py::arg("path"))

        .def("getModSource", &SurgeSynthesizerWithPythonExtensions::getModSource,
             "Given a constant from surge.constants.ms_*, provide a modulator object",
             py::arg("modId"))
        .def("setModDepth01", &SurgeSynthesizerWithPythonExtensions::setModulationPy,
             "Set a modulation to a given depth", py::arg("targetParameter"),
             py::arg("modulationSource"), py::arg("depth"), py::arg("scene") = 0,
             py::arg("index") = 0)
        .def("getModDepth01", &SurgeSynthesizerWithPythonExtensions::getModulationPy,
             "Get the modulation depth from a source to a parameter.", py::arg("targetParameter"),
             py::arg("modulationSource"), py::arg("scene") = 0, py::arg("index") = 0)
        .def("isValidModulation", &SurgeSynthesizerWithPythonExtensions::isValidModulationPy,
             "Is it possible to modulate between target and source?", py::arg("targetParameter"),
             py::arg("modulationSource"))
        .def("isActiveModulation", &SurgeSynthesizerWithPythonExtensions::isActiveModulationPy,
             "Is there an established modulation between target and source?",
             py::arg("targetParameter"), py::arg("modulationSource"), py::arg("scene") = 0,
             py::arg("index") = 0)
        .def("isBipolarModulation", &SurgeSynthesizerWithPythonExtensions::isBipolarModulationPy,
             "Is the given modulation source bipolar?", py::arg("modulationSource"))

        .def("getAllModRoutings", &SurgeSynthesizerWithPythonExtensions::getAllModRoutings,
             "Get the entire modulation matrix for this instance.")

        .def("process", &SurgeSynthesizer::process,
             "Run Surge XT for one block and update the internal output buffer.")
        .def("getOutput", &SurgeSynthesizerWithPythonExtensions::getOutput,
             "Retrieve the internal output buffer as a 2 * BLOCK_SIZE numpy array.")

        .def("createMultiBlock", &SurgeSynthesizerWithPythonExtensions::createMultiBlock,
             "Create a numpy array suitable to hold up to b blocks of Surge XT processing in "
             "processMultiBlock",
             py::arg("blockCapacity"))
        .def("processMultiBlock", &SurgeSynthesizerWithPythonExtensions::processMultiBlock,
             "Run the Surge XT engine for multiple blocks, updating the value in the numpy array. "
             "Either populate the\n"
             "entire array, or starting at startBlock position in the output, populate nBlocks.",
             py::arg("val"), py::arg("startBlock") = 0, py::arg("nBlocks") = -1)
        .def("processMultiBlockWithInput",
             &SurgeSynthesizerWithPythonExtensions::processMultiBlockWithInput,
             "Run the Surge XT engine for multiple blocks using the input numpy array, "
             "updating the value in the output numpy array. "
             "Either populate the\n"
             "entire array, or starting at startBlock position in the output, populate nBlocks.",
             py::arg("inVal"), py::arg("outVal"), py::arg("startBlock") = 0,
             py::arg("nBlocks") = -1)

        .def("getPatch", &SurgeSynthesizerWithPythonExtensions::getPatchAsPy,
             "Get a Python dictionary with the Surge XT parameters laid out in the logical patch "
             "format")

        .def("loadSCLFile", &SurgeSynthesizerWithPythonExtensions::loadSCLFile,
             "Load an SCL tuning file and apply tuning to this instance")
        .def("retuneToStandardTuning",
             &SurgeSynthesizerWithPythonExtensions::retuneToStandardTuning,
             "Return this instance to 12-TET Concert Keyboard Mapping")
        .def("retuneToStandardScale", &SurgeSynthesizerWithPythonExtensions::retuneToStandardScale,
             "Return this instance to 12-TET Scale")
        .def("loadKBMFile", &SurgeSynthesizerWithPythonExtensions::loadKBMFile,
             "Load a KBM mapping file and apply tuning to this instance")
        .def("remapToStandardKeyboard",
             &SurgeSynthesizerWithPythonExtensions::remapToStandardKeyboard,
             "Return to standard C-centered keyboard mapping")
        .def_property("mpeEnabled", &SurgeSynthesizerWithPythonExtensions::getMPEEnabled,
                      &SurgeSynthesizerWithPythonExtensions::setMPEEnabled)
        .def_property("tuningApplicationMode",
                      &SurgeSynthesizerWithPythonExtensions::getTuningApplicationMode,
                      &SurgeSynthesizerWithPythonExtensions::setTuningApplicationMode);

    py::class_<SurgePyControlGroup>(m, "SurgeControlGroup")
        .def("getId", &SurgePyControlGroup::getControlGroupId)
        .def("getName", &SurgePyControlGroup::getControlGroupName)
        .def("getEntries", &SurgePyControlGroup::getEntries)
        .def("__repr__", &SurgePyControlGroup::toString);

    py::class_<SurgePyControlGroupEntry>(m, "SurgeControlGroupEntry")
        .def("getEntry", &SurgePyControlGroupEntry::getEntry)
        .def("getScene", &SurgePyControlGroupEntry::getScene)
        .def("getParams", &SurgePyControlGroupEntry::getParams)
        .def("__repr__", &SurgePyControlGroupEntry::toString);

    py::class_<SurgePyNamedParam>(m, "SurgeNamedParamId")
        .def("getName", &SurgePyNamedParam::getName)
        .def("getId", &SurgePyNamedParam::getID)
        .def("__repr__", &SurgePyNamedParam::toString);

    py::class_<SurgePyModSource>(m, "SurgeModSource")
        .def("getModSource", &SurgePyModSource::getModSource)
        .def("getName", &SurgePyModSource::getName)
        .def("__repr__", &SurgePyModSource::toString);

    py::class_<SurgePyModRouting>(m, "SurgeModRouting")
        .def("getSource", [](const SurgePyModRouting &r) { return r.source; })
        .def("getDest", [](const SurgePyModRouting &r) { return r.dest; })
        .def("getSourceScene", [](const SurgePyModRouting &r) { return r.source_scene; })
        .def("getSourceIndex", [](const SurgePyModRouting &r) { return r.source_index; })
        .def("getDepth", [](const SurgePyModRouting &r) { return r.depth; })
        .def("getNormalizedDepth", [](const SurgePyModRouting &r) { return r.normalizedDepth; })
        .def("__repr__", [](const SurgePyModRouting &r) {
            std::ostringstream oss;
            oss << "<SurgeModRouting src='" << r.source.name << "' dest='" << r.dest.name
                << "' depth=" << r.depth << " normdepth=" << r.normalizedDepth << ">";
            return oss.str();
        });

    py::module m_const =
        m.def_submodule("constants", "Constants which are used to navigate Surge XT");

#define C(x) m_const.attr(#x) = py::int_((int)(x));
    C(cg_GLOBAL);
    C(cg_OSC);
    C(cg_MIX);
    C(cg_FILTER);
    C(cg_ENV);
    C(cg_LFO);
    C(cg_FX);

    C(ms_velocity);
    C(ms_releasevelocity);
    C(ms_keytrack);
    C(ms_lowest_key);
    C(ms_highest_key);
    C(ms_latest_key);
    C(ms_polyaftertouch);
    C(ms_aftertouch);
    C(ms_modwheel);
    C(ms_breath);
    C(ms_expression);
    C(ms_sustain);
    C(ms_pitchbend);
    C(ms_timbre);
    C(ms_alternate_bipolar);
    C(ms_alternate_unipolar);
    C(ms_random_bipolar);
    C(ms_random_unipolar);
    C(ms_filtereg);
    C(ms_ampeg);
    C(ms_lfo1);
    C(ms_lfo2);
    C(ms_lfo3);
    C(ms_lfo4);
    C(ms_lfo5);
    C(ms_lfo6);
    C(ms_slfo1);
    C(ms_slfo2);
    C(ms_slfo3);
    C(ms_slfo4);
    C(ms_slfo5);
    C(ms_slfo6);
    C(ms_ctrl1);
    C(ms_ctrl2);
    C(ms_ctrl3);
    C(ms_ctrl4);
    C(ms_ctrl5);
    C(ms_ctrl6);
    C(ms_ctrl7);
    C(ms_ctrl8);

    C(ot_classic);
    C(ot_sine);
    C(ot_wavetable);
    C(ot_shnoise);
    C(ot_audioinput);
    C(ot_FM3);
    C(ot_FM2);
    C(ot_window);

    C(adsr_ampeg);
    C(adsr_filteg);

    C(fxt_off);
    C(fxt_delay);
    C(fxt_reverb);
    C(fxt_phaser);
    C(fxt_rotaryspeaker);
    C(fxt_distortion);
    C(fxt_eq);
    C(fxt_freqshift);
    C(fxt_conditioner);
    C(fxt_chorus4);
    C(fxt_vocoder);
    C(fxt_reverb2);
    C(fxt_flanger);
    C(fxt_ringmod);
    C(fxt_airwindows);
    C(fxt_neuron);

    C(fxslot_ains1);
    C(fxslot_ains2);
    C(fxslot_bins1);
    C(fxslot_bins2);
    C(fxslot_send1);
    C(fxslot_send2);
    C(fxslot_global1);
    C(fxslot_global2);

    C(pm_poly);
    C(pm_mono);
    C(pm_mono_st);
    C(pm_mono_fp);
    C(pm_mono_st_fp);
    C(pm_latch);

    C(sm_single);
    C(sm_split);
    C(sm_dual);
    C(sm_chsplit);

    C(fc_serial1);
    C(fc_serial2);
    C(fc_serial3);
    C(fc_dual1);
    C(fc_dual2);
    C(fc_stereo);
    C(fc_ring);
    C(fc_wide);

    C(fm_off);
    C(fm_2to1);
    C(fm_3to2to1);
    C(fm_2and3to1);

    C(lt_sine);
    C(lt_tri);
    C(lt_square);
    C(lt_ramp);
    C(lt_noise);
    C(lt_snh);
    C(lt_envelope);
    C(lt_mseg);
    C(lt_formula);

    {
        using sst::waveshapers::WaveshaperType;
        C(WaveshaperType::wst_none);
        C(WaveshaperType::wst_soft);
        C(WaveshaperType::wst_hard);
        C(WaveshaperType::wst_asym);
        C(WaveshaperType::wst_sine);
        C(WaveshaperType::wst_digital);
    }

    {
        using sst::filters::FilterType;
        C(FilterType::fut_none);
        C(FilterType::fut_lp12);
        C(FilterType::fut_lp24);
        C(FilterType::fut_lpmoog);
        C(FilterType::fut_hp12);
        C(FilterType::fut_hp24);
        C(FilterType::fut_bp12);
        C(FilterType::fut_notch12);
        C(FilterType::fut_comb_pos);
        C(FilterType::fut_SNH);
        C(FilterType::fut_vintageladder);
        C(FilterType::fut_obxd_2pole_lp);
        C(FilterType::fut_obxd_4pole);
        C(FilterType::fut_k35_lp);
        C(FilterType::fut_k35_hp);
        C(FilterType::fut_diode);
        C(FilterType::fut_cutoffwarp_lp);
        C(FilterType::fut_cutoffwarp_hp);
        C(FilterType::fut_cutoffwarp_n);
        C(FilterType::fut_cutoffwarp_bp);
        C(FilterType::fut_obxd_2pole_hp);
        C(FilterType::fut_obxd_2pole_n);
        C(FilterType::fut_obxd_2pole_bp);
        C(FilterType::fut_bp24);
        C(FilterType::fut_notch24);
        C(FilterType::fut_comb_neg);
        C(FilterType::fut_apf);
        C(FilterType::fut_cutoffwarp_ap);
        C(FilterType::fut_resonancewarp_lp);
        C(FilterType::fut_resonancewarp_hp);
        C(FilterType::fut_resonancewarp_n);
        C(FilterType::fut_resonancewarp_bp);
        C(FilterType::fut_resonancewarp_ap);
        C(FilterType::fut_tripole);
    }

    py::enum_<SurgeStorage::TuningApplicationMode>(m, "TuningApplicationMode")
        .value("RETUNE_ALL", SurgeStorage::TuningApplicationMode::RETUNE_ALL)
        .value("RETUNE_MIDI_ONLY", SurgeStorage::TuningApplicationMode::RETUNE_MIDI_ONLY);
}
