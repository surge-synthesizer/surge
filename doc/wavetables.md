# Surge Wavetables (.wt)

Surge has a wavetable oscillator which plays a wavetable file. A wavetable is a collection 
of short (128-1024 or so) sample waves which have multiple waves in a single file. The 'shape'
parameter in the wavetable oscillator walks through the tables interpolating.

The `.wt` file is a simple header plus collection of raw binary data, and is documented
[here](https://github.com/surge-synthesizer/surge/blob/master/resources/data/wavetables/wt%20fileformat.txt) but
many users have asked us how to create these.

The program **AudioTerm** runs on Windows and creates **Surge** wavetable files, but is Windows only
and is an extensive program. The program [**WaveEdit**](http://synthtech.com/waveedit) is multiplatform and is able to export `.wav`-files of 256 sample length, and these have been demonstrated to be usable with the following knowledge to create **Surge** compatible `.wt` -files. You can also create your own 256, 512, 1024 sample length wavefiles, place them in a folder and use the script to create a brand new `.wt`. Just make sure that they are all 44.1khz, 16-bit, mono files at a specific length (power of 2).

In order to make manipulating wavetables easier, the **Surge** devs have put together some simple Python code to read, explode, and create `.wt` files. To use these you need a **Python3** install on your computer and to be comfortable with the command line. You also need the [**GitHub** repo](http://github.com/surge-synthesizer/surge) cloned or to at least grab a copy of the [script](https://github.com/surge-synthesizer/surge/tree/master/scripts)

Please note that this is pretty experimental. Error checking and so on needs to be improved over time.
We are throwing this out there to see if people use and improve it.

## Get information on a file

```
$ python3 ./scripts/wt-tool/wt-tool.py --action=info --file=<filename>
```

For instance

```
$ python3 ./scripts/wt-tool/wt-tool.py --action=info --file=resources/data/wavetables/sampled/cello.wt 
WT :'resources/data/wavetables/sampled/cello.wt'
  contains  45 samples
  of length 128
  in format int16
```

## Extract a .wt file into constituent .wav files

A `.wt` file contains a number of wave samples. You can extract those into numbered `.wav` files 

```
$ python3 ./scripts/wt-tool/wt-tool.py --action=explode --wav_dir=/tmp/cello --file=resources/data/wavetables/sampled/cello.wt 
Exploding 'resources/data/wavetables/sampled/cello.wt' into '/tmp/cello'
Creating /tmp/cello//wt_sample_000.wav
Creating /tmp/cello//wt_sample_001.wav
...
Creating /tmp/cello//wt_sample_043.wav
Creating /tmp/cello//wt_sample_044.wav
```

## Creating a wt file from a directory

Given a directory containing a set of `.wav` files with mono 16bit integer power of 2 samples, you can create a **Surge**
`.wt` file.  The `.wav` files are included in alphabetical order so using a naming scheme like the one
explode does is a good idea. The mechanism to make the input `.wav` files is an exercise left to the reader.

```
$ python3 ./scripts/wt-tool/wt-tool.py --action=create --file=newC.wt --wav_dir=/tmp/cello
Creating 'newC.wt' with 45 tables of length 128
```

## I wish this did x, y, or z

Hop on [Slack](https://join.slack.com/t/surgesynth/shared_invite/enQtNTE4OTg0MTU2NDY5LTE4MmNjOTBhMjU5ZjEwNGU5MjExODNhZGM0YjQxM2JiYTI5NDE5NGZkZjYxZTkzODdiNTM0ODc1ZmNhYzQ3NTU), [open an issue](http://github.com/surge-synthesizer/surge/issues/new), or crack open a python book and fire in a pull request! We're all ears on this stuff.
