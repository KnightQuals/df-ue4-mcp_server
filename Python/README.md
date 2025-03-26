# Unreal MCP

Python bridge for interacting with Unreal Engine 5.5 using the Model Context Protocol (MCP).

## Setup

1. Make sure Python 3.10+ is installed
2. Install dependencies: `pip install -r requirements.txt`

## Running the Bridge

### Option 1: Direct Connection to Unreal Engine

This approach uses direct socket communication with the Unreal Engine plugin:

```bash
# The Unreal Engine plugin must be loaded in the editor
# No Python server needs to be running for this approach
python scripts/test_cube.py
```

### Option 2: MCP Server (Recommended)

This approach uses the MCP server as an intermediate layer:

1. Start the MCP server:
```bash
python run_mcp_server.py
```

2. In a separate terminal, run client scripts:
```bash
python scripts/test_cube_mcp.py
```

## Architecture

The system has two operation modes:

1. **Direct Socket Connection**:
   - Client → Port 55557 → Unreal Engine Plugin
   - Simple, no middleware
   - Limited to raw socket operations

2. **MCP Server (Recommended)**:
   - Client → Port 19998 → Python MCP Server → Port 55557 → Unreal Engine Plugin
   - Provides a standardized API
   - Supports async/await pattern
   - Better error handling and validation

## Available Tools

The following tools are available via the MCP server:

### Actor Management

- `get_actors_in_level()`: Get all actors in the current level
- `find_actors_by_name(pattern)`: Find actors matching a name pattern
- `create_actor(name, type, location, rotation, scale)`: Create a new actor
- `delete_actor(name)`: Delete an actor by name
- `set_actor_transform(name, location, rotation, scale)`: Change actor transform
- `get_actor_properties(name)`: Get all properties of an actor

### Editor Tools

- `focus_viewport(target, location, distance, orientation)`: Focus the viewport on an actor or location
- `take_screenshot(filename, show_ui, resolution)`: Capture a screenshot

## Example Usage

```python
import asyncio
from mcp.client.fastmcp_client import FastMCPClient

async def main():
    # Connect to MCP server
    client = FastMCPClient("127.0.0.1", 19998)
    
    # Create a cube
    cube = await client.call_tool("create_actor", {
        "name": "MyCube",
        "type": "CUBE",
        "location": [0, 0, 100],
        "rotation": [0, 45, 0],
        "scale": [1, 1, 1]
    })
    
    # Get all actors
    actors = await client.call_tool("get_actors_in_level")
    print(f"Found {len(actors)} actors")
    
    # Close connection
    client.close()

if __name__ == "__main__":
    asyncio.run(main())
```

## Troubleshooting

- If direct connection isn't working, ensure the Unreal Engine plugin is loaded and the editor is running
- If MCP server connection fails, make sure to run `run_mcp_server.py` first
- Check logs in `unreal_mcp.log` and `unreal_mcp_network.log` for detailed error information

## Development

To add new tools, modify the `UnrealMCPBridge.py` file to add new command handlers, and update the `unreal_mcp_server.py` file to expose them through the HTTP API. 