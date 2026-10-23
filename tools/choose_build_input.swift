import AppKit
let kind = CommandLine.arguments.dropFirst().first ?? "game"
let app = NSApplication.shared
app.setActivationPolicy(.accessory)
let panel = NSOpenPanel()
panel.canChooseDirectories = kind == "game"
panel.canChooseFiles = kind != "game"
panel.allowsMultipleSelection = false
panel.title = kind == "game" ? "选择动森游戏目录（本体和 3.0.3 更新 NSP）" : "选择自己的 prod.keys"
panel.prompt = "选择"
app.activate(ignoringOtherApps: true)
if panel.runModal() == .OK, let url = panel.url { print(url.path) } else { exit(130) }
