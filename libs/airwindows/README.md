This code is an adapted version of the Airwindows plugins https://github.com/airwindows/airwindows/

Airwindows is released under the MIT license as described in the LICENSE file in this directory.

To add another plugin

1. Clone airwindows at the same level as surge (so you have ~/dev/surge and ~/dev/airwindows or whatever); or clone it somewhere else and remember the path
2. In this directory do `perl grab.pl FXName (airwindowspath)` 
3. Edit CMakeLists.txt here to add FXName.cpp and FXNameProc.cpp appropriately
4. Edit surce/src/common/dsp/effects/airwindows_adapter/airwindows_register.cpp and add a registerAirWindow for Fx::Fx and an #include "FXName.h" at the top

and then cmake again and it should all work.
