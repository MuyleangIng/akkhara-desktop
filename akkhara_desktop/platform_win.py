"""Windows part: global hotkey, clipboard, and pasting into the app you were typing in.

Only the Windows API through ctypes (nothing to install).
- Hotkey: RegisterHotKey(Ctrl+Alt+K) in a small background thread with its own message loop.
- Paste: remember the window you were typing in, put the Khmer on the clipboard,
  give the focus back to that window, then press Ctrl+V for you.
"""

from __future__ import annotations

import ctypes
import threading
from ctypes import wintypes
from typing import Callable

NAME = "windows"
HOTKEY_LABEL = "Ctrl+Alt+K"
PASTE_LABEL = "Ctrl+V"

user32 = ctypes.WinDLL("user32", use_last_error=True)
user32.GetForegroundWindow.restype = wintypes.HWND
user32.SetForegroundWindow.argtypes = [wintypes.HWND]

MOD_ALT, MOD_CONTROL, MOD_NOREPEAT = 0x1, 0x2, 0x4000
VK_K, VK_V, VK_CONTROL = 0x4B, 0x56, 0x11
WM_HOTKEY = 0x0312
KEYEVENTF_KEYUP = 0x2

_target = None  # the window to paste into


def register_hotkey(callback: Callable[[], None], root=None) -> bool:
    """Call `callback` when Ctrl+Alt+K is pressed anywhere. The callback runs on a background
    thread, so the app passes a callback that only puts a message in a queue."""
    ok = threading.Event()
    result = {"ok": False}

    def loop():
        result["ok"] = bool(user32.RegisterHotKey(None, 1, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_K))
        ok.set()
        msg = wintypes.MSG()
        while user32.GetMessageW(ctypes.byref(msg), None, 0, 0) > 0:
            if msg.message == WM_HOTKEY:
                callback()

    threading.Thread(target=loop, daemon=True).start()
    ok.wait(2)
    return result["ok"]


# ---- focus ---------------------------------------------------------------------------------
def remember_target() -> None:
    """Call BEFORE showing Akkhara: remember the window the user was typing in."""
    global _target
    _target = user32.GetForegroundWindow()


def bring_to_front(root) -> None:
    root.deiconify()
    root.lift()
    root.attributes("-topmost", True)
    root.focus_force()


def hide(root) -> None:
    root.withdraw()
    if _target:
        user32.SetForegroundWindow(_target)


# ---- clipboard + paste (through Tk, which handles Unicode text on Windows) ----------------
_root = None


def attach(root) -> None:
    global _root
    _root = root


def get_clipboard() -> str | None:
    try:
        return _root.clipboard_get()
    except Exception:
        return None


def set_clipboard(text: str) -> None:
    _root.clipboard_clear()
    _root.clipboard_append(text)
    _root.update()  # make sure Windows has it before Ctrl+V


def can_auto_paste() -> bool:
    return True


def open_permission_settings() -> None:
    pass


def press_paste() -> None:
    user32.keybd_event(VK_CONTROL, 0, 0, 0)
    user32.keybd_event(VK_V, 0, 0, 0)
    user32.keybd_event(VK_V, 0, KEYEVENTF_KEYUP, 0)
    user32.keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0)


def khmer_fonts() -> list[str]:
    return ["Khmer UI", "Leelawadee UI", "Noto Sans Khmer", "DaunPenh"]
