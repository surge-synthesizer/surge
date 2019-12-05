/*
** Portable (using standard fread and so on) support for .wav files generating
** wavetables. 
**
** Two things which matter in addition to the fmt and data block
**
**  1: The `smpl` block
**  2: the `clm ` block - indicates a serum file
**  3: the 'cue' block which apparently NI uses
**
** I read them in this order
**
**  1. If there is a clm block that wins, we ignore the smpl and cue block, and you get your 2048 wt
**  2. If there is a cue block and no clm, use the offsets if they are power of 2 and regular
**  3. Else if there is no smpl block and you have a smpl block if you have a power of 2 sample length
**     we interpret you as a wt 
**  4. otherwise as a one shot or - right now-  an error
*/

#define WAV_STDOUT_INFO 0

#include <stdio.h>
#include "UserInteractions.h"
#include "SurgeStorage.h"
#include <sstream>

#if LINUX
#include <experimental/filesystem>
#elif MAC || TARGET_RACK
#include <filesystem.h>
#else
#include <filesystem>
#endif

namespace fs = std::experimental::filesystem;

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

struct FcloseGuard {
    FILE *fp = nullptr;
    FcloseGuard( FILE *f ) { fp = f; }
    ~FcloseGuard() {
        if( fp )
        {
            fclose(fp);
        }
    }
                
};
void SurgeStorage::load_wt_wav_portable(std::string fn, Wavetable *wt)
{
    std::string uitag = "Wav File Load Error";
#if WAV_STDOUT_INFO
    std::cout << "Loading wt_wav_portable" << std::endl;
    std::cout << "  fn='" << fn << "'" << std::endl;
#endif

    FILE *fp = fopen(fn.c_str(), "rb");
    if( ! fp )
    {
        std::ostringstream oss;
        oss << "Unable to open file '" << fn << "'";
        Surge::UserInteractions::promptError(oss.str(), uitag );
        return;
    }
    FcloseGuard closeOnReturn(fp);
    
    char riff[4], szd[4], wav[4];
    auto hds = fread(riff, 1, 4, fp);
    hds += fread(szd, 1, 4, fp);
    hds += fread(wav, 1, 4, fp);
    if (hds != 12)
    {
       std::ostringstream oss;
       oss << "File does not contain a valid RIFF header chunk. file='" << fn << "'";
       Surge::UserInteractions::promptError(oss.str(),
                                            uitag );
       return;
    }

    if( ! four_chars(riff, 'R', 'I', 'F', 'F' ) &&
        ! four_chars(wav,  'W', 'A', 'V', 'E' ) )
    {
       std::ostringstream oss;
       oss << "File is not a standard RIFF/WAVE file. Header is: [" << riff[0] << riff[1] << riff[2]
           << riff[3] << " " << wav[0] << wav[1] << wav[2] << wav[3] << ". file='" << fn << "'";
       Surge::UserInteractions::promptError(oss.str(), uitag );
       return;
    }
    
    // WAV HEADER
    unsigned short audioFormat, numChannels;
    unsigned int sampleRate, byteRate;
    unsigned short blockAlign, bitsPerSample;

    // Result of data read
    bool hasSMPL = false;
    bool hasCLM = false;;
    bool hasCUE = false;
    bool hasSRGE = false;
    bool hasSRGO = false;
    int clmLEN = 0;
    int smplLEN = 0;
    int cueLEN = 0;
    int srgeLEN = 0;
    
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
        int cs = pl_int(chunkSzD);

#if WAV_STDOUT_INFO
        std::cout << "  CHUNK  `";
        for (int i = 0; i < 4; ++i)
           std::cout << chunkType[i];
        std::cout << "`  sz=" << cs << std::endl;
#endif
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

#if WAV_STDOUT_INFO
            std::cout << "     FMT=" << audioFormat << " x " << numChannels << " at " << bitsPerSample << " bits" << std::endl;
#endif

            free(data);

            // Do a format check here to bail out
            if (! ( numChannels == 1 &&
                    ( (audioFormat == 1 /* WAVE_FORMAT_PCM */) && (bitsPerSample == 16) ) ||
                    ( (audioFormat == 3 /* IEEE_FLOAT */ ) && (bitsPerSample == 32) ) ) )
            {
                std::string fname = "Neither";
                if( audioFormat == 1 ) fname = "PCM";
                if( audioFormat == 3 ) fname = "IEEE";
                std::ostringstream oss;
                oss << "Sorry, Surge only supports 16-bit PCM or 32-bit IEEE float mono (single channel) wav files. "
                    << " You provided a wav with format=" << audioFormat << " (" << fname << ") "
                    << " bitsPerSample=" << bitsPerSample
                    << " and channels=" << numChannels << (numChannels == 2 ? " (stereo)" : "" )
                    << ". file='" << fn << "'";
                
                Surge::UserInteractions::promptError( oss.str(), uitag );
                return;
            }
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
        else if( four_chars(chunkType, 'u', 'h', 'W', 'T'))
        {
            // This is HIVE metadata so treat it just like CLM / Serum
            hasCLM = true;
            clmLEN = 2048;

            free(data);
        }
        else if( four_chars(chunkType, 's', 'r', 'g', 'e'))
        {
            hasSRGE = true;
            char *dp = data;
            int version = pl_int(dp); dp += 4;
            srgeLEN = pl_int(dp);
            free(data);
        }
        else if( four_chars(chunkType, 's', 'r', 'g', 'o'))
        {
            hasSRGO = true;
            char *dp = data;
            int version = pl_int(dp); dp += 4;
            srgeLEN = pl_int(dp);
            free(data);
        }
        else if( four_chars(chunkType, 'c', 'u', 'e', ' ' ))
        {
            char *dp = data;
            int numCues = pl_int(dp); dp += 4;
            std::vector<int> chunkStarts;
            for( int i=0; i<numCues; ++i )
            {
                for( int j=0; j<6; ++j )
                {
                    auto d = pl_int(dp);
                    if( j == 5 )
                        chunkStarts.push_back(d);

                    dp += 4;
                }
            }

            // Now are my chunkstarts regular
            int d = -1;
            bool regular = true;
            for( auto i=1; i<chunkStarts.size(); ++i )
            {
                if( d == -1 )
                    d = chunkStarts[i] - chunkStarts[i-1];
                else
                {
                    if( d != chunkStarts[i] - chunkStarts[i-1] )
                        regular = false;
                }
            }

            if( regular )
            {
                hasCUE = true;
                cueLEN = d;
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
#if WAV_STDOUT_INFO
            std::cout << "   SMPL: nloops=" << nloops << " " << sdsz << std::endl;
#endif

            if( nloops == 0 )
            {
                // It seems RAPID uses a smpl block with no samples to indicate a 2048.
                hasSMPL = true;
                smplLEN = 2048;
            }
            
            if( nloops > 1 )
            {
                // We don't support this. Indicate somehow.
                // FIXME
            }
            
            for( int i=0; i<nloops && i < 1; ++i )
            {
                unsigned int loopdata[6];
                for( int j=0; j<6; ++j )
                {
                    loopdata[j] = pl_int(dp); dp += 4;
#if WAV_STDOUT_INFO
                    std::cout << "      loopdata[" << j << "] = " << loopdata[j] << std::endl;
#endif
                }
                hasSMPL = true;
                smplLEN = loopdata[3] - loopdata[2] + 1;
            }
        }
        else
        {
            /*std::cout << "Default Dump\n";
            for( int i=0; i<cs; ++i ) std::cout << data[i];
            std::cout << std::endl; */
            free(data);
        }
    }

#if WAV_STDOUT_INFO
    std::cout << "  hasCLM =" << hasCLM << " / " << clmLEN << std::endl;
    std::cout << "  hasCUE =" << hasCUE << " / " << cueLEN << std::endl;
    std::cout << "  hasSMPL=" << hasSMPL << "/" << smplLEN << std::endl;
    std::cout << "  hasSRGE=" << hasSRGE << "/" << srgeLEN << std::endl;
#endif

    bool loopData = hasSMPL || hasCLM || hasSRGE;
    int loopLen = hasCLM ? clmLEN :
        ( hasCUE ? cueLEN :
          (hasSRGE ? srgeLEN :
           (hasSMPL ? smplLEN : -1 ) ) );
    int loopCount = datasamples / loopLen;

#if WAV_STDOUT_INFO
    std::cout << "  samples=" << datasamples << " loopLen=" << loopLen << " loopCount=" << loopCount << std::endl;
#endif

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
#if WAV_STDOUT_INFO
           std::cout << "Setting style to sample" << std::endl;
#endif
           wh.flags = wtf_is_sample;
           break;
        }
    }

    if( loopLen != -1 && ( sh == 0 || loopCount < 3 ) )
    {
        std::ostringstream oss;
        oss << "Sorry, Surge only supports power-of-2-samplecount wavetables up to 4096 with at least 3 loops at this time. "
            << " You provided a wavetable with " << loopCount << " loops of " << loopLen << " samples. file='" << fn << "'";
        Surge::UserInteractions::promptError( oss.str(), uitag );
                                              
        if (wavdata) free(wavdata);
        return;
    }

    wh.n_samples = 1 << sh;
    int mask = wt->size - 1;
    int sample_length = std::min(datasamples, max_wtable_size * max_subtables);
    wh.n_tables = std::min(max_subtables, (sample_length >> sh));

    if( wh.flags & wtf_is_sample )
    {
        auto windowSize = 1024;
        if( hasSRGO )
           windowSize = srgeLEN;
        
        while( windowSize * 4 > sample_length && windowSize > 8 )
            windowSize = windowSize / 2;
        wh.n_samples = windowSize;
        wh.n_tables = (int)( sample_length / windowSize );
    }
    
    int channels = 1;

    
    if ((audioFormat == 1 /* WAVE_FORMAT_PCM */) &&
        (bitsPerSample == 16) &&
        numChannels == 1)
    {
        // assert(wh.n_samples * wh.n_tables * 2 <= datasz);
        wh.flags |= wtf_int16;
    }
    else if ((audioFormat == 3 /* WAVE_FORMAT_IEEE_FLOAT */) &&
             (bitsPerSample == 32) &&
             numChannels == 1
        )
    {
        // assert(wh.n_samples * wh.n_tables * 4 <= datasz);
    }
    else
    {
        std::ostringstream oss;
        oss << "Sorry, Surge only supports 16-bit PCM or 32-bit IEEE float mono wav files. "
            << " You provided a wav with format=" << audioFormat
            << " bitsPerSample=" << bitsPerSample
            << " and numChannels=" << numChannels
            << ". file='" << fn << "'";

        Surge::UserInteractions::promptError( oss.str(), uitag );
                                              
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

void SurgeStorage::export_wt_wav_portable(std::string fbase, Wavetable *wt)
{
   std::ostringstream oss;
   oss << userDataPath << "/ExportedWaveTables";
   fs::create_directories( oss.str() );
   std::string dirName = oss.str();

   auto fnamePre = fbase + ".wav";
   auto fname = dirName + "/" + fbase + ".wav";
   int fnum = 1;
   while( fs::exists( fs::path( fname ) ) ) {
      fname = dirName + "/" + fbase + "_" + std::to_string(fnum) + ".wav";
      fnamePre = fbase + "_" + std::to_string(fnum) + ".wav";
      fnum++;
   }
   
   struct FG {
      FILE *f = nullptr;
      ~FG() {
         if( f ) fclose( f );
      }
   };
   std::string errorMessage = "Unknown Error";
   FG fguard;
   
   {
      FILE *wfp = fopen( fname.c_str(), "wb" );
      if( ! wfp )
      {
         errorMessage = "Unable to open file '" + fname + "'";
         goto error;
      }
      fguard.f = wfp;
      
      auto audioFormat = 3;
      auto bitsPerSample = 32;
      auto sampleRate = 44100;
      auto nChannels = 1;

      auto w4i = [wfp](unsigned int v) {
                    unsigned char fi[4];
                    for( int i=0; i<4; ++i )
                    {
                       fi[i] = (unsigned char)( v & 255 );
                       v = v / 256;
                    }
                    fwrite( fi, 1, 4, wfp );
                 };

         
      auto w2i = [wfp](unsigned int v) {
                    unsigned char fi[2];
                    for( int i=0; i<2; ++i )
                    {
                       fi[i] = (unsigned char)( v & 255 );
                       v = v / 256;
                    }
                    fwrite( fi, 1, 2, wfp );
                 };

      fwrite( "RIFF", 1, 4, wfp );

      bool isSample = false;
      if( wt->flags & wtf_is_sample )
         isSample = true;

      // OK so how much data do I have.
      unsigned int tableSize = nChannels * bitsPerSample / 8 * wt->n_tables * wt->size;
      unsigned int dataSize =  4 + // 'WAVE'
         4 + 4 + 18 + // fmt chunk
         4 + 4 + tableSize;

      if( ! isSample )
         dataSize += 4 + 4 + 8; // srge chunk


      w4i(dataSize);
      fwrite( "WAVE", 1, 4, wfp );

      // OK so format chunk
      fwrite( "fmt ", 1, 4, wfp );
      w4i( 16 );
      w2i( audioFormat );
      w2i( nChannels );
      w4i( sampleRate );
      w4i( sampleRate * nChannels * bitsPerSample );
      w2i( bitsPerSample * nChannels );
      w2i( bitsPerSample );

      if( ! isSample )
      {
         fwrite( "srge", 1, 4, wfp );
         w4i( 8 );
         w4i( 1 );
         w4i( wt->size );
      }
      else
      {
         fwrite( "srgo", 1, 4, wfp ); // o for oneshot
         w4i( 8 );
         w4i( 1 );
         w4i( wt->size );

      }
      
      fwrite( "data", 1, 4, wfp );
      w4i( tableSize );
      for( int i=0; i<wt->n_tables; ++i )
      {
         fwrite( wt->TableF32WeakPointers[0][i], wt->size, bitsPerSample / 8, wfp );
      }

      fclose(wfp);
      fguard.f = nullptr;
      refresh_wtlist();

      Surge::UserInteractions::promptInfo( "Exported to your Surge Documents in `ExportedWaveTables/" + fnamePre + "'",
                                           "Export Succeeded" );
      
      return;
   }
   
error:
   Surge::UserInteractions::promptError( errorMessage, "Export" );
   return;
   
   
}
