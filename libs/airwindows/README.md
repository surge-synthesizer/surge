This code is an adapted version of the Airwindows plugins https://github.com/airwindows/airwindows/

Airwindows is released under the MIT license as described in the LICENSE file in this directory.

To add another plugin:

1. Clone Airwindows at the same level as Surge XT (so you have `~/dev/surge-xt` and `~/dev/airwindows`, or whatever), or clone it somewhere else and remember the path
2. From this directory, run `perl grab.pl FXName (airwindowspath)`. If `airwindowspath` is not provided, Airwindows repository will be expected at the same folder level as Surge XT. Alternatively, one can also run the Python script in exactly the same way: `python grab.py FXName (airwindowspath)`.
3. Edit CMakeLists.txt here to add FXName.cpp and FXNameProc.cpp appropriately.
4. Edit libs/airwindows/AirWinBaseClass_pluginRegistry.cpp:
   add a `reg.emplace_back(...)` for FXName::FXName **at the end** of `pluginRegistry()` method, and an #include "FXName.h" at the top.
5. Modify the signature of `getParameterDisplay` to take additional `float extVal` and `bool isExternal` arguments, with the implementation using the `EXTV` macro instead of raw parameter value (in FXName.h and FXName.cpp). Python script will at least modify the signature with additional arguments.
6. Compile and fix the *in++ idiom with the unused value.
7. Implement `parseParameterValueFromString()` and `isBipolar()` for the new FX, if needed.
8. Add the new FX init presets to the presets in `resources/data/fx_presets`.
9. Compile again and everything should work!
