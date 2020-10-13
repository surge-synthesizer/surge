
#include "SurgeStorage.h"
#include "UserDefaults.h"

#include "audioeffect_airwinstub.h"

int AirWinBaseClass::airwindowsSurgeDisplayPrecision() {
   int detailedMode = false;
   
   if (storage)
      detailedMode = Surge::Storage::getUserDefaultValue(storage, "highPrecisionReadouts", 0);
   
   int dp = (detailedMode ? 6 : 2);
   return dp;
}

double AirWinBaseClass::getSampleRate() {
   return dsamplerate;
}
