// Key test without a real app: a fake text box (FakeClient) receives what the keyboard writes.
// Real key events go through AkkharaInputController.handle, exactly like when you type.
//   built by build.sh as build/akkhara_keytest; run: build/akkhara_keytest <model_dir>
import Cocoa
import InputMethodKit

final class FakeClient: NSObject, IMKTextInput {
    var text = ""      // committed text
    var marked = ""    // underlined Latin
    func insertText(_ string: Any!, replacementRange: NSRange) {
        text += (string as? String) ?? (string as? NSAttributedString)?.string ?? ""
        marked = ""
    }
    func setMarkedText(_ string: Any!, selectionRange: NSRange, replacementRange: NSRange) {
        marked = (string as? String) ?? (string as? NSAttributedString)?.string ?? ""
    }
    func selectedRange() -> NSRange { NSRange(location: (text as NSString).length, length: 0) }
    func markedRange() -> NSRange { NSRange(location: (text as NSString).length, length: (marked as NSString).length) }
    func attributedSubstring(from range: NSRange) -> NSAttributedString! { NSAttributedString(string: "") }
    func length() -> Int { (text as NSString).length }
    func characterIndex(for point: NSPoint, tracking mappingMode: IMKLocationToOffsetMappingMode,
                        inMarkedRange: UnsafeMutablePointer<ObjCBool>!) -> Int { 0 }
    func attributes(forCharacterIndex index: Int, lineHeightRectangle: UnsafeMutablePointer<NSRect>!) -> [AnyHashable: Any]! {
        lineHeightRectangle?.pointee = NSRect(x: 200, y: 600, width: 1, height: 18)
        return [:]
    }
    func validAttributesForMarkedText() -> [Any]! { [] }
    func overrideKeyboard(withKeyboardNamed keyboardUniqueName: String!) {}
    func selectMode(_ modeIdentifier: String!) {}
    func supportsUnicode() -> Bool { true }
    func bundleIdentifier() -> String! { "test.fake" }
    func windowLevel() -> CGWindowLevel { 0 }
    func supportsProperty(_ property: TSMDocumentPropertyTag) -> Bool { false }
    func uniqueClientIdentifierString() -> String! { "fake" }
    func string(from range: NSRange, actualRange: NSRangePointer!) -> String! { "" }
    func firstRect(forCharacterRange aRange: NSRange, actualRange: NSRangePointer!) -> NSRect { .zero }
}

func key(_ chars: String, _ code: UInt16, _ mods: NSEvent.ModifierFlags = []) -> NSEvent {
    NSEvent.keyEvent(with: .keyDown, location: .zero, modifierFlags: mods, timestamp: 0, windowNumber: 0,
                     context: nil, characters: chars, charactersIgnoringModifiers: chars, isARepeat: false, keyCode: code)!
}

func wait(_ s: Double) { RunLoop.main.run(until: Date(timeIntervalSinceNow: s)) }

let server = IMKServer(name: "Akkhara_KeyTest_Connection", bundleIdentifier: "com.muyleanging.inputmethod.Akkhara")
Engine.shared.start(modelDir: CommandLine.arguments[1])
for _ in 0..<100 where !Engine.shared.ready { wait(0.1) }
guard Engine.shared.ready else { print("FAIL: model did not load: \(Engine.shared.failed ?? "?")"); exit(1) }

var failures = 0
func check(_ name: String, _ got: String, _ want: String) {
    print((got == want ? "PASS " : "FAIL ") + name + ": " + got + (got == want ? "" : "   (want " + want + ")"))
    if got != want { failures += 1 }
}

/// Type `latin` letter by letter, wait for the top-5, then press the keys in `then`.
func run(_ latin: String, then keys: [NSEvent], waitFirst: Double = 1.0) -> FakeClient {
    let client = FakeClient()
    let c = AkkharaInputController()
    for ch in latin { _ = c.handle(key(String(ch), ch == " " ? 49 : 0), client: client) }
    wait(waitFirst)
    for k in keys { _ = c.handle(k, client: client) }
    return client
}

let enter = key("\r", 36), esc = key("\u{1b}", 53), down = key("", 125), shiftEnter = key("\r", 36, .shift)
check("Enter", run("nh jg tv sala rean", then: [enter]).text, "ខ្ញុំចង់ទៅសាលារៀន")
check("Down + Enter (full name #2)", run("nh tver ka nv mptc", then: [down, enter]).text,
      "ខ្ញុំធ្វើការនៅក្រសួងប្រៃសណីយ៍និងទូរគមនាគមន៍")
check("Esc keeps the Latin", run("sok sabay", then: [esc]).text, "sok sabay")
check("Shift+Enter keeps the Latin", run("hello", then: [shiftEnter]).text, "hello")
check("Enter at once (before the top-5)", run("nh jg tv sala rean", then: [enter], waitFirst: 0).text, "ខ្ញុំចង់ទៅសាលារៀន")
let bs = run("sala x", then: [key("\u{7f}", 51), key("\u{7f}", 51)])
check("Backspace", bs.marked, "sala")
let digit = FakeClient()
let c2 = AkkharaInputController()
check("a digit alone is not taken", String(c2.handle(key("1", 18), client: digit)), "false")
check("⌘C is not taken", String(c2.handle(key("c", 8, .command), client: digit)), "false")

Engine.shared.shutdown()
print(failures == 0 ? "ALL PASS" : "\(failures) FAILED")
fflush(stdout)
// quit at once: skip C++ static clean-up (ONNX Runtime threads can still be ending; the real keyboard never quits itself)
_exit(failures == 0 ? 0 : 1)
