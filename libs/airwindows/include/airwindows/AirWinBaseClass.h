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

#define EXTV(a) (isExternal ? extVal : a)

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
   virtual void getIntegralDisplayForValue( VstInt32 index, float value, char *txt )
   {
      sprintf( txt, "%d", (int)( value * ( parameterIntegralUpperBound(index) + 0.99 )) );
   }

   virtual void processReplacing( float **in, float **out, VstInt32 sampleFrames ) = 0;

   virtual void getParameterName(VstInt32 index, char *text) = 0;    // name of the parameter
   virtual void getParameterLabel(VstInt32 index, char *txt) = 0;
   virtual void getParameterDisplay(VstInt32 index, char *txt, float extVal = 0, bool isExternal = false) = 0;

   double sr = 0;
   double getSampleRate() { return sr; }

   int paramCount = 0;

   static constexpr int kVstMaxProgNameLen = 64;
   static constexpr int kVstMaxParamStrLen = 64;
   static constexpr int kVstMaxProductStrLen = 64;
   static constexpr int kVstMaxVendorStrLen = 64;

   static constexpr int kPlugCategEffect = 0;

   SurgeStorage *storage = nullptr;

   int displayPrecision = 2;
   int airwindowsSurgeDisplayPrecision() { return displayPrecision; }

   inline char *vst_strncpy ( char * destination, const char * source, size_t num ) { return strncpy( destination, source, num ); }
   inline void  float2string( float f, char* t, size_t num ) {
      auto dp = airwindowsSurgeDisplayPrecision();
      snprintf( t, num, "%.*f", dp, f );
   }
   inline void int2string( int i, char *t, size_t num ) {
      snprintf( t, num, "%d", i );
   }
   inline void dB2string( float value, char *t, size_t num ) {
      if (value <= 0.00001) // -100 dB, show -inf from that point onwards
         vst_strncpy (t, "-inf", num);
      else
         float2string ((float)(20.0 * log10 (value)), t, num);
   }
   inline float string2dB(const char *t, float value)
   {
      if (strcmp(t, "-inf") == 0)
      {
         return 0.0;
      }
      else
      {
         return pow(10.0, value / 20.0);
      }
   }
   struct Registration {
      std::unique_ptr<AirWinBaseClass> (* const create)(int id, double sr, int displayPrecision );
      const int id, displayOrder;
      const std::string groupName, name;
      Registration(std::unique_ptr<AirWinBaseClass> (*create)(int, double, int),
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
   static std::vector<int> pluginRegistryOrdering();

   bool denormBeforeProcess{false};
};

/*
 * We use this as a placeholder if we decide to retire an AW
 */
struct AirWindowsNoOp : AirWinBaseClass {
   AirWindowsNoOp( audioMasterCallback amc ) : AirWinBaseClass( amc, 0, 0 ) { }

   virtual bool getEffectName( char *name ) {
       strncpy(name, "NoOp", 5 );
       return true;
   };

   virtual float getParameter( VstInt32 index ) { return 0; };
   virtual void setParameter( VstInt32 index, float value ) {};
   virtual void processReplacing( float **in, float **out, VstInt32 sampleFrames ) {
       memset(out[0],0,sampleFrames * sizeof(float));
       memset(out[1],0,sampleFrames * sizeof(float));
   };

   virtual void getParameterName(VstInt32 index, char *text) {};    // name of the parameter
   virtual void getParameterLabel(VstInt32 index, char *txt) {};
   virtual void getParameterDisplay(VstInt32 index, char *txt, float extVal = 0, bool isExternal = false) {};
};