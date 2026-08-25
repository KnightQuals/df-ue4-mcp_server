# df-ue4-mcp_server — 基于 MCP + LLM 的 UE4.27 自动开发系统

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-4.27-orange)](https://www.unrealengine.com)
[![Toolchain](https://img.shields.io/badge/Toolchain-VS2019%20(MSVC%20v142)-blue)]()
[![Python](https://img.shields.io/badge/Python-3.10%2B-yellow)](https://www.python.org)

> 实习课题项目。目标：基于 MCP 协议 + LLM，让 AI 自动开发 UE4.27 C++ 游戏，
> 最终由 AI 自动实现「全面战场 MiniGame」。核心 KPI = 打通「AI 写码 → 编译 → 进引擎建蓝图 → Play」的自动闭环。

本仓库 = **UnrealMCP 插件（UE4.27 移植版）+ 用该系统开发出的完整「全面战场 MiniGame」**。
在其他设备 clone 本仓库即可同时获得两者：插件位于 `MCPGameProject/Plugins/UnrealMCP/`，
MiniGame 位于 `MCPGameProject/`（关卡、C++ 玩法、蓝图、配置）。玩法已打包为可直接运行的
Windows 交付包（见下文「交付包玩法验证」）。

## 本项目与上游的关系

本仓库以开源项目 [chongdashu/unreal-mcp](https://github.com/chongdashu/unreal-mcp)（UE5.5）为基线，
**backport 移植到 UE4.27 + VS2019**（对齐大型项目的引擎环境）。

- `main` 分支：UE5.5 原版留底（原生跑通的参照系）
- `port/ue4.27` 分支：**移植到 UE4.27 的主力版本**（本分支）

## 开发历程

| 阶段 | 时间 | 里程碑 |
|---|---|---|
| W1 | 6/15–6/20 | 环境搭建：UE4.27 + Rider + VS2019 工具链；跑通官方 C++ Quick Start |
| W2 | 6/22–6/28 | unreal-mcp backport 启动：Python Server + C++ 插件移植；首批 MCP 接口跑通 |
| W3 | 7/1–7/7 | 三条链路合体：CLI Agent 写码 → Build.bat 编译取报错 → UE MCP 建蓝图/Spawn；`compile.sh` + CLI Agent 自闭环变色 |
| W4 | 7/10–7/17 | 路 B `orchestrator.py` 一条命令跑通完整闭环；V1 据点/出生点/胜负判定骨架 |
| W5 | 7/20–7/24 | V1 玩法成型：占点变色（石碑）、材质接口、双人分队雏形 |
| W6 | 7/27–8/3 | 第一视角、守方 AI 巡逻、多人 Replication、KiteDemo 环境接入、XGE 卡死根治、关卡默认地图修复 |
| W7 | 8/4–8/11 | **V2 核心玩法全量**：DataTable 配置、外围禁区淘汰重生、多据点推进、Conquest 模式、Spline 区域边界、占点规则定版、三区可视化 |
| W8 | 8/12–8/14 | **交付定版**：正式 HUD（比分/倒计时/进度条）、三局铃声结算、回合战场重置、Windows 交付包（单人/双人脚本）、环境植被点缀、交付 cap |
| W9 | 8/18–8/20 | HUD 全面汉化、符号化俯视小地图、Conquest 修复（空区不回退 + 归属方远程进度条）；**战场 demo 定版**，转向烽火地带 demo + 实习总结 |

详细按天记录见 `WorkLog/`。

## 已完成：全面战场 MiniGame（V1 + V2，已交付）

> 下表「课题要求」= mentor 原始需求；「自研扩展」= 开发中沉淀的新设计。两者现已全部落地。

### 课题原始要求（全部完成，对照课题需求文档）

| 要求 | 实现 |
|---|---|
| 大战场核心玩法：争夺区域 `BattleSectorBase`，攻方出生在左侧攻方基地（`BattleCampSector`），守方出生在右侧守方基地，两阵营争夺据点（`BattleSectorAnchor`） | 全套类落地：`ABattleSectorBase` / `ABattleSectorAnchor` / `ABattleCampSector` / `ABattleDefenderCamp` + `ASpawnAreaHub` |
| 据点被攻方占领后，切换到下一区域继续争夺 | 多据点推进：Anchor_1 被占后公告并解锁 Anchor_2，全占攻方胜 |
| 双玩法模式：攻防（Breakthrough）+ 占领（Conquest） | `AGameMode_Breakthrough`（顺序推进）/ `AGameMode_Conquest`（铃声结算），各配蓝图子类 |
| 玩法共享引擎基础（GameState / PlayerController / Character / PlayerState） | `ABattleGameState`（比分/回合/倒计时 Replicated）、自定义 PlayerController、`ABreakthroughCharacter`（Team/移动/动画全同步） |
| 多人（≥2 人）并分配到不同阵营（攻=0，守=1） | GameMode 按加入顺序自动分队；单人自动补守方 AI，双人时 AI 自动退场 |
| 双方在自己基地出生，向目标区域行军进入据点区域 | 双 SpawnHub 随机出生点 + 五类区域标线引导 |
| 据点默认守方拥有，进度值 -1（守方占领） | CaptureProgress 连续模型 [-1, 1]，初始 -1 |
| 攻方人数优势：进度 -1→0（中立）→+1（完全占领）；守方优势反向 | 人数差推进：攻多涨 / 守多退 / 对峙冻结 / 空区冻结（2026-08-18 修订：据点稳定保持，守方须进区反夺） |
| 配置时间内攻方占领完毕攻方胜，反之守方胜 | MatchDuration 倒计时胜负判定（Breakthrough）；Conquest 另有总分判定 |
| V2：用 Spline 制作基地、交战区域、据点区域范围 | `ABattleAreaSpline` 闭合样条 + 射线法内外判定 + 五类区域贴地标线（可编辑器拖点改形） |
| V2：安全区/禁区划分，进入禁区倒计时 10s 后死亡 | 外围禁区：位置判定 10s 淘汰 + 3s 按阵营重生（敌方基地算禁区的细则经评估简化为不做） |
| V2：DataTable 配置对局时间、禁区倒计时，每行 Key=MapID | `DT_BattleMapConfig`，Key=NewMap，12 项参数全配置化 |

### 自研扩展玩法（全部完成）

| 功能 | 说明 |
|---|---|
| 石碑据点变色 | 据点换 GroundRevealRock 岩石模型 + 动态材质：红=攻方已占 / 蓝=守方待占 / 灰=未解锁，全屏公告 |
| 守方 AI 巡逻 | 纯 C++ 巡逻（无 NavMesh/BT）：随机选点走动 + 停顿 + Idle/Jog 动画切换；单人模式自动补位，双人模式自动退场 |
| 第一视角 | 眼高相机 + OwnerNoSee 本体隐藏；Shift 加速（400/800）、空格跳跃 |
| 外围禁区 | 安全区外 10s 倒计时淘汰 → 3s 按阵营重生；server 裁定 + Replicated；边界行走不误判 |
| DataTable 配置 | `DT_BattleMapConfig`（Key=NewMap）：对局时长、禁区倒计时、重生、安全区尺寸、占点速度、Conquest 参数全部配置化 |
| 多据点推进（Breakthrough） | 据点 1 解锁据点 2，全占攻方胜、超时守方胜 |
| Conquest 三局铃声结算 | 3 局 × 30s；每局结束按该瞬间据点归属结算（2-0/1-1/0-2），总分高者胜、平分平局；每局结束全员回出生点、据点复原 |
| 五类区域可视化 | 攻/守基地、交战区、据点区、安全区的贴地彩色标线（Spline 可编辑多边形）+ 玩家所在区域 HUD 提示 |
| 正式 HUD | 顶部攻红守蓝比分 + 局数 + 倒计时 + 上局结算；据点内红蓝拉锯占点进度条 + 状态文字；终局胜负 Banner；**己方据点被敌推进度条远程同步（【远处】标记）**；界面文案全中文 |
| 符号化小地图 | 右上角俯视地图样式 Canvas 小地图：地面/禁区底图 + 微网格、菱形据点（①②）+ 占领中黄圈、出生点旗帜（攻/守）、队友与敌人色点、本地玩家方向三角 |
| 战场环境 | KiteDemo 开放世界资产：外围树木/岩石/植被点缀 + 场内边缘层次（不影响玩法交互） |

### Windows 交付包（Release/）

- Development 配置、默认窗口化（单人 1280×720；双人两个 960×540 窗口左右排列）
- `Play_Solo.bat` 单人试玩；`Play_Local_2P.bat` 同机双人（主机 `?listen` + 本机客户端）
- 动态资产（动画/材质/岩石）已全部强制 Cook；运行冒烟验证通过
- 二进制包体积约 657MB，**存放于 GitHub Releases**（Git 单文件 100MB 限制），源码侧在 `Release/` 保留说明

## 已完成：MCP 接口清单（L2 自动化系统）

按能力分组，全部为 UE4.27 backport 后实测可用的接口：

| 分组 | 接口 |
|---|---|
| Actor 操作 | `spawn_actor`（含 `mesh_path` 程序化摆放 + 重名防崩）/ `delete_actor` / `set_actor_transform` / `find_actors_by_name` / `get_actors_in_level` / `set_actor_skeletal_mesh` / `spawn_blueprint_actor` |
| 属性与反射 | `get_actor_properties`（TFieldIterator 全量 UPROPERTY）/ `set_actor_property`（含 UClass* 引用分支） |
| 蓝图 | `create_blueprint` / `add_component_to_blueprint` / `set_component_property` / `compile_blueprint` / `set_blueprint_property` / `set_physics_properties` / 节点图操作（add_event/function/variable、connect 等） |
| 材质 | `create_material` / `add_vector_parameter` / `set_material_on_component` |
| 关卡管理 | `save_level` / `new_level` / `open_level` / `duplicate_level`（独立 Map Asset ID；活跃关卡崩溃已修为受控报错） |
| DataTable | `create_data_table` / `add_row`（含保存落盘修复）/ `read_row` |
| 工程与日志 | `create_input_mapping` / `get_output_log`（读 UE 输出日志） |
| 视口 | `focus_viewport` / `take_screenshot` |
| Editor-only Commandlet | `MCPConfigureMap`（无界面设 GameMode / 关卡快照） |

自动化闭环交付物：`orchestrator.py`（仓库根目录）——一条命令：关 UE → CLI Agent（GLM-5.2）写 C++ → 编译（清超长环境变量）→ 报错自修（≤3 轮）→ 起 UE → MCP 建蓝图/Spawn，全程实时输出。

## 架构（三块）

1. **UnrealMCP 插件（C++）** — `MCPGameProject/Plugins/UnrealMCP`，在 UE 内开 TCP server（127.0.0.1:55557），接收命令并调用引擎 API 建蓝图/改参数/spawn。← backport 的主战场。
2. **Python MCP Server** — `Python/`，基于 FastMCP，向 AI 客户端暴露工具（JSON Schema），把工具调用翻译成 TCP 命令。
3. **MCPGameProject** — 不再是空白工程：内含用本系统开发完成的**全面战场 MiniGame**（玩法见上文），同时仍是插件的验证环境。

## 快速开始（UE4.27）

1. **clone 后下载 AnimStarterPack**（Mannequin mesh + 动画，BreakthroughCharacter 引用）：
   - 打开 Epic Games Launcher → 商城 → 搜索 `Animation Starter Pack` → 添加到项目 → 选 MCPGameProject
   - 或手动放到 `MCPGameProject/Content/AnimStarterPack/`
2. 用 UE4.27 打开 `MCPGameProject/MCPGameProject.uproject`（首次会编译插件与着色器）。
3. 确认 `编辑 > 插件` 中 `UnrealMCP` 已启用；输出日志出现 `Server started on 127.0.0.1:55557`。
4. 起 Python 服务：`cd Python && uv venv && uv pip install -e .`
5. 验证（保持 UE 开着）：`.venv/Scripts/python scripts/actors/test_cube.py`。

## 交付包玩法验证

> 三图管理：**MainMap** = 稳定存档（仅明确授权才覆盖）；**NewMap** = 工作台（当前玩法全量）；**BackUpMap** = 每次大改前的回退快照。

### 直接运行（推荐）

解压 Release 交付包，在 `WindowsNoEditor/` 下：

1. **`Play_Solo.bat`**：单人。你是攻方，1.5 秒后系统补一个守方 AI 巡逻。
2. **`Play_Local_2P.bat`**：同机双人。先弹主机窗口（攻方），3 秒后弹客户端窗口（守方），自动分队。

### 玩法规则（Conquest 三局铃声结算）

- 共 3 局 × 30 秒；两个据点开局都归守方。
- 站进据点半径约 5 秒翻转归属（红蓝进度条实时显示）；**占下的点稳定保持**，守方须亲自进区才能反夺（空区不回退）。
- 己方据点被敌方推进时，即使人在别处也会远程显示该进度条（带【远处】标记）。
- 每局倒计时归零时按该瞬间归属结算：两点全归一方 = 2:0，各占一个 = 1:1。
- 每局结束全员回出生点、据点复原；三局后比总分，平分 = DRAW。

### 编辑器内验证

1. UE 打开项目（默认加载 NewMap，其 WorldSettings 已锁定 `BP_GameMode_Conquest`）。
2. ▶ Play 即进入上述对局；双人：Play 下拉 → Number of Players = 2 → Play As Listen Server。

## 未来计划

战场 MiniGame 已于 2026-08-20 结项（与 cap 确认定版）。

## 已知环境注意事项

- **工具链**：UE4.27 + VS2019（MSVC v142）。编译前清掉 >1000 字符的环境变量（某些企业软件注入的 `ACC_PRODUCT_CONFIG_V3` 会撑爆 Windows 环境块 65535 限制）。
- **XGE / IncrediBuild**：授权失效会导致着色器编译卡死。已禁用 `XGEController.uplugin`（`EnabledByDefault=false`）回退本地编译。
- **Cook/打包环境**：精简 PATH 时必须保留 UE 引擎与 EnhancedInput 的 Binaries\Win64 目录，否则 Cook 阶段项目 DLL 加载失败；editor-only 代码（引用 UnrealEd/ObjectTools）必须 `#if WITH_EDITOR` 隔离并提供运行态 stub；动态 `LoadObject` 的资产需 `DirectoriesToAlwaysCook` 强制打包。
- **命名铁律**：项目名/路径/类名全程纯英文+数字（中文会导致 UnrealHeaderTool 乱码）。
- **默认地图必须设**：`DefaultEngine.ini` 的 `GameDefaultMap` + `EditorStartupMap` 都指向项目关卡，否则每次启动打开临时地图、动态 spawn 全丢。
- **地图备份**：禁止 raw copy `.umap`（PrimaryAssetID 重复）；用 `duplicate_level` 接口或 `MCPConfigureMap -SnapshotSource/-SnapshotDestination`（无界面、可覆盖需先删目标）。
- **DataTable 写盘**：`add_row` 保存用 `UPackage::SavePackage` 直存；若偶发保存失败，重开编辑器会话后重试（会话内保存状态可能被污染）。
- **改插件后必须重启 UE** 才加载新 DLL。

---

<details>
<summary>以下为上游 chongdashu/unreal-mcp 原始 README（保留作参考）</summary>

This project enables AI assistant clients like Cursor, Windsurf and Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP).

## ⚠️ Experimental Status

This project is currently in an **EXPERIMENTAL** state (upstream note). The API, functionality, and implementation details are subject to significant changes.

## 🌟 Overview

The Unreal MCP integration provides comprehensive tools for controlling Unreal Engine through natural language: actor management, blueprint development, blueprint node graph editing, and editor control — all accessible through natural language commands via AI assistants.

## 🧩 Components

### Sample Project (MCPGameProject) `MCPGameProject`
- Based off the Blank Project, but with the UnrealMCP plugin added.

### Plugin (UnrealMCP) `MCPGameProject/Plugins/UnrealMCP`
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server `Python/unreal_mcp_server.py`
- Manages TCP socket connections to the C++ plugin (port 55557)
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## License
MIT

## Questions
For questions, you can reach the upstream author on X/Twitter: [@chongdashu](https://www.x.com/chongdashu)
</details>
