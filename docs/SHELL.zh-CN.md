# 不含游戏的空壳发布包

`ACNHMobile-shell-build-input.zip` 只有 iOS 运行框架，不含游戏程序、游戏着色器、资源、密钥或存档，不能直接玩。它使用 ZIP 扩展名防止误当作安装包。

仅把扩展名改成 IPA、仅向手机导入 `.data`，都不能补齐游戏程序。完整 IPA 需要用户在电脑上用自己的文件生成，并签名安装。

## 在 GitHub 生成空壳

1. 把本版本源码完整提交到仓库，包括 `.github/workflows/shell-build.yml`。
2. 打开 **Actions → Build game-free iOS shell → Run workflow**，选择 `main` 后运行。
3. 完成后在该次运行底部 **Artifacts** 下载 `ACNHMobile-shell-build-input`。解开外层下载 ZIP，里面才是空壳 ZIP、内容检查报告与 SHA256SUMS。
4. 如需 Releases：给对应源码提交创建 `v` 开头的标签，例如 `v1.0.0`。流程会生成同标签的**草稿 Release**并附上空壳；核对后手动发布。普通手动运行只生成 Artifact。

流程使用 GitHub 的 `macos-26` 机器，只安装 XcodeGen，不接收游戏文件，不需要 Apple 证书、密钥或其他 Secrets。它不会在 GitHub 上构建含游戏的完整 IPA。

## 用户在自己电脑上生成 IPA

当前准备游戏程序和着色器仍需要 Apple Silicon Mac、Xcode 26+、Metal 工具与 [BUILD.zh-CN.md](BUILD.zh-CN.md) 中的依赖。尚未提供 Windows/Linux 一键准备全部游戏产物的功能。

必须使用与空壳匹配的源码版本。在自己的电脑上准备合法持有的解密 ExeFS、RomFS 和可使用的标准 TTF 字体，然后：

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r requirements.txt
./setup.sh rewrite --exefs /path/to/ExeFS --output /path/to/private/patched
./setup.sh shaders --exefs /path/to/ExeFS --romfs /path/to/RomFS --output /path/to/private/shaders
python tools/pack_shell.py \
  --shell /path/to/ACNHMobile-shell-build-input.zip \
  --patched /path/to/private/patched \
  --shaders /path/to/private/shaders/ios \
  --font /path/to/standard.ttf \
  --output /path/to/private/ACNHMobile-unsigned.ipa
```

最后用侧载工具签名**整个 IPA，包括 Guest.framework**，再安装到 iPhone/iPad。此私人 IPA 已含游戏程序和转换后的着色器，不能作为空壳公开上传。

`.data` 资源仍外置，按 [GAME_FILES.zh-CN.md](GAME_FILES.zh-CN.md) 制作后放到 App 的 `Documents/ACNHMobile`，运行时直接读取，不解压。原有完整数据包可继续使用。

## 在 Mac 本地生成公开空壳

```sh
python tools/build_shell.py --work /path/to/new-empty-work --output /path/to/shell-output
```

无需任何游戏文件。构建器严格检查空壳的文件清单、Bundle ID 和可执行文件，拒绝残留的游戏模块内嵌段。公开源码和空壳可发布；用于核对的私人 IPA、程序、着色器、字体、数据与存档不能发布。

本版本已完成本地 iOS 编译、空壳内容检查和私人组包结构核对；空壳方式尚待手机签名安装和实际游玩验证。GitHub 托管构建需上传源码后查看首次运行结果。
