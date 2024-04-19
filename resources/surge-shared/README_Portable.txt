The Surge XT Windows and Linux Portable Distribution

Windows
-------

The Surge XT Portable Zip bundles the assets, vst3, clap, and exe into a single directory which is set up
so all assets are self contained. If you re-organize the directory structure, your system may not work
without manual creation of hard links. Please consult the surge manual.

The VST3 spec does not allow installs of VST3 outside prescribed locations. Some DAWs allow this nonetheless,
but FL does not. If you are a FL user, do not use this, but rather use our standard installer.

Linux
-----
In combination with the `-pluginsonly.tar.gz` this binary contains a directory called "SurgeXTData" which must
be somewhere in the parent path of your executable, vst3, etc... That is, if your vst3 is in /foo/baz/frank/Surge XT.vst3 then
this durectory must be /SurgeXTData, /foo/SurgeXTData, /foo/baz/SurgeXTData /foo/baz/frank/SurgeXTData

You can also override the directory location on linux with the `SURGE_DATA_HOME` environment as you see fit.