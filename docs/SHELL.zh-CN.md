# 不含游戏的空壳发布包

公开附件是未签名、无游戏的空壳 IPA：`ACNHMobile-iOS26` 最低 iOS26；`ACNHMobile-iOS18-compat` 最低 iOS18，使用Metal3。两版均不含游戏程序、转换着色器、运行资源、密钥或存档。签名空壳、仅向手机导入 `.data` 都不能补齐游戏程序；完整游戏 IPA 仍需使用者在本地用自己持有的文件生成。

## 在 GitHub 生成空壳

1. 提交审计通过的公开源码，先准备好 `ios18-compat` 分支，并使两版版本与构建号一致。
2. 打开 **Actions → Build game-free iOS shell → Run workflow**。两个标签字段留空只生成Actions附件。
3. 下载 `ACNHMobile-dual-shell-v<版本>-build<构建>`，解开外层ZIP后得到两版空壳IPA、审计报告与SHA256SUMS。
4. 版本标签 `v*` 或手动输入新标签会在两版均构建及核验成功后生成一个正式Release，只附两版IPA和共同SHA256SUMS。本次main首次推入0.1.21/构建34也会自动发布；已有标签/Release不会自动替换。

流程在GitHub macos-26上安装公开构建依赖，严格核对版本、系统下限、源码指纹、ZIP和无游戏文件白名单。上传为草稿并重新下载核对哈希后才正式发布。不接收游戏、密钥或Apple签名证书；不因手动创建Release事件启动另一个争抢附件的流程。

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
  --shell /path/to/ACNHMobile-iOS26-v0.1.21-build34-shell-unsigned.ipa \
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

必须选择与空壳对应的主线或兼容源码分支，混用源码指纹会被拒绝。本地build_shell.py仍输出ACNHMobile-shell-build-input.zip，也可传给同一组装命令。
