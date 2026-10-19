// Original project icon, rendered at build time without game resources.
import AppKit
let destination = CommandLine.arguments[1]
let bitmap = NSBitmapImageRep(bitmapDataPlanes: nil, pixelsWide: 1024, pixelsHigh: 1024, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
NSGraphicsContext.saveGraphicsState()
NSGraphicsContext.current = NSGraphicsContext(bitmapImageRep: bitmap)
NSColor(calibratedRed: 0.10, green: 0.64, blue: 0.62, alpha: 1).setFill()
NSBezierPath(rect: NSRect(x: 0, y: 0, width: 1024, height: 1024)).fill()
NSColor(calibratedRed: 0.98, green: 0.87, blue: 0.56, alpha: 1).setFill()
NSBezierPath(ovalIn: NSRect(x: 120, y: 145, width: 784, height: 220)).fill()
let leaf = NSBezierPath()
leaf.move(to: NSPoint(x: 340, y: 350))
leaf.curve(to: NSPoint(x: 753, y: 805), controlPoint1: NSPoint(x: 180, y: 700), controlPoint2: NSPoint(x: 570, y: 865))
leaf.curve(to: NSPoint(x: 340, y: 350), controlPoint1: NSPoint(x: 900, y: 420), controlPoint2: NSPoint(x: 570, y: 250))
NSColor.white.setFill(); leaf.fill()
let vein = NSBezierPath()
vein.move(to: NSPoint(x: 322, y: 300)); vein.curve(to: NSPoint(x: 684, y: 724), controlPoint1: NSPoint(x: 405, y: 455), controlPoint2: NSPoint(x: 535, y: 590))
vein.lineWidth = 28; vein.lineCapStyle = .round
NSColor(calibratedRed: 0.10, green: 0.64, blue: 0.62, alpha: 1).setStroke(); vein.stroke()
NSGraphicsContext.restoreGraphicsState()
try bitmap.representation(using: .png, properties: [:])!.write(to: URL(fileURLWithPath: destination))
