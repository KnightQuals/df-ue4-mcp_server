#!/usr/bin/env python
"""
Test script for blueprint variables and physics via MCP.
This script creates a physical obstacle with configurable variables controlling its behavior.
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
logger = logging.getLogger("TestPhysicsVariables")

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
    """Main function to test physics variables in blueprints."""
    try:
        # Step 1: Create blueprint for a physics-based obstacle
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        bp_params = {
            "name": "PhysicsObstacleBP",
            "parent_class": "Actor"
        }
        
        response = send_command(sock, "create_blueprint", bp_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to create blueprint: {response}")
            return
        
        # Check if blueprint already existed
        if response.get("result", {}).get("already_exists"):
            logger.info(f"Blueprint 'PhysicsObstacleBP' already exists, reusing it")
        else:
            logger.info("Blueprint created successfully!")
        
        # Close and reopen connection for each command
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 2: Add variables to control physics behavior
        var_params_list = [
            {
                "blueprint_name": "PhysicsObstacleBP",
                "variable_name": "Mass",
                "variable_type": "Float",
                "default_value": 10.0,
                "is_exposed": True
            },
            {
                "blueprint_name": "PhysicsObstacleBP",
                "variable_name": "RotationSpeed",
                "variable_type": "Float",
                "default_value": 100.0,
                "is_exposed": True
            },
            {
                "blueprint_name": "PhysicsObstacleBP",
                "variable_name": "BounceFactor",
                "variable_type": "Float",
                "default_value": 0.8,
                "is_exposed": True
            },
            {
                "blueprint_name": "PhysicsObstacleBP",
                "variable_name": "IsTrigger",
                "variable_type": "Boolean",
                "default_value": False,
                "is_exposed": True
            }
        ]
        
        for var_params in var_params_list:
            response = send_command(sock, "add_blueprint_variable", var_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add variable: {response}")
                return
                
            logger.info(f"Variable {var_params['variable_name']} added successfully!")
            
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
        
        # Step 3: Add a static mesh component for the obstacle
        component_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "component_type": "StaticMesh",
            "component_name": "ObstacleMesh",
            "location": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        }
        
        response = send_command(sock, "add_component_to_blueprint", component_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add component: {response}")
            return
            
        logger.info("Obstacle mesh component added successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 4: Set physics properties using the variables
        physics_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "component_name": "ObstacleMesh",
            "simulate_physics": True,
            "gravity_enabled": True
        }
        
        response = send_command(sock, "set_physics_properties", physics_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to set physics properties: {response}")
            return
            
        logger.info("Physics properties set successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 5: Add BeginPlay event node
        begin_play_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "event_type": "BeginPlay",
            "node_position": [0, 0]
        }
        
        response = send_command(sock, "add_blueprint_event_node", begin_play_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add BeginPlay event node: {response}")
            return
            
        logger.info("BeginPlay event node added successfully!")
        
        # Save the node ID for later connections
        begin_play_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 6: Add Tick event node
        tick_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "event_type": "Tick",
            "node_position": [0, 200]
        }
        
        response = send_command(sock, "add_blueprint_event_node", tick_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add Tick event node: {response}")
            return
            
        logger.info("Tick event node added successfully!")
        
        # Save the node ID for later connections
        tick_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 7: Add function node to set mesh physics settings from variables
        function_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "target": "ObstacleMesh",
            "function_name": "SetMassScale",
            "params": {
                "BoneName": "None",
                "InMassScale": 10.0  # This will be replaced by the Mass variable dynamically
            },
            "node_position": [300, 0]
        }
        
        response = send_command(sock, "add_blueprint_function_node", function_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add function node: {response}")
            return
            
        logger.info("SetMassScale function node added successfully!")
        
        # Save the node ID for later connections
        set_mass_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 8: Add function node to rotate the obstacle
        function_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "target": "ObstacleMesh",
            "function_name": "AddTorqueInRadians",
            "params": {
                "Torque": [0, 0, 100.0],  # This should be connected to the RotationSpeed variable
                "BoneName": "None",
                "bAccelChange": True
            },
            "node_position": [300, 200]
        }
        
        response = send_command(sock, "add_blueprint_function_node", function_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add function node: {response}")
            return
            
        logger.info("AddTorqueInRadians function node added successfully!")
        
        # Save the node ID for later connections
        add_torque_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 9: Connect BeginPlay to SetMassScale
        connect_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "source_node_id": begin_play_node_id,
            "source_pin": "Then",  # Execute pin on BeginPlay event
            "target_node_id": set_mass_node_id,
            "target_pin": "execute"  # Execute pin on function
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect nodes: {response}")
            return
            
        logger.info("BeginPlay connected to SetMassScale successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 10: Connect Tick to AddTorqueInRadians
        connect_params = {
            "blueprint_name": "PhysicsObstacleBP",
            "source_node_id": tick_node_id,
            "source_pin": "Then",  # Execute pin on Tick event
            "target_node_id": add_torque_node_id,
            "target_pin": "execute"  # Execute pin on function
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect nodes: {response}")
            return
            
        logger.info("Tick connected to AddTorqueInRadians successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 11: Compile the blueprint
        compile_params = {
            "blueprint_name": "PhysicsObstacleBP"
        }
        
        response = send_command(sock, "compile_blueprint", compile_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint compiled successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 12: Spawn multiple instances of the obstacle at different positions
        positions = [
            [100.0, 0.0, 200.0],
            [0.0, 100.0, 200.0],
            [-100.0, 0.0, 200.0],
            [0.0, -100.0, 200.0]
        ]
        
        for i, position in enumerate(positions):
            spawn_params = {
                "blueprint_name": "PhysicsObstacleBP",
                "actor_name": f"Obstacle_{i+1}",
                "location": position,
                "rotation": [0.0, 0.0, 45.0 * i],  # Different rotations
                "scale": [1.0, 1.0, 1.0]
            }
            
            response = send_command(sock, "spawn_blueprint_actor", spawn_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to spawn blueprint actor {i+1}: {response}")
                return
                
            logger.info(f"Obstacle {i+1} spawned successfully!")
            
            # Close and reopen connection
            if i < len(positions) - 1:  # Don't reopen if this is the last one
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(("127.0.0.1", 55557))
        
        logger.info("Physics obstacles created successfully!")
        logger.info("The obstacles should start rotating due to the Tick event connection")
        
        # Close final connection
        sock.close()
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 