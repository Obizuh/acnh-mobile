# v0.3.40 · 构建 42

- 同步官方上传暂存分批及完成后回收修复，减少资源长时间堆积；128 MiB 为单批预算，不代表整个应用的内存上限。
- 同步纹理读回自动释放修复与临时缓冲所有权修复；资源注册表使用优先级继承锁。
- 同步脏页时间戳发布和上传竞态修复，避免并发写入后沿用过期的纹理内容判断。
- 同步显示链路停顿恢复修复：前台回调长时间中断时临时获取 drawable，并以有上限的退避重试，兼顾焦点与挂起状态。
- 启动页采用原生导航列表，个性化内提供默认关闭的帧缓冲；支持 Mac 独立应用制作；保留专属／通用／自定义控制器、系统输入法、相册及原有 `.data` 流程。

官方来源：[上传预算](https://github.com/seedds/nxnative/commit/7dc250e2f1f0)、[读回与所有权](https://github.com/seedds/nxnative/commit/5a81e13f2459)、[读回回归](https://github.com/seedds/nxnative/commit/2d344f050c85)、[脏页竞态](https://github.com/seedds/nxnative/commit/5b0c11a4f307)、[显示恢复](https://github.com/seedds/nxnative/commit/9feac2f1e7f6)。仅选择适用且可独立验证的部分。

验证：实际 Metal 上传预算两种编码模式各 261 项、显示恢复 CPU 43 项与实际 Metal 47 项、读回像素及寿命 8 项、alpha 渲染 1165 项通过；脏页并发、宿主、启动设置与构建工具回归通过。完整私人 IPA 与无游戏空壳分别编译、校验与审计。手机长期实测仍待验证，不据此承诺帧率、温度或续航改善。

[Mac 制作教程](MAC-APP.zh-CN.md)。
