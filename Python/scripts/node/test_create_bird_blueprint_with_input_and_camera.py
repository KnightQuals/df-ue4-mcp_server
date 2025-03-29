#!/usr/bin/env python
"""
Test script for blueprint node tools via MCP.
This script creates a simple flapping bird Blueprint with basic physics and input handling.

Commands used:
1. create_blueprint - Creates a new Blueprint class inheriting from Pawn
2. add_component_to_blueprint - Adds a StaticMeshComponent named "BirdMesh"
3. set_physics_properties - Configures physics properties for the mesh
4. add_blueprint_variable - Adds a Float variable "FlapStrength"
5. set_static_mesh_properties - Sets the mesh to a sphere
6. create_input_mapping - Creates a "Flap" action mapped to SpaceBar
7. find_blueprint_nodes - Searches for existing nodes (e.g., BeginPlay)
8. add_blueprint_event_node - Creates event nodes (BeginPlay)
9. add_blueprint_input_action_node - Creates input action node for Flap
10. add_blueprint_get_self_component_reference - Creates component reference node
11. add_blueprint_function_node - Creates function nodes (AddImpulse, GetActorOfClass, etc.)
12. connect_blueprint_nodes - Connects nodes together
13. compile_blueprint - Compiles the Blueprint
14. set_pawn_properties - Sets auto-possess properties
15. spawn_blueprint_actor - Spawns the bird in the level
16. create_actor - Creates a camera actor

Blueprint Graph Layout:
```
[BeginPlay] ──────┐
                 │
[InputAction] ───┼─── [BirdMesh] ─── [AddImpulse]
                 │
[GetActorOfClass] ─── [SetViewTargetWithBlend]
     │
     └── [GetPlayerController]
```

Node Positions:
- BeginPlay: [-400, 0]
- InputAction: [-400, 300]
- BirdMesh: [0, 300]
- AddImpulse: [400, 300]
- GetActorOfClass: [0, -200]
- SetViewTargetWithBlend: [400, -200]
- GetPlayerController: [0, -100]

Connections:
1. InputAction.Pressed -> AddImpulse.Execute
2. BirdMesh -> AddImpulse.self
3. BeginPlay.Then -> GetActorOfClass.Execute
4. GetActorOfClass.Then -> SetViewTargetWithBlend.Execute
5. GetActorOfClass.ReturnValue -> SetViewTargetWithBlend.NewViewTarget
6. GetPlayerController.ReturnValue -> SetViewTargetWithBlend.self
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

def send_mcp_command(command: str, params: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal MCP server with automatic socket lifecycle management."""
    sock = None
    try:
        # Create a new socket for each command
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect(("127.0.0.1", 55557))
        
        # Send the command and get the response
        return send_command(sock, command, params)
        
    except Exception as e:
        logger.error(f"Error in socket communication: {e}")
        return None
    finally:
        # Always close the socket when done
        if sock:
            sock.close()

def main():
    """Main function to test blueprint node tools."""
    try:
        # Step 1: Create a blueprint for the bird
        bp_params = {
            "name": "BirdBP",
            "parent_class": "Pawn"
        }
        
        response = send_mcp_command("create_blueprint", bp_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to create blueprint: {response}")
            return
        
        # Check if blueprint already existed
        if response.get("result", {}).get("already_exists"):
            logger.info(f"Blueprint 'BirdBP' already exists, reusing it")
        else:
            logger.info("Blueprint created successfully!")
        
        # Step 2: Add a static mesh component
        component_params = {
            "blueprint_name": "BirdBP",
            "component_type": "StaticMeshComponent",
            "component_name": "BirdMesh",
            "location": [0.0, 0.0, 0.0],
            "rotation": [0.0, 0.0, 0.0],
            "scale": [0.5, 0.5, 0.5]  # Smaller bird
        }
        
        response = send_mcp_command("add_component_to_blueprint", component_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add component: {response}")
            return
            
        logger.info("Bird mesh component added successfully!")
        
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
        
        response = send_mcp_command("set_physics_properties", physics_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to set physics properties: {response}")
            return
            
        logger.info("Physics properties set successfully!")
        
        # Step 4: Add variables for tracking bird state
        response = send_mcp_command("add_blueprint_variable", {
            "blueprint_name": "BirdBP",
            "variable_name": "FlapStrength",
            "variable_type": "Float",
            "default_value": 500.0,
            "is_exposed": True
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add variable: {response}")
            return
            
        logger.info("FlapStrength variable added successfully!")
        
        # Step 4b: Set the static mesh of the BirdMesh component to a sphere
        response = send_mcp_command("set_static_mesh_properties", {
            "blueprint_name": "BirdBP",
            "component_name": "BirdMesh",
            "static_mesh": "/Engine/BasicShapes/Sphere.Sphere"
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to set static mesh: {response}")
            return  

        logger.info("Setting Static Mesh Component of Bird to sphere successfully!")

        # Step 5: Create input mapping for flap action
        response = send_mcp_command("create_input_mapping", {
            "action_name": "Flap",
            "key": "SpaceBar",
            "input_type": "Action"
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to create input mapping: {response}")
            return
            
        logger.info("Flap input mapping created successfully!")
        
        # Step 6: Create BeginPlay event node - check if it exists first
        begin_play_response = None
        
        # First try to find existing BeginPlay nodes
        find_begin_play_params = {
            "blueprint_name": "BirdBP",
            "node_type": "Event",
            "event_name": "ReceiveBeginPlay"  # Use the exact Unreal Engine event name
        }
        
        response = send_mcp_command("find_blueprint_nodes", find_begin_play_params)
        
        if response and response.get("status") == "success":
            # Look for BeginPlay nodes in the response
            nodes = response.get("result", {}).get("node_guids", [])
            if nodes:
                logger.info(f"Found existing ReceiveBeginPlay node, reusing it")
                # Use the first BeginPlay node we find
                begin_play_node_id = nodes[0]
                begin_play_response = {"result": {"success": True, "node_id": begin_play_node_id}}
        
        # Only create a new BeginPlay node if we didn't find one
        if begin_play_response is None:
            begin_play_params = {
                "blueprint_name": "BirdBP",
                "event_name": "ReceiveBeginPlay",
                "node_position": [-400, 0]  # Move BeginPlay further left
            }
            
            response = send_mcp_command("add_blueprint_event_node", begin_play_params)
            
            if not response or response.get("status") != "success":
                logger.error(f"Failed to add ReceiveBeginPlay event node: {response}")
                return
                
            logger.info("ReceiveBeginPlay event node added successfully!")
            
        # Save the node ID for later connections
        begin_play_node_id = begin_play_response.get("result", {}).get("node_id")
        
        # Step 7: Create input action event node
        input_action_params = {
            "blueprint_name": "BirdBP",
            "action_name": "Flap",
            "node_position": [-400, 300]  # Move input action down and left
        }
        
        # Create the InputAction event node using the dedicated function
        response = send_mcp_command("add_blueprint_input_action_node", input_action_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add Input action node: {response}")
            return
            
        logger.info("Input action node added successfully")
        
        # Save the node ID for later connections
        input_node_id = response.get("result", {}).get("node_id")
        
        # Step 8: Add a get component reference node for BirdMesh 
        get_component_params = {
            "blueprint_name": "BirdBP",
            "component_name": "BirdMesh",
            "node_position": [0, 300]  # Center the component reference
        }
        
        response = send_mcp_command("add_blueprint_get_self_component_reference", get_component_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add component reference node: {response}")
            return
        
        get_component_node_id = response.get("result", {}).get("node_id")
           
        # Step 9: Add function node to apply impulse on flap
        function_params = {
            "blueprint_name": "BirdBP",
            "function_name": "AddImpulse",
            "target": "UPrimitiveComponent",
            "params": {
                "Impulse": [0, 0, 1000]
            },
            "node_position": [400, 300]  # Move AddImpulse to the right
        }
        
        response = send_mcp_command("add_blueprint_function_node", function_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add AddImpulse function node: {response}")
            # If UPrimitiveComponent fails, try alternatives
            targets_to_try = ["PrimitiveComponent", "SceneComponent", "USceneComponent"] 
            
            for target in targets_to_try:
                logger.info(f"Trying alternative class for AddImpulse: {target}")
                function_params["target"] = target
                
                response = send_mcp_command("add_blueprint_function_node", function_params)
                if response and response.get("status") == "success":
                    logger.info(f"Successfully added AddImpulse using target class: {target}")
                    break
            else:
                logger.error("All attempts to add AddImpulse function failed")
                return
            
        logger.info("AddImpulse function node added successfully!")
        
        # Save the node ID for later connections
        add_impulse_node_id = response.get("result", {}).get("node_id")
        
        # Step 10: Connect input event to add impulse function
        connect_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": input_node_id,
            "source_pin": "Pressed",  # Execute pin on input action event node
            "target_node_id": add_impulse_node_id,
            "target_pin": "Execute"  # Execute pin on function node
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect nodes: {response}")
            return
            
        logger.info("Input node connected to AddImpulse successfully!")
        
        # Step 11: Connect get component to add impulse function (target connection)
        connect_target_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_component_node_id,
            "source_pin": "BirdMesh",  # Use component name as the output pin name (UE5.5 convention)
            "target_node_id": add_impulse_node_id,
            "target_pin": "self"  # Change from "Target" to "self" - this is the actual pin name in UE5.5
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_target_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect component to target pin: {response}")
            return

        logger.info("Component target connected successfully!")
        
        # Step 12: Compile the blueprint
        response = send_mcp_command("compile_blueprint", {
            "blueprint_name": "BirdBP"
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint compiled successfully!")        

        # Step 13: Set pawn properties using the new utility function
        response = send_mcp_command("set_pawn_properties", {
            "blueprint_name": "BirdBP",
            "auto_possess_player": "Player0"  # Use short enum name as per reflection docs
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to set pawn properties: {response}")
            return
            
        logger.info("Pawn properties set successfully!")

        # Step 14: Add GetActorOfClass node to BirdBP's BeginPlay
        # Note: In UE5.5, class references must use the full path format: /Script/ModuleName.ClassName
        # This format is the standard way Unreal Engine references classes internally:
        # - /Script/ prefix indicates this is a C++ (native) class
        # - ModuleName is the module where the class is defined (e.g., Engine, Game)
        # - ClassName is the actual class name
        # Examples:
        # - /Script/Engine.CameraActor for the camera actor class
        # - /Script/Engine.PlayerController for the player controller class
        get_camera_params = {
            "blueprint_name": "BirdBP",
            "function_name": "GetActorOfClass",
            "target": "UGameplayStatics",
            "params": {
                "ActorClass": "/Script/Engine.CameraActor"  
            },
            "node_position": [0, -200]  # Move camera setup nodes down
        }
        
        response = send_mcp_command("add_blueprint_function_node", get_camera_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add GetActorOfClass node: {response}")
            return
            
        logger.info("GetActorOfClass node added successfully!")
        get_camera_node_id = response.get("result", {}).get("node_id")
        
        # Step 15 (formerly 17): Add SetViewTargetWithBlend node
        # Add SetViewTargetWithBlend function node
        set_view_params = {
            "blueprint_name": "BirdBP",
            "function_name": "SetViewTargetWithBlend",
            "target": "PlayerController",
            "params": {
                "BlendTime": 0.0,
                "BlendFunc": "VTBlend_EaseInOut",
                "LockOutgoing": True
            },
            "node_position": [400, -200]  # Align with GetActorOfClass
        }
        
        response = send_mcp_command("add_blueprint_function_node", set_view_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add SetViewTargetWithBlend node: {response}")
            return
            
        logger.info("SetViewTargetWithBlend node added successfully!")
        set_view_node_id = response.get("result", {}).get("node_id")
        
        # Step 16 (formerly 18): Connect BeginPlay to the SetViewTargetWithBlend node
        # Try to find existing BeginPlay node first
        find_begin_play_params = {
            "blueprint_name": "BirdBP",
            "node_type": "Event",
            "event_name": "ReceiveBeginPlay"  # Use the exact Unreal Engine event name
        }
        
        response = send_mcp_command("find_blueprint_nodes", find_begin_play_params)
        
        if response and response.get("status") == "success":
            # Use existing BeginPlay node if found
            nodes = response.get("result", {}).get("nodes", [])
            if nodes:
                logger.info(f"Found existing ReceiveBeginPlay node, reusing it")
                begin_play_node_id = nodes[0].get("node_id")
            else:
                # Create a new BeginPlay node only if none exists
                begin_play_params = {
                    "blueprint_name": "BirdBP",
                    "event_name": "ReceiveBeginPlay",  # Use the exact Unreal Engine event name
                    "node_position": [-400, 0]  # Move BeginPlay further left
                }

                response = send_mcp_command("add_blueprint_event_node", begin_play_params)
                
                if not response or response.get("status") != "success":
                    logger.error(f"Failed to get/create ReceiveBeginPlay node: {response}")
                    return
                    
                begin_play_node_id = response.get("result", {}).get("node_id")
        else:
            logger.error(f"Failed to search for ReceiveBeginPlay nodes: {response}")
            return

        # Connect BeginPlay to GetActorOfClass (instead of directly to SetViewTargetWithBlend)
        connect_begin_play_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": begin_play_node_id,
            "source_pin": "Then",
            "target_node_id": get_camera_node_id,
            "target_pin": "Execute"  # Connect to GetActorOfClass's execute pin (capital E)
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_begin_play_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect BeginPlay to GetActorOfClass: {response}")
            return
            
        logger.info("Connected BeginPlay to GetActorOfClass successfully!")
        
        # Then connect GetActorOfClass to SetViewTargetWithBlend
        connect_camera_exec_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_camera_node_id,
            "source_pin": "Then",  # Output execution pin from GetActorOfClass (capital T)
            "target_node_id": set_view_node_id,
            "target_pin": "Execute"  # Input execution pin on SetViewTargetWithBlend (capital E)
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_camera_exec_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect GetActorOfClass to SetViewTargetWithBlend execution: {response}")
            return
            
        logger.info("Connected GetActorOfClass execution to SetViewTargetWithBlend successfully!")
        
        # Now connect GetActorOfClass result to SetViewTargetWithBlend's target parameter
        connect_camera_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_camera_node_id,
            "source_pin": "ReturnValue",
            "target_node_id": set_view_node_id,
            "target_pin": "NewViewTarget"
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_camera_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect camera to SetViewTargetWithBlend: {response}")
            return
            
        logger.info("Connected GetActorOfClass to SetViewTargetWithBlend successfully!")
        
        # Step 18 (formerly 20): Get Player Controller for the SetViewTargetWithBlend function
        # Add Get Player Controller node
        get_pc_params = {
            "blueprint_name": "BirdBP",
            "function_name": "GetPlayerController",
            "target": "UGameplayStatics",
            "params": {
                "PlayerIndex": 0
            },
            "node_position": [0, -100]  # Place between GetActorOfClass and SetViewTarget
        }
        
        response = send_mcp_command("add_blueprint_function_node", get_pc_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to add GetPlayerController node: {response}")
            return
            
        logger.info("GetPlayerController node added successfully!")
        get_pc_node_id = response.get("result", {}).get("node_id")
        
        # Connect Player Controller to SetViewTargetWithBlend
        connect_pc_params = {
            "blueprint_name": "BirdBP",
            "source_node_id": get_pc_node_id,
            "source_pin": "ReturnValue",
            "target_node_id": set_view_node_id,
            "target_pin": "self"
        }
        
        response = send_mcp_command("connect_blueprint_nodes", connect_pc_params)
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to connect player controller to SetViewTargetWithBlend: {response}")
            return
            
        logger.info("Connected PlayerController to SetViewTargetWithBlend target successfully!")
        
        # Step 19 (formerly 21): Compile the blueprint with the new camera view setup
        response = send_mcp_command("compile_blueprint", {
            "blueprint_name": "BirdBP"
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to compile blueprint: {response}")
            return
            
        logger.info("Blueprint with camera view setup compiled successfully!")
        
        # Step 20 (formerly 14): Spawn the bird in the level
        response = send_mcp_command("spawn_blueprint_actor", {
            "blueprint_name": "BirdBP",
            "actor_name": "Bird",
            "location": [0.0, 0.0, 200.0],  # 200 units up
            "rotation": [0.0, 0.0, 0.0],
            "scale": [1.0, 1.0, 1.0]
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to spawn blueprint actor: {response}")
            return
            
        logger.info("Bird spawned successfully!")

        # Step 21 (formerly 15): Add a camera to the level
        # Create a camera actor
        response = send_mcp_command("create_actor", {
            "name": "GameCamera",
            "type": "CameraActor",
            "location": [500.0, 0.0, 250.0],  # Position camera to view the bird from a distance
            "rotation": [0.0, 180.0, 0.0],    # Point camera at bird's spawn location
            "scale": [1.0, 1.0, 1.0]
        })
        
        if not response or response.get("status") != "success":
            logger.error(f"Failed to create camera actor: {response}")
            return
            
        logger.info("Camera actor created successfully!")

        logger.info("You can now press spacebar to make the bird flap! The camera will automatically view the bird.")
        
    except Exception as e:
        logger.error(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main() 