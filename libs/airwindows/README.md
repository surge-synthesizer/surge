This code is an adapted version of the Airwindows plugins https://github.com/airwindows/airwindows/

Airwindows is released under the MIT license as described in the LICENSE file in this directory.

To add another plugin

1. Clone airwindows at the same level as surge (so you have ~/dev/surge and ~/dev/airwindows or whatever); or clone it somewhere else and remember the path
2. In this directory do `perl grab.pl FXName (airwindowspath)` 
3. Edit CMakeLists.txt here to add FXName.cpp and FXNameProc.cpp appropriately
4. Edit libs/airwindows/AirWinBaseClass_pluginRegistry.cpp 
   and add a registerAirWindow for Fx::Fx **at the end** and an #include "FXName.h" at the top
5. Modify getParameterDisplay to take a float extVal and bool isExternal using the EXTV macro (in FXName.h and FXName.cpp)
6. Compile and fix the *in++ idion with the unused value   
7. As needed implement parseParameterValueFromString and isBipolar on the new FX
8. Add the new FX to the patches in resources/data/fx_presets
9. Compile again and everything should work
