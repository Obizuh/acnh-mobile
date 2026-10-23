## ACNH Mobile v0.3.16 · 构建 18

- 更新官方统一着色器链及 inputs.tsv 完整构建指纹。整套重建会清理过期输出，宿主及打包工具检查游戏、输入和配套表，防止旧缓存混用。优化变体兼容整数输出入口和空参数入口。
- Mac 支持每个手柄独立分配玩家，按游戏允许人数处理，并保留断开重连的槽位；可用 DM_PAD_MERGE=1 恢复合并到玩家一的行为。手机仍沿用原通用、专属及自定义控制逻辑。
- Mac 全屏时，可将不匹配游戏帧率的显示模式切到同尺寸的兼容刷新率；退出全屏、切换屏幕或退出宿主时恢复。窗口模式不切换；DM_SCREEN_MATCH=0 可关闭。
- 增加 Mac 开发构建入口，只编译 Mac 着色器和宿主，不生成 IPA。已有输入可用 setup.sh mac；从原始游戏准备可用 one_click.py ipa --platform mac，原 iOS 一键流程不变。
- 同步官方无事件音频打开修复，并兼容动森 SDK 内部嵌套调用：仅在最外层成功返回后启动一次渲染线程。注册补丁拆分后，动森三个模块及改写表与旧版逐字节一致，只有来源校验记录更新。
- 保留当前地图透明度处理、系统输入法、事务存档、amiibo、相册及震动逻辑。没有加入其他游戏配置或改变手机按键布局。

原 3.0.3 `.data` 无需重做。新的配套着色器必须与宿主一起更新。仍要求 iOS 26，没有新增 iOS 18、旧 Metal 或 A12/A13 支持。

### 验证与范围

Mac 多玩家输入 44 项、音频打开 27 项回归通过；音频嵌套问题在修复前复现，修复后不提前/重复启动，打开失败不启动。构建链、输入指纹、缓存恢复、Mac-only NVN 复用及改写注册测试通过。透明度与 tile-fetch 的真实 GPU 检查 1,165 项、绘制跳过 38 项、几何回归 95 项通过。Mac/iOS 各 9,766 个基础着色器、4,291 个可选优化变体全部生成，完整库均携带匹配的输入指纹。

新增 Mac 入口实际完成宿主编译并运行存档副本，中文播报、进岛及完整地图正常；音频线程只启动一次，无重复创建提示，缓存指纹和配套表被正确接受。完整 iPhone/iPad IPA 与从干净源码构建的空壳编译、版本、图标、源代码身份、着色器完整性及校验和检查通过。测试进程主动退出。

两只实体手柄、144/165 Hz 外接屏的实际切换及手机长时间运行仍待对应设备验证，不宣称性能或降温改善。

来源：[优化变体](https://github.com/seedds/nxnative/commit/4e91a3d05302)、[统一链与输入指纹](https://github.com/seedds/nxnative/commit/86d9ba123967)、[Mac 多手柄](https://github.com/seedds/nxnative/commit/76545a58a790)、[全屏刷新率](https://github.com/seedds/nxnative/commit/a6787033eba1)、[Mac 开发模式](https://github.com/seedds/nxnative/commit/98a91a674954)、[改写工具拆分](https://github.com/seedds/nxnative/commit/f4f0a85d6e58)、[音频兼容](https://github.com/seedds/nxnative/commit/c392d56af005)。
