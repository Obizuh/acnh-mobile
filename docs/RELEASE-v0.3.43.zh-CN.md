# v0.3.43 · 构建 45

- 适配Duo测试：横排启动页、完整等比游戏画面、专属按键内收，并支持避让铰链预留区域。
- 改进存档文件协调替换，保留专属控制器截图键及原有照片保存流程。
- 同步 NXNative 官方深度清屏、打包纹理布局及显示分配器回收修复。
- 同步官方音频工作缓冲区尺寸和震动设备能力识别修复。
- GitHub 构建使用 Xcode 27.1，并校验空壳包含 Duo 适配代码；最低系统要求仍为 iOS 26。

已完成本机编译、针对性回归及安装包内容校验。Duo 真机折叠、切屏和触摸效果仍需测试验证；编译通过不代表真机验证。

公开附件仅为不含游戏、数据包、转换后游戏着色器、keys、存档和字体的空壳 IPA、审计报告及 SHA256 校验文件。现有 3.0.3 `.data` 分卷和用户构建流程保持兼容。

官方修复来源：[深度清屏](https://github.com/seedds/nxnative/commit/a5513d4ae08a)、[纹理布局](https://github.com/seedds/nxnative/commit/4896c78d8ab3)、[显示分配器](https://github.com/seedds/nxnative/commit/82ab25b40b75)、[音频缓冲](https://github.com/seedds/nxnative/commit/435a2310511b)、[震动识别](https://github.com/seedds/nxnative/commit/2db12f7efdf2)。
