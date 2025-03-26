#!/usr/bin/env python
"""
Test script for blueprint node tools via MCP.
This script creates a simple flapping bird Blueprint with basic physics and input handling.
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
logger = logging.getLogger("TestBlueprintNodes")

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
    """Main function to test blueprint node tools."""
    try:
        # Step 1: Create a blueprint for the bird
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        bp_params = {
            "name": "BirdBP",
            "parent_class": "Pawn"
        }
        
        response = send_command(sock, "create_blueprint", bp_params)
        
        # Fixed response check to handle nested structure
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to create blueprint: {response}")
            return
        
        # Check if blueprint already existed
        if response.get("result", {}).get("already_exists"):
            logger.info(f"Blueprint 'BirdBP' already exists, reusing it")
        else:
            logger.info("Blueprint created successfully!")
        
        # Close and reopen connection for each command
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 2: Add a static mesh component
        component_params = {
            "blueprint_name": "BirdBP",
            "component_type": "StaticMesh",
            "component_name": "BirdMesh",
            "location": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [0.5, 0.5, 0.5]  # Smaller bird
        }
        
        response = send_command(sock, "add_component_to_blueprint", component_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add component: {response}")
            return
            
        logger.info("Bird mesh component added successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 3: Add physics properties to the mesh
        physics_params = {
            "blueprint_name": "BirdBP",
            "component_name": "BirdMesh",
            "simulate_physics": True,
            "gravity_enabled": True,
            "mass": 2.0,  # Light bird
            "linear_damping": 0.5,  # Some air resistance
            "angular_damping": 0.5  # Prevent too much spinning
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
        
        # Step 4: Add variables for tracking bird state
        var_params = {
            "blueprint_name": "BirdBP",
            "variable_name": "FlapStrength",
            "variable_type": "Float",
            "default_value": 500.0,
            "is_exposed": True
        }
        
        response = send_command(sock, "add_blueprint_variable", var_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add variable: {response}")
            return
            
        logger.info("FlapStrength variable added successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 5: Create input mapping for flap action
        input_params = {
            "action_name": "Flap",
            "key": "SpaceBar",
            "input_type": "Action"
        }
        
        response = send_command(sock, "create_input_mapping", input_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to create input mapping: {response}")
            return
            
        logger.info("Flap input mapping created successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 6: Create BeginPlay event node - check if it exists first
        begin_play_response = None
        
        # First try to find existing BeginPlay nodes
        find_begin_play_params = {
            "blueprint_name": "BirdBP",
            "node_type": "Event",
            "event_type": "BeginPlay"
        }
        
        response = send_command(sock, "find_blueprint_nodes", find_begin_play_params)
        
        if response and response.get("status") == "success" and response.get("result", {}).get("success"):
            # Look for BeginPlay nodes in the response
            nodes = response.get("result", {}).get("nodes", [])
            if nodes:
                logger.info(f"Found existing BeginPlay node, reusing it")
                # Use the first BeginPlay node we find
                begin_play_node_id = nodes[0].get("node_id")
                begin_play_response = {"result": {"success": True, "node_id": begin_play_node_id}}
        
        # Only create a new BeginPlay node if we didn't find one
        if begin_play_response is None:
            begin_play_params = {
                "blueprint_name": "BirdBP",
                "event_type": "BeginPlay",
                "node_position": [0, 0]
            }
            
            begin_play_response = send_command(sock, "add_blueprint_event_node", begin_play_params)
            
            if not begin_play_response or begin_play_response.get("status") != "success" or not begin_play_response.get("result", {}).get("success"):
                logger.error(f"Failed to add BeginPlay event node: {begin_play_response}")
                return
                
            logger.info("BeginPlay event node added successfully!")
            
        # Save the node ID for later connections
        begin_play_node_id = begin_play_response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 7: Create input action event node
        input_action_params = {
            "blueprint_name": "BirdBP",
            "action_name": "Flap",  # Use the action name we created earlier
            "node_position": [0, 200]
        }
        
        # Create the InputAction event node using the dedicated function
        response = send_command(sock, "add_blueprint_input_action_node", input_action_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add Input action node: {response}")
            return
            
        logger.info("Input action node added successfully")
        
        # Save the node ID for later connections
        input_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Compile the blueprint before trying to get component references
        compile_params = {
            "blueprint_name": "BirdBP"
        }
        
        response = send_command(sock, "compile_blueprint", compile_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to compile blueprint before getting component reference: {response}")
            return
            
        logger.info("Blueprint compiled successfully before getting component reference!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 8: Add a get component reference node for BirdMesh 
        get_component_params = {
            "blueprint_name": "BirdBP",
            "component_name": "BirdMesh",
            "node_position": [200, 200]
        }
        
        response = send_command(sock, "add_blueprint_get_self_component_reference", get_component_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add component reference node: {response}")
            
            # Try alternative approach using get component by name
            logger.info("Trying alternative approach to get component reference...")
            
            # First, add a 'Get Self' node
            get_self_params = {
                "blueprint_name": "BirdBP",
                "node_position": [200, 200]
            }
            
            response = send_command(sock, "add_blueprint_self_reference", get_self_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add self reference node: {response}")
                return
                
            get_self_node_id = response.get("result", {}).get("node_id")
            
            # Then add GetComponentByName function
            get_by_name_params = {
                "blueprint_name": "BirdBP",
                "function_name": "GetComponentByName",
                "params": {
                    "Name": "BirdMesh"
                },
                "node_position": [350, 200]
            }
            
            response = send_command(sock, "add_blueprint_function_node", get_by_name_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to add GetComponentByName node: {response}")
                return
                
            get_by_name_node_id = response.get("result", {}).get("node_id")
            
            # Connect Self to GetComponentByName
            connect_self_params = {
                "blueprint_name": "BirdBP",
                "source_node_id": get_self_node_id,
                "source_pin": "Self",
                "target_node_id": get_by_name_node_id,
                "target_pin": "Target"
            }
            
            response = send_command(sock, "connect_blueprint_nodes", connect_self_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to connect Self to GetComponentByName: {response}")
                return
                
            # Use GetComponentByName node id for later connections
            get_component_node_id = get_by_name_node_id
            logger.info("Alternative component reference approach successful!")
            
        else:
            # Save the node ID for later connections if the original approach worked
            get_component_node_id = response.get("result", {}).get("node_id")
            logger.info("Component reference node added successfully!")

        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 9: Add function node to apply impulse on flap
        function_params = {
            "blueprint_name": "BirdBP",
            "function_name": "AddImpulse",
            "target": "SceneComponent",  # Specify UPrimitiveComponent target class
            "params": {
                "Impulse": [0, 0, 1000]  # Upward impulse
            },
            "node_position": [400, 200]
        }
        
        response = send_command(sock, "add_blueprint_function_node", function_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add function node: {response}")
            # Try alternative approach with different target class if it fails
            logger.info("Trying alternative approach for AddImpulse function...")
            
            # Try with alternative target class
            function_params["target"] = "PrimitiveComponent"  # Try with UPrimitiveComponent
            
            response = send_command(sock, "add_blueprint_function_node", function_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Alternative approach also failed: {response}")
                return
            
        logger.info("AddImpulse function node added successfully!")
        
        # Save the node ID for later connections
        add_impulse_node_id = response.get("result", {}).get("node_id")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 10: Connect input event to add impulse function
        connect_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": input_node_id,
            "source_pin": "Pressed",  # Execute pin on input action event node
            "target_node_id": add_impulse_node_id,
            "target_pin": "Execute"  # Execute pin on function node
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect nodes: {response}")
            return
            
        logger.info("Input node connected to AddImpulse successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 11: Connect get component to add impulse function (target connection)
        connect_target_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_component_node_id,
            "source_pin": "BirdMesh",  # Use component name as the output pin name (UE5.5 convention)
            "target_node_id": add_impulse_node_id,
            "target_pin": "Target"  # The target pin on AddImpulse node
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_target_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect component to target pin: {response}")
            
            # Try with other pin name alternatives
            logger.info("Trying alternative pin names for component connection...")
            
            # Try several common pin names
            pin_names = ["BirdMesh", "Value", "ReturnValue", "Component"]
            for pin_name in pin_names[1:]:  # Skip the first one since we already tried it
                logger.info(f"Trying with pin name: '{pin_name}'")
                connect_target_params["source_pin"] = pin_name
                
                # Close and reopen connection
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(("127.0.0.1", 55557))
                
                response = send_command(sock, "connect_blueprint_nodes", connect_target_params)
                if response and response.get("status") == "success" and response.get("result", {}).get("success"):
                    logger.info(f"Successfully connected with pin name: '{pin_name}'")
                    break
            else:
                # If all pin names fail, try the fallback approach with direct variable reference
                logger.info("Trying final fallback: using direct variable reference to BirdMesh...")
                
                # Create a get variable node for BirdMesh
                get_var_params = {
                    "blueprint_name": "BirdBP",
                    "variable_name": "BirdMesh",
                    "node_position": [200, 200]
                }
                
                # Close and reopen connection
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(("127.0.0.1", 55557))
                
                response = send_command(sock, "add_blueprint_get_variable", get_var_params)
                
                if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                    logger.error(f"Failed to add variable reference node for BirdMesh: {response}")
                    return
                    
                var_node_id = response.get("result", {}).get("node_id")
                
                # Connect variable reference to AddImpulse
                connect_target_var_params = {
                    "blueprint_name": "BirdBP",
                    "source_node_id": var_node_id,
                    "source_pin": "Value",  # Standard output pin name for variables
                    "target_node_id": add_impulse_node_id,
                    "target_pin": "Target"
                }
                
                # Close and reopen connection
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(("127.0.0.1", 55557))
                
                response = send_command(sock, "connect_blueprint_nodes", connect_target_var_params)
                
                if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                    logger.error(f"Failed to connect variable to AddImpulse: {response}")
                    return
                    
                logger.info("Direct variable reference connected successfully!")

        logger.info("Component target connected successfully!")
        
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Step 12: Compile the blueprint
        compile_params = {
            "blueprint_name": "BirdBP"
        }
        
        response = send_command(sock, "compile_blueprint", compile_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint compiled successfully!")        

        # Step 13: Set pawn properties using the new utility function
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Use our new pawn properties utility function
        pawn_props_params = {
            "blueprint_name": "BirdBP",
            "auto_possess_player": "EAutoReceiveInput::Player0"
        }
        
        response = send_command(sock, "set_pawn_properties", pawn_props_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            # If the utility function doesn't exist yet, fall back to the generic property setter
            logger.warning("set_pawn_properties command not available, trying generic property setter...")
            
            # Close and reopen connection
            sock.close()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(("127.0.0.1", 55557))
            
            # Try using the generic property setter
            prop_params = {
                "blueprint_name": "BirdBP",
                "property_name": "AutoPossessPlayer",
                "property_value": "EAutoReceiveInput::Player0"  # Must match the exact enum value expected by UE5.5
            }
            
            response = send_command(sock, "set_blueprint_property", prop_params)
            
            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                logger.error(f"Failed to set auto possession using generic property setter: {response}")
                # Fall back to enabling input in BeginPlay if both property setters fail
                logger.warning("Using fallback to enable input in BeginPlay...")
                
                # Close and reopen connection
                sock.close()
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.connect(("127.0.0.1", 55557))
                
                # Add a Get Player Controller node
                get_pc_params = {
                    "blueprint_name": "BirdBP",
                    "function_name": "GetPlayerController",
                    "params": {
                        "PlayerIndex": 0  # Just use a literal integer, not float
                    },
                    "node_position": [200, 50]
                }
                
                response = send_command(sock, "add_blueprint_function_node", get_pc_params)
                
                if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                    logger.error(f"Failed to add GetPlayerController node: {response}")
                else:
                    get_pc_node_id = response.get("result", {}).get("node_id")
                    
                    # Add an Enable Input node
                    # Close and reopen connection
                    sock.close()
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.connect(("127.0.0.1", 55557))
                    
                    enable_input_params = {
                        "blueprint_name": "BirdBP",
                        "function_name": "EnableInput",
                        "params": {},
                        "node_position": [400, 50]
                    }
                    
                    response = send_command(sock, "add_blueprint_function_node", enable_input_params)
                    
                    if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                        logger.error(f"Failed to add EnableInput node: {response}")
                    else:
                        enable_input_node_id = response.get("result", {}).get("node_id")
                        
                        # Find existing BeginPlay node or create one
                        # Get the BeginPlay node ID
                        # Close and reopen connection
                        sock.close()
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        sock.connect(("127.0.0.1", 55557))
                        
                        # Try to find existing BeginPlay node first
                        find_begin_play_params = {
                            "blueprint_name": "BirdBP",
                            "node_type": "Event",
                            "event_type": "BeginPlay"
                        }
                        
                        begin_play_response = send_command(sock, "find_blueprint_nodes", find_begin_play_params)
                        
                        if begin_play_response and begin_play_response.get("status") == "success" and begin_play_response.get("result", {}).get("success"):
                            # Look for BeginPlay nodes in the response
                            nodes = begin_play_response.get("result", {}).get("nodes", [])
                            if nodes:
                                logger.info(f"Found existing BeginPlay node, reusing it")
                                begin_play_node_id = nodes[0].get("node_id")
                            else:
                                # Only create a new BeginPlay node if we didn't find one
                                begin_play_params = {
                                    "blueprint_name": "BirdBP",
                                    "event_type": "BeginPlay",
                                    "node_position": [0, 50]
                                }
                                
                                begin_play_response = send_command(sock, "add_blueprint_event_node", begin_play_params)
                                
                                if not begin_play_response or begin_play_response.get("status") != "success" or not begin_play_response.get("result", {}).get("success"):
                                    logger.error(f"Failed to get/create BeginPlay node: {begin_play_response}")
                                else:
                                    begin_play_node_id = begin_play_response.get("result", {}).get("node_id")
                        else:
                            logger.error(f"Failed to search for BeginPlay nodes: {begin_play_response}")
                            return
                        
                        # Connect BeginPlay to EnableInput
                        # Close and reopen connection
                        sock.close()
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        sock.connect(("127.0.0.1", 55557))
                        
                        connect_begin_play_params = {
                            "blueprint_name": "BirdBP",
                            "source_node_id": begin_play_node_id,
                            "source_pin": "Then",
                            "target_node_id": enable_input_node_id,
                            "target_pin": "Execute"
                        }
                        
                        response = send_command(sock, "connect_blueprint_nodes", connect_begin_play_params)
                        
                        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                            logger.error(f"Failed to connect BeginPlay to EnableInput: {response}")
                        else:
                            logger.info("Connected BeginPlay to EnableInput successfully!")
                            
                            # Connect PlayerController to EnableInput
                            # Close and reopen connection
                            sock.close()
                            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                            sock.connect(("127.0.0.1", 55557))
                            
                            # Get Self node
                            get_self_params = {
                                "blueprint_name": "BirdBP",
                                "node_position": [300, 100]
                            }
                            
                            response = send_command(sock, "add_blueprint_self_reference", get_self_params)
                            
                            if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                                logger.error(f"Failed to add Self reference node: {response}")
                            else:
                                get_self_node_id = response.get("result", {}).get("node_id")
                                
                                # Connect Self to EnableInput
                                # Close and reopen connection
                                sock.close()
                                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                                sock.connect(("127.0.0.1", 55557))
                                
                                connect_self_params = {
                                    "blueprint_name": "BirdBP",
                                    "source_node_id": get_self_node_id,
                                    "source_pin": "Self",
                                    "target_node_id": enable_input_node_id,
                                    "target_pin": "Target"
                                }
                                
                                response = send_command(sock, "connect_blueprint_nodes", connect_self_params)
                                
                                if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                                    logger.error(f"Failed to connect Self to EnableInput target: {response}")
                                else:
                                    logger.info("Connected Self to EnableInput target successfully!")
                                    
                                    # Connect PlayerController to EnableInput
                                    # Close and reopen connection
                                    sock.close()
                                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                                    sock.connect(("127.0.0.1", 55557))
                                    
                                    connect_pc_params = {
                                        "blueprint_name": "BirdBP",
                                        "source_node_id": get_pc_node_id,
                                        "source_pin": "ReturnValue",
                                        "target_node_id": enable_input_node_id,
                                        "target_pin": "PlayerController"
                                    }
                                    
                                    response = send_command(sock, "connect_blueprint_nodes", connect_pc_params)
                                    
                                    if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                                        logger.error(f"Failed to connect PlayerController to EnableInput: {response}")
                                    else:
                                        logger.info("Connected PlayerController to EnableInput successfully!")
                                        logger.info("Input handling set up through BeginPlay event!")
            else:
                logger.info("Auto possession set successfully using generic property setter!")
        else:
            logger.info("Pawn properties set successfully!")

        # Step 14 (formerly 16): Add GetActorOfClass node to BirdBP's BeginPlay
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Add Get Actor Of Class node for Camera Actor - corrected to use GameplayStatics
        get_camera_params = {
            "blueprint_name": "BirdBP",
            "function_name": "GetActorOfClass",
            "target": "UGameplayStatics",  # Use UGameplayStatics as the exact class name
            "params": {
                "ActorClass": "Camera Actor"  # Match the exact name as shown in the dropdown
            },
            "node_position": [200, -100]
        }
        
        response = send_command(sock, "add_blueprint_function_node", get_camera_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add GetActorOfClass node: {response}")
            return
            
        logger.info("GetActorOfClass node added successfully!")
        get_camera_node_id = response.get("result", {}).get("node_id")
        
        # Step 15 (formerly 17): Add SetViewTargetWithBlend node
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Add SetViewTargetWithBlend function node
        set_view_params = {
            "blueprint_name": "BirdBP",
            "function_name": "SetViewTargetWithBlend",
            "target": "PlayerController",  # Specify that this is a PlayerController function
            "params": {
                "BlendTime": 0.0,  # 0 second blend time
                "BlendFunc": "VTBlend_EaseInOut",
                "LockOutgoing": True
            },
            "node_position": [400, -100]
        }
        
        response = send_command(sock, "add_blueprint_function_node", set_view_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add SetViewTargetWithBlend node: {response}")
            return
            
        logger.info("SetViewTargetWithBlend node added successfully!")
        set_view_node_id = response.get("result", {}).get("node_id")
        
        # Step 16 (formerly 18): Connect BeginPlay to the SetViewTargetWithBlend node
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Try to find existing BeginPlay node first
        find_begin_play_params = {
            "blueprint_name": "BirdBP",
            "node_type": "Event",
            "event_type": "BeginPlay"
        }
        
        response = send_command(sock, "find_blueprint_nodes", find_begin_play_params)
        
        if response and response.get("status") == "success" and response.get("result", {}).get("success"):
            # Use existing BeginPlay node if found
            nodes = response.get("result", {}).get("nodes", [])
            if nodes:
                logger.info(f"Found existing BeginPlay node, reusing it")
                begin_play_node_id = nodes[0].get("node_id")
            else:
                # Create a new BeginPlay node only if none exists
                begin_play_params = {
                    "blueprint_name": "BirdBP",
                    "event_type": "BeginPlay",
                    "node_position": [0, -100]
                }
                
                response = send_command(sock, "add_blueprint_event_node", begin_play_params)
                
                if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
                    logger.error(f"Failed to get/create BeginPlay node: {response}")
                    return
                    
                begin_play_node_id = response.get("result", {}).get("node_id")
        else:
            logger.error(f"Failed to search for BeginPlay nodes: {response}")
            return

        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Connect BeginPlay to GetActorOfClass (instead of directly to SetViewTargetWithBlend)
        connect_begin_play_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": begin_play_node_id,
            "source_pin": "Then",
            "target_node_id": get_camera_node_id,
            "target_pin": "Execute"  # Connect to GetActorOfClass's execute pin (capital E)
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_begin_play_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect BeginPlay to GetActorOfClass: {response}")
            return
            
        logger.info("Connected BeginPlay to GetActorOfClass successfully!")
        
        # Then connect GetActorOfClass to SetViewTargetWithBlend
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        connect_camera_exec_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_camera_node_id,
            "source_pin": "Then",  # Output execution pin from GetActorOfClass (capital T)
            "target_node_id": set_view_node_id,
            "target_pin": "Execute"  # Input execution pin on SetViewTargetWithBlend (capital E)
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_camera_exec_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect GetActorOfClass to SetViewTargetWithBlend execution: {response}")
            return
            
        logger.info("Connected GetActorOfClass execution to SetViewTargetWithBlend successfully!")
        
        # Now connect GetActorOfClass result to SetViewTargetWithBlend's target parameter
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        connect_camera_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_camera_node_id,
            "source_pin": "ReturnValue",
            "target_node_id": set_view_node_id,
            "target_pin": "NewViewTarget"
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_camera_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect camera to SetViewTargetWithBlend: {response}")
            return
            
        logger.info("Connected GetActorOfClass to SetViewTargetWithBlend successfully!")
        
        # Step 18 (formerly 20): Get Player Controller for the SetViewTargetWithBlend function
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Add Get Player Controller node
        get_pc_params = {
            "blueprint_name": "BirdBP",
            "function_name": "GetPlayerController",
            "target": "UGameplayStatics",  # Add UGameplayStatics as the target class
            "params": {
                "PlayerIndex": 0  # Just use a literal integer, not float
            },
            "node_position": [200, -50]
        }
        
        response = send_command(sock, "add_blueprint_function_node", get_pc_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to add GetPlayerController node: {response}")
            return
            
        logger.info("GetPlayerController node added successfully!")
        get_pc_node_id = response.get("result", {}).get("node_id")
        
        # Connect Player Controller to SetViewTargetWithBlend
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        connect_pc_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_pc_node_id,
            "source_pin": "ReturnValue",
            "target_node_id": set_view_node_id,
            "target_pin": "Target"
        }
        
        response = send_command(sock, "connect_blueprint_nodes", connect_pc_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to connect player controller to SetViewTargetWithBlend: {response}")
            return
            
        logger.info("Connected PlayerController to SetViewTargetWithBlend target successfully!")
        
        # Step 19 (formerly 21): Compile the blueprint with the new camera view setup
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        compile_params = {
            "blueprint_name": "BirdBP"
        }
        
        response = send_command(sock, "compile_blueprint", compile_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint with camera view setup compiled successfully!")
        
        # Step 20 (formerly 14): Spawn the bird in the level
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        spawn_params = {
            "blueprint_name": "BirdBP",
            "actor_name": "Bird",
            "location": [0.0, 0.0, 200.0],  # 200 units up
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        }
        
        response = send_command(sock, "spawn_blueprint_actor", spawn_params)
        
        if not response or response.get("status") != "success" or not response.get("result", {}).get("success"):
            logger.error(f"Failed to spawn blueprint actor: {response}")
            return
            
        logger.info("Bird spawned successfully!")

        # Step 21 (formerly 15): Add a camera to the level
        # Close and reopen connection
        sock.close()
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Create a camera actor
        camera_params = {
            "name": "GameCamera",
            "type": "Camera",
            "location": [500.0, 0.0, 250.0],  # Position camera to view the bird from a distance
            "rotation": [0.0, 180.0, 0.0],    # Point camera at bird's spawn location
            "scale": [1.0, 1.0, 1.0]
        }
        
        response = send_command(sock, "create_actor", camera_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to create camera actor: {response}")
            return
            
        logger.info("Camera actor created successfully!")

        logger.info("You can now press spacebar to make the bird flap! The camera will automatically view the bird.")
        
        # Close connection
        sock.close()
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 