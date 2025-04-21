#!/usr/bin/env python3
import sys
import warnings
from unreal_mcp.server import mcp

def main():
    warnings.warn(
        "\nDEPRECATION WARNING: Running unreal_mcp_server.py directly is deprecated.\n"
        "Please update your MCP configuration to use the module entry point instead:\n\n"
        "# Option 1: Direct from repository (Recommended)\n"
        '{\n  "mcpServers": {\n    "unrealMCP": {\n'
        '      "command": "uvx",\n'
        '      "args": ["--from", "git+https://github.com/chongdashu/unreal-mcp#subdirectory=Python", "unreal_mcp_server"]\n'
        "    }\n  }\n}\n\n"
        "# Option 2: Local installation\n"
        '{\n  "mcpServers": {\n    "unrealMCP": {\n'
        '      "command": "uv",\n'
        '      "args": ["--directory", "<path/to/repo>/Python", "run", "unreal_mcp_server"]\n'
        "    }\n  }\n}",
        DeprecationWarning,
        stacklevel=2
    )
    return mcp.run()

if __name__ == "__main__":
    sys.exit(main())
