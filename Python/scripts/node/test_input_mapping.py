#!/usr/bin/env python
"""
Test script for blueprint input mapping via MCP.
This script creates input mappings and connects them to functions in a Blueprint.
"""

import sys
import os
import time
import socket
import json
import logging
from typing import Dict, List, Any, Optional

# Add the parent directory to the path so we can import the server module
sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

# Set up logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger("TestInputMapping")

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

def setup_input_mapping(sock: socket.socket, action_name: str, key: str, input_type: str = "Action") -> bool:
    """Helper function to set up an input mapping."""
    input_params = {
        "action_name": action_name,
        "key": key,
        "input_type": input_type
    }
    
    response = send_command(sock, "create_input_mapping", input_params)
    
    success = (response and 
               response.get("status") == "success" and 
               response.get("result", {}).get("success"))
    
    if success:
        logger.info(f"Input mapping '{action_name}' created with key '{key}'")
    else:
        logger.error(f"Failed to create input mapping: {response}")
    
    return success

def main():
    """Main function to test input mappings in blueprints."""
    try:
        # Step 1: Create a controller blueprint
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        bp_params = {
            "name": "InputControllerBP",
            "parent_class": "Actor"
        }
        
        response = send_command(sock, "create_blueprint", bp_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to create blueprint: {response}")
            return
        
        # Check if blueprint already existed
        if response.get("result", {}).get("already_exists"):
            logger.info(f"Blueprint 'InputControllerBP' already exists, reusing it")
        else:
            logger.info("Controller blueprint created successfully!")
        
        # Close and reopen connection for each command
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 2: Add variables to track state
        var_params_list = [
            {
                "blueprint_name": "InputControllerBP",
                "variable_name": "Score",
                "variable_type": "Integer",
                "default_value": 0,
                "is_exposed": True
            },
            {
                "blueprint_name": "InputControllerBP",
                "variable_name": "IsGameActive",
                "variable_type": "Boolean",
                "default_value": True,
                "is_exposed": True
            },
            {
                "blueprint_name": "InputControllerBP",
                "variable_name": "PlayerName",
                "variable_type": "String",
                "default_value": "Player1",
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
        
        # Step 3: Set up input mappings for a simple game controller
        input_mappings = [
            ("Jump", "SpaceBar", "Action"),
            ("Pause", "P", "Action"),
            ("Restart", "R", "Action"),
            ("MoveForward", "W", "Axis"),
            ("MoveRight", "D", "Axis")
        ]
        
        for action_name, key, input_type in input_mappings:
            success = setup_input_mapping(sock, action_name, key, input_type)
            if not success:
                return
                
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
        
        # Step 4: Add event nodes for BeginPlay and input actions
        event_node_ids = {}
        
        # BeginPlay event
        begin_play_params = {
            "blueprint_name": "InputControllerBP",
            "event_type": "BeginPlay",
            "node_position": [0, 0]
        }
        
        response = send_command(sock, "add_blueprint_event_node", begin_play_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add BeginPlay event node: {response}")
            return
            
        logger.info("BeginPlay event node added successfully!")
        event_node_ids["BeginPlay"] = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 5: Add function nodes for different actions
        function_node_ids = {}
        
        # Print String function for BeginPlay
        function_params = {
            "blueprint_name": "InputControllerBP",
            "target": "self",
            "function_name": "PrintString",
            "params": {
                "InString": "Input Controller Initialized",
                "Duration": 5.0
            },
            "node_position": [250, 0]
        }
        
        response = send_command(sock, "add_blueprint_function_node", function_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add PrintString function node: {response}")
            return
            
        logger.info("PrintString function node added successfully!")
        function_node_ids["PrintInit"] = response.get("result", {}).get("node_id")
        
        # For each action, add a event node and function node
        action_positions = {
            "Jump": [0, 150],
            "Pause": [0, 300],
            "Restart": [0, 450],
            "MoveForward": [0, 600],
            "MoveRight": [0, 750]
        }
        
        function_positions = {
            "Jump": [250, 150],
            "Pause": [250, 300],
            "Restart": [250, 450],
            "MoveForward": [250, 600],
            "MoveRight": [250, 750]
        }
        
        # Since we can't directly create InputAction nodes, we'll simulate with BeginPlay
        for action_name in ["Jump", "Pause", "Restart"]:
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            # Create placeholder event node
            event_params = {
                "blueprint_name": "InputControllerBP",
                "event_type": "BeginPlay",  # Using BeginPlay as a placeholder for InputAction
                "node_position": action_positions[action_name]
            }
            
            response = send_command(sock, "add_blueprint_event_node", event_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add event node for {action_name}: {response}")
                return
                
            logger.info(f"Event node for {action_name} added (simulated)")
            event_node_ids[action_name] = response.get("result", {}).get("node_id")
            
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            # Create function node to print what action was performed
            function_params = {
                "blueprint_name": "InputControllerBP",
                "target": "self",
                "function_name": "PrintString",
                "params": {
                    "InString": f"{action_name} Action Triggered",
                    "Duration": 2.0
                },
                "node_position": function_positions[action_name]
            }
            
            response = send_command(sock, "add_blueprint_function_node", function_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add function node for {action_name}: {response}")
                return
                
            logger.info(f"Function node for {action_name} added successfully!")
            function_node_ids[action_name] = response.get("result", {}).get("node_id")
        
        # Step 6: Connect nodes
        for action_name in ["BeginPlay"] + list(action_positions.keys())[:3]:  # BeginPlay + first 3 actions
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            # Connect appropriate function based on event type
            if action_name == "BeginPlay":
                target_function = "PrintInit"
            else:
                target_function = action_name
                
            connect_params = {
                "blueprint_name": "InputControllerBP",
                "source_node_id": event_node_ids[action_name],
                "source_pin": "Then",  # Execute pin on event
                "target_node_id": function_node_ids[target_function],
                "target_pin": "execute"  # Execute pin on function
            }
            
            response = send_command(sock, "connect_blueprint_nodes", connect_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to connect nodes for {action_name}: {response}")
                return
                
            logger.info(f"Connected {action_name} event to function successfully!")
        
        # Step 7: Compile the blueprint
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        compile_params = {
            "blueprint_name": "InputControllerBP"
        }
        
        response = send_command(sock, "compile_blueprint", compile_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint compiled successfully!")
        
        # Step 8: Spawn the controller in the level
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        spawn_params = {
            "blueprint_name": "InputControllerBP",
            "actor_name": "InputController",
            "location": [0.0, 0.0, 100.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        }
        
        response = send_command(sock, "spawn_blueprint_actor", spawn_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to spawn blueprint actor: {response}")
            return
            
        logger.info("Input controller spawned successfully!")
        logger.info("The controller will run the BeginPlay event and show a message.")
        logger.info("The following input mappings have been set up:")
        
        for action_name, key, input_type in input_mappings:
            logger.info(f" - {action_name}: {key} ({input_type})")
        
        # Close final connection
        sock.close()
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 