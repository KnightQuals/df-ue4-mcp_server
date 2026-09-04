# df-ue4-mcp_server — 基于 MCP + LLM 的 UE4.27 自动开发系统

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-4.27-orange)](https://www.unrealengine.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-VS2019%20(MSVC%20v142)-blue)]()
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)

> 腾讯实习课题项目。目标：基于 MCP 协议 + LLM，让 AI 自动开发 UE4.27 C++ 游戏，
> 最终由 AI 自动实现「全面战场 MiniGame」。核心 KPI = 打通「AI 写码 → 编译 → 进引擎建蓝图 → Play」的自动闭环。

## 本项目与上游的关系

本仓库以开源项目 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（UE5.5）为基线，
**backport 移植到 UE4.27 + VS2019**（向 DFM 大项目的引擎环境对齐）。

- `main` 分支：UE5.5 原版留底（原生跑通的参照系）
- `port/ue4.27` 分支：**移植到 UE4.27 的主力版本**（本分支）

## 当前进度（W5，2026-07-24）

- [x] W4 收尾（详见 2026-07-17 进度条目）
- [x] **W5 V1 完整多人占点逻辑**（7/24）：
  - `ASpawnAreaHub` 加 Team UPROPERTY（0=攻方基地/1=守方基地）
  - `AGameMode_Breakthrough` 改造为双 SpawnHub 模式（AttackerHub + DefenderHub）+ 懒查找兜底
  - SpawnDefaultPawnFor 按玩家 Team 选对应 SpawnHub spawn
  - PostLogin 自动 spawn 守方 AI Character（Team=1，站守方基地）—— 单人 Play 模拟多人占点
  - BattleSectorAnchor 已实现人数比较占领（`AttackersInZone - DefendersInZone` 决定方向）
- [x] **W5 环境修复**（7/23）：VS2019 重装 + PATH 加 UE 引擎 Binaries + EnhancedInput Binaries（修复 dll 加载 GetLastError=126）
- [x] **W5 输入修复**（7/23）：BreakthroughCharacter 改回旧版 Input BindAxis + DefaultInput.ini 加 AxisMappings（不依赖 IMC/InputAction 资产）
- [x] **W5 视觉化提升**（7/24）：
  - SpawnAreaHub + BattleSectorAnchor 加 BoxComponent 线框（绿/红/蓝）+ TextRenderComponent 悬浮文字
  - BreakthroughCharacter 加载 UE4_Mannequin + Idle 动画（Animation Starter Pack）
  - Floor Scale=10（10000×10000 大平台）+ CaptureRadius 300→800（占领区域扩大）
  - 文字/Mannequin BeginPlay 按位置自动旋转朝场景中心
- [x] **W5 MCP 接口拓展**：`set_actor_skeletal_mesh`（让用户前端选 SkeletalMesh 资产设给 Character）
- [ ] **W6 已知问题**：第一视角角色模型遮挡视野 → Camera + SpringArm 改造或 PlayerController 第一/第三人称切换
- [ ] **W7+ V2 详细设计**（iWiki 4019862612 全面战场 V2）：DataTable / Spline / 安全区 / GameMode 两种 / 多人同步

详细按天记录见 `worklog/`。

## 自动化闭环（路 B orchestrator）

`orchestrator.py`（仓库根目录）是课题核心交付物——一条命令跑通完整闭环：

```bash
python orchestrator.py "任务描述，如：新建守方基地 ABattleDefenderCamp"
```

流程：关 UE → tclaude + GLM-5.2 自主写 C++ → 编译（删超长环境变量避免环境块超长）→ 报错自修循环（最多 3 轮）→ 起 UE + 等 55557 → tclaude 用 MCP 建蓝图 + spawn。实时显示 tclaude 的思考 / 工具调用 / 结果（stream-json 解析）。

**已解决的坑**：① tclaude Bash 工具起 UE 后台不生效卡死 → 起 UE 移到外层脚本；② UE 编译环境块超长（ACC_PRODUCT_CONFIG_V3 34 万字符撑爆 65535 限制）→ 编译时 env 删 >1000 字符变量。

## V1 玩法代码（MCPGameProject/Source/MCPGameProject/）

| 类 | 状态 | 作用 |
|---|---|---|
| `ABattleSectorAnchor` | ✅ 已建 + 验收 | 据点：CaptureZone 球体 + Overlap 回调 + Tick 双向占领（-1 守 / 0 中立 / 1 攻）+ 变色（ApplyAnchorColor） |
| `ABattleCampSector` | ✅ 已建 | 攻方基地（Cube 外形） |
| `ABattleDefenderCamp` | ✅ 已建 | 守方基地（Cube 外形） |
| `ABattleSectorBase` | ✅ 胜负判定补全 | 区域容器（持有基地/据点引用 + MatchDuration 倒计时 + bMatchOver + 时间到判攻守胜） |
| `ASpawnAreaHub` | ✅ 已建（7/16） | 出生点集（TArray SpawnPoints + GetRandomSpawnPoint） |
| `ABreakthroughCharacter` | ✅ 已建（7/16） | 玩家 Character（Team 阵营 + BeginPlay UE_LOG） |
| `ADefaultPlayerController` | ✅ 已建（7/16） | 玩家控制器（V1 空实现用引擎默认输入） |
| `AGameMode_Breakthrough` | ✅ 已建（7/16） | GameMode（找 SpawnHub + 重写 SpawnDefaultPawnFor 调 GetRandomSpawnPoint spawn 玩家 + 分阵营） |

## 未来计划（Roadmap）

### V1 待补（玩法完整度）
- [x] **出生点 `ASpawnAreaHub`**（7/16 完成）：TArray SpawnPoints + GetRandomSpawnPoint
- [x] **胜负判定**（7/16 完成）：ABattleSectorBase Tick 倒计时 + bMatchOver + 时间到判攻守胜
- [x] **材质变色**（7/15 完成）：MCP 材质接口（create_material/add_vector_parameter/set_material_on_component）让 AI 自主建材质，用户 Play 确认据点变色
- [ ] **V1 Play 完整验证**：补 Character 输入映射（InputMappingContext）+ Play 跑通玩家在 SpawnHub 出生 + 占领据点变色 + 胜负判定完整流程
- [ ] **对局时间配置**（V1 简化版可用 C++ `UPROPERTY`，V2 用 DataTable）

### V2 详细设计（iWiki 4019862612 全面战场 V2）
- [ ] **DataTable 配置**（Key=MapID）：每行存对局时间 / 禁区倒计时 / 据点位置 / 阵营初始归属，用 DataTable 替代硬编码
- [ ] **Spline 区域范围**：用 `USplineComponent` 制作基地 / 据点 / 交战区边界（替代 USphereComponent）
- [ ] **安全区 / 禁区机制**：我方基地 / 据点区 = 安全区；敌方基地 / 外围 = 禁区，玩家进禁区 10s 倒计时后死亡
- [ ] **GameMode 两种**：`AGameMode_Breakthrough`（攻防模式）+ `AGameMode_Conquest`（占领模式），共享 `AGameState` / `ADefaultPlayerController` / `ABreakthroughCharacter` / `ABattleFieldPlayerState`
- [ ] **多人同步 Replication / RPC**：`UPROPERTY(Replicated)` + Server/Client RPC，让阵营归属 / 占领进度在多客户端同步（V2 核心难点）

### MCP 接口拓展（L2 自动化系统扩展）
**策略**：边编译做游戏边拓展接口边界增强 MCP 能力（用户定调）。三次验证成功。
- [x] **材质接口**（7/15 完成）：`UnrealMCPMaterialCommands`（create_material / add_vector_parameter / set_material_on_component）+ Python material_tools.py
- [x] **FClassProperty 分支**（7/17 完成）：`set_actor_property` 支持 UClass* 引用（设 GameMode 等），改 UnrealMCPCommonUtils SetObjectProperty
- [x] **日志接口**（7/17 完成）：`UnrealMCPDebugCommands`（get_output_log 读 UE 输出日志 + filter 过滤）+ Python debug_tools.py，FILEREAD_AllowWrite 修复 UE 独占写
- [ ] **资产接口**：`create_asset` / `import_texture` / `set_default_material` 等（Content Browser 操作）
- [ ] **关卡接口**：`new_level` / `save_level` / `open_level`（自动化关卡操作）
- [ ] **DataTable 接口**：`create_data_table` / `add_row` / `read_row`（V2 配置）
- [ ] **get_actor_properties 反射属性**：当前只返回基础 transform，要扩展返回完整反射属性（DefaultGameMode 等）
- [ ] **GameMode 接口**：`set_game_mode` / `start_match`（V2 流程）

### 收尾
- [ ] 录 Demo（一条命令跑通完整闭环视频）
- [ ] 写 iWiki 系统架构文档（mentor 要求）
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

## V1 玩法验证（MainMap）

1. UE 打开后，Content Browser → Maps → 双击 `MainMap` 加载（已含攻方基地/守方基地/据点/SectorBase/Floor）。
2. WorldSettings → DefaultGameMode = `BP_GameMode_Breakthrough`（如丢失，用 MCP `set_actor_property` 重设）。
3. ▶ Play → 攻方玩家在 SpawnHub_Attacker @ [-400,0,100] 出生 → 走到中间 Anchor_1 → 占领（红）。
4. PostLogin 自动 spawn 守方 AI（Mannequin + Idle 动画）在守方基地。

## W6–W12 进度更新（2026-08-01 ~ 2026-09-04）

- [x] **W6 战场 V1 定版 + 交付**（8/14–8/20）：第一视角、守方 AI 巡逻、多人 Replication、KiteDemo 环境接入、Windows 交付包（单人/双人 bat）；**战场 demo 封包验收（8/20）**
- [x] **W7 战场 V2 核心玩法**（8/4–8/11）：DataTable 配置（Key=MapID）、外围禁区淘汰重生、多据点推进、Conquest 三局铃声结算、Spline 区域边界、汉化 HUD、符号化俯视小地图
- [x] **W8 联机架构定稿**（8/25–8/28）：V1 监听服务器 → V2 专用服务器（DS）架构评审，Server RPC / 属性复制代码路径定稿
- [x] **W9 SOL 立项 + V1 四功能**（8/21–8/28）：容器（权重随机）、F 开箱首开决定道具、双击转移、丢弃
- [x] **W10 SOL 联机与第一闭环**（8/31–9/1）：服务器权威架构实装（容器 PostLoad 兜底复制）、3 撤离点（风险收益梯度）、负重减速（数值与动画联动）、8 容器 18 物品三层价值密度、行走/奔跑动画接入
- [x] **W11 SOL 战斗收口**（9/3）：拾荒者 NPC（派生自 Character 零重复；GuardRadius 结构保证出生区安全）、hitscan 战斗 + 死亡掉落、对局时限 + 超时结算 + R 键回合重置
- [x] **W12 SOL 定版封包**（9/3–9/4）：Development 配置 `RunUAT BuildCookRun -compressed`（228MB / SHA256 已留档）、Release 上 GitHub、封包前体检（运行时编译 + cook 验证 + 多人实测）通过

## 已完成：SOL MiniGame（V1，已封包定版）

> 本系统的第二个产出：UE4.27 C++ 搜打撤（search-and-extract）mini-game。同一套 MCP 工具链跑出第二个完整游戏——证明自动化管线**跨玩法类型可复用**。本节只讲 SOL 独有的设计（服务器权威、属性复制、Canvas HUD 等与战场章节重复的不再展开）。

### 玩法定位：搜打撤（与战场并列的第二类型）

进入地图搜刮容器物资，负重与价值权衡，限时走向撤离点把东西带出去——**搜到的东西只有活着带出去才算收益**。战场张力在对抗，SOL 张力在取舍。

### 三个核心系统

**① 容器与物品经济（三层价值密度，重物必须低价）** —— 8 种容器（鸟笼/军械箱/服务器机柜/走私箱…）独立战利品池，18 种物品按价值/重量密度分三层：轻质高价（非洲之心 1200 / 0.5kg、军用芯片 900 / 0.2kg）→中段→重质低价（燃料罐 80 / 6kg）。重物必须低价，否则负重系统不构成取舍——这是按 spawnWeight 加权验算的（不是按理想组合推断）。

**② 撤离点：风险收益梯度** —— 3 个撤离点呈 ×1.0 / ×1.3 / ×1.8 倍率梯度，时长与门槛（0 / 400 / 1200 价值）随距离递增。带得越少、走越近、越安全；带得越多、走越远、收益越高——`拿多少 + 走哪个出口` 是真决策不是流程。

**③ 拾荒者 NPC：守卫半径（结构保证出生安全）** —— 派生自 `ASOLCharacter`：血量、死亡、掉落、动画全部继承，零重复玩法代码；`GuardRadius=1500cm` 在感知阶段就过滤掉距所守容器 15m 外的目标，巡逻路过出生区也无视玩家。**出生区安全是结构保证不是数值调出来的运气**——曾因巡逻半径撞进视距被迫两轮调参无果，最终靠这个结构方案根治。

### 负重与动画的咬合

负重曲线 0-30% 免惩罚 → 线性下降至 55%（满载 30kg 时 330）。动画阈值 340 卡在区间中间——**满载视觉上是"走"、轻装视觉上是"跑"，别人的负重状态从剪影就能读出来**。数值系统之间互相咬合，是答辩/面试里被追问设计思路时的好素材。

### 死亡掉落：闭环关键咬合点

死亡把背包物资全部撒在原地（`ASOLItemPickup::SpawnGrounded` 向下 trace 落地，避免胶囊体碰撞导致的悬浮 bug）。结果：**杀带货玩家 → 物资就在地上 → 撤离成为唯一保住收益的方式**。价值密度、负重、撤离门槛到这一步全部串起来。

### 网络架构要点（与战场对照）

- 联机代码路径与战场完全一致：Server RPC + `HasAuthority()` + 属性复制（不在此重复）
- **隐私分层**：战场全公开；SOL 背包用 `COND_OwnerOnly`，别人的搜刮别人看不见（搜打撤的"信息不对称"才有战术意义）
- **多进程真实 UDP 演示**：`SOL-MiniGame-Release/` 下的 `Play_Local_2P.bat` 起 1 个 listen server + 2 个独立 client 进程，真实跨进程通信，不依赖 PIE 内存模拟

### Windows 交付包（已封包定版）

- Development 配置，`RunUAT BuildCookRun` + `-compressed`（pak 61MB，整包 228MB）
- `Play_Solo.bat` / `Play_Local_2P.bat`（纯 ASCII）
- **封包前体检**：运行时 target 编译过 / cook 验证含动态资产 / 多人实测两个窗口均能加入并交互
- 包体存放于 GitHub Releases（Git 单文件 100MB 限制，228MB 二进制不能入 Git 历史）
- 包内 `WindowsNoEditor/SOLProject/Saved/Logs/SOLProject.log` 已实证：8 容器 / 3 撤离点 / `Anim load: idle=OK walk=OK jog=OK death=OK` ×4（动态资产 cook 进包验证）

### 已知限制（被追问时诚实清单）

- **无音效**（项目零音频资产，命中/死亡无声音反馈）
- **NPC 不绕障碍**（开阔地直线移动，无 NavMesh/行为树，与战场同样简化）
- **击杀信息流只有自己**（全服 feed 需要独立复制事件通道）
- **动画三档切换无过渡混合**（Idle / Walk / Jog 之间硬切，无 blendspace）

## 已知环境注意事项（backport 踩坑记录）

- **工具链**：UE4.27 需 VS2019（MSVC v142）；UE5.5 需 VS2022（v143）。二者可共存。
- **XGE / IncrediBuild**：若 IncrediBuild 授权失效，UE4.27 会把着色器派给 XGE 并卡死。
  解决：将 `IncrediBuild/xgConsole.exe` 改名（UE 检测不到即回退本地编译）。
- **命名铁律**：项目名/路径/类名全程纯英文+数字（中文路径会导致 UnrealHeaderTool 乱码）。

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
