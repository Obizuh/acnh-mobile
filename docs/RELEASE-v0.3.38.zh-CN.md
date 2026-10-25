# v0.3.38 · 构建 40

- 同步官方内存查询、映射预算及栈借用状态一致性修复，保护仍在使用的相邻内存，内存权限变更失败时准确报告。
- 同步预编译预测后的裁剪区域恢复修复，避免后续绘制使用错误的裁剪状态。
- 同步大几何捕获拆分和并行索引压缩修复；临时缓冲选择合适尺寸并回收长期闲置的大块。
- 同步着色器资源绑定数量边界修复；Mac 窗口失焦时释放自身输入，恢复焦点时重新读取手柄。
- 包含此前已在本地验证的完整附件清屏、单采样 Downsample、震动引擎恢复和卡顿诊断安全修复。
- 首次导入前创建资源目录；诊断文件统一放入 `logs`，兼容迁移旧文件。保留控制器、系统输入法、相册及原有 `.data` 使用方式。

官方来源：[内存保护](https://github.com/seedds/nxnative/commit/9ba44da80a94)、[内存一致性](https://github.com/seedds/nxnative/commit/cae5ec215568)、[几何与绘制状态](https://github.com/seedds/nxnative/commit/16f4889192dd)、[Mac 输入焦点](https://github.com/seedds/nxnative/commit/787523f2cce6)、[按需加载 LZ4](https://github.com/seedds/nxnative/commit/b7dfba4dd0b2)。只选择适用的既有功能修复。

验证：宿主回归及内存 ASan、UBSan、ThreadSanitizer、脏页并发测试通过。实际 Metal 验证模式下几何 95 项、管线 52 项、alpha 渲染 1165 项通过；构建工具 53 项和日志迁移回归通过。完整私人 IPA 和无游戏空壳分别编译、校验与审计。尚未进行手机长期实测。
