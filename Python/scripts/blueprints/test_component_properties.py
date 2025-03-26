#!/usr/bin/env python
"""
Test script for setting custom properties on blueprint components via MCP.
"""

import sys
import os
import time
import socket
import json
import logging
from typing import Dict, Any, Optional

# Add the parent directory to the path so we can import the server module
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("TestComponentProperties")

def send_command(sock: socket.socket, command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal MCP server and get the response."""
    try:
        # Create command object
        command_obj = {
            "type": command,
            "params": params
        }
        
        # Convert to JSON and send
        command_json = json.dumps(command_obj)
        logger.info(f"Sending command: {command_json}")
        sock.sendall(command_json.encode('utf-8'))
        
        # Receive response
        chunks = []
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            chunks.append(chunk)
            
            # Try parsing to see if we have a complete response
            try:
                data = b''.join(chunks)
                json.loads(data.decode('utf-8'))
                # If we can parse it, we have the complete response
                break
            except json.JSONDecodeError:
                # Not a complete JSON object yet, continue receiving
                continue
        
        # Parse response
        data = b''.join(chunks)
        response = json.loads(data.decode('utf-8'))
        logger.info(f"Received response: {response}")
        return response
        
    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return None

def create_new_connection():
    """Create a new connection to the Unreal MCP server."""
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(("127.0.0.1", 55557))
    return sock

def main():
    """Main function to test setting properties on blueprint components."""
    try:
        # Connect to Unreal MCP server
        sock = create_new_connection()
        
        try:
            # Step 1: Create a blueprint
            bp_params = {
                "name": "PropertiesBP",
                "parent_class": "Actor"
            }
            
            response = send_command(sock, "create_blueprint", bp_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to create blueprint: {response}")
                return
            
            # Check if blueprint already existed
            if response.get("result", {}).get("already_exists"):
                logger.info(f"Blueprint 'PropertiesBP' already exists, reusing it")
            else:
                logger.info("Blueprint created successfully!")
            
            # Step 2: Add a static mesh component
            sock.close()
            sock = create_new_connection()
            
            component_params = {
                "blueprint_name": "PropertiesBP",
                "component_type": "StaticMesh",
                "component_name": "MainMesh",
                "location": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }
            
            response = send_command(sock, "add_component_to_blueprint", component_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add component: {response}")
                return
                
            logger.info("Static mesh component added successfully!")
            
            # Step 3: Add a point light component
            sock.close()
            sock = create_new_connection()
            
            light_params = {
                "blueprint_name": "PropertiesBP",
                "component_type": "PointLight",
                "component_name": "PointLight",
                "location": [0.0, 0.0, 50.0],  # 50 units above the mesh
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }
            
            response = send_command(sock, "add_component_to_blueprint", light_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add point light component: {response}")
                return
                
            logger.info("Point light component added successfully!")
            
            # Step 4: Set custom properties on the light
            sock.close()
            sock = create_new_connection()
            
            # Set light color to red
            color_params = {
                "blueprint_name": "PropertiesBP",
                "component_name": "PointLight",
                "property_name": "LightColor",
                "property_value": {"R": 255, "G": 0, "B": 0, "A": 255}
            }
            
            response = send_command(sock, "set_component_property", color_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to set light color: {response}")
                return
                
            logger.info("Light color set successfully!")
            
            # Set light intensity
            sock.close()
            sock = create_new_connection()
            
            intensity_params = {
                "blueprint_name": "PropertiesBP",
                "component_name": "PointLight",
                "property_name": "Intensity",
                "property_value": 5000.0  # Bright light
            }
            
            response = send_command(sock, "set_component_property", intensity_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to set light intensity: {response}")
                return
                
            logger.info("Light intensity set successfully!")
            
            # Set light attenuation radius
            sock.close()
            sock = create_new_connection()
            
            radius_params = {
                "blueprint_name": "PropertiesBP",
                "component_name": "PointLight",
                "property_name": "AttenuationRadius",
                "property_value": 500.0  # Large radius
            }
            
            response = send_command(sock, "set_component_property", radius_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to set light radius: {response}")
                return
                
            logger.info("Light radius set successfully!")
            
            # Step 5: Compile the blueprint
            sock.close()
            sock = create_new_connection()
            
            compile_params = {
                "blueprint_name": "PropertiesBP"
            }
            
            response = send_command(sock, "compile_blueprint", compile_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to compile blueprint: {response}")
                return
                
            logger.info("Blueprint compiled successfully!")
            
            # Step 6: Spawn an instance of the blueprint
            sock.close()
            sock = create_new_connection()
            
            spawn_params = {
                "blueprint_name": "PropertiesBP",
                "actor_name": "PropertiesBPInstance",
                "location": [0.0, 0.0, 100.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }
            
            response = send_command(sock, "spawn_blueprint_actor", spawn_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to spawn blueprint actor: {response}")
                return
                
            logger.info("Blueprint actor spawned successfully!")
            logger.info("You should see a cube with a bright red light above it.")
            
        finally:
            # Close the socket
            sock.close()
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 