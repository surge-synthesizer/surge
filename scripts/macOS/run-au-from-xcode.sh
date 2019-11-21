#!/bin/sh

rsync -r --delete "resources/data/" "$HOME/Library/Application Support/SurgePlusPlus/"
rsync -r --delete "products/Surge++.component/" "$HOME/Library/Audio/Plug-Ins/Components/Surge++.component/"

"/Applications/Hosting AU.app/Contents/MacOS/Hosting AU" "scripts/macOS/SurgePlusPlus.hosting"

