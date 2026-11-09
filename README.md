# Akkhara for Windows and Mac

Type **Sing Khmer** in any app and get **Khmer**: `nh jg tv sala rean` → **ខ្ញុំចង់ទៅសាលារៀន**

It uses the same AI model as the Akkhara phone keyboard
([akkhara-mini](https://huggingface.co/INGMUYLEANG/akkhara-mini), ONNX int8, 18.5 MB) and runs
**offline on your computer**; nothing you type is sent anywhere. Made by Ing Muyleang.

## Which file do I download?

From the [latest release](https://github.com/MuyleangIng/akkhara-desktop/releases/latest):

| Your computer | Download | What you get |
|---|---|---|
| **Windows 10 / 11** ⭐ recommended | **`Akkhara-Keyboard-Setup-….exe`** | a **real keyboard** next to English: Win + Space → Akkhara, type in any app |
| **Mac** (Apple M-series, macOS 13.3+) ⭐ new | **one Terminal line** ([below](#akkhara-keyboard-for-mac)) or `Akkhara-Keyboard-Mac-….zip` | a **real keyboard** in Input Sources: Ctrl + Space → Akkhara, type in any app |
| Mac, test app | `Akkhara-Desktop-…-mac.zip` | the older popup app: a hotkey opens a small window, Enter pastes the Khmer |
| Windows, no install | `Akkhara-Desktop-…-windows.exe` | the same test app for Windows; use Setup above if you can |

## Akkhara Keyboard for Windows 10 / 11

### Install with Setup (recommended)

![Setup: ① welcome → ② choose the folder → ③ add to my keyboards → Install → ④ how to type](docs/img/win-0-setup.svg)

1. Download **`Akkhara-Keyboard-Setup-….exe`** and open it.
   Windows may say *"Windows protected your PC"* (the app is new and not signed): **More info → Run anyway**.
2. **Welcome** → **License** (Apache-2.0) → **Next**.
3. **Choose the folder.** Keep the default `C:\Program Files\Akkhara` (recommended) or click **Browse** for
   another one.
4. Keep **"Add Akkhara to my keyboards now"** ticked (and the Start menu shortcuts if you like) → **Install**.
5. Setup shows a picture of how to type → **Finish**. Go to *Switch and type* below.

If Setup could not add the keyboard by itself, it tells you and opens Settings: then follow
*Add it by hand* below (it takes 1 minute).

### Add it by hand (if you unticked the box, or it is missing)

| | |
|---|---|
| ![Settings → Time & language → Language & region](docs/img/win-1-settings.svg) | **Win + I** opens Settings → ① **Time & language** → ② **Language & region** |
| ![Khmer → ··· → Language options](docs/img/win-2-khmer-options.svg) | No Khmer in the list? **Add a language** → *Khmer*. Then ③ **···** next to Khmer → ④ **Language options** |
| ![Add a keyboard → Akkhara (Sing Khmer)](docs/img/win-3-add-keyboard.svg) | Under *Keyboards*: ⑤ **Add a keyboard** → ⑥ **Akkhara (Sing Khmer)** |

### Switch and type

![Win + Space → Akkhara, then type Sing Khmer](docs/img/win-4-switch-and-type.svg)

| Key | What it does |
|---|---|
| **Win + Space** (or the ENG / KHM button on the taskbar, or Alt + Shift) | switch between English and Akkhara |
| type Sing Khmer | the Latin stays underlined; the top-5 Khmer appear under it |
| **Enter** | insert the highlighted Khmer |
| **↑ / ↓**, **Tab**, or click | choose another suggestion |
| **Shift + Enter** or **Esc** | keep exactly the Latin you typed |
| **Backspace** | edit what you typed |
| Ctrl / Alt shortcuts | work as usual |

Apps that were already open while installing may need a restart to see Akkhara.

### Update, uninstall, install again (no restart)

![Install → use → update → uninstall, no restart](docs/img/win-lifecycle.svg)

- **Update:** Start menu → Akkhara → **Check for updates** (or the weekly check, if you ticked it in
  Setup). It compares your version with the [newest GitHub release](https://github.com/MuyleangIng/akkhara-desktop/releases/latest)
  and installs it over the old one. You can also just run a newer `Akkhara-Keyboard-Setup-….exe`.
  Only this check goes online; the keyboard itself never does.
- **Uninstall:** Settings → Apps → Installed apps → **Akkhara Keyboard** → Uninstall (or Start menu →
  Akkhara → *Uninstall Akkhara Keyboard*). It removes Akkhara from your keyboard list, unregisters it,
  removes the update check and deletes its folder.
- **No restart:** open apps keep the keyboard file loaded, so Windows cannot delete it right away.
  Setup moves such files to `Windows\Temp\Akkhara-removed` (Windows deletes them at your next normal
  restart), so neither an update nor an uninstall asks you to restart.
- **Install again** any time with the same Setup.

## Akkhara Keyboard for Mac

![Install with one line → choose Akkhara in the menu bar → type in any app](docs/img/mac-1-install-and-type.svg)

Apple M-series Mac, macOS 13.3 or newer. New in 0.4.0: please tell us if something does not work.

### Install (one line, recommended)

Open **Terminal** (⌘ Space → "Terminal") and paste:

```bash
curl -fsSL https://raw.githubusercontent.com/MuyleangIng/akkhara-desktop/main/mac-ime/get.sh | bash
```

It downloads the newest `Akkhara-Keyboard-Mac-….zip` from the releases and installs it into your own
`~/Library/Input Methods`: **no admin password, no restart**. (The app is not signed by Apple; files
downloaded this way are not blocked, a zip opened from the browser is.)

Or by hand: download `Akkhara-Keyboard-Mac-….zip`, unzip it and run
`bash ~/Downloads/"Akkhara Keyboard"/install.sh` in Terminal.

### Switch and type
1. Click the input menu in the menu bar (**A** or **ក**) → **Akkhara** (icon **អ**), or press **Ctrl + Space**.
   Not in the menu? System Settings → Keyboard → Input Sources → **Edit…** → **+** → **Khmer** → **Akkhara** → **Add**.
2. Type Sing Khmer in any app: `nh jg tv sala rean`. The Latin is underlined and the top-5 Khmer appear under it.

| Key | What it does |
|---|---|
| **Enter** | insert the highlighted Khmer |
| **↑ / ↓**, **Tab**, click | choose another suggestion (click inserts it) |
| **Esc** or **Shift + Enter** | keep the Latin as typed |
| **Backspace** | delete one letter |

Only a letter starts Sing Khmer; numbers, punctuation and ⌘ shortcuts work as usual.
Abbreviations: `nh tver ka nv mptc` → #1 ខ្ញុំធ្វើការនៅ mptc, #2 (↓) the full ministry name.

### Update, uninstall (no restart)
- **Update:** run the same one line again (it always takes the newest release).
- **Uninstall:**
  ```bash
  curl -fsSL https://raw.githubusercontent.com/MuyleangIng/akkhara-desktop/main/mac-ime/get.sh | bash -s uninstall
  ```
  (or `bash uninstall.sh` from the zip). It is removed from Input Sources and deleted; nothing is left.

Build it yourself: `bash mac-ime/build.sh` (Xcode Command Line Tools; needs
`third_party/onnxruntime-osx-arm64-1.22.0` and `models/akkhara-mini`), then `bash mac-ime/install.sh`.
The keyboard uses the same C++ engine as the Windows one (`core/`), so both give the same top-5.

## Akkhara Desktop test app (Mac, or Windows without installing)

Download from the [latest release](https://github.com/MuyleangIng/akkhara-desktop/releases/latest):

**Windows 10 / 11:** `Akkhara-Desktop-…-windows.exe`
1. Double-click it. Windows may say *"Windows protected your PC"* (the app is new and not signed):
   click **More info → Run anyway**.
2. The first start takes a few seconds. The Akkhara window opens.

**Mac (Apple M-series):** `Akkhara-Desktop-…-mac.zip`
1. Unzip it and move **Akkhara** to *Applications*.
2. The first time, **right-click Akkhara → Open → Open** (the app is not signed by Apple).
   If macOS still blocks it: System Settings → Privacy & Security → **Open Anyway**.
3. To let Akkhara paste for you: System Settings → Privacy & Security → **Accessibility** → turn on
   **Akkhara**. Without it, the Khmer is copied and you press ⌘V yourself.

Then keep Akkhara running, go to any app and press the hotkey (below).

## Run from source (developers)

```bash
cd akkhara-desktop
uv venv --python 3.12 .venv && uv pip install --python .venv/bin/python -e .
bash scripts/get_model.sh              # model → models/akkhara-mini (skip if already copied)
.venv/bin/python -m akkhara_desktop    # Windows: .venv\Scripts\python -m akkhara_desktop
```

## Use it

| Key | What it does |
|---|---|
| **⌃⌥K** (Mac) / **Ctrl+Alt+K** (Windows) | open Akkhara from any app |
| type Sing Khmer | top-5 Khmer suggestions appear as you type |
| **Enter** | insert the highlighted suggestion |
| **↑ / ↓**, click, **Ctrl+1…5** | choose another suggestion |
| **Shift+Enter** | insert exactly what you typed |
| **Esc** | close without inserting |

Your old clipboard text is put back about 1 second after pasting.

**Mac, one time:** to let Akkhara press ⌘V for you, allow the app that runs it (Terminal, or
Python) in **System Settings → Privacy & Security → Accessibility**. Without it, the Khmer is only
copied and you press ⌘V yourself.

## How it works

```
any app ──hotkey──▶ Akkhara window ──▶ akkhara-mini (ONNX, CPU) ──▶ top-5 ──Enter──▶ clipboard + paste ──▶ any app
```

| File | What |
|---|---|
| `akkhara_desktop/engine.py` | the "brain": tokenizer, ONNX greedy + beam search, 6-word chunks + cache (same as the phone) |
| `akkhara_desktop/app.py` | the window (Tk): typing, suggestions, insert |
| `akkhara_desktop/platform_mac.py` | Mac: hotkey (Carbon), clipboard, ⌘V |
| `akkhara_desktop/platform_win.py` | Windows: hotkey, focus, clipboard, Ctrl+V (Windows API) |
| `core/akkhara_engine.cpp` | the same "brain" in C++, used by the Windows and Mac keyboards |
| `windows-ime/` | Windows keyboard (TSF, C++) + Setup (Inno) |
| `mac-ime/` | Mac keyboard (InputMethodKit, Swift): `src/InputController.swift` keys, `CandidateWindow.swift` top-5 window, `test/` typing test |
| `tests/` | `python -m pytest -q` |

## License

Code: Apache-2.0 © 2026 Ing Muyleang. Model: [akkhara-mini](https://huggingface.co/INGMUYLEANG/akkhara-mini) (Apache-2.0).
