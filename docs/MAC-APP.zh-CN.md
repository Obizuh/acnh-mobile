# 制作可以双击启动的 Mac 版

从 v0.3.39 开始，本项目接入 NXNative 官方的 Mac 应用启动器和打包器。生成 `ACNH Mobile.app` 后，可以移动到「应用程序」或其他文件夹，双击启动，不再依赖构建目录中的启动脚本。

## 准备

- Apple 芯片 Mac；不支持 Intel Mac。渲染仍使用 Metal 4。
- 完整 Xcode 和 Metal 编译工具。第一次打开 Xcode，完成许可与组件安装。
- Python 3.12 或更新版本，推荐 3.14。
- 自备动森本体、3.0.3 更新及自己的 `prod.keys`。游戏目录只放这一个游戏及所需更新。
- 一份可用的字体文件。

应用的最低 macOS 版本从实际编译出的主程序读取，不会强行写低。本机使用当前 Xcode 构建的验证包要求 macOS 27.0；其他构建环境应查看产物 `Contents/Info.plist` 中的 `LSMinimumSystemVersion`，不能据此保证旧系统可运行。

## 双击一键制作（推荐）

每次双击项目根目录的 `一键生成Mac应用.command`，依次选择放有本体和 3.0.3 更新的游戏文件夹，以及 `prod.keys`。脚本自动检查并补齐可安装的依赖、准备 Mac 着色器、编译和打包。完整 Xcode 仍需事先安装并完成许可。

本机也可打开项目的「一键工具」文件夹，双击同名入口。双击时会让你选择保存位置，生成该目录下的 `ACNH Mobile.app`。未启用交互选择的命令行构建则使用配置或默认输出目录。成功后终端显示完整路径，双击该应用即可游玩。再次生成会替换同名应用，游戏存档独立保存。

成功后清理本次 Mac 编译临时目录及自动生成的准备缓存，保留日志。失败时保留现场。这个入口不生成 IPA，不需要 iOS 的 `.data`，也不会自动发布。

需要重新选择或只检查准备情况时：

```bash
./一键生成Mac应用.command --select
./一键生成Mac应用.command --plan --game-dir "/path/to/game" --keys "/path/to/prod.keys"
```

## 方法一：从游戏文件直接制作

在项目根目录打开终端，执行：

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt

./setup.sh mac-app \
  --game "/path/to/owned/ACNH-NSPs" \
  --keys "/path/to/owned/prod.keys" \
  --font "/path/to/owned/font.ttf" \
  --work "/path/to/private/mac-work" \
  --output "/path/to/private/ACNH Mobile.app"
```

替换上述路径，包含空格的路径保留双引号。当前支持的游戏更新为 3.0.3，与 iOS 制作流程一致。

脚本检查完整 Xcode，并自动安装可安装的构建依赖，然后完成提取、程序改写、官方 Mac 着色器转换和 `.app` 打包。首次处理较慢；一键入口成功后自动清理本次准备缓存，下一次会重新准备。游戏文件、keys 和生成的完整应用都不上传 GitHub。

`--work` 放私人构建缓存；`--output` 是最后生成的 `.app` 路径。建议二者与游戏目录在同一个 APFS 磁盘，支持时会使用克隆文件减少复制。跨磁盘或不支持克隆时会普通复制，并检查可用空间。克隆不是符号链接，应用生成后可以独立移动。

## 方法二：复用已经准备好的文件

已有改写程序、RomFS 和同一着色器链的 **Mac 着色器** 时：

```bash
./setup.sh mac-app \
  --patched "/path/to/owned/patched" \
  --romfs "/path/to/owned/RomFS" \
  --shaders "/path/to/owned/shaders/mac" \
  --font "/path/to/owned/font.ttf" \
  --work "/path/to/private/mac-app-work" \
  --output "/path/to/private/ACNH Mobile.app"
```

`--shaders` 指向包含匹配 `b-<chain>` 的 Mac 缓存父目录，不能拿 iOS 的 `.metallib` 替代。不要同时传方法一和方法二的输入参数。

## 安装和启动

1. 等终端显示 `Mac app: .../ACNH Mobile.app`。
2. 将应用移动到需要的位置后双击打开。首次读取外置磁盘时，按系统提示允许文件访问。
3. 默认使用本地临时签名；如确实有自己的签名身份，可追加 `--sign "Developer ID Application: 姓名 (团队编号)"`。它不启用 hardened runtime，也不代表已经公证。

macOS 若阻止打开，应先确认应用来源和构建过程，再通过系统提供的「仍要打开」操作处理。不要为了运行应用关闭全局安全设置。

## 操作

| 键盘 | 对应功能 |
|---|---|
| W / A / S / D | 左摇杆移动 |
| J / K / U / I | A / B / X / Y |
| Q / E | L / R |
| Z / C | ZL / ZR |
| 方向键 | 十字键 |
| Return / Tab | ＋ / － |
| F / G | 左／右摇杆按下 |
| Control + Command + F | 全屏切换 |
| Command + Q | 退出 |

也可以连接系统支持的手柄。Mac 应用使用原生 Mac 游戏窗口，不会显示 iPhone 的触屏按键或 iOS 启动设置页。

## 存档、设置和日志在哪里

在 Finder 按 `Command + Shift + G`，输入：

```text
~/Library/Application Support/nxnative/01006F8002326000/
```

这里有 `save`、运行中的事务工作存档、`temp` 和 `pso_user.tsv`。游戏完全退出后再备份或导入 `save`；首次打开现有存档前，先保留原始备份。覆盖 `.app` 不会覆盖这个目录。

日志在：

```text
~/Library/Logs/nxnative/01006F8002326000.log
~/Library/Logs/nxnative/01006F8002326000.prev.log
```

Mac 使用官方目录规则，与 iOS 的 `Documents/logs` 不同。

需要启用帧缓冲时，在上述 Application Support 目录新建 `env.txt`：

```text
DM_PRESENT_SNAP=1
```

关闭时改为 `0`，完全退出并重新启动。默认关闭；开启可能减少卡顿，也会增加显示延迟，具体效果需要按设备测试。`--env-file "/path/to/env.txt"` 可以把设置放进应用内，本地 Application Support 的设置优先。

## 和 iOS `.data` 的区别

本次 Mac 打包沿用官方完整应用布局，应用内包含 RomFS、改写程序和 Mac 着色器，不直接以 iOS 的 `.data` 分卷作为制作输入。原有 iOS `.data` 格式和使用方法不变，也不需要重新制作。

只需原来的 Mac 开发启动脚本时，仍可使用 `./setup.sh mac ...`，它与新的 `mac-app` 命令并存。

双击入口现在依次选择：游戏目录 → prod.keys → 生成结果的保存文件夹。可在最后一个窗口新建文件夹。IPA 与数据包（或 Mac 应用）生成在所选目录中，构建缓存仍独立保存。命令行可使用 `--output-dir "/path/to/output"` 指定保存位置。取消任一步都不会开始构建。

### 成功后自动清理缓存

完整 IPA（以及所选的数据包）或 Mac 应用生成、校验成功后，自动删除本次编译目录和本次自动生成的提取、程序改写、着色器转换缓存。保留成品、日志、用户原始游戏、keys 和已安装依赖；手工提供的预处理输入不会删除。失败时不清理，便于排查。下次制作会重新处理，因此耗时增加。

每次运行仍自动检测并补齐可安装的依赖；完整 Xcode 和许可需用户先完成。
