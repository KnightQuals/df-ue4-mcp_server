import subprocess, os
env = os.environ.copy()
# 删所有 >1000 字符的变量
removed = []
for k in list(env.keys()):
    if len(str(env[k])) > 1000:
        removed.append((k, len(env[k])))
        del env[k]
print("删除的超长变量:", removed)
# 计算环境块总长
total = sum(len(k) + len(str(v)) + 2 for k, v in env.items())
print(f"删后环境块总长: {total} 字节 (限制 65535)")
build_bat = r"C:\Program Files\Epic Games\UE_4.27\Engine\Build\BatchFiles\Build.bat"
uproject = r"D:\dev\df-ue4-mcp_server\MCPGameProject\MCPGameProject.uproject"
with open(r"D:\dev\df-ue4-mcp_server\build.log", "w", encoding="utf-8", errors="ignore") as f:
    r = subprocess.run([build_bat, "MCPGameProjectEditor", "Win64", "Development",
                        f"-Project={uproject}", "-WaitMutex", "-FromMSBuild"],
                       cwd=r"D:\dev\df-ue4-mcp_server\MCPGameProject",
                       stdout=f, stderr=subprocess.STDOUT, env=env)
print("EXIT=", r.returncode)
