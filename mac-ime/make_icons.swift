// Draws the icons: menu-bar icon (the letter អ, black on clear, macOS tints it) and the app icon.
//   swift make_icons.swift <out_dir>
import Cocoa

let out = URL(fileURLWithPath: CommandLine.arguments[1])

func image(_ px: Int, _ draw: (NSRect) -> Void) -> NSBitmapImageRep {
    let rep = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: px, pixelsHigh: px, bitsPerSample: 8,
                               samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
                               colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: rep)
    draw(NSRect(x: 0, y: 0, width: px, height: px))
    NSGraphicsContext.restoreGraphicsState()
    return rep
}

func letter(_ s: String, in r: NSRect, size: CGFloat, color: NSColor) {
    let font = NSFont(name: "Khmer Sangam MN", size: size) ?? NSFont.systemFont(ofSize: size)
    let a: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: color]
    let b = (s as NSString).boundingRect(with: r.size, options: [.usesLineFragmentOrigin], attributes: a)
    (s as NSString).draw(at: NSPoint(x: r.midX - b.width / 2, y: r.midY - b.height / 2 + size * 0.06), withAttributes: a)
}

// menu bar: 16 pt at 1x and 2x in one TIFF
let menu = NSImage(size: NSSize(width: 16, height: 16))
for px in [16, 32] {
    menu.addRepresentation(image(px) { r in letter("អ", in: r, size: CGFloat(px) * 0.9, color: .black) })
}
try! menu.tiffRepresentation!.write(to: out.appendingPathComponent("menu-icon.tiff"))

// app icon: blue rounded square with a white អ
let iconset = out.appendingPathComponent("AppIcon.iconset")
try? FileManager.default.createDirectory(at: iconset, withIntermediateDirectories: true)
for (name, px) in [("16x16", 16), ("16x16@2x", 32), ("32x32", 32), ("32x32@2x", 64), ("128x128", 128),
                   ("128x128@2x", 256), ("256x256", 256), ("256x256@2x", 512), ("512x512", 512)] {
    let rep = image(px) { r in
        let inset = r.insetBy(dx: r.width * 0.06, dy: r.width * 0.06)
        NSColor(red: 0.10, green: 0.36, blue: 0.85, alpha: 1).setFill()
        NSBezierPath(roundedRect: inset, xRadius: inset.width * 0.22, yRadius: inset.width * 0.22).fill()
        letter("អ", in: inset, size: CGFloat(px) * 0.62, color: .white)
    }
    try! rep.representation(using: .png, properties: [:])!.write(to: iconset.appendingPathComponent("icon_\(name).png"))
}
