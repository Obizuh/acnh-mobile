# v0.3.26 · 构建 28

- 同步官方 GPU 安全修复：图像原子操作使用足够大的临时缓冲区，变换反馈绘制前结束复制编码器，避免越界与编码器冲突。
- 同步 NVN 着色器代码版本范围按 SDK 调整，以及窗口裁剪和采样器 LOD 查询接口。
- 同步 iOS NRO 共享分段嵌入和加载表支持，保留动森的完整包与空壳构建流程。
- 保留个性化折叠、原生设置样式、控制器、相册导出和输入法，现有 .data 继续使用。

来源：[GPU 安全](https://github.com/seedds/nxnative/commit/066f2afc76b0)、[iOS NRO](https://github.com/seedds/nxnative/commit/ff6435288591)、[NVN 查询](https://github.com/seedds/nxnative/commit/6034fcad8981)。

验证：NVN 100 项、几何 95 项、uniform 47 项、顶点缓冲 43 项、既有透明度 1165 项回归通过；NRO 分段打包及加载、启动设置和打包工具测试通过。iOS IPA 与空壳编译、内容及 SHA256 校验通过。
