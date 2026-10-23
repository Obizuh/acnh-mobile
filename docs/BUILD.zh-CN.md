# 本地构建

只支持配置中列出的 ACNH 3.0.3。输入必须是使用者自己准备的解密后文件；本项目不提供游戏解包所需的密钥。下列 `/path/to/...` 均须替换为自己的路径。

## 1. 检查依赖

按首页创建 Python 环境并执行 `./setup.sh check`。`.NET 10` 负责从第三方源码编译着色器翻译器；首次还原它的公开 NuGet 依赖需要网络。Xcode 的 Metal 编译组件必须可用。需要 `lz4`、`capstone`、`fontTools` 三个 Python 包。构建过程不获取游戏文件。

## 2. 改写自有程序

```sh
./setup.sh rewrite --exefs /path/to/ExeFS --output /path/to/private-work/patched
```

ExeFS 必须包含 `main`、`subsdk0`、`sdk` 三个 NSO0 文件。脚本检查 main 的构建标识，再执行改写。产物中包含原始游戏代码，只留在私人工作区。

## 3. 从自有文件生成着色器

```sh
./setup.sh shaders --exefs /path/to/ExeFS --romfs /path/to/RomFS \
  --output /path/to/private-work/shaders
```

这一步会扫描游戏、生成着色器包、编译翻译工具、记录本机工具指纹、更新配置中的链版本，再翻译和编译所有着色器。最后应用顶点位置一致性和三个地图图层的透明度修复。耗时取决于机器及工具版本。

生成结果分别在 `shaders/mac/b-<chain>` 与 `shaders/ios/b-<chain>`。不要把其中的 `.metal`、`.metallib`、原始 shader 包上传。工具版本改变会生成新的链标识；不要混用旧链的库或旧的整合 IPA。

## 4. 制作仅供本人使用的 IPA

```sh
./setup.sh build --patched /path/to/private-work/patched \
  --romfs /path/to/RomFS --shaders /path/to/private-work/shaders/ios \
  --font /path/to/your-standard-font.ttf \
  --work /path/to/private-work/app --output /path/to/private-work/ACNHMobile.ipa
```

标准字体由使用者自行提供；可使用自己有权在本机使用的字体。生成的 App 不复制 RomFS，而是读取另外导入的 `.data`。但 IPA **仍内嵌游戏程序和转换着色器**，所以不是可随源码发布的“纯空壳”。未签名 IPA 需要使用自己的安装/签名方式。

## 5. 制作 .data

见 [GAME_FILES.zh-CN.md](GAME_FILES.zh-CN.md)。文件放在手机应用 Documents 下的 `ACNHMobile` 文件夹，启动时直接读取，无需展开。

## 发布源码

```sh
python3 tools/audit_release.py .
```

只发布本次提供的 `acnh-mobile-source-*.zip`，或在干净源码树中运行检查后打包。`Gen`、构建输出、`.data`、IPA、存档和密钥均必须留在发布包之外。源码检查是白名单与敏感格式检查，不声称完成全流程游戏兼容性验证。

从 v0.3.8 起合入官方计算式几何着色器路径。锁定官方工具的链标识为 `ade9bce36598`，旧版着色器缓存需要重新生成并随完整 IPA 打包；原 `.data` 游戏数据无需重新生成。本机重新构建翻译器并更新私人锁时，以其生成的链和配套宿主为准，勿混用。
