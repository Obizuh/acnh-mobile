# NFC只读通道测试（构建23）

安装测试IPA后，在游戏中进入amiibo扫描界面。

- 若提示宿主没有TAG权限：无需继续贴卡，把log.txt发来。需要实际运行的LiveContainer或LiveProcess具备有效NFC签名权限；只给游戏IPA添加配置无效。
- 若无法核对权限或宿主用途说明：本次不启动扫描，把log.txt发来；不要把它当作手机硬件不支持的结论。
- 若弹出iOS扫描界面：将一张卡靠近手机顶部。测试只尝试读取Ultralight兼容标签的16字节测试页，不写卡。
- 若显示读取成功：仅代表当前签名和传输通道可用，不代表游戏已识别该卡或能够召唤角色。按B退出游戏内等待界面。

实体卡验证只能在iPhone完成。Mac测试覆盖NFP进入、取消、结束、重进与无标签语义；iOS Release编译不代表真机读卡通过。

依据：Apple NFCTagReaderSession / NFCMiFareTag文档；LiveContainer官方README中的客体权限限制。
