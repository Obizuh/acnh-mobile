# amiibo 文件导入（iOS 优化分支）

从优化分支 0.1.15 / 构建27 单独合入（本分支待发布）：进入游戏的 amiibo 扫描界面，选择“导入 amiibo 文件”，在系统文件选择器中选择自己的原始 `.bin` 备份。无需 NFC 签名权限。选择菜单、系统文件选择器、文件读取和错误重试期间暂停游戏计时；成功、取消或扫描结束时恢复，不修改手机日期时间。取消后可按游戏的 B 退出，再进入重新选择。

支持原始 NTAG215 标签布局的 532、540、572 字节文件。校验长度、厂商标识、UID 校验字节、容量头和型号布局；这是格式校验，不是密码学真伪认证。不支持压缩包、文本 `.nfc`、amiitool 内部布局、密钥文件。不随项目提供卡片数据或密钥。

只从文件读取公开标签标识和角色型号，按游戏现有 ROM 挂载流程返回。动森 3.0.3 的导入符号与调用点已核对：GetTagInfo → Mount(model=0,target=1) → GetModelInfo。角色是否适用于当前设施、剧情进度和活动，由游戏判断。本版本不解密主人资料、不实现应用区读写，也不修改或持久化原始文件。系统文件提供商可能需要先下载云端文件。

每次游戏开始扫描分配独立会话令牌；停止、结束、重新开始后旧文件回调作废。读取至多573字节，超长拒绝。多次导入、边界和过期会话均由生产服务单元测试覆盖。底层只读接口与 iOS 编译测试已通过；手机文件选择和实际角色召唤仍需设备验证。

实体 NFC 通道测试保留为独立选项，仍受当前运行进程的 NFC 签名权限限制；仅验证读卡通道，尚未连接游戏角色识别。

参考格式：[libnx NFC 接口](https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/nfc.h)、[amiitool 标签/内部布局转换](https://github.com/socram8888/amiitool/blob/master/amiibo.c)。本实现独立编写，未复制其加解密实现。

构建25修正标准 amiibo 的 `F1 10 FF EE` 容量头识别，兼容普通 NTAG215 备份头；此前构建24的校验会误拒绝标准文件。依据：[原始读卡研究记录](https://recon.cx/2018/montreal/schedule/system/event_attachments/attachments/000/000/046/original/RECON-MTL-2018-Hacking_Amiibo_with_SDR.pdf)。

构建26修正 Character ID 为原始三字节序列，只有 Numbering ID 交换字节。用户提供509份标准大小原始数据均通过生产接口回归；另外两份80字节文件不属于完整数据。参见 [NFP ModelInfo 字段](https://switchbrew.org/wiki/NFC_services#ModelInfo)。

构建27除冻结游戏时钟外，同时阻挡生产图形提交入口，避免按帧更新的扫描 UI 继续推进。暂停按原因组合，后台往返不能解除文件选择暂停；用户取消/完成/扫描结束时仅清理文件选择原因。
