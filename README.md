# Unreal Engine MCP Integration

This project enables AI assistants like Claude Desktop to control Unreal Engine through natural language using the Model Context Protocol (MCP). It provides a native C++ plugin implementation for seamless integration with Unreal Engine.

## Overview

The Unreal MCP integration allows you to:
- Create and manipulate actors in Unreal Engine scenes
- Control object transforms (position, rotation, scale)
- Query actor properties
- Manage level content
- Work with Unreal blueprints
- All through natural language commands via AI assistants

## Architecture

For a detailed architecture overview, see [Docs/Architecture.md](Docs/Architecture.md).

### C++ Plugin (UnrealMCP)
- Native TCP server for MCP communication
- Integrates with Unreal Editor subsystems
- Implements actor manipulation tools
- Handles command execution and response handling

### Python MCP Server
- Implemented in `unreal_mcp_server.py`
- Creates a bridge between AI assistants and Unreal Engine
- Manages TCP socket connections to the C++ plugin (port 55557)
- Handles command serialization and response parsing
- Provides error handling and connection management
- Loads and registers tool modules from the `tools` directory
- Uses the FastMCP library to implement the Model Context Protocol

## Directory Structure

- **MCPGameProject/** - Example Unreal project
  - **Plugins/UnrealMCP/** - C++ plugin source
    - **Source/UnrealMCP/** - Plugin source code
    - **UnrealMCP.uplugin** - Plugin definition

- **Python/** - Python server and tools
  - **tools/** - Tool modules for actor, editor, and blueprint operations
  - **scripts/** - Example scripts and demos

- **Docs/** - Comprehensive documentation
  - See [Docs/README.md](Docs/README.md) for documentation index

## Quick Start Guide

### Prerequisites
- Unreal Engine 5.5+
- Python 3.11+
- MCP Client (e.g., Claude Desktop, Cursor, Windsurf)

### C++ Plugin Setup

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
   - Open solution and build

### Python Server Setup

See [Python/README.md](Python/README.md) for detailed Python setup instructions, including:
- Setting up your Python environment
- Running the MCP server
- Using direct or server-based connections

### Configure MCP Client

Use the following JSON for your mcp configuration based on your MCP client:

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory",
        "<path/to/the/folder/PYTHON",
        "run",
        "unreal_mcp_server.py"
      ]
    }
  }
}
```

## Testing Your Setup

1. First, make sure you're in an empty level in Unreal Engine:
   - File > New Level > Empty Level

2. Open the Output Log in Unreal Engine:
   - Window > Developer Tools > Output Log

3. Try these commands
   - "Create a cube named TestCube at position 0,0,100"
   - "List all actors in the current level"
   - "Rotate TestCube by 45 degrees around the Z axis"
   - "Get the properties of TestCube"



## Contributing

Contributions welcome! See main repository guidelines.

## License

See LICENSE file for details.
