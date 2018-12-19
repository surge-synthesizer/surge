./build-osx.sh

rsync -r --delete "products/Surge.component/" ~/Library/Audio/Plug-Ins/Components/Surge.component/
rsync -r --delete "products/Surge.vst/" ~/Library/Audio/Plug-Ins/VST/Surge.vst/
rsync -r --delete "products/Surge.vst3/" ~/Library/Audio/Plug-Ins/VST3/Surge.vst3/
