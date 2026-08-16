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

#define WAV_STDOUT_INFO 0

#include <stdio.h>
#include "SurgeStorage.h"
#include <sstream>
#include <cerrno>
#include <cstring>
#include <algorithm>

#include "filesystem/import.h"

// Sigh - lets write a portable ntol by hand
unsigned int pl_int(char *d)
{
    return (unsigned char)d[0] + (((unsigned char)d[1]) << 8) + (((unsigned char)d[2]) << 16) +
           (((unsigned char)d[3]) << 24);
}

unsigned short pl_short(char *d) { return (unsigned char)d[0] + (((unsigned char)d[1]) << 8); }

bool four_chars(char *v, char a, char b, char c, char d)
{
    return v[0] == a && v[1] == b && v[2] == c && v[3] == d;
}

bool SurgeStorage::load_wt_wav_portable(std::string fn, Wavetable *wt, std::string &metadata)
{
    std::string uitag = "Wavetable Import Error";

    metadata = {};

#if WAV_STDOUT_INFO
    std::cout << "Loading wt_wav_portable" << std::endl;
    std::cout << "  fn = '" << fn << "'" << std::endl;
#endif

    std::filebuf fp;

    if (!fp.open(string_to_path(fn), std::ios::binary | std::ios::in))
    {
        std::ostringstream oss;
        oss << "Unable to open file '" << fn << "'!";
        reportError(oss.str(), uitag);
        return false;
    }

    char riff[4], szd[4], wav[4];
    auto hds = fp.sgetn(riff, sizeof(riff));

    hds += fp.sgetn(szd, sizeof(szd));
    hds += fp.sgetn(wav, sizeof(wav));

    if (hds != 12)
    {
        std::ostringstream oss;
        oss << "'" << fn << "' does not contain a valid RIFF header chunk!";
        reportError(oss.str(), uitag);
        return false;
    }

    // an AVI shares the outer RIFF header, so check the form type too
    if (!four_chars(riff, 'R', 'I', 'F', 'F') || !four_chars(wav, 'W', 'A', 'V', 'E'))
    {
        std::ostringstream oss;
        oss << "'" << fn << "' is not a standard RIFF/WAVE file. Header is: " << riff[0] << riff[1]
            << riff[2] << riff[3] << " " << wav[0] << wav[1] << wav[2] << wav[3] << ".";
        reportError(oss.str(), uitag);
        return false;
    }

    // WAV HEADER
    unsigned short audioFormat, numChannels{1};
    unsigned int sampleRate, byteRate;
    unsigned short blockAlign{0}, bitsPerSample{0};

    // Result of data read
    bool hasFMT = false;
    bool hasSMPL = false;
    bool hasCLM = false;
    bool hasCUE = false;
    bool hasSRGE = false;
    bool hasSRGO = false;
    int clmLEN = 0;
    int smplLEN = 0;
    int cueLEN = 0;
    int srgeLEN = 0;

    // Now start reading chunks
    int tbr = 4;
    char *wavdata{nullptr};
    int datasz{0}, datasamples{0};

    while (true)
    {
        char chunkType[4], chunkSzD[4];
        int br;

        if (fp.sgetn(chunkType, sizeof(chunkType)) != sizeof(chunkType))
        {
            break;
        }

        br = fp.sgetn(chunkSzD, sizeof(chunkSzD));

        // FIXME - deal with br
        int cs = pl_int(chunkSzD);

        // RIFF requires all chunks to be in 2 byte sizes
        if (cs % 2 == 1)
            cs = cs + 1;

#if WAV_STDOUT_INFO
        std::cout << "  CHUNK  \'";
        for (int i = 0; i < 4; ++i)
            std::cout << chunkType[i];
        std::cout << "\'  sz = " << cs << std::endl;
#endif

        // chunk size comes from the file; refuse one we cannot represent as an int
        if (cs < 0)
        {
            break;
        }

        tbr += 8 + cs;

        char *data = (char *)malloc(cs);
        br = fp.sgetn(data, cs);

        if (br != cs)
        {
            free(data);

            break;
        }

        // each branch reads fixed bytes out of a file-declared size; check it fits
        if (four_chars(chunkType, 'f', 'm', 't', ' '))
        {
            if (cs < 16)
            {
                free(data);

                std::ostringstream oss;
                oss << "'" << fn << "' has a format chunk of only " << cs
                    << " bytes, which is too short to describe a WAV file.";
                reportError(oss.str(), uitag);

                return false;
            }

            hasFMT = true;

            char *dp = data;
            audioFormat = pl_short(dp);
            dp += 2; // 1 is PCM; 3 is IEEE Float
            numChannels = pl_short(dp);
            dp += 2;
            sampleRate = pl_int(dp);
            dp += 4;
            byteRate = pl_int(dp);
            dp += 4;
            blockAlign = pl_short(dp);
            dp += 2;
            bitsPerSample = pl_short(dp);
            dp += 2;

#if WAV_STDOUT_INFO
            std::cout << "     FMT = " << audioFormat << " x " << numChannels << " at "
                      << bitsPerSample << " bits" << std::endl;
#endif

            free(data);

            // Do a format check here to bail out
            // numChannels is part of this check because the data chunk divides by it
            if (!((((audioFormat == 1 /* WAVE_FORMAT_PCM */) && (bitsPerSample == 16)) ||
                   ((audioFormat == 3 /* IEEE_FLOAT */) && (bitsPerSample == 32))) &&
                  numChannels > 0))
            {
                std::string formname = "Unknown (" + std::to_string(audioFormat) + ")";

                if (audioFormat == 1)
                    formname = "PCM";
                if (audioFormat == 3)
                    formname = "float";

                std::ostringstream oss;

                oss << "Currently, Surge XT only supports 16-bit PCM or 32-bit float mono WAV "
                       "files. You have provided a "
                    << bitsPerSample << "-bit " << formname << " " << numChannels
                    << "-channel file.";

                reportError(oss.str(), uitag);
                return false;
            }
        }
        else if (four_chars(chunkType, 'c', 'l', 'm', ' '))
        {
            // These all begin '<!>dddd' where d is 2048 it seems
            char *dp = data + 3;
            if (cs >= 7 && four_chars(dp, '2', '0', '4', '8'))
            {
                // 2048 CLM detected
                hasCLM = true;
                clmLEN = 2048;
            }
            free(data);
        }
        else if (four_chars(chunkType, 'u', 'h', 'W', 'T'))
        {
            // This is HIVE metadata so treat it just like CLM / Serum
            hasCLM = true;
            clmLEN = 2048;

            free(data);
        }
        else if (four_chars(chunkType, 's', 'r', 'g', 'e'))
        {
            if (cs >= 8)
            {
                hasSRGE = true;
                char *dp = data;
                int version = pl_int(dp);
                dp += 4;
                srgeLEN = pl_int(dp);
            }
            free(data);
        }
        else if (four_chars(chunkType, 's', 'r', 'g', 'o'))
        {
            if (cs >= 8)
            {
                hasSRGO = true;
                char *dp = data;
                int version = pl_int(dp);
                dp += 4;
                srgeLEN = pl_int(dp);
            }
            free(data);
        }
        else if (four_chars(chunkType, 'c', 'u', 'e', ' '))
        {
            char *dp = data;
            int numCues = cs >= 4 ? pl_int(dp) : 0;

            dp += 4;

            // trust the chunk size over the cue count it declares
            const int cuesInChunk = cs >= 4 ? (cs - 4) / 24 : 0;

            if (numCues < 0 || numCues > cuesInChunk)
                numCues = cuesInChunk;

            std::vector<int> chunkStarts;

            for (int i = 0; i < numCues; ++i)
            {
                for (int j = 0; j < 6; ++j)
                {
                    auto d = pl_int(dp);

                    if (j == 5)
                        chunkStarts.push_back(d);

                    dp += 4;
                }
            }

            // Now are my chunkstarts regular
            int d = -1;
            bool regular = true;

            for (auto i = 1; i < chunkStarts.size(); ++i)
            {
                if (d == -1)
                    d = chunkStarts[i] - chunkStarts[i - 1];
                else
                {
                    if (d != chunkStarts[i] - chunkStarts[i - 1])
                        regular = false;
                }
            }

            if (regular)
            {
                hasCUE = true;
                cueLEN = d;
            }

            free(data);
        }
        else if (four_chars(chunkType, 'd', 'a', 't', 'a'))
        {
            datasz = cs;

            // these divisors are only set by the format chunk, so require one
            if (!hasFMT)
            {
                std::ostringstream oss;

                oss << "'" << fn
                    << "' has a data chunk but no format chunk to describe it, so it is not a "
                       "WAV file Surge XT can read.";
                reportError(oss.str(), uitag);
                free(data);

                return false;
            }

            datasamples = cs * 8 / bitsPerSample / numChannels;
            wavdata = data;
        }
        else if (four_chars(chunkType, 'w', 't', 'm', 'd'))
        {
            datasz = cs;
            // not null terminated, so bound the string by the chunk
            metadata = std::string(data, std::find(data, data + cs, 0));
            free(data);
        }
        else if (four_chars(chunkType, 's', 'm', 'p', 'l'))
        {
            if (cs < 36)
            {
                free(data);
                continue;
            }

            char *dp = data;
            unsigned int samplechunk[9];

            for (int i = 0; i < 9; ++i)
            {
                samplechunk[i] = pl_int(dp);
                dp += 4;
            }

            unsigned int nloops = samplechunk[7];
            unsigned int sdsz = samplechunk[8];

#if WAV_STDOUT_INFO
            std::cout << "   SMPL: nloops = " << nloops << " " << sdsz << std::endl;
#endif

            if (nloops == 0)
            {
                // It seems RAPID uses a smpl block with no samples to indicate a 2048.
                hasSMPL = true;
                smplLEN = 2048;
            }

            if (nloops > 1)
            {
                // We don't support this. Indicate somehow.
                // FIXME
            }

            // the loop record which follows the 36 byte header is another six words
            for (int i = 0; i < nloops && i < 1 && cs >= 60; ++i)
            {
                unsigned int loopdata[6];

                for (int j = 0; j < 6; ++j)
                {
                    loopdata[j] = pl_int(dp);
                    dp += 4;

#if WAV_STDOUT_INFO
                    std::cout << "      loopdata[" << j << "] = " << loopdata[j] << std::endl;
#endif
                }

                hasSMPL = true;
                smplLEN = loopdata[3] - loopdata[2] + 1;

                if (smplLEN == 0)
                    smplLEN = 2048;
            }

            free(data);
        }
        else
        {
#if WAV_STDOUT_INFO
            std::cout << "Default Dump\n";

            for (int i = 0; i < cs; ++i)
                std::cout << data[i] << std::endl;
#endif

            free(data);
        }
    }

    // name the missing format chunk rather than blaming the loop points
    if (!hasFMT)
    {
        std::ostringstream oss;
        oss << "'" << fn << "' contains no format chunk, so it is not a readable WAV file.";
        reportError(oss.str(), uitag);

        if (wavdata)
            free(wavdata);

        return false;
    }

#if WAV_STDOUT_INFO
    std::cout << "  hasCLM  = " << hasCLM << " / " << clmLEN << std::endl;
    std::cout << "  hasCUE  = " << hasCUE << " / " << cueLEN << std::endl;
    std::cout << "  hasSMPL = " << hasSMPL << " / " << smplLEN << std::endl;
    std::cout << "  hasSRGE = " << hasSRGE << " / " << srgeLEN << std::endl;
#endif

    bool loopData = hasSMPL || hasCLM || hasSRGE;
    int loopLen =
        hasCLM ? clmLEN : (hasCUE ? cueLEN : (hasSRGE ? srgeLEN : (hasSMPL ? smplLEN : -1)));
    bool fullRange = false;

    if (loopLen == -1 && wt->frame_size_if_absent > 0)
    {
        loopLen = wt->frame_size_if_absent;
        loopData = true;
        fullRange = true;
        wt->frame_size_if_absent = -1;
    }
    if (loopLen == 0)
    {
        std::ostringstream oss;
        oss << "Surge XT cannot understand this particular .wav file. Please consult the Surge XT "
               "Wiki for information on .wav file metadata.";

        reportError(oss.str(), uitag);

        if (wavdata)
            free(wavdata);

        return false;
    }

    int loopCount = datasamples / loopLen;

#if WAV_STDOUT_INFO
    std::cout << "  samples = " << datasamples << " loop length = " << loopLen
              << " loop count = " << loopCount << std::endl;
#endif

    // wt format header (surge internal)
    wt_header wh;
    memset(&wh, 0, sizeof(wt_header));
    wh.flags = wtf_is_sample;

    int sh = 0;

    if (loopData)
    {
        wh.flags = 0;

        switch (loopLen)
        {
        case 4096:
            sh = 12;
            break;
        case 2048:
            sh = 11;
            break;
        case 1024:
            sh = 10;
            break;
        case 512:
            sh = 9;
            break;
        case 256:
            sh = 8;
            break;
        case 128:
            sh = 7;
            break;
        case 64:
            sh = 6;
            break;
        case 32:
            sh = 5;
            break;
        case 16:
            sh = 4;
            break;
        case 8:
            sh = 3;
            break;
        case 4:
            sh = 2;
            break;
        case 2:
            sh = 1;
            break;
        default:
#if WAV_STDOUT_INFO
            std::cout << "Setting style to sample" << std::endl;
#endif
            wh.flags = wtf_is_sample;
            break;
        }
    }

    if (loopLen != -1 && (sh == 0 || loopCount < 1))
    {
        std::ostringstream oss;
        oss << "Currently, Surge XT only supports wavetables with at least a single frame, with up "
               "to 4096 samples per frame in power-of-two increments. You have provided a "
               "wavetable with "
            << loopCount << (loopCount == 1 ? " frame" : " frames") << " of " << loopLen
            << " samples. '" << fn << "'";
        reportError(oss.str(), uitag);

        if (wavdata)
            free(wavdata);

        return false;
    }

    wh.n_samples = 1 << sh;
    int mask = wt->size - 1;
    int sample_length = std::min(datasamples, max_wtable_size * max_subtables);
    wh.n_tables = std::min(max_subtables, (sample_length >> sh));

    if (wh.flags & wtf_is_sample)
    {
        auto windowSize = 1024;

        if (hasSRGO)
            windowSize = srgeLEN;

        while (windowSize * 4 > sample_length && windowSize > 8)
            windowSize = windowSize / 2;

        wh.n_samples = windowSize;
        // Clamp so BuildWT stays within the fixed-size tables. Samples are built
        // with AppendSilence, which adds 3 frames, so leave room for those.
        wh.n_tables = std::min(max_subtables - 3, (int)(sample_length / windowSize));
    }

    int channels = 1;

    if ((audioFormat == 1 /* WAVE_FORMAT_PCM */) && (bitsPerSample == 16))
    {
        // assert(wh.n_samples * wh.n_tables * 2 <= datasz);
        wh.flags |= wtf_int16;
    }
    else if ((audioFormat == 3 /* WAVE_FORMAT_IEEE_FLOAT */) && (bitsPerSample == 32))
    {
        // assert(wh.n_samples * wh.n_tables * 4 <= datasz);
    }
    else
    {
        std::ostringstream oss;
        oss << "Currently, Surge XT only supports 16-bit PCM or 32-bit floating point WAV files. "
            << " You provided a " << bitsPerSample << "-bit" << audioFormat << " file.";

        reportError(oss.str(), uitag);

        if (wavdata)
        {
            free(wavdata);
        }

        return false;
    }

    if (wavdata && wt)
    {
        if (fullRange && (wh.flags & wtf_int16))
        {
            wh.flags |= wtf_int16_is_16;
        }
        if (numChannels == 1)
        {
            waveTableDataMutex.lock();
            wt->BuildWT(wavdata, wh, wh.flags & wtf_is_sample);
            waveTableDataMutex.unlock();
        }
        else
        {
            char *leftChannel;
            auto memsize = datasamples * bitsPerSample / 8;

            leftChannel = (char *)malloc(memsize * sizeof(char));

            for (size_t i = 0; i < datasamples; ++i)
            {
                size_t destAddr = i * bitsPerSample / 8;
                size_t srcAddr = i * bitsPerSample / 8 * numChannels;

                std::memcpy(leftChannel + destAddr, wavdata + srcAddr, bitsPerSample / 8);
            }

            waveTableDataMutex.lock();
            wt->BuildWT(leftChannel, wh, wh.flags & wtf_is_sample);
            waveTableDataMutex.unlock();

            free(leftChannel);
        }

        free(wavdata);
    }

    return true;
}

std::string SurgeStorage::export_wt_wav_portable(const fs::path &fname, Wavetable *wt,
                                                 const std::string &metadata, bool exportForSerum)
{
    std::string errorMessage = "Unknown error!";
    std::string clm_string;

    {
        std::filebuf wfp;

        if (!wfp.open(fname, std::ios::binary | std::ios::out))
        {
            errorMessage = "Unable to open file " + fname.u8string() + "!";
            errorMessage += std::strerror(errno);

            reportError(errorMessage, "Export Error");

            return "";
        }

        auto audioFormat = 3;
        auto bitsPerSample = 32;
        auto sampleRate = 44100;
        auto nChannels = 1;

        auto w4i = [&wfp](unsigned int v) {
            unsigned char fi[4];
            for (int i = 0; i < 4; ++i)
            {
                fi[i] = (unsigned char)(v & 255);
                v = v / 256;
            }
            wfp.sputn(reinterpret_cast<char *>(fi), sizeof(fi));
        };

        auto w2i = [&wfp](unsigned int v) {
            unsigned char fi[2];
            for (int i = 0; i < 2; ++i)
            {
                fi[i] = (unsigned char)(v & 255);
                v = v / 256;
            }
            wfp.sputn(reinterpret_cast<char *>(fi), sizeof(fi));
        };

        wfp.sputn("RIFF", 4);

        bool isSample = false;
        if (wt->flags & wtf_is_sample)
            isSample = true;

        // OK so how much data do I have.
        unsigned int tableSize = nChannels * bitsPerSample / 8 * wt->n_tables * wt->size;
        unsigned int dataSize = 4 +                 // 'WAVE'
                                4 + 4 + 18 +        // fmt chunk
                                4 + 4 + tableSize + // data chunk
                                4 + 4 + 8;          // srgo/srge chunk

        // Serum's clm chunk description:
        // https://www.kvraudio.com/forum/viewtopic.php?p=7266103&sid=708f193e517d29fe94c1c974e911af97#p7266103
        if (exportForSerum)
        {
            std::ostringstream clm_string_stream;
            clm_string_stream
                << "<!>" << wt->size << " 10000000" // Flag set for linear interpolation
                << " wavetable Surge-XT"; // No space and hyphen to keep string length even
            clm_string = clm_string_stream.str();
            dataSize += 4 + 4 + clm_string.length() + 1; // null term
        }

        if (!metadata.empty())
            dataSize += 4 + 4 + metadata.length() + 1; // null term

        w4i(dataSize);
        wfp.sputn("WAVE", 4);

        // OK so format chunk
        wfp.sputn("fmt ", 4);
        w4i(16);
        w2i(audioFormat);
        w2i(nChannels);
        w4i(sampleRate);
        w4i(sampleRate * nChannels * bitsPerSample);
        w2i(bitsPerSample * nChannels);
        w2i(bitsPerSample);

        if (!isSample)
        {
            wfp.sputn("srge", 4);
            w4i(8);
            w4i(1);
            w4i(wt->size);
        }
        else
        {
            wfp.sputn("srgo", 4); // o for oneshot
            w4i(8);
            w4i(1);
            w4i(wt->size);
        }

        if (exportForSerum)
        {
            wfp.sputn("clm ", 4);
            w4i(clm_string.length() + 1);
            wfp.sputn(clm_string.c_str(), clm_string.length() + 1);
        }

        wfp.sputn("data", 4);
        w4i(tableSize);

        for (int i = 0; i < wt->n_tables; ++i)
        {
            wfp.sputn(reinterpret_cast<char *>(wt->TableF32WeakPointers[0][i]),
                      wt->size * bitsPerSample / 8);
        }

        if (!metadata.empty())
        {
            wfp.sputn("wtmd", 4);
            w4i(metadata.length() + 1);
            wfp.sputn(metadata.c_str(), metadata.length() + 1);
        }
    }

    return path_to_string(fname);
}

// Writes a single wavetable frame as a mono 32-bit float WAV.
static bool write_single_frame_wav(const fs::path &fname, Wavetable *wt, int frame)
{
    std::filebuf wfp;

    if (!wfp.open(fname, std::ios::binary | std::ios::out))
    {
        return false;
    }

    auto w4i = [&wfp](unsigned int v) {
        unsigned char fi[4];
        for (int i = 0; i < 4; ++i)
        {
            fi[i] = (unsigned char)(v & 255);
            v = v / 256;
        }
        wfp.sputn(reinterpret_cast<char *>(fi), sizeof(fi));
    };

    auto w2i = [&wfp](unsigned int v) {
        unsigned char fi[2];
        for (int i = 0; i < 2; ++i)
        {
            fi[i] = (unsigned char)(v & 255);
            v = v / 256;
        }
        wfp.sputn(reinterpret_cast<char *>(fi), sizeof(fi));
    };

    const unsigned int audioFormat = 3; // IEEE float
    const unsigned int bitsPerSample = 32;
    const unsigned int sampleRate = 44100;
    const unsigned int nChannels = 1;
    const unsigned int bytesPerSample = bitsPerSample / 8;

    unsigned int tableSize = nChannels * bytesPerSample * wt->size;
    unsigned int dataSize = 4 +                // 'WAVE'
                            4 + 4 + 16 +       // fmt chunk (id + size + 16 bytes)
                            4 + 4 + 8 +        // srge chunk (id + size + 8 bytes)
                            4 + 4 + tableSize; // data chunk (id + size + data)

    wfp.sputn("RIFF", 4);
    w4i(dataSize);
    wfp.sputn("WAVE", 4);

    wfp.sputn("fmt ", 4);
    w4i(16);
    w2i(audioFormat);
    w2i(nChannels);
    w4i(sampleRate);
    w4i(sampleRate * nChannels * bytesPerSample); // byte rate
    w2i(nChannels * bytesPerSample);              // block align
    w2i(bitsPerSample);

    // Single-cycle marker so the frame reimports as one cycle of wt->size samples.
    wfp.sputn("srge", 4);
    w4i(8);
    w4i(1); // version
    w4i(wt->size);

    wfp.sputn("data", 4);
    w4i(tableSize);
    wfp.sputn(reinterpret_cast<char *>(wt->TableF32WeakPointers[0][frame]),
              wt->size * bytesPerSample);

    return wfp.pubsync() == 0;
}

std::string SurgeStorage::export_wt_wav_frames_portable(const fs::path &parentDir,
                                                        const std::string &baseName, Wavetable *wt)
{
    // Land each export in its own guaranteed-empty subfolder, inserting a number if the folder
    // already exists so frames from different exports never mix. The frame files reuse the same
    // numbered name.
    std::string folderName = baseName;
    std::error_code ec;

    // string_to_path (fs::u8path) keeps unicode names intact.
    fs::path dir = parentDir / string_to_path(folderName + " - Frames");
    int fnum = 2;

    // Non-throwing exists() so a filesystem error can't escape the FileChooser callback.
    while (fs::exists(dir, ec))
    {
        if (fnum > 99)
        {
            reportError("Unable to find a free export folder for " + baseName + "!",
                        "Export Error");
            return "";
        }
        folderName = baseName + " " + std::to_string(fnum);
        dir = parentDir / string_to_path(folderName + " - Frames");
        fnum++;
    }

    fs::create_directories(dir, ec);

    if (ec)
    {
        reportError("Unable to create export folder " + dir.u8string() + "!", "Export Error");
        return "";
    }

    for (int i = 0; i < wt->n_tables; ++i)
    {
        char idx[8];
        snprintf(idx, sizeof(idx), "%03d", i + 1);

        auto fname = dir / string_to_path(folderName + " " + idx + ".wav");

        if (!write_single_frame_wav(fname, wt, i))
        {
            reportError("Unable to write wavetable frame to " + fname.u8string() + "!",
                        "Export Error");
            return "";
        }
    }

    return path_to_string(dir);
}
