#surgesynthteam_tuningui

This is a JUCE module which provides the various UI things we've set up for tuning. Currently it
            contains the table model which takes a tuning from the tuning -
        library and bridges it to a TableListBoxModel.We use this to make the tuning - workbench -
        synth table and
    (soon) the dexed show tuning table also.

    To use it,
    I generally add it as a submodule
            .So

``` mkdir lib cd lib git submodule add http
    : // github.com/surge-synthesizer/surgesynnthteam_tuningui
```

      the open the projucer and add a module and voila
            .

      I will write
      documentation here one day I promise.For now just go look how we use
      this in TWSTuningGrid.h in the tuning
        - workbench - synth
