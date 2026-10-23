## ACNH Mobile v0.3.6 · 构建 8

- 修复道路选择菜单中泥土路预览偏蓝的纹理通道映射问题。合入 NXNative a08bbdd：RGB565 类上传纹理正确组合格式和视图通道，Metal 已渲染纹理保留自身通道顺序，同时补齐纹理层级边界及可写视图处理。上游作者在 Issue #69 确认此为道路图标渐变颜色异常的原因。
- 新增 iOS 启动设置页：运行模式、游戏语言、帧率显示、性能面板；设置在点击开始游戏后保存。
- 新增「通用 / 专属」控制器选择。默认专属使用原动森图标、工具环和 R 系统输入；通用采用 NXNative 官方 2D/3D 触屏布局。通用的 R 是普通手柄按键，不含专属输入辅助。
- 合入官方音频恢复修复，避免从后台返回后误判并重复重启刚恢复的音频。

保留此前官方 Metal 编码线程和 uniform 优化、图标、事务存档、amiibo 导入、相册导出及震动反馈。原 3.0.3 `.data` 与配套着色器继续使用，无需重做数据包。

### 验证

iPhone/iPad Release 编译与私人 IPA 完整性、版本和双控制器检查通过；Metal API Validation 下纹理颜色通道 203 项、纹理层级 34 项检查通过，启动设置检查 3 项通过。音频后台检查通过。额外格式视图测试 t_pfv 有 1 项失败，修复前对照同样失败。道路菜单颜色、启动页和控制器选择仍待真机复测，不把 GPU 单元测试视为完整游戏验证。

发布附件仅提供 GitHub Actions 构建的未签名无游戏空壳 IPA、审计报告和 SHA256，不含游戏程序、数据、keys、存档、字体或转换后的游戏着色器。空壳需要使用自备文件制作完整 IPA 后签名，不能仅导入 `.data` 直接游玩。

来源：[纹理修复](https://github.com/seedds/nxnative/commit/a08bbdd3ba14)、[道路颜色问题确认](https://github.com/seedds/nxnative/issues/69#issuecomment-6358362841)、[启动页](https://github.com/seedds/nxnative/commit/2f69480260068c0979ee42a1c31b420eecfd27e2)、[音频恢复](https://github.com/seedds/nxnative/commit/fe23c103075711f090760cd25bfe6c088ede7274)。
