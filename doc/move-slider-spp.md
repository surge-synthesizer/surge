# How to move a slider from fixed to xml layout 

Old sliders are fixed layout. New sliders are in XML. How do you move them? This document tells you how

## Step one - build and run Surge++ and pick a slider

Pretty simple. Look for a slider which is *not* an FX slider (leave those to BP) but which is in the 'old part' of the UI. So pick
one. in this example I am going to use the "FM Depth" slider.

## Step two - modify SurgePatch.cpp

SurgePatch.cpp lists all the parameters and their names. In Surge 1.6.4 it also contains their screen coordinates.
This is the reason we can't easily change the UI. So what we want to do is for our slider change it from haivng a 
screen coordinate to having a logical name.

So open `src/common/SurgePatch.cpp` and find the FM Depth parameter.

```cpp
      a->push_back(scene[sc].fm_depth.assign(
          p_id.next(), id_s++, "fm_depth", "FM Depth", ct_decibel_fmdepth, gui_col3_x,
          gui_uppersec_y + gui_hfader_dist * 4, sc_id, cg_GLOBAL, 0, true,
          Surge::ParamConfig::kHorizontal | kWhite | sceasy));
```

you will see something that looks like that. OK so what you are seeing is the IDs (which should stay the same),
the parameter names and control types, the location, and some control stuff. We need to remove the
two position arguments (here gui_col3_x and gui_uppersec_y+) and remove the stuff at the end which says it is horizontal
and give it a name. The names I've been using are using dot syntax. So here I would use "fm.depth". That means change
the code to look like this:

```cpp
      a->push_back(scene[sc].fm_depth.assign(
                      p_id.next(), id_s++, "fm_depth", "FM Depth", "fm.depth",
                      ct_decibel_fmdepth, sc_id, cg_GLOBAL, 0, true ));
```

if you recompile and run you should see that (1) the FM Depth slider is gone from the old side and (2) that you see output
from your host which looks like this:

```
ERR :  Failed to construct component 'fm.depth' with failmsg 'No Component for guiid '
Control is not a ISliderKnobInterface
```

Great. That's the (bad) error reporting which says you need to layout the node. 

## Step 3: Put the newly named node in the layout

Open `resources/data/layouts/original.layout/layout.xml` and find the "FM" block.

```
            <layout height="auto" width="auto" style="roundedlabel" bgcolor="$mixenv.bg" fgcolor="$mixenv.fg" label="FM"/>
```

You need to change that to include the node you want with the control you want. In this case we want a light background
hslider. So 

```
            <layout height="auto" width="auto" style="roundedlabel" bgcolor="$mixenv.bg" fgcolor="$mixenv.fg" label="FM">
              <node guiid="fm.depth" xoff="center"   yoff="20" component="hslider.light"/>
            </layout>
```

and now if you redploy the layout (basically by doing another install/build cycle) you should see the FM depth slider in the new side. The components
are available at the top of the XML document.

and voila! A slider has moved.
