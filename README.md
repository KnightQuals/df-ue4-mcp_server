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

## 当前进度

- [x] 选型并在原生 UE5.5 上复现跑通，验证「外部命令 → UE 自动建对象/建蓝图」核心链路
- [x] **backport 到 UE4.27 + VS2019**：插件 C++ 编译通过（6 处 API 适配）
- [x] **UE4.27 运行时功能验证通过**：Actor 建/改、程序化建蓝图（create/add component/set mesh/compile/spawn）全部正常
- [ ] W2：CLI 写码 / 编译取报错 / UE 填参 Play 三条链路各自打通
- [ ] W2–W3：串成自动闭环（写码 → 编译 → 读错 → 自动修 → 建蓝图 → Play）
- [ ] W3–W4：用系统自动生成「全面战场」核心玩法

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
