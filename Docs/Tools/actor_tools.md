# Unreal MCP Editor Tools

This document provides detailed information about the actor tools available in the Unreal MCP integration.

## Overview

Actor tools allow you to manipulate actors in the Unreal Engine scene.

## Actor Tools

### get_actors_in_level

Get a list of all actors in the current level.

**Parameters:**
- None

**Returns:**
- List of all actors with their properties

**Example:**
```json
{
  "command": "get_actors_in_level",
  "params": {}
}
```

### find_actors_by_name

Find actors in the current level by name pattern.

**Parameters:**
- `pattern` (string) - The name or partial name pattern to search for

**Returns:**
- List of matching actor names

**Example:**
```json
{
  "command": "find_actors_by_name",
  "params": {
    "pattern": "Cube"
  }
}
```

### create_actor

Create a new actor in the current level.

**Parameters:**
- `name` (string) - The name for the new actor (must be unique)
- `type` (string) - The type of actor to create (must be uppercase)
- `location` (array, optional) - [X, Y, Z] coordinates for the actor's position, defaults to [0, 0, 0]
- `rotation` (array, optional) - [Pitch, Yaw, Roll] values for the actor's rotation, defaults to [0, 0, 0]
- `scale` (array, optional) - [X, Y, Z] values for the actor's scale, defaults to [1, 1, 1]

**Returns:**
- Information about the created actor

**Example:**
```json
{
  "command": "create_actor",
  "params": {
    "name": "MyCube",
    "type": "CUBE",
    "location": [0, 0, 100],
    "rotation": [0, 45, 0],
    "scale": [2, 2, 2]
  }
}
```

### delete_actor

Delete an actor by name.

**Parameters:**
- `name` (string) - The name of the actor to delete

**Returns:**
- Result of the delete operation

**Example:**
```json
{
  "command": "delete_actor",
  "params": {
    "name": "MyCube"
  }
}
```

### set_actor_transform

Set the transform (location, rotation, scale) of an actor.

**Parameters:**
- `name` (string) - The name of the actor to modify
- `location` (array, optional) - [X, Y, Z] coordinates for the actor's position
- `rotation` (array, optional) - [Pitch, Yaw, Roll] values for the actor's rotation
- `scale` (array, optional) - [X, Y, Z] values for the actor's scale

**Returns:**
- Result of the transform operation

**Example:**
```json
{
  "command": "set_actor_transform",
  "params": {
    "name": "MyCube",
    "location": [100, 200, 300],
    "rotation": [0, 90, 0]
  }
}
```

### get_actor_properties

Get all properties of an actor.

**Parameters:**
- `name` (string) - The name of the actor

**Returns:**
- Object containing all actor properties

**Example:**
```json
{
  "command": "get_actor_properties",
  "params": {
    "name": "MyCube"
  }
}
```

## Error Handling

All command responses include a "success" field indicating whether the operation succeeded, and an optional "message" field with details in case of failure.

```json
{
  "success": false,
  "message": "Actor 'MyCube' not found in the current level"
}
```

## Implementation Notes

- All numeric parameters for transforms (location, rotation, scale) must be provided as lists of 3 float values
- Actor types should be provided in uppercase
- The server maintains logging of all operations with detailed information and error messages
- All commands are executed through a connection to the Unreal Engine editor

## Type Reference

### Actor Types

Supported actor types for the `create_actor` command:

- `CUBE` - Static mesh cube
- `SPHERE` - Static mesh sphere
- `CYLINDER` - Static mesh cylinder
- `PLANE` - Static mesh plane
- `POINT_LIGHT` - Point light source
- `SPOT_LIGHT` - Spot light source
- `DIRECTIONAL_LIGHT` - Directional light source
- `CAMERA` - Camera actor
- `EMPTY` - Empty actor (container)

## Future Extensions

The following tool categories are planned for future releases:

- **Level Tools**: Managing Unreal Engine levels
- **Material Tools**: Creating and editing materials
- **Blueprint Tools**: Manipulating Blueprints
- **Asset Tools**: Managing project assets
- **Editor Tools**: Controlling the Unreal Editor