# Duo 半折叠游戏布局

使用 Apple iOS 27.1 的 UIHingeInteraction 观察开合状态，再查询当前窗口的活动 division reserved region；不使用机型名称、固定分辨率或猜测的折叠角度阈值。

当窗口具有横向折痕且设备部分展开时，游戏画面按完整 16:9 比例放到上方可用区域，现有虚拟控制器放到下方区域。上下区域的边界来自系统折痕区域。完全展开、外屏或没有可用的横向折痕时恢复原布局。普通 iPhone 和 iPad 保持原有横屏限制。Duo 游戏期间允许转向以使用横向折痕，启动设置页仍为横排。

通用、专属及自定义布局继续使用原控制器，按键不作横纵方向不同的拉伸。切换区域时释放旧触点，游戏触屏坐标以实际画面区域为准。

已完成 iPhone/iPad Release 编译、上下区域几何检查、原有控制器布局和启动页回归检查。尚未完成 Duo 真机开合测试。

官方参考：
- https://developer.apple.com/documentation/uikit/uihinge
- https://developer.apple.com/documentation/uikit/uihingeinteraction
- https://developer.apple.com/videos/play/tech-talks/111463/
