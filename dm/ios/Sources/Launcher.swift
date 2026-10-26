// Launcher: the settings screen shown before the game starts (front_ios.m dm_show_launcher). Its settings come from the app's dm/launcher.json, which build_ipa.sh writes for this game
// from dm/games/settings.tsv (dm/games/settings.py): each sets an environment variable the host reads when the game starts. A variable an env file set (Documents/env.txt, the bundle's
// dm/env.txt; main.m lists them in DM_ENV_FILE_KEYS) is shown but kept. The choices are kept in UserDefaults. DM_LAUNCHER=0 in Documents/env.txt starts the game at once, as an app
// without launcher.json does
import SwiftUI
import UIKit
import os

@_silgen_name("dm_edit_pad_layout")
private func dmEditPadLayout(_ parent: UnsafeMutableRawPointer, _ base: UnsafePointer<CChar>)

private struct Setting: Decodable, Identifiable {
    let env, kind, label, note, def: String
    let options: [[String]]?   // choice: [value, label]
    let min, max, step: Double?   // range
    var id: String { env }
    enum CodingKeys: String, CodingKey { case env, kind, label, note, def = "default", options, min, max, step }
}
private struct SettingsFile: Decodable { let settings: [Setting] }

private func info(_ key: String) -> Any? { Bundle.main.object(forInfoDictionaryKey: key) }
private let defaults = UserDefaults.standard
private func localized(_ english: String, _ chinese: String) -> String {
    LauncherText.text(english, chinese)
}
private func label(_ s: Setting) -> String {
    let names = ["DM_PRESENT_SNAP": "帧缓冲", "DM_RUMBLE": "游戏震动", "DM_RUMBLE_GAIN": "震动强度", "DM_TOUCH_HAPTICS": "触屏反馈", "ACNH_CUSTOM_BASE": "调整对象", "ACNH_AUDIO_IGNORE_SILENT": "静音模式下播放声音", "ACNH_CONTROLLER_LAYOUT": "控制器布局", "DM_FPS_TARGET": "帧率", "DM_RENDER_SCALE": "渲染比例", "DM_OPMODE": "运行模式", "DM_LANG": "游戏语言", "DM_SHOW_FPS": "显示帧率", "DM_METAL_HUD": "性能信息面板"]
    return localized(s.label, names[s.env] ?? s.label)
}
private func optionLabel(_ s: Setting, _ option: [String]) -> String {
    if s.env == "ACNH_CONTROLLER_LAYOUT" || s.env == "ACNH_CUSTOM_BASE" { return localized(option[1], option[0] == "custom" ? "自定义" : option[0] == "generic" ? "通用" : "专属") }
    if s.env == "DM_OPMODE" { return localized(option[1], option[0] == "1" ? "主机模式" : "掌机模式") }
    if s.env == "DM_LANG" {
        return Locale(identifier: Locale.preferredLanguages.first ?? "en").localizedString(forIdentifier: option[0]) ?? option[1]
    }
    return option[1]
}

private final class LauncherModel: ObservableObject {
    let settings: [Setting]
    let fromFile: Set<String>   // set by an env file: shown, not changed
    @Published var values: [String: String] = [:]

    init(_ settings: [Setting]) {
        self.settings = settings
        fromFile = Set((ProcessInfo.processInfo.environment["DM_ENV_FILE_KEYS"] ?? "").split(separator: " ").map(String.init))
        for s in settings {
            if fromFile.contains(s.env), let v = ProcessInfo.processInfo.environment[s.env] { values[s.env] = v; continue }
            let saved = defaults.string(forKey: "launcher." + s.env)
            values[s.env] = saved.flatMap { v in s.options == nil || s.options!.contains { $0[0] == v } ? v : nil } ?? s.def   // a saved choice the game no longer offers: its default
        }
    }

    func binding(_ s: Setting) -> Binding<String> { Binding(get: { self.values[s.env] ?? s.def }, set: { self.values[s.env] = $0 }) }

    // Before the game starts: every setting not set by an env file goes into the environment (DM_LANG replaces the packed language main.m set as a default)
    func apply() {
        for s in settings {
            let v = values[s.env] ?? s.def
            if fromFile.contains(s.env) { fputs("launcher: \(s.env) kept at \(v) (env file)\n", stderr); continue }
            defaults.set(v, forKey: "launcher." + s.env)
            setenv(s.env, v, 1); fputs("launcher: \(s.env)=\(v)\n", stderr)
        }
    }
}

private struct SettingRow: View {
    let s: Setting
    @ObservedObject var m: LauncherModel
    let edit: (String) -> Void

    private var locked: Bool { m.fromFile.contains(s.env) }
    private var note: String {
        let translated = LauncherText.note(env: s.env, fallback: s.note)
        return locked ? (translated.isEmpty ? "" : translated + " · ") + localized("Set by an env file", "由配置文件设置") : translated
    }
    private var rangeValue: Binding<Double> {
        Binding(get: { Double(m.values[s.env] ?? s.def) ?? s.max ?? 1 },
                set: { m.values[s.env] = String(format: "%.2f", $0) })
    }
    private var percentage: String { "\(Int((rangeValue.wrappedValue * 100).rounded())) %" }

    private var settingLabel: some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(label(s))
            if !note.isEmpty { Text(note).font(.footnote).foregroundStyle(.secondary) }
        }
    }

    var body: some View {
        Group {
            switch s.kind {
            case "choice":
                Picker(selection: m.binding(s)) {
                    ForEach(s.options ?? [], id: \.[0]) { Text(optionLabel(s, $0)).tag($0[0]) }
                } label: {
                    settingLabel
                }
                .pickerStyle(.navigationLink)
            case "range":
                NavigationLink {
                    Form {
                        Section {
                            HStack {
                                Text(label(s))
                                Spacer()
                                Text(percentage).monospacedDigit().foregroundStyle(.secondary)
                            }
                            Slider(value: rangeValue, in: (s.min ?? 0)...(s.max ?? 1), step: s.step ?? 0.05)
                                .accessibilityLabel(label(s))
                                .accessibilityValue(percentage)
                                .disabled(locked)
                        } footer: {
                            if !note.isEmpty { Text(note) }
                        }
                    }
                    .contentMargins(.horizontal, 12, for: .scrollContent)
                    .navigationTitle(label(s))
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar(.visible, for: .navigationBar)
                } label: {
                    HStack {
                        settingLabel
                        Spacer()
                        Text(percentage).monospacedDigit().foregroundStyle(.secondary)
                    }
                }
            default:
                Toggle(isOn: Binding(get: { (m.values[s.env] ?? s.def) == "1" }, set: { m.values[s.env] = $0 ? "1" : "0" })) {
                    settingLabel
                }
            }
            if s.env == "ACNH_CUSTOM_BASE" {
                Button(localized("Adjust positions and sizes", "调整位置与大小")) { edit(m.values[s.env] ?? "acnh") }
            }
        }
        .disabled(locked)
    }
}

private struct LauncherView: View {
    @ObservedObject var m: LauncherModel
    let edit: (String) -> Void
    let play: () -> Void
    @State private var memory = os_proc_available_memory()
    @State private var personalizationExpanded = false
    private let personalizationKeys = ["ACNH_CONTROLLER_LAYOUT", "ACNH_CUSTOM_BASE", "ACNH_AUDIO_IGNORE_SILENT", "DM_PRESENT_SNAP", "DM_TOUCH_HAPTICS", "DM_RUMBLE", "DM_RUMBLE_GAIN"]

    var body: some View {
        GeometryReader { geometry in
            HStack(spacing: 0) {
                VStack(spacing: 16) {
                    if let icon = appIcon() {
                        Image(uiImage: icon).resizable().frame(width: 96, height: 96)
                            .clipShape(RoundedRectangle(cornerRadius: 22)).accessibilityHidden(true)
                    }
                    Text((info("CFBundleDisplayName") as? String) ?? localized("Game", "游戏"))
                        .font(.title.weight(.bold)).multilineTextAlignment(.center)
                    Text("\(localized("Version", "版本")) \((info("CFBundleShortVersionString") as? String) ?? "") · \((info("CFBundleVersion") as? String) ?? "")")
                        .font(.callout).foregroundStyle(.secondary)
                    playButton
                }
                .padding(16)
                .frame(width: geometry.size.width / 3, height: geometry.size.height)
                NavigationStack {
                    List {
                        Section {
                            ForEach(m.settings.filter { !personalizationKeys.contains($0.env) }) { setting in
                                SettingRow(s: setting, m: m, edit: edit)
                            }
                            Button {
                                withAnimation(.easeInOut(duration: 0.2)) { personalizationExpanded.toggle() }
                            } label: {
                                HStack {
                                    Text(localized("Personalization", "个性化")).foregroundStyle(Color(uiColor: .label))
                                    Spacer()
                                    Image(systemName: personalizationExpanded ? "chevron.down" : "chevron.right")
                                        .foregroundStyle(Color(uiColor: .secondaryLabel)).accessibilityHidden(true)
                                }.contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                            .accessibilityValue(personalizationExpanded ? localized("Expanded", "已展开") : localized("Collapsed", "已折叠"))
                            if personalizationExpanded {
                                ForEach(personalizationKeys, id: \.self) { key in
                                    if let setting = m.settings.first(where: { $0.env == key }), key != "ACNH_CUSTOM_BASE" || m.values["ACNH_CONTROLLER_LAYOUT"] == "custom" {
                                        SettingRow(s: setting, m: m, edit: edit)
                                    }
                                }
                            }
                        }
                        Section {
                            HStack {
                                Text(localized("Memory available", "可用内存"))
                                Spacer()
                                Text(ByteCountFormatter.string(fromByteCount: Int64(memory), countStyle: .memory)).monospacedDigit().foregroundStyle(.secondary)
                            }
                        }
                    }
                    .listStyle(.insetGrouped)
                    .contentMargins(.horizontal, 12, for: .scrollContent)
                    .navigationBarTitleDisplayMode(.inline)
                    .toolbar(.hidden, for: .navigationBar)
                }
                .frame(width: geometry.size.width * 2 / 3)
            }
        }
        .preferredColorScheme(.dark)
        .task {
            while !Task.isCancelled {
                memory = os_proc_available_memory()
                try? await Task.sleep(for: .seconds(2))
            }
        }
    }

    @ViewBuilder var playButton: some View {
        let button = Button(action: play) { Text(localized("Play", "开始游戏")).font(.title3.weight(.semibold)).frame(maxWidth: 220) }.controlSize(.large)
        if #available(iOS 26.0, *) { button.buttonStyle(.glassProminent) } else { button.buttonStyle(.borderedProminent) }
    }

    private func appIcon() -> UIImage? {
        if let i = UIImage(named: "AppIcon") { return i }
        guard let icons = info("CFBundleIcons") as? [String: Any], let primary = icons["CFBundlePrimaryIcon"] as? [String: Any],
              let files = primary["CFBundleIconFiles"] as? [String], let last = files.last else { return nil }
        return UIImage(named: last)
    }
}

// Keep the settings covered while the initial system orientation transition settles.
// This does not disable SpringBoard's animation or change subsequent device rotation.
private final class LandscapeLauncherController: UIHostingController<LauncherView> {
    private var startupCover: UIView?
    private var settleLink: CADisplayLink?
    private var appearedAt: CFTimeInterval = 0
    private var stableSince: CFTimeInterval?
    private var transitioning = false
    private var revealed = false

    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { .landscape }
    override var preferredInterfaceOrientationForPresentation: UIInterfaceOrientation {
        let current = presentingViewController?.view.window?.windowScene?.interfaceOrientation
        return current?.isLandscape == true ? current! : .landscapeRight
    }
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        guard !revealed, startupCover == nil else { return }
        let cover = UIView(frame: view.bounds)
        cover.backgroundColor = .black
        cover.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        view.addSubview(cover)
        startupCover = cover
    }
    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        guard !revealed, settleLink == nil else { return }
        appearedAt = CACurrentMediaTime()
        let link = CADisplayLink(target: self, selector: #selector(checkStartupOrientation))
        settleLink = link
        link.add(to: .main, forMode: .common)
    }
    override func viewWillTransition(to size: CGSize, with coordinator: UIViewControllerTransitionCoordinator) {
        super.viewWillTransition(to: size, with: coordinator)
        guard !revealed else { return }
        transitioning = true
        stableSince = nil
        coordinator.animate(alongsideTransition: nil) { [weak self] _ in
            self?.transitioning = false
            self?.stableSince = nil
        }
    }
    @objc private func checkStartupOrientation() {
        let now = CACurrentMediaTime()
        // The inner display does not report orientation like the outer display.
        let landscape = view.bounds.width > view.bounds.height
        let ready = UIApplication.shared.applicationState == .active && landscape && !transitioning
        if ready { if stableSince == nil { stableSince = now } } else { stableSince = nil }
        let settled = stableSince.map { now - $0 >= 0.18 } ?? false
        // Bounded fallback: an unusual container lifecycle must not leave a permanent black screen.
        guard (settled && now - appearedAt >= 0.4) || now - appearedAt >= 2 else { return }
        revealed = true
        settleLink?.invalidate()
        settleLink = nil
        UIView.performWithoutAnimation { startupCover?.removeFromSuperview() }
        startupCover = nil
        fputs("launcher: startup cover removed (landscape=\(landscape), settled=\(settled))\n", stderr)
    }
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        settleLink?.invalidate()
        settleLink = nil
    }
}

// Shows the launcher over the root view controller and calls done once the settings are in the environment; false (done is not called) when there is no launcher
@_cdecl("dm_show_launcher")
public func dmShowLauncher(_ viewController: UnsafeMutableRawPointer, _ done: @convention(c) () -> Void) -> Bool {
    if ProcessInfo.processInfo.environment["DM_LAUNCHER"] == "0" { fputs("launcher: off (DM_LAUNCHER=0)\n", stderr); return false }
    guard let url = Bundle.main.resourceURL?.appendingPathComponent("dm/launcher.json"), let data = try? Data(contentsOf: url) else { return false }
    guard let file = try? JSONDecoder().decode(SettingsFile.self, from: data), !file.settings.isEmpty else { fputs("[RED] launcher: dm/launcher.json cannot be read; starting the game\n", stderr); return false }
    let parent = Unmanaged<UIViewController>.fromOpaque(viewController).takeUnretainedValue()
    let model = LauncherModel(file.settings)
    var host: UIViewController?
    let view = LauncherView(m: model, edit: { base in
        guard let h = host else { return }
        base.withCString { dmEditPadLayout(Unmanaged.passUnretained(h).toOpaque(), $0) }
    }) {
        guard let h = host else { return }
        host = nil
        model.apply()
        h.dismiss(animated: false) { done() }
    }
    let h = LandscapeLauncherController(rootView: view)
    h.modalPresentationStyle = .fullScreen
    host = h
    parent.present(h, animated: false)
    fputs("launcher: \(file.settings.count) settings shown\n", stderr)
    return true
}
