# Building For Apple Silicon

Install XCode 12.2 beta or later, then `xcode-select` it:

```
sudo xcode-select -s /Applications/Xcode-beta.app/
```

Run CMake with both architectures, then `xcodebuild`:

```
cmake -GXcode -Bbuild_fat -D"CMAKE_OSX_ARCHITECTURES=arm64;x86_64"
xcodebuild -target surge-headless build -project build_fat/Surge.xcodeproj/ -arch x86_64 -arch arm64 ONLY_ACTIVE_ARCH=NO
```

And then:

```
paul:~/dev/music/surge$ lipo -archs build_fat/Debug/surge-headless
x86_64 arm64
```

Similarly, you can build the AU or VST3 and get a fat binary in the distro.
