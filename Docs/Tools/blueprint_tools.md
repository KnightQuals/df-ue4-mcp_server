# Unreal MCP Blueprint Tools

This document provides detailed information about the Blueprint tools available in the Unreal MCP integration.

## Overview

Blueprint tools allow you to create and manipulate Blueprint assets in Unreal Engine, including creating new Blueprint classes, adding components, setting properties, and spawning Blueprint actors in the level.

## Blueprint Tools

Tools for creating and manipulating Blueprint assets in Unreal Engine.

### create_blueprint

Create a new Blueprint class.

**Parameters:**
- `name` (string) - The name for the new Blueprint class
- `parent_class` (string) - The parent class for the Blueprint

**Returns:**
- Information about the created Blueprint

**Example:**
```json
{
  "command": "create_blueprint",
  "params": {
    "name": "MyActor",
    "parent_class": "Actor"
  }
}
```

### add_component_to_blueprint

Add a component to a Blueprint.

**Parameters:**
- `blueprint_name` (string) - The name of the Blueprint
- `component_type` (string) - The type of component to add
- `component_name` (string) - The name for the new component
- `location` (array, optional) - [X, Y, Z] coordinates for the component's position
- `rotation` (array, optional) - [Pitch, Yaw, Roll] values for the component's rotation
- `scale` (array, optional) - [X, Y, Z] values for the component's scale

**Returns:**
- Information about the added component

**Example:**
```json
{
  "command": "add_component_to_blueprint",
  "params": {
    "blueprint_name": "MyActor",
    "component_type": "StaticMeshComponent",
    "component_name": "Mesh",
    "location": [0, 0, 0],
    "rotation": [0, 0, 0],
    "scale": [1, 1, 1]
  }
}
```

### set_component_property

Set a property on a component in a Blueprint.

**Parameters:**
- `blueprint_name` (string) - The name of the Blueprint
- `component_name` (string) - The name of the component
- `property_name` (string) - The name of the property to set
- `property_value` (any) - The value to set for the property

**Returns:**
- Result of the property setting operation

**Example:**
```json
{
  "command": "set_component_property",
  "params": {
    "blueprint_name": "MyActor",
    "component_name": "Mesh",
    "property_name": "StaticMesh",
    "property_value": "/Game/StarterContent/Shapes/Shape_Cube.Shape_Cube"
  }
}
```

### set_physics_properties

Set physics properties on a component.

**Parameters:**
- `blueprint_name` (string) - The name of the Blueprint
- `component_name` (string) - The name of the component
- `simulate_physics` (boolean, optional) - Whether to simulate physics (default: true)
- `gravity_enabled` (boolean, optional) - Whether gravity is enabled (default: true)
- `mass` (float, optional) - The mass of the component (default: 1.0)
- `linear_damping` (float, optional) - Linear damping value (default: 0.01)
- `angular_damping` (float, optional) - Angular damping value (default: 0.0)

**Returns:**
- Result of the physics properties setting operation

**Example:**
```json
{
  "command": "set_physics_properties",
  "params": {
    "blueprint_name": "MyActor",
    "component_name": "Mesh",
    "simulate_physics": true,
    "gravity_enabled": true,
    "mass": 10.0,
    "linear_damping": 0.05,
    "angular_damping": 0.1
  }
}
```

### compile_blueprint

Compile a Blueprint.

**Parameters:**
- `blueprint_name` (string) - The name of the Blueprint to compile

**Returns:**
- Result of the compilation operation

**Example:**
```json
{
  "command": "compile_blueprint",
  "params": {
    "blueprint_name": "MyActor"
  }
}
```

### spawn_blueprint_actor

Spawn an actor from a Blueprint.

**Parameters:**
- `blueprint_name` (string) - The name of the Blueprint to spawn
- `actor_name` (string) - The name for the spawned actor
- `location` (array, optional) - [X, Y, Z] coordinates for the actor's position
- `rotation` (array, optional) - [Pitch, Yaw, Roll] values for the actor's rotation
- `scale` (array, optional) - [X, Y, Z] values for the actor's scale

**Returns:**
- Information about the spawned actor

**Example:**
```json
{
  "command": "spawn_blueprint_actor",
  "params": {
    "blueprint_name": "MyActor",
    "actor_name": "MyActorInstance",
    "location": [0, 0, 100],
    "rotation": [0, 45, 0],
    "scale": [1, 1, 1]
  }
}
```

## Error Handling

All command responses include a "success" field indicating whether the operation succeeded, and an optional "message" field with details in case of failure.

```json
{
  "success": false,
  "message": "Blueprint 'MyActor' not found in the project",
  "command": "compile_blueprint"
}
```

## Component Types

Common component types for the `add_component_to_blueprint` command:

- `StaticMeshComponent` - Static mesh component
- `SkeletalMeshComponent` - Skeletal mesh component
- `CameraComponent` - Camera component
- `PointLightComponent` - Point light component
- `SpotLightComponent` - Spot light component
- `AudioComponent` - Audio component
- `ParticleSystemComponent` - Particle system component
- `SceneComponent` - Base scene component
