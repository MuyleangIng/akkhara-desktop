// The small window under the text with the top-5 Khmer suggestions (like the Windows keyboard).
// It never takes the focus away from the app you are typing in.
import Cocoa

final class CandidateWindow {
    static let shared = CandidateWindow()

    var onClick: ((Int) -> Void)?
    private let panel: NSPanel
    private let view = CandidateView()

    private init() {
        panel = NSPanel(contentRect: NSRect(x: 0, y: 0, width: 300, height: 100),
                        styleMask: [.borderless, .nonactivatingPanel], backing: .buffered, defer: true)
        panel.level = NSWindow.Level(rawValue: NSWindow.Level.popUpMenu.rawValue + 1)
        panel.isOpaque = false
        panel.backgroundColor = .clear
        panel.hasShadow = true
        panel.hidesOnDeactivate = false
        panel.collectionBehavior = [.canJoinAllSpaces, .fullScreenAuxiliary, .ignoresCycle]
        panel.contentView = view
        view.onClick = { [weak self] i in self?.onClick?(i) }
    }

    /// items: suggestions, selected: highlighted row, status: grey help line,
    /// caret: the text line on screen (macOS coordinates, origin bottom-left).
    func show(items: [String], selected: Int, status: String, caret: NSRect) {
        view.items = items
        view.selected = selected
        view.status = status
        let size = view.fittingSize()
        var origin = NSPoint(x: caret.minX, y: caret.minY - size.height - 6)
        if let screen = NSScreen.screens.first(where: { $0.frame.contains(caret.origin) }) ?? NSScreen.main {
            let f = screen.visibleFrame
            if origin.y < f.minY { origin.y = caret.maxY + 6 }  // no room below: show above the text
            origin.x = min(max(origin.x, f.minX), f.maxX - size.width)
        }
        panel.setFrame(NSRect(origin: origin, size: size), display: true)
        view.needsDisplay = true
        panel.orderFrontRegardless()
    }

    func hide() { panel.orderOut(nil) }
}

private final class CandidateView: NSView {
    var items: [String] = []
    var selected = 0
    var status = ""
    var onClick: ((Int) -> Void)?

    private let itemFont = NSFont.systemFont(ofSize: 20)
    private let numFont = NSFont.monospacedDigitSystemFont(ofSize: 13, weight: .medium)
    private let statusFont = NSFont.systemFont(ofSize: 11)
    private let rowH: CGFloat = 34, pad: CGFloat = 8, statusH: CGFloat = 22

    override var isFlipped: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    func fittingSize() -> NSSize {
        var w: CGFloat = 260
        for s in items { w = max(w, (s as NSString).size(withAttributes: [.font: itemFont]).width + 56) }
        w = max(w, (status as NSString).size(withAttributes: [.font: statusFont]).width + 2 * pad + 8)
        return NSSize(width: min(ceil(w), 900), height: CGFloat(items.count) * rowH + statusH + 2 * pad)
    }

    override func draw(_ dirtyRect: NSRect) {
        let dark = effectiveAppearance.bestMatch(from: [.darkAqua, .aqua]) == .darkAqua
        let bg = NSBezierPath(roundedRect: bounds.insetBy(dx: 0.5, dy: 0.5), xRadius: 10, yRadius: 10)
        (dark ? NSColor(white: 0.15, alpha: 0.98) : NSColor(white: 0.99, alpha: 0.98)).setFill()
        bg.fill()
        NSColor.separatorColor.setStroke()
        bg.stroke()

        let text: NSColor = dark ? .white : .black
        let muted: NSColor = dark ? NSColor(white: 0.62, alpha: 1) : NSColor(white: 0.42, alpha: 1)
        for (i, s) in items.enumerated() {
            let r = NSRect(x: pad / 2, y: pad + CGFloat(i) * rowH, width: bounds.width - pad, height: rowH)
            let isSel = i == selected
            if isSel {
                NSColor.controlAccentColor.setFill()
                NSBezierPath(roundedRect: r, xRadius: 7, yRadius: 7).fill()
            }
            let num = "\(i + 1)" as NSString
            num.draw(at: NSPoint(x: r.minX + 10, y: r.midY - 8),
                     withAttributes: [.font: numFont, .foregroundColor: isSel ? NSColor.white : muted])
            let str = s as NSString
            let h = str.size(withAttributes: [.font: itemFont]).height
            str.draw(at: NSPoint(x: r.minX + 32, y: r.midY - h / 2),
                     withAttributes: [.font: itemFont, .foregroundColor: isSel ? NSColor.white : text])
        }
        (status as NSString).draw(at: NSPoint(x: pad + 4, y: pad + CGFloat(items.count) * rowH + 4),
                                  withAttributes: [.font: statusFont, .foregroundColor: muted])
    }

    override func mouseDown(with event: NSEvent) {
        let p = convert(event.locationInWindow, from: nil)
        let i = Int((p.y - pad) / rowH)
        if p.y >= pad, i >= 0, i < items.count { onClick?(i) }
    }
}
