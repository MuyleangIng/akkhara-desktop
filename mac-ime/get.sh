#!/bin/bash
# One-line install / update / uninstall of the Akkhara keyboard for Mac (no admin password, no restart):
#   curl -fsSL https://raw.githubusercontent.com/MuyleangIng/akkhara-desktop/main/mac-ime/get.sh | bash
#   curl -fsSL https://raw.githubusercontent.com/MuyleangIng/akkhara-desktop/main/mac-ime/get.sh | bash -s uninstall
# It downloads the newest Akkhara-Keyboard-Mac-*.zip from GitHub Releases and runs its install.sh.
# (Files downloaded by curl are not blocked by macOS, unlike a zip opened from the browser.)
set -euo pipefail
REPO=MuyleangIng/akkhara-desktop
APP="$HOME/Library/Input Methods/Akkhara.app"

if [ "${1:-}" = uninstall ]; then
  [ -x "$APP/Contents/MacOS/Akkhara" ] && "$APP/Contents/MacOS/Akkhara" --unregister 2>/dev/null || true
  pkill -x Akkhara 2>/dev/null || true
  rm -rf "$APP"
  echo "Akkhara removed. (No restart needed.)"
  exit 0
fi

[ "$(uname -m)" = arm64 ] || { echo "Sorry: the Akkhara Mac keyboard needs an Apple M-series Mac for now."; exit 1; }
ver=$(sw_vers -productVersion); major=${ver%%.*}; rest=${ver#*.}; minor=${rest%%.*}
if [ "$major" -lt 13 ] || { [ "$major" -eq 13 ] && [ "${minor:-0}" -lt 3 ]; }; then
  echo "Sorry: Akkhara needs macOS 13.3 or newer (you have $ver)."; exit 1
fi

echo "Looking for the newest Akkhara release…"
url=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" \
      | grep -o '"browser_download_url": *"[^"]*Akkhara-Keyboard-Mac-[^"]*\.zip"' | head -1 | sed 's/.*"\(https[^"]*\)"/\1/')
[ -n "$url" ] || { echo "No Mac keyboard found in the latest release: https://github.com/$REPO/releases"; exit 1; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
echo "Downloading ${url##*/}"
curl -fL --progress-bar -o "$tmp/akkhara.zip" "$url"
ditto -x -k "$tmp/akkhara.zip" "$tmp"
bash "$tmp/Akkhara Keyboard/install.sh"
