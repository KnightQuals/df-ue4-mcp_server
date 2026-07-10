#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
W3 块③ 路B orchestrator —— 完整无人值守闭环 + 实时显示 cli-agent 工作进度

把"关UE → 调cli-agent写码 → 编译 → 报错自修循环 → 起UE → 等55557 → 调cli-agent用MCP建蓝图spawn"
全缝死成一个脚本。解决 cli-agent 自己起 UE 卡死的问题（起UE移到外层脚本）。

核心特性：用 stream-json 解析实时打印 cli-agent 的 thinking/工具调用/结果，让用户看到进度。

用法:
    python orchestrator.py "<任务描述>"
    例: python orchestrator.py "新建守方基地 ABattleDefenderCamp，简单AActor带Cube外形"
"""
import subprocess, socket, time, json, sys, os, re

# ===== 配置 =====
CLI_AGENT = r"C:\Users\dev\.workbuddy\binaries\node\versions\22.22.2\agent-cli.cmd"
PROJECT = r"D:\dev\ue4-mcp-port"
UPROJECT = r"D:\dev\ue4-mcp-port\MCPGameProject\MCPGameProject.uproject"
UE_EXE = r"C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor.exe"
SESSION = "00000000-0000-0000-0000-000000000000"  # 7/3 长期对话，保持上下文连贯
MODEL = "GLM-5.2"
MCP_CONFIG = os.path.join(PROJECT, ".mcp.json")
BUILD_LOG = os.path.join(PROJECT, "build.log")


def log(msg):
    """实时打印（flush=True 确保不缓冲）"""
    print(msg, flush=True)


def run_cli-agent_stream(prompt, use_mcp=False, max_seconds=420):
    """
    跑 cli-agent -p，实时解析 stream-json 打印 thinking/工具/结果，返回最终文本。
    stream-json 每行一个 JSON 事件。
    """
    cmd = [CLI_AGENT, "--resume", SESSION, "-p", prompt,
           "--model", MODEL, "--dangerously-skip-permissions",
           "--output-format", "stream-json", "--verbose"]
    if use_mcp:
        cmd += ["--mcp-config", MCP_CONFIG, "--allowedTools", "mcp__unrealMCP"]

    log(f"\n{'='*60}")
    log(f"[CLI_AGENT 任务] {prompt[:100]}")
    log(f"{'='*60}")

    proc = subprocess.Popen(cmd, cwd=PROJECT, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="ignore")
    final_text = ""
    start = time.time()

    for line in proc.stdout:
        line = line.strip()
        if not line:
            continue
        # 尝试解析 stream-json
        try:
            ev = json.loads(line)
            t = ev.get("type", "")
            if t == "assistant":
                msg = ev.get("message", {})
                for block in msg.get("content", []):
                    btype = block.get("type", "")
                    if btype == "thinking":
                        txt = block.get("thinking", "")[:200]
                        log(f"  💭 [思考] {txt}")
                    elif btype == "text":
                        txt = block.get("text", "")
                        final_text += txt
                        log(f"  💬 [回复] {txt[:300]}")
                    elif btype == "tool_use":
                        name = block.get("name", "")
                        inp = str(block.get("input", {}))[:150]
                        log(f"  🔧 [工具] {name}({inp})")
            elif t == "user":
                msg = ev.get("message", {})
                for block in msg.get("content", []):
                    if block.get("type") == "tool_result":
                        content = str(block.get("content", ""))[:250]
                        log(f"  ✅ [结果] {content}")
            elif t == "result":
                final_text = ev.get("result", final_text)
                log(f"  🏁 [最终] {str(ev.get('result',''))[:300]}")
            elif t == "system":
                # 系统事件（init等），简略
                pass
            else:
                log(f"  [{t}] {str(ev)[:150]}")
        except json.JSONDecodeError:
            # 非 JSON 行（如更新提示），简略
            if "Update available" not in line and line:
                log(f"  [raw] {line[:150]}")

        if time.time() - start > max_seconds:
            proc.terminate()
            log("  ⏰ [超时终止]")
            break

    proc.wait()
    rc = proc.returncode
    log(f"  [cli-agent 退出码={rc}]")
    return final_text


def kill_ue():
    """关闭 UE（dll 锁，编译前必须关）"""
    log("\n🔵 [STEP] 关闭 UE4Editor...")
    subprocess.run(["taskkill", "/f", "/im", "UE4Editor.exe"],
                   capture_output=True, text=True, encoding="gbk", errors="ignore")
    time.sleep(2)


def compile_project():
    """跑 Build.bat 编译，返回 (是否成功, build.log内容)。传精简 env(只留系统PATH)避免 PATH 超长致 UnrealBuildTool 崩。UE Build.bat 内部自定位 VS 工具链。"""
    log("\n🔵 [STEP] 编译项目（Build.bat + 精简 env）...")
    build_bat = r"C:\Program Files\Epic Games\UE_4.27\Engine\Build\BatchFiles\Build.bat"
    # copy 父环境但删超长变量(>1000字符)，避免环境块超 65535 字节致 UnrealBuildTool 崩
    # (元凶: ACC_PRODUCT_CONFIG_V3 347266字符 + PATH 1815字符，删后环境块~6800字节)
    env = os.environ.copy()
    for k in list(env.keys()):
        if len(str(env[k])) > 1000:
            del env[k]
    with open(BUILD_LOG, "w", encoding="utf-8", errors="ignore") as logf:
        subprocess.run([build_bat, "MCPGameProjectEditor", "Win64", "Development",
                        f"-Project={UPROJECT}", "-WaitMutex", "-FromMSBuild"],
                       cwd=os.path.join(PROJECT, "MCPGameProject"),
                       stdout=logf, stderr=subprocess.STDOUT,
                       env=env, encoding="gbk", errors="ignore")
    try:
        with open(BUILD_LOG, encoding="utf-8", errors="ignore") as f:
            log_content = f.read()
    except Exception as e:
        log(f"  ❌ 读 build.log 失败: {e}")
        return False, ""

    ok = "Total execution time" in log_content and "error C" not in log_content
    # 打印编译摘要
    errors = [l for l in log_content.split("\n") if "error C" in l]
    if ok:
        # 提取 Total execution time
        m = re.search(r"Total execution time: ([\d.]+) seconds", log_content)
        t = m.group(1) if m else "?"
        log(f"  ✅ 编译成功（{t}秒，0 error）")
    else:
        log(f"  ❌ 编译失败（{len(errors)} 个 error）")
        for e in errors[:5]:
            log(f"     {e.strip()[:200]}")
    return ok, log_content


def start_ue_wait_port(max_wait=180):
    """后台起 UE，轮询 55557 就绪"""
    log("\n🔵 [STEP] 启动 UE4.27（后台）...")
    subprocess.Popen([UE_EXE, UPROJECT], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)  # 后台起，不等待，不污染日志
    log("  等 55557 端口就绪...")
    for i in range(max_wait // 10):
        try:
            s = socket.socket()
            s.settimeout(2)
            s.connect(("127.0.0.1", 55557))
            s.close()
            log(f"  ✅ 55557 就绪（等待了约 {i*10} 秒）")
            return True
        except Exception:
            if i % 3 == 0:
                log(f"  ⏳ 等待中... ({i*10}s)")
            time.sleep(10)
    log("  ❌ UE 未就绪超时")
    return False


def extract_errors(log_content):
    """从 build.log 提取 error C 行"""
    return [l.strip() for l in log_content.split("\n") if "error C" in l]


def main():
    task = sys.argv[1] if len(sys.argv) > 1 else "给 BattleSectorAnchor 加占领进度 UI"

    log("🚀 " + "=" * 58)
    log(f"🚀 路B orchestrator 启动 — 任务: {task}")
    log("🚀 " + "=" * 58)

    # ===== 阶段1: cli-agent 写码（不挂 MCP，只写 C++）=====
    write_prompt = (
        f"自主任务（写码阶段）：{task}。"
        f"在 Source/MCPGameProject/ 下创建或修改相应 .h/.cpp 文件。"
        f"标准 UE4.27 反射宏(UCLASS/GENERATED_BODY/UPROPERTY/UFUNCTION) + MCPGAMEPROJECT_API。"
        f"只写代码，不要编译，不要调 MCP。写完简述你改了什么。"
    )
    run_cli-agent_stream(write_prompt, use_mcp=False)

    # ===== 阶段2: 编译 + 报错自修循环（最多3轮）=====
    kill_ue()
    ok = False
    for attempt in range(1, 4):
        log(f"\n🔵 [STEP] 编译第 {attempt} 轮...")
        ok, log_content = compile_project()
        if ok:
            break
        # 有错，让 cli-agent 修
        errors = extract_errors(log_content)
        if not errors:
            # 编译失败但无 error C（环境/工具问题，如 PATH 超长），cli-agent 修不了，直接退出
            log("  ⚠️ 编译失败但无 error C（可能是环境/工具问题），cli-agent 修不了，退出闭环。")
            log(f"  build.log 末尾: ...{log_content[-400:]}")
            break
        fix_prompt = (
            f"编译报错，请修复 Source/MCPGameProject/ 下的代码。报错如下：\n"
            + "\n".join(errors[:20])
            + "\n\n分析报错原因，修改对应的 .h/.cpp，只改代码不要编译。改完简述修了什么。"
        )
        run_cli-agent_stream(fix_prompt, use_mcp=False)

    if not ok:
        log("\n❌ 编译 3 轮未通过，闭环失败。请检查 build.log。")
        sys.exit(1)

    # ===== 阶段3: 起 UE + 等 55557 =====
    if not start_ue_wait_port():
        log("\n❌ UE 未就绪，闭环失败。")
        sys.exit(1)

    # ===== 阶段4: cli-agent 用 MCP 建蓝图 + spawn =====
    mcp_prompt = (
        f"UE已启动且55557就绪。用 mcp__unrealMCP 工具完成刚才所写代码的引擎落地："
        f"根据你刚才写的 C++ 类，建对应的蓝图(Blueprint)继承它→编译蓝图→spawn一个实例进场景(位置自选合理)。"
        f"每步看返回status是否success，失败分析重试。完成后简述三步结果。"
    )
    run_cli-agent_stream(mcp_prompt, use_mcp=True)

    log("\n🎉 " + "=" * 58)
    log("🎉 路B orchestrator 完成 — 完整闭环跑通")
    log("🎉 " + "=" * 58)


if __name__ == "__main__":
    main()
