#!/bin/bash
# Install (or update) the Akkhara keyboard for the current user — no admin password, no restart.
#   bash install.sh            (next to Akkhara.app, or after build.sh: uses build/Akkhara.app)
set -euo pipefail
cd "$(dirname "$0")"
SRC=Akkhara.app; [ -d "$SRC" ] || SRC=build/Akkhara.app
DEST="$HOME/Library/Input Methods"
mkdir -p "$DEST"
pkill -x Akkhara 2>/dev/null || true        # an old version may be running: macOS restarts it when needed
rm -rf "$DEST/Akkhara.app"
cp -R "$SRC" "$DEST/"
xattr -dr com.apple.quarantine "$DEST/Akkhara.app" 2>/dev/null || true   # downloaded zip: allow it to run
"$DEST/Akkhara.app/Contents/MacOS/Akkhara" --register
echo
echo "Done. Switch to Akkhara: click the input menu in the menu bar (or press Ctrl+Space / fn) → Akkhara."
echo "If it is not in the list: System Settings → Keyboard → Input Sources → Edit… → + → Khmer → Akkhara."
