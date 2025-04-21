# Unreal MCP

Python bridge for interacting with Unreal Engine 5.5 using the Model Context Protocol (MCP).

## Setup

### 1. Install `uv` if you haven't already:
```bash
# On Unix/macOS:
curl -LsSf https://astral.sh/uv/install.sh | sh

# On Windows (PowerShell):
powershell -ExecutionPolicy ByPass -c "irm https://astral.sh/uv/install.ps1 | iex"
```

### 2. Configuring your MCP Client

Use the following configuration in your MCP client's config file:

```json
{
  "mcpServers": {
    "unrealMCP": {
      "command": "uv",
      "args": [
        "--directory", 
        "<path/to/your/repo/Python>", 
        "run", 
        "unreal_mcp_server"
      ]
    }
  }
}
```

You can find the value of `<path/to/your/repo/>` by running `pwd` in the root directory of the repository.

## Configuration Locations

The location of your MCP configuration file depends on your MCP client. Common locations include:

- Cursor: `~/.cursor/mcp_config.json`
- Claude Desktop: `~/.config/claude/mcp_config.json`
- Windsurf: `~/.codeium/windsurf/mcp_config.json`
- Other clients: Refer to your client's documentation

## Testing Scripts

There are several scripts in the [scripts](./scripts) folder. They are useful for testing the tools and the Unreal Bridge via a direct connection. This means that you do not need to have an MCP Server running.

You should make sure you have installed dependencies and/or are running in the `uv` virtual environment in order for the scripts to work.

## Troubleshooting

- Make sure Unreal Engine editor is loaded loaded and running before running the server.
- Check logs in `unreal_mcp.log` for detailed error information

## Development

To add new tools, modify the `UnrealMCPBridge.py` file to add new command handlers, and update the `unreal_mcp_server.py` file to expose them through the HTTP API. 