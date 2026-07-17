"""
Debug Tools for Unreal MCP.

This module provides tools for reading the Unreal Engine output log, letting
an AI self-diagnose runtime issues (e.g. checking UE_LOG output from gameplay
classes) without needing a human to copy/paste log text.
"""

import logging
from typing import Dict, Any
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")


def register_debug_tools(mcp: FastMCP):
    """Register Debug tools with the MCP server."""

    @mcp.tool()
    def get_output_log(
        ctx: Context,
        filter: str = "",
        max_lines: int = 50
    ) -> Dict[str, Any]:
        """
        Read the Unreal Engine output log file.

        Args:
            filter: Optional keyword; only lines containing this substring are returned.
                    Leave empty to return the most recent lines unfiltered.
            max_lines: Maximum number of matching lines to return (default 50).

        Returns:
            Response with "lines" (list of log line strings) and "total" (match count).
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "filter": filter,
                "max_lines": int(max_lines),
            }

            logger.info(f"Getting output log with params: {params}")
            response = unreal.send_command("get_output_log", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Get output log response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error getting output log: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Debug tools registered successfully")
