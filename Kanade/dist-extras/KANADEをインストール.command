#!/bin/bash
# KANADE installer: copies the plugins into your user plugin folders.
cd "$(dirname "$0")" || exit 1

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
mkdir -p "$VST3_DIR" "$AU_DIR"

rm -rf "$VST3_DIR/KANADE.vst3" "$AU_DIR/KANADE.component"
cp -R KANADE.vst3 "$VST3_DIR/"
cp -R KANADE.component "$AU_DIR/"

# Downloaded files are blocked by macOS until this flag is removed
xattr -dr com.apple.quarantine "$VST3_DIR/KANADE.vst3" "$AU_DIR/KANADE.component" 2>/dev/null
[ -d KANADE.app ] && xattr -dr com.apple.quarantine KANADE.app 2>/dev/null

# Make macOS notice the new Audio Unit
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "KANADE をインストールしました。"
echo "  VST3: $VST3_DIR/KANADE.vst3"
echo "  AU:   $AU_DIR/KANADE.component"
echo ""
echo "FL Studio を開いて、プラグインを再スキャンしてください。"
echo "(このウィンドウは閉じて大丈夫です)"
