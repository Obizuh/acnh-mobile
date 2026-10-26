# 本地一键生成

需要 Apple Silicon Mac、完整 Xcode，以及你自己的动森本体和 **3.0.3 更新 NSP**、`prod.keys`。目录中只放一个更新版本。项目不提供、下载或上传游戏与密钥。

双击项目根目录的入口，每次都会弹窗选择自己的游戏目录和 `prod.keys`，不会沿用私人配置中的旧输入路径。命令行传参时仍按所给参数运行：

| 入口 | 输出 |
| --- | --- |
| 一键生成Mac应用.command | 可独立双击运行的 Mac `.app`，包含所需游戏资源和 Mac 着色器；详见 [Mac 制作说明](MAC-APP.zh-CN.md) |
| 一键生成完整IPA.command | 完整未签名 IPA：含自备程序、官方转换后的着色器、字体和项目图标；需要另外配合 `.data` |
| 一键生成IPA和数据包.command | 同样的完整 IPA，另生成 `ACNHMobile` 文件夹中的 `.data` 分卷和索引 |

第一次会提取本体和更新、校验程序 build ID、改写程序，并运行官方 Metal 着色器转换。转换会同时生成 Mac 和 iOS 缓存，以及匹配的着色器链、c1 和 uniform 表，然后用这些文件编译 IPA。首次可能耗时较长，建议预留至少 30 GB 空间。原游戏、keys 和存档不会修改。

转换和编译的私人源码副本在 `work/local-build/prepared/`，不会改写 Git 中的官方着色器配置。制作成功后清理本次缓存；失败时保留，重试可复用仍有效的准备结果。输入文件或相关源码发生变化时建立新缓存，不混用旧核心和新着色器。文件名不能代替版本检查，实际提取后还会核实三个程序模块的 build ID。

## 自动补齐依赖

脚本会自动安装缺少的 XcodeGen、spirv-cross、zstd、Python 3.12+、.NET 10 和 Python 依赖，无需逐项确认。Python 依赖安装到项目 `.venv`；.NET 缺失时使用微软官方安装脚本安装到用户 `.dotnet`。Homebrew 缺失时尝试其官方非交互安装脚本；需要管理员权限而无法自动完成时会停止并提示，不代填密码。缺少 Metal 编译组件时调用 Xcode 下载。

完整 Xcode 需要用户自行安装、打开并完成许可确认。依赖下载需要网络；失败时保留日志和已有输出，可解决网络或权限问题后重试。依赖安装发生在用户的 Mac，GitHub 发布仍只提供源码与空壳。

## 已有准备好的输入

已有私人配置时继续使用原来的预处理输入，不强制重新提取。想重新选择游戏和 keys，可运行：

```sh
./一键生成完整IPA.command --select
./一键生成IPA和数据包.command --select
```

也可以指定路径：

```sh
./一键生成IPA和数据包.command --game-dir '/path/to/game' --keys '/path/to/prod.keys'
```

配置模板 `config/local-build.example.json` 复制成根目录 `local-build.local.json`。该文件不提交、不放入空壳：

- `input_mode` 为 `raw` 时使用 NSP 流程；`game_dir` 和 `keys` 可指定路径，否则打开选择窗口。
- 预处理模式使用 `data_root`，其下默认寻找 `01006F8002326000_build/pristine_upstream/patched`、`extract/3.0.3/RomFS`、`upstream_shader/ios`；也可以分别指定 `patched`、`romfs`、`shaders`。
- `font` 可选；默认使用系统 Arial Unicode 字体。
- `python` 可指定既有 Python 环境。
- `output_root`、`work_root` 默认为项目 `outputs/`、`work/local-build/`，建议放同一磁盘，且不要把工作目录放在游戏目录内。
- `--plan` 仅显示依赖和输入检查结果，不安装、不构建；原始输入模式请同时提供 `--game-dir` 和 `--keys`，避免打开选择窗口。

## 输出和使用

版本和构建号读取 `config/version.json`。IPA 完整性、应用标识、系统下限、版本和着色器校验通过后才输出。失败不清理旧包。

完整 IPA **不嵌入 RomFS**。用户需自行整包签名；现有完整的 `.data` 可以继续用，放到应用 `Documents/ACNHMobile`，或启动时选择对应文件夹。IPA 加数据包模式会生成直接读取的 `.data`，手机不用解压。

数据先在临时目录生成，逐文件验证 SHA256 后才输出。重复运行会校验并复用来源相同的数据；提取位置变化时，逐文件核对内容，完全一致仍可复用。内容不一致时停止，不覆盖已有数据。输出只含 `.data` 与隐藏的来源记录，不需要 JSON 清单。

成功后输出目录仅保留最新与一个旧 IPA，同版本同类型的重复包移入废纸篓。不清理游戏、keys、数据包或存档。每次有独立日志，同一个工作目录同时只运行一个构建。

完整 IPA 和数据包只供私人使用。可公开发布的是源码和无游戏空壳；脚本不会自动上传、推送、签名或安装。原有 `./setup.sh build --patched … --romfs … --shaders … --font … --work … --output …` 接口保持不变。

双击入口现在依次选择：游戏目录 → prod.keys → 生成结果的保存文件夹。可在最后一个窗口新建文件夹。IPA 与数据包（或 Mac 应用）生成在所选目录中，构建缓存仍独立保存。命令行可使用 `--output-dir "/path/to/output"` 指定保存位置。取消任一步都不会开始构建。

### 成功后自动清理缓存

完整 IPA（以及所选的数据包）或 Mac 应用生成、校验成功后，自动删除本次编译目录和本次自动生成的提取、程序改写、着色器转换缓存。保留成品、日志、用户原始游戏、keys 和已安装依赖；手工提供的预处理输入不会删除。失败时不清理，便于排查。下次制作会重新处理，因此耗时增加。

每次运行仍自动检测并补齐可安装的依赖；完整 Xcode 和许可需用户先完成。
