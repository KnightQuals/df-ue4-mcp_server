# df-ue4-mcp_server — 基于 MCP + LLM 的 UE4.27 自动开发系统

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-4.27-orange)](https://www.unrealengine.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-VS2019%20(MSVC%20v142)-blue)]()
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)

> 实习课题项目。目标：基于 MCP 协议 + LLM，让 AI 自动开发 UE4.27 C++ 游戏，
> 最终由 AI 自动实现「全面战场 MiniGame」。核心 KPI = 打通「AI 写码 → 编译 → 进引擎建蓝图 → Play」的自动闭环。

## 本项目与上游的关系

本仓库以开源项目 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（UE5.5）为基线，
**backport 移植到 UE4.27 + VS2019**（向 大型项目的引擎环境对齐）。

- `main` 分支：UE5.5 原版留底（原生跑通的参照系）
- `port/ue4.27` 分支：**移植到 UE4.27 的主力版本**（本分支）

## 当前进度（W6，2026-08-03 —— 最后一周收尾中）

- [x] W4/W5 收尾（详见 WorkLog/2026-07-17.md、2026-08-03.md）
- [x] **W6 MCP 接口大拓展**（7/27）：
  - `get_actor_properties` 反射扩展 —— `TFieldIterator<UProperty>` 返回完整 UPROPERTY（OwningTeam/CaptureRadius/CaptureProgress 等）
  - `save_level` / `new_level` / `open_level` —— AI 自主管理关卡
  - DataTable 接口（`create_data_table` / `add_row` / `read_row`）—— V2 配置基础
  - `spawn_actor` 加 `mesh_path` 参数（程序化 spawn StaticMesh）+ 重名崩溃修复（MakeUniqueObjectName）
- [x] **W6 第一视角修复**（7/27）：加 FirstPersonCamera（眼高 64cm + bUsePawnControlRotation）+ `SetOwnerNoSee(true)`（玩家不见自己 Mannequin，防遮挡；AI 仍可见）
- [x] **W6 守方 AI 巡逻**（7/29-7/30）：C++ 手动移动（无 NavMesh/BT）—— 随机选点走动 + 停顿 + Idle/Jog 动画切换，`SetActorLocation(bSweep=true)` 移动无 Controller pawn
- [x] **W6 V1 玩法打磨**（7/30）：
  - Shift 加速（Sprint 400/800）
  - 占点变色 bug 根治（GetActorTeam 改用真实 Team 属性 + 运行时 NewObject 重建 CaptureZone 绕过蓝图组件模板残留）
  - 玩家动画按速度切 Idle/Jog（第三方/多人可见）
- [x] **W6 区域扩大 + 布局**（7/30-7/31）：SpawnHub 30m / CaptureZone 16m / SpawnHub 拉到 ±3000 / Floor Scale 30 / AI 巡逻半径 24m
- [x] **W6 多人 Replication**（7/31）：Character bReplicates + Team 复制 + 巡逻仅 server；GameMode 延迟判断玩家数（单人 spawn AI / 多人跳过）；Anchor OnRep 同步变色（编译 0 error，待真机 2 窗口 PIE 验证）
- [x] **W6 环境资源接入**（7/30-7/31）：KiteDemo（Open World Demo Collection）+ **XGE 卡死彻底修复**（禁 XGEController.uplugin）
- [x] **★W6 关卡反复丢失根因★**（7/31）：`DefaultEngine.ini` 默认地图是引擎模板 OpenWorld，无 EditorStartupMap → 每次启动打开临时空地图。修复：`GameDefaultMap` + `EditorStartupMap` = `/Game/Maps/NewMap`
- [x] **W6 环境装饰返工**（7/31-8/3）：巨石 scale 6-10 太大糊脸挡视野 → 删 11 个大装饰，改小尺度松树（scale 1.0-1.2）摆战场外围边缘（Y=±2800 两侧，避开中间走廊/据点），只做背景点缀
- [ ] **收尾（最后一周）**：NewMap 环境定版 + 多人 2 窗口 PIE 真机验证 + V2 挑核心做（安全区/禁区 > DataTable > Spline）+ 录 Demo + 写 wiki 架构文档

> **协作模式（7/30 与 cap 沟通后确定）**：AI 只写后端 C++ 代码 + 改 MCP 接口；UE 引擎前端操作（调蓝图 / 拖角色 / 导入资源 / Play）由用户手动做，AI 提供手把手教程。主 MainMap 保住已验证成果，环境实验在 NewMap 里做。

详细按天记录见 `WorkLog/`。

## 自动化闭环（路 B orchestrator）

`orchestrator.py`（仓库根目录）是课题核心交付物——一条命令跑通完整闭环：

```bash
python orchestrator.py "任务描述，如：新建守方基地 ABattleDefenderCamp"
```

流程：关 UE → cli-agent + GLM-5.2 自主写 C++ → 编译（删超长环境变量避免环境块超长）→ 报错自修循环（最多 3 轮）→ 起 UE + 等 55557 → cli-agent 用 MCP 建蓝图 + spawn。实时显示 cli-agent 的思考 / 工具调用 / 结果（stream-json 解析）。

**已解决的坑**：① cli-agent Bash 工具起 UE 后台不生效卡死 → 起 UE 移到外层脚本；② UE 编译环境块超长（ACC_PRODUCT_CONFIG_V3 34 万字符撑爆 65535 限制）→ 编译时 env 删 >1000 字符变量。

## V1 玩法代码（MCPGameProject/Source/MCPGameProject/）

| 类 | 状态 | 作用 |
|---|---|---|
| `ABattleSectorAnchor` | ✅ 已建 + 验收 | 据点：CaptureZone（BoxComponent，运行时重建防序列化残留）+ Overlap 计数 + Tick 双向占领（-1 守 / 0 中立 / 1 攻，按真实 Team 属性判定）+ 变色（ApplyAnchorColor）+ OwningTeam/CaptureProgress **Replicated（OnRep 同步变色）** |
| `ABattleCampSector` | ✅ 已建 | 攻方基地（Cube 外形） |
| `ABattleDefenderCamp` | ✅ 已建 | 守方基地（Cube 外形） |
| `ABattleSectorBase` | ✅ 胜负判定补全 | 区域容器（持有基地/据点引用 + MatchDuration 倒计时 + bMatchOver + 时间到判攻守胜） |
| `ASpawnAreaHub` | ✅ 已建（7/16）+ 视觉化 | 出生点集（TArray SpawnPoints + GetRandomSpawnPoint）+ Team 属性 + BoxComponent 线框 + TextRender 文字 |
| `ABreakthroughCharacter` | ✅ 已建 + 多人 + 巡逻 + 加速 | 玩家/AI Character：Team 阵营（**Replicated**）+ FirstPersonCamera + Mannequin（OwnerNoSee）+ **守方 AI C++ 巡逻**（SetActorLocation sweep 移动 + Idle/Jog 动画切换）+ **Shift 加速**（400/800）+ **bReplicates 多人同步** |
| `ADefaultPlayerController` | ✅ 已建（7/16） | 玩家控制器（V1 空实现用引擎默认输入） |
| `AGameMode_Breakthrough` | ✅ 已建 + 多人 | GameMode（双 SpawnHub 按 Team 分队 spawn）+ **多人：延迟 1.5s 判断玩家数，单人 spawn AI 守方 / 双人真人分攻守** |

## 未来计划（Roadmap）

### V1 待补（玩法完整度）
- [x] **出生点 `ASpawnAreaHub`**（7/16 完成）：TArray SpawnPoints + GetRandomSpawnPoint
- [x] **胜负判定**（7/16 完成）：ABattleSectorBase Tick 倒计时 + bMatchOver + 时间到判攻守胜
- [x] **材质变色**（7/15 完成）：MCP 材质接口让 AI 自主建材质，用户 Play 确认据点变色
- [x] **占点玩法验收**（7/30 完成）：玩家进 CaptureZone 2s 倒数 → 变色（初始蓝守方 → 攻方占领变红）
- [x] **守方 AI 巡逻**（7/30 完成）：C++ 手动巡逻 + Idle/Jog 动画，防守区内闲逛
- [x] **Shift 加速**（7/30 完成）：Sprint 400/800
- [x] **多人 Replication**（7/31 完成，编译通过）：Character/Anchor/GameMode 网络复制，2 player 分攻守
- [ ] **多人 2 窗口 PIE 真机验证**：Play 选 2 players + Listen Server，验证双人对战 + 占点同步变色
- [ ] **环境定版**：NewMap 外围小树点缀视觉确认（不挡视野）

### V2 详细设计（课题 全面战场 V2）
- [ ] **DataTable 配置**（Key=MapID）：对局时间 / 禁区倒计时 / 据点位置 / 阵营归属（**MCP 接口已备好，逻辑待写**）
- [ ] **Spline 区域范围**：用 `USplineComponent` 制作基地 / 据点 / 交战区边界
- [ ] **安全区 / 禁区机制**：敌方区/外围 = 禁区，进入 10s 倒计时死亡（**玩法完整度最高，最后一周优先**）
- [ ] **GameMode 两种**：Breakthrough（攻防）+ Conquest（占领）
- [ ] **多人同步进阶**：Server/Client RPC + PlayerState 计分（V1 已打通 Replicated 基础）

### MCP 接口拓展（L2 自动化系统扩展）
**策略**：边编译做游戏边拓展接口边界增强 MCP 能力（用户定调）。持续验证成功。
- [x] **材质接口**（7/15 完成）：`UnrealMCPMaterialCommands`（create_material / add_vector_parameter / set_material_on_component）
- [x] **FClassProperty 分支**（7/17 完成）：`set_actor_property` 支持 UClass* 引用（设 GameMode 等）
- [x] **日志接口**（7/17 完成）：`get_output_log` 读 UE 输出日志（FILEREAD_AllowWrite 修复独占写）
- [x] **反射属性**（7/27 完成）：`get_actor_properties` 用 TFieldIterator 返回完整 UPROPERTY
- [x] **关卡接口**（7/27 完成）：`save_level` / `new_level` / `open_level`
- [x] **DataTable 接口**（7/27 完成）：`create_data_table` / `add_row` / `read_row`
- [x] **mesh 摆放**（7/30 完成）：`spawn_actor` 加 `mesh_path` 参数（程序化 spawn StaticMesh）+ 重名崩溃修复（MakeUniqueObjectName）
- [ ] **资产接口**：`create_asset` / `import_texture` / `set_default_material`（Content Browser 操作）
- [ ] **GameMode 接口**：`set_game_mode` / `start_match`（V2 流程）

### 收尾
- [ ] 录 Demo（一条命令跑通完整闭环视频）
- [ ] 写 wiki 系统架构文档（mentor 要求）
- [ ] 整理 GitHub 仓库（README 持续更新）

## 架构（三块）

1. **UnrealMCP 插件（C++）** — `MCPGameProject/Plugins/UnrealMCP`，在 UE 内开 TCP server（127.0.0.1:55557），
   接收命令并调用引擎 API 建蓝图/改参数/spawn。← backport 的主战场。
2. **Python MCP Server** — `Python/`，基于 FastMCP，向 AI 客户端暴露工具（JSON Schema），把工具调用翻译成 TCP 命令。
3. **示例工程 MCPGameProject** — UE4.27 空白工程 + 已配置好的插件，用于验证。

## 快速开始（UE4.27）

1. **clone 后下载 AnimStarterPack**（Mannequin mesh + 动画，BreakthroughCharacter 引用）：
   - 打开 Epic Games Launcher → 商城 → 搜索 `Animation Starter Pack` → 添加到项目 → 选 MCPGameProject
   - 或手动放到 `MCPGameProject/Content/AnimStarterPack/`（路径要对，C++ 用 `FObjectFinder` 加载 `/Game/AnimStarterPack/UE4_Mannequin/Mesh/SK_Mannequin`）
2. 用 UE4.27 打开 `MCPGameProject/MCPGameProject.uproject`（首次会编译插件与着色器）。
3. 确认 `编辑 > 插件` 中 `UnrealMCP` 已启用；输出日志出现 `Server started on 127.0.0.1:55557`。
4. 起 Python 服务：`cd Python && uv venv && uv pip install -e .`
5. 验证（保持 UE 开着）：`.venv/Scripts/python scripts/actors/test_cube.py`、
   `scripts/blueprints/test_create_and_spawn_cube_blueprint.py`。

## V1 玩法验证

> 两张关卡：**MainMap** = 已验证的双人 + 占点主玩法（保住成果）；**NewMap** = 环境装饰实验（外围小树点缀）。`EditorStartupMap` 默认加载其一。

### 单人验证
1. UE 打开后加载关卡（已含攻方/守方基地 @ ±3000、据点 Anchor_1 @ 中心、SectorBase、Floor Scale 30）。
2. WorldSettings → DefaultGameMode = `BP_GameMode_Breakthrough`（如丢失，用 MCP `set_actor_property` 重设）。
3. ▶ Play → 攻方玩家在 SpawnHub_Attacker 出生 → **Shift 加速**走到中间 Anchor_1 → 站 2s 倒数 → 据点变红（攻方占领）。
4. 守方 AI（Mannequin + Jog 动画）在守方基地 24m 范围内巡逻走动（不越界）。

### 多人（2 Player）验证
1. 顶部工具栏 ▶ 旁下拉 → **Number of Players = 2** + **Net Mode = Play As Listen Server**。
2. ▶ Play → 弹出两个窗口：玩家 1 = 攻方（左），玩家 2 = 守方（右），此时不 spawn AI。
3. 两窗口能看到对方角色移动（Replication）；攻方占领据点时两个窗口据点同步变红（OnRep）。

## 已知环境注意事项（backport 踩坑记录）

- **工具链**：UE4.27 需 VS2019（MSVC v142）；UE5.5 需 VS2022（v143）。二者可共存。
- **XGE / IncrediBuild（重要）**：若 IncrediBuild 授权失效，UE4.27 会把着色器派给 XGE 并卡死（SpeedTree/KiteDemo 尤其明显，卡 70% 死）。
  彻底解决（三重，最后一个最关键）：① `Engine/Config/ConsoleVariables.ini` 取消注释 `r.XGEShaderCompile = 0`；② `BaseEngine.ini [DevOptions.Shaders]` 加 `bAllowDistributedCompilingWithXGE=False`；③ **`Engine/Plugins/XGEController/XGEController.uplugin` 设 `EnabledByDefault=false`**（禁插件，UE 找不到 XGE 只能本地编译）。
- **命名铁律**：项目名/路径/类名全程纯英文+数字（中文路径会导致 UnrealHeaderTool 乱码）。
- **默认地图必须设**：`DefaultEngine.ini` 的 `GameDefaultMap` + `EditorStartupMap` 都要指向项目关卡，否则 UE 每次启动打开引擎模板/临时地图，spawn 的 actor 一关就丢。
- **C++ 改蓝图继承的组件类型**（如 Sphere→Box）会留序列化残留，recompile 清不掉 → 运行时 `NewObject` 重建组件规避。

---

<details>
<summary>以下为上游 chongdashu/unreal-mcp 原始 README（保留作参考）</summary>

This project enables AI assistant clients like Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state (upstream note). The API, functionality, and implementation details are subject to significant changes:

- Breaking changes may occur without notice
- Features may be incomplete or unstable
- Documentation may be outdated or missing
- Production use is not recommended at this time

## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language:

| Category | Capabilities |
|----------|-------------|
| **Actor Management** | • Create and delete actors (cubes, spheres, lights, cameras, etc.)<br>• Set actor transforms (position, rotation, scale)<br>• Query actor properties and find actors by name<br>• List all actors in the current level |
| **Blueprint Development** | • Create new Blueprint classes with custom components<br>• Add and configure components (mesh, camera, light, etc.)<br>• Set component properties and physics settings<br>• Compile Blueprints and spawn Blueprint actors<br>• Create input mappings for player controls |
| **Blueprint Node Graph** | • Add event nodes (BeginPlay, Tick, etc.)<br>• Create function call nodes and connect them<br>• Add variables with custom types and default values<br>• Create component and self references<br>• Find and manage nodes in the graph |
| **Editor Control** | • Focus viewport on specific actors or locations<br>• Control viewport camera orientation and distance |

All these capabilities are accessible through natural language commands via AI assistants, making it easy to automate and control Unreal Engine workflows.

## 🧩 Components

### Sample Project (MCPGameProject) `MCPGameProject`
- Based off the Blank Project, but with the UnrealMCP plugin added.

### Plugin (UnrealMCP) `MCPGameProject/Plugins/UnrealMCP`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Python/unreal_mcp_server.py`
- Implemented in `unreal_mcp_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## 📂 Directory Structure

- **MCPGameProject/** - Example Unreal project
  - **Plugins/UnrealMCP/** - C++ plugin source
    - **Source/UnrealMCP/** - Plugin source code
    - **UnrealMCP.uplugin** - Plugin definition

- **Python/** - Python server and tools
  - **tools/** - Tool modules for actor, editor, and blueprint operations
  - **scripts/** - Example scripts and demos

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## 🚀 Quick Start Guide

### Prerequisites
- Unreal Engine 5.5+
- Python 3.12+
- MCP Client (e.g., Claude Desktop, Cursor, Windsurf)

### Sample project

For getting started quickly, feel free to use the starter project in `MCPGameProject`. This is a UE 5.5 Blank Starter Project with the `UnrealMCP.uplugin` already configured. 

1. **Prepare the project**
   - Right-click your .uproject file
   - Generate Visual Studio project files
2. **Build the project (including the plugin)**
   - Open solution (`.sln`)
   - Choose `Development Editor` as your target.
   - Build

### Plugin
Otherwise, if you want to use the plugin in your existing project:

1. **Copy the plugin to your project**
   - Copy `MCPGameProject/Plugins/UnrealMCP` to your project's Plugins folder

2. **Enable the plugin**
   - Edit > Plugins
   - Find "UnrealMCP" in Editor category
   - Enable the plugin
   - Restart editor when prompted

3. **Build the plugin**
   - Right-click your .uproject file
   - Generate Visual Studio project files
   - Open solution (`.sln)
   - Build with your target platform and output settings

### Python Server Setup

See [Python/README.md](Python/README.md) for detailed Python setup instructions, including:
- Setting up your Python environment
- Running the MCP server
- Using direct or server-based connections

### Configuring your MCP Client

Use the following JSON for your mcp configuration based on your MCP client.

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON>",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

An example is found in `mcp.json`

### MCP Configuration Locations

Depending on which MCP client you're using, the configuration file location will differ:

| MCP Client | Configuration File Location | Notes |
|------------|------------------------------|-------|
| Claude Desktop | `~/.config/claude-desktop/mcp.json` | On Windows: `%USERPROFILE%\.config\claude-desktop\mcp.json` |
| Cursor | `.cursor/mcp.json` | Located in your project root directory |
| Windsurf | `~/.config/windsurf/mcp.json` | On Windows: `%USERPROFILE%\.config\windsurf\mcp.json` |

Each client uses the same JSON format as shown in the example above. 
Simply place the configuration in the appropriate location for your MCP client.


## License
MIT

## Questions

For questions, you can reach me on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)
</details>
