// Akkhara keyboard for Mac (InputMethodKit). Same behaviour as the Windows keyboard:
//   type Sing Khmer → the Latin is underlined and the top-5 Khmer suggestions appear under it
//   Enter: insert the highlighted Khmer    ↑ ↓ / Tab: choose    click: insert that one
//   Shift+Enter or Esc: keep the Latin as typed    Backspace: delete one letter
// Only a letter starts Sing Khmer; numbers and punctuation on their own are typed as usual.
import Cocoa
import InputMethodKit

@objc(AkkharaInputController)
final class AkkharaInputController: IMKInputController {
    private var latin = ""
    private var items: [String] = []
    private var itemsFor = ""  // the Latin the items belong to
    private var selected = 0

    private static let help = "Enter: insert · ↑↓: choose · Esc: keep Latin"

    private var composing: Bool { !latin.isEmpty }
    // the text box that sent the last key (falls back to IMK's current client)
    private weak var sender: (IMKTextInput & NSObjectProtocol)?
    private var textClient: (IMKTextInput & NSObjectProtocol)? { sender ?? client() }

    override func recognizedEvents(_ sender: Any!) -> Int {
        Int(NSEvent.EventTypeMask.keyDown.rawValue)
    }

    override func activateServer(_ sender: Any!) {
        super.activateServer(sender)
        CandidateWindow.shared.onClick = { [weak self] i in self?.choose(i) }
    }

    override func deactivateServer(_ sender: Any!) {
        finish(with: latin)  // switched keyboard / app: keep what was typed
        super.deactivateServer(sender)
    }

    // the app wants the text now (clicked elsewhere, …): the Latin stays as typed
    override func commitComposition(_ sender: Any!) {
        finish(with: latin)
    }

    // ---- keys ---------------------------------------------------------------------------------
    override func handle(_ event: NSEvent!, client sender: Any!) -> Bool {
        guard let event = event, event.type == .keyDown else { return false }
        if let c = sender as? (IMKTextInput & NSObjectProtocol) { self.sender = c }
        let mods = event.modifierFlags.intersection(.deviceIndependentFlagsMask)
        if !mods.intersection([.command, .control, .option]).isEmpty {  // shortcuts (⌘C …)
            if composing { finish(with: latin) }
            return false
        }
        let chars = event.characters ?? ""

        if !composing {
            guard chars.count == 1, let c = chars.unicodeScalars.first, c.isASCII,
                  CharacterSet.letters.contains(c) else { return false }
            append(chars)
            return true
        }

        switch event.keyCode {
        case 51:  // Backspace
            latin.removeLast()
            if latin.isEmpty { finish(with: "") } else { changed() }
        case 36, 76:  // Return, Enter
            finish(with: mods.contains(.shift) ? latin : choice())
        case 53:  // Esc
            finish(with: latin)
        case 126:  // ↑
            move(-1)
        case 125:  // ↓
            move(+1)
        case 48:  // Tab
            move(mods.contains(.shift) ? -1 : +1)
        case 123, 124, 115, 119, 116, 121, 117:  // ← → Home End PgUp PgDn Delete
            break  // keep the cursor at the end of the Latin
        default:
            guard chars.count == 1, let c = chars.unicodeScalars.first, c.value >= 0x20, c.value < 0x7F else {
                return true
            }
            append(chars)
        }
        return true
    }

    // ---- composition ----------------------------------------------------------------------------
    private func append(_ s: String) {
        latin += s
        changed()
    }

    private func changed() {
        guard let c = textClient else { return }
        let attrs: [NSAttributedString.Key: Any] = [.underlineStyle: NSUnderlineStyle.single.rawValue]
        c.setMarkedText(NSAttributedString(string: latin, attributes: attrs),
                        selectionRange: NSRange(location: latin.utf16.count, length: 0),
                        replacementRange: NSRange(location: NSNotFound, length: 0))
        let text = latin
        Engine.shared.request(text) { [weak self] newItems, final in
            guard let self = self, self.latin == text else { return }
            if final || self.items.isEmpty || self.itemsFor != text || self.items.first != newItems.first {
                self.items = newItems  // fast #1 first, then the top-5
                self.selected = 0
            }
            self.itemsFor = text
            self.refresh()
        }
        refresh()
    }

    private func move(_ d: Int) {
        guard !items.isEmpty else { return }
        selected = (selected + d + items.count) % items.count
        refresh()
    }

    private func choice() -> String {
        if !items.isEmpty, itemsFor == latin { return items[min(selected, items.count - 1)] }
        return Engine.shared.quickNow(latin) ?? latin  // Enter came before the suggestions
    }

    private func choose(_ i: Int) {
        guard composing, itemsFor == latin, i >= 0, i < items.count else { return }
        finish(with: items[i])
    }

    /// Insert `text` in place of the underlined Latin and reset.
    private func finish(with text: String) {
        let had = composing
        latin = ""
        items = []
        itemsFor = ""
        selected = 0
        Engine.shared.cancel()
        CandidateWindow.shared.hide()
        guard had, let c = textClient else { return }
        c.insertText(text, replacementRange: NSRange(location: NSNotFound, length: 0))
    }

    private func refresh() {
        guard composing, let c = textClient else {
            CandidateWindow.shared.hide()
            return
        }
        var status = Self.help
        if let f = Engine.shared.failed { status = "Akkhara: model not found (\(f)). Reinstall; Enter keeps the Latin." }
        else if !Engine.shared.ready { status = "Akkhara is loading…" }
        var line = NSRect.zero
        _ = c.attributes(forCharacterIndex: 0, lineHeightRectangle: &line)
        let shown = items
        CandidateWindow.shared.show(items: shown, selected: selected, status: status, caret: line)
    }
}
