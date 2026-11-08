"""Akkhara Desktop (test app): Sing Khmer → Khmer in any app, on Mac and Windows.

How to use:
  1. Start it:  python -m akkhara_desktop
  2. In any app (Word, Chrome, Telegram …) press the hotkey: ⌃⌥K on Mac, Ctrl+Alt+K on Windows.
  3. Type Sing Khmer:   nh jg tv sala rean
  4. Pick a suggestion: Enter = the highlighted one, ↑/↓ to move, click, or Ctrl+1…5.
     Shift+Enter keeps exactly what you typed. Esc closes without inserting.
  5. Akkhara closes and the Khmer is pasted where you were typing.

Typing is never sent anywhere: the model runs on this computer (onnxruntime, CPU).
"""

from __future__ import annotations

import os
import queue
import sys
import threading
import time
import tkinter as tk
import tkinter.font as tkfont
from pathlib import Path

from akkhara_desktop.engine import Engine

if sys.platform == "darwin":
    from akkhara_desktop import platform_mac as osx
elif sys.platform == "win32":
    from akkhara_desktop import platform_win as osx
else:
    sys.exit("Akkhara Desktop supports macOS and Windows.")

HERE = Path(__file__).resolve().parent
MODEL_DIR = Path(os.environ.get("AKKHARA_MODEL", HERE.parent / "models" / "akkhara-mini"))
if getattr(sys, "frozen", False):  # inside a packaged app (PyInstaller)
    MODEL_DIR = Path(sys._MEIPASS) / "models" / "akkhara-mini"

# colours (light / dark follow the system on Mac; one simple theme here for the test app)
BG, CARD, INK, MUTED, ACCENT, SEL = "#ffffff", "#f4f6f8", "#1b1f24", "#6b7280", "#1baf7a", "#e3f6ee"


class App:
    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("Akkhara")
        self.root.configure(bg=BG)
        self.root.resizable(False, False)
        self.root.attributes("-topmost", True)
        osx.attach(self.root)

        self.events: queue.Queue = queue.Queue()  # messages from background threads → UI
        self.request = 0  # grows on every key press; older work stops when it changes
        self.items: list[str] = []
        self.sel = 0

        self._fonts()
        self._build()
        self.status("Loading the model…")
        threading.Thread(target=self._load, daemon=True).start()

        ok = osx.register_hotkey(lambda: self.events.put(("hotkey",)), self.root)
        self.hotkey_ok = ok
        self.root.protocol("WM_DELETE_WINDOW", self.cancel)
        self.root.after(30, self._poll)

    # ---- layout ----------------------------------------------------------------------------
    def _fonts(self) -> None:
        have = set(tkfont.families(self.root))
        khmer = next((f for f in osx.khmer_fonts() if f in have), None)
        self.f_latin = tkfont.Font(family="Helvetica", size=18)
        self.f_khmer = tkfont.Font(family=khmer or "TkDefaultFont", size=20)
        self.f_small = tkfont.Font(family="Helvetica", size=11)
        self.khmer_font_name = khmer or "(default font)"

    def _build(self) -> None:
        pad = dict(padx=14)
        tk.Label(self.root, text="Sing Khmer → ខ្មែរ", bg=BG, fg=MUTED, font=self.f_small,
                 anchor="w").pack(fill="x", pady=(12, 2), **pad)
        self.entry = tk.Entry(self.root, font=self.f_latin, width=34, relief="flat", bg=CARD, fg=INK,
                              insertbackground=INK, highlightthickness=2, highlightcolor=ACCENT,
                              highlightbackground=CARD)
        self.entry.pack(fill="x", ipady=6, **pad)
        self.entry.bind("<KeyRelease>", self._on_key)
        self.entry.bind("<Return>", lambda e: self.pick(self.sel))
        self.entry.bind("<KP_Enter>", lambda e: self.pick(self.sel))
        self.entry.bind("<Shift-Return>", lambda e: self.keep_typed())
        self.entry.bind("<Escape>", lambda e: self.cancel())
        self.entry.bind("<Down>", lambda e: self.move(1))
        self.entry.bind("<Up>", lambda e: self.move(-1))
        for i in range(1, 6):
            self.entry.bind(f"<Control-Key-{i}>", lambda e, i=i: self.pick(i - 1))

        self.list = tk.Frame(self.root, bg=BG)
        self.list.pack(fill="x", pady=(8, 4), **pad)
        self.rows: list[tk.Label] = []
        for i in range(5):
            row = tk.Label(self.list, text="", anchor="w", bg=BG, fg=INK, font=self.f_khmer,
                           padx=8, pady=2, cursor="hand2")
            row.pack(fill="x")
            row.bind("<Button-1>", lambda e, i=i: self.pick(i))
            self.rows.append(row)

        self.status_lbl = tk.Label(self.root, text="", bg=BG, fg=MUTED, font=self.f_small,
                                   anchor="w", justify="left", wraplength=470)
        self.status_lbl.pack(fill="x", pady=(2, 12), **pad)

    def status(self, text: str) -> None:
        self.status_lbl.config(text=text)

    def help_text(self) -> str:
        hk = osx.HOTKEY_LABEL if self.hotkey_ok else "(hotkey not available)"
        auto = "" if osx.can_auto_paste() else (
            f"\nAuto-paste is off: allow Akkhara in Accessibility settings, or press {osx.PASTE_LABEL} yourself.")
        return f"Enter = insert · ↑↓ choose · Shift+Enter = keep as typed · Esc = close · {hk} = open{auto}"

    # ---- model -----------------------------------------------------------------------------
    def _load(self) -> None:
        t = time.time()
        try:
            self.engine = Engine(MODEL_DIR)
            self.engine.quick("nh jg tv sala rean")  # warm-up: the first run is slower
            self.events.put(("loaded", time.time() - t))
        except Exception as e:  # e.g. the model files are missing
            self.events.put(("error", f"Could not load the model from {MODEL_DIR}: {e}"))

    def _on_key(self, event) -> None:
        if event.keysym in ("Return", "KP_Enter", "Escape", "Up", "Down", "Shift_L", "Shift_R"):
            return
        text = self.entry.get()
        self.request += 1
        req = self.request
        if not text.strip():
            self.show([])
            return
        if not hasattr(self, "engine"):
            return
        threading.Thread(target=self._work, args=(text, req), daemon=True).start()

    def _work(self, text: str, req: int) -> None:
        """Background: fast #1 first, then the full top-5. Stops if a newer key press came."""
        stale = lambda: req != self.request  # noqa: E731
        t = time.time()
        first = self.engine.quick(text, stale)
        if stale():
            return
        self.events.put(("quick", req, first, time.time() - t))
        top = self.engine.candidates(text, stale)
        if top and not stale():
            self.events.put(("top", req, top, time.time() - t))

    # ---- events from threads -----------------------------------------------------------------
    def _poll(self) -> None:
        try:
            while True:
                ev = self.events.get_nowait()
                kind = ev[0]
                if kind == "hotkey":
                    self.open()
                elif kind == "loaded":
                    self.status(f"Ready ({ev[1]:.1f} s) · font: {self.khmer_font_name}\n" + self.help_text())
                    if self.entry.get().strip():
                        self._on_key(type("E", (), {"keysym": ""})())
                elif kind == "error":
                    self.status(ev[1])
                elif kind in ("quick", "top") and ev[1] == self.request:
                    items = [ev[2]] if kind == "quick" else ev[2]
                    if kind == "quick" and self.items and self.items[0] == ev[2]:
                        items = self.items  # keep the old list while the new top-5 is coming
                    self.show(items)
                    if kind == "top":
                        self.status(f"{ev[3] * 1000:.0f} ms · " + self.help_text())
        except queue.Empty:
            pass
        self.root.after(30, self._poll)

    # ---- candidates --------------------------------------------------------------------------
    def show(self, items: list[str]) -> None:
        self.items = items
        self.sel = 0
        for i, row in enumerate(self.rows):
            row.config(text=f"{i + 1}  {items[i]}" if i < len(items) else "")
        self._paint()

    def _paint(self) -> None:
        for i, row in enumerate(self.rows):
            row.config(bg=SEL if (i == self.sel and i < len(self.items)) else BG)

    def move(self, d: int) -> str:
        if self.items:
            self.sel = (self.sel + d) % len(self.items)
            self._paint()
        return "break"

    # ---- open / insert / close ---------------------------------------------------------------
    def open(self) -> None:
        osx.remember_target()
        self.entry.delete(0, "end")
        self.show([])
        osx.bring_to_front(self.root)
        self.entry.focus_set()

    def pick(self, i: int) -> str:
        if not self.items:  # model not ready yet / nothing typed: Enter keeps the Latin
            return self.keep_typed()
        self.insert(self.items[min(i, len(self.items) - 1)])
        return "break"

    def keep_typed(self) -> str:
        if self.entry.get():
            self.insert(self.entry.get())
        return "break"

    def insert(self, text: str) -> None:
        """Paste `text` into the app the user came from, then put the old clipboard back."""
        old = osx.get_clipboard()
        osx.set_clipboard(text)
        self.entry.delete(0, "end")
        self.show([])
        osx.hide(self.root)
        if osx.can_auto_paste():
            self.root.after(150, osx.press_paste)  # wait until the other app has the focus
            if old is not None:
                self.root.after(900, lambda: osx.set_clipboard(old))
        elif not getattr(self, "asked_permission", False):
            self.asked_permission = True  # once: show where to allow auto-paste (Mac)
            osx.open_permission_settings()

    def cancel(self) -> None:
        self.request += 1
        osx.hide(self.root)

    def run(self) -> None:
        self.status(self.help_text())
        self.entry.focus_set()
        self.root.mainloop()


def smoke_test() -> None:
    """`AKKHARA_SMOKE=1`: load the model, print one result, exit (used to check a build)."""
    e = Engine(MODEL_DIR)
    line = f"smoke: Tk {tk.TkVersion} | {e.candidates('nh jg tv sala rean')[:3]}"
    out = os.environ["AKKHARA_SMOKE"]
    if out == "1":
        print(line, flush=True)
    else:  # a windowed .exe has no console: write the result to a file
        Path(out).write_text(line, encoding="utf-8")


def main() -> None:
    if os.environ.get("AKKHARA_SMOKE"):
        smoke_test()
        return
    App().run()


if __name__ == "__main__":
    main()
