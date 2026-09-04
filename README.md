# df-ue4-mcp_server — 基于 MCP + LLM 的 UE4.27 自动开发系统

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-4.27-orange)](https://www.unrealengine.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-VS2019%20(MSVC%20v142)-blue)]()
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)

> 实习课题项目。目标：基于 MCP 协议 + LLM，让 AI 自动开发 UE4.27 C++ 游戏，
> 最终由 AI 自动实现「全面战场 MiniGame」。核心 KPI = 打通「AI 写码 → 编译 → 进引擎建蓝图 → Play」的自动闭环。

## 本项目与上游的关系

本仓库以开源项目 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（UE5.5）为基线，
**backport 移植到 UE4.27 + VS2019**（向大型项目的引擎环境对齐）。

- `main` 分支：UE5.5 原版留底（原生跑通的参照系）
- `port/ue4.27` 分支：**移植到 UE4.27 的主力版本**（本分支）

## 开发进度（2026-06-15 ~ 2026-09-04，两个 demo 均已定版交付）

| 周 | 日期 | 里程碑 |
|---|---|---|
| W1 | 6/15–6/20 | 环境搭建：UE4.27 + Rider + VS2019 工具链；跑通官方 C++ Quick Start |
| W2 | 6/22–6/28 | unreal-mcp backport 启动：Python Server + C++ 插件移植；首批 MCP 接口跑通 |
| W3 | 7/1–7/7 | 三条链路合体：CLI Agent 写码 → Build.bat 编译取报错 → MCP 建蓝图/Spawn；报错自修闭环跑通 |
| W4 | 7/10–7/17 | `orchestrator.py` 一条命令完整闭环；V1 据点/出生点/胜负判定骨架 |
| W5 | 7/20–7/24 | V1 玩法成型：完整多人占点逻辑（双 SpawnHub 分队 + 守方 AI 补位）、视觉化（Mannequin + 线框 + 悬浮文字）、材质/日志/反射接口拓展 |
| W6 | 7/27–8/3 | 第一视角（Camera + OwnerNoSee）、守方 AI 巡逻、多人 Replication、KiteDemo 环境接入、关卡默认地图修复 |
| W7 | 8/4–8/11 | **V2 核心玩法全量**：DataTable 配置（Key=MapID）、外围禁区淘汰重生、多据点推进、Conquest 模式、Spline 区域边界、汉化 HUD、符号化俯视小地图 |
| W8 | 8/12–8/14 | **战场交付定版**：正式 HUD（比分/倒计时/进度条）、三局铃声结算、回合重置、Windows 交付包（单人/双人脚本） |
| W9 | 8/18–8/20 | HUD 收尾修复（Conquest 空区不回退 + 归属方远程进度条）；**战场 demo 封包验收结项（8/20）**，转向 SOL |
| W10 | 8/21–8/28 | SOL 立项 + v1 四功能：容器（spawnWeight 随机）、F 开箱（首开决定道具）、双击转移、丢弃 |
| W11 | 8/31–9/1 | **SOL 联机与第一闭环**：DS 架构实装（服务器权威 + 复制兜底）、3 撤离点（风险收益梯度）、负重减速（数值与动画联动）、8 容器 18 物品三层价值密度、行走/奔跑动画 |
| W12 | 9/3–9/4 | **SOL 战斗收口 + 定版交付**：拾荒者 NPC（GuardRadius 结构保证出生安全）、hitscan 战斗 + 死亡掉落、对局时限 + 回合重置、Windows 交付包 + GitHub Release（`sol-v1.0`） |

两个 demo 的完整玩法说明分别见下文「V1 玩法代码/玩法验证」（战场）与「已完成：SOL MiniGame」章节。

## 自动化闭环（路 B orchestrator）

`orchestrator.py`（仓库根目录）是课题核心交付物——一条命令跑通完整闭环：

```bash
python orchestrator.py "任务描述，如：新建守方基地 ABattleDefenderCamp"
```

流程：关 UE → CLI Agent（LLM）自主写 C++ → 编译（删超长环境变量避免环境块超长）→ 报错自修循环（最多 3 轮）→ 起 UE + 等 55557 → CLI Agent 用 MCP 建蓝图 + spawn。实时显示 Agent 的思考 / 工具调用 / 结果（stream-json 解析）。

**已解决的坑**：① CLI Agent 的 Bash 工具起 UE 后台不生效卡死 → 起 UE 移到外层脚本；② UE 编译环境块超长（某个超长配置变量撑爆 65535 限制）→ 编译时 env 删 >1000 字符变量。

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

## 未来计划

> 两个 demo 已定版交付，代码冻结。以下为候选方向。

**工具链**
- 资产接口扩展：`create_asset` / `import_texture` / `set_default_material` 等 Content Browser 操作
- 对局流程接口：`start_match` 等 GameMode 生命周期控制
- 蓝图节点图操作增强

**自动化验证**
- 玩法回归 commandlet 化，接入 CI
- 录制完整闭环 Demo 视频

**玩法扩展（SOL）**
- 移动端触控适配：虚拟摇杆 + 拖动视角
- 战斗深度：更多武器类型 / NPC 行为树 / 音效反馈

**部署**
- 源码版引擎编译 Server target，实现 Dedicated Server 独立进程部署

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
