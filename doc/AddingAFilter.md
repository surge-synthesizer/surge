# HOw to add a filter

1. Create src/common/dsp/filters/(name).h and (name).cpp. Look at RKMoog as an example. It should have two functions, a coeff and a process
   function. You are making free function here not an object. The RKMoog signatures are good starting points. Add the cpp to CMakeLists.txt
   You will need at least two functions, "makeCoefficients" and "process". The Prototype for process is fixed; the prototype for makeCoefficients
   is up to you.

2. In FilterConfiguration.h:
* add an enum to fu_type. ADD IT AT THE END.
* add a name to fut_names and fut_menu_names
* add an entry to fut_subcount for the number of subtypes your filter has
* add any additional strings or other constants your subtypes might use to this file too
* add a line to FilterSelectorMapper
* add an entry to fut_glyph_index

3. In FilterCoefficientMaker.h you get an opportunity to set up state variables. You have 8 coefficients C[8] you can set to - well - anything.
   This should redirect to the makeCoefficients function you put in your namespace.
   1. in FilterCoefficientMaker::MakeCoeffs call it in the switch
   2. This is called at most once every BLOCK_SIZE_OS and then only when F or R change
   
4. In QuadFilterUnit.cpp in GetQFPtrFilterUnit you need to return a function which does the filtering from the setup.
   1. It has to have the prototype `_m128 op( QuadFilterUnitState * __restrict, _m128 in )`
   2. The function to return is your namespaced process function. It will get a QuadFilterUnitState which
      has the coefficients you set in make Coeff (the "C") and also an array of registers R you can use.
   3. The most important thing about this code is it is called in parallel per voice. That is you may get
      it called with between 1 and 4 of the SSE channels populated. The f->active array tells you which voices
      are active. Each of the values (in, C, R) are also SSE-wide arrays. You can see the SSE implementations in QuadFilterUnit.cpp 
      and we should aspire to those. But for now you can also see a manual unroll in RKMoog.cpp
    
5. If you are adding subtypes you also want to add the stringification to Parameter.cpp again in an ifdef. RKMoog shows this too.

6. If your subtype needs to be accessible to your SSE function, check out the switch statements in `SurgeVoice::SetQFB`.

Then you can code up your filter. Again follow RKMoog as a worked example.
