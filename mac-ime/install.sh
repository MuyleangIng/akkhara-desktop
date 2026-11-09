#!/bin/bash
# Install (or update) the Akkhara keyboard for the current user — no admin password, no restart.
#   bash install.sh     (next to Akkhara.app in the download, or after build.sh: uses build/Akkhara.app)
set -euo pipefail
cd "$(dirname "$0")"
SRC=Akkhara.app; [ -d "$SRC" ] || SRC=build/Akkhara.app
DEST="$HOME/Library/Input Methods"
mkdir -p "$DEST"
old=$(defaults read "$DEST/Akkhara.app/Contents/Info" CFBundleShortVersionString 2>/dev/null || true)
pkill -x Akkhara 2>/dev/null || true        # the old version: macOS starts the new one when you type
rm -rf "$DEST/Akkhara.app"
cp -R "$SRC" "$DEST/"
xattr -dr com.apple.quarantine "$DEST/Akkhara.app" 2>/dev/null || true   # zip opened from a browser: allow it
"$DEST/Akkhara.app/Contents/MacOS/Akkhara" --register 2>&1 | grep -v -e "Keyboard Layouts" -e TISFileInterrogator || true
new=$(defaults read "$DEST/Akkhara.app/Contents/Info" CFBundleShortVersionString)
echo
if [ -n "$old" ]; then echo "Akkhara updated: $old → $new"; else echo "Akkhara $new installed."; fi
echo "Switch to it: menu bar input menu (A / ក) → Akkhara, or press Ctrl+Space."
echo "Not in the menu? System Settings → Keyboard → Input Sources → Edit… → + → Khmer → Akkhara → Add."
