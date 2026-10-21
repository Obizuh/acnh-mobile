# v0.1.21 · 构建34

## 修复与更新
- 修复采样器描述符替换时过早释放旧采样器的问题：等待相应 GPU 栅栏完成后再释放，避免在途绘制访问失效句柄。
- 保留参数编码兼容、相册导出、amiibo 文件导入与等待暂停，以及已有 CPU/内存优化和画面修复。
- 公开空壳构建排除私有 local/outputs 目录；公开附件不包含游戏程序、游戏资源、转换着色器、密钥、存档或卡片数据。

## 版本选择
- ACNHMobile-iOS26：最低 iOS26 的主线版。
- ACNHMobile-iOS18-compat：最低 iOS18 的 Metal3 兼容版（附件由兼容工作区分别构建并核验）。
- 两版保留原定画质和功能，安装下限不等于所有机型实测通过；空壳需要自行准备拥有的运行文件。

## 验证与限制
- 主线：iOS Release 空壳构建及8文件严格白名单审计通过；版本0.1.21/build34、Info.plist及Mach-O下限26.0、ZIP完整性已核验。
- Apple M4实际GPU：采样器替换后存活、未完成栅栏仍保留、完成后释放回归通过；Metal API Validation及参数编码/偏移/缓存/GPU输出对照通过。修复前寿命回归明确失败。
- NFP服务5133项、amiibo等待64轮、相册清理及1537缓冲登记回归通过。
- 用户已明确将铲头变黄、坑边黑块、持续发热/掉帧留待后续处理；本次不宣称解决。4/32MiB长期同场景对照未完成，不宣称32MiB更稳定或更凉。iPhone真机效果仍需复测。

## English
Sampler replacement now retains the old sampler until the GPU fence completes. Existing graphics quality and feature configuration are preserved. Public downloads are game-free shells only. The iOS26 mainline requires iOS26; the Metal3 compatibility edition requires iOS18 and is built separately. Device installation minimums are not a claim of testing every device. Shovel discoloration/dark ground edges and sustained phone heating/frame drops remain known issues deferred by the user; no 32MiB upload stability/cooling benefit has been established.
