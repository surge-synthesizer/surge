# Adding an SVG to the Resources Bundle

The easiest way to do this is fire up `bash` on a machine with Perl and run the script.

```
perl scripts/misc/perl/addSVG.pl (filename) (idname) (idnum)
```

For example:

```
perl ./scripts/misc/addSVG.pl assets/SurgeClassic/exported/bmp00307.svg IDB_MSEG_CONTROLPOINT 307
```

We have some 'well named' SVGs which compile into the bundle. These are all in
assets/SurgeClassic/exported. What happens if you want to add one?

1. Add the ID to the IDB_ in resource.h. 
2. Add the addEntry in SurgeBitmaps.cpp. You are now done for Mac and Linux actually!
3. Add the IDB_BLAH_SVG_DATA in svgresources.rc
4. Add the IDB_MENU_IN_SLIDER_SCALE_BLAH defines to scalableresource.h
5. Add the forward name to the top of src/common/gui/SkinImageMaps.h
6. Add the ID to the set at the bottom of src/common/gui/SkinImageMaps.h