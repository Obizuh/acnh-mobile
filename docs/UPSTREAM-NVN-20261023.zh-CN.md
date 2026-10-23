# NXNative 通用 NVN 优化（待真机验证）

来源：[8a4b1dc](https://github.com/seedds/nxnative/commit/8a4b1dc82073f9d8635bd51d634af681f5d71ee1)。仅选取 `nvn_mtl.m` 与新增 `nvn_rec.h`，保留本项目已有功能，不合入其他游戏适配或 Mac 窗口分辨率变更。

- iOS 默认独立线程处理 Metal 编码；Mac 保持默认关闭，可用 `DM_NVN_ENC_THREAD=1` 验证。
- 大于等于 512 字节的 uniform shadow 在后台复制；提交 GPU 前等待所需复制完成。`DM_NVN_UB_COPY_THREAD=0` 可切回原路径。
- uniform 内联更新跨渲染通道保留，遇到依赖或提交结束时落地；默认最多 160 条，`DM_NVN_UB_LAZY=0` 可关闭跨通道保留。
- 显存地址查找增加两级缓存。
- `DM_NVN_GPUPROF=帧号` 或 `起始帧-结束帧` 开启 GPU 编码器耗时诊断，默认关闭。未加入 Metal HUD。

不修改着色器翻译链，仍为 `87eb0f1ee74a`；原配套着色器与 3.0.3 `.data` 保持兼容。GPU 时间戳、编码器标签的线程转发，以及提交和资源使用前的等待随本次优化一并保留。

验证：Mac 主程序及 iPhone/iPad Release 编译通过。官方 NVN 回归在编码线程开、关两种配置下均为 99/100；两者唯一失败仍是已有的 `counter timestamp written (non-zero)`，不是本次新增失败。独立测试存档实际运行 75 秒，进入机场并显示中文对话，按测试要求正常退出；uniform shadow 校验 4,000 次一致、0 次不一致，GPU 帧 600 耗时诊断成功输出。两条纹理类型提示在合入前的 integrated-boot.log 已存在。手机发热、帧率及长时间运行效果仍需真机对比，不据其他游戏的数据宣称动森收益。

对应 v0.3.3 / 构建 5；空壳一键工具继续仅本地保留。
