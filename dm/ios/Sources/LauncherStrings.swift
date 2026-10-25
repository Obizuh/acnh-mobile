// Launcher language follows the device/app language, independently of DM_LANG.
import Foundation

enum LauncherText {
    static func text(_ english: String, _ simplified: String, language: String = Locale.preferredLanguages.first ?? "en") -> String {
        guard language.lowercased().hasPrefix("zh") else { return english }
        let code = language.lowercased()
        let traditional = code.contains("hant") || (!code.contains("hans") && ["tw", "hk", "mo"].contains(code.split(separator: "-").last.map(String.init) ?? ""))
        return traditional ? simplified.applyingTransform(StringTransform("Hans-Hant"), reverse: false) ?? simplified : simplified
    }
    static func note(env: String, fallback: String, language: String = Locale.preferredLanguages.first ?? "en") -> String {
        if language.lowercased().hasPrefix("ja"), !fallback.isEmpty {
            let japanese = [
                "DM_PRESENT_SNAP": "カクつきを軽減する場合がありますが、表示の遅延が増えます。",
                "DM_RUMBLE": "ゲームからの振動をスマートフォンや接続したコントローラーで再生します。",
                "DM_RUMBLE_GAIN": "ゲーム内の振動設定と組み合わせて強さを調整します。",
                "DM_TOUCH_HAPTICS": "タッチ操作のボタンやスティックを押したときに軽く振動します。",
                "DM_OPMODE": "携帯モードは低い解像度で描画するため、通常は GPU の負荷と消費電力を抑えられます。一部のゲームは携帯モードでのみタッチ操作に対応します。",
                "DM_METAL_HUD": "ゲーム画面に Apple Metal のパフォーマンス情報を表示します：フレームレート、フレーム時間、GPU 時間、メモリ使用量。",
                "DM_FPS_TARGET": "30 fps にすると消費電力と発熱を抑えられます。",
                "DM_RENDER_SCALE": "100% 未満では、3D 画面を選択した倍率で描画し、ゲームの動的解像度を無効にします。UI の文字は鮮明なままです。"
            ]
            return japanese[env] ?? fallback
        }
        let notes = [
            "DM_PRESENT_SNAP": "可能减少卡顿，但会增加显示延迟。",
            "DM_RUMBLE": "通过手机或已连接的手柄呈现游戏内震动。",
            "DM_RUMBLE_GAIN": "配合游戏内的震动设置，调整游戏震动强度。",
            "DM_TOUCH_HAPTICS": "按下触屏按键或开始触摸摇杆时提供轻微震动反馈。",
            "DM_OPMODE": "掌机模式以较低分辨率渲染，通常能减轻 GPU 负担并节省电量；部分游戏仅在掌机模式下读取触屏操作。",
            "DM_METAL_HUD": "在游戏画面上显示 Apple Metal 性能面板：帧率、每帧耗时、GPU 耗时及内存。",
            "DM_FPS_TARGET": "30 帧可节省电量并减少发热。",
            "DM_RENDER_SCALE": "低于 100% 时，3D 画面按所选比例渲染并关闭游戏动态分辨率，界面文字保持清晰。"
        ]
        guard let chinese = notes[env], !fallback.isEmpty else { return fallback }
        return text(fallback, chinese, language: language)
    }
}
