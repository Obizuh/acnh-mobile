# v0.3.31 · 构建 33

- 同步官方纹理模板视图修复，避免 Metal 验证模式下的视图创建错误。
- 同步上传线程提交修复，避免等待尚未交给编码线程的任务而卡住。
- 命令内存按请求大小分配，减少小请求占用并正确容纳大记录。
- 同步 AAC 视频音轨的输出数量与缓冲区大小修复，避免越界或播放时钟异常。
- 同步忙等循环识别修复，减少线程争抢造成的停顿；Mac 输入框清除残留按键并遵守最短字数。
- 同步离线网络请求状态时序修复。
- 临时目录与存档事务目录使用隐藏属性，保留原路径和内容；保留个性化、控制器、相册及输入法，原有 `.data` 继续使用。

官方来源：[模板视图](https://github.com/seedds/nxnative/commit/7861e7c1a991)、[命令内存](https://github.com/seedds/nxnative/commit/fc6c0a2a1112)、[上传提交](https://github.com/seedds/nxnative/commit/b87a0cc6aacb)、[视频音频](https://github.com/seedds/nxnative/commit/3f4d973ac5e6)、[忙等](https://github.com/seedds/nxnative/commit/6d349844880e)、[Mac 输入](https://github.com/seedds/nxnative/commit/fa69e20a2804)、[离线请求](https://github.com/seedds/nxnative/commit/7a029f66d92c)。仅选择上述适用修复。

验证：实际 Metal 验证模式下模板视图 16 项、NVN 命令回归 114 项、alpha 渲染 1165 项通过；独立编码线程下模板视图与命令测试通过。宿主、忙等、构建工具回归通过。完整私人 IPA 与公开空壳分别编译和审计。新修复尚未进行手机长期实测。
