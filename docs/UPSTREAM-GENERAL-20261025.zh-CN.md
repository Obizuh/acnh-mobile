# 2026-10-25 通用修复集成

版本：0.3.35，构建 37。

- [9c1e44947042](https://github.com/seedds/nxnative/commit/9c1e44947042)：仅合入两项通用 NVN 修复。清屏附件大于当前渲染区域时，先单独清除完整附件；单采样 Downsample 支持整级缩放和格式转换。沿用当前着色器链。
- [0f99c353d3ab](https://github.com/seedds/nxnative/commit/0f99c353d3ab)：合入震动引擎故障重建、退避、手柄默认通道回退及旧回调隔离，包含官方测试。
- [188c1d8f9985](https://github.com/seedds/nxnative/commit/188c1d8f9985)：合入卡顿诊断安全修复，并补齐此前未包含的诊断基础代码及测试桩符号。DM_HITCH_DUMP 默认关闭；快照在锁内复制线程状态，恢复线程后再解析符号。

验证：真实 Metal GPU 清屏测试 49 项；单采样 Downsample 三种像素场景（缩放、BGRA 转 RGBA、双线性过滤）；alpha 测试 1165 项；NVN 测试 114 项；官方震动恢复测试、线程同步和线程回收测试；启动页测试 7 项；iOS Release 编译。

地图使用存档副本作对照：绕过本地 alpha 修复仍出现斑块，启用后恢复正常，继续保留本地修复。测试开关仅存在于私有测试副本。iPhone 和实体手柄仍需实机验证。
