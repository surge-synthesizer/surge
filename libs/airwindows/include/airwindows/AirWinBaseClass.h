#pragma once

/*
** We copy the AirWindows VST2s but we don't need the VST2 API; we just need the process and
** param methods. So this is a minimal header that lets us compile
*/

#include <cstdio>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

typedef int32_t VstInt32;
typedef int audioMasterCallback;

class SurgeStorage;

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

   virtual bool parseParameterValueFromString( VstInt32 index, const char* str, float &f ) {
      return false;
   }
   virtual bool isParameterBipolar( VstInt32 index ) { return false; }
   virtual bool isParameterIntegral( VstInt32 index ) { return false; }
   virtual int parameterIntegralUpperBound( VstInt32 index ) { return -1; }
   
   virtual void processReplacing( float **in, float **out, VstInt32 sampleFrames ) = 0;

   virtual void getParameterName(VstInt32 index, char *text) = 0;    // name of the parameter
   virtual void getParameterLabel(VstInt32 index, char *txt) = 0;
   virtual void getParameterDisplay(VstInt32 index, char *txt) = 0;
   
   double getSampleRate();

   int paramCount = 0;
   
   static constexpr int kVstMaxProgNameLen = 64;
   static constexpr int kVstMaxParamStrLen = 64;
   static constexpr int kVstMaxProductStrLen = 64;
   static constexpr int kVstMaxVendorStrLen = 64;

   static constexpr int kPlugCategEffect = 0;

   SurgeStorage *storage = nullptr;
   
   int airwindowsSurgeDisplayPrecision();

   inline char *vst_strncpy ( char * destination, const char * source, size_t num ) { return strncpy( destination, source, num ); }
   inline void  float2string( float f, char* t, size_t num ) {
      auto dp = airwindowsSurgeDisplayPrecision();
      snprintf( t, num, "%.*f", dp, f );
   }
   inline void int2string( int i, char *t, size_t num ) {
      snprintf( t, num, "%d", i );
   }
   inline void dB2string( float value, char *t, size_t num ) {
      if (value <= 0)
         vst_strncpy (t, "-inf", num);
      else
         float2string ((float)(20. * log10 (value)), t, num);
   }
   struct Registration {
      std::unique_ptr<AirWinBaseClass> (* const create)(int);
      const int id, displayOrder;
      const std::string groupName, name;
      Registration(std::unique_ptr<AirWinBaseClass> (*create)(int),
                   int id,
                   int displayOrder,
                   std::string groupName,
                   std::string name)
      : create(create)
      , id(id), displayOrder(displayOrder)
      , groupName(std::move(groupName)), name(std::move(name))
      {}
   };
   static std::vector<Registration> pluginRegistry();
};
