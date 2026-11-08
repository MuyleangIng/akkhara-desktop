# Akkhara Desktop (test app)

Type **Sing Khmer** in any app on **Mac or Windows** and get **Khmer**:
press a hotkey, type `nh jg tv sala rean`, press Enter → `ខ្ញុំចង់ទៅសាលារៀន` is pasted where you were typing.

It uses the same AI model as the Akkhara phone keyboard
([akkhara-mini](https://huggingface.co/INGMUYLEANG/akkhara-mini), ONNX int8, 18.5 MB) and runs
**offline on your computer**; nothing you type is sent anywhere.

> This is a **test app**, not yet a real system keyboard. Step 2 = a real Mac keyboard (Swift),
> step 3 = a real Windows keyboard.

## Install (no Python needed)

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
| `tests/` | `python -m pytest -q` |

## License

Code: Apache-2.0 © 2026 Ing Muyleang. Model: [akkhara-mini](https://huggingface.co/INGMUYLEANG/akkhara-mini) (Apache-2.0).
