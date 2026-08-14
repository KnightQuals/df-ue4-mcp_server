# Release — Windows 交付包

本目录存放「全面战场 MiniGame」的 Windows 运行包（Development 配置，UE4.27）。

## 当前版本

| 文件 | 说明 |
|---|---|
| `MCPGameProject_MiniGame_Win64_Fixed.zip` | V1+V2 全量玩法交付包（657MB，Conquest 三局铃声结算、正式 HUD、窗口化、单人/双人脚本） |

> 二进制 ZIP 不进入 Git 历史（GitHub 单文件 100MB 限制），通过 **GitHub Releases** 发布下载；
> 本目录的 ZIP 仅作为本地副本。SHA256 见对应 Release 说明。

## 使用

1. 解压 ZIP，进入 `WindowsNoEditor/`。
2. `Play_Solo.bat`：单人试玩（攻方玩家 + 守方 AI）。
3. `Play_Local_2P.bat`：同机双人（主机攻方 + 客户端守方，两个 960×540 窗口）。

玩法规则见仓库根 README「交付包玩法验证」。

## 重新打包

```bat
RunUAT.bat BuildCookRun -project=<uproject> -noP4 -platform=Win64 -clientconfig=Development -build -cook -map=/Game/Maps/NewMap -stage -pak -archive -archivedirectory=<out> -prereqs -IgnoreCookErrors
```

注意事项（PATH 保留 EnhancedInput、editor-only 代码 WITH_EDITOR 隔离、动态资产 DirectoriesToAlwaysCook）见根 README「已知环境注意事项」。
