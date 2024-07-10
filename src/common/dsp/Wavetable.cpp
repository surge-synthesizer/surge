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
#include "Wavetable.h"
#include <assert.h>
#include "DSPUtils.h"
#include <vembertech/basic_dsp.h>
#include "SurgeStorage.h"

#include "sst/basic-blocks/mechanics/endian-ops.h"
namespace mech = sst::basic_blocks::mechanics;

#if WINDOWS
#include <intrin.h>
#endif

using namespace std;

const float hrfilter[63] = {
    -9.637663112e-008f, -2.216513622e-006f, -1.200509132e-006f, 1.79627641e-005f,
    1.773084477e-005f,  -5.898886593e-005f, -8.980041457e-005f, 0.0001233910152f,
    0.0002964516752f,   -0.0001573183545f,  -0.0007465034723f,  1.204636671e-018f,
    0.001525280299f,    0.0006605535164f,   -0.002588451374f,   -0.002282966627f,
    0.003618633142f,    0.005384810269f,    -0.003885820275f,   -0.01036664937f,
    0.002154163085f,    0.0172905419f,      0.003383208299f,    -0.02569983155f,
    -0.01536878385f,    0.03457865119f,     0.0387589559f,      -0.04251147807f,
    -0.0895993337f,     0.04802387953f,     0.3125254214f,      0.4499996006f,
    0.3125254214f,      0.04802387953f,     -0.0895993337f,     -0.04251147807f,
    0.0387589559f,      0.03457865119f,     -0.01536878385f,    -0.02569983155f,
    0.003383208299f,    0.0172905419f,      0.002154163085f,    -0.01036664937f,
    -0.003885820275f,   0.005384810269f,    0.003618633142f,    -0.002282966627f,
    -0.002588451374f,   0.0006605535164f,   0.001525280299f,    1.204636671e-018f,
    -0.0007465034723f,  -0.0001573183545f,  0.0002964516752f,   0.0001233910152f,
    -8.980041457e-005f, -5.898886593e-005f, 1.773084477e-005f,  1.79627641e-005f,
    -1.200509132e-006f, -2.216513622e-006f, -9.637663112e-008};

const int HRFilterI16[64] = {
    1,   33,   -8,    -48,   31,   72,    -74,   -92,  143,   95,    -240, -66, 364,
    -14, -505, 168,   642,   -416, -748,  779,   782,  -1279, -687,  1951, 375, -2874,
    331, 4293, -1957, -7315, 7773, 31275, 31275, 7773, -7315, -1957, 4293, 331, -2874,
    375, 1951, -687,  -1279, 782,  779,   -748,  -416, 642,   168,   -505, -14, 364,
    -66, -240, 95,    143,   -92,  -74,   72,    31,   -48,   -8,    33,   1};

int min_F32_tables = 3;

#if MAC || LINUX
bool _BitScanReverse(unsigned int *result, unsigned int bits)
{
    *result = __builtin_ctz(bits);
    return true;
}
#endif

//! Calculate the worst-case scenario of the needed samples for a specific wavetable and see if it
//! fits
size_t RequiredWTSize(int TableSize, int TableCount)
{
    int Size = 0;

    TableCount += 3; // for sample padding. Should match the "3" in the AppendSilence block below.
    while (TableSize > 0)
    {
        Size += TableCount * (TableSize + FIRoffsetI16 + FIRipolI16_N);

        TableSize = TableSize >> 1;
    }
    return Size;
}

int GetWTIndex(int WaveIdx, int WaveSize, int NumWaves, int MipMap, int Padding = 0)
{
    int Index = WaveIdx * ((WaveSize >> MipMap) + Padding);
    int Offset = NumWaves * WaveSize;
    for (int i = 0; i < MipMap; i++)
    {
        Index += Offset >> i;
        Index += Padding * NumWaves;
    }
    return Index;
}

Wavetable::Wavetable()
{
    dataSizes = 35000;
    TableF32Data = (float *)malloc(dataSizes * sizeof(float));
    TableI16Data = (short *)malloc(dataSizes * sizeof(short));
    memset(TableF32Data, 0, dataSizes * sizeof(float));
    memset(TableI16Data, 0, dataSizes * sizeof(short));
    memset(TableF32WeakPointers, 0, sizeof(TableF32WeakPointers));
    memset(TableI16WeakPointers, 0, sizeof(TableI16WeakPointers));
    current_id = -1;
    queue_id = -1;
    everBuilt = false;
    refresh_display = true; // I have never been drawn so assume I need refresh if asked
}

Wavetable::~Wavetable()
{
    free(TableF32Data);
    free(TableI16Data);
}

void Wavetable::allocPointers(size_t newSize)
{
    free(TableF32Data);
    free(TableI16Data);
    dataSizes = newSize;
    TableF32Data = (float *)malloc(dataSizes * sizeof(float));
    TableI16Data = (short *)malloc(dataSizes * sizeof(short));
    memset(TableF32Data, 0, dataSizes * sizeof(float));
    memset(TableI16Data, 0, dataSizes * sizeof(short));
}

void Wavetable::Copy(Wavetable *wt)
{
    size = wt->size;
    size_po2 = wt->size_po2;
    flags = wt->flags;
    dt = wt->dt;
    n_tables = wt->n_tables;

    current_id = -1;
    queue_id = -1;
    everBuilt = wt->everBuilt;

    if (dataSizes < wt->dataSizes)
    {
        allocPointers(wt->dataSizes);
    }

    memcpy(TableF32Data, wt->TableF32Data, dataSizes * sizeof(float));
    memcpy(TableI16Data, wt->TableI16Data, dataSizes * sizeof(short));

    for (int i = 0; i < max_mipmap_levels; i++)
    {
        for (int j = 0; j < max_subtables; j++)
        {
            if (wt->TableF32WeakPointers[i][j])
            {
                size_t Offset = wt->TableF32WeakPointers[i][j] - wt->TableF32Data;
                TableF32WeakPointers[i][j] = TableF32Data + Offset;
            }
            else
                TableF32WeakPointers[i][j] = NULL;

            if (wt->TableI16WeakPointers[i][j])
            {
                size_t Offset = wt->TableI16WeakPointers[i][j] - wt->TableI16Data;
                TableI16WeakPointers[i][j] = TableI16Data + Offset;
            }
            else
                TableI16WeakPointers[i][j] = NULL;
        }
    }

    current_id = wt->current_id;
}

bool Wavetable::BuildWT(void *wdata, wt_header &wh, bool AppendSilence)
{
    assert(wdata);

    flags = mech::endian_read_int16LE(wh.flags);
    n_tables = mech::endian_read_int16LE(wh.n_tables);
    size = mech::endian_read_int32LE(wh.n_samples);

    size_t req_size = RequiredWTSize(size, n_tables);

    if (req_size > dataSizes)
    {
        allocPointers(req_size);
    }

    int wdata_tables = n_tables;

    if (AppendSilence)
    {
        n_tables += 3; // this "3" should match the "3" in RequiredWTSize
    }

#if WINDOWS
    unsigned long MSBpos;
    _BitScanReverse(&MSBpos, size);
#else
    unsigned int MSBpos;
    _BitScanReverse(&MSBpos, size);
#endif

    size_po2 = MSBpos;

    dt = 1.0f / size;

    for (int i = 0; i < max_mipmap_levels; i++)
    {
        for (int j = 0; j < max_subtables; j++)
        {
            // TODO WARNING: Crashes here with patchbyte!
            /*free(wt->TableF32WeakPointers[i][j]);
            free(wt->TableI16WeakPointers[i][j]);
            wt->TableF32WeakPointers[i][j] = 0;
            wt->TableI16WeakPointers[i][j] = 0;*/
        }
    }
    for (int j = 0; j < this->n_tables; j++)
    {
        TableF32WeakPointers[0][j] = TableF32Data + GetWTIndex(j, size, n_tables, 0);
        // + padding for a non-wrapping interpolator
        TableI16WeakPointers[0][j] = TableI16Data + GetWTIndex(j, size, n_tables, 0, FIRipolI16_N);
    }
    for (int j = this->n_tables; j < min_F32_tables; j++)
    {
        unsigned int s = this->size;
        int l = 0;

        while (s && (l < max_mipmap_levels))
        {
            TableF32WeakPointers[l][j] = TableF32Data + GetWTIndex(j, size, n_tables, l);
            memset(TableF32WeakPointers[l][j], 0, s * sizeof(float));
            s = s >> 1;
            l++;
        }
    }

    if (this->flags & wtf_int16)
    {
        for (int j = 0; j < wdata_tables; j++)
        {
            mech::endian_copyblock16LE(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                                       &((short *)wdata)[this->size * j], this->size);
            if (this->flags & wtf_int16_is_16)
            {
                i162float_block(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                                this->TableF32WeakPointers[0][j], this->size);
            }
            else
            {
                i152float_block(&this->TableI16WeakPointers[0][j][FIRoffsetI16],
                                this->TableF32WeakPointers[0][j], this->size);
            }
        }
    }
    else
    {
        for (int j = 0; j < wdata_tables; j++)
        {
            mech::endian_copyblock32LE((int32_t *)this->TableF32WeakPointers[0][j],
                                       &((int32_t *)wdata)[this->size * j], this->size);
            float2i15_block(this->TableF32WeakPointers[0][j],
                            &this->TableI16WeakPointers[0][j][FIRoffsetI16], this->size);
        }
    }

    // clear any appended tables (not read, but included in table for post-silence)
    for (int j = wdata_tables; j < this->n_tables; j++)
    {
        memset(this->TableF32WeakPointers[0][j], 0, this->size * sizeof(float));
        memset(this->TableI16WeakPointers[0][j], 0, (this->size + FIRoffsetI16) * sizeof(short));
    }

    for (int j = 0; j < wdata_tables; j++)
    {
        memcpy(&this->TableI16WeakPointers[0][j][this->size + FIRoffsetI16],
               &this->TableI16WeakPointers[0][j][FIRoffsetI16], FIRoffsetI16 * sizeof(short));
        memcpy(&this->TableI16WeakPointers[0][j][0], &this->TableI16WeakPointers[0][j][this->size],
               FIRoffsetI16 * sizeof(short));
    }

    MipMapWT();

    everBuilt = true;
    return true;
}

void Wavetable::MipMapWT()
{
    int levels = 1;
    while (((1 << levels) < size) & (levels < max_mipmap_levels))
        levels++;
    int ns = this->n_tables;

    const int filter_size = 63;
    const int filter_id_of = (filter_size - 1) >> 1;

    for (int l = 1; l < levels; l++)
    {
        int psize = size >> (l - 1);
        int lsize = size >> l;

        for (int s = 0; s < ns; s++)
        {
            this->TableF32WeakPointers[l][s] = TableF32Data + GetWTIndex(s, size, n_tables, l);
            this->TableI16WeakPointers[l][s] =
                TableI16Data + GetWTIndex(s, size, n_tables, l, FIRipolI16_N);

            if (this->flags & wtf_is_sample)
            {
                for (int i = 0; i < lsize; i++)
                {
                    this->TableF32WeakPointers[l][s][i] = 0;
                    for (int a = 0; a < filter_size; a++)
                    {
                        int srcindex = (i << 1) + a - filter_id_of;
                        int srctable = max(0, s + (srcindex / psize));
                        srcindex = srcindex & (psize - 1);
                        if (srctable < ns)
                            this->TableF32WeakPointers[l][s][i] +=
                                hrfilter[a] * this->TableF32WeakPointers[l - 1][srctable][srcindex];
                    }
                    this->TableI16WeakPointers[l][s][i + FIRoffsetI16] =
                        0; // not supported in int16 atm
                }
            }
            else
            {
                for (int i = 0; i < lsize; i++)
                {
                    this->TableF32WeakPointers[l][s][i] = 0;
                    for (int a = 0; a < filter_size; a++)
                    {
                        this->TableF32WeakPointers[l][s][i] +=
                            hrfilter[a] * this->TableF32WeakPointers[l - 1][s][(
                                              ((i << 1) + a - filter_id_of) & (psize - 1))];
                    }
                    int ival = 0;
                    for (int a = 0; a < filter_size; a++)
                    {
                        ival += HRFilterI16[a] *
                                this->TableI16WeakPointers[l - 1][s]
                                                          [(((i << 1) + a - 31) & (psize - 1)) +
                                                           FIRoffsetI16];
                    }
                    this->TableI16WeakPointers[l][s][i + FIRoffsetI16] = ival >> 16;
                }
            }
            // float2i16_block(this->TableF32WeakPointers[l][s],this->TableI16WeakPointers[l][s],lsize);
            auto toCopy = std::min(FIRoffsetI16, lsize);
            memcpy(&this->TableI16WeakPointers[l][s][lsize + FIRoffsetI16],
                   &this->TableI16WeakPointers[l][s][FIRoffsetI16], toCopy * sizeof(short));
            memcpy(&this->TableI16WeakPointers[l][s][0], &this->TableI16WeakPointers[l][s][lsize],
                   toCopy * sizeof(short));
        }
        // fwrite(this->TableI16WeakPointers[l][0],lsize*sizeof(short),1,F);
    }
    // fclose(F);

    // TODO I16 mipmaps end up out of phase
    // The click/knot/bug probably results from the fact that there is no padding in the beginning,
    // so it becomes out of phase at mipmap switch - makes sense because as they were off by a whole
    // sample at the mipmap switch, which cannot be explained by the half rate filter
}
