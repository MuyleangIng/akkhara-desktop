"""macOS part: global hotkey, clipboard, and pasting into the app you were typing in.

- Hotkey: Carbon RegisterEventHotKey (through ctypes). It needs NO special permission,
  and the key press is "eaten" (the other app does not see it).
- Paste: put the Khmer on the clipboard, hide Akkhara (macOS then gives the focus back to
  the app you came from), then press ⌘V for you. Pressing ⌘V for you needs the
  Accessibility permission (System Settings → Privacy & Security → Accessibility).
  Without it, the Khmer is only copied and you press ⌘V yourself.
"""

from __future__ import annotations

import ctypes
import ctypes.util
from typing import Callable

import AppKit
import Quartz

NAME = "mac"
HOTKEY_LABEL = "⌃⌥K"  # Control + Option + K
PASTE_LABEL = "⌘V"

# ---- hotkey (Carbon, ctypes) ---------------------------------------------------------------
_carbon = ctypes.CDLL("/System/Library/Frameworks/Carbon.framework/Carbon")


class _EventHotKeyID(ctypes.Structure):
    _fields_ = [("signature", ctypes.c_uint32), ("id", ctypes.c_uint32)]


class _EventTypeSpec(ctypes.Structure):
    _fields_ = [("eventClass", ctypes.c_uint32), ("eventKind", ctypes.c_uint32)]


def _fourcc(s: str) -> int:
    return int.from_bytes(s.encode("ascii"), "big")


_HANDLER = ctypes.CFUNCTYPE(ctypes.c_int32, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p)
_carbon.GetApplicationEventTarget.restype = ctypes.c_void_p
_carbon.InstallEventHandler.argtypes = [ctypes.c_void_p, _HANDLER, ctypes.c_ulong,
                                        ctypes.POINTER(_EventTypeSpec), ctypes.c_void_p, ctypes.c_void_p]
_carbon.RegisterEventHotKey.argtypes = [ctypes.c_uint32, ctypes.c_uint32, _EventHotKeyID,
                                        ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_void_p)]
_keep_alive: list = []  # ctypes callbacks must not be garbage-collected

KEY_K = 40  # kVK_ANSI_K
CONTROL, OPTION = 0x1000, 0x0800


def register_hotkey(callback: Callable[[], None], root=None) -> bool:
    """Call `callback` (on the main thread) when ⌃⌥K is pressed anywhere. True if it worked."""
    def handler(_next, _event, _data):
        callback()
        return 0

    cb = _HANDLER(handler)
    spec = _EventTypeSpec(_fourcc("keyb"), 5)  # kEventClassKeyboard, kEventHotKeyPressed
    target = _carbon.GetApplicationEventTarget()
    if _carbon.InstallEventHandler(target, cb, 1, ctypes.byref(spec), None, None) != 0:
        return False
    ref = ctypes.c_void_p()
    ok = _carbon.RegisterEventHotKey(KEY_K, CONTROL | OPTION, _EventHotKeyID(_fourcc("AKKH"), 1),
                                     target, 0, ctypes.byref(ref)) == 0
    _keep_alive.extend([cb, ref])
    return ok


# ---- focus ---------------------------------------------------------------------------------
def remember_target() -> None:
    """Nothing to do on macOS: hiding Akkhara gives the focus back to the previous app."""


def attach(root) -> None:
    """Nothing to keep on macOS (the clipboard goes through NSPasteboard)."""


def bring_to_front(root) -> None:
    app = AppKit.NSApplication.sharedApplication()
    app.unhide_(None)
    app.activateIgnoringOtherApps_(True)
    root.deiconify()
    root.lift()
    root.focus_force()


def hide(root) -> None:
    root.withdraw()
    AppKit.NSApplication.sharedApplication().hide_(None)  # → focus returns to the previous app


# ---- clipboard + paste ---------------------------------------------------------------------
def get_clipboard() -> str | None:
    pb = AppKit.NSPasteboard.generalPasteboard()
    return pb.stringForType_(AppKit.NSPasteboardTypeString)


def set_clipboard(text: str) -> None:
    pb = AppKit.NSPasteboard.generalPasteboard()
    pb.clearContents()
    pb.setString_forType_(text, AppKit.NSPasteboardTypeString)


def _ax_trusted() -> bool:
    lib = ctypes.CDLL(ctypes.util.find_library("ApplicationServices"))
    lib.AXIsProcessTrusted.restype = ctypes.c_bool
    return bool(lib.AXIsProcessTrusted())


def can_auto_paste() -> bool:
    return _ax_trusted()


def open_permission_settings() -> None:
    url = "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"
    AppKit.NSWorkspace.sharedWorkspace().openURL_(AppKit.NSURL.URLWithString_(url))


def press_paste() -> None:
    """Press ⌘V in the app that has the focus."""
    for down in (True, False):
        ev = Quartz.CGEventCreateKeyboardEvent(None, 9, down)  # 9 = kVK_ANSI_V
        Quartz.CGEventSetFlags(ev, Quartz.kCGEventFlagMaskCommand)
        Quartz.CGEventPost(Quartz.kCGHIDEventTap, ev)


def khmer_fonts() -> list[str]:
    return ["Khmer Sangam MN", "Khmer MN", "Noto Sans Khmer"]
