#!/bin/bash
# Remove the Akkhara keyboard — no restart.
set -uo pipefail
APP="$HOME/Library/Input Methods/Akkhara.app"
[ -x "$APP/Contents/MacOS/Akkhara" ] && "$APP/Contents/MacOS/Akkhara" --unregister
pkill -x Akkhara 2>/dev/null
rm -rf "$APP"
echo "Akkhara removed."
