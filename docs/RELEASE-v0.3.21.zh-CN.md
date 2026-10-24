# v0.3.21 · 构建 23

- 同步 NX 官方上传线程等待修复，提交已记录的任务后再等待繁忙上传槽，避免互相等待。
- 同步官方纹理读回修复：复制前落实待执行清屏；升级纹理用途时保留原缓冲区关联。
- 同步官方显示纹理资源提交，避免首次显示前未提交资源驻留集合。
- 启动页加入默认开启、可关闭的“触屏反馈”；通用、专属及相应自定义布局按下按键或握住摇杆时轻震，编辑预览不震动。
- 同步官方六声道 PCM 转立体声混音与音频嵌套打开保护，替代原有的对应本地保护。
- 同步着色器拆分脚本的字符串语法修复；当前保留的 NSP 构建流程已通过语法检查。
- 保留原有相册导出、系统输入法和手机控制布局，现有 .data 继续使用。

来源：[上传等待](https://github.com/seedds/nxnative/commit/9cb7af32378d)、[纹理读回](https://github.com/seedds/nxnative/commit/cd0e61c23b74)、[线性纹理](https://github.com/seedds/nxnative/commit/672681f83f95)、[显示资源](https://github.com/seedds/nxnative/commit/43b35a689e69)、[触屏反馈](https://github.com/seedds/nxnative/commit/80bc18896f98)、[音频混音](https://github.com/seedds/nxnative/commit/8e8ea51cda67)、[音频打开](https://github.com/seedds/nxnative/commit/6de59702ec8a)。

验证：真实 GPU 纹理读回 6 项、线性纹理读回 17 项、既有渲染回归 1203 项全部通过；音频打开 27 项、六声道混音、启动设置与打包工具测试通过。iOS IPA 编译、内容校验通过。手机震动手感与长期运行仍需实机确认。
