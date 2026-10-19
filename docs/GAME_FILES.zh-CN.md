# 游戏文件与 .data

游戏、密钥与存档均不随本项目提供。版本配置为 Switch ACNH 3.0.3。准备自己的解密后 `ExeFS` 与 `RomFS`，不要把这些目录放入公开仓库。

将自己的 RomFS 打成直接读取的分卷：

```sh
python3 tools/pack_data.py /path/to/RomFS /path/to/private-work/ACNHMobile
```

目标目录须为空。默认每卷约 1 GiB；内容不能跨卷拆开，单个大文件可使该卷超过此大小。工具生成 `ACNH-index.data`、`ACNH-001.data` 等分卷及本地校验清单。不同大小来自文件边界及最后一卷余量，扩展名本身不代表压缩格式。

将索引和全部分卷放到手机应用的 `Documents/ACNHMobile/`，保留同一套文件的原名。运行时映射索引并按范围读分卷，不会生成展开的 RomFS。现有构建 7–12 使用的同一套 `.data` 可以继续使用。

已有存档留在应用自己的存档目录，不放进资源分卷。不要把个人存档加入源码包。
