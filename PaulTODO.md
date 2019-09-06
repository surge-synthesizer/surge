Here's my sort of running todo as I think of it, in rough order

* Pitch Bend Area
* Labels and Fonts
* Move the CHSwitch2s as is for now
  * FM routing
  * Filter config
* Move the LFO section over
* Modulator Sliders move on over
* Glyph Switch
  * Alignment of Text (MONO STFP?)
* Slider Parameterization and Work
  * Handle and BG SVGs
  * API for painting ticks on sliders; and sizing sliders
  * SVG for slider handles
* Modulation Button Properties for Colors exposed
  * including hover, background blink, font, etc...
* VU Meter and VU Meter Optimization for Background
* Find and swap layouts
  * LayoutDir to '.layout'
  * Scan for Layouts in the userpath and factorypath
  * Menu to swap layouts
* Finish the Scene and Patch section
  * What to do about the patch browser
* Finish OSC Section
  * Especially the rounded rects
* Sublayouts maybe?
* Finish positioning all the sliders with the XML
* Move the rest of the CHSwitch2s over
* FX routing and FX section
* Patch Browser
* What's left in legacy?
* Font Controls
* Second layout with stupid pet tricks
* Cleanup of bitmaps at close. Do they all get forgotten?
* About Screen 
  * Lets do proper git ID at make time also
* don't do kMeta or kEasy - separate those out
* Components on whom build never gets called
* VST2 doesn't delete instance else it crashes. I think it is because the LOE is deleted out of order. Check on Linux.

* Bugs I haven't debugged yet I know about
  * global octave < 0 clicks when switching in latch. Does it do it in surge proper too?
  
* Crashes
  * If you misconfigure octave (param type etc) you get all sorts of blowups
  * If you can't load an SVG and return a null you get a blowup
  * Why's it coredump if layout is missing?
  * Why's it coredump if layout is malformed

* Knobs glorious knobs
* Strings file
  * Layout strings to a saprate file
  * Engine Strings
* Multi-OSC view (thumbnails!)
* Add 3 more oscillators and have the parameters not break (use the parameter defered chaning)

* Context Help
* Where to put menus? How to specify menus in the layout?

* VST2 as well as 3 and AU
* Installer -> Surge++ nomenclature mac
* Installer Windows
* Installer Linux
* Pipeline in BaconPaul land

* Unleash the hounds


