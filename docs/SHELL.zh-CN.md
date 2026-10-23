# 无游戏空壳补包

Release 中的 IPA 是无游戏空壳，不能直接游玩，仅导入 `.data` 不够。

下载对应版本的源码，使用自己持有并按本版官方核心准备好的 3.0.3 改写程序、完整 iOS 着色器缓存和字体。原构建参数不变；增加 `--shell` 可使用 GitHub 已编译的空壳补包：

```sh
./setup.sh build \
  --shell /path/to/ACNHMobile-iOS26-v0.3.0-build2-shell-unsigned.ipa \
  --patched /path/to/owned/patched \
  --romfs /path/to/owned/RomFS \
  --shaders /path/to/owned/shaders/ios \
  --font /path/to/owned/font.ttf \
  --work /path/to/private/work \
  --output /path/to/private/ACNHMobile.ipa
```

需要 Python 和 FontTools。补包时核对源码指纹，生成包含自备程序的 Guest.framework；请对整个 IPA（包括此框架）签名后安装。不传 `--shell` 时仍按原方式在 Mac 上编译完整私人 IPA。

着色器必须与官方核心配套，本版基线目录为 `b-87eb0f1ee74a`，包含完整反射 JSON 和派生变体，不能混用旧版着色器补丁。已有 3.0.3 `.data` 可以继续用；将索引和全部编号分卷放在 `Documents/ACNHMobile`，直接读取、不解压。

空壳不含游戏程序、游戏资源、转换后的游戏着色器、key、存档和字体；图标保留。补包后的完整 IPA 含用户自备内容，只供私人使用，请勿上传公开。

本机已完成空壳构建、内容审计和自备文件补包验证；手机签名、安装和完整游戏流程仍需用户真机测试。
