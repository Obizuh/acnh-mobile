import AppKit

let kind = CommandLine.arguments.dropFirst().first ?? "game"
let app = NSApplication.shared
app.setActivationPolicy(.accessory)
let step: String
let subject: String
let detail: String
let button: String
switch kind {
case "keys":
    step = "第 2 步 / 共 3 步"
    subject = "选择 prod.keys 文件"
    detail = "请选择名为 prod.keys 的密钥文件，不是文件夹，也不是 title.keys。"
    button = "使用此密钥"
case "output":
    step = "第 3 步 / 共 3 步"
    subject = "选择成品保存文件夹"
    detail = "生成的 IPA、数据包或 Mac 应用将保存在这里。可以新建文件夹。"
    button = "保存到此文件夹"
default:
    step = "第 1 步 / 共 3 步"
    subject = "动森游戏目录 · 仅支持 3.0.3"
    detail = "当前仅支持游戏 3.0.3。请选择同时包含本体和 3.0.3 更新 NSP 的文件夹，其他更新版本无法构建。"
    button = "使用此游戏目录"
}

final class KeySelectionDelegate: NSObject, NSOpenSavePanelDelegate {
    func panel(_ sender: Any, shouldEnable url: URL) -> Bool {
        url.hasDirectoryPath || url.lastPathComponent == "prod.keys"
    }
    func panel(_ sender: Any, validate url: URL) throws {
        if url.hasDirectoryPath || url.lastPathComponent != "prod.keys" {
            throw NSError(domain: "ACNHBuildInput", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "请选择名为 prod.keys 的密钥文件。"])
        }
    }
}
let panel = NSOpenPanel()
panel.canChooseDirectories = kind != "keys"
panel.canChooseFiles = kind == "keys"
panel.allowsMultipleSelection = false
panel.canCreateDirectories = kind == "output"
panel.title = "\(step)：\(subject)"
panel.message = detail
panel.prompt = button

// Native titles use system styling; use a prominent red accessory heading as well.
let accessory = NSView(frame: NSRect(x: 0, y: 0, width: 540, height: 72))
let heading = NSTextField(labelWithString: subject)
heading.font = .systemFont(ofSize: 21, weight: .bold)
heading.textColor = .systemRed
heading.frame = NSRect(x: 0, y: 42, width: 540, height: 28)
let caption = NSTextField(wrappingLabelWithString: detail)
caption.font = .systemFont(ofSize: 13)
caption.textColor = kind == "game" ? .systemRed : .secondaryLabelColor
caption.frame = NSRect(x: 0, y: 0, width: 540, height: 36)
accessory.addSubview(heading)
accessory.addSubview(caption)
panel.accessoryView = accessory
panel.isAccessoryViewDisclosed = true
let keyDelegate = KeySelectionDelegate()
if kind == "keys" { panel.delegate = keyDelegate }
app.activate(ignoringOtherApps: true)
let response = withExtendedLifetime(keyDelegate) { panel.runModal() }
if response == .OK, let url = panel.url { print(url.path) } else { exit(130) }
