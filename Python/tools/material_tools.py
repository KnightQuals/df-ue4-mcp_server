"""
Material Tools for Unreal MCP.

This module provides tools for creating and manipulating Material assets in
Unreal Engine, and assigning them to mesh components. It closes the MCP
boundary that previously prevented runtime recoloring (e.g. the BattleSector
anchor) by letting an AI create a material with a vector parameter and point
a mesh component at it.
"""

import logging
from typing import Dict, List, Any, Optional
from mcp.server.fastmcp import FastMCP, Context

# Get logger
logger = logging.getLogger("UnrealMCP")


def register_material_tools(mcp: FastMCP):
    """Register Material tools with the MCP server."""

    @mcp.tool()
    def create_material(
        ctx: Context,
        material_name: str,
        save_path: str = "/Game/Materials"
    ) -> Dict[str, Any]:
        """
        Create a new Material asset.

        Args:
            material_name: Name of the new material asset (no path, no extension).
            save_path: Content browser package path (default "/Game/Materials").

        Returns:
            Response with the created material's asset path.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "material_name": material_name,
                "save_path": save_path,
            }

            logger.info(f"Creating material with params: {params}")
            response = unreal.send_command("create_material", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Create material response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error creating material: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def add_vector_parameter(
        ctx: Context,
        material_name: str,
        param_name: str,
        default_color: Optional[List[float]] = None
    ) -> Dict[str, Any]:
        """
        Add a Vector Parameter expression to a material and connect it to Base Color.

        Args:
            material_name: Material asset name or full asset path
                           (e.g. "M_Anchor" or "/Game/Materials/M_Anchor.M_Anchor").
            param_name: Name of the vector parameter (e.g. "Color").
            default_color: Default color as [r, g, b, a] in 0..1 range.
                           Defaults to mid-gray [0.5, 0.5, 0.5, 1.0].

        Returns:
            Response with the material path and the connected parameter name.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "material_name": material_name,
                "param_name": param_name,
                "default_color": default_color or [0.5, 0.5, 0.5, 1.0],
            }

            logger.info(f"Adding vector parameter with params: {params}")
            response = unreal.send_command("add_vector_parameter", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Add vector parameter response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error adding vector parameter: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    @mcp.tool()
    def set_material_on_component(
        ctx: Context,
        actor_name: str,
        component_name: str,
        material_path: str,
        material_index: int = 0
    ) -> Dict[str, Any]:
        """
        Assign a material asset to a mesh component on a level actor.

        Args:
            actor_name: Name of the actor in the current level.
            component_name: Name (or substring) of the mesh component on that actor
                            (e.g. "AnchorMesh").
            material_path: Full asset path of the material
                           (e.g. "/Game/Materials/M_Anchor.M_Anchor").
            material_index: Material slot index to set (default 0).

        Returns:
            Response confirming the assignment.
        """
        from unreal_mcp_server import get_unreal_connection

        try:
            unreal = get_unreal_connection()
            if not unreal:
                logger.error("Failed to connect to Unreal Engine")
                return {"success": False, "message": "Failed to connect to Unreal Engine"}

            params = {
                "actor_name": actor_name,
                "component_name": component_name,
                "material_path": material_path,
                "material_index": int(material_index),
            }

            logger.info(f"Setting material on component with params: {params}")
            response = unreal.send_command("set_material_on_component", params)

            if not response:
                logger.error("No response from Unreal Engine")
                return {"success": False, "message": "No response from Unreal Engine"}

            logger.info(f"Set material on component response: {response}")
            return response

        except Exception as e:
            error_msg = f"Error setting material on component: {e}"
            logger.error(error_msg)
            return {"success": False, "message": error_msg}

    logger.info("Material tools registered successfully")
