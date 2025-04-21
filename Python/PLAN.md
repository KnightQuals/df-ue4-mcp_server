# Plan to Enable `uvx` Execution for Unreal MCP Server

This document outlines the steps taken to restructure the Python project and configure `pyproject.toml` so that the `unreal_mcp_server.py` script can be executed using `uvx`.

1.  **Update `pyproject.toml`:**
    *   Added `[build-system]` section specifying `setuptools` as the build backend.
    *   Defined `[project]` metadata including name (`unreal-mcp`), version, description, authors, Python requirement, and dependencies (`fastmcp`, etc.).
    *   Added `[project.optional-dependencies]` for development tools.
    *   Defined a script entry point under `[project.scripts]` mapping `unreal_mcp_server` to `unreal_mcp.unreal_mcp_server:main`.
    *   Configured `[tool.setuptools.packages.find]` to locate the `unreal_mcp` package.

2.  **Restructure Project into a Python Package:**
    *   Created the main package directory: `Python/unreal_mcp/`.
    *   Added an `__init__.py` file to `Python/unreal_mcp/` to mark it as a package.
    *   Created subdirectories for modules: `Python/unreal_mcp/tools/` and `Python/unreal_mcp/unreal_bridge/`.
    *   Added `__init__.py` files to `Python/unreal_mcp/tools/` and `Python/unreal_mcp/unreal_bridge/`.

3.  **Modify Core Server Script:**
    *   Moved the original `Python/unreal_mcp_server.py` to `Python/unreal_mcp/unreal_mcp_server.py`.
    *   Refactored the script to include a `main()` function containing the server setup and execution logic (`uvicorn.run`).
    *   Ensured the script uses relative imports where necessary (e.g., `from unreal_mcp.tool_registry import build_tool_registry`).

4.  **Relocate and Create Supporting Modules:**
    *   Moved `Python/tool_registry.py` to `Python/unreal_mcp/tool_registry.py`.
    *   Created a minimal implementation for the Unreal bridge communication in `Python/unreal_mcp/unreal_bridge/unrealbridge.py`.
    *   Created stub files for each tool category (`editor_tools.py`, `actor_tools.py`, etc.) within the `Python/unreal_mcp/tools/` directory, ensuring they import necessary components and define example tools using `@register_tool`.

5.  **Add Documentation:**
    *   Created `Python/README.md` explaining how to install the package (using `pip install -e .`) and run the server via the entry point (`unreal_mcp_server`) or `uvx`.

**Outcome:**

These changes transform the Python project into an installable package with a defined entry point. This allows tools like `pip` and `uvx` to install the package and execute the server script directly using the command `unreal_mcp_server` after installation or `uvx --from <repo> unreal_mcp_server`.
