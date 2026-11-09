#!/bin/bash
# build/Akkhara.app → Akkhara-Keyboard-Mac-<version>.zip (folder "Akkhara Keyboard": app + install/uninstall + read me)
set -euo pipefail
cd "$(dirname "$0")"
VERSION="${1:?version}"
P=build/pkg/"Akkhara Keyboard"
rm -rf build/pkg && mkdir -p "$P"
cp -R build/Akkhara.app "$P/"
cp install.sh uninstall.sh "$P/"
cat > "$P/READ ME.txt" <<TXT
Akkhara keyboard for Mac $VERSION — type Sing Khmer, get Khmer (Apple M-series, macOS 13.3+). By Ing Muyleang.

Install: open Terminal and run
  bash ~/Downloads/"Akkhara Keyboard"/install.sh
(or the one-line installer: curl -fsSL https://raw.githubusercontent.com/MuyleangIng/akkhara-desktop/main/mac-ime/get.sh | bash)

Then: menu bar input menu → Akkhara (or Ctrl+Space). Type "nh jg tv sala rean", press Enter → ខ្ញុំចង់ទៅសាលារៀន
Enter insert · ↑↓/Tab choose · click insert · Esc or Shift+Enter keep the Latin · Backspace delete
Remove: bash uninstall.sh   — no admin password, no restart.
More: https://github.com/MuyleangIng/akkhara-desktop#akkhara-keyboard-for-mac
TXT
(cd build/pkg && ditto -c -k --sequesterRsrc --keepParent "Akkhara Keyboard" "../../Akkhara-Keyboard-Mac-$VERSION.zip")
ls -lh "Akkhara-Keyboard-Mac-$VERSION.zip"
