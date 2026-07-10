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

## 当前进度（W3 收尾，2026-07-10）

- [x] 选型并在原生 UE5.5 上复现跑通，验证「外部命令 → UE 自动建对象/建蓝图」核心链路
- [x] **backport 到 UE4.27 + VS2019**：插件 C++ 编译通过（6 处 API 适配）
- [x] **UE4.27 运行时功能验证通过**：Actor 建/改、程序化建蓝图全部正常
- [x] **W2 三链路打通 + 合体**：CLI 写码（cli-agent + GLM-5.2）/ 编译取报错 / UE 填参 Play，cli-agent 挂 30 个 UE MCP 工具
- [x] **修 4 个接口 bug**：create_blueprint 不落盘 / spawn 重名崩溃 / reparent 父类查找失败 / add_component 重名崩溃
- [x] **W3 自动闭环跑通（课题 KPI 核心达成）**：`orchestrator.py` 一条命令自主「关UE → AI 写 C++ → 编译 → 报错自修 → 起 UE → MCP 建蓝图 + spawn」全链路无人值守
- [x] **V1 占领逻辑核心验收通过**：阵营 0=攻/1=守，进度 -1/0/1 双向占领，Play 日志 `Capture progress -0.9 → +1.0` + `Sector captured by attackers!`
- [ ] **W4 计划**：先增加 MCP 材质接口（颜色变色暴露接口边界不够）→ V1 玩法补全（出生点 / 胜负）→ V2 详细设计（DataTable / Spline / 安全区 / GameMode）→ 收尾（录 Demo + 写 wiki）

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
| `ABattleSectorAnchor` | ✅ 已建 + 验收 | 据点：CaptureZone 球体 + Overlap 回调 + Tick 双向占领（-1 守 / 0 中立 / 1 攻）+ 变色（ApplyAnchorColor） |
| `ABattleCampSector` | ✅ 已建（未验收） | 攻方基地（Cube 外形） |
| `ABattleDefenderCamp` | ✅ 已建（未验收） | 守方基地（Cube 外形） |
| `ABattleSectorBase` | ⚠️ 骨架有未测 | 区域容器（持有基地 / 据点引用 + 对局时间 + 胜负判定骨架） |

## 未来计划（Roadmap）

### V1 待补（玩法完整度）
- [ ] **出生点 `ASpawnAreaHub`**：V1 规则要求"双方阵营在自己的基地出生"——基地区域内多出生集，玩家选择/随机分配
- [ ] **胜负判定**：`ABattleSectorBase` Tick 检查对局时间到 + `OwningTeam` 判定攻/守胜，UI 广播
- [ ] **对局时间配置**（V1 简化版可用 C++ `UPROPERTY`，V2 用 DataTable）
- [ ] **材质变色**（红/蓝/白）：M_AnchorColor 材质 shader 失败根因 = **MCP 接口边界不够**（无材质属性/节点连接接口），AI 无法自修，W4 先增加 MCP 材质接口解决

### V2 详细设计（课题 全面战场 V2）
- [ ] **DataTable 配置**（Key=MapID）：每行存对局时间 / 禁区倒计时 / 据点位置 / 阵营初始归属，用 DataTable 替代硬编码
- [ ] **Spline 区域范围**：用 `USplineComponent` 制作基地 / 据点 / 交战区边界（替代 USphereComponent）
- [ ] **安全区 / 禁区机制**：我方基地 / 据点区 = 安全区；敌方基地 / 外围 = 禁区，玩家进禁区 10s 倒计时后死亡
- [ ] **GameMode 两种**：`AGameMode_Breakthrough`（攻防模式）+ `AGameMode_Conquest`（占领模式），共享 `AGameState` / `ADefaultPlayerController` / `ABreakthroughCharacter` / `ABattleFieldPlayerState`
- [ ] **多人同步 Replication / RPC**：`UPROPERTY(Replicated)` + Server/Client RPC，让阵营归属 / 占领进度在多客户端同步（V2 核心难点）

### MCP 接口拓展（L2 自动化系统扩展）
**策略**：边编译做游戏边拓展接口边界增强 MCP 能力（用户定调）。
- [ ] **材质接口**（W4 优先）：C++ 写 `UnrealMCPMaterialCommands`（`create_material` / `set_vector_parameter` / `connect_node` / `add_to_element` 等）+ Python 暴露，让 AI 自主建/改材质
- [ ] **资产接口**：`create_asset` / `import_texture` / `set_default_material` 等（Content Browser 操作）
- [ ] **关卡接口**：`new_level` / `save_level` / `open_level`（自动化关卡操作）
- [ ] **DataTable 接口**：`create_data_table` / `add_row` / `read_row`（V2 配置）
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

1. 用 UE4.27 打开 `MCPGameProject/MCPGameProject.uproject`（首次会编译插件与着色器）。
2. 确认 `编辑 > 插件` 中 `UnrealMCP` 已启用；输出日志出现 `Server started on 127.0.0.1:55557`。
3. 起 Python 服务：`cd Python && uv venv && uv pip install -e .`
4. 验证（保持 UE 开着）：`.venv/Scripts/python scripts/actors/test_cube.py`、
   `scripts/blueprints/test_create_and_spawn_cube_blueprint.py`。

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
