/*
** Portable (using standard fread and so on) support for .wav files generating
** wavetables. 
**
** Two things which matter in addition to the fmt and data block
**
**  1: The `smpl` block
**  2: the `clm ` block - indicates a serum file
**
** I read them in this order
**
**  1. If there is a clm block that wins, we ignore the smpl block, and you get your 2048 wt
**  2. If there is no smpl block and you have a smpl block if you have a power of 2 sample length
**     we interpret you as a wt otherwise as a one shot
*/


#include <stdio.h>
#include "UserInteractions.h"
#include "SurgeStorage.h"
#include <sstream>

// Sigh - lets write a portable ntol by hand
unsigned int pl_int(char *d)
{
    return (unsigned char)d[0] + (((unsigned char)d[1]) << 8) + (((unsigned char)d[2]) << 16) + (((unsigned char)d[3]) << 24);
}

unsigned short pl_short(char *d)
{
    return (unsigned char)d[0] + (((unsigned char)d[1]) << 8);
}

bool four_chars(char *v, char a, char b, char c, char d)
{
    return v[0] == a &&
        v[1] == b &&
        v[2] == c &&
        v[3] == d;
}

void SurgeStorage::load_wt_wav_portable(std::string fn, Wavetable *wt)
{
    std::cout << "Loading wt_wav_portable" << std::endl;
    std::cout << "  fn='" << fn << "'" << std::endl;

    FILE *fp = fopen(fn.c_str(), "rb");
    if( ! fp )
    {
        std::ostringstream oss;
        oss << "Unable to open file '" << fn << "'";
        Surge::UserInteractions::promptError(oss.str(), "WaveTable Error" );
        return;
    }

    char riff[4], szd[4], wav[4];
    auto hds = fread(riff, 1, 4, fp);
    hds += fread(szd, 1, 4, fp);
    hds += fread(wav, 1, 4, fp);
    if (hds != 12)
    {
       Surge::UserInteractions::promptError("File does not contain valid RIFF header chunk",
                                            "Wavetable Error");
       return;
    }

    if( ! four_chars(riff, 'R', 'I', 'F', 'F' ) &&
        ! four_chars(wav,  'W', 'A', 'V', 'E' ) )
    {
       std::ostringstream oss;
       oss << "File is not a standard RIFF/WAVE file. Header is: [" << riff[0] << riff[1] << riff[2]
           << riff[3] << " " << wav[0] << wav[1] << wav[2] << wav[3] << ".";
       Surge::UserInteractions::promptError(oss.str(), "WaveTable Error");
       return;
    }
    
    // WAV HEADER
    short audioFormat, numChannels;
    int sampleRate, byteRate;
    short blockAlign, bitsPerSample;

    // Result of data read
    bool hasSMPL = false;
    bool hasCLM = false;;
    int clmLEN = 0;
    int smplLEN = 0;
    
    // Now start reading chunks
    int tbr = 4;
    char *wavdata = nullptr;
    int datasz = 0, datasamples;
    while( true )
    {
        char chunkType[4], chunkSzD[4];
        int br;
        if( ! ( ( br = fread(chunkType, 1, 4, fp) ) == 4 ) )
        {
            break;
        }
        fread(chunkSzD, 1, 4, fp);
        std::cout << "  CHUNK  `"; for( int i=0; i<4; ++i ) std::cout << chunkType[i]; 
        int cs = pl_int(chunkSzD);
        std::cout << "`  sz=" << cs << std::endl;
        tbr += 8 + cs;
        
        char* data = (char *)malloc( cs );
        br = fread( data, 1, cs, fp );
        if( br != cs )
        {
            free(data);
            
            break;
        }

        if( four_chars( chunkType, 'f','m','t',' '))
        {
            char *dp = data;
            audioFormat = pl_short(dp); dp += 2; // 1 is PCM; 3 is IEEE Float
            numChannels = pl_short(dp); dp += 2;
            sampleRate = pl_int(dp); dp += 4;
            byteRate = pl_int(dp); dp += 4;
            blockAlign = pl_short(dp); dp += 2;
            bitsPerSample = pl_short(dp); dp += 2;

            free(data);           
        }
        else if( four_chars(chunkType, 'c', 'l', 'm', ' '))
        {
            // These all begin '<!>dddd' where d is 2048 it seems
            char *dp = data + 3;
            if( four_chars(dp, '2', '0', '4', '8' ) )
            {
                // 2048 CLM detected
                hasCLM = true;
                clmLEN = 2048;
            }
            free(data);
        }
        else if( four_chars(chunkType, 'd', 'a', 't', 'a' ))
        {
            datasz = cs;
            datasamples = cs * 8 / bitsPerSample / numChannels;
            wavdata = data;
        }
        else if( four_chars(chunkType, 's', 'm', 'p', 'l' ))
        {
            char *dp = data;
            unsigned int samplechunk[9];
            for( int i=0; i<9; ++i )
            {
                samplechunk[i] = pl_int(dp); dp += 4;
            }
            unsigned int nloops = samplechunk[7];
            unsigned int sdsz = samplechunk[8];

            if( nloops != 1 )
            {
                // We don't support this. Indicate somehow.
            }
            
            for( int i=0; i<nloops && i < 1; ++i )
            {
                unsigned int loopdata[6];
                for( int j=0; j<6; ++j )
                {
                    loopdata[j] = pl_int(dp); dp += 4;
                }
                hasSMPL = true;
                smplLEN = loopdata[3] - loopdata[2] + 1;
            }
        }
        else
        {
            free(data);
        }
    }


    bool loopData = hasSMPL || hasCLM;
    int loopLen = hasCLM ? clmLEN : smplLEN;
    int loopCount = datasamples / loopLen;
    if( loopCount * loopLen != datasz )
    {
        // Do somethign about this error state where loops are not integer
    }

    std::cout << "  samples=" << datasamples << " loopLen=" << loopLen << " loopCount=" << loopCount << std::endl;
    
    // wt format header (surge internal)
    wt_header wh;
    memset(&wh, 0, sizeof(wt_header));
    wh.flags = wtf_is_sample;

    int sh = 0;
    if( loopData )
    {
        wh.flags = 0;
        switch( loopLen )
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
            wh.flags = wtf_is_sample;
            break;
        }
    }

    if( sh == 0 )
    {
        Surge::UserInteractions::promptError( "Sorry we only can read power of 2 wavetables right now but sample support coming soon!q",
                                              "Error loading WAV" );
    }
    
    wh.n_samples = 1 << sh;
    int mask = wt->size - 1;
    
    int sample_length = std::min(datasamples, max_wtable_size * max_subtables);
    
    wh.n_tables = std::min(max_subtables, (sample_length >> sh));
    
    int channels = 1;

    
    if ((audioFormat == 1 /* WAVE_FORMAT_PCM */) &&
        (bitsPerSample == 16) &&
        numChannels == 1)
    {
        assert(wh.n_samples * wh.n_tables * 2 <= datasz);
        wh.flags |= wtf_int16;
    }
    else if ((audioFormat == 3 /* WAVE_FORMAT_IEEE_FLOAT */) &&
             (bitsPerSample == 32) &&
             numChannels == 1
        )
    {
        assert(wh.n_samples * wh.n_tables * 4 <= datasz);
    }
    else
    {
        Surge::UserInteractions::promptError( "Sorry, we only support 16 bit mono PCM and 32 bit IEEE float",
                                              "Wav File Error" );
        if( wavdata ) free( wavdata );
        return;
    }

    if( wavdata && wt )
    {
        CS_WaveTableData.enter();
        
        wt->BuildWT(wavdata, wh, wh.flags & wtf_is_sample);
        
        CS_WaveTableData.leave();
        free( wavdata );
    }
    return;
}
