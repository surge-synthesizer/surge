# Version Numbers

This document is a reminder list of all the places you need to update when you have a version 
number tick.

So version numbers live in

* src/common/surge_auversion.h
* resource/osx-*/Info.plist
* src/windows/surge.rc (in several places in for 1,6,0,0)
* src/common/version.h
* src/common/version.h also contains a copyright year as does the AU info.plist
* src/lv2/SurgeLv2Export.cpp generates minor and micro version
