// -*-c++-*-
/*
** We copy the AirWindows VST2s but we don't need the VST2 API; we just need the process and
** param methods. So this is a minimal header that lets us compile
*/
#define __audioeffect__

#include "SurgeStorage.h"
#include <cstdint>
#include <cstring>

typedef int32_t VstInt32;
typedef int audioMasterCallback;


struct AirWinBaseClass {
   
   AirWinBaseClass( audioMasterCallback amc, int knpr, int knpa ) : paramCount( knpa ) { }
   virtual ~AirWinBaseClass() = default;

   virtual void setNumInputs( int i ) { }
   virtual void setNumOutputs( int i ) { }
   virtual void setUniqueID( unsigned long l ) { }

   virtual void programsAreChunks( bool b ) { }
   
   virtual bool canProcessReplacing() { return true; }
   virtual bool canDoubleReplacing() { return false; }

   virtual bool getEffectName( char *name ) = 0;
   
   virtual float getParameter( VstInt32 index ) = 0;
   virtual void setParameter( VstInt32 index, float value ) = 0;
   virtual void processReplacing( float **in, float **out, VstInt32 sampleFrames ) = 0;

   virtual void getParameterName(VstInt32 index, char *text) = 0;    // name of the parameter
   virtual void getParameterLabel(VstInt32 index, char *txt) = 0;
   virtual void getParameterDisplay(VstInt32 index, char *txt) = 0;
   
   double getSampleRate() { return dsamplerate; }

   int paramCount = 0;
   
   static constexpr int kVstMaxProgNameLen = 64;
   static constexpr int kVstMaxParamStrLen = 64;
   static constexpr int kVstMaxProductStrLen = 64;
   static constexpr int kVstMaxVendorStrLen = 64;

   static constexpr int kPlugCategEffect = 0;

};


inline char *vst_strncpy ( char * destination, const char * source, size_t num ) { return strncpy( destination, source, num ); }
inline void  float2string( float f, char* t, size_t num )
{
   snprintf( t, num, "%8.3f", f );
}
inline void int2string( int i, char *t, size_t num )
{
   snprintf( t, num, "%d", i );
}
inline void dB2string( float value, char *t, size_t num )
{
   if (value <= 0)
      vst_strncpy (t, "-inf", num);
   else
      float2string ((float)(20. * log10 (value)), t, num);
}

typedef AirWinBaseClass AudioEffectX;
typedef long VstPlugCategory;
