// Akkhara for Mac: an input method (System Settings → Keyboard → Input Sources → + → Khmer → Akkhara).
//   Akkhara              run by macOS as the keyboard (from ~/Library/Input Methods)
//   Akkhara --register   add + turn on Akkhara in Input Sources (used by the installer)
//   Akkhara --unregister turn it off (used by the uninstaller)
//   Akkhara --selftest   load the model and print the top-5 for a few sentences (build check)
import Carbon
import Cocoa
import InputMethodKit

let bundleID = Bundle.main.bundleIdentifier ?? "com.muyleanging.inputmethod.Akkhara"

func akkharaSources() -> [TISInputSource] {
    let filter = [kTISPropertyBundleID as String: bundleID] as CFDictionary
    return (TISCreateInputSourceList(filter, true)?.takeRetainedValue() as? [TISInputSource]) ?? []
}

let args = CommandLine.arguments
if args.contains("--register") {
    let st = TISRegisterInputSource(Bundle.main.bundleURL as CFURL)
    var n = 0
    for s in akkharaSources() {
        if TISEnableInputSource(s) == noErr { n += 1 }
    }
    print(st == noErr && n > 0 ? "Akkhara added to Input Sources" : "register: status \(st), enabled \(n)")
    exit(st == noErr && n > 0 ? 0 : 1)
}
if args.contains("--unregister") {
    for s in akkharaSources() { TISDisableInputSource(s) }
    print("Akkhara removed from Input Sources")
    exit(0)
}

if args.contains("--selftest") {
    let dir = Bundle.main.resourceURL!.appendingPathComponent("model").path
    var err = [CChar](repeating: 0, count: 512)
    guard let h = akk_open(dir, &err, err.count) else { print("FAIL: \(String(cString: err))"); exit(1) }
    var buf = [CChar](repeating: 0, count: 8192)
    for s in ["nh jg tv sala rean", "nh tver ka nv mptc", "theanakear aba"] {
        let t0 = Date()
        let n = akk_candidates(h, 0, s, &buf, buf.count)
        let ms = Int(Date().timeIntervalSince(t0) * 1000)
        print("\(s) → \(n) [\(ms) ms]\n  " + String(cString: buf).replacingOccurrences(of: "\n", with: " | "))
    }
    akk_close(h)
    exit(0)
}

let connection = Bundle.main.infoDictionary?["InputMethodConnectionName"] as? String ?? "Akkhara_Connection"
let server = IMKServer(name: connection, bundleIdentifier: bundleID)
Engine.shared.start()
withExtendedLifetime(server) {
    NSApplication.shared.run()
}
