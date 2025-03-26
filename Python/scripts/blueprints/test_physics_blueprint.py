#!/usr/bin/env python
"""
Test script for creating a blueprint with physics properties via MCP.
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
logger = logging.getLogger("TestPhysicsBlueprint")

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

def main():
    """Main function to test creating a physics blueprint."""
    try:
        # Connect to Unreal MCP server
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        try:
            # Step 1: Create a blueprint
            bp_params = {
                "name": "PhysicsBP",
                "parent_class": "Actor"
            }
            
            response = send_command(sock, "create_blueprint", bp_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to create blueprint: {response}")
                return
            
            # Check if blueprint already existed
            if response.get("result", {}).get("already_exists"):
                logger.info(f"Blueprint 'PhysicsBP' already exists, reusing it")
            else:
                logger.info("Blueprint created successfully!")
            
            # Step 2: Add a static mesh component
            component_params = {
                "blueprint_name": "PhysicsBP",
                "component_type": "StaticMesh",
                "component_name": "PhysicsCube",
                "location": [0.0, 0.0, 0.0],
                "rotation": [0.0, 0.0, 0.0],
                "scale": [1.0, 1.0, 1.0]
            }
            
            # Close and reopen connection for each command
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            response = send_command(sock, "add_component_to_blueprint", component_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add component: {response}")
                return
                
            logger.info("Component added successfully!")
            
            # Step 3: Set physics properties on the component
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            physics_params = {
                "blueprint_name": "PhysicsBP",
                "component_name": "PhysicsCube",
                "simulate_physics": True,
                "gravity_enabled": True,
                "mass": 10.0,  # A heavier cube
                "linear_damping": 0.1,  # Some damping to slow movement
                "angular_damping": 0.1  # Some rotational damping
            }
            
            response = send_command(sock, "set_physics_properties", physics_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to set physics properties: {response}")
                return
                
            logger.info("Physics properties set successfully!")
            
            # Step 4: Compile the blueprint
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            compile_params = {
                "blueprint_name": "PhysicsBP"
            }
            
            response = send_command(sock, "compile_blueprint", compile_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to compile blueprint: {response}")
                return
                
            logger.info("Blueprint compiled successfully!")
            
            # Step 5: Spawn an instance of the blueprint
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            spawn_params = {
                "blueprint_name": "PhysicsBP",
                "actor_name": "PhysicsBPInstance",
                "location": [0.0, 0.0, 200.0],  # 200 units up so it will fall
                "rotation": [0.0, 0.0, 45.0],   # Rotated 45 degrees to see physics in action
                "scale": [1.0, 1.0, 1.0]
            }
            
            response = send_command(sock, "spawn_blueprint_actor", spawn_params)
            
            # Fixed response check to handle nested structure
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to spawn blueprint actor: {response}")
                return
                
            logger.info("Blueprint actor spawned successfully!")
            logger.info("The physics cube should fall due to gravity.")
            
        finally:
            # Close the socket
            sock.close()
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 