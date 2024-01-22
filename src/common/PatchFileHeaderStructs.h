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

#ifndef SURGE_SRC_COMMON_PATCHFILEHEADERSTRUCTS_H
#define SURGE_SRC_COMMON_PATCHFILEHEADERSTRUCTS_H

namespace sst::io
{

#pragma pack(push, 1)
struct fxChunkSetCustom
{
    int chunkMagic; // 'CcnK'
    int byteSize;   // of this chunk, excl. magic + byteSize

    int fxMagic; // 'FPCh'
    int version;
    int fxID; // fx unique id
    int fxVersion;

    int numPrograms;
    char prgName[28];

    int chunkSize;
    // char chunk[8]; // variable
};

struct patch_header
{
    char tag[4];
    // TODO: FIX SCENE AND OSC COUNT ASSUMPTION for wtsize
    // (but also since it's used in streaming, do it with care!)
    unsigned int xmlsize, wtsize[2][3];
};
#pragma pack(pop)
} // namespace sst::io
#endif // SURGE_FXPHEADERSTRUCTS_H
